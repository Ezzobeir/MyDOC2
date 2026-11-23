#ifndef OLLAMACLIENT_H
#define OLLAMACLIENT_H

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class QNetworkReply;

// Assistant de saisie de la consultation : envoie le message du médecin et l'état du formulaire à un modèle
// Ollama local, reçoit une réponse courte et les champs à mettre à jour (JSON structuré).
// Aucune donnée ne quitte le poste : le serveur Ollama tourne en local.
//
// Configuration (facultative) : fichier ollama.ini à côté de cabinet.db
//   [Ollama]
//   Host=http://127.0.0.1:11434
//   Model=qwen3.5:4b
//   UseGpu=false     (false par défaut : calcul sur le processeur ; true pour utiliser la carte graphique)
class OllamaClient : public QObject
{
    Q_OBJECT

public:
    explicit OllamaClient(QObject *parent = nullptr);

    static QString configPath();

    // Chaîne vide si tout est prêt, sinon l'explication de ce qui manque.
    QString configurationProblem() const;

    bool isBusy() const { return m_pending != nullptr; }

    // formState : valeurs actuelles du formulaire (aucune donnée d'identité du patient).
    void sendMessage(const QString &doctorText, const QJsonObject &formState);

signals:
    // fields : uniquement les clés renseignées par le modèle (les valeurs nulles sont retirées).
    void replyReceived(const QString &reply, const QJsonObject &fields);
    void requestFailed(const QString &message);

private slots:
    void onFinished();

private:
    QString host() const;
    QString model() const;
    bool useGpu() const;

    QNetworkAccessManager m_network;
    QNetworkReply *m_pending = nullptr;
    QJsonArray m_history;
    QString m_pendingDoctorText;
};

#endif // OLLAMACLIENT_H
