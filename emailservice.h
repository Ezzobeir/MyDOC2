#ifndef EMAILSERVICE_H
#define EMAILSERVICE_H

#include <QString>

class EmailService
{
public:
    static bool sendEmail(const QString &to, const QString &subject, const QString &body, QString &errorMessage);
};

#endif // EMAILSERVICE_H
