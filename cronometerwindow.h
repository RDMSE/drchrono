#ifndef CRONOMETERWINDOW_H
#define CRONOMETERWINDOW_H

#include <QMainWindow>
#include <stdbool.h>
#include <QTimer>
#include <QTime>
#include <QCloseEvent>
#include "dbmanager.h"
#include "trialcontroller.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class CronometerWindow;
}
QT_END_NAMESPACE

class CronometerWindow : public QMainWindow
{
    Q_OBJECT

public:
    CronometerWindow(QWidget *parent = nullptr);
    ~CronometerWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void startCounterTimer();
    void stopCounterTimer();
    void updateCounterTimer();
    void updateRegisterButton();

    void on_btnRegister_clicked();
    void on_btnStart_clicked();

private:
    Ui::CronometerWindow *ui;

    bool started = false;
    const QString timeFormat = "HH:mm:ss";
    const QString dbPath = "cronometro.db";

    void setControlsStatus(const bool status);
    void disableControls();

    QTimer timer;
    QDateTime startTime;
    DBManager chronoDb;
    TrialController* controller;
};
#endif // CRONOMETERWINDOW_H
