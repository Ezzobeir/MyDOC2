#ifndef USERMANAGERDIALOG_H
#define USERMANAGERDIALOG_H

#include <QDialog>

#include "databasemanager.h"

class QTableWidget;
class QLineEdit;
class QComboBox;
class QPushButton;

class UserManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UserManagerDialog(const QString &currentLogin, QWidget *parent = nullptr);

private slots:
    void onAddUserClicked();
    void onApplyChangesClicked();
    void onDeleteUserClicked();
    void onManageAssignedDoctorsClicked();
    void refreshConnectionStatus();

private:
    void setupUi();
    void refreshUserList();

    QString m_currentLogin;
    QList<UserRecord> m_users;

    QTableWidget *userTable;
    QLineEdit *newFirstNameEdit;
    QLineEdit *newLastNameEdit;
    QLineEdit *newLoginEdit;
    QLineEdit *newEmailEdit;
    QComboBox *newRoleCombo;
    QPushButton *addButton;
    QPushButton *applyButton;
    QPushButton *deleteButton;
    QPushButton *assignedDoctorsButton;
};

#endif // USERMANAGERDIALOG_H
