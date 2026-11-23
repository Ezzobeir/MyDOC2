#ifndef PRESCRIPTIONDIALOG_H
#define PRESCRIPTIONDIALOG_H

#include <QDialog>

#include "databasemanager.h"

class QTextEdit;
class QPushButton;

class PrescriptionDialog : public QDialog
{
    Q_OBJECT

public:
    PrescriptionDialog(const PatientRecord &patient, const QString &doctorLogin, QWidget *parent = nullptr);

    // Chemin du fichier .txt créé, valable après acceptation du dialogue.
    QString savedFilePath() const { return m_savedFilePath; }

private slots:
    void onSaveClicked();

private:
    void setupUi();

    PatientRecord m_patient;
    QString m_doctorLogin;
    QString m_savedFilePath;

    QTextEdit *prescriptionEdit;
    QPushButton *saveButton;
    QPushButton *cancelButton;
};

#endif // PRESCRIPTIONDIALOG_H
