#include "patientdialog.h"

#include "patientfolder.h"

#include <QComboBox>
#include <QDir>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

PatientDialog::PatientDialog(QWidget *parent)
    : QDialog(parent), m_patientId(-1)
{
    setWindowTitle("Nouveau patient");
    setupUi();
}

PatientDialog::PatientDialog(const PatientRecord &patient, QWidget *parent)
    : QDialog(parent), m_patientId(patient.id), m_path(patient.path)
{
    setWindowTitle("Modifier le patient");
    setupUi();

    firstNameEdit->setText(patient.firstName);
    lastNameEdit->setText(patient.lastName);
    birthDateEdit->setText(patient.birthDate);
    sexCombo->setCurrentIndex(qMax(0, sexCombo->findData(patient.sex)));
    phoneEdit->setText(patient.phone);
}

void PatientDialog::setupUi()
{
    firstNameEdit = new QLineEdit(this);
    lastNameEdit = new QLineEdit(this);

    birthDateEdit = new QLineEdit(this);
    birthDateEdit->setPlaceholderText("jj/mm/aaaa");

    sexCombo = new QComboBox(this);
    sexCombo->addItem("— Choisir —", QString());
    sexCombo->addItem("Masculin", QString("M"));
    sexCombo->addItem("Féminin", QString("F"));

    phoneEdit = new QLineEdit(this);

    saveButton = new QPushButton("Enregistrer", this);
    saveButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(saveButton, &QPushButton::clicked, this, &PatientDialog::onSaveClicked);

    cancelButton = new QPushButton("Annuler", this);
    cancelButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    QFormLayout *formLayout = new QFormLayout;
    formLayout->addRow("Prénom :", firstNameEdit);
    formLayout->addRow("Nom :", lastNameEdit);
    formLayout->addRow("Date de naissance :", birthDateEdit);
    formLayout->addRow("Sexe :", sexCombo);
    formLayout->addRow("Téléphone :", phoneEdit);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(saveButton);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(formLayout);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void PatientDialog::onSaveClicked()
{
    const QString firstName = firstNameEdit->text().trimmed();
    const QString lastName = lastNameEdit->text().trimmed();
    const QString birthDate = birthDateEdit->text().trimmed();
    const QString phone = phoneEdit->text().trimmed();
    const QString sex = sexCombo->currentData().toString();

    if (sex.isEmpty()) {
        QMessageBox::warning(this, "Patient", "Le sexe est obligatoire.");
        return;
    }

    QString errorMessage;
    if (m_patientId <= 0) {
        int newPatientId = -1;
        if (!DatabaseManager::createPatient(firstName, lastName, birthDate, phone, newPatientId, errorMessage,
                                            sex)) {
            QMessageBox::warning(this, "Patient", errorMessage);
            return;
        }
        m_patientId = newPatientId;

        // Dossier du patient : ordonnances, examens, etc. Le patient reste enregistré même si la création échoue.
        const QString folderPath = PatientFolder::pathFor(m_patientId, firstName, lastName, birthDate);
        if (QDir().mkpath(folderPath)) {
            m_path = folderPath;
            if (!DatabaseManager::updatePatientDetails(m_patientId, sex, m_path, errorMessage)) {
                QMessageBox::warning(this, "Patient",
                                      "Patient enregistré, mais le chemin du dossier n'a pas pu être mémorisé :\n" +
                                          errorMessage);
            }
        } else {
            QMessageBox::warning(this, "Patient",
                                  "Patient enregistré, mais le dossier n'a pas pu être créé :\n" + folderPath);
        }
    } else {
        if (!DatabaseManager::updatePatient(m_patientId, firstName, lastName, birthDate, phone, errorMessage) ||
            !DatabaseManager::updatePatientDetails(m_patientId, sex, m_path, errorMessage)) {
            QMessageBox::warning(this, "Patient", errorMessage);
            return;
        }
    }

    accept();
}
