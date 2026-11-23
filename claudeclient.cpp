#include "claudeclient.h"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QSslSocket>
#include <QStandardPaths>
#include <QUrl>

namespace {

const char *kDefaultModel = "claude-opus-5-5";
const int kMaxHistoryMessages = 20;

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
    "Le médecin relit toujours le formulaire avant de l'enregistrer.";

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

ClaudeClient::ClaudeClient(QObject *parent) : QObject(parent)
{
}

QString ClaudeClient::configPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/claude.ini";
}

QString ClaudeClient::apiKey() const
{
    QSettings settings(configPath(), QSettings::IniFormat);
    const QString fromFile = settings.value("Claude/ApiKey").toString().trimmed();
    if (!fromFile.isEmpty()) {
        return fromFile;
    }
    return qEnvironmentVariable("ANTHROPIC_API_KEY").trimmed();
}

QString ClaudeClient::model() const
{
    QSettings settings(configPath(), QSettings::IniFormat);
    const QString configured = settings.value("Claude/Model").toString().trimmed();
    return configured.isEmpty() ? QString::fromLatin1(kDefaultModel) : configured;
}

QString ClaudeClient::configurationProblem() const
{
    if (apiKey().isEmpty()) {
        return "Assistant non configuré. Créez le fichier :\n" + configPath() +
               "\n\navec le contenu :\n[Claude]\nApiKey=votre_clé_API_Anthropic\n"
               "(Model=claude-opus-5-5 est facultatif.)";
    }
    if (!QSslSocket::supportsSsl()) {
        return "TLS/SSL indisponible sur ce poste : connexion sécurisée impossible.";
    }
    return QString();
}

void ClaudeClient::sendMessage(const QString &doctorText, const QJsonObject &formState)
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

    QJsonArray messages = m_history;
    messages.append(QJsonObject{{"role", "user"}, {"content", userContent}});

    const QJsonObject body{
        {"model", model()},
        {"max_tokens", 16000},
        {"system", QString::fromUtf8(kSystemPrompt)},
        {"output_config", QJsonObject{{"effort", "low"},
                                       {"format", QJsonObject{{"type", "json_schema"},
                                                               {"schema", responseSchema()}}}}},
        {"messages", messages}};

    QNetworkRequest request(QUrl("https://api.anthropic.com/v1/messages"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("x-api-key", apiKey().toUtf8());
    request.setRawHeader("anthropic-version", "2023-06-01");
    request.setTransferTimeout(90000);

    m_pendingDoctorText = doctorText;
    m_pending = m_network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_pending, &QNetworkReply::finished, this, &ClaudeClient::onFinished);
}

void ClaudeClient::onFinished()
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
        emit requestFailed("Connexion à l'assistant impossible : " + reply->errorString());
        return;
    }

    const QJsonObject response = QJsonDocument::fromJson(data).object();

    if (status != 200) {
        QString message = response.value("error").toObject().value("message").toString();
        if (status == 401) {
            message = "Clé API refusée. Vérifiez ApiKey dans " + configPath();
        }
        emit requestFailed(QString("Erreur de l'assistant (%1) : %2").arg(status).arg(message));
        return;
    }

    const QString stopReason = response.value("stop_reason").toString();
    if (stopReason == "refusal") {
        emit requestFailed("L'assistant a refusé de traiter cette demande.");
        return;
    }
    if (stopReason == "max_tokens") {
        emit requestFailed("La réponse de l'assistant a été tronquée. Reformulez ou raccourcissez la demande.");
        return;
    }

    QString text;
    const QJsonArray content = response.value("content").toArray();
    for (const QJsonValue &block : content) {
        if (block.toObject().value("type").toString() == "text") {
            text += block.toObject().value("text").toString();
        }
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
