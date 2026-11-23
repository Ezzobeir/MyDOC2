#include "consultationwidget.h"

#include "consultationsessiondialog.h"

#include <QDate>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

ConsultationWidget::ConsultationWidget(const QString &currentLogin, QWidget *parent)
    : QWidget(parent), m_currentLogin(currentLogin)
{
    setupUi();
    refreshWaitingRoom();
}

void ConsultationWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshWaitingRoom();
}

void ConsultationWidget::setupUi()
{
    waitingRoomListWidget = new QListWidget(this);
    waitingRoomListWidget->setMaximumWidth(240);
    connect(waitingRoomListWidget, &QListWidget::currentRowChanged, this, &ConsultationWidget::onPatientSelected);

    QVBoxLayout *leftLayout = new QVBoxLayout;
    leftLayout->addWidget(new QLabel("Liste d'attente du jour :", this));
    leftLayout->addWidget(waitingRoomListWidget);

    patientInfoLabel = new QLabel("Sélectionnez un patient dans la liste d'attente.", this);
    patientInfoLabel->setStyleSheet("font-weight: 600; font-size: 11pt;");
    patientInfoLabel->setWordWrap(true);

    startConsultationButton = new QPushButton("Démarrer une consultation", this);
    startConsultationButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    startConsultationButton->setEnabled(false);
    connect(startConsultationButton, &QPushButton::clicked, this, &ConsultationWidget::onStartConsultationClicked);

    consultationsTable = new QTableWidget(this);
    consultationsTable->setColumnCount(5);
    consultationsTable->setHorizontalHeaderLabels({"Date", "Médecin", "Motif", "Diagnostic", "Traitement"});
    consultationsTable->horizontalHeader()->setStretchLastSection(true);
    consultationsTable->setAlternatingRowColors(true);
    consultationsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    consultationsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QVBoxLayout *rightLayout = new QVBoxLayout;
    rightLayout->addWidget(patientInfoLabel);
    rightLayout->addWidget(startConsultationButton);
    rightLayout->addWidget(new QLabel("Historique des consultations :", this));
    rightLayout->addWidget(consultationsTable);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addLayout(leftLayout);
    mainLayout->addLayout(rightLayout, 1);

    setLayout(mainLayout);
}

void ConsultationWidget::refreshWaitingRoom()
{
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    m_waitingRoomPatients = DatabaseManager::getWaitingRoomPatients(today, {m_currentLogin});

    const int previousRow = waitingRoomListWidget->currentRow();

    waitingRoomListWidget->clear();
    for (const PatientRecord &patient : m_waitingRoomPatients) {
        QString label = patient.firstName + " " + patient.lastName;
        QString tooltip;

        const PreConsultationRecord preConsultation =
            DatabaseManager::getPreConsultation(patient.id, m_currentLogin, today);
        if (preConsultation.done) {
            label = "✔ " + label;
            tooltip = preConsultation.summary();
        }

        QListWidgetItem *item = new QListWidgetItem(label, waitingRoomListWidget);
        item->setToolTip(tooltip);
    }

    if (previousRow >= 0 && previousRow < waitingRoomListWidget->count()) {
        waitingRoomListWidget->setCurrentRow(previousRow);
    }

    refreshSelectedPatientInfo();
}

void ConsultationWidget::onPatientSelected()
{
    refreshSelectedPatientInfo();
}

void ConsultationWidget::refreshSelectedPatientInfo()
{
    const int row = waitingRoomListWidget->currentRow();
    const bool hasSelection = row >= 0 && row < m_waitingRoomPatients.size();

    startConsultationButton->setEnabled(hasSelection);

    if (!hasSelection) {
        patientInfoLabel->setText("Sélectionnez un patient dans la liste d'attente.");
        consultationsTable->setRowCount(0);
        return;
    }

    const PatientRecord &patient = m_waitingRoomPatients[row];

    QString info = patient.firstName + " " + patient.lastName;
    if (!patient.birthDate.isEmpty()) {
        info += " — né(e) le " + patient.birthDate;
    }
    if (!patient.phone.isEmpty()) {
        info += " — tél. " + patient.phone;
    }
    const PreConsultationRecord preConsultation = DatabaseManager::getPreConsultation(
        patient.id, m_currentLogin, QDate::currentDate().toString(Qt::ISODate));
    if (preConsultation.done) {
        info += "\nPré-consultation : " + preConsultation.summary();
    }
    patientInfoLabel->setText(info);

    const QList<ConsultationRecord> consultations = DatabaseManager::getConsultationsForPatient(patient.id);
    const QList<UserRecord> doctors = DatabaseManager::getAllUsers();

    consultationsTable->setRowCount(consultations.size());
    for (int i = 0; i < consultations.size(); ++i) {
        const ConsultationRecord &consultation = consultations[i];

        QString doctorName = consultation.doctorLogin;
        for (const UserRecord &doctor : doctors) {
            if (doctor.login == consultation.doctorLogin) {
                doctorName = (doctor.firstName + " " + doctor.lastName).trimmed();
                if (doctorName.isEmpty()) {
                    doctorName = doctor.login;
                }
                break;
            }
        }

        consultationsTable->setItem(i, 0, new QTableWidgetItem(
            QDate::fromString(consultation.date, Qt::ISODate).toString("dd/MM/yyyy")));
        consultationsTable->setItem(i, 1, new QTableWidgetItem(doctorName));
        consultationsTable->setItem(i, 2, new QTableWidgetItem(consultation.motif));
        consultationsTable->setItem(i, 3, new QTableWidgetItem(consultation.diagnostic));
        consultationsTable->setItem(i, 4, new QTableWidgetItem(consultation.treatment));
    }
}

void ConsultationWidget::onStartConsultationClicked()
{
    const int row = waitingRoomListWidget->currentRow();
    if (row < 0 || row >= m_waitingRoomPatients.size()) {
        return;
    }

    ConsultationSessionDialog dialog(m_waitingRoomPatients[row], m_currentLogin, this);
    if (dialog.exec() == QDialog::Accepted) {
        refreshWaitingRoom();
    }
}
