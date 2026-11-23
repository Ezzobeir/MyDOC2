#include "waitingroomwidget.h"

#include "preconsultationdialog.h"

#include <QComboBox>
#include <QCompleter>
#include <QDateEdit>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

WaitingRoomWidget::WaitingRoomWidget(const QString &currentLogin, UserRole currentRole, QWidget *parent)
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
    refreshPatientCombo();
    refreshWaitingRoom();
}

QList<UserRecord> WaitingRoomWidget::visibleDoctors() const
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

QString WaitingRoomWidget::selectedDoctorLogin() const
{
    return doctorCombo->currentData().toString();
}

void WaitingRoomWidget::setupUi()
{
    dateEdit = new QDateEdit(QDate::currentDate(), this);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("dd/MM/yyyy");
    connect(dateEdit, &QDateEdit::dateChanged, this, &WaitingRoomWidget::onFilterChanged);

    doctorCombo = new QComboBox(this);
    const QList<UserRecord> doctors = visibleDoctors();
    for (const UserRecord &doctor : doctors) {
        QString label = (doctor.firstName + " " + doctor.lastName).trimmed();
        if (label.isEmpty()) {
            label = doctor.login;
        }
        doctorCombo->addItem(label, doctor.login);
    }
    connect(doctorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &WaitingRoomWidget::onFilterChanged);

    // Un seul médecin : inutile d'afficher le choix.
    doctorLabel = new QLabel("Médecin :", this);
    doctorLabel->setVisible(doctorCombo->count() > 1);
    doctorCombo->setVisible(doctorCombo->count() > 1);

    noDoctorLabel = new QLabel("Aucun médecin n'est associé à votre compte. Contactez un administrateur.", this);
    noDoctorLabel->setWordWrap(true);
    noDoctorLabel->setVisible(doctorCombo->count() == 0);

    QHBoxLayout *filterLayout = new QHBoxLayout;
    filterLayout->addWidget(new QLabel("Date :", this));
    filterLayout->addWidget(dateEdit);
    filterLayout->addSpacing(16);
    filterLayout->addWidget(doctorLabel);
    filterLayout->addWidget(doctorCombo);
    filterLayout->addStretch();

    waitingListWidget = new QListWidget(this);
    connect(waitingListWidget, &QListWidget::currentRowChanged, this, &WaitingRoomWidget::onSelectionChanged);

    preConsultationButton = new QPushButton("Pré-consultation", this);
    preConsultationButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(preConsultationButton, &QPushButton::clicked, this, &WaitingRoomWidget::onPreConsultationClicked);

    removeButton = new QPushButton("Retirer de la liste d'attente", this);
    removeButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(removeButton, &QPushButton::clicked, this, &WaitingRoomWidget::onRemoveClicked);

    QHBoxLayout *listButtonsLayout = new QHBoxLayout;
    listButtonsLayout->addWidget(preConsultationButton);
    listButtonsLayout->addWidget(removeButton);
    listButtonsLayout->addStretch();

    patientCombo = new QComboBox(this);
    patientCombo->setEditable(true);
    patientCombo->setInsertPolicy(QComboBox::NoInsert);
    patientCombo->setMinimumWidth(280);
    patientCombo->lineEdit()->setPlaceholderText("Rechercher un patient...");
    patientCombo->completer()->setFilterMode(Qt::MatchContains);
    patientCombo->completer()->setCaseSensitivity(Qt::CaseInsensitive);

    addButton = new QPushButton("Ajouter à la liste d'attente", this);
    addButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(addButton, &QPushButton::clicked, this, &WaitingRoomWidget::onAddClicked);

    QHBoxLayout *addLayout = new QHBoxLayout;
    addLayout->addWidget(patientCombo);
    addLayout->addWidget(addButton);
    addLayout->addStretch();

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(filterLayout);
    mainLayout->addWidget(noDoctorLabel);
    mainLayout->addWidget(new QLabel("Liste d'attente :", this));
    mainLayout->addWidget(waitingListWidget, 1);
    mainLayout->addLayout(listButtonsLayout);
    mainLayout->addSpacing(16);
    mainLayout->addWidget(new QLabel("Ajouter un patient :", this));
    mainLayout->addLayout(addLayout);

    setLayout(mainLayout);

    const bool hasDoctor = doctorCombo->count() > 0;
    patientCombo->setEnabled(hasDoctor);
    addButton->setEnabled(hasDoctor);
}

void WaitingRoomWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshPatientCombo();
    refreshWaitingRoom();
}

void WaitingRoomWidget::showWaitingRoom(const QDate &date, const QString &doctorLogin)
{
    dateEdit->blockSignals(true);
    dateEdit->setDate(date);
    dateEdit->blockSignals(false);

    const int doctorIndex = doctorCombo->findData(doctorLogin);
    if (doctorIndex >= 0) {
        doctorCombo->blockSignals(true);
        doctorCombo->setCurrentIndex(doctorIndex);
        doctorCombo->blockSignals(false);
    }

    refreshWaitingRoom();
}

void WaitingRoomWidget::refreshPatientCombo()
{
    const QList<PatientRecord> patients = DatabaseManager::getAllPatients();

    patientCombo->clear();
    for (const PatientRecord &patient : patients) {
        QString label = patient.firstName + " " + patient.lastName;
        if (!patient.birthDate.isEmpty()) {
            label += " (" + patient.birthDate + ")";
        }
        patientCombo->addItem(label, patient.id);
    }
    patientCombo->setCurrentIndex(-1);
    patientCombo->clearEditText();
}

void WaitingRoomWidget::refreshWaitingRoom()
{
    const QString doctorLogin = selectedDoctorLogin();

    m_waitingPatients.clear();
    if (!doctorLogin.isEmpty()) {
        m_waitingPatients =
            DatabaseManager::getWaitingRoomPatients(dateEdit->date().toString(Qt::ISODate), {doctorLogin});
    }

    const int previousRow = waitingListWidget->currentRow();

    waitingListWidget->clear();
    for (const PatientRecord &patient : m_waitingPatients) {
        QString label = patient.firstName + " " + patient.lastName;
        QString tooltip;

        const PreConsultationRecord preConsultation = DatabaseManager::getPreConsultation(
            patient.id, doctorLogin, dateEdit->date().toString(Qt::ISODate));
        if (preConsultation.done) {
            label = "✔ " + label;
            tooltip = preConsultation.summary();
        }

        QListWidgetItem *item = new QListWidgetItem(label, waitingListWidget);
        item->setToolTip(tooltip);
    }

    if (previousRow >= 0 && previousRow < waitingListWidget->count()) {
        waitingListWidget->setCurrentRow(previousRow);
    }

    onSelectionChanged();
}

void WaitingRoomWidget::onFilterChanged()
{
    refreshWaitingRoom();
}

void WaitingRoomWidget::onSelectionChanged()
{
    const int row = waitingListWidget->currentRow();
    const bool hasSelection = row >= 0 && row < m_waitingPatients.size();
    preConsultationButton->setEnabled(hasSelection);
    removeButton->setEnabled(hasSelection);
}

void WaitingRoomWidget::onPreConsultationClicked()
{
    const int row = waitingListWidget->currentRow();
    if (row < 0 || row >= m_waitingPatients.size()) {
        return;
    }

    PreConsultationDialog dialog(m_waitingPatients[row], selectedDoctorLogin(), dateEdit->date(), m_currentLogin,
                                 this);
    if (dialog.exec() == QDialog::Accepted) {
        refreshWaitingRoom();
    }
}

void WaitingRoomWidget::onAddClicked()
{
    const QString doctorLogin = selectedDoctorLogin();
    if (doctorLogin.isEmpty()) {
        QMessageBox::warning(this, "Salle d'attente", "Aucun médecin disponible.");
        return;
    }

    const int index = patientCombo->findText(patientCombo->currentText(), Qt::MatchFixedString);
    if (index < 0) {
        QMessageBox::information(this, "Salle d'attente", "Sélectionnez un patient dans la liste.");
        return;
    }

    QString errorMessage;
    if (!DatabaseManager::addToWaitingRoom(patientCombo->itemData(index).toInt(), doctorLogin,
                                            dateEdit->date().toString(Qt::ISODate), errorMessage)) {
        QMessageBox::warning(this, "Salle d'attente", errorMessage);
        return;
    }

    patientCombo->setCurrentIndex(-1);
    patientCombo->clearEditText();
    refreshWaitingRoom();
}

void WaitingRoomWidget::onRemoveClicked()
{
    const int row = waitingListWidget->currentRow();
    if (row < 0 || row >= m_waitingPatients.size()) {
        return;
    }

    QString errorMessage;
    if (!DatabaseManager::removeFromWaitingRoom(m_waitingPatients[row].id, selectedDoctorLogin(),
                                                 dateEdit->date().toString(Qt::ISODate), errorMessage)) {
        QMessageBox::warning(this, "Salle d'attente", errorMessage);
        return;
    }

    refreshWaitingRoom();
}
