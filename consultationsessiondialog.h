#ifndef CONSULTATIONSESSIONDIALOG_H
#define CONSULTATIONSESSIONDIALOG_H

#include <QDialog>
#include <QJsonObject>

#include "databasemanager.h"

class OllamaClient;
class QLineEdit;
class QListWidget;
class QTextBrowser;
class QTextEdit;
class QPushButton;

class ConsultationSessionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConsultationSessionDialog(const PatientRecord &patient, const QString &doctorLogin,
                                        QWidget *parent = nullptr);

private slots:
    void onFinishClicked();
    void onInterruptClicked();
    void onPrescriptionClicked();
    void onAssistantSendClicked();
    void onAssistantReply(const QString &reply, const QJsonObject &fields);
    void onAssistantFailed(const QString &message);

private:
    void setupUi();
    QJsonObject currentFormState() const;
    QStringList applyAssistantFields(const QJsonObject &fields);
    void appendChat(const QString &speaker, const QString &text);

    PatientRecord m_patient;
    QString m_doctorLogin;
    int m_prepUserId = 0;

    QLineEdit *heightEdit;
    QLineEdit *weightEdit;
    QLineEdit *systolicEdit;
    QLineEdit *diastolicEdit;
    QLineEdit *glycemiaEdit;
    QLineEdit *temperatureEdit;
    QLineEdit *otherMeasuresEdit;

    QLineEdit *motifEdit;
    QTextEdit *symptomsEdit;
    QTextEdit *diagnosisEdit;

    QPushButton *prescriptionButton;
    QListWidget *prescriptionsList;

    QPushButton *finishButton;
    QPushButton *interruptButton;

    OllamaClient *m_assistant;
    QTextBrowser *chatLog;
    QLineEdit *chatInput;
    QPushButton *chatSendButton;
};

#endif // CONSULTATIONSESSIONDIALOG_H
