#ifndef CHANGEPASSWORDDIALOG_H
#define CHANGEPASSWORDDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;
class QPushButton;

class ChangePasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChangePasswordDialog(const QString &login, bool mandatory = false, QWidget *parent = nullptr);

private slots:
    void onValidateClicked();

private:
    void setupUi(bool mandatory);

    QString m_login;

    QLineEdit *currentPasswordEdit;
    QLineEdit *newPasswordEdit;
    QLineEdit *confirmPasswordEdit;
    QPushButton *validateButton;
    QPushButton *cancelButton;
};

#endif // CHANGEPASSWORDDIALOG_H
