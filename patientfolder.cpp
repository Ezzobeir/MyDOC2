#include "patientfolder.h"

#include <QDate>
#include <QDir>
#include <QRegularExpression>

namespace {

const char *kPatientsRootDir = "C:/Patients";

QString folderSafe(const QString &text)
{
    QString result = text.trimmed();
    result.remove(QRegularExpression("[\\\\/:*?\"<>|]"));
    result.replace(QRegularExpression("\\s+"), "-");
    while (result.endsWith('.')) {
        result.chop(1);
    }
    return result;
}

}

namespace PatientFolder {

QString pathFor(int patientId, const QString &firstName, const QString &lastName, const QString &birthDate)
{
    QString birthPart;
    const QDate date = QDate::fromString(birthDate.trimmed(), "d/M/yyyy");
    if (date.isValid()) {
        birthPart = date.toString("ddMMyyyy");
    } else {
        birthPart = birthDate;
        birthPart.remove(QRegularExpression("\\D"));
    }
    if (birthPart.isEmpty()) {
        birthPart = "SansDate";
    }

    QString name = folderSafe(lastName) + "_" + folderSafe(firstName) + "_" + birthPart;
    const QDir root(kPatientsRootDir);
    if (QDir(root.filePath(name)).exists()) {
        name += "_" + QString::number(patientId);
    }
    return root.filePath(name);
}

QString ensure(const PatientRecord &patient, QString &errorMessage)
{
    const bool isNew = patient.path.isEmpty();
    const QString path =
        isNew ? pathFor(patient.id, patient.firstName, patient.lastName, patient.birthDate) : patient.path;

    if (!QDir().mkpath(path)) {
        errorMessage = "Impossible de créer le dossier du patient :\n" + path;
        return QString();
    }

    if (isNew && !DatabaseManager::updatePatientDetails(patient.id, patient.sex, path, errorMessage)) {
        errorMessage = "Dossier créé (" + path + ") mais son chemin n'a pas pu être mémorisé :\n" + errorMessage;
        return QString();
    }

    return path;
}

}
