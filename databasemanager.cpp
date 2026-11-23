#include "databasemanager.h"

#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QPair>
#include <QRandomGenerator>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QTime>
#include <QVariant>

namespace {

QStringList tableColumns(const QString &table)
{
    QStringList columns;
    QSqlQuery info("PRAGMA table_info(" + table + ")");
    while (info.next()) {
        columns << info.value("name").toString();
    }
    return columns;
}

// Ajoute chaque colonne manquante ({nom, définition}) à une table existante.
bool addMissingColumns(const QString &table, const QList<QPair<QString, QString>> &columns, QString &errorMessage)
{
    const QStringList existing = tableColumns(table);
    QSqlQuery alter;
    for (const auto &column : columns) {
        if (existing.contains(column.first)) {
            continue;
        }
        if (!alter.exec("ALTER TABLE " + table + " ADD COLUMN " + column.first + " " + column.second)) {
            errorMessage = alter.lastError().text();
            return false;
        }
    }
    return true;
}

// Message d'erreur si une mesure est hors bornes, chaîne vide sinon.
QString measuresError(const QVariant &weightKg, const QVariant &heightCm, const QVariant &bpSystolic,
                      const QVariant &bpDiastolic, const QVariant &glycemiaGl = QVariant(),
                      const QVariant &temperatureC = QVariant())
{
    auto outOfRange = [](const QVariant &value, double minimum, double maximum) {
        return !value.isNull() && (value.toDouble() < minimum || value.toDouble() > maximum);
    };

    if (outOfRange(weightKg, 1, 400)) {
        return "Le poids doit être compris entre 1 et 400 kg.";
    }
    if (outOfRange(heightCm, 30, 250)) {
        return "La taille doit être comprise entre 30 et 250 cm.";
    }
    if (bpSystolic.isNull() != bpDiastolic.isNull()) {
        return "Renseignez la tension systolique et diastolique, ou aucune des deux.";
    }
    if (outOfRange(bpSystolic, 30, 300) || outOfRange(bpDiastolic, 30, 300)) {
        return "La tension doit être comprise entre 30 et 300.";
    }
    if (!bpSystolic.isNull() && bpSystolic.toInt() < bpDiastolic.toInt()) {
        return "La tension systolique doit être supérieure ou égale à la diastolique.";
    }
    if (outOfRange(glycemiaGl, 0.1, 10)) {
        return "La glycémie doit être comprise entre 0,1 et 10 g/L.";
    }
    if (outOfRange(temperatureC, 30, 45)) {
        return "La température doit être comprise entre 30 et 45 °C.";
    }
    return QString();
}

// 1 = mineur, 0 = majeur, -1 = indéterminé (date de naissance absente ou illisible).
int minorStatusAt(const QString &birthDateText, const QString &isoDate)
{
    QDate birthDate = QDate::fromString(birthDateText.trimmed(), "dd/MM/yyyy");
    if (!birthDate.isValid()) {
        birthDate = QDate::fromString(birthDateText.trimmed(), Qt::ISODate);
    }
    const QDate onDate = QDate::fromString(isoDate, Qt::ISODate);
    if (!birthDate.isValid() || !onDate.isValid()) {
        return -1;
    }
    return birthDate.addYears(18) > onDate ? 1 : 0;
}

}

QString DatabaseManager::hashPassword(const QString &password)
{
    return QString(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool DatabaseManager::init(QString &errorMessage)
{
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dataDir + "/cabinet.db");

    if (!db.open()) {
        errorMessage = db.lastError().text();
        return false;
    }

    QSqlQuery query;

    // Migration ponctuelle : ancienne table "users" -> T_USERS avec les nouveaux noms de colonnes.
    QSqlQuery legacyCheck("SELECT name FROM sqlite_master WHERE type='table' AND name='users'");
    if (legacyCheck.next()) {
        if (!query.exec("ALTER TABLE users RENAME TO T_USERS")) {
            errorMessage = query.lastError().text();
            return false;
        }
        query.exec("ALTER TABLE T_USERS RENAME COLUMN id TO USR_ID");
        query.exec("ALTER TABLE T_USERS RENAME COLUMN login TO USR_LOGIN");
        query.exec("ALTER TABLE T_USERS RENAME COLUMN password_hash TO USR_PASSWORD");
        query.exec("ALTER TABLE T_USERS RENAME COLUMN role TO USR_ROLE");
        query.exec("ALTER TABLE T_USERS RENAME COLUMN must_change_password TO USER_PASSWORDTOCHANGE");
        query.exec("ALTER TABLE T_USERS RENAME COLUMN first_name TO USR_FIRSTNAME");
        query.exec("ALTER TABLE T_USERS RENAME COLUMN last_name TO USR_LASTNAME");
        query.exec("ALTER TABLE T_USERS RENAME COLUMN first_login_at TO USR_FIRSTLOGIN_DATE");
        query.exec("ALTER TABLE T_USERS RENAME COLUMN last_login_at TO USR_LASTLOGIN_DATE");
        query.exec("ALTER TABLE T_USERS RENAME COLUMN is_connected TO USR_ISCONNECTED");
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS T_USERS ("
                     "USR_ID INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "USR_LOGIN TEXT UNIQUE NOT NULL, "
                     "USR_PASSWORD TEXT NOT NULL, "
                     "USR_ROLE TEXT NOT NULL DEFAULT 'medecin' "
                     "CHECK(USR_ROLE IN ('medecin', 'medecin_admin', 'assistant', 'assistant_admin')), "
                     "USER_PASSWORDTOCHANGE INTEGER NOT NULL DEFAULT 0, "
                     "USR_FIRSTNAME TEXT NOT NULL DEFAULT '', "
                     "USR_LASTNAME TEXT NOT NULL DEFAULT '', "
                     "USR_EMAIL TEXT NOT NULL DEFAULT '', "
                     "USR_FIRSTLOGIN_DATE TEXT, "
                     "USR_LASTLOGIN_DATE TEXT, "
                     "USR_ISCONNECTED INTEGER NOT NULL DEFAULT 0)")) {
        errorMessage = query.lastError().text();
        return false;
    }

    QSqlQuery emailColumnCheck("PRAGMA table_info(T_USERS)");
    bool hasEmailColumn = false;
    while (emailColumnCheck.next()) {
        if (emailColumnCheck.value("name").toString() == "USR_EMAIL") {
            hasEmailColumn = true;
            break;
        }
    }
    if (!hasEmailColumn) {
        if (!query.exec("ALTER TABLE T_USERS ADD COLUMN USR_EMAIL TEXT NOT NULL DEFAULT ''")) {
            errorMessage = query.lastError().text();
            return false;
        }
    }

    query.exec("SELECT COUNT(*) FROM T_USERS");
    if (query.next() && query.value(0).toInt() == 0) {
        QSqlQuery insert;
        insert.prepare("INSERT INTO T_USERS (USR_LOGIN, USR_PASSWORD, USR_ROLE) VALUES (:login, :hash, :role)");
        insert.bindValue(":login", "admin");
        insert.bindValue(":hash", hashPassword("admin"));
        insert.bindValue(":role", roleToString(UserRole::MedecinAdmin));
        if (!insert.exec()) {
            errorMessage = insert.lastError().text();
            return false;
        }
    }

    // Migration ponctuelle : ancienne table "appointments" -> T_RDV avec les nouveaux noms de colonnes.
    QSqlQuery legacyAppointmentsCheck("SELECT name FROM sqlite_master WHERE type='table' AND name='appointments'");
    if (legacyAppointmentsCheck.next()) {
        if (!query.exec("ALTER TABLE appointments RENAME TO T_RDV")) {
            errorMessage = query.lastError().text();
            return false;
        }
        query.exec("ALTER TABLE T_RDV RENAME COLUMN id TO RDV_ID");
        query.exec("ALTER TABLE T_RDV RENAME COLUMN patient_name TO RDV_PATIENTNAME");
        query.exec("ALTER TABLE T_RDV RENAME COLUMN doctor_login TO RDV_DOCTORLOGIN");
        query.exec("ALTER TABLE T_RDV RENAME COLUMN date TO RDV_DATE");
        query.exec("ALTER TABLE T_RDV RENAME COLUMN start_time TO RDV_STARTTIME");
        query.exec("ALTER TABLE T_RDV RENAME COLUMN duration_minutes TO RDV_DURATION");
        query.exec("ALTER TABLE T_RDV RENAME COLUMN notes TO RDV_NOTES");
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS T_RDV ("
                     "RDV_ID INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "RDV_TYPE TEXT NOT NULL DEFAULT 'patient', "
                     "RDV_PATIENTNAME TEXT NOT NULL, "
                     "RDV_PATIENT_ID INTEGER, "
                     "RDV_DOCTORLOGIN TEXT NOT NULL, "
                     "RDV_DATE TEXT NOT NULL, "
                     "RDV_STARTTIME TEXT NOT NULL, "
                     "RDV_DURATION INTEGER NOT NULL, "
                     "RDV_NOTES TEXT NOT NULL DEFAULT '')")) {
        errorMessage = query.lastError().text();
        return false;
    }

    const QStringList requiredRdvColumns = {"RDV_PATIENT_ID", "RDV_TYPE"};
    QStringList missingRdvColumns = requiredRdvColumns;
    QSqlQuery rdvColumnCheck("PRAGMA table_info(T_RDV)");
    while (rdvColumnCheck.next()) {
        missingRdvColumns.removeAll(rdvColumnCheck.value("name").toString());
    }
    if (missingRdvColumns.contains("RDV_PATIENT_ID")) {
        if (!query.exec("ALTER TABLE T_RDV ADD COLUMN RDV_PATIENT_ID INTEGER")) {
            errorMessage = query.lastError().text();
            return false;
        }
    }
    if (missingRdvColumns.contains("RDV_TYPE")) {
        if (!query.exec("ALTER TABLE T_RDV ADD COLUMN RDV_TYPE TEXT NOT NULL DEFAULT 'patient'")) {
            errorMessage = query.lastError().text();
            return false;
        }
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS T_SESSION ("
                     "SES_ID INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "SES_LOGIN TEXT NOT NULL, "
                     "SES_START_DATE TEXT NOT NULL, "
                     "SES_END_DATE TEXT)")) {
        errorMessage = query.lastError().text();
        return false;
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS T_ASSISTANT_MEDECIN ("
                     "AM_ID INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "AM_ASSISTANT_LOGIN TEXT NOT NULL, "
                     "AM_DOCTOR_LOGIN TEXT NOT NULL, "
                     "UNIQUE(AM_ASSISTANT_LOGIN, AM_DOCTOR_LOGIN))")) {
        errorMessage = query.lastError().text();
        return false;
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS T_PATIENT ("
                     "PAT_ID INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "PAT_FIRSTNAME TEXT NOT NULL, "
                     "PAT_LASTNAME TEXT NOT NULL, "
                     "PAT_BIRTHDATE TEXT, "
                     "PAT_PHONE TEXT, "
                     "PAT_SEX TEXT NOT NULL DEFAULT '' CHECK(PAT_SEX IN ('', 'M', 'F')), "
                     "PAT_PATH TEXT NOT NULL DEFAULT '')")) {
        errorMessage = query.lastError().text();
        return false;
    }

    if (!addMissingColumns("T_PATIENT",
                           {{"PAT_SEX", "TEXT NOT NULL DEFAULT '' CHECK(PAT_SEX IN ('', 'M', 'F'))"},
                            {"PAT_PATH", "TEXT NOT NULL DEFAULT ''"}},
                           errorMessage)) {
        return false;
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS T_CONSULTATION ("
                     "CSL_ID INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "CSL_APPOINTMENT_ID INTEGER, "
                     "CSL_PATIENT_ID INTEGER NOT NULL, "
                     "CSL_DOCTOR_LOGIN TEXT NOT NULL, "
                     "CSL_DOCTOR_ID INTEGER, "
                     "CSL_DATE TEXT NOT NULL, "
                     "CSL_TIME TEXT, "
                     "CSL_IS_MINOR INTEGER, "
                     "CSL_FIRST_CONSULT_DATE TEXT, "
                     "CSL_LAST_CONSULT_DATE TEXT, "
                     "CSL_IS_CONTROL INTEGER NOT NULL DEFAULT 0, "
                     "CSL_WEIGHT REAL, "
                     "CSL_HEIGHT REAL, "
                     "CSL_BP_SYSTOLIC INTEGER, "
                     "CSL_BP_DIASTOLIC INTEGER, "
                     "CSL_PREP_USER_ID INTEGER, "
                     "CSL_GLYCEMIA REAL, "
                     "CSL_TEMPERATURE REAL, "
                     "CSL_OTHER_MEASURES TEXT NOT NULL DEFAULT '', "
                     "CSL_MOTIF TEXT NOT NULL DEFAULT '', "
                     "CSL_SYMPTOMS TEXT NOT NULL DEFAULT '', "
                     "CSL_DIAGNOSTIC TEXT NOT NULL DEFAULT '', "
                     "CSL_TRAITEMENT TEXT NOT NULL DEFAULT '')")) {
        errorMessage = query.lastError().text();
        return false;
    }

    // Les consultations existantes doivent être complétées une seule fois, à l'ajout des colonnes.
    const bool consultationNeedsBackfill = !tableColumns("T_CONSULTATION").contains("CSL_FIRST_CONSULT_DATE");

    if (!addMissingColumns("T_CONSULTATION",
                           {{"CSL_DOCTOR_ID", "INTEGER"},
                            {"CSL_TIME", "TEXT"},
                            {"CSL_IS_MINOR", "INTEGER"},
                            {"CSL_FIRST_CONSULT_DATE", "TEXT"},
                            {"CSL_LAST_CONSULT_DATE", "TEXT"},
                            {"CSL_IS_CONTROL", "INTEGER NOT NULL DEFAULT 0"},
                            {"CSL_WEIGHT", "REAL"},
                            {"CSL_HEIGHT", "REAL"},
                            {"CSL_BP_SYSTOLIC", "INTEGER"},
                            {"CSL_BP_DIASTOLIC", "INTEGER"},
                            {"CSL_PREP_USER_ID", "INTEGER"},
                            {"CSL_GLYCEMIA", "REAL"},
                            {"CSL_TEMPERATURE", "REAL"},
                            {"CSL_OTHER_MEASURES", "TEXT NOT NULL DEFAULT ''"},
                            {"CSL_SYMPTOMS", "TEXT NOT NULL DEFAULT ''"}},
                           errorMessage)) {
        return false;
    }

    if (consultationNeedsBackfill) {
        const QStringList backfill = {
            "UPDATE T_CONSULTATION SET CSL_DOCTOR_ID = "
            "(SELECT USR_ID FROM T_USERS WHERE USR_LOGIN = T_CONSULTATION.CSL_DOCTOR_LOGIN)",
            "UPDATE T_CONSULTATION SET CSL_FIRST_CONSULT_DATE = "
            "(SELECT MIN(c.CSL_DATE) FROM T_CONSULTATION c WHERE c.CSL_PATIENT_ID = T_CONSULTATION.CSL_PATIENT_ID)",
            "UPDATE T_CONSULTATION SET CSL_LAST_CONSULT_DATE = "
            "(SELECT MAX(c.CSL_DATE) FROM T_CONSULTATION c WHERE c.CSL_PATIENT_ID = T_CONSULTATION.CSL_PATIENT_ID "
            "AND (c.CSL_DATE < T_CONSULTATION.CSL_DATE "
            "OR (c.CSL_DATE = T_CONSULTATION.CSL_DATE AND c.CSL_ID < T_CONSULTATION.CSL_ID)))"};
        for (const QString &statement : backfill) {
            if (!query.exec(statement)) {
                errorMessage = query.lastError().text();
                return false;
            }
        }

        QSqlQuery minorQuery("SELECT c.CSL_ID, c.CSL_DATE, p.PAT_BIRTHDATE FROM T_CONSULTATION c "
                             "LEFT JOIN T_PATIENT p ON p.PAT_ID = c.CSL_PATIENT_ID");
        QSqlQuery minorUpdate;
        minorUpdate.prepare("UPDATE T_CONSULTATION SET CSL_IS_MINOR = :isMinor WHERE CSL_ID = :id");
        while (minorQuery.next()) {
            const int status = minorStatusAt(minorQuery.value(2).toString(), minorQuery.value(1).toString());
            minorUpdate.bindValue(":isMinor", status >= 0 ? QVariant(status) : QVariant());
            minorUpdate.bindValue(":id", minorQuery.value(0));
            minorUpdate.exec();
        }
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS T_SALLEATTENTE ("
                     "SAT_ID INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "SAT_PATIENT_ID INTEGER NOT NULL, "
                     "SAT_DOCTOR_LOGIN TEXT NOT NULL, "
                     "SAT_DATE TEXT NOT NULL, "
                     "SAT_ADDED_AT TEXT NOT NULL, "
                     "SAT_WEIGHT REAL, "
                     "SAT_HEIGHT REAL, "
                     "SAT_BP_SYSTOLIC INTEGER, "
                     "SAT_BP_DIASTOLIC INTEGER, "
                     "SAT_PREP_USER_ID INTEGER, "
                     "SAT_PREP_AT TEXT)")) {
        errorMessage = query.lastError().text();
        return false;
    }

    if (!addMissingColumns("T_SALLEATTENTE",
                           {{"SAT_WEIGHT", "REAL"},
                            {"SAT_HEIGHT", "REAL"},
                            {"SAT_BP_SYSTOLIC", "INTEGER"},
                            {"SAT_BP_DIASTOLIC", "INTEGER"},
                            {"SAT_PREP_USER_ID", "INTEGER"},
                            {"SAT_PREP_AT", "TEXT"}},
                           errorMessage)) {
        return false;
    }

    return true;
}

bool DatabaseManager::checkCredentials(const QString &login, const QString &password)
{
    QSqlQuery query;
    query.prepare("SELECT USR_PASSWORD FROM T_USERS WHERE USR_LOGIN = :login");
    query.bindValue(":login", login);

    if (!query.exec() || !query.next()) {
        return false;
    }

    return query.value(0).toString() == hashPassword(password);
}

UserRole DatabaseManager::getUserRole(const QString &login)
{
    QSqlQuery query;
    query.prepare("SELECT USR_ROLE FROM T_USERS WHERE USR_LOGIN = :login");
    query.bindValue(":login", login);

    if (query.exec() && query.next()) {
        return roleFromString(query.value(0).toString());
    }
    return UserRole::Medecin;
}

QString DatabaseManager::roleToString(UserRole role)
{
    switch (role) {
    case UserRole::Medecin: return "medecin";
    case UserRole::MedecinAdmin: return "medecin_admin";
    case UserRole::Assistant: return "assistant";
    case UserRole::AssistantAdmin: return "assistant_admin";
    }
    return QString();
}

UserRole DatabaseManager::roleFromString(const QString &roleStr)
{
    if (roleStr == "medecin_admin") return UserRole::MedecinAdmin;
    if (roleStr == "assistant") return UserRole::Assistant;
    if (roleStr == "assistant_admin") return UserRole::AssistantAdmin;
    return UserRole::Medecin;
}

QString DatabaseManager::roleDisplayName(UserRole role)
{
    switch (role) {
    case UserRole::Medecin: return "Médecin";
    case UserRole::MedecinAdmin: return "Médecin admin";
    case UserRole::Assistant: return "Assistant";
    case UserRole::AssistantAdmin: return "Assistant admin";
    }
    return QString();
}

QList<UserRecord> DatabaseManager::getAllUsers()
{
    QList<UserRecord> users;

    QSqlQuery query("SELECT USR_ID, USR_LOGIN, USR_FIRSTNAME, USR_LASTNAME, USR_EMAIL, USR_ROLE, "
                     "USR_FIRSTLOGIN_DATE, USR_LASTLOGIN_DATE, USR_ISCONNECTED FROM T_USERS ORDER BY USR_LOGIN");
    while (query.next()) {
        UserRecord record;
        record.id = query.value(0).toInt();
        record.login = query.value(1).toString();
        record.firstName = query.value(2).toString();
        record.lastName = query.value(3).toString();
        record.email = query.value(4).toString();
        record.role = roleFromString(query.value(5).toString());
        record.firstLoginAt = query.value(6).toString();
        record.lastLoginAt = query.value(7).toString();
        record.isConnected = query.value(8).toInt() != 0;
        users.append(record);
    }

    return users;
}

bool DatabaseManager::createUser(const QString &login, const QString &password, const QString &firstName,
                                  const QString &lastName, const QString &email, UserRole role,
                                  QString &errorMessage)
{
    if (login.trimmed().isEmpty() || password.isEmpty()) {
        errorMessage = "Le login et le mot de passe sont obligatoires.";
        return false;
    }

    if (firstName.trimmed().isEmpty() || lastName.trimmed().isEmpty()) {
        errorMessage = "Le prénom et le nom sont obligatoires.";
        return false;
    }

    if (email.trimmed().isEmpty()) {
        errorMessage = "L'adresse email est obligatoire (elle sert à envoyer le mot de passe).";
        return false;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO T_USERS (USR_LOGIN, USR_PASSWORD, USR_FIRSTNAME, USR_LASTNAME, USR_EMAIL, "
                   "USR_ROLE, USER_PASSWORDTOCHANGE) VALUES (:login, :hash, :firstName, :lastName, :email, :role, 1)");
    query.bindValue(":login", login.trimmed());
    query.bindValue(":hash", hashPassword(password));
    query.bindValue(":firstName", firstName.trimmed());
    query.bindValue(":lastName", lastName.trimmed());
    query.bindValue(":email", email.trimmed());
    query.bindValue(":role", roleToString(role));

    if (!query.exec()) {
        errorMessage = "Ce login existe déjà.";
        return false;
    }

    return true;
}

bool DatabaseManager::updateUser(const QString &originalLogin, const QString &newLogin, const QString &firstName,
                                  const QString &lastName, const QString &email, UserRole role,
                                  QString &errorMessage)
{
    if (newLogin.trimmed().isEmpty()) {
        errorMessage = "Le login est obligatoire.";
        return false;
    }

    if (firstName.trimmed().isEmpty() || lastName.trimmed().isEmpty()) {
        errorMessage = "Le prénom et le nom sont obligatoires.";
        return false;
    }

    if (email.trimmed().isEmpty()) {
        errorMessage = "L'adresse email est obligatoire (elle sert à envoyer le mot de passe).";
        return false;
    }

    QSqlQuery query;
    query.prepare("UPDATE T_USERS SET USR_LOGIN = :newLogin, USR_FIRSTNAME = :firstName, "
                   "USR_LASTNAME = :lastName, USR_EMAIL = :email, USR_ROLE = :role WHERE USR_LOGIN = :originalLogin");
    query.bindValue(":newLogin", newLogin.trimmed());
    query.bindValue(":firstName", firstName.trimmed());
    query.bindValue(":lastName", lastName.trimmed());
    query.bindValue(":email", email.trimmed());
    query.bindValue(":role", roleToString(role));
    query.bindValue(":originalLogin", originalLogin);

    if (!query.exec()) {
        errorMessage = "Ce login existe déjà.";
        return false;
    }

    return true;
}

void DatabaseManager::recordLogin(const QString &login)
{
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    QSqlQuery query;
    query.prepare("UPDATE T_USERS SET "
                   "USR_FIRSTLOGIN_DATE = COALESCE(USR_FIRSTLOGIN_DATE, :now), "
                   "USR_LASTLOGIN_DATE = :now, "
                   "USR_ISCONNECTED = 1 "
                   "WHERE USR_LOGIN = :login");
    query.bindValue(":now", now);
    query.bindValue(":login", login);
    query.exec();
}

void DatabaseManager::setConnected(const QString &login, bool connected)
{
    QSqlQuery query;
    query.prepare("UPDATE T_USERS SET USR_ISCONNECTED = :connected WHERE USR_LOGIN = :login");
    query.bindValue(":connected", connected ? 1 : 0);
    query.bindValue(":login", login);
    query.exec();
}

int DatabaseManager::openSession(const QString &login)
{
    QSqlQuery query;
    query.prepare("INSERT INTO T_SESSION (SES_LOGIN, SES_START_DATE) VALUES (:login, :now)");
    query.bindValue(":login", login);
    query.bindValue(":now", QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!query.exec()) {
        return -1;
    }

    return query.lastInsertId().toInt();
}

void DatabaseManager::closeSession(int sessionId)
{
    QSqlQuery query;
    query.prepare("UPDATE T_SESSION SET SES_END_DATE = :now WHERE SES_ID = :id");
    query.bindValue(":now", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":id", sessionId);
    query.exec();
}

bool DatabaseManager::mustChangePassword(const QString &login)
{
    QSqlQuery query;
    query.prepare("SELECT USER_PASSWORDTOCHANGE FROM T_USERS WHERE USR_LOGIN = :login");
    query.bindValue(":login", login);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() != 0;
    }
    return false;
}

bool DatabaseManager::changePassword(const QString &login, const QString &newPassword, QString &errorMessage)
{
    if (newPassword.isEmpty()) {
        errorMessage = "Le mot de passe ne peut pas être vide.";
        return false;
    }

    QSqlQuery query;
    query.prepare("UPDATE T_USERS SET USR_PASSWORD = :hash, USER_PASSWORDTOCHANGE = 0 WHERE USR_LOGIN = :login");
    query.bindValue(":hash", hashPassword(newPassword));
    query.bindValue(":login", login);

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }

    return true;
}

QString DatabaseManager::generateRandomPassword(int length)
{
    const QString chars = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789";
    QString password;
    for (int i = 0; i < length; ++i) {
        password.append(chars.at(QRandomGenerator::global()->bounded(chars.size())));
    }
    return password;
}

QString DatabaseManager::getEmail(const QString &login)
{
    QSqlQuery query;
    query.prepare("SELECT USR_EMAIL FROM T_USERS WHERE USR_LOGIN = :login");
    query.bindValue(":login", login);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

bool DatabaseManager::resetPassword(const QString &login, QString &newPassword, QString &errorMessage)
{
    newPassword = generateRandomPassword();

    QSqlQuery query;
    query.prepare("UPDATE T_USERS SET USR_PASSWORD = :hash, USER_PASSWORDTOCHANGE = 1 WHERE USR_LOGIN = :login");
    query.bindValue(":hash", hashPassword(newPassword));
    query.bindValue(":login", login);

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() == 0) {
        errorMessage = "Utilisateur introuvable.";
        return false;
    }

    return true;
}

bool DatabaseManager::deleteUser(const QString &login, QString &errorMessage)
{
    QSqlQuery query;
    query.prepare("DELETE FROM T_USERS WHERE USR_LOGIN = :login");
    query.bindValue(":login", login);

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() == 0) {
        errorMessage = "Utilisateur introuvable.";
        return false;
    }

    QSqlQuery cleanup;
    cleanup.prepare("DELETE FROM T_ASSISTANT_MEDECIN WHERE AM_ASSISTANT_LOGIN = :login OR AM_DOCTOR_LOGIN = :login");
    cleanup.bindValue(":login", login);
    cleanup.exec();

    return true;
}

QList<UserRecord> DatabaseManager::getAssignedDoctors(const QString &assistantLogin)
{
    QList<UserRecord> doctors;

    QSqlQuery query;
    query.prepare("SELECT AM_DOCTOR_LOGIN FROM T_ASSISTANT_MEDECIN WHERE AM_ASSISTANT_LOGIN = :login");
    query.bindValue(":login", assistantLogin);
    query.exec();

    QStringList doctorLogins;
    while (query.next()) {
        doctorLogins.append(query.value(0).toString());
    }

    const QList<UserRecord> allDoctors = getDoctors();
    for (const UserRecord &doctor : allDoctors) {
        if (doctorLogins.contains(doctor.login)) {
            doctors.append(doctor);
        }
    }

    return doctors;
}

bool DatabaseManager::assignDoctor(const QString &assistantLogin, const QString &doctorLogin, QString &errorMessage)
{
    QSqlQuery query;
    query.prepare("INSERT INTO T_ASSISTANT_MEDECIN (AM_ASSISTANT_LOGIN, AM_DOCTOR_LOGIN) "
                   "VALUES (:assistant, :doctor)");
    query.bindValue(":assistant", assistantLogin);
    query.bindValue(":doctor", doctorLogin);

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::unassignDoctor(const QString &assistantLogin, const QString &doctorLogin, QString &errorMessage)
{
    QSqlQuery query;
    query.prepare("DELETE FROM T_ASSISTANT_MEDECIN WHERE AM_ASSISTANT_LOGIN = :assistant AND AM_DOCTOR_LOGIN = :doctor");
    query.bindValue(":assistant", assistantLogin);
    query.bindValue(":doctor", doctorLogin);

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }

    return true;
}

QList<UserRecord> DatabaseManager::getDoctors()
{
    QList<UserRecord> doctors;

    const QList<UserRecord> allUsers = getAllUsers();
    for (const UserRecord &user : allUsers) {
        if (user.role == UserRole::Medecin || user.role == UserRole::MedecinAdmin) {
            doctors.append(user);
        }
    }

    return doctors;
}

QList<AppointmentRecord> DatabaseManager::getAppointmentsForDate(const QString &date, const QStringList &doctorLogins)
{
    QList<AppointmentRecord> appointments;

    if (doctorLogins.isEmpty()) {
        return appointments;
    }

    QStringList placeholders;
    for (int i = 0; i < doctorLogins.size(); ++i) {
        placeholders << QString(":doctor%1").arg(i);
    }

    QSqlQuery query;
    query.prepare("SELECT RDV_ID, RDV_TYPE, RDV_PATIENTNAME, RDV_PATIENT_ID, RDV_DOCTORLOGIN, RDV_DATE, "
                   "RDV_STARTTIME, RDV_DURATION, RDV_NOTES FROM T_RDV WHERE RDV_DATE = :date "
                   "AND RDV_DOCTORLOGIN IN (" + placeholders.join(",") + ") ORDER BY RDV_STARTTIME");
    query.bindValue(":date", date);
    for (int i = 0; i < doctorLogins.size(); ++i) {
        query.bindValue(placeholders[i], doctorLogins[i]);
    }
    query.exec();

    while (query.next()) {
        AppointmentRecord record;
        record.id = query.value(0).toInt();
        record.type = query.value(1).toString();
        record.patientName = query.value(2).toString();
        record.patientId = query.value(3).isNull() ? -1 : query.value(3).toInt();
        record.doctorLogin = query.value(4).toString();
        record.date = query.value(5).toString();
        record.startTime = query.value(6).toString();
        record.durationMinutes = query.value(7).toInt();
        record.notes = query.value(8).toString();
        appointments.append(record);
    }

    return appointments;
}

QStringList DatabaseManager::getAppointmentDates(const QStringList &doctorLogins)
{
    QStringList dates;

    if (doctorLogins.isEmpty()) {
        return dates;
    }

    QStringList placeholders;
    for (int i = 0; i < doctorLogins.size(); ++i) {
        placeholders << QString(":doctor%1").arg(i);
    }

    QSqlQuery query;
    query.prepare("SELECT DISTINCT RDV_DATE FROM T_RDV WHERE RDV_DOCTORLOGIN IN (" + placeholders.join(",") + ")");
    for (int i = 0; i < doctorLogins.size(); ++i) {
        query.bindValue(placeholders[i], doctorLogins[i]);
    }
    query.exec();

    while (query.next()) {
        dates.append(query.value(0).toString());
    }

    return dates;
}

bool DatabaseManager::createAppointment(int patientId, const QString &patientName, const QString &doctorLogin,
                                         const QString &date, const QString &startTime, int durationMinutes,
                                         const QString &notes, QString &errorMessage)
{
    if (patientId <= 0) {
        errorMessage = "Veuillez sélectionner un patient.";
        return false;
    }

    if (doctorLogin.isEmpty()) {
        errorMessage = "Veuillez sélectionner un médecin.";
        return false;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO T_RDV (RDV_TYPE, RDV_PATIENTNAME, RDV_PATIENT_ID, RDV_DOCTORLOGIN, RDV_DATE, "
                   "RDV_STARTTIME, RDV_DURATION, RDV_NOTES) "
                   "VALUES ('patient', :patientName, :patientId, :doctorLogin, :date, :startTime, :duration, :notes)");
    query.bindValue(":patientName", patientName.trimmed());
    query.bindValue(":patientId", patientId);
    query.bindValue(":doctorLogin", doctorLogin);
    query.bindValue(":date", date);
    query.bindValue(":startTime", startTime);
    query.bindValue(":duration", durationMinutes);
    query.bindValue(":notes", notes.trimmed());

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::createOtherAppointment(const QString &label, const QString &doctorLogin, const QString &date,
                                              const QString &startTime, int durationMinutes, const QString &notes,
                                              QString &errorMessage)
{
    if (label.trimmed().isEmpty()) {
        errorMessage = "Veuillez indiquer un intitulé pour ce rendez-vous.";
        return false;
    }

    if (doctorLogin.isEmpty()) {
        errorMessage = "Veuillez sélectionner un médecin.";
        return false;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO T_RDV (RDV_TYPE, RDV_PATIENTNAME, RDV_DOCTORLOGIN, RDV_DATE, RDV_STARTTIME, "
                   "RDV_DURATION, RDV_NOTES) "
                   "VALUES ('autre', :label, :doctorLogin, :date, :startTime, :duration, :notes)");
    query.bindValue(":label", label.trimmed());
    query.bindValue(":doctorLogin", doctorLogin);
    query.bindValue(":date", date);
    query.bindValue(":startTime", startTime);
    query.bindValue(":duration", durationMinutes);
    query.bindValue(":notes", notes.trimmed());

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }

    return true;
}

QList<PatientRecord> DatabaseManager::getAllPatients()
{
    QList<PatientRecord> patients;

    QSqlQuery query("SELECT PAT_ID, PAT_FIRSTNAME, PAT_LASTNAME, PAT_BIRTHDATE, PAT_PHONE, PAT_SEX, PAT_PATH "
                     "FROM T_PATIENT ORDER BY PAT_LASTNAME, PAT_FIRSTNAME");
    while (query.next()) {
        PatientRecord record;
        record.id = query.value(0).toInt();
        record.firstName = query.value(1).toString();
        record.lastName = query.value(2).toString();
        record.birthDate = query.value(3).toString();
        record.phone = query.value(4).toString();
        record.sex = query.value(5).toString();
        record.path = query.value(6).toString();
        patients.append(record);
    }

    return patients;
}

bool DatabaseManager::createPatient(const QString &firstName, const QString &lastName, const QString &birthDate,
                                     const QString &phone, int &newPatientId, QString &errorMessage,
                                     const QString &sex, const QString &path)
{
    if (firstName.trimmed().isEmpty() || lastName.trimmed().isEmpty()) {
        errorMessage = "Le prénom et le nom sont obligatoires.";
        return false;
    }

    if (!sex.isEmpty() && sex != "M" && sex != "F") {
        errorMessage = "Le sexe doit être M (masculin) ou F (féminin).";
        return false;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO T_PATIENT (PAT_FIRSTNAME, PAT_LASTNAME, PAT_BIRTHDATE, PAT_PHONE, PAT_SEX, PAT_PATH) "
                   "VALUES (:firstName, :lastName, :birthDate, :phone, :sex, :path)");
    query.bindValue(":firstName", firstName.trimmed());
    query.bindValue(":lastName", lastName.trimmed());
    query.bindValue(":birthDate", birthDate.trimmed());
    query.bindValue(":phone", phone.trimmed());
    query.bindValue(":sex", sex);
    query.bindValue(":path", path.trimmed());

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }

    newPatientId = query.lastInsertId().toInt();
    return true;
}

bool DatabaseManager::updatePatient(int patientId, const QString &firstName, const QString &lastName,
                                     const QString &birthDate, const QString &phone, QString &errorMessage)
{
    if (firstName.trimmed().isEmpty() || lastName.trimmed().isEmpty()) {
        errorMessage = "Le prénom et le nom sont obligatoires.";
        return false;
    }

    QSqlQuery query;
    query.prepare("UPDATE T_PATIENT SET PAT_FIRSTNAME = :firstName, PAT_LASTNAME = :lastName, "
                   "PAT_BIRTHDATE = :birthDate, PAT_PHONE = :phone WHERE PAT_ID = :id");
    query.bindValue(":firstName", firstName.trimmed());
    query.bindValue(":lastName", lastName.trimmed());
    query.bindValue(":birthDate", birthDate.trimmed());
    query.bindValue(":phone", phone.trimmed());
    query.bindValue(":id", patientId);

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::updatePatientDetails(int patientId, const QString &sex, const QString &path,
                                            QString &errorMessage)
{
    if (!sex.isEmpty() && sex != "M" && sex != "F") {
        errorMessage = "Le sexe doit être M (masculin) ou F (féminin).";
        return false;
    }

    QSqlQuery query;
    query.prepare("UPDATE T_PATIENT SET PAT_SEX = :sex, PAT_PATH = :path WHERE PAT_ID = :id");
    query.bindValue(":sex", sex);
    query.bindValue(":path", path.trimmed());
    query.bindValue(":id", patientId);

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }

    return true;
}

QList<ConsultationRecord> DatabaseManager::getConsultationsForPatient(int patientId)
{
    QList<ConsultationRecord> consultations;

    QSqlQuery query;
    query.prepare("SELECT CSL_ID, CSL_APPOINTMENT_ID, CSL_PATIENT_ID, CSL_DOCTOR_LOGIN, CSL_DOCTOR_ID, CSL_DATE, "
                   "CSL_TIME, CSL_IS_MINOR, CSL_FIRST_CONSULT_DATE, CSL_LAST_CONSULT_DATE, CSL_IS_CONTROL, "
                   "CSL_WEIGHT, CSL_HEIGHT, CSL_BP_SYSTOLIC, CSL_BP_DIASTOLIC, CSL_PREP_USER_ID, "
                   "CSL_MOTIF, CSL_DIAGNOSTIC, CSL_TRAITEMENT, CSL_GLYCEMIA, CSL_TEMPERATURE, "
                   "CSL_OTHER_MEASURES, CSL_SYMPTOMS FROM T_CONSULTATION WHERE CSL_PATIENT_ID = :patientId "
                   "ORDER BY CSL_DATE DESC, CSL_ID DESC");
    query.bindValue(":patientId", patientId);
    query.exec();

    while (query.next()) {
        ConsultationRecord record;
        record.id = query.value(0).toInt();
        record.appointmentId = query.value(1).toInt();
        record.patientId = query.value(2).toInt();
        record.doctorLogin = query.value(3).toString();
        record.doctorId = query.value(4).toInt();
        record.date = query.value(5).toString();
        record.time = query.value(6).toString();
        record.isMinor = query.value(7).isNull() ? -1 : query.value(7).toInt();
        record.firstConsultDate = query.value(8).toString();
        record.lastConsultDate = query.value(9).toString();
        record.isControl = query.value(10).toInt() != 0;
        record.weightKg = query.value(11);
        record.heightCm = query.value(12);
        record.bpSystolic = query.value(13);
        record.bpDiastolic = query.value(14);
        record.prepUserId = query.value(15).toInt();
        record.motif = query.value(16).toString();
        record.diagnostic = query.value(17).toString();
        record.treatment = query.value(18).toString();
        record.glycemiaGl = query.value(19);
        record.temperatureC = query.value(20);
        record.otherMeasures = query.value(21).toString();
        record.symptoms = query.value(22).toString();
        consultations.append(record);
    }

    return consultations;
}

bool DatabaseManager::createConsultation(int appointmentId, int patientId, const QString &doctorLogin,
                                          const QString &date, const QString &motif, const QString &diagnostic,
                                          const QString &treatment, QString &errorMessage,
                                          const QString &time, bool isControl, const QVariant &weightKg,
                                          const QVariant &heightCm, const QVariant &bpSystolic,
                                          const QVariant &bpDiastolic, int prepUserId, const QString &symptoms,
                                          const QVariant &glycemiaGl, const QVariant &temperatureC,
                                          const QString &otherMeasures)
{
    if (patientId <= 0) {
        errorMessage = "Veuillez sélectionner un patient.";
        return false;
    }

    if (!time.isEmpty() && !QTime::fromString(time, "HH:mm").isValid()) {
        errorMessage = "L'heure de consultation doit être au format HH:mm.";
        return false;
    }

    errorMessage = measuresError(weightKg, heightCm, bpSystolic, bpDiastolic, glycemiaGl, temperatureC);
    if (!errorMessage.isEmpty()) {
        return false;
    }

    // Valeurs déduites : première consultation, consultation précédente, mineur/majeur, id du médecin.
    QString firstConsultDate = date;
    QString lastConsultDate;
    QSqlQuery history;
    history.prepare("SELECT MIN(CSL_DATE), MAX(CASE WHEN CSL_DATE <= :date THEN CSL_DATE END) "
                     "FROM T_CONSULTATION WHERE CSL_PATIENT_ID = :patientId");
    history.bindValue(":date", date);
    history.bindValue(":patientId", patientId);
    if (history.exec() && history.next()) {
        const QString earliest = history.value(0).toString();
        if (!earliest.isEmpty() && earliest < date) {
            firstConsultDate = earliest;
        }
        lastConsultDate = history.value(1).toString();
    }

    int minorStatus = -1;
    QSqlQuery patientQuery;
    patientQuery.prepare("SELECT PAT_BIRTHDATE FROM T_PATIENT WHERE PAT_ID = :id");
    patientQuery.bindValue(":id", patientId);
    if (patientQuery.exec() && patientQuery.next()) {
        minorStatus = minorStatusAt(patientQuery.value(0).toString(), date);
    }

    QVariant doctorId;
    QSqlQuery doctorQuery;
    doctorQuery.prepare("SELECT USR_ID FROM T_USERS WHERE USR_LOGIN = :login");
    doctorQuery.bindValue(":login", doctorLogin);
    if (doctorQuery.exec() && doctorQuery.next()) {
        doctorId = doctorQuery.value(0);
    }

    QSqlQuery query;
    query.prepare("INSERT INTO T_CONSULTATION (CSL_APPOINTMENT_ID, CSL_PATIENT_ID, CSL_DOCTOR_LOGIN, CSL_DOCTOR_ID, "
                   "CSL_DATE, CSL_TIME, CSL_IS_MINOR, CSL_FIRST_CONSULT_DATE, CSL_LAST_CONSULT_DATE, CSL_IS_CONTROL, "
                   "CSL_WEIGHT, CSL_HEIGHT, CSL_BP_SYSTOLIC, CSL_BP_DIASTOLIC, CSL_PREP_USER_ID, "
                   "CSL_GLYCEMIA, CSL_TEMPERATURE, CSL_OTHER_MEASURES, "
                   "CSL_MOTIF, CSL_SYMPTOMS, CSL_DIAGNOSTIC, CSL_TRAITEMENT) "
                   "VALUES (:appointmentId, :patientId, :doctorLogin, :doctorId, :date, :time, :isMinor, "
                   ":firstDate, :lastDate, :isControl, :weight, :height, :bpSystolic, :bpDiastolic, :prepUserId, "
                   ":glycemia, :temperature, COALESCE(:otherMeasures, ''), "
                   "COALESCE(:motif, ''), COALESCE(:symptoms, ''), COALESCE(:diagnostic, ''), "
                   "COALESCE(:treatment, ''))");
    query.bindValue(":appointmentId", appointmentId > 0 ? QVariant(appointmentId) : QVariant());
    query.bindValue(":patientId", patientId);
    query.bindValue(":doctorLogin", doctorLogin);
    query.bindValue(":doctorId", doctorId);
    query.bindValue(":date", date);
    query.bindValue(":time", time.isEmpty() ? QVariant() : QVariant(time));
    query.bindValue(":isMinor", minorStatus >= 0 ? QVariant(minorStatus) : QVariant());
    query.bindValue(":firstDate", firstConsultDate);
    query.bindValue(":lastDate", lastConsultDate.isEmpty() ? QVariant() : QVariant(lastConsultDate));
    query.bindValue(":isControl", isControl ? 1 : 0);
    query.bindValue(":weight", weightKg);
    query.bindValue(":height", heightCm);
    query.bindValue(":bpSystolic", bpSystolic);
    query.bindValue(":bpDiastolic", bpDiastolic);
    query.bindValue(":prepUserId", prepUserId > 0 ? QVariant(prepUserId) : QVariant());
    query.bindValue(":glycemia", glycemiaGl);
    query.bindValue(":temperature", temperatureC);
    query.bindValue(":otherMeasures", otherMeasures.trimmed());
    query.bindValue(":motif", motif.trimmed());
    query.bindValue(":symptoms", symptoms.trimmed());
    query.bindValue(":diagnostic", diagnostic.trimmed());
    query.bindValue(":treatment", treatment.trimmed());

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }

    return true;
}

QList<PatientRecord> DatabaseManager::getWaitingRoomPatients(const QString &date, const QStringList &doctorLogins)
{
    QList<PatientRecord> result;

    if (doctorLogins.isEmpty()) {
        return result;
    }

    QStringList placeholders;
    for (int i = 0; i < doctorLogins.size(); ++i) {
        placeholders << QString(":doctor%1").arg(i);
    }
    const QString inClause = "(" + placeholders.join(",") + ")";

    // La salle d'attente ne contient que les patients ajoutés explicitement, par ordre d'arrivée.
    QSqlQuery query;
    query.prepare("SELECT p.PAT_ID, p.PAT_FIRSTNAME, p.PAT_LASTNAME, p.PAT_BIRTHDATE, p.PAT_PHONE, p.PAT_SEX, "
                   "p.PAT_PATH FROM T_SALLEATTENTE s JOIN T_PATIENT p ON p.PAT_ID = s.SAT_PATIENT_ID "
                   "WHERE s.SAT_DATE = :date AND s.SAT_DOCTOR_LOGIN IN " + inClause + " "
                   "GROUP BY p.PAT_ID ORDER BY MIN(s.SAT_ADDED_AT), p.PAT_ID");
    query.bindValue(":date", date);
    for (int i = 0; i < doctorLogins.size(); ++i) {
        query.bindValue(placeholders[i], doctorLogins[i]);
    }
    query.exec();

    while (query.next()) {
        PatientRecord record;
        record.id = query.value(0).toInt();
        record.firstName = query.value(1).toString();
        record.lastName = query.value(2).toString();
        record.birthDate = query.value(3).toString();
        record.phone = query.value(4).toString();
        record.sex = query.value(5).toString();
        record.path = query.value(6).toString();
        result.append(record);
    }

    return result;
}

bool DatabaseManager::addToWaitingRoom(int patientId, const QString &doctorLogin, const QString &date,
                                        QString &errorMessage)
{
    if (patientId <= 0) {
        errorMessage = "Veuillez sélectionner un patient.";
        return false;
    }

    QSqlQuery existing;
    existing.prepare("SELECT 1 FROM T_SALLEATTENTE WHERE SAT_PATIENT_ID = :patientId "
                      "AND SAT_DOCTOR_LOGIN = :doctorLogin AND SAT_DATE = :date");
    existing.bindValue(":patientId", patientId);
    existing.bindValue(":doctorLogin", doctorLogin);
    existing.bindValue(":date", date);
    if (existing.exec() && existing.next()) {
        errorMessage = "Ce patient est déjà dans la salle d'attente de ce médecin pour cette date.";
        return false;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO T_SALLEATTENTE (SAT_PATIENT_ID, SAT_DOCTOR_LOGIN, SAT_DATE, SAT_ADDED_AT) "
                   "VALUES (:patientId, :doctorLogin, :date, :addedAt)");
    query.bindValue(":patientId", patientId);
    query.bindValue(":doctorLogin", doctorLogin);
    query.bindValue(":date", date);
    query.bindValue(":addedAt", QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::removeFromWaitingRoom(int patientId, const QString &doctorLogin, const QString &date,
                                             QString &errorMessage)
{
    QSqlQuery query;
    query.prepare("DELETE FROM T_SALLEATTENTE WHERE SAT_PATIENT_ID = :patientId "
                   "AND SAT_DOCTOR_LOGIN = :doctorLogin AND SAT_DATE = :date");
    query.bindValue(":patientId", patientId);
    query.bindValue(":doctorLogin", doctorLogin);
    query.bindValue(":date", date);

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }

    return true;
}

QString PreConsultationRecord::summary() const
{
    QStringList parts;
    if (!heightCm.isNull()) {
        parts << "Taille : " + QString::number(heightCm.toDouble(), 'f', 0) + " cm";
    }
    if (!weightKg.isNull()) {
        parts << "Poids : " + QString::number(weightKg.toDouble(), 'g', 4) + " kg";
    }
    if (!bpSystolic.isNull() && !bpDiastolic.isNull()) {
        parts << "Tension : " + QString::number(bpSystolic.toInt()) + "/" + QString::number(bpDiastolic.toInt());
    }
    return parts.join(" — ");
}

PreConsultationRecord DatabaseManager::getPreConsultation(int patientId, const QString &doctorLogin,
                                                          const QString &date)
{
    PreConsultationRecord record;

    QSqlQuery query;
    query.prepare("SELECT SAT_WEIGHT, SAT_HEIGHT, SAT_BP_SYSTOLIC, SAT_BP_DIASTOLIC, SAT_PREP_USER_ID, SAT_PREP_AT "
                   "FROM T_SALLEATTENTE WHERE SAT_PATIENT_ID = :patientId AND SAT_DOCTOR_LOGIN = :doctorLogin "
                   "AND SAT_DATE = :date ORDER BY SAT_PREP_AT IS NULL, SAT_ID LIMIT 1");
    query.bindValue(":patientId", patientId);
    query.bindValue(":doctorLogin", doctorLogin);
    query.bindValue(":date", date);

    if (query.exec() && query.next()) {
        record.weightKg = query.value(0);
        record.heightCm = query.value(1);
        record.bpSystolic = query.value(2);
        record.bpDiastolic = query.value(3);
        record.prepUserId = query.value(4).toInt();
        record.prepAt = query.value(5).toString();
        record.done = !record.prepAt.isEmpty();
    }

    return record;
}

bool DatabaseManager::savePreConsultation(int patientId, const QString &doctorLogin, const QString &date,
                                           const QVariant &weightKg, const QVariant &heightCm,
                                           const QVariant &bpSystolic, const QVariant &bpDiastolic,
                                           const QString &prepUserLogin, QString &errorMessage)
{
    if (weightKg.isNull() && heightCm.isNull() && bpSystolic.isNull() && bpDiastolic.isNull()) {
        errorMessage = "Renseignez au moins une mesure (taille, poids ou tension).";
        return false;
    }
    errorMessage = measuresError(weightKg, heightCm, bpSystolic, bpDiastolic);
    if (!errorMessage.isEmpty()) {
        return false;
    }

    QVariant prepUserId;
    QSqlQuery userQuery;
    userQuery.prepare("SELECT USR_ID FROM T_USERS WHERE USR_LOGIN = :login");
    userQuery.bindValue(":login", prepUserLogin);
    if (userQuery.exec() && userQuery.next()) {
        prepUserId = userQuery.value(0);
    }

    QSqlQuery query;
    query.prepare("UPDATE T_SALLEATTENTE SET SAT_WEIGHT = :weight, SAT_HEIGHT = :height, "
                   "SAT_BP_SYSTOLIC = :bpSystolic, SAT_BP_DIASTOLIC = :bpDiastolic, "
                   "SAT_PREP_USER_ID = :prepUserId, SAT_PREP_AT = :prepAt "
                   "WHERE SAT_PATIENT_ID = :patientId AND SAT_DOCTOR_LOGIN = :doctorLogin AND SAT_DATE = :date");
    query.bindValue(":weight", weightKg);
    query.bindValue(":height", heightCm);
    query.bindValue(":bpSystolic", bpSystolic);
    query.bindValue(":bpDiastolic", bpDiastolic);
    query.bindValue(":prepUserId", prepUserId);
    query.bindValue(":prepAt", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":patientId", patientId);
    query.bindValue(":doctorLogin", doctorLogin);
    query.bindValue(":date", date);

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }
    if (query.numRowsAffected() <= 0) {
        errorMessage = "Ce patient n'est plus dans la salle d'attente.";
        return false;
    }

    return true;
}

bool DatabaseManager::deleteAppointment(int id, QString &errorMessage)
{
    QSqlQuery query;
    query.prepare("DELETE FROM T_RDV WHERE RDV_ID = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        errorMessage = query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() == 0) {
        errorMessage = "Rendez-vous introuvable.";
        return false;
    }

    return true;
}
