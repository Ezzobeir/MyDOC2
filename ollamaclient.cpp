#include "ollamaclient.h"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace {

// 127.0.0.1 plutôt que localhost : évite l'attente d'une résolution IPv6 (::1) alors qu'Ollama écoute en IPv4.
const char *kDefaultHost = "http://127.0.0.1:11434";
const char *kDefaultModel = "qwen3.5:4b";
const int kMaxHistoryMessages = 20;
// Le premier appel doit charger le modèle en mémoire : prévoir large.
const int kRequestTimeoutMs = 180000;

const char *kSystemPrompt =
    "Tu es l'assistant de saisie du dossier de consultation d'un médecin. Le médecin te dicte ou t'écrit "
    "des informations en français ; tu les reportes dans les champs du formulaire.\n\n"
    "Règles :\n"
    "- Renseigne un champ uniquement si le médecin l'a clairement indiqué. Mets null pour tous les autres "
    "champs : ils restent inchangés. N'invente jamais une mesure, un symptôme ou un diagnostic.\n"
    "- Ne pose jamais de diagnostic et ne propose jamais de traitement de ton propre chef : recopie ce que le "
    "médecin dicte, reformulé de façon concise et médicale si besoin.\n"
    "- Pour un champ texte (motif, symptômes, diagnostic, autres mesures), renvoie la valeur COMPLÈTE du champ "
    "après modification, en tenant compte du contenu actuel fourni dans l'état du formulaire (le médecin peut "
    "demander d'ajouter, de retirer ou de corriger un élément).\n"
    "- Unités : taille en cm, poids en kg, tension en mmHg (si le médecin donne une tension en cmHg, par "
    "exemple 13/8, convertis en 130/80), glycémie en g/L (si elle est donnée en mmol/L, divise par 5,55), "
    "température en °C.\n"
    "- Si une information est ambiguë ou incomplète, ne renseigne pas le champ et pose une question courte "
    "dans la réponse.\n"
    "- La réponse (champ reply) est courte, en français, et indique ce que tu as renseigné. Tu ne connais pas "
    "l'identité du patient et tu n'en as pas besoin.\n"
    "Le médecin relit toujours le formulaire avant de l'enregistrer.\n\n"
    "Exemples (champs non cités = null) :\n"
    "- « tension 13/8, température 38,5 » -> bp_systolic=130, bp_diastolic=80, temperature_c=38.5\n"
    "- « glycémie 1,10 » -> glycemia_gl=1.1\n"
    "- « il pèse 72 kilos pour 1 mètre 78 » -> weight_kg=72, height_cm=178\n"
    "- « toux sèche depuis 3 jours » -> symptoms=\"Toux sèche depuis 3 jours\" (en ajoutant au contenu actuel)";

QJsonObject nullableNumber(const QString &description, bool integer)
{
    return QJsonObject{
        {"description", description},
        {"anyOf", QJsonArray{QJsonObject{{"type", integer ? "integer" : "number"}},
                              QJsonObject{{"type", "null"}}}}};
}

QJsonObject nullableText(const QString &description)
{
    return QJsonObject{
        {"description", description},
        {"anyOf", QJsonArray{QJsonObject{{"type", "string"}}, QJsonObject{{"type", "null"}}}}};
}

QJsonObject responseSchema()
{
    const QJsonObject properties{
        {"reply", QJsonObject{{"type", "string"},
                               {"description", "Réponse courte au médecin : ce qui a été renseigné, ou une "
                                               "question de clarification."}}},
        {"height_cm", nullableNumber("Taille en cm", false)},
        {"weight_kg", nullableNumber("Poids en kg", false)},
        {"bp_systolic", nullableNumber("Tension systolique en mmHg", true)},
        {"bp_diastolic", nullableNumber("Tension diastolique en mmHg", true)},
        {"glycemia_gl", nullableNumber("Glycémie en g/L", false)},
        {"temperature_c", nullableNumber("Température en °C", false)},
        {"other_measures", nullableText("Autres mesures, texte libre (valeur complète du champ)")},
        {"motif", nullableText("Motif de la consultation (valeur complète du champ)")},
        {"symptoms", nullableText("Symptômes (valeur complète du champ)")},
        {"diagnosis", nullableText("Diagnostic (valeur complète du champ)")}};

    QJsonArray required;
    for (const QString &key : properties.keys()) {
        required.append(key);
    }

    return QJsonObject{{"type", "object"},
                        {"properties", properties},
                        {"required", required},
                        {"additionalProperties", false}};
}

}

OllamaClient::OllamaClient(QObject *parent) : QObject(parent)
{
}

QString OllamaClient::configPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/ollama.ini";
}

QString OllamaClient::host() const
{
    QSettings settings(configPath(), QSettings::IniFormat);
    QString configured = settings.value("Ollama/Host").toString().trimmed();
    if (configured.isEmpty()) {
        configured = QString::fromLatin1(kDefaultHost);
    }
    while (configured.endsWith('/')) {
        configured.chop(1);
    }
    return configured;
}

QString OllamaClient::model() const
{
    QSettings settings(configPath(), QSettings::IniFormat);
    const QString configured = settings.value("Ollama/Model").toString().trimmed();
    return configured.isEmpty() ? QString::fromLatin1(kDefaultModel) : configured;
}

bool OllamaClient::useGpu() const
{
    QSettings settings(configPath(), QSettings::IniFormat);
    return settings.value("Ollama/UseGpu", false).toBool();
}

QString OllamaClient::configurationProblem() const
{
    const QUrl url(host());
    if (!url.isValid() || url.host().isEmpty() || (url.scheme() != "http" && url.scheme() != "https")) {
        return "Adresse Ollama invalide : " + host() + "\nVérifiez Host dans " + configPath() +
               " (exemple : http://127.0.0.1:11434).";
    }
    return QString();
}

void OllamaClient::sendMessage(const QString &doctorText, const QJsonObject &formState)
{
    if (m_pending) {
        return;
    }

    const QString problem = configurationProblem();
    if (!problem.isEmpty()) {
        emit requestFailed(problem);
        return;
    }

    const QString stateText = QString::fromUtf8(QJsonDocument(formState).toJson(QJsonDocument::Compact));
    const QString userContent = "État actuel du formulaire (JSON) :\n" + stateText +
                                "\n\nMessage du médecin :\n" + doctorText;

    QJsonArray messages;
    messages.append(QJsonObject{{"role", "system"}, {"content", QString::fromUtf8(kSystemPrompt)}});
    for (const QJsonValue &message : m_history) {
        messages.append(message);
    }
    messages.append(QJsonObject{{"role", "user"}, {"content", userContent}});

    QJsonObject options{{"temperature", 0}, {"num_ctx", 8192}};
    if (!useGpu()) {
        // Calcul sur le processeur : plus lent, mais évite les plantages CUDA de certains pilotes NVIDIA.
        options.insert("num_gpu", 0);
    }

    const QJsonObject body{
        {"model", model()},
        {"messages", messages},
        {"stream", false},
        // Pas de phase de réflexion : trop lent pour de la saisie, et la sortie est contrainte par le schéma.
        {"think", false},
        {"format", responseSchema()},
        {"keep_alive", "30m"},
        {"options", options}};

    QNetworkRequest request(QUrl(host() + "/api/chat"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(kRequestTimeoutMs);

    m_pendingDoctorText = doctorText;
    m_pending = m_network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_pending, &QNetworkReply::finished, this, &OllamaClient::onFinished);
}

void OllamaClient::onFinished()
{
    QNetworkReply *reply = m_pending;
    m_pending = nullptr;
    if (!reply) {
        return;
    }
    reply->deleteLater();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();

    if (status == 0) {
        if (reply->error() == QNetworkReply::ConnectionRefusedError ||
            reply->error() == QNetworkReply::HostNotFoundError) {
            emit requestFailed("Ollama est injoignable (" + host() +
                               "). Vérifiez qu'Ollama est lancé (icône dans la zone de notification, "
                               "ou commande « ollama serve »).");
        } else if (reply->error() == QNetworkReply::OperationCanceledError ||
                   reply->error() == QNetworkReply::TimeoutError) {
            emit requestFailed("Ollama n'a pas répondu à temps. Le modèle est peut-être en cours de chargement : "
                               "réessayez dans un instant.");
        } else {
            emit requestFailed("Connexion à Ollama impossible : " + reply->errorString());
        }
        return;
    }

    const QJsonObject response = QJsonDocument::fromJson(data).object();

    if (status != 200) {
        QString message = response.value("error").toString();
        if (message.contains("CUDA", Qt::CaseInsensitive)) {
            message += "\n\nLe calcul sur carte graphique a échoué. Dans " + configPath() +
                       ", mettez UseGpu=false (valeur par défaut) ou mettez à jour le pilote NVIDIA.";
        }
        if (status == 404) {
            message = "Modèle introuvable : " + model() + ". Installez-le avec « ollama pull " + model() +
                      " » ou changez Model dans " + configPath() + ".";
        }
        emit requestFailed(QString("Erreur d'Ollama (%1) : %2").arg(status).arg(message));
        return;
    }

    const QString text = response.value("message").toObject().value("content").toString();
    if (response.value("done_reason").toString() == "length") {
        emit requestFailed("La réponse de l'assistant a été tronquée. Reformulez ou raccourcissez la demande.");
        return;
    }

    QJsonParseError parseError;
    const QJsonObject result = QJsonDocument::fromJson(text.toUtf8(), &parseError).object();
    if (parseError.error != QJsonParseError::NoError || !result.contains("reply")) {
        emit requestFailed("Réponse de l'assistant illisible.");
        return;
    }

    const QString replyText = result.value("reply").toString();

    QJsonObject fields;
    for (auto it = result.begin(); it != result.end(); ++it) {
        if (it.key() != "reply" && !it.value().isNull()) {
            fields.insert(it.key(), it.value());
        }
    }

    m_history.append(QJsonObject{{"role", "user"}, {"content", m_pendingDoctorText}});
    m_history.append(QJsonObject{{"role", "assistant"}, {"content", replyText}});
    while (m_history.size() > kMaxHistoryMessages) {
        m_history.removeFirst();
        m_history.removeFirst();
    }

    emit replyReceived(replyText, fields);
}
