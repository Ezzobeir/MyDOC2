#include "appointmentswidget.h"

#include "databasemanager.h"
#include "patientdialog.h"

#include <QCalendarWidget>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextCharFormat>
#include <QTimeEdit>
#include <QVBoxLayout>

AppointmentsWidget::AppointmentsWidget(const QString &currentLogin, UserRole currentRole, QWidget *parent)
    : QWidget(parent), m_currentLogin(currentLogin), m_currentRole(currentRole), m_selectedDate(QDate::currentDate())
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
    refreshCalendarHighlights();
    refreshAppointmentsForSelectedDate();
    calendar->setFocus();
}

QList<UserRecord> AppointmentsWidget::visibleDoctors() const
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

void AppointmentsWidget::setupUi()
{
    calendar = new QCalendarWidget(this);
    calendar->setSelectedDate(m_selectedDate);
    calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    calendar->setMaximumWidth(320);
    calendar->setMaximumHeight(280);
    connect(calendar, &QCalendarWidget::clicked, this, &AppointmentsWidget::onDateSelected);
    connect(calendar, &QCalendarWidget::currentPageChanged, this, [this]() { refreshCalendarHighlights(); });

    selectedDateLabel = new QLabel(this);
    selectedDateLabel->setStyleSheet("font-weight: 600; font-size: 11pt;");

    appointmentsTable = new QTableWidget(this);
    appointmentsTable->setColumnCount(4);
    appointmentsTable->setHorizontalHeaderLabels({"Heure", "Patient / Objet", "Médecin", "Motif"});
    appointmentsTable->horizontalHeader()->setStretchLastSection(true);
    appointmentsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    appointmentsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    appointmentsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    appointmentsTable->setAlternatingRowColors(true);

    deleteButton = new QPushButton("Supprimer le rendez-vous sélectionné", this);
    deleteButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(deleteButton, &QPushButton::clicked, this, &AppointmentsWidget::onDeleteAppointmentClicked);

    noDoctorLabel = new QLabel("Aucun médecin associé à votre compte. Contactez un administrateur.", this);
    noDoctorLabel->setStyleSheet("color: #B91C1C;");
    noDoctorLabel->setWordWrap(true);
    noDoctorLabel->setVisible(m_visibleDoctorLogins.isEmpty());

    appointmentTypeCombo = new QComboBox(this);
    appointmentTypeCombo->addItem("Rendez-vous patient", "patient");
    appointmentTypeCombo->addItem("Rendez-vous autre", "autre");
    connect(appointmentTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AppointmentsWidget::onAppointmentTypeChanged);

    patientCombo = new QComboBox(this);
    refreshPatientCombo();

    newPatientButton = new QPushButton("+ Nouveau patient", this);
    newPatientButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(newPatientButton, &QPushButton::clicked, this, &AppointmentsWidget::onNewPatientClicked);

    QWidget *patientFieldsWidget = new QWidget(this);
    QHBoxLayout *patientLayout = new QHBoxLayout(patientFieldsWidget);
    patientLayout->setContentsMargins(0, 0, 0, 0);
    patientLayout->addWidget(patientCombo, 1);
    patientLayout->addWidget(newPatientButton);

    otherLabelEdit = new QLineEdit(this);
    otherLabelEdit->setPlaceholderText("Intitulé (ex: réunion, formation...)");

    appointmentDetailsStack = new QStackedWidget(this);
    appointmentDetailsStack->addWidget(patientFieldsWidget);
    appointmentDetailsStack->addWidget(otherLabelEdit);

    doctorCombo = new QComboBox(this);
    const QList<UserRecord> doctors = visibleDoctors();
    for (const UserRecord &doctor : doctors) {
        QString label = (doctor.firstName + " " + doctor.lastName).trimmed();
        if (label.isEmpty()) {
            label = doctor.login;
        }
        doctorCombo->addItem(label, doctor.login);
    }

    startTimeEdit = new QTimeEdit(this);
    startTimeEdit->setDisplayFormat("HH:mm");
    startTimeEdit->setTime(QTime(9, 0));

    durationCombo = new QComboBox(this);
    durationCombo->addItem("15 min", 15);
    durationCombo->addItem("30 min", 30);
    durationCombo->addItem("45 min", 45);
    durationCombo->addItem("60 min", 60);
    durationCombo->setCurrentIndex(1);

    notesEdit = new QLineEdit(this);

    addButton = new QPushButton("Ajouter le rendez-vous", this);
    addButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(addButton, &QPushButton::clicked, this, &AppointmentsWidget::onAddAppointmentClicked);

    QFormLayout *formLayout = new QFormLayout;
    formLayout->addRow("Type :", appointmentTypeCombo);
    formLayout->addRow("Détails :", appointmentDetailsStack);
    formLayout->addRow("Médecin :", doctorCombo);
    formLayout->addRow("Heure :", startTimeEdit);
    formLayout->addRow("Durée :", durationCombo);
    formLayout->addRow("Motif :", notesEdit);

    QHBoxLayout *tableActionsLayout = new QHBoxLayout;
    tableActionsLayout->addWidget(deleteButton);
    tableActionsLayout->addStretch();

    QVBoxLayout *rightLayout = new QVBoxLayout;
    rightLayout->addWidget(selectedDateLabel);
    rightLayout->addWidget(appointmentsTable);
    rightLayout->addLayout(tableActionsLayout);
    rightLayout->addWidget(new QLabel("Nouveau rendez-vous :", this));
    rightLayout->addWidget(noDoctorLabel);
    rightLayout->addLayout(formLayout);
    rightLayout->addWidget(addButton);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addWidget(calendar, 0, Qt::AlignTop);
    mainLayout->addLayout(rightLayout, 1);

    setLayout(mainLayout);
}

void AppointmentsWidget::refreshPatientCombo(int selectPatientId)
{
    patientCombo->clear();

    const QList<PatientRecord> patients = DatabaseManager::getAllPatients();
    int indexToSelect = -1;

    for (int i = 0; i < patients.size(); ++i) {
        const PatientRecord &patient = patients[i];
        patientCombo->addItem(patient.firstName + " " + patient.lastName, patient.id);
        if (selectPatientId > 0 && patient.id == selectPatientId) {
            indexToSelect = i;
        }
    }

    if (indexToSelect >= 0) {
        patientCombo->setCurrentIndex(indexToSelect);
    }
}

void AppointmentsWidget::onNewPatientClicked()
{
    PatientDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        refreshPatientCombo(dialog.patientId());
    }
}

void AppointmentsWidget::onAppointmentTypeChanged(int index)
{
    appointmentDetailsStack->setCurrentIndex(index);
}

void AppointmentsWidget::onDateSelected(const QDate &date)
{
    m_selectedDate = date;
    refreshAppointmentsForSelectedDate();
}

void AppointmentsWidget::refreshCalendarHighlights()
{
    QTextCharFormat defaultFormat;
    for (const QString &dateStr : m_highlightedDates) {
        calendar->setDateTextFormat(QDate::fromString(dateStr, Qt::ISODate), defaultFormat);
    }

    m_highlightedDates = DatabaseManager::getAppointmentDates(m_visibleDoctorLogins);

    QTextCharFormat highlightFormat;
    highlightFormat.setBackground(QColor("#DC2626"));
    highlightFormat.setForeground(QColor("#FFFFFF"));
    highlightFormat.setFontWeight(QFont::Bold);

    for (const QString &dateStr : m_highlightedDates) {
        calendar->setDateTextFormat(QDate::fromString(dateStr, Qt::ISODate), highlightFormat);
    }
}

void AppointmentsWidget::refreshAppointmentsForSelectedDate()
{
    selectedDateLabel->setText("Rendez-vous du " + m_selectedDate.toString("dd/MM/yyyy"));

    m_appointments =
        DatabaseManager::getAppointmentsForDate(m_selectedDate.toString(Qt::ISODate), m_visibleDoctorLogins);
    const QList<UserRecord> doctors = visibleDoctors();

    appointmentsTable->setRowCount(m_appointments.size());
    for (int i = 0; i < m_appointments.size(); ++i) {
        const AppointmentRecord &appointment = m_appointments[i];

        QString doctorName = appointment.doctorLogin;
        for (const UserRecord &doctor : doctors) {
            if (doctor.login == appointment.doctorLogin) {
                doctorName = (doctor.firstName + " " + doctor.lastName).trimmed();
                if (doctorName.isEmpty()) {
                    doctorName = doctor.login;
                }
                break;
            }
        }

        auto *timeItem = new QTableWidgetItem(
            appointment.startTime + " (" + QString::number(appointment.durationMinutes) + " min)");
        timeItem->setData(Qt::UserRole, appointment.id);
        appointmentsTable->setItem(i, 0, timeItem);
        appointmentsTable->setItem(i, 1, new QTableWidgetItem(appointment.patientName));
        appointmentsTable->setItem(i, 2, new QTableWidgetItem(doctorName));
        appointmentsTable->setItem(i, 3, new QTableWidgetItem(appointment.notes));
    }
}

void AppointmentsWidget::onAddAppointmentClicked()
{
    if (doctorCombo->count() == 0) {
        QMessageBox::warning(this, "Rendez-vous", "Aucun médecin disponible pour créer un rendez-vous.");
        return;
    }

    const QString type = appointmentTypeCombo->currentData().toString();
    const QString doctorLogin = doctorCombo->currentData().toString();
    const QString startTime = startTimeEdit->time().toString("HH:mm");
    const int duration = durationCombo->currentData().toInt();
    const QString notes = notesEdit->text().trimmed();

    QString errorMessage;
    if (type == "patient") {
        if (patientCombo->count() == 0) {
            QMessageBox::warning(this, "Rendez-vous", "Créez d'abord un patient.");
            return;
        }

        const int patientId = patientCombo->currentData().toInt();
        const QString patientName = patientCombo->currentText();

        if (!DatabaseManager::createAppointment(patientId, patientName, doctorLogin,
                                                 m_selectedDate.toString(Qt::ISODate), startTime, duration, notes,
                                                 errorMessage)) {
            QMessageBox::warning(this, "Rendez-vous", errorMessage);
            return;
        }
    } else {
        const QString label = otherLabelEdit->text().trimmed();

        if (!DatabaseManager::createOtherAppointment(label, doctorLogin, m_selectedDate.toString(Qt::ISODate),
                                                       startTime, duration, notes, errorMessage)) {
            QMessageBox::warning(this, "Rendez-vous", errorMessage);
            return;
        }

        otherLabelEdit->clear();
    }

    notesEdit->clear();

    refreshCalendarHighlights();
    refreshAppointmentsForSelectedDate();
}

void AppointmentsWidget::onDeleteAppointmentClicked()
{
    const int row = appointmentsTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, "Rendez-vous", "Sélectionnez un rendez-vous à supprimer.");
        return;
    }

    const int id = appointmentsTable->item(row, 0)->data(Qt::UserRole).toInt();

    if (QMessageBox::question(this, "Rendez-vous", "Supprimer ce rendez-vous ?") != QMessageBox::Yes) {
        return;
    }

    QString errorMessage;
    if (!DatabaseManager::deleteAppointment(id, errorMessage)) {
        QMessageBox::warning(this, "Rendez-vous", errorMessage);
        return;
    }

    refreshCalendarHighlights();
    refreshAppointmentsForSelectedDate();
}
