#ifndef CONSULTATIONWIDGET_H
#define CONSULTATIONWIDGET_H

#include <QWidget>

#include "databasemanager.h"

class QListWidget;
class QTableWidget;
class QLabel;
class QPushButton;

class ConsultationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ConsultationWidget(const QString &currentLogin, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onPatientSelected();
    void onStartConsultationClicked();

private:
    void setupUi();
    void refreshWaitingRoom();
    void refreshSelectedPatientInfo();

    QString m_currentLogin;

    QList<PatientRecord> m_waitingRoomPatients;

    QListWidget *waitingRoomListWidget;

    QLabel *patientInfoLabel;
    QTableWidget *consultationsTable;
    QPushButton *startConsultationButton;
};

#endif // CONSULTATIONWIDGET_H
