#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QApplication>
#include <QWidget>


#include <QPushButton>
#include <QLineEdit>
#include <QLabel>

class LoginWindow : public QWidget
{
    Q_OBJECT

public:
    LoginWindow(QWidget *parent = nullptr);

    /*bool checkLogin();
    bool checkPwd();

    bool isBlocked();
    int checkNbrTentatives();
    void incrNbrTentatives();
    void reinitTentatives();
    bool bloquerLogin();

    bool ouvrirSession();
    bool fermerSession();*/

private slots:
    void onValidClicked();
    void onMotDePasseOublieClicked();
    //void onCloseClicked();


private:
    void setupUi();
    //void setupStateMachine();

    QLineEdit *loginEdit;
    QLineEdit *pwdEdit;
    QPushButton *validButton;
    //QPushButton *motDePasseOublieButton;
    QPushButton *closeButton;

    QLabel *lienMotDePasseOublie;



};

#endif // LOGINWINDOW_H
