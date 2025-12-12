#include "cronometerwindow.h"
#include "ui_cronometerwindow.h"
#include <QSettings>
#include <QString>
#include <QDebug>
#include <algorithm>
#include <QMessageBox>
#include "report.h"



CronometerWindow::CronometerWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::CronometerWindow)
    , chronoDb(dbPath) {
    ui->setupUi(this);

    ui->lstGroups->clear();

    controller = new TrialController(QSqlDatabase::database());

    if (controller->loadActiveTrial()) {
        auto activeGroupsInfo = controller->getTrialGroups();
        QStringList groups;
        std::transform( activeGroupsInfo.value().begin(),
                        activeGroupsInfo.value().end(),
                        std::back_inserter(groups),
                        [](const GroupInfo& info) { return info.name; }
                    );

        ui->lstGroups->addItems(groups);
        qDebug() << groups;

        QString activeTrialStartTime = controller->getActiveTrialStartDateTime();
        startTime = QDateTime::fromString(activeTrialStartTime, Qt::ISODate);
        started = true;
        setControlsStatus(started);
        startCounterTimer();
    } else {
        auto groupsInfo = controller->loadGroupsFromConfiguration("settings.ini");
        QStringList groupNames;
        std::transform( groupsInfo.begin(), 
                        groupsInfo.end(), 
                        std::back_inserter(groupNames), 
                        [](const GroupInfo& info) { return info.name; }
                    );

        ui->lstGroups->addItems(groupNames);
        qDebug() << groupNames;
        startTime = QDateTime::currentDateTime();
        started = false;
        setControlsStatus(started);

        // verify if the loaded trial from settings is already finished on db
        auto trialInfo = controller->getTrialInformationByName(controller->getActiveTrialName());
        if (!trialInfo.has_value()) {
            qFatal() << "Trial from settings not found on db: " << controller->getActiveTrialName();
        }

        if (!trialInfo.value().endDateTime.isEmpty()) {
            QString message = QString("Trial %1 already exist on and it was started on %2 and finished on %3")
                                  .arg(trialInfo.value().name).arg(trialInfo.value().startDateTime).arg(trialInfo.value().endDateTime);
            auto reply = QMessageBox::question(this, "Confirmation", message, QMessageBox::Yes | QMessageBox::No );
            if (reply == QMessageBox::No)
                disableControls();
        }

    }
    updateCounterTimer();
    //startCounterTimer();

    ui->btnRegister->setEnabled(false);

    connect(&timer, &QTimer::timeout, this, &CronometerWindow::updateCounterTimer);
    connect(ui->lstGroups, &QComboBox::currentIndexChanged, this, &CronometerWindow::updateRegisterButton);
    connect(ui->edtPlaque, &QLineEdit::textChanged, this, &CronometerWindow::updateRegisterButton);
}

CronometerWindow::~CronometerWindow() {
    delete ui;
}

void CronometerWindow::disableControls() {
    ui->lstGroups->setEnabled(false);
    ui->edtPlaque->setEnabled(false);
    ui->btnStart->setText("WARNING");
    ui->btnStart->setEnabled(false);
}

void CronometerWindow::setControlsStatus(const bool status) {
    ui->lstGroups->setEnabled(status);
    ui->edtPlaque->setEnabled(status);
    ui->btnStart->setText(!status ? "Start" : "Stop");
}

void CronometerWindow::on_btnStart_clicked() {

    if (started) {
        auto reply = QMessageBox::question(this, "Confirmation", "You really want to stop?", QMessageBox::Yes | QMessageBox::No );
        if (reply == QMessageBox::No)
            return;
    }

    started = !started;

    if (started) {
        controller->startTrial();
        startTime = QDateTime::currentDateTime();
        startCounterTimer();
    } else {
        controller->stopTrial();
        stopCounterTimer();
        QString reportFileName = QString("REP_%1.xlsx").arg(controller->getActiveTrialName());
        Report::exportExcel(controller->getTrialId(), reportFileName, QSqlDatabase::database());
    }

    setControlsStatus(started);
}

void CronometerWindow::closeEvent(QCloseEvent *event) {
    if(!started) {
        event->accept();
    } else {
        event->ignore();
    }
}

void CronometerWindow::startCounterTimer() {
    //startTime = QTime::currentTime();  // marca hora inicial
    //ui->lblTime->setText("00:00:00");
    //startTime = QDateTime::currentDateTime();
    timer.start(1000); // atualiza a cada 1s
}

void CronometerWindow::stopCounterTimer() {
    timer.stop();
}

void CronometerWindow::updateCounterTimer() {
    int secs = startTime.secsTo(QDateTime::currentDateTime());
    QTime display(0, 0);
    display = display.addSecs(secs);

    ui->lblTime->setText(display.toString(timeFormat));
}

void CronometerWindow::updateRegisterButton() {
    bool hasGroup = ui->lstGroups->currentIndex() >= 0;
    bool hasPlaque = !ui->edtPlaque->text().trimmed().isEmpty();

    ui->btnRegister->setEnabled(hasGroup && hasPlaque);
}

void CronometerWindow::on_btnRegister_clicked() {
    QString group = ui->lstGroups->currentText();
    QStringList placas = ui->edtPlaque->text().split(",", Qt::SkipEmptyParts);
    std::transform(placas.begin(), placas.end(), placas.begin(), [](const QString &s){ return s.trimmed(); });
    QDateTime curTime = QDateTime::currentDateTime();
    int duration = startTime.secsTo(curTime);

    QTime t(0,0,0);
    t = t.addSecs(duration);

    QStringList notes;
    for(int i = 0; i < placas.length(); i++){
        auto& placa = placas.at(i);
        QString note = QString("Athlete %1 from group %2 finished on %3 with duration of %4 hours, %5 minutes and %6 seconds")
                            .arg(placa).arg(group).arg(curTime.toString(Qt::ISODate)).arg(t.hour()).arg(t.minute()).arg(t.second());

        auto success = controller->registerEvent(
            controller->getTrialId(),
            group,
            placa,
            controller->getActiveTrialStartDateTime(),
            curTime.toString(Qt::ISODate),
            duration * 1000,
            note
        );

        if (!success) {
            qWarning() << "Error saving time for athlete " << QString(group).append("-").append(placa);
        }
    }
}

