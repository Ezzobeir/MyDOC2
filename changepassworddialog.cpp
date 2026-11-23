#include "changepassworddialog.h"

#include "databasemanager.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

ChangePasswordDialog::ChangePasswordDialog(const QString &login, bool mandatory, QWidget *parent)
    : QDialog(parent), m_login(login)
{
    setWindowTitle("Changer le mot de passe");
    setupUi(mandatory);
}

void ChangePasswordDialog::setupUi(bool mandatory)
{
    currentPasswordEdit = new QLineEdit(this);
    currentPasswordEdit->setEchoMode(QLineEdit::Password);

    newPasswordEdit = new QLineEdit(this);
    newPasswordEdit->setEchoMode(QLineEdit::Password);

    confirmPasswordEdit = new QLineEdit(this);
    confirmPasswordEdit->setEchoMode(QLineEdit::Password);

    validateButton = new QPushButton("Valider", this);
    validateButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(validateButton, &QPushButton::clicked, this, &ChangePasswordDialog::onValidateClicked);

    cancelButton = new QPushButton("Annuler", this);
    cancelButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    QFormLayout *formLayout = new QFormLayout;
    formLayout->addRow("Mot de passe actuel :", currentPasswordEdit);
    formLayout->addRow("Nouveau mot de passe :", newPasswordEdit);
    formLayout->addRow("Confirmer le nouveau mot de passe :", confirmPasswordEdit);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(validateButton);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    if (mandatory) {
        QLabel *infoLabel = new QLabel("Vous devez changer votre mot de passe avant de continuer.", this);
        infoLabel->setWordWrap(true);
        mainLayout->addWidget(infoLabel);
    }
    mainLayout->addLayout(formLayout);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void ChangePasswordDialog::onValidateClicked()
{
    const QString currentPassword = currentPasswordEdit->text();
    const QString newPassword = newPasswordEdit->text();
    const QString confirmPassword = confirmPasswordEdit->text();

    if (!DatabaseManager::checkCredentials(m_login, currentPassword)) {
        QMessageBox::warning(this, "Changement de mot de passe", "Mot de passe actuel incorrect.");
        return;
    }

    if (newPassword.isEmpty()) {
        QMessageBox::warning(this, "Changement de mot de passe", "Le nouveau mot de passe ne peut pas être vide.");
        return;
    }

    if (newPassword != confirmPassword) {
        QMessageBox::warning(this, "Changement de mot de passe", "Les mots de passe ne correspondent pas.");
        return;
    }

    QString errorMessage;
    if (!DatabaseManager::changePassword(m_login, newPassword, errorMessage)) {
        QMessageBox::warning(this, "Changement de mot de passe", errorMessage);
        return;
    }

    QMessageBox::information(this, "Changement de mot de passe", "Mot de passe modifié avec succès.");
    accept();
}
