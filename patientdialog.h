#ifndef PATIENTDIALOG_H
#define PATIENTDIALOG_H

#include <QDialog>

#include "databasemanager.h"

class QLineEdit;
class QComboBox;
class QPushButton;

class PatientDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PatientDialog(QWidget *parent = nullptr);
    explicit PatientDialog(const PatientRecord &patient, QWidget *parent = nullptr);

    int patientId() const { return m_patientId; }

private slots:
    void onSaveClicked();

private:
    void setupUi();

    int m_patientId;
    QString m_path;

    QLineEdit *firstNameEdit;
    QLineEdit *lastNameEdit;
    QLineEdit *birthDateEdit;
    QComboBox *sexCombo;
    QLineEdit *phoneEdit;
    QPushButton *saveButton;
    QPushButton *cancelButton;
};

#endif // PATIENTDIALOG_H
