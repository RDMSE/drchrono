#ifndef CRONOMETERWINDOW_H
#define CRONOMETERWINDOW_H

#include <QMainWindow>
#include <stdbool.h>
#include <QTimer>
#include <QTime>
#include <QCloseEvent>

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

    void on_btnStart_clicked();

    void startCounterTimer();
    void updateCounterTimer();
    void updateRegisterButton();

private:
    Ui::CronometerWindow *ui;

    bool started = false;
    const QString timeFormat = "HH:mm:ss";


    void setControlsStatus(const bool status);

    QTimer timer;
    QTime startTime;
};
#endif // CRONOMETERWINDOW_H
