#ifndef CRONOMETERWINDOW_H
#define CRONOMETERWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QTime>
#include <QDateTime>
#include <QCloseEvent>
#include <QMessageBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include <QDialog>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QDesktopServices>
#include <QDir>
#include "dbmanager.h"
#include "model/trialinfo.h"
#include "participantswindow.h"
#include "loadparticipantswindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class CronometerWindow; }
QT_END_NAMESPACE

class CronometerWindow : public QMainWindow
{
    Q_OBJECT

public:
    CronometerWindow(QWidget *parent = nullptr);
    ~CronometerWindow();

private slots:
    void on_btnStart_clicked();
    void on_btnRegister_clicked();
    void updateCounterTimer();
    void updateRegisterButton();
    void onEventSelected();
    void on_actionCreate_New_Event_triggered();
    void on_actionShow_triggered();
    void on_actionLoad_from_file_triggered();
    void on_actionGenerate_Excel_triggered();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::CronometerWindow *ui;
    DBManager chronoDb;
    
    // cronometer state
    bool started;
    QDateTime startTime;
    QTimer timer;
    QTimer startButtonUpdateTimer;
    int currentTrialId;
    
    // Dynamic events menu
    QMenu* eventsSubmenu;
    QVector<Trials::TrialInfo> loadedEvents;
    QString selectedEventName;
    
    // Configuration
    const char* timeFormat = "hh:mm:ss";
    const char* eventMenuTimeFormat = "dd/MM/yyyy hh:mm";
    QString dbPath;
    QString reportPath;
    int openTrialWindowDays;
    
    // Helper methods
    void loadSettings();
    void loadEventsToMenu();
    void selectEventById(int eventId);
    void setControlsStatus(const bool status);
    void startCounterTimer();
    void stopCounterTimer();
    void loadTodayTrial();
    void checkAndStartRunningTrial();
    void updateMenusState();
    void showEventSelectionDialog();
    void generateReport();
    void closeOpenedEvents();
    bool canStartEvent(const QDateTime& scheduledDateTime) const;
    void updateStartButtonState();
    bool canGenerateReport() const;
};

#endif // CRONOMETERWINDOW_H
