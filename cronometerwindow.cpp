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

    QSettings settings("settings.ini", QSettings::IniFormat);

    QStringList groups = settings.value("Groups").toString().split(",", Qt::SkipEmptyParts);
    std::transform(groups.begin(), groups.end(), groups.begin(), [](const QString &s){ return s.trimmed();});

    ui->lstGroups->clear();
    ui->lstGroups->addItems(groups);

    setControlsStatus(false);

    ui->btnRegister->setEnabled(false);

    connect(&timer, &QTimer::timeout, this, &CronometerWindow::updateCounterTimer);
    connect(ui->lstGroups, &QComboBox::currentIndexChanged, this, &CronometerWindow::updateRegisterButton);
    connect(ui->edtPlaque, &QLineEdit::textChanged, this, &CronometerWindow::updateRegisterButton);
    qDebug() << groups;
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
    setControlsStatus(started);
    startCounterTimer();
}


void CronometerWindow::closeEvent(QCloseEvent *event) {
    if(!started) {
        event->accept();
    } else {
        event->ignore();
    }
}


void CronometerWindow::startCounterTimer() {
    if (started) {
        startTime = QTime::currentTime();  // marca hora inicial
        ui->lblTime->setText("00:00:00");
        timer.start(1000); // atualiza a cada 1s
    } else {
        timer.stop();
    }
}

void CronometerWindow::updateCounterTimer() {
    int secs = startTime.secsTo(QTime::currentTime());
    QTime display(0, 0);
    display = display.addSecs(secs);

    ui->lblTime->setText(display.toString(timeFormat));
}

void CronometerWindow::updateRegisterButton() {
    bool hasGroup = ui->lstGroups->currentIndex() >= 0;
    bool hasPlaque = !ui->edtPlaque->text().trimmed().isEmpty();

    ui->btnRegister->setEnabled(hasGroup && hasPlaque);
}
