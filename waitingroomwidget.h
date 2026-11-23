#ifndef WAITINGROOMWIDGET_H
#define WAITINGROOMWIDGET_H

#include <QDate>
#include <QStringList>
#include <QWidget>

#include "databasemanager.h"

class QComboBox;
class QDateEdit;
class QLabel;
class QListWidget;
class QPushButton;

class WaitingRoomWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WaitingRoomWidget(const QString &currentLogin, UserRole currentRole, QWidget *parent = nullptr);

    void showWaitingRoom(const QDate &date, const QString &doctorLogin);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onFilterChanged();
    void onSelectionChanged();
    void onAddClicked();
    void onRemoveClicked();
    void onPreConsultationClicked();

private:
    void setupUi();
    void refreshPatientCombo();
    void refreshWaitingRoom();
    QList<UserRecord> visibleDoctors() const;
    QString selectedDoctorLogin() const;

    QString m_currentLogin;
    UserRole m_currentRole;
    QStringList m_visibleDoctorLogins;

    QList<PatientRecord> m_waitingPatients;

    QDateEdit *dateEdit;
    QLabel *doctorLabel;
    QComboBox *doctorCombo;
    QLabel *noDoctorLabel;
    QListWidget *waitingListWidget;
    QPushButton *preConsultationButton;
    QPushButton *removeButton;
    QComboBox *patientCombo;
    QPushButton *addButton;
};

#endif // WAITINGROOMWIDGET_H
