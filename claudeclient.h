#ifndef CLAUDECLIENT_H
#define CLAUDECLIENT_H

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class QNetworkReply;

// Assistant de saisie de la consultation : envoie le message du médecin et l'état du formulaire à l'API Claude,
// reçoit une réponse courte et les champs à mettre à jour (JSON structuré).
//
// Configuration : fichier claude.ini à côté de cabinet.db
//   [Claude]
//   ApiKey=sk-ant-...
//   Model=claude-opus-5-5      (facultatif)
// À défaut, la clé est lue dans la variable d'environnement ANTHROPIC_API_KEY.
class ClaudeClient : public QObject
{
    Q_OBJECT

public:
    explicit ClaudeClient(QObject *parent = nullptr);

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
    QString apiKey() const;
    QString model() const;

    QNetworkAccessManager m_network;
    QNetworkReply *m_pending = nullptr;
    QJsonArray m_history;
    QString m_pendingDoctorText;
};

#endif // CLAUDECLIENT_H
