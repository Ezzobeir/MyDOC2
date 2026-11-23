#ifndef PATIENTFILEWIDGET_H
#define PATIENTFILEWIDGET_H

#include <QDate>
#include <QWidget>
#include <QStringList>

#include "databasemanager.h"

class QListWidget;
class QLineEdit;
class QComboBox;
class QDateEdit;
class QPushButton;
class QLabel;

class PatientFileWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PatientFileWidget(const QString &currentLogin, UserRole currentRole, QWidget *parent = nullptr);

signals:
    void addedToWaitingRoom(const QDate &date, const QString &doctorLogin);

private slots:
    void onSearchTextChanged(const QString &text);
    void onPatientSelected();
    void onNewPatientClicked();
    void onEditPatientClicked();
    void onAddToWaitingRoomClicked();

private:
    void setupUi();
    void refreshPatientList();
    void refreshSelectedPatientInfo();
    QList<UserRecord> visibleDoctors() const;

    QString m_currentLogin;
    UserRole m_currentRole;
    QStringList m_visibleDoctorLogins;

    QList<PatientRecord> m_allPatients;
    QList<PatientRecord> m_filteredPatients;

    QLineEdit *searchEdit;
    QListWidget *patientListWidget;
    QPushButton *newPatientButton;

    QLabel *patientInfoLabel;
    QPushButton *editPatientButton;
    QDateEdit *dayEdit;
    QComboBox *doctorCombo;
    QPushButton *addToWaitingRoomButton;
};

#endif // PATIENTFILEWIDGET_H
