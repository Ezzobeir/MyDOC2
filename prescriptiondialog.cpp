#include "prescriptiondialog.h"

#include "patientfolder.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>

PrescriptionDialog::PrescriptionDialog(const PatientRecord &patient, const QString &doctorLogin, QWidget *parent)
    : QDialog(parent), m_patient(patient), m_doctorLogin(doctorLogin)
{
    setWindowTitle("Ordonnance — " + patient.firstName + " " + patient.lastName);
    setupUi();
}

void PrescriptionDialog::setupUi()
{
    QLabel *titleLabel = new QLabel(m_patient.firstName + " " + m_patient.lastName, this);
    titleLabel->setStyleSheet("font-weight: 600; font-size: 12pt;");

    prescriptionEdit = new QTextEdit(this);
    prescriptionEdit->setPlaceholderText("Rédigez l'ordonnance...");

    saveButton = new QPushButton("Enregistrer", this);
    saveButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(saveButton, &QPushButton::clicked, this, &PrescriptionDialog::onSaveClicked);

    cancelButton = new QPushButton("Annuler", this);
    cancelButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(saveButton);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(prescriptionEdit);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
    resize(520, 480);
}

void PrescriptionDialog::onSaveClicked()
{
    const QString text = prescriptionEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        QMessageBox::warning(this, "Ordonnance", "L'ordonnance est vide.");
        return;
    }

    QString errorMessage;
    const QString folder = PatientFolder::ensure(m_patient, errorMessage);
    if (folder.isEmpty()) {
        QMessageBox::warning(this, "Ordonnance",
                              errorMessage + "\n\nL'ordonnance n'a pas été enregistrée, elle reste dans cette fenêtre.");
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();
    const QDir dir(folder);
    const QString baseName = "Ordonnance_" + now.toString("yyyy-MM-dd_HHmm");
    QString fileName = baseName + ".txt";
    for (int suffix = 2; dir.exists(fileName); ++suffix) {
        fileName = baseName + "_" + QString::number(suffix) + ".txt";
    }

    QString doctorName = m_doctorLogin;
    const QList<UserRecord> users = DatabaseManager::getAllUsers();
    for (const UserRecord &user : users) {
        if (user.login == m_doctorLogin) {
            const QString fullName = (user.firstName + " " + user.lastName).trimmed();
            if (!fullName.isEmpty()) {
                doctorName = fullName;
            }
            break;
        }
    }

    QFile file(dir.filePath(fileName));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ordonnance",
                              "Impossible de créer le fichier :\n" + file.fileName() +
                                  "\n\nL'ordonnance n'a pas été enregistrée, elle reste dans cette fenêtre.");
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);
    out << "ORDONNANCE\n==========\n\n";
    out << "Patient : " << m_patient.firstName << " " << m_patient.lastName << "\n";
    if (!m_patient.birthDate.isEmpty()) {
        out << "Date de naissance : " << m_patient.birthDate << "\n";
    }
    out << "Médecin : " << doctorName << "\n";
    out << "Date : " << now.toString("dd/MM/yyyy HH:mm") << "\n\n";
    out << "----------------------------------------\n\n";
    out << text << "\n";
    out.flush();

    if (file.error() != QFileDevice::NoError) {
        QMessageBox::warning(this, "Ordonnance",
                              "Erreur d'écriture :\n" + file.errorString() +
                                  "\n\nL'ordonnance n'a pas été enregistrée, elle reste dans cette fenêtre.");
        return;
    }
    file.close();

    m_savedFilePath = QFileInfo(file).absoluteFilePath();
    accept();
}
