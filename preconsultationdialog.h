#ifndef PRECONSULTATIONDIALOG_H
#define PRECONSULTATIONDIALOG_H

#include <QDate>
#include <QDialog>

#include "databasemanager.h"

class QLineEdit;
class QPushButton;

class PreConsultationDialog : public QDialog
{
    Q_OBJECT

public:
    PreConsultationDialog(const PatientRecord &patient, const QString &doctorLogin, const QDate &date,
                          const QString &userLogin, QWidget *parent = nullptr);

private slots:
    void onSaveClicked();

private:
    void setupUi();

    PatientRecord m_patient;
    QString m_doctorLogin;
    QDate m_date;
    QString m_userLogin;

    QLineEdit *heightEdit;
    QLineEdit *weightEdit;
    QLineEdit *systolicEdit;
    QLineEdit *diastolicEdit;
    QPushButton *saveButton;
    QPushButton *cancelButton;
};

#endif // PRECONSULTATIONDIALOG_H
