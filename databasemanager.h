#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QVariant>

enum class UserRole {
    Medecin,
    MedecinAdmin,
    Assistant,
    AssistantAdmin
};

struct UserRecord {
    int id;
    QString login;
    QString firstName;
    QString lastName;
    QString email;
    UserRole role;
    QString firstLoginAt;
    QString lastLoginAt;
    bool isConnected;
};

struct AppointmentRecord {
    int id;
    QString type;
    int patientId;
    QString patientName;
    QString doctorLogin;
    QString date;
    QString startTime;
    int durationMinutes;
    QString notes;
};

struct PatientRecord {
    int id;
    QString firstName;
    QString lastName;
    QString birthDate;
    QString phone;
    QString sex;   // "M", "F" ou vide
    QString path;  // dossier du patient (ordonnances, examens...)
};

struct ConsultationRecord {
    int id;
    int appointmentId;
    int patientId;
    QString doctorLogin;
    int doctorId;           // 0 si inconnu
    QString date;
    QString time;           // "HH:mm", vide si non renseignée
    int isMinor;            // 1 mineur, 0 majeur, -1 inconnu
    QString firstConsultDate;
    QString lastConsultDate;  // consultation précédente, vide si première
    bool isControl;
    QVariant weightKg;      // null si non mesuré
    QVariant heightCm;
    QVariant bpSystolic;
    QVariant bpDiastolic;
    int prepUserId;         // 0 si aucune prise de mesures
    QVariant glycemiaGl;    // g/L, null si non mesurée
    QVariant temperatureC;  // °C, null si non mesurée
    QString otherMeasures;
    QString motif;
    QString symptoms;
    QString diagnostic;
    QString treatment;
};

// Mesures prises avant la consultation, rattachées à la ligne de salle d'attente.
struct PreConsultationRecord {
    bool done = false;
    QVariant weightKg;
    QVariant heightCm;
    QVariant bpSystolic;
    QVariant bpDiastolic;
    int prepUserId = 0;
    QString prepAt;

    QString summary() const;
};

class DatabaseManager
{
public:
    static bool init(QString &errorMessage);
    static bool checkCredentials(const QString &login, const QString &password);
    static UserRole getUserRole(const QString &login);

    static QString roleToString(UserRole role);
    static UserRole roleFromString(const QString &roleStr);
    static QString roleDisplayName(UserRole role);

    static QList<UserRecord> getAllUsers();
    static bool createUser(const QString &login, const QString &password, const QString &firstName,
                            const QString &lastName, const QString &email, UserRole role, QString &errorMessage);
    static bool updateUser(const QString &originalLogin, const QString &newLogin, const QString &firstName,
                            const QString &lastName, const QString &email, UserRole role, QString &errorMessage);
    static bool deleteUser(const QString &login, QString &errorMessage);

    static bool mustChangePassword(const QString &login);
    static bool changePassword(const QString &login, const QString &newPassword, QString &errorMessage);

    static QString generateRandomPassword(int length = 10);
    static QString getEmail(const QString &login);
    static bool resetPassword(const QString &login, QString &newPassword, QString &errorMessage);

    static void recordLogin(const QString &login);
    static void setConnected(const QString &login, bool connected);

    static int openSession(const QString &login);
    static void closeSession(int sessionId);

    static QList<UserRecord> getDoctors();

    static QList<UserRecord> getAssignedDoctors(const QString &assistantLogin);
    static bool assignDoctor(const QString &assistantLogin, const QString &doctorLogin, QString &errorMessage);
    static bool unassignDoctor(const QString &assistantLogin, const QString &doctorLogin, QString &errorMessage);

    static QList<AppointmentRecord> getAppointmentsForDate(const QString &date, const QStringList &doctorLogins);
    static QStringList getAppointmentDates(const QStringList &doctorLogins);
    static bool createAppointment(int patientId, const QString &patientName, const QString &doctorLogin,
                                   const QString &date, const QString &startTime, int durationMinutes,
                                   const QString &notes, QString &errorMessage);
    static bool createOtherAppointment(const QString &label, const QString &doctorLogin, const QString &date,
                                        const QString &startTime, int durationMinutes, const QString &notes,
                                        QString &errorMessage);
    static bool deleteAppointment(int id, QString &errorMessage);

    static QList<PatientRecord> getAllPatients();
    static bool createPatient(const QString &firstName, const QString &lastName, const QString &birthDate,
                               const QString &phone, int &newPatientId, QString &errorMessage,
                               const QString &sex = QString(), const QString &path = QString());
    static bool updatePatient(int patientId, const QString &firstName, const QString &lastName,
                               const QString &birthDate, const QString &phone, QString &errorMessage);
    static bool updatePatientDetails(int patientId, const QString &sex, const QString &path,
                                      QString &errorMessage);

    static QList<ConsultationRecord> getConsultationsForPatient(int patientId);
    // Mineur/majeur, première et dernière consultation, id du médecin sont calculés à l'enregistrement.
    static bool createConsultation(int appointmentId, int patientId, const QString &doctorLogin,
                                    const QString &date, const QString &motif, const QString &diagnostic,
                                    const QString &treatment, QString &errorMessage,
                                    const QString &time = QString(), bool isControl = false,
                                    const QVariant &weightKg = QVariant(), const QVariant &heightCm = QVariant(),
                                    const QVariant &bpSystolic = QVariant(), const QVariant &bpDiastolic = QVariant(),
                                    int prepUserId = 0, const QString &symptoms = QString(),
                                    const QVariant &glycemiaGl = QVariant(),
                                    const QVariant &temperatureC = QVariant(),
                                    const QString &otherMeasures = QString());

    static QList<PatientRecord> getWaitingRoomPatients(const QString &date, const QStringList &doctorLogins);
    static bool addToWaitingRoom(int patientId, const QString &doctorLogin, const QString &date,
                                  QString &errorMessage);
    static bool removeFromWaitingRoom(int patientId, const QString &doctorLogin, const QString &date,
                                       QString &errorMessage);

    static PreConsultationRecord getPreConsultation(int patientId, const QString &doctorLogin, const QString &date);
    static bool savePreConsultation(int patientId, const QString &doctorLogin, const QString &date,
                                     const QVariant &weightKg, const QVariant &heightCm,
                                     const QVariant &bpSystolic, const QVariant &bpDiastolic,
                                     const QString &prepUserLogin, QString &errorMessage);

private:
    static QString hashPassword(const QString &password);
};

#endif // DATABASEMANAGER_H
