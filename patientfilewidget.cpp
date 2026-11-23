#include "patientfilewidget.h"

#include "patientdialog.h"

#include <QDate>
#include <QDateEdit>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QComboBox>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

PatientFileWidget::PatientFileWidget(const QString &currentLogin, UserRole currentRole, QWidget *parent)
    : QWidget(parent), m_currentLogin(currentLogin), m_currentRole(currentRole)
{
    if (m_currentRole == UserRole::Medecin || m_currentRole == UserRole::MedecinAdmin) {
        m_visibleDoctorLogins = {m_currentLogin};
    } else {
        const QList<UserRecord> assignedDoctors = DatabaseManager::getAssignedDoctors(m_currentLogin);
        for (const UserRecord &doctor : assignedDoctors) {
            m_visibleDoctorLogins.append(doctor.login);
        }
    }

    setupUi();
    refreshPatientList();
}

QList<UserRecord> PatientFileWidget::visibleDoctors() const
{
    QList<UserRecord> result;

    const QList<UserRecord> allDoctors = DatabaseManager::getDoctors();
    for (const UserRecord &doctor : allDoctors) {
        if (m_visibleDoctorLogins.contains(doctor.login)) {
            result.append(doctor);
        }
    }

    return result;
}

void PatientFileWidget::setupUi()
{
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("Rechercher un patient...");
    connect(searchEdit, &QLineEdit::textChanged, this, &PatientFileWidget::onSearchTextChanged);

    patientListWidget = new QListWidget(this);
    patientListWidget->setMaximumWidth(240);
    connect(patientListWidget, &QListWidget::currentRowChanged, this, &PatientFileWidget::onPatientSelected);

    newPatientButton = new QPushButton("+ Nouveau patient", this);
    newPatientButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(newPatientButton, &QPushButton::clicked, this, &PatientFileWidget::onNewPatientClicked);

    QVBoxLayout *leftLayout = new QVBoxLayout;
    leftLayout->addWidget(searchEdit);
    leftLayout->addWidget(patientListWidget);
    leftLayout->addWidget(newPatientButton);

    patientInfoLabel = new QLabel("Sélectionnez un patient.", this);
    patientInfoLabel->setStyleSheet("font-weight: 600; font-size: 11pt;");
    patientInfoLabel->setWordWrap(true);

    editPatientButton = new QPushButton("Modifier la fiche", this);
    editPatientButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(editPatientButton, &QPushButton::clicked, this, &PatientFileWidget::onEditPatientClicked);

    doctorCombo = new QComboBox(this);
    const QList<UserRecord> doctors = visibleDoctors();
    for (const UserRecord &doctor : doctors) {
        QString label = (doctor.firstName + " " + doctor.lastName).trimmed();
        if (label.isEmpty()) {
            label = doctor.login;
        }
        doctorCombo->addItem(label, doctor.login);
    }

    dayEdit = new QDateEdit(QDate::currentDate(), this);
    dayEdit->setCalendarPopup(true);
    dayEdit->setDisplayFormat("dd/MM/yyyy");

    addToWaitingRoomButton = new QPushButton("Ajouter à la liste d'attente du jour", this);
    addToWaitingRoomButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(addToWaitingRoomButton, &QPushButton::clicked, this, &PatientFileWidget::onAddToWaitingRoomClicked);

    QHBoxLayout *waitingRoomLayout = new QHBoxLayout;
    waitingRoomLayout->addWidget(new QLabel("Jour :", this));
    waitingRoomLayout->addWidget(dayEdit);
    waitingRoomLayout->addWidget(new QLabel("Médecin :", this));
    waitingRoomLayout->addWidget(doctorCombo);
    waitingRoomLayout->addWidget(addToWaitingRoomButton);
    waitingRoomLayout->addStretch();

    QVBoxLayout *rightLayout = new QVBoxLayout;
    rightLayout->addWidget(patientInfoLabel);
    rightLayout->addWidget(editPatientButton);
    rightLayout->addSpacing(16);
    rightLayout->addLayout(waitingRoomLayout);
    rightLayout->addStretch();

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addLayout(leftLayout);
    mainLayout->addLayout(rightLayout, 1);

    setLayout(mainLayout);
}

void PatientFileWidget::refreshPatientList()
{
    m_allPatients = DatabaseManager::getAllPatients();
    onSearchTextChanged(searchEdit->text());
}

void PatientFileWidget::onSearchTextChanged(const QString &text)
{
    m_filteredPatients.clear();
    patientListWidget->clear();

    for (const PatientRecord &patient : m_allPatients) {
        const QString fullName = patient.firstName + " " + patient.lastName;
        if (text.isEmpty() || fullName.contains(text, Qt::CaseInsensitive)) {
            m_filteredPatients.append(patient);
            patientListWidget->addItem(fullName);
        }
    }

    refreshSelectedPatientInfo();
}

void PatientFileWidget::onPatientSelected()
{
    refreshSelectedPatientInfo();
}

void PatientFileWidget::refreshSelectedPatientInfo()
{
    const int row = patientListWidget->currentRow();
    const bool hasSelection = row >= 0 && row < m_filteredPatients.size();

    editPatientButton->setEnabled(hasSelection);
    addToWaitingRoomButton->setEnabled(hasSelection && doctorCombo->count() > 0);

    if (!hasSelection) {
        patientInfoLabel->setText("Sélectionnez un patient.");
        return;
    }

    const PatientRecord &patient = m_filteredPatients[row];

    QString info = patient.firstName + " " + patient.lastName;
    if (!patient.birthDate.isEmpty()) {
        info += " — né(e) le " + patient.birthDate;
    }
    if (patient.sex == "M") {
        info += " — Masculin";
    } else if (patient.sex == "F") {
        info += " — Féminin";
    }
    if (!patient.phone.isEmpty()) {
        info += " — tél. " + patient.phone;
    }
    patientInfoLabel->setText(info);
}

void PatientFileWidget::onNewPatientClicked()
{
    PatientDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        refreshPatientList();
    }
}

void PatientFileWidget::onEditPatientClicked()
{
    const int row = patientListWidget->currentRow();
    if (row < 0 || row >= m_filteredPatients.size()) {
        QMessageBox::information(this, "Patient", "Sélectionnez un patient à modifier.");
        return;
    }

    PatientDialog dialog(m_filteredPatients[row], this);
    if (dialog.exec() == QDialog::Accepted) {
        refreshPatientList();
    }
}

void PatientFileWidget::onAddToWaitingRoomClicked()
{
    const int row = patientListWidget->currentRow();
    if (row < 0 || row >= m_filteredPatients.size()) {
        QMessageBox::information(this, "Liste d'attente", "Sélectionnez un patient.");
        return;
    }

    if (doctorCombo->count() == 0) {
        QMessageBox::warning(this, "Liste d'attente", "Aucun médecin disponible.");
        return;
    }

    const int patientId = m_filteredPatients[row].id;
    const QString doctorLogin = doctorCombo->currentData().toString();

    QString errorMessage;
    if (!DatabaseManager::addToWaitingRoom(patientId, doctorLogin, dayEdit->date().toString(Qt::ISODate),
                                            errorMessage)) {
        QMessageBox::warning(this, "Liste d'attente", errorMessage);
        return;
    }

    emit addedToWaitingRoom(dayEdit->date(), doctorLogin);
}
