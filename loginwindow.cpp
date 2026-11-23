#include "loginwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QDebug>
#include <QApplication>
#include <QMessageBox>

#include "databasemanager.h"
#include "mainwindow.h"
#include "changepassworddialog.h"
#include "emailservice.h"

LoginWindow::LoginWindow(QWidget *parent) : QWidget(parent) {
   setupUi();

}

void LoginWindow::setupUi() {
    QLabel *titleLabel = new QLabel("MyDoc — Cabinet médical", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 14pt; font-weight: 600; color: #285A8A; margin-bottom: 8px;");

    loginEdit = new QLineEdit(this);
    pwdEdit = new QLineEdit(this);
    pwdEdit->setEchoMode(QLineEdit::Password);


    validButton = new QPushButton("Valider", this);
    validButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    //motDePasseOublieButton = new QPushButton("Mot de passe oublié ?", this);
    closeButton = new QPushButton("Fermer", this);
    closeButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    lienMotDePasseOublie = new QLabel(this);

    lienMotDePasseOublie->setText("<a href='#'>Mot de passe oublié ?</a>");
    lienMotDePasseOublie->setTextFormat(Qt::RichText);
    lienMotDePasseOublie->setTextInteractionFlags(Qt::TextBrowserInteraction);
    lienMotDePasseOublie->setOpenExternalLinks(false); // important: on veut gérer le clic nous-mêmes

    connect(closeButton, SIGNAL(clicked()), qApp, SLOT(quit()));

    connect(validButton, SIGNAL(clicked()), this, SLOT(onValidClicked()));

    connect(lienMotDePasseOublie, &QLabel::linkActivated, this, [this]() { onMotDePasseOublieClicked(); });

    //QLabel *linkLabel = new QLabel(this);



    QFormLayout *formLayout = new QFormLayout;
    formLayout->addRow("Login :", loginEdit);
    formLayout->addRow("Mot de passe :", pwdEdit);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(titleLabel);
    layout->addLayout(formLayout);

    layout->addWidget(lienMotDePasseOublie);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(validButton);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);

    setLayout(layout);

    //QWidget::connect(fermerButton, SIGNAL(clicked()), qApp, SLOT(quit()));
    //QWidget::connect(fermerButton, SIGNAL(clicked()), qApp, SLOT( this->onFermerClicked() ) );
    //this->setupStateMachine();
}

void LoginWindow::onValidClicked()
{
    const QString login = loginEdit->text();
    const QString password = pwdEdit->text();

    if (login.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "Connexion", "Veuillez renseigner le login et le mot de passe.");
        return;
    }

    if (!DatabaseManager::checkCredentials(login, password)) {
        QMessageBox::warning(this, "Connexion", "Login ou mot de passe incorrect.");
        return;
    }

    DatabaseManager::recordLogin(login);

    if (DatabaseManager::mustChangePassword(login)) {
        ChangePasswordDialog changePasswordDialog(login, true, this);
        if (changePasswordDialog.exec() != QDialog::Accepted) {
            QMessageBox::warning(this, "Connexion", "Vous devez changer votre mot de passe pour continuer.");
            return;
        }
    }

    const int sessionId = DatabaseManager::openSession(login);

    MainWindow *mainWindow = new MainWindow(login, DatabaseManager::getUserRole(login), sessionId);
    mainWindow->setAttribute(Qt::WA_DeleteOnClose);
    mainWindow->show();

    close();
}
void LoginWindow::onMotDePasseOublieClicked()
{
    const QString login = loginEdit->text().trimmed();
    if (login.isEmpty()) {
        QMessageBox::information(this, "Mot de passe oublié",
                                  "Veuillez saisir votre login dans le champ ci-dessus, puis cliquer à nouveau sur ce lien.");
        return;
    }

    const QString email = DatabaseManager::getEmail(login);
    if (email.trimmed().isEmpty()) {
        QMessageBox::warning(this, "Mot de passe oublié",
                              "Aucune adresse email n'est associée à ce compte. Contactez un administrateur.");
        return;
    }

    QString newPassword;
    QString errorMessage;
    if (!DatabaseManager::resetPassword(login, newPassword, errorMessage)) {
        QMessageBox::warning(this, "Mot de passe oublié", errorMessage);
        return;
    }

    const QString subject = "MyDoc - Réinitialisation de votre mot de passe";
    const QString body = "Bonjour,\n\nVotre mot de passe a été réinitialisé.\n\n"
                          "Nouveau mot de passe temporaire : " + newPassword +
                          "\n\nVous devrez le changer dès votre prochaine connexion.\n\nMyDoc - Cabinet médical";

    QString emailError;
    if (!EmailService::sendEmail(email, subject, body, emailError)) {
        QMessageBox::warning(this, "Mot de passe oublié",
                              "Le mot de passe a été réinitialisé mais l'email n'a pas pu être envoyé :\n" +
                              emailError + "\n\nMot de passe temporaire (à communiquer manuellement) : " + newPassword);
        return;
    }

    QMessageBox::information(this, "Mot de passe oublié",
                              "Un email contenant votre nouveau mot de passe a été envoyé à " + email + ".");
}
