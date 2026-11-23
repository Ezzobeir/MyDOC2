#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "usermanagerdialog.h"
#include "changepassworddialog.h"
#include "loginwindow.h"
#include "appointmentswidget.h"
#include "patientfilewidget.h"
#include "consultationwidget.h"
#include "waitingroomwidget.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QCloseEvent>
#include <QListWidget>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(const QString &currentLogin, UserRole currentRole, int sessionId, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_currentLogin(currentLogin)
    , m_currentRole(currentRole)
    , m_sessionId(sessionId)
{
    ui->setupUi(this);

    setupNavigation();

    const QString userInfo = "Connecté : " + m_currentLogin + " (" + DatabaseManager::roleDisplayName(m_currentRole) + ")";
    setWindowTitle("MyDoc - " + userInfo);
    statusBar()->addPermanentWidget(new QLabel(userInfo, this));

    QMenu *sessionMenu = menuBar()->addMenu("Session");
    QAction *changePasswordAction = sessionMenu->addAction("Changer le mot de passe");
    connect(changePasswordAction, &QAction::triggered, this, &MainWindow::onChangePasswordClicked);

    sessionMenu->addSeparator();

    QAction *logoutAction = sessionMenu->addAction("Déconnexion");
    connect(logoutAction, &QAction::triggered, this, &MainWindow::onLogoutClicked);

    if (m_currentRole == UserRole::MedecinAdmin || m_currentRole == UserRole::AssistantAdmin) {
        QMenu *adminMenu = menuBar()->addMenu("Administration");
        QAction *manageUsersAction = adminMenu->addAction("Gérer les utilisateurs");
        connect(manageUsersAction, &QAction::triggered, this, &MainWindow::onManageUsersClicked);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupNavigation()
{
    sidebarMenu = new QListWidget(this);
    sidebarMenu->addItem("Agenda");
    sidebarMenu->addItem("Fiche patients");
    sidebarMenu->addItem("Salle d'attente");

    const bool isDoctor = (m_currentRole == UserRole::Medecin || m_currentRole == UserRole::MedecinAdmin);
    if (isDoctor) {
        sidebarMenu->addItem("Consultations");
    }

    sidebarMenu->setFixedWidth(160);
    sidebarMenu->setStyleSheet(
        "QListWidget { background-color: #1F2937; color: #E5E7EB; border: none; outline: none; font-size: 10pt; }"
        "QListWidget::item { padding: 14px 16px; }"
        "QListWidget::item:selected { background-color: #3B82C4; color: #FFFFFF; }"
        "QListWidget::item:hover:!selected { background-color: #374151; }");

    contentStack = new QStackedWidget(this);
    PatientFileWidget *patientFileWidget = new PatientFileWidget(m_currentLogin, m_currentRole, this);
    WaitingRoomWidget *waitingRoomWidget = new WaitingRoomWidget(m_currentLogin, m_currentRole, this);

    contentStack->addWidget(new AppointmentsWidget(m_currentLogin, m_currentRole, this));
    contentStack->addWidget(patientFileWidget);
    contentStack->addWidget(waitingRoomWidget);

    connect(patientFileWidget, &PatientFileWidget::addedToWaitingRoom, this,
            [this, waitingRoomWidget](const QDate &date, const QString &doctorLogin) {
                waitingRoomWidget->showWaitingRoom(date, doctorLogin);
                sidebarMenu->setCurrentRow(contentStack->indexOf(waitingRoomWidget));
            });

    if (isDoctor) {
        contentStack->addWidget(new ConsultationWidget(m_currentLogin, this));
    }

    connect(sidebarMenu, &QListWidget::currentRowChanged, contentStack, &QStackedWidget::setCurrentIndex);
    sidebarMenu->setCurrentRow(0);

    QWidget *centralContainer = new QWidget(this);
    QHBoxLayout *centralLayout = new QHBoxLayout(centralContainer);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(sidebarMenu);
    centralLayout->addWidget(contentStack, 1);

    setCentralWidget(centralContainer);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    DatabaseManager::setConnected(m_currentLogin, false);
    DatabaseManager::closeSession(m_sessionId);
    QMainWindow::closeEvent(event);
}

void MainWindow::onManageUsersClicked()
{
    UserManagerDialog dialog(m_currentLogin, this);
    dialog.exec();
}

void MainWindow::onChangePasswordClicked()
{
    ChangePasswordDialog dialog(m_currentLogin, false, this);
    dialog.exec();
}

void MainWindow::onLogoutClicked()
{
    LoginWindow *loginWindow = new LoginWindow();
    loginWindow->setAttribute(Qt::WA_DeleteOnClose);
    loginWindow->setFixedSize(380, 260);
    loginWindow->show();

    close();
}

