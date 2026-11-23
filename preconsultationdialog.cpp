#include "preconsultationdialog.h"

#include "numberparse.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

PreConsultationDialog::PreConsultationDialog(const PatientRecord &patient, const QString &doctorLogin,
                                             const QDate &date, const QString &userLogin, QWidget *parent)
    : QDialog(parent), m_patient(patient), m_doctorLogin(doctorLogin), m_date(date), m_userLogin(userLogin)
{
    setWindowTitle("Pré-consultation — " + patient.firstName + " " + patient.lastName);
    setupUi();

    const PreConsultationRecord existing =
        DatabaseManager::getPreConsultation(m_patient.id, m_doctorLogin, m_date.toString(Qt::ISODate));
    if (existing.done) {
        if (!existing.heightCm.isNull()) {
            heightEdit->setText(QString::number(existing.heightCm.toDouble(), 'g', 5));
        }
        if (!existing.weightKg.isNull()) {
            weightEdit->setText(QString::number(existing.weightKg.toDouble(), 'g', 5));
        }
        if (!existing.bpSystolic.isNull()) {
            systolicEdit->setText(QString::number(existing.bpSystolic.toInt()));
        }
        if (!existing.bpDiastolic.isNull()) {
            diastolicEdit->setText(QString::number(existing.bpDiastolic.toInt()));
        }
    }
}

void PreConsultationDialog::setupUi()
{
    QLabel *titleLabel = new QLabel(m_patient.firstName + " " + m_patient.lastName, this);
    titleLabel->setStyleSheet("font-weight: 600; font-size: 12pt;");

    heightEdit = new QLineEdit(this);
    heightEdit->setPlaceholderText("ex. 172");

    weightEdit = new QLineEdit(this);
    weightEdit->setPlaceholderText("ex. 68,5");

    systolicEdit = new QLineEdit(this);
    systolicEdit->setPlaceholderText("systolique");
    diastolicEdit = new QLineEdit(this);
    diastolicEdit->setPlaceholderText("diastolique");

    QHBoxLayout *pressureLayout = new QHBoxLayout;
    pressureLayout->addWidget(systolicEdit);
    pressureLayout->addWidget(new QLabel("/", this));
    pressureLayout->addWidget(diastolicEdit);

    saveButton = new QPushButton("Enregistrer", this);
    saveButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(saveButton, &QPushButton::clicked, this, &PreConsultationDialog::onSaveClicked);

    cancelButton = new QPushButton("Annuler", this);
    cancelButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    QFormLayout *formLayout = new QFormLayout;
    formLayout->addRow("Taille (cm) :", heightEdit);
    formLayout->addRow("Poids (kg) :", weightEdit);
    formLayout->addRow("Tension (mmHg) :", pressureLayout);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(saveButton);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(titleLabel);
    mainLayout->addLayout(formLayout);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void PreConsultationDialog::onSaveClicked()
{
    bool heightOk = true;
    bool weightOk = true;
    bool systolicOk = true;
    bool diastolicOk = true;

    const QVariant height = parseOptionalNumber(heightEdit->text(), false, heightOk);
    const QVariant weight = parseOptionalNumber(weightEdit->text(), false, weightOk);
    const QVariant systolic = parseOptionalNumber(systolicEdit->text(), true, systolicOk);
    const QVariant diastolic = parseOptionalNumber(diastolicEdit->text(), true, diastolicOk);

    if (!heightOk || !weightOk || !systolicOk || !diastolicOk) {
        QMessageBox::warning(this, "Pré-consultation", "Veuillez saisir des valeurs numériques valides.");
        return;
    }

    QString errorMessage;
    if (!DatabaseManager::savePreConsultation(m_patient.id, m_doctorLogin, m_date.toString(Qt::ISODate), weight,
                                              height, systolic, diastolic, m_userLogin, errorMessage)) {
        QMessageBox::warning(this, "Pré-consultation", errorMessage);
        return;
    }

    accept();
}
