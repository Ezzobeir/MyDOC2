#include "loginwindow.h"
#include "databasemanager.h"
#include <QApplication>
#include <QMessageBox>

static const char *kAppStyleSheet = R"(
    QWidget {
        background-color: #F4F7FA;
        color: #1F2937;
        font-size: 10pt;
    }

    QLabel {
        background: transparent;
    }

    QLineEdit, QComboBox {
        background-color: #FFFFFF;
        border: 1px solid #D0D7E2;
        border-radius: 6px;
        padding: 6px 8px;
        selection-background-color: #3B82C4;
    }

    QLineEdit:focus, QComboBox:focus {
        border: 1px solid #3B82C4;
    }

    QComboBox::drop-down {
        border: none;
        width: 24px;
    }

    QPushButton {
        background-color: #3B82C4;
        color: #FFFFFF;
        border: none;
        border-radius: 6px;
        padding: 7px 16px;
        font-weight: 600;
    }

    QPushButton:hover {
        background-color: #316EA8;
    }

    QPushButton:pressed {
        background-color: #285A8A;
    }

    QPushButton:disabled {
        background-color: #B7C4D3;
        color: #F0F2F5;
    }

    QMenuBar {
        background-color: #FFFFFF;
        border-bottom: 1px solid #D0D7E2;
    }

    QMenuBar::item {
        padding: 6px 12px;
        background: transparent;
    }

    QMenuBar::item:selected {
        background-color: #E4EDF7;
        border-radius: 4px;
    }

    QMenu {
        background-color: #FFFFFF;
        border: 1px solid #D0D7E2;
    }

    QMenu::item {
        padding: 6px 24px;
    }

    QMenu::item:selected {
        background-color: #E4EDF7;
    }

    QStatusBar {
        background-color: #FFFFFF;
        border-top: 1px solid #D0D7E2;
    }

    QTableWidget {
        background-color: #FFFFFF;
        alternate-background-color: #F4F7FA;
        gridline-color: #E2E8F0;
        border: 1px solid #D0D7E2;
        border-radius: 6px;
        selection-background-color: #3B82C4;
        selection-color: #FFFFFF;
    }

    QHeaderView::section {
        background-color: #EEF2F7;
        color: #1F2937;
        padding: 6px;
        border: none;
        border-bottom: 1px solid #D0D7E2;
        font-weight: 600;
    }
)";

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setStyleSheet(kAppStyleSheet);

    QString errorMessage;
    if (!DatabaseManager::init(errorMessage)) {
        QMessageBox::critical(nullptr, "Erreur", "Impossible d'initialiser la base de données:\n" + errorMessage);
        return 1;
    }

    LoginWindow w;
    w.setFixedSize(380, 260);
    w.show();

    return a.exec();
}
