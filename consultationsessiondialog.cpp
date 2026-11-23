#include "consultationsessiondialog.h"

#include "ollamaclient.h"
#include "numberparse.h"
#include "prescriptiondialog.h"

#include <QDate>
#include <QDateTime>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextEdit>
#include <QVBoxLayout>

ConsultationSessionDialog::ConsultationSessionDialog(const PatientRecord &patient, const QString &doctorLogin,
                                                       QWidget *parent)
    : QDialog(parent), m_patient(patient), m_doctorLogin(doctorLogin)
{
    setWindowTitle("Consultation — " + patient.firstName + " " + patient.lastName);
    setupUi();
}

void ConsultationSessionDialog::setupUi()
{
    QLabel *titleLabel = new QLabel(m_patient.firstName + " " + m_patient.lastName, this);
    titleLabel->setStyleSheet("font-weight: 600; font-size: 12pt;");

    // Mesures : préremplies par la pré-consultation, modifiables par le médecin.
    heightEdit = new QLineEdit(this);
    weightEdit = new QLineEdit(this);
    systolicEdit = new QLineEdit(this);
    systolicEdit->setPlaceholderText("systolique");
    diastolicEdit = new QLineEdit(this);
    diastolicEdit->setPlaceholderText("diastolique");
    glycemiaEdit = new QLineEdit(this);
    glycemiaEdit->setPlaceholderText("ex. 1,05");
    temperatureEdit = new QLineEdit(this);
    temperatureEdit->setPlaceholderText("ex. 37,2");
    otherMeasuresEdit = new QLineEdit(this);

    QLabel *preConsultationLabel = new QLabel(this);
    const PreConsultationRecord preConsultation = DatabaseManager::getPreConsultation(
        m_patient.id, m_doctorLogin, QDate::currentDate().toString(Qt::ISODate));
    if (preConsultation.done) {
        const QDateTime preparedAt = QDateTime::fromString(preConsultation.prepAt, Qt::ISODate);
        preConsultationLabel->setText("Pré-consultation réalisée à " + preparedAt.toString("HH:mm") +
                                       " — valeurs modifiables.");
        m_prepUserId = preConsultation.prepUserId;

        if (!preConsultation.heightCm.isNull()) {
            heightEdit->setText(QString::number(preConsultation.heightCm.toDouble(), 'g', 5));
        }
        if (!preConsultation.weightKg.isNull()) {
            weightEdit->setText(QString::number(preConsultation.weightKg.toDouble(), 'g', 5));
        }
        if (!preConsultation.bpSystolic.isNull()) {
            systolicEdit->setText(QString::number(preConsultation.bpSystolic.toInt()));
        }
        if (!preConsultation.bpDiastolic.isNull()) {
            diastolicEdit->setText(QString::number(preConsultation.bpDiastolic.toInt()));
        }
    } else {
        preConsultationLabel->setText("Pré-consultation non réalisée — les mesures peuvent être saisies ici.");
    }

    QHBoxLayout *pressureLayout = new QHBoxLayout;
    pressureLayout->addWidget(systolicEdit);
    pressureLayout->addWidget(new QLabel("/", this));
    pressureLayout->addWidget(diastolicEdit);

    QFormLayout *measuresLayout = new QFormLayout;
    measuresLayout->addRow("Taille (cm) :", heightEdit);
    measuresLayout->addRow("Poids (kg) :", weightEdit);
    measuresLayout->addRow("Tension (mmHg) :", pressureLayout);
    measuresLayout->addRow("Glycémie (g/L) :", glycemiaEdit);
    measuresLayout->addRow("Température (°C) :", temperatureEdit);
    measuresLayout->addRow("Autres mesures :", otherMeasuresEdit);

    QVBoxLayout *measuresBoxLayout = new QVBoxLayout;
    measuresBoxLayout->addWidget(preConsultationLabel);
    measuresBoxLayout->addLayout(measuresLayout);

    QGroupBox *measuresBox = new QGroupBox("Mesures", this);
    measuresBox->setLayout(measuresBoxLayout);

    // Examen clinique.
    motifEdit = new QLineEdit(this);
    motifEdit->setPlaceholderText("Raison de la visite");

    symptomsEdit = new QTextEdit(this);
    symptomsEdit->setPlaceholderText("Symptômes...");
    symptomsEdit->setMaximumHeight(90);

    diagnosisEdit = new QTextEdit(this);
    diagnosisEdit->setPlaceholderText("Diagnostic...");
    diagnosisEdit->setMaximumHeight(90);

    QFormLayout *clinicalLayout = new QFormLayout;
    clinicalLayout->addRow("Motif :", motifEdit);
    clinicalLayout->addRow("Symptômes :", symptomsEdit);
    clinicalLayout->addRow("Diagnostic :", diagnosisEdit);

    // Ordonnances.
    prescriptionButton = new QPushButton("Rédiger une ordonnance", this);
    prescriptionButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(prescriptionButton, &QPushButton::clicked, this, &ConsultationSessionDialog::onPrescriptionClicked);

    prescriptionsList = new QListWidget(this);
    prescriptionsList->setMaximumHeight(70);

    finishButton = new QPushButton("Terminer la consultation", this);
    finishButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(finishButton, &QPushButton::clicked, this, &ConsultationSessionDialog::onFinishClicked);

    interruptButton = new QPushButton("Interrompre la consultation", this);
    interruptButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(interruptButton, &QPushButton::clicked, this, &ConsultationSessionDialog::onInterruptClicked);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(interruptButton);
    buttonLayout->addWidget(finishButton);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(measuresBox);
    mainLayout->addLayout(clinicalLayout);
    mainLayout->addWidget(prescriptionButton);
    mainLayout->addWidget(new QLabel("Ordonnances enregistrées :", this));
    mainLayout->addWidget(prescriptionsList);
    mainLayout->addLayout(buttonLayout);

    // Assistant de saisie (prototype) : discussion à droite du formulaire.
    m_assistant = new OllamaClient(this);
    connect(m_assistant, &OllamaClient::replyReceived, this, &ConsultationSessionDialog::onAssistantReply);
    connect(m_assistant, &OllamaClient::requestFailed, this, &ConsultationSessionDialog::onAssistantFailed);

    QLabel *assistantTitle = new QLabel("Assistant de saisie (prototype)", this);
    assistantTitle->setStyleSheet("font-weight: 600;");

    QLabel *assistantNote = new QLabel(
        "Dictez ou écrivez les informations à reporter. Le traitement est effectué localement par Ollama : "
        "rien ne quitte ce poste et aucune identité de patient n'est transmise au modèle. Relisez toujours "
        "avant d'enregistrer.",
        this);
    assistantNote->setWordWrap(true);
    assistantNote->setStyleSheet("color: #6B7280; font-size: 9pt;");

    chatLog = new QTextBrowser(this);

    chatInput = new QLineEdit(this);
    chatInput->setPlaceholderText("Ex. : tension 13/8, glycémie 1,10, toux sèche depuis 3 jours");
    connect(chatInput, &QLineEdit::returnPressed, this, &ConsultationSessionDialog::onAssistantSendClicked);

    chatSendButton = new QPushButton("Envoyer", this);
    chatSendButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(chatSendButton, &QPushButton::clicked, this, &ConsultationSessionDialog::onAssistantSendClicked);

    QHBoxLayout *chatInputLayout = new QHBoxLayout;
    chatInputLayout->addWidget(chatInput);
    chatInputLayout->addWidget(chatSendButton);

    QVBoxLayout *chatLayout = new QVBoxLayout;
    chatLayout->addWidget(assistantTitle);
    chatLayout->addWidget(assistantNote);
    chatLayout->addWidget(chatLog, 1);
    chatLayout->addLayout(chatInputLayout);

    QHBoxLayout *rootLayout = new QHBoxLayout;
    rootLayout->addLayout(mainLayout, 3);
    rootLayout->addLayout(chatLayout, 2);

    setLayout(rootLayout);
    resize(1020, 720);
}

QJsonObject ConsultationSessionDialog::currentFormState() const
{
    auto number = [](const QLineEdit *edit, bool integer) -> QJsonValue {
        bool ok = true;
        const QVariant value = parseOptionalNumber(edit->text(), integer, ok);
        if (!ok || value.isNull()) {
            return QJsonValue(QJsonValue::Null);
        }
        return integer ? QJsonValue(value.toInt()) : QJsonValue(value.toDouble());
    };

    return QJsonObject{{"height_cm", number(heightEdit, false)},
                        {"weight_kg", number(weightEdit, false)},
                        {"bp_systolic", number(systolicEdit, true)},
                        {"bp_diastolic", number(diastolicEdit, true)},
                        {"glycemia_gl", number(glycemiaEdit, false)},
                        {"temperature_c", number(temperatureEdit, false)},
                        {"other_measures", otherMeasuresEdit->text()},
                        {"motif", motifEdit->text()},
                        {"symptoms", symptomsEdit->toPlainText()},
                        {"diagnosis", diagnosisEdit->toPlainText()}};
}

QStringList ConsultationSessionDialog::applyAssistantFields(const QJsonObject &fields)
{
    QStringList updated;

    auto setNumber = [&](const char *key, QLineEdit *edit, const QString &label, bool integer) {
        if (!fields.contains(key) || !fields.value(key).isDouble()) {
            return;
        }
        const double value = fields.value(key).toDouble();
        edit->setText(integer ? QString::number(qRound(value)) : QString::number(value, 'g', 5));
        updated << label;
    };
    auto setLine = [&](const char *key, QLineEdit *edit, const QString &label) {
        if (fields.contains(key) && fields.value(key).isString()) {
            edit->setText(fields.value(key).toString());
            updated << label;
        }
    };
    auto setText = [&](const char *key, QTextEdit *edit, const QString &label) {
        if (fields.contains(key) && fields.value(key).isString()) {
            edit->setPlainText(fields.value(key).toString());
            updated << label;
        }
    };

    setNumber("height_cm", heightEdit, "Taille", false);
    setNumber("weight_kg", weightEdit, "Poids", false);
    setNumber("bp_systolic", systolicEdit, "Tension systolique", true);
    setNumber("bp_diastolic", diastolicEdit, "Tension diastolique", true);
    setNumber("glycemia_gl", glycemiaEdit, "Glycémie", false);
    setNumber("temperature_c", temperatureEdit, "Température", false);
    setLine("other_measures", otherMeasuresEdit, "Autres mesures");
    setLine("motif", motifEdit, "Motif");
    setText("symptoms", symptomsEdit, "Symptômes");
    setText("diagnosis", diagnosisEdit, "Diagnostic");

    return updated;
}

void ConsultationSessionDialog::appendChat(const QString &speaker, const QString &text)
{
    chatLog->append("<p><b>" + speaker.toHtmlEscaped() + "</b><br>" +
                    text.toHtmlEscaped().replace("\n", "<br>") + "</p>");
}

void ConsultationSessionDialog::onAssistantSendClicked()
{
    const QString text = chatInput->text().trimmed();
    if (text.isEmpty() || m_assistant->isBusy()) {
        return;
    }

    appendChat("Vous", text);

    const QString problem = m_assistant->configurationProblem();
    if (!problem.isEmpty()) {
        appendChat("Assistant", problem);
        return;
    }

    chatInput->clear();
    chatInput->setEnabled(false);
    chatSendButton->setEnabled(false);
    chatSendButton->setText("…");

    m_assistant->sendMessage(text, currentFormState());
}

void ConsultationSessionDialog::onAssistantReply(const QString &reply, const QJsonObject &fields)
{
    chatInput->setEnabled(true);
    chatSendButton->setEnabled(true);
    chatSendButton->setText("Envoyer");
    chatInput->setFocus();

    const QStringList updated = applyAssistantFields(fields);

    QString text = reply;
    if (!updated.isEmpty()) {
        text += "\n\nChamps mis à jour : " + updated.join(", ") + ".";
    }
    appendChat("Assistant", text);
}

void ConsultationSessionDialog::onAssistantFailed(const QString &message)
{
    chatInput->setEnabled(true);
    chatSendButton->setEnabled(true);
    chatSendButton->setText("Envoyer");

    appendChat("Assistant", message);
}

void ConsultationSessionDialog::onPrescriptionClicked()
{
    PrescriptionDialog dialog(m_patient, m_doctorLogin, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QFileInfo savedFile(dialog.savedFilePath());
    // Le dossier a pu être créé à l'instant : on le mémorise pour les ordonnances suivantes.
    m_patient.path = savedFile.absolutePath();
    prescriptionsList->addItem(savedFile.fileName());
}

void ConsultationSessionDialog::onInterruptClicked()
{
    QString message = "Interrompre la consultation ? Les informations saisies ne seront pas enregistrées.";
    if (prescriptionsList->count() > 0) {
        message += "\n\nLes ordonnances déjà enregistrées restent dans le dossier du patient.";
    }

    if (QMessageBox::question(this, "Consultation", message) == QMessageBox::Yes) {
        reject();
    }
}

void ConsultationSessionDialog::onFinishClicked()
{
    bool heightOk = true;
    bool weightOk = true;
    bool systolicOk = true;
    bool diastolicOk = true;
    bool glycemiaOk = true;
    bool temperatureOk = true;

    const QVariant height = parseOptionalNumber(heightEdit->text(), false, heightOk);
    const QVariant weight = parseOptionalNumber(weightEdit->text(), false, weightOk);
    const QVariant systolic = parseOptionalNumber(systolicEdit->text(), true, systolicOk);
    const QVariant diastolic = parseOptionalNumber(diastolicEdit->text(), true, diastolicOk);
    const QVariant glycemia = parseOptionalNumber(glycemiaEdit->text(), false, glycemiaOk);
    const QVariant temperature = parseOptionalNumber(temperatureEdit->text(), false, temperatureOk);

    if (!heightOk || !weightOk || !systolicOk || !diastolicOk || !glycemiaOk || !temperatureOk) {
        QMessageBox::warning(this, "Consultation", "Veuillez saisir des valeurs numériques valides pour les mesures.");
        return;
    }

    QString errorMessage;
    if (!DatabaseManager::createConsultation(-1, m_patient.id, m_doctorLogin,
                                              QDate::currentDate().toString(Qt::ISODate), motifEdit->text(),
                                              diagnosisEdit->toPlainText(), QString(), errorMessage, QString(), false,
                                              weight, height, systolic, diastolic, m_prepUserId,
                                              symptomsEdit->toPlainText(), glycemia, temperature,
                                              otherMeasuresEdit->text())) {
        QMessageBox::warning(this, "Consultation", errorMessage);
        return;
    }

    // La consultation est terminée : le patient quitte la salle d'attente de ce médecin pour aujourd'hui.
    QString waitingRoomError;
    if (!DatabaseManager::removeFromWaitingRoom(m_patient.id, m_doctorLogin,
                                                 QDate::currentDate().toString(Qt::ISODate), waitingRoomError)) {
        QMessageBox::warning(this, "Consultation",
                              "Consultation enregistrée, mais le patient n'a pas pu être retiré de la salle "
                              "d'attente :\n" + waitingRoomError);
        accept();
        return;
    }

    QMessageBox::information(this, "Consultation", "Consultation enregistrée dans l'historique du patient.");
    accept();
}
