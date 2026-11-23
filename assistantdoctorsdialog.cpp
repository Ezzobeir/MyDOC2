#include "assistantdoctorsdialog.h"

#include "databasemanager.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

AssistantDoctorsDialog::AssistantDoctorsDialog(const QString &assistantLogin, QWidget *parent)
    : QDialog(parent), m_assistantLogin(assistantLogin)
{
    setWindowTitle("Médecins associés");
    setupUi();
}

void AssistantDoctorsDialog::setupUi()
{
    doctorListWidget = new QListWidget(this);

    const QList<UserRecord> allDoctors = DatabaseManager::getDoctors();
    const QList<UserRecord> assignedDoctors = DatabaseManager::getAssignedDoctors(m_assistantLogin);

    QStringList assignedLogins;
    for (const UserRecord &doctor : assignedDoctors) {
        assignedLogins.append(doctor.login);
    }

    for (const UserRecord &doctor : allDoctors) {
        QString label = (doctor.firstName + " " + doctor.lastName).trimmed();
        if (label.isEmpty()) {
            label = doctor.login;
        }

        QListWidgetItem *item = new QListWidgetItem(label, doctorListWidget);
        item->setData(Qt::UserRole, doctor.login);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(assignedLogins.contains(doctor.login) ? Qt::Checked : Qt::Unchecked);
    }

    saveButton = new QPushButton("Enregistrer", this);
    saveButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(saveButton, &QPushButton::clicked, this, &AssistantDoctorsDialog::onSaveClicked);

    cancelButton = new QPushButton("Annuler", this);
    cancelButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(saveButton);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(doctorListWidget);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
    resize(350, 300);
}

void AssistantDoctorsDialog::onSaveClicked()
{
    const QList<UserRecord> currentlyAssigned = DatabaseManager::getAssignedDoctors(m_assistantLogin);
    QStringList currentlyAssignedLogins;
    for (const UserRecord &doctor : currentlyAssigned) {
        currentlyAssignedLogins.append(doctor.login);
    }

    for (int i = 0; i < doctorListWidget->count(); ++i) {
        QListWidgetItem *item = doctorListWidget->item(i);
        const QString doctorLogin = item->data(Qt::UserRole).toString();
        const bool checked = item->checkState() == Qt::Checked;
        const bool wasAssigned = currentlyAssignedLogins.contains(doctorLogin);

        QString errorMessage;
        if (checked && !wasAssigned) {
            DatabaseManager::assignDoctor(m_assistantLogin, doctorLogin, errorMessage);
        } else if (!checked && wasAssigned) {
            DatabaseManager::unassignDoctor(m_assistantLogin, doctorLogin, errorMessage);
        }
    }

    accept();
}
