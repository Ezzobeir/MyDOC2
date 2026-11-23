#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "databasemanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(const QString &currentLogin, UserRole currentRole, int sessionId, QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onManageUsersClicked();
    void onChangePasswordClicked();
    void onLogoutClicked();

private:
    void setupNavigation();

    Ui::MainWindow *ui;
    QString m_currentLogin;
    UserRole m_currentRole;
    int m_sessionId;

    class QListWidget *sidebarMenu;
    class QStackedWidget *contentStack;
};
#endif // MAINWINDOW_H
