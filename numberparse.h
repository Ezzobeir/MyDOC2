#ifndef NUMBERPARSE_H
#define NUMBERPARSE_H

#include <QString>
#include <QVariant>

// Texte saisi -> valeur numérique. Un champ vide donne une valeur nulle ; un texte invalide met ok à false.
// La virgule décimale est acceptée.
inline QVariant parseOptionalNumber(const QString &text, bool integer, bool &ok)
{
    ok = true;

    QString cleaned = text.trimmed();
    if (cleaned.isEmpty()) {
        return QVariant();
    }
    cleaned.replace(',', '.');

    if (integer) {
        const int value = cleaned.toInt(&ok);
        return ok ? QVariant(value) : QVariant();
    }

    const double value = cleaned.toDouble(&ok);
    return ok ? QVariant(value) : QVariant();
}

#endif // NUMBERPARSE_H
