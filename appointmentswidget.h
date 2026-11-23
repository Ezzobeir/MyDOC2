#ifndef APPOINTMENTSWIDGET_H
#define APPOINTMENTSWIDGET_H

#include <QWidget>
#include <QDate>
#include <QStringList>

#include "databasemanager.h"

class QCalendarWidget;
class QTableWidget;
class QLineEdit;
class QComboBox;
class QTimeEdit;
class QPushButton;
class QLabel;
class QStackedWidget;

class AppointmentsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AppointmentsWidget(const QString &currentLogin, UserRole currentRole, QWidget *parent = nullptr);

private slots:
    void onDateSelected(const QDate &date);
    void onAddAppointmentClicked();
    void onDeleteAppointmentClicked();
    void onNewPatientClicked();
    void onAppointmentTypeChanged(int index);

private:
    void setupUi();
    void refreshCalendarHighlights();
    void refreshAppointmentsForSelectedDate();
    void refreshPatientCombo(int selectPatientId = -1);
    QList<UserRecord> visibleDoctors() const;

    QString m_currentLogin;
    UserRole m_currentRole;
    QStringList m_visibleDoctorLogins;

    QDate m_selectedDate;
    QStringList m_highlightedDates;
    QList<AppointmentRecord> m_appointments;

    QCalendarWidget *calendar;
    QLabel *selectedDateLabel;
    QTableWidget *appointmentsTable;
    QPushButton *deleteButton;

    QComboBox *appointmentTypeCombo;
    QStackedWidget *appointmentDetailsStack;

    QComboBox *patientCombo;
    QPushButton *newPatientButton;
    QLineEdit *otherLabelEdit;

    QComboBox *doctorCombo;
    QTimeEdit *startTimeEdit;
    QComboBox *durationCombo;
    QLineEdit *notesEdit;
    QPushButton *addButton;
    QLabel *noDoctorLabel;
};

#endif // APPOINTMENTSWIDGET_H
