#include "cronometerwindow.h"
#include "ui_cronometerwindow.h"
#include <QSettings>
#include <QString>
#include <QDebug>
#include <algorithm>
#include <QMessageBox>
#include <QDate>
#include <QDialog>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QMenu>
#include "report.h"
#include "repository/trials/trialsrepository.h"
#include "repository/registrations/registrationsrepository.h"
#include "repository/results/resultsrepository.h"
#include "neweventwindow.h"
#include "participantswindow.h"
#include "loadparticipantswindow.h"
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QFileDialog>
#include <QFileInfo>
#include <QRegularExpression>
#include "utils/timeutils.h"

CronometerWindow::CronometerWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::CronometerWindow)
    , chronoDb("") {
    
    // first load settings to get the database path
    loadSettings();
    
    // Initialize database with the path from settings
    chronoDb = DBManager(m_dbPath);
    
    ui->setupUi(this);
    
    // Fix the window size as defined in Qt Designer
    setFixedSize(size());
    
    // Open the database
    if (!chronoDb.open()) {
        QMessageBox::critical(this, "Database Error", 
            QString("Failed to open database: %1").arg(m_dbPath));
    }
    
    // Initialize state
    m_started = false;
    m_startTime = Utils::DateTimeUtils::now();
    m_currentTrialId = -1; // Will be set when starting a trial
    
    setControlsStatus(m_started);
    updateCounterTimer();
    
    ui->btnRegister->setEnabled(false);

    connect(&m_timer, &QTimer::timeout, this, &CronometerWindow::updateCounterTimer);
    connect(ui->edtPlaque, &QLineEdit::textChanged, this, &CronometerWindow::updateRegisterButton);
    
    // Configure timer to update the state of the Start button
    connect(&m_startButtonUpdateTimer, &QTimer::timeout, this, &CronometerWindow::updateStartButtonState);
    m_startButtonUpdateTimer.start(60000); // Update every minute
    
    // Connect Exit menu directly to close()
    auto* exitAction = findChild<QAction*>("actionExit");
    if (exitAction) {
        connect(exitAction, &QAction::triggered, this, &QWidget::close);
    }
    
    // Initialize events submenu
    m_eventsSubmenu = nullptr;

    // close open events with scheduled datetime < NOW() - DAYS(openTrialWindowDays)
    closeOpenedEvents();
    
    // Load events to menu
    loadEventsToMenu();
    
    // Check if there is a running trial and start automatically
    checkAndStartRunningTrial();
}

CronometerWindow::~CronometerWindow() {
    delete ui;
}

void CronometerWindow::setControlsStatus(const bool status) const {
    ui->edtPlaque->setEnabled(status);
    ui->btnStart->setText(!status ? "Start" : "Stop");
}

void CronometerWindow::on_btnStart_clicked() {
    Trials::Repository trialsRepo(chronoDb.database());

    if (m_started) {
        if (auto reply = QMessageBox::question(this, "Confirmation", "You really want to stop?", QMessageBox::Yes | QMessageBox::No); reply == QMessageBox::No)
            return;
    }

    m_started = !m_started;
    
    // Update menus state
    updateMenusState();

    if (m_started) {
        // Check if there are existing results before starting
        Results::Repository resultsRepo(chronoDb.database());

        if (auto existingResultsCheck = resultsRepo.getResultsByTrial(m_currentTrialId); existingResultsCheck.has_value() && !existingResultsCheck.value().isEmpty()) {
            auto reply = QMessageBox::question(this, "Existing Results", 
                QString("There are %1 existing results for this trial.\n\nDo you want to remove all previous results and start fresh?")
                    .arg(existingResultsCheck.value().size()),
                QMessageBox::Yes | QMessageBox::No);
                
            if (reply == QMessageBox::No) {
                m_started = false; // Reverter estado
                updateMenusState();
                return;
            }
            
            // Delete all existing results
            const auto& existingResults = existingResultsCheck.value();
            for (const auto& result : existingResults) {
                if (auto deleteResult = resultsRepo.deleteResultById(result.id); !deleteResult.has_value()) {
                    QMessageBox::warning(this, "Error",
                                         QString("Failed to delete result ID %1: %2").arg(QString::number(result.id), deleteResult.error()));
                }
            }
            qDebug() << "Deleted" << existingResults.size() << "existing results for trial" << m_currentTrialId;
        }
        
        m_startTime = Utils::DateTimeUtils::now();
        startCounterTimer();
        qDebug() << "Started trial:" << m_selectedEventName << "with ID:" << m_currentTrialId;

        // Create struct with only the field we want to update
        Trials::TrialInfo updateInfo;
        updateInfo.startDateTime = m_startTime;
        if (auto startedTrial = trialsRepo.updateTrialById(m_currentTrialId, updateInfo); startedTrial.has_value()) {
            qDebug() << "Saved trial :" << startedTrial.value().name << " at " << startedTrial.value().startDateTime;
        } else {
            qFatal("Error saving trial: %s", startedTrial.error().toLocal8Bit().constData());
        }
    } else {
        stopCounterTimer();
        ui->btnRegister->setEnabled(false);
        qDebug() << "Finalizing trial with ID:" << m_currentTrialId;
        
        // Finalize trial by setting endDateTime
        QDateTime endTime = Utils::DateTimeUtils::now();
        Trials::TrialInfo updateInfo;
        updateInfo.endDateTime = endTime;

        if (auto finishedTrial = trialsRepo.updateTrialById(m_currentTrialId, updateInfo); finishedTrial.has_value()) {
            qDebug() << "Trial finished:" << finishedTrial.value().name 
                     << "End time:" << finishedTrial.value().endDateTime;
        } else {
            QMessageBox::warning(this, "Error", 
                QString("Error finalizing trial: %1").arg(finishedTrial.error()));
        }
    }

    setControlsStatus(m_started);
}

void CronometerWindow::closeEvent(QCloseEvent *event) {
    if(!m_started) {
        event->accept();
    } else {
        if (const auto reply =
                QMessageBox::question(this, "Trial in Progress", "A trial is currently running. Do you want to stop it and exit?",
                    QMessageBox::Yes | QMessageBox::No); reply == QMessageBox::Yes) {
            m_started = false;
            stopCounterTimer();
            event->accept();
        } else {
            event->ignore();
        }
    }
}

void CronometerWindow::startCounterTimer() {
    m_timer.start(1000); // update every 1s
}

void CronometerWindow::stopCounterTimer() {
    m_timer.stop();
}

void CronometerWindow::updateCounterTimer() const {
    const int secs = static_cast<int>(m_startTime.secsTo(Utils::DateTimeUtils::now()));
    QTime display(0, 0);
    display = display.addSecs(secs);

    ui->lblTime->setText(display.toString(timeFormat));
}

void CronometerWindow::updateRegisterButton() const {
    bool hasPlaque = !ui->edtPlaque->text().trimmed().isEmpty();
    ui->btnRegister->setEnabled(m_started && hasPlaque);
}

void CronometerWindow::on_btnRegister_clicked() {
    if (!m_started || m_currentTrialId == -1) {
        QMessageBox::warning(this, "Warning", "No active trial. Please start a trial first.");
        return;
    }
    
    QStringList placas = ui->edtPlaque->text().split(",", Qt::SkipEmptyParts);
    std::ranges::transform(placas, placas.begin(), [](const QString &s){ return s.trimmed(); });
    
    QDateTime curTime = Utils::DateTimeUtils::now();
    int durationMs = static_cast<int>(m_startTime.msecsTo(curTime));

    int registered = 0;
    int errors = 0;
    QStringList errorMessages;

    try {
        Registrations::Repository registrationsRepo(chronoDb.database());
        Results::Repository resultsRepo(chronoDb.database());

        for(const auto& placa : placas) {
            // Search for participant registration by plate number
            auto registrationResult = registrationsRepo.getRegistrationByPlateCode(m_currentTrialId, placa);
            
            if (!registrationResult.has_value()) {
                errorMessages.append(QString("Placa %1: %2").arg(placa, registrationResult.error()));
                errors++;
                continue;
            }
            
            auto registration = registrationResult.value();
            
            // Create the result (allows multiple results for the same plate)
            QString note = QString("Athlete %1 finished at %2")
                                .arg(placa, curTime.toString(Qt::ISODate));
            
            auto resultCreated = resultsRepo.createResult(
                registration.id,
                m_startTime,
                curTime,
                durationMs,
                note
            );
            
            if (resultCreated.has_value()) {
                registered++;
                qDebug() << "Successfully registered result for plate" << placa 
                         << "Registration ID:" << registration.id
                         << "Result ID:" << resultCreated.value().id
                         << "Duration:" << durationMs << "ms";
            } else {
                errorMessages.append(QString("Placa %1: %2").arg(placa, resultCreated.error()));
                errors++;
            }
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", QString("Erro ao registrar resultados: %1").arg(e.what()));
        return;
    }

    // Clear field after registration
    ui->edtPlaque->clear();
    ui->btnRegister->setEnabled(false);
    
    // Show result in statusBar
    QString message;
    if (registered > 0 && errors == 0) {
        message = QString("✓ Registrados %1 resultado(s) com sucesso!").arg(registered);
        statusBar()->showMessage(message, 5000); // 5 seconds
        statusBar()->setStyleSheet("QStatusBar { background-color: #d4edda; color: #155724; }");
    } else if (registered > 0 && errors > 0) {
        message = QString("⚠ Registrados %1 resultado(s) - %2 erro(s)")
                    .arg(registered)
                    .arg(errors);
        statusBar()->showMessage(message, 8000); // 8 segundos
        statusBar()->setStyleSheet("QStatusBar { background-color: #fff3cd; color: #856404; }");
        
        // Detailed log of errors
        qDebug() << "Detailed errors:" << errorMessages.join("; ");
    } else {
        message = QString("✗ Nenhum resultado registrado - %1 erro(s)")
                    .arg(errors);
        statusBar()->showMessage(message, 8000); // 8 seconds
        statusBar()->setStyleSheet("QStatusBar { background-color: #f8d7da; color: #721c24; }");
        
        // Detailed log of errors
        qDebug() << "Detailed errors:" << errorMessages.join("; ");
        return;
    }
    
    // Results registered successfully (no automatic report generation)
}

void CronometerWindow::generateReport() const {
    if (m_currentTrialId == -1) {
        return;
    }
    
    // Define report file name
    const QString defaultFileName = QString("report_%1_%2.xlsx")
                                .arg(m_selectedEventName, QDate::currentDate().toString("yyyy-MM-dd"));
    
    QString defaultFilePath = QDir(m_reportPath).filePath(defaultFileName);
    
    QString fileName = defaultFilePath;
    
    if (fileName.isEmpty()) {
        return;
    }
    
    // Generate the report
    if (!Report::exportExcel(m_currentTrialId, fileName, chronoDb.database())) {
        statusBar()->showMessage("✗ Error generating Excel report", 5000);
        statusBar()->setStyleSheet("QStatusBar { background-color: #f8d7da; color: #721c24; }");
    }
}

void CronometerWindow::on_actionGenerate_Excel_triggered() {
    if (m_currentTrialId <= 0) {
        QMessageBox::warning(this, "No Event Selected", 
            "Select an event before generating the Excel report.");
        return;
    }
    
    // Generate report for the selected event
    generateReport();
}



void CronometerWindow::loadTodayTrial() {
    try {
        const Trials::Repository trialsRepo(chronoDb.database());
        const QDate today = QDate::currentDate();
        
        auto trialsResult = trialsRepo.getTrialsByDate(today);
        
        if (!trialsResult.has_value()) {
            qDebug() << "Error loading today's trials:" << trialsResult.error();
            return;
        }
        
        const auto& todayTrials = trialsResult.value();
        
        if (todayTrials.isEmpty()) {
            qDebug() << "No trials scheduled for today (" << today.toString(Qt::ISODate) << ")";
            setWindowTitle("No trial scheduled for today");
            return;
        }
        
        // Get the first trial of the day (can be improved to choose the closest to the current time)
        const auto& selectedTrial = todayTrials.first();
        m_currentTrialId = selectedTrial.id;

        const QString windowTitle = QString("%1 (Scheduled: %2)")
                                .arg(selectedTrial.name, selectedTrial.scheduledDateTime.toString(eventMenuTimeFormat));
        
        setWindowTitle(windowTitle);
        
        qDebug() << "Loaded trial for today:" << selectedTrial.name 
                 << "ID:" << selectedTrial.id 
                 << "Scheduled:" << selectedTrial.scheduledDateTime;
                 
        // If there are multiple trials today, inform the user
        if (todayTrials.size() > 1) {
            QStringList trialNames;
            for (const auto& trial : todayTrials) {
                trialNames << QString("%1 (%2)")
                              .arg(trial.name, trial.scheduledDateTime.toString("hh:mm"));
            }
            
            QMessageBox::information(this, "Multiple Trials Today", 
                QString("Multiple trials scheduled for today:\n\n%1\n\nSelected: %2")
                    .arg(trialNames.join("\n"), selectedTrial.name));
        }
        
    } catch (const std::exception& e) {
        qWarning() << "Exception loading today's trial:" << e.what();
    } catch (...) {
        qWarning() << "Unknown exception loading today's trial";
    }
}



void CronometerWindow::loadSettings() {
    // Detect the project root (where settings.ini is located)
    QString projectRoot = QCoreApplication::applicationDirPath();
    QString settingsPath = projectRoot + "/settings.ini";
    
    // If settings.ini is not in the executable directory, look in the current directory
    if (!QFile::exists(settingsPath)) {
        settingsPath = "settings.ini";
        projectRoot = QDir::currentPath();
    }
    
    QSettings settings(settingsPath, QSettings::IniFormat);
    
    // Load database settings
    settings.beginGroup("Database");
    const QString relativePath = settings.value("Path", "chronometer.db").toString();
    settings.setValue("Path", relativePath);
    settings.endGroup();
    
    // Build full path using the project root
    if (QDir::isRelativePath(relativePath)) {
        m_dbPath = projectRoot + "/" + relativePath;
    } else {
        m_dbPath = relativePath; // If absolute, use as is
    }
    
    // Load report settings
    settings.beginGroup("Reports");
    const QString relativeReportPath = settings.value("OutputPath", "reports").toString();
    settings.setValue("OutputPath", relativeReportPath);
    settings.endGroup();
    
    // Build full path for reports
    if (QDir::isRelativePath(relativeReportPath)) {
        m_reportPath = projectRoot + "/" + relativeReportPath;
    } else {
        m_reportPath = relativeReportPath; // If absolute, use as is
    }
    
    // Create reports directory if it doesn't exist
    if (QDir reportDir(m_reportPath); !reportDir.exists()) {
        reportDir.mkpath(".");
        qDebug() << "Created reports directory:" << m_reportPath;
    }

    // read OpenTrialWindowDays
    settings.beginGroup("General");
    // Other settings can be loaded here in the future
    m_openTrialWindowDays = settings.value("OpenTrialWindowDays", "2").toInt();
    settings.endGroup();

    
    // Log loaded configuration
    qDebug() << "Project root detected:" << projectRoot;
    qDebug() << "Database path resolved to:" << m_dbPath;
    qDebug() << "Reports path resolved to:" << m_reportPath;
}

void CronometerWindow::loadEventsToMenu() {
    try {
        const Trials::Repository trialsRepo(chronoDb.database());
        
        auto trialsResult = trialsRepo.getAllTrials();
        
        if (!trialsResult.has_value()) {
            qWarning() << "Error loading trials for menu:" << trialsResult.error();
            return;
        }
        
        m_loadedEvents = trialsResult.value();
        
        // Criar ou limpar submenu
        if (m_eventsSubmenu) {
            m_eventsSubmenu->clear();
        } else {
            m_eventsSubmenu = new QMenu("Select Event", this);
            
            // Encontrar a ação "List Events" no menu e adicionar o submenu
            QMenu* eventMenu = nullptr;
            for (QMenu* menu : findChildren<QMenu*>()) {
                if (menu->title() == "Event") {
                    eventMenu = menu;
                    break;
                }
            }
            
            if (eventMenu) {
                eventMenu->addSeparator();
                eventMenu->addMenu(m_eventsSubmenu);
            }
        }
        
        // Load previously selected event from settings
        QSettings settings("settings.ini", QSettings::IniFormat);
        settings.beginGroup("General");
        const QString previouslySelected = settings.value("SelectedEvent", "").toString();
        const int previouslySelectedId = settings.value("SelectedEventId", -1).toInt();
        settings.endGroup();
        
        // Add events to submenu
        ui->btnStart->setEnabled(false);
        if (m_loadedEvents.isEmpty()) {
            QAction* noEventsAction = m_eventsSubmenu->addAction("No events available");
            noEventsAction->setEnabled(false);
        } else {
            for (const auto& trial : m_loadedEvents) {
                QString actionText = QString("%1").arg(trial.name);
                if (trial.scheduledDateTime.isValid()) {
                    actionText += QString(" (%1)").arg(trial.scheduledDateTime.toString(eventMenuTimeFormat));
                }
                
                QAction* eventAction = m_eventsSubmenu->addAction(actionText);
                eventAction->setData(trial.id); // Armazenar ID do trial
                eventAction->setCheckable(true);
                
                // Restaurar seleção anterior se existir
                if (trial.id == previouslySelectedId && !previouslySelected.isEmpty()) {
                    eventAction->setChecked(true);
                    m_currentTrialId = trial.id;
                    m_selectedEventName = trial.name;
                    
                    QString windowTitle = trial.name;
                    if (trial.scheduledDateTime.isValid()) {
                        windowTitle += QString(" (Scheduled: %1)")
                                          .arg(trial.scheduledDateTime.toString(eventMenuTimeFormat));
                    }
                    setWindowTitle(windowTitle);
                    
                    qDebug() << "Restored previous selection:" << trial.name;

                    // Update initial state of the Start button
                    updateStartButtonState();

                }
                
                connect(eventAction, &QAction::triggered, this, &CronometerWindow::onEventSelected);
            }
        }
        
        qDebug() << "Loaded" << m_loadedEvents.size() << "events to submenu";
        
        // Update menus state after loading events
        updateMenusState();
        
    } catch (const std::exception& e) {
        qWarning() << "Exception loading events to menu:" << e.what();
    } catch (...) {
        qWarning() << "Unknown exception loading events to menu";
    }
}

void CronometerWindow::onEventSelected() {
    auto* senderAction = qobject_cast<QAction*>(sender());
    if (!senderAction) return;
    
    // Do not allow changing trial if the timer is running
    if (m_started) {
        QMessageBox::warning(this, "Trial Running", 
            "You cannot change the event while the timer is running.\n\nStop the timer before selecting another event.");
        return;
    }
    
    int selectedTrialId = senderAction->data().toInt();
    
    // Find the selected trial
    const auto it = std::ranges::find_if(m_loadedEvents,
                                   [selectedTrialId](const Trials::TrialInfo& trial) {
                                       return trial.id == selectedTrialId;
                                   });
    
    if (it != m_loadedEvents.end()) {
        const auto& selectedTrial = *it;
        m_currentTrialId = selectedTrialId;
        m_selectedEventName = selectedTrial.name; // Save event name
        
        // Uncheck all actions in the submenu
        if (m_eventsSubmenu) {
            for (QAction* action : m_eventsSubmenu->actions()) {
                action->setCheckable(true);
                action->setChecked(false);
            }
        }
        
        // Check the selected action
        senderAction->setCheckable(true);
        senderAction->setChecked(true);
        
        QString windowTitle = selectedTrial.name;
        if (selectedTrial.scheduledDateTime.isValid()) {
            windowTitle += QString(" (Scheduled: %1)")
                              .arg(selectedTrial.scheduledDateTime.toString(eventMenuTimeFormat));
        }
        
        setWindowTitle(windowTitle);
        
        qDebug() << "Event selected:" << selectedTrial.name << "ID:" << selectedTrialId;
        qDebug() << "Selected event name saved:" << m_selectedEventName;

        // Update initial state of the Start button
        updateStartButtonState();
        
        // Save the selection in settings to persist between sessions
        QSettings settings("settings.ini", QSettings::IniFormat);
        settings.beginGroup("General");
        settings.setValue("SelectedEvent", m_selectedEventName);
        settings.setValue("SelectedEventId", selectedTrialId);
        settings.endGroup();
        
        // Update menus state
        updateMenusState();
    }
}

void CronometerWindow::on_actionCreate_New_Event_triggered()
{
    try {
        NewEventWindow newEventDialog(this);
        
        // Load existing events for autocomplete
        Trials::Repository trialsRepo(chronoDb.database());
        if (auto existingTrialsResult = trialsRepo.getAllTrials(); existingTrialsResult.has_value()) {
            QStringList existingEventNames;
            for (const auto& trial : existingTrialsResult.value()) {
                if (!trial.name.isEmpty()) {
                    existingEventNames.append(trial.name);
                }
            }
            newEventDialog.setExistingEventNames(existingEventNames);
        }

        if (auto status = newEventDialog.exec(); status == QDialog::Accepted) {
            QString eventName = newEventDialog.getEventName();
            QDateTime startTime = newEventDialog.getEventStartTime();

            qDebug() << "Event name:" << eventName;
            qDebug() << "Scheduled date and time:" << startTime.toString(eventMenuTimeFormat);

            auto trailInfo = trialsRepo.getTrialByName(eventName);
            if (!trailInfo.has_value()) {
                QMessageBox::warning(this, "Error", "Error fetching trail on database");
                return;
            }

            int eventId = -1;
            if (trailInfo.value().name.isEmpty()) {
                // Create new event
                auto createdTrial = trialsRepo.createTrial(eventName, startTime);
                if (!createdTrial.has_value()) {
                    QMessageBox::warning(this, "Error", "Error creating trail '" + eventName + "' on database");
                    return;
                }
                eventId = createdTrial.value().id;
                qDebug() << "Created trial '" << eventName << " ' with ID '" << eventId << "'";
            } else {
                // Update existing event
                Trials::TrialInfo trail;
                trail.id = trailInfo.value().id;  // Use the actual event ID
                trail.name = eventName;
                trail.scheduledDateTime = startTime;
                // startDateTime and endDateTime keep default values (epoch=0)

                auto updatedTrial = trialsRepo.updateTrialById(trailInfo.value().id, trail);
                if (!updatedTrial.has_value()) {
                    QMessageBox::warning(this, "Error", "Error updating trail '" + eventName + "' on database");
                    return;
                }
                eventId = updatedTrial.value().id;
                qDebug() << "Updated trial '" << eventName << " ' with ID '" << eventId << "'";
            }
            
            // Update the list of events in the menu
            loadEventsToMenu();
            
            // Select the newly created/updated event only if the timer is not running
            if (eventId != -1 && !m_started) {
                selectEventById(eventId);
            } else if (m_started) {
                QMessageBox::information(this, "Event Created", 
                    QString("Event '%1' created successfully!\n\nSince the timer is running, the current event was not changed.").arg(eventName));
            }
        }

    } catch (const std::exception& e) {
        qWarning() << "Exception saving new event:" << e.what();
    } catch (...) {
        qWarning() << "Unknown exception saving new event";
    }
}

void CronometerWindow::selectEventById(int eventId)
{
    try {
        if (!m_eventsSubmenu) {
            qWarning() << "Events submenu not initialized";
            return;
        }
        
        // Uncheck all actions first
        for (QAction* action : m_eventsSubmenu->actions()) {
            action->setChecked(false);
        }
        
        // Find and select the action corresponding to eventId
        for (QAction* action : m_eventsSubmenu->actions()) {
            if (action->data().toInt() == eventId) {
                action->setChecked(true);
                
                // Find the corresponding trial to update the window title
                const auto it = std::ranges::find_if(m_loadedEvents,
                                                     [eventId](const Trials::TrialInfo& trial) {
                                                         return trial.id == eventId;
                                                     });
                
                if (it != m_loadedEvents.end()) {
                    m_currentTrialId = it->id;
                    m_selectedEventName = it->name;
                    
                    QString windowTitle = it->name;
                    if (it->scheduledDateTime.isValid()) {
                        windowTitle += QString(" (Scheduled: %1)")
                                          .arg(it->scheduledDateTime.toString(eventMenuTimeFormat));
                    }
                    setWindowTitle(windowTitle);
                    
                    // Save the selection in settings to persist between sessions
                    QSettings settings("settings.ini", QSettings::IniFormat);
                    settings.beginGroup("General");
                    settings.setValue("SelectedEvent", it->name);
                    settings.setValue("SelectedEventId", it->id);
                    settings.endGroup();
                    
                    qDebug() << "Selected event:" << it->name << "with ID:" << it->id;
                }
                break;
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "Exception selecting event by ID:" << e.what();
    } catch (...) {
        qWarning() << "Unknown exception selecting event by ID";
    }
}

void CronometerWindow::on_actionShow_triggered()
{
    if (m_currentTrialId <= 0) {
        // Try to load today's event if none is selected
        loadTodayTrial();
        
        if (m_currentTrialId <= 0) {
            const QMessageBox::StandardButton reply = QMessageBox::question(this, "No Active Event",
                "No event is selected. Do you want to see participants of a specific event?",
                QMessageBox::Yes | QMessageBox::No);
                
            if (reply == QMessageBox::Yes) {
                // Show list of events to choose from
                showEventSelectionDialog();
                if (m_currentTrialId <= 0) {
                    return; // User canceled or did not select anything
                }
            } else {
                return;
            }
        }
    }
    
    try {
        const Trials::Repository trialsRepo(chronoDb.database());
        auto trialResult = trialsRepo.getTrialById(m_currentTrialId);
        
        if (!trialResult.has_value() || trialResult.value().name.isEmpty()) {
            QMessageBox::warning(this, "Erro", "Erro ao carregar informações do evento atual.");
            return;
        }
        
        ParticipantsWindow participantsDialog(chronoDb, this);
        participantsDialog.setCurrentTrial(trialResult.value());
        participantsDialog.exec();
        
    } catch (const std::exception& e) {
        qDebug() << "Error showing participants:" << e.what();
        QMessageBox::critical(this, "Erro", "Erro ao abrir janela de participantes: " + QString(e.what()));
    }
}

void CronometerWindow::on_actionLoad_from_file_triggered()
{
    try {
        // Do not allow importing participants if the timer is running
        if (m_started) {
            QMessageBox::warning(this, "Timer Running", 
                "It is not possible to import participants while the timer is running.\n\nStop the timer before importing participants.");
            return;
        }
        
        // Check if there is a selected trial and if it already has participants
        if (m_currentTrialId > 0) {
            const Registrations::Repository registrationsRepo(chronoDb.database());

            if (auto existingRegistrations = registrationsRepo.getRegistrationsByTrial(m_currentTrialId); existingRegistrations.has_value() && !existingRegistrations.value().isEmpty()) {
                const auto reply = QMessageBox::question(this, "Existing Participants",
                    QString("The current event already has %1 participant(s) registered.\n\nImporting new participants may cause conflicts.\n\nDo you want to continue anyway?")
                    .arg(existingRegistrations.value().size()),
                    QMessageBox::Yes | QMessageBox::No);
                    
                if (reply == QMessageBox::No) {
                    return;
                }
            }
        }

        const Trials::Repository trialsRepo(chronoDb.database());
        auto trialsResult = trialsRepo.getAllTrials();
        
        if (!trialsResult.has_value()) {
            QMessageBox::warning(this, "Error", "Error loading events: " + trialsResult.error());
            return;
        }
        
        LoadParticipantsWindow loadDialog(chronoDb, this);
        
        // Set active trial ID if there's one selected
        if (m_currentTrialId > 0) {
            loadDialog.setActiveTrialId(m_currentTrialId);
        }
        
        loadDialog.setAvailableTrials(trialsResult.value());
        
        if (loadDialog.exec() == QDialog::Accepted) {
            // Refresh current event display if needed
            loadEventsToMenu();
            QMessageBox::information(this, "Success", 
                "Participants imported successfully! Use 'Show Participants' to view them.");
        }
        
    } catch (const std::exception& e) {
        qDebug() << "Error loading participants from file:" << e.what();
        QMessageBox::critical(this, "Error", "Error opening import window: " + QString(e.what()));
    }
}

void CronometerWindow::showEventSelectionDialog()
{
    try {
        const Trials::Repository trialsRepo(chronoDb.database());
        auto trialsResult = trialsRepo.getAllTrials();
        
        if (!trialsResult.has_value() || trialsResult.value().isEmpty()) {
            QMessageBox::information(this, "Information", "No events registered in the system.");
            return;
        }
        
        QStringList eventNames;
        QVector<int> eventIds;
        
        for (const auto& trial : trialsResult.value()) {
            QString displayText = QString("%1 - %2")
                                .arg(trial.name, trial.scheduledDateTime.toString( eventMenuTimeFormat));
            eventNames.append(displayText);
            eventIds.append(trial.id);
        }
        
        bool ok;
        const QString selectedEvent = QInputDialog::getItem(this,
            "Select Event", 
            "Choose the event to view participants:",
            eventNames, 0, false, &ok);
            
        if (ok && !selectedEvent.isEmpty()) {
            if (int selectedIndex = static_cast<int>(eventNames.indexOf(selectedEvent)); selectedIndex >= 0) {
                m_currentTrialId = eventIds[selectedIndex];
                
                // Update window title
                const auto& selectedTrial = trialsResult.value()[selectedIndex];
                const QString windowTitle = QString("%1 (Scheduled: %2)")
                                    .arg(selectedTrial.name, selectedTrial.scheduledDateTime.toString(eventMenuTimeFormat));
                setWindowTitle(windowTitle);
            }
        }
        
    } catch (const std::exception& e) {
        qDebug() << "Error in showEventSelectionDialog:" << e.what();
        QMessageBox::critical(this, "Error", "Error loading events list.");
    }
}

void CronometerWindow::closeOpenedEvents() const {
    try {
        Trials::Repository trialRepository(chronoDb.database());

        auto trials = trialRepository.getAllTrials();
        if (!trials) {
            qDebug() << "Error fetching all trials";
            return;
        }

        auto limitDate = QDate::currentDate().addDays(-m_openTrialWindowDays);

        const auto epochZero = Utils::DateTimeUtils::epochZero();
        const auto now = Utils::DateTimeUtils::now();
        
        // Filter trials that need to be closed and process each one
        std::ranges::for_each(trials.value(),
                              [&limitDate, &epochZero, &now, &trialRepository](auto& trial) {
                                  // Check if the trial meets the criteria to be closed:
                                  // 1. scheduledDateTime < limitDate (trial too old)
                                  // 2. endDateTime == epoch zero (not yet finished)
                                  if (trial.scheduledDateTime.date() < limitDate && trial.endDateTime == epochZero) {
                    
                                      // If not yet started, set start time to now
                                      if (trial.startDateTime == epochZero) {
                                          trial.startDateTime = now;
                                      }
                    
                                      // Finish the trial
                                      trial.endDateTime = now;
                    
                                      // Update in the database
                                      (void)trialRepository.updateTrialById(trial.id, trial);
                                  }
                              });


    } catch (const std::exception& e) {
        qFatal("Error in closeOpenEvents: %s", e.what());
    }
}

void CronometerWindow::checkAndStartRunningTrial() {
    try {
        const Trials::Repository trialsRepo(chronoDb.database());
        
        auto runningTrialResult = trialsRepo.getRunningTrial(m_openTrialWindowDays);
        
        if (!runningTrialResult.has_value()) {
            qDebug() << "Error checking running trial:" << runningTrialResult.error();
            return;
        }
        
        const auto& runningTrialOptional = runningTrialResult.value();
        if (!runningTrialOptional.has_value()) {
            qDebug() << "No running trial found";
            return;
        }
        
        const auto& runningTrial = runningTrialOptional.value();
        
        // Set the current trial
        m_currentTrialId = runningTrial.id;
        m_selectedEventName = runningTrial.name;
        
        // Set the chronometer with the trial's startDateTime
        m_startTime = runningTrial.startDateTime;
        m_started = true;
        
        // Update interface
        setControlsStatus(m_started);
        ui->btnStart->setEnabled(true);  // Enable button to allow finishing the trial
        startCounterTimer();
        
        QString windowTitle = QString("%1 (RUNNING)")
                                .arg(runningTrial.name);
        setWindowTitle(windowTitle);
        
        // Mark event as selected in the menu
        if (m_eventsSubmenu) {
            for (QAction* action : m_eventsSubmenu->actions()) {
                if (action->data().toInt() == runningTrial.id) {
                    action->setChecked(true);
                    break;
                }
            }
        }
        
        qDebug() << "Auto-started running trial:" << runningTrial.name 
                 << "ID:" << runningTrial.id 
                 << "Started at:" << runningTrial.startDateTime;
                 
        // Show notification to the user
        const QString message = QString("Chronometer automatically resumed for trial: %1\nStarted at: %2")
                           .arg(runningTrial.name, runningTrial.startDateTime.toString(eventMenuTimeFormat));
        
        QMessageBox::information(this, "Trial Resumed", message);
        
        // Update menu states after automatically resuming trial
        updateMenusState();
        
    } catch (const std::exception& e) {
        qWarning() << "Exception checking running trial:" << e.what();
    } catch (...) {
        qWarning() << "Unknown exception checking running trial";
    }
}

void CronometerWindow::updateMenusState() const {
    // Disable event selection if the chronometer is running
    if (m_eventsSubmenu) {
        m_eventsSubmenu->setEnabled(!m_started);
    }
    
    // Find the "Event" menu and disable "Load from file" if necessary
    const QMenu* eventMenu = nullptr;
    for (QMenu* menu : findChildren<QMenu*>()) {
        if (menu->title() == "Event") {
            eventMenu = menu;
            break;
        }
    }
    
    if (eventMenu) {
        // Search for the "Load from file" action
        for (QAction* action : eventMenu->actions()) {
            if (action->text().contains("Load from file")) {
                // Check if there are participants in the current trial
                bool hasParticipants = false;
                if (m_currentTrialId > 0) {
                    try {
                        const Registrations::Repository registrationsRepo(chronoDb.database());
                        auto existingRegistrations = registrationsRepo.getRegistrationsByTrial(m_currentTrialId);
                        hasParticipants = existingRegistrations.has_value() && !existingRegistrations.value().isEmpty();
                    } catch (...) {
                        // In case of error, keep enabled
                    }
                }
                
                // Disable if running or if there are already participants
                action->setEnabled(!m_started && !hasParticipants);
                
                if (hasParticipants) {
                    action->setToolTip("Import desabilitado: o evento atual já possui participantes cadastrados");
                } else if (m_started) {
                    action->setToolTip("Import desabilitado: cronômetro em execução");
                } else {
                    action->setToolTip("Importar participantes de arquivo Excel");
                }
                break;
            }
        }
    }
    
    // Control "Reports" menu -> Generate Excel
    if (auto* generateExcelAction = findChild<QAction*>("actionGenerate_Excel")) {
        bool canGenerate = canGenerateReport();
        generateExcelAction->setEnabled(canGenerate);
        
        if (!canGenerate) {
            generateExcelAction->setToolTip("Select an event to generate report");
        } else {
            generateExcelAction->setToolTip("Generate Excel report for the selected event");
        }
    }
}

bool CronometerWindow::canStartEvent(const QDateTime& scheduledDateTime) {
    if (!scheduledDateTime.isValid()) {
        return false;
    }

    const QDateTime currentDateTime = QDateTime::currentDateTime();
    
    // Check if the event is today
    if (scheduledDateTime.date() != currentDateTime.date()) {
        return false;
    }
    
    // Check if less than 1 hour remains until the event starts
    const qint64 secondsToEvent = currentDateTime.secsTo(scheduledDateTime);
    
    // If the event has already passed or less than 1 hour (3600 seconds) remains, it can start
    return secondsToEvent <= 3600;
}

void CronometerWindow::updateStartButtonState() {
    // Only update if not running and a trial is selected
    if (m_started || m_currentTrialId <= 0) {
        return;
    }
    
    // Find the current trial in the loaded events
    auto it = std::ranges::find_if(m_loadedEvents,
                                   [this](const Trials::TrialInfo& trial) {
                                       return trial.id == m_currentTrialId;
                                   });
    
    if (it != m_loadedEvents.end()) {
        ui->btnStart->setEnabled(canStartEvent(it->scheduledDateTime));
    }
}

bool CronometerWindow::canGenerateReport() const {
    // Allow generating report only for events of the day or past that are finished
    if (m_currentTrialId <= 0)
        return false;

    // Search for the selected event
    auto it = std::ranges::find_if(m_loadedEvents,
                                   [this](const Trials::TrialInfo& trial) {
                                       return trial.id == m_currentTrialId;
                                   });
    if (it == m_loadedEvents.end())
        return false;

    const auto& trial = *it;
    const auto epochZero = Utils::DateTimeUtils::epochZero();

    // Only allow if:
    // - Event is in the past or today AND is finished
    // - OR is currently being timed (started == true for the selected event)
    // - Future events never enable
    if (const QDate today = QDate::currentDate(); trial.scheduledDateTime.date() > today)
        return false;

    // Allow if currently being timed
    if (m_started && trial.id == m_currentTrialId)
        return true;

    // Allow if finished
    if (trial.endDateTime != epochZero)
        return true;

    return false;
}
