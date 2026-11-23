#include "emailservice.h"

#include <QFile>
#include <QSettings>
#include <QSslSocket>
#include <QStandardPaths>

namespace {

struct SmtpConfig {
    QString host;
    int port = 465;
    QString user;
    QString password;
    QString from;
};

QString smtpConfigPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/smtp.ini";
}

bool loadSmtpConfig(SmtpConfig &config, QString &errorMessage)
{
    const QString configPath = smtpConfigPath();

    if (!QFile::exists(configPath)) {
        errorMessage = "Configuration SMTP manquante. Créez le fichier :\n" + configPath +
                        "\n\navec le contenu (exemple Gmail) :\n"
                        "[SMTP]\n"
                        "Host=smtp.gmail.com\n"
                        "Port=465\n"
                        "User=votre_compte@gmail.com\n"
                        "Password=votre_mot_de_passe_application\n"
                        "From=votre_compte@gmail.com";
        return false;
    }

    QSettings settings(configPath, QSettings::IniFormat);
    settings.beginGroup("SMTP");
    config.host = settings.value("Host").toString();
    config.port = settings.value("Port", 465).toInt();
    config.user = settings.value("User").toString();
    config.password = settings.value("Password").toString();
    config.from = settings.value("From").toString();
    settings.endGroup();

    if (config.host.isEmpty() || config.user.isEmpty() || config.password.isEmpty() || config.from.isEmpty()) {
        errorMessage = "Configuration SMTP incomplète dans :\n" + configPath;
        return false;
    }

    return true;
}

}

bool EmailService::sendEmail(const QString &to, const QString &subject, const QString &body, QString &errorMessage)
{
    SmtpConfig config;
    if (!loadSmtpConfig(config, errorMessage)) {
        return false;
    }

    if (!QSslSocket::supportsSsl()) {
        errorMessage = "TLS/SSL indisponible sur ce poste (DLL OpenSSL manquantes à côté de MyDoc.exe).";
        return false;
    }

    QSslSocket socket;
    socket.connectToHostEncrypted(config.host, static_cast<quint16>(config.port));
    if (!socket.waitForEncrypted(8000)) {
        errorMessage = "Connexion au serveur SMTP impossible : " + socket.errorString();
        return false;
    }

    auto readResponse = [&]() -> QString {
        if (!socket.waitForReadyRead(8000)) {
            return QString();
        }
        QByteArray data = socket.readAll();
        while (socket.waitForReadyRead(200)) {
            data += socket.readAll();
        }
        return QString::fromUtf8(data);
    };

    auto sendLine = [&](const QString &line) {
        socket.write((line + "\r\n").toUtf8());
        socket.waitForBytesWritten(5000);
    };

    readResponse();

    sendLine("EHLO localhost");
    readResponse();

    sendLine("AUTH LOGIN");
    readResponse();

    sendLine(QString::fromLatin1(config.user.toUtf8().toBase64()));
    readResponse();

    sendLine(QString::fromLatin1(config.password.toUtf8().toBase64()));
    const QString authResponse = readResponse();
    if (!authResponse.startsWith("235")) {
        errorMessage = "Authentification SMTP refusée. Vérifiez User/Password dans smtp.ini.";
        socket.disconnectFromHost();
        return false;
    }

    sendLine("MAIL FROM:<" + config.from + ">");
    readResponse();

    sendLine("RCPT TO:<" + to + ">");
    const QString rcptResponse = readResponse();
    if (!rcptResponse.startsWith("250")) {
        errorMessage = "Adresse destinataire refusée par le serveur.";
        socket.disconnectFromHost();
        return false;
    }

    sendLine("DATA");
    readResponse();

    const QString message = "From: " + config.from + "\r\n"
                             "To: " + to + "\r\n"
                             "Subject: " + subject + "\r\n"
                             "Content-Type: text/plain; charset=UTF-8\r\n"
                             "\r\n" + body + "\r\n.";
    sendLine(message);
    const QString dataResponse = readResponse();

    sendLine("QUIT");
    socket.disconnectFromHost();
    socket.waitForDisconnected(3000);

    if (!dataResponse.startsWith("250")) {
        errorMessage = "Le serveur SMTP a refusé l'envoi du message.";
        return false;
    }

    return true;
}
