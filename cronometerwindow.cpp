#include "cronometerwindow.h"
#include "ui_cronometerwindow.h"
#include <QSettings>
#include <QString>
#include <QDebug>
#include <algorithm>
#include <QMessageBox>


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
        std::transform( activeGroupsInfo.begin(),
                        activeGroupsInfo.end(),
                        std::back_inserter(groups),
                        [](const GroupInfo& info) { return info.name; }
                    );

        ui->lstGroups->addItems(groups);
        qDebug() << groups;

        QString activeTrialStartTime = controller->getActiveTrialStartDateTime();
        startTime = QDateTime::fromString(activeTrialStartTime, Qt::ISODate);
        setControlsStatus(true);
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

        setControlsStatus(false);
    }
    updateCounterTimer();
    startCounterTimer();

    ui->btnRegister->setEnabled(false);

    connect(&timer, &QTimer::timeout, this, &CronometerWindow::updateCounterTimer);
    connect(ui->lstGroups, &QComboBox::currentIndexChanged, this, &CronometerWindow::updateRegisterButton);
    connect(ui->edtPlaque, &QLineEdit::textChanged, this, &CronometerWindow::updateRegisterButton);
}

CronometerWindow::~CronometerWindow() {
    delete ui;
}

void CronometerWindow::setControlsStatus(const bool status) {
    ui->lstGroups->setEnabled(status);
    ui->edtPlaque->setEnabled(status);
    ui->btnStart->setText( !status ? "Start" : "Stop");
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
        startCounterTimer();
    } else {
        controller->stopTrial();
        stopCounterTimer();
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
