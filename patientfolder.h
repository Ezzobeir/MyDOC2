#ifndef PATIENTFOLDER_H
#define PATIENTFOLDER_H

#include <QString>

#include "databasemanager.h"

// Dossier de chaque patient (ordonnances, examens...) : C:/Patients/NOM_Prenom_jjmmaaaa
namespace PatientFolder {

// Chemin à utiliser pour un nouveau patient ; l'id est ajouté si le dossier existe déjà (homonyme).
QString pathFor(int patientId, const QString &firstName, const QString &lastName, const QString &birthDate);

// Dossier du patient, créé et mémorisé en base s'il n'existe pas encore.
// Retourne une chaîne vide et renseigne errorMessage en cas d'échec.
QString ensure(const PatientRecord &patient, QString &errorMessage);

}

#endif // PATIENTFOLDER_H
