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
    ~CronometerWindow() override;

private slots:
    void on_btnStart_clicked();
    void on_btnRegister_clicked();
    void updateCounterTimer() const;
    void updateRegisterButton() const;
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
    bool m_started;
    QDateTime m_startTime;
    QTimer m_timer;
    QTimer m_startButtonUpdateTimer;
    int m_currentTrialId;
    
    // Dynamic events menu
    QMenu* m_eventsSubmenu;
    QVector<Trials::TrialInfo> m_loadedEvents;
    QString m_selectedEventName;
    
    // Configuration
    static constexpr auto timeFormat = "hh:mm:ss";
    static constexpr auto eventMenuTimeFormat = "dd/MM/yyyy hh:mm:ss";
    QString m_dbPath;
    QString m_reportPath;
    int m_openTrialWindowDays;
    
    // Helper methods
    void loadSettings();
    void loadEventsToMenu();
    void selectEventById(int eventId);
    void setControlsStatus(bool status) const;
    void startCounterTimer();
    void stopCounterTimer();
    void loadTodayTrial();
    void checkAndStartRunningTrial();
    void updateMenusState() const;
    void showEventSelectionDialog();
    void generateReport() const;
    void closeOpenedEvents() const;
    static bool canStartEvent(const QDateTime& scheduledDateTime) ;
    void updateStartButtonState();
    [[nodiscard]] bool canGenerateReport() const;
};

#endif // CRONOMETERWINDOW_H
