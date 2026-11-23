#include "usermanagerdialog.h"

#include "emailservice.h"
#include "assistantdoctorsdialog.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {

void populateRoleCombo(QComboBox *combo)
{
    combo->addItem(DatabaseManager::roleDisplayName(UserRole::Medecin), static_cast<int>(UserRole::Medecin));
    combo->addItem(DatabaseManager::roleDisplayName(UserRole::MedecinAdmin), static_cast<int>(UserRole::MedecinAdmin));
    combo->addItem(DatabaseManager::roleDisplayName(UserRole::Assistant), static_cast<int>(UserRole::Assistant));
    combo->addItem(DatabaseManager::roleDisplayName(UserRole::AssistantAdmin), static_cast<int>(UserRole::AssistantAdmin));
}

}

UserManagerDialog::UserManagerDialog(const QString &currentLogin, QWidget *parent)
    : QDialog(parent), m_currentLogin(currentLogin)
{
    setWindowTitle("Gestion des utilisateurs");
    setupUi();
    refreshUserList();

    QTimer *statusRefreshTimer = new QTimer(this);
    connect(statusRefreshTimer, &QTimer::timeout, this, &UserManagerDialog::refreshConnectionStatus);
    statusRefreshTimer->start(2000);
}

void UserManagerDialog::setupUi()
{
    userTable = new QTableWidget(this);
    userTable->setColumnCount(8);
    userTable->setHorizontalHeaderLabels(
        {"Prénom", "Nom", "Login", "Email", "Profil", "Statut", "Première connexion", "Dernière connexion"});
    userTable->horizontalHeader()->setStretchLastSection(true);
    userTable->setAlternatingRowColors(true);
    userTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    userTable->setSelectionMode(QAbstractItemView::SingleSelection);
    userTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    applyButton = new QPushButton("Appliquer", this);
    applyButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(applyButton, &QPushButton::clicked, this, &UserManagerDialog::onApplyChangesClicked);

    deleteButton = new QPushButton("Supprimer l'utilisateur sélectionné", this);
    deleteButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(deleteButton, &QPushButton::clicked, this, &UserManagerDialog::onDeleteUserClicked);

    assignedDoctorsButton = new QPushButton("Médecins associés", this);
    assignedDoctorsButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(assignedDoctorsButton, &QPushButton::clicked, this, &UserManagerDialog::onManageAssignedDoctorsClicked);

    newFirstNameEdit = new QLineEdit(this);
    newLastNameEdit = new QLineEdit(this);

    newLoginEdit = new QLineEdit(this);
    newEmailEdit = new QLineEdit(this);

    newRoleCombo = new QComboBox(this);
    populateRoleCombo(newRoleCombo);

    addButton = new QPushButton("Ajouter", this);
    addButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(addButton, &QPushButton::clicked, this, &UserManagerDialog::onAddUserClicked);

    QFormLayout *formLayout = new QFormLayout;
    formLayout->addRow("Prénom :", newFirstNameEdit);
    formLayout->addRow("Nom :", newLastNameEdit);
    formLayout->addRow("Login :", newLoginEdit);
    formLayout->addRow("Email :", newEmailEdit);
    formLayout->addRow("Profil :", newRoleCombo);

    QHBoxLayout *actionsLayout = new QHBoxLayout;
    actionsLayout->addWidget(applyButton);
    actionsLayout->addWidget(deleteButton);
    actionsLayout->addWidget(assignedDoctorsButton);
    actionsLayout->addStretch();

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(new QLabel(
        "Double-cliquez sur une cellule pour la modifier, puis cliquez sur \"Appliquer\".", this));
    mainLayout->addWidget(userTable);
    mainLayout->addLayout(actionsLayout);
    mainLayout->addWidget(new QLabel("Nouvel utilisateur :", this));
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(addButton);

    setLayout(mainLayout);
    resize(750, 450);
}

void UserManagerDialog::refreshUserList()
{
    m_users = DatabaseManager::getAllUsers();

    userTable->setRowCount(m_users.size());
    for (int i = 0; i < m_users.size(); ++i) {
        userTable->setItem(i, 0, new QTableWidgetItem(m_users[i].firstName));
        userTable->setItem(i, 1, new QTableWidgetItem(m_users[i].lastName));
        userTable->setItem(i, 2, new QTableWidgetItem(m_users[i].login));
        userTable->setItem(i, 3, new QTableWidgetItem(m_users[i].email));

        QComboBox *roleCombo = new QComboBox(userTable);
        populateRoleCombo(roleCombo);
        roleCombo->setCurrentIndex(roleCombo->findData(static_cast<int>(m_users[i].role)));
        userTable->setCellWidget(i, 4, roleCombo);

        QTableWidgetItem *statusItem = new QTableWidgetItem(m_users[i].isConnected ? "Connecté" : "Déconnecté");
        statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);
        userTable->setItem(i, 5, statusItem);

        QTableWidgetItem *firstLoginItem =
            new QTableWidgetItem(m_users[i].firstLoginAt.isEmpty() ? "-" : m_users[i].firstLoginAt);
        firstLoginItem->setFlags(firstLoginItem->flags() & ~Qt::ItemIsEditable);
        userTable->setItem(i, 6, firstLoginItem);

        QTableWidgetItem *lastLoginItem =
            new QTableWidgetItem(m_users[i].lastLoginAt.isEmpty() ? "-" : m_users[i].lastLoginAt);
        lastLoginItem->setFlags(lastLoginItem->flags() & ~Qt::ItemIsEditable);
        userTable->setItem(i, 7, lastLoginItem);
    }
}

void UserManagerDialog::refreshConnectionStatus()
{
    const QList<UserRecord> freshUsers = DatabaseManager::getAllUsers();

    if (freshUsers.size() != m_users.size()) {
        refreshUserList();
        return;
    }

    for (int row = 0; row < freshUsers.size(); ++row) {
        if (freshUsers[row].login != m_users[row].login) {
            refreshUserList();
            return;
        }

        m_users[row].isConnected = freshUsers[row].isConnected;
        m_users[row].firstLoginAt = freshUsers[row].firstLoginAt;
        m_users[row].lastLoginAt = freshUsers[row].lastLoginAt;

        userTable->item(row, 5)->setText(m_users[row].isConnected ? "Connecté" : "Déconnecté");
        userTable->item(row, 6)->setText(m_users[row].firstLoginAt.isEmpty() ? "-" : m_users[row].firstLoginAt);
        userTable->item(row, 7)->setText(m_users[row].lastLoginAt.isEmpty() ? "-" : m_users[row].lastLoginAt);
    }
}

void UserManagerDialog::onAddUserClicked()
{
    const QString firstName = newFirstNameEdit->text().trimmed();
    const QString lastName = newLastNameEdit->text().trimmed();
    const QString login = newLoginEdit->text().trimmed();
    const QString email = newEmailEdit->text().trimmed();
    const UserRole role = static_cast<UserRole>(newRoleCombo->currentData().toInt());

    const QString password = DatabaseManager::generateRandomPassword();

    QString errorMessage;
    if (!DatabaseManager::createUser(login, password, firstName, lastName, email, role, errorMessage)) {
        QMessageBox::warning(this, "Ajout utilisateur", errorMessage);
        return;
    }

    const QString subject = "MyDoc - Création de votre compte";
    const QString body = "Bonjour " + firstName + ",\n\nVotre compte MyDoc a été créé.\n\n"
                          "Login : " + login + "\nMot de passe temporaire : " + password +
                          "\n\nVous devrez le changer dès votre première connexion.\n\nMyDoc - Cabinet médical";

    QString emailError;
    if (!EmailService::sendEmail(email, subject, body, emailError)) {
        QMessageBox::warning(this, "Ajout utilisateur",
                              "Utilisateur créé, mais l'email n'a pas pu être envoyé :\n" + emailError +
                              "\n\nMot de passe temporaire (à communiquer manuellement) : " + password);
    } else {
        QMessageBox::information(this, "Ajout utilisateur",
                                  "Utilisateur créé. Un email avec le mot de passe temporaire a été envoyé à " +
                                  email + ".");
    }

    newFirstNameEdit->clear();
    newLastNameEdit->clear();
    newLoginEdit->clear();
    newEmailEdit->clear();
    refreshUserList();
}

void UserManagerDialog::onApplyChangesClicked()
{
    QStringList errors;

    for (int row = 0; row < m_users.size(); ++row) {
        const QString originalLogin = m_users[row].login;

        const QString firstName = userTable->item(row, 0)->text().trimmed();
        const QString lastName = userTable->item(row, 1)->text().trimmed();
        const QString newLogin = userTable->item(row, 2)->text().trimmed();
        const QString email = userTable->item(row, 3)->text().trimmed();

        QComboBox *roleCombo = qobject_cast<QComboBox *>(userTable->cellWidget(row, 4));
        const UserRole role = static_cast<UserRole>(roleCombo->currentData().toInt());

        QString errorMessage;
        if (!DatabaseManager::updateUser(originalLogin, newLogin, firstName, lastName, email, role, errorMessage)) {
            errors << originalLogin + " : " + errorMessage;
        }
    }

    refreshUserList();

    if (!errors.isEmpty()) {
        QMessageBox::warning(this, "Application des modifications", errors.join("\n"));
    } else {
        QMessageBox::information(this, "Application des modifications", "Modifications enregistrées.");
    }
}

void UserManagerDialog::onManageAssignedDoctorsClicked()
{
    const int row = userTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, "Médecins associés", "Sélectionnez un assistant.");
        return;
    }

    const UserRole role = m_users[row].role;
    if (role != UserRole::Assistant && role != UserRole::AssistantAdmin) {
        QMessageBox::information(this, "Médecins associés",
                                  "Cette fonctionnalité concerne uniquement les profils assistant.");
        return;
    }

    AssistantDoctorsDialog dialog(m_users[row].login, this);
    dialog.exec();
}

void UserManagerDialog::onDeleteUserClicked()
{
    const int row = userTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, "Suppression", "Sélectionnez un utilisateur à supprimer.");
        return;
    }

    const QString login = userTable->item(row, 2)->text();

    if (login == m_currentLogin) {
        QMessageBox::warning(this, "Suppression", "Vous ne pouvez pas supprimer votre propre compte.");
        return;
    }

    if (QMessageBox::question(this, "Suppression", "Supprimer l'utilisateur \"" + login + "\" ?") != QMessageBox::Yes) {
        return;
    }

    QString errorMessage;
    if (!DatabaseManager::deleteUser(login, errorMessage)) {
        QMessageBox::warning(this, "Suppression", errorMessage);
        return;
    }

    refreshUserList();
}
