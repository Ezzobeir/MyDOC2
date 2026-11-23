#ifndef ASSISTANTDOCTORSDIALOG_H
#define ASSISTANTDOCTORSDIALOG_H

#include <QDialog>
#include <QString>

class QListWidget;
class QPushButton;

class AssistantDoctorsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AssistantDoctorsDialog(const QString &assistantLogin, QWidget *parent = nullptr);

private slots:
    void onSaveClicked();

private:
    void setupUi();

    QString m_assistantLogin;

    QListWidget *doctorListWidget;
    QPushButton *saveButton;
    QPushButton *cancelButton;
};

#endif // ASSISTANTDOCTORSDIALOG_H
