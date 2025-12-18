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
    chronoDb = DBManager(dbPath);
    
    ui->setupUi(this);
    
    // Fix the window size as defined in Qt Designer
    setFixedSize(size());
    
    // Open the database
    if (!chronoDb.open()) {
        QMessageBox::critical(this, "Database Error", 
            QString("Failed to open database: %1").arg(dbPath));
    }
    
    // Initialize state
    started = false;
    startTime = Utils::DateTimeUtils::now();
    currentTrialId = -1; // Will be set when starting a trial
    
    setControlsStatus(started);
    updateCounterTimer();
    
    ui->btnRegister->setEnabled(false);

    connect(&timer, &QTimer::timeout, this, &CronometerWindow::updateCounterTimer);
    connect(ui->edtPlaque, &QLineEdit::textChanged, this, &CronometerWindow::updateRegisterButton);
    
    // Configure timer to update the state of the Start button
    connect(&startButtonUpdateTimer, &QTimer::timeout, this, &CronometerWindow::updateStartButtonState);
    startButtonUpdateTimer.start(60000); // Update every minute
    
    // Connect Exit menu directly to close()
    QAction* exitAction = findChild<QAction*>("actionExit");
    if (exitAction) {
        connect(exitAction, &QAction::triggered, this, &QWidget::close);
    }
    
    // Initialize events submenu
    eventsSubmenu = nullptr;

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

void CronometerWindow::setControlsStatus(const bool status) {
    ui->edtPlaque->setEnabled(status);
    ui->btnStart->setText(!status ? "Start" : "Stop");
}

void CronometerWindow::on_btnStart_clicked() {
    Trials::Repository trialsRepo(chronoDb.database());

    if (started) {
        auto reply = QMessageBox::question(this, "Confirmation", "You really want to stop?", QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No)
            return;
    }

    started = !started;
    
    // Update menus state
    updateMenusState();

    if (started) {
        // Check if there are existing results before starting
        Results::Repository resultsRepo(chronoDb.database());
        auto existingResultsCheck = resultsRepo.getResultsByTrial(currentTrialId);
        
        if (existingResultsCheck.has_value() && !existingResultsCheck.value().isEmpty()) {
            auto reply = QMessageBox::question(this, "Existing Results", 
                QString("There are %1 existing results for this trial.\n\nDo you want to remove all previous results and start fresh?")
                    .arg(existingResultsCheck.value().size()),
                QMessageBox::Yes | QMessageBox::No);
                
            if (reply == QMessageBox::No) {
                started = false; // Reverter estado
                updateMenusState();
                return;
            }
            
            // Delete all existing results
            const auto& existingResults = existingResultsCheck.value();
            for (const auto& result : existingResults) {
                auto deleteResult = resultsRepo.deleteResultById(result.id);
                if (!deleteResult.has_value()) {
                    QMessageBox::warning(this, "Error",
                                         QString("Failed to delete result ID %1: %2").arg(QString::number(result.id), deleteResult.error()));
                }
            }
            qDebug() << "Deleted" << existingResults.size() << "existing results for trial" << currentTrialId;
        }
        
        startTime = Utils::DateTimeUtils::now();
        startCounterTimer();
        qDebug() << "Started trial:" << selectedEventName << "with ID:" << currentTrialId;

        // Create struct with only the field we want to update
        Trials::TrialInfo updateInfo;
        updateInfo.startDateTime = startTime;
        auto startedTrial = trialsRepo.updateTrialById(currentTrialId, updateInfo);
        if (startedTrial.has_value()) {
            qDebug() << "Saved trial :" << startedTrial.value().name << " at " << startedTrial.value().startDateTime;
        } else {
            qFatal("Error saving trial: %s", startedTrial.error().toLocal8Bit().constData());
        }
    } else {
        stopCounterTimer();
        ui->btnRegister->setEnabled(false);
        qDebug() << "Finalizing trial with ID:" << currentTrialId;
        
        // Finalize trial by setting endDateTime
        QDateTime endTime = Utils::DateTimeUtils::now();
        Trials::TrialInfo updateInfo;
        updateInfo.endDateTime = endTime;
        
        auto finishedTrial = trialsRepo.updateTrialById(currentTrialId, updateInfo);
        if (finishedTrial.has_value()) {
            qDebug() << "Trial finished:" << finishedTrial.value().name 
                     << "End time:" << finishedTrial.value().endDateTime;
        } else {
            QMessageBox::warning(this, "Error", 
                QString("Error finalizing trial: %1").arg(finishedTrial.error()));
        }
    }

    setControlsStatus(started);
}

void CronometerWindow::closeEvent(QCloseEvent *event) {
    if(!started) {
        event->accept();
    } else {
        auto reply = QMessageBox::question(this, "Trial in Progress", "A trial is currently running. Do you want to stop it and exit?", QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            started = false;
            stopCounterTimer();
            event->accept();
        } else {
            event->ignore();
        }
    }
}

void CronometerWindow::startCounterTimer() {
    timer.start(1000); // update every 1s
}

void CronometerWindow::stopCounterTimer() {
    timer.stop();
}

void CronometerWindow::updateCounterTimer() {
    int secs = startTime.secsTo(Utils::DateTimeUtils::now());
    QTime display(0, 0);
    display = display.addSecs(secs);

    ui->lblTime->setText(display.toString(timeFormat));
}

void CronometerWindow::updateRegisterButton() {
    bool hasPlaque = !ui->edtPlaque->text().trimmed().isEmpty();
    ui->btnRegister->setEnabled(started && hasPlaque);
}

void CronometerWindow::on_btnRegister_clicked() {
    if (!started || currentTrialId == -1) {
        QMessageBox::warning(this, "Warning", "No active trial. Please start a trial first.");
        return;
    }
    
    QStringList placas = ui->edtPlaque->text().split(",", Qt::SkipEmptyParts);
    std::transform(placas.begin(), placas.end(), placas.begin(), [](const QString &s){ return s.trimmed(); });
    
    QDateTime curTime = Utils::DateTimeUtils::now();
    int durationMs = startTime.msecsTo(curTime);

    int registered = 0;
    int errors = 0;
    QStringList errorMessages;

    try {
        Registrations::Repository registrationsRepo(chronoDb.database());
        Results::Repository resultsRepo(chronoDb.database());

        for(const auto& placa : placas) {
            // Search for participant registration by plate number
            auto registrationResult = registrationsRepo.getRegistrationByPlateCode(currentTrialId, placa);
            
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
                startTime,
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
    
    // If at least one result was registered, generate report
    if (registered > 0) {
        generateReport();
    }
}

void CronometerWindow::generateReport() {
    if (currentTrialId == -1) {
        return;
    }
    
    // Define report file name
    QString defaultFileName = QString("report_%1_%2.xlsx")
                                .arg(selectedEventName)
                                .arg(QDate::currentDate().toString("yyyy-MM-dd"));
    
    QString defaultFilePath = QDir(reportPath).filePath(defaultFileName);
    
    QString fileName = defaultFilePath;
    
    if (fileName.isEmpty()) {
        return;
    }
    
    // Generate the report
    if (!Report::exportExcel(currentTrialId, fileName, chronoDb.database())) {
        statusBar()->showMessage("✗ Error generating Excel report", 5000);
        statusBar()->setStyleSheet("QStatusBar { background-color: #f8d7da; color: #721c24; }");
    }
}

void CronometerWindow::on_actionGenerate_Excel_triggered() {
    if (currentTrialId <= 0) {
        QMessageBox::warning(this, "No Event Selected", 
            "Select an event before generating the Excel report.");
        return;
    }
    
    // Check if the event has already ended
    try {
        Trials::Repository trialsRepo(chronoDb.database());
        auto trialResult = trialsRepo.getTrialById(currentTrialId);
        
        if (!trialResult.has_value()) {
            QMessageBox::warning(this, "Error", "Unable to load event information.");
            return;
        }
        
        const auto& trial = trialResult.value();
        
        // Check if the event has endDateTime set (event has ended)
        if (!trial.endDateTime.isValid() || trial.endDateTime == Utils::DateTimeUtils::epochZero()) {
            QMessageBox::warning(this, "Event Not Finished", 
                "The Excel report can only be generated for events that have already finished.\n\n"
                "Complete the event before generating the report.");
            return;
        }
        
        generateReport();
        
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", 
            QString("Error checking event status: %1").arg(e.what()));
    } catch (...) {
        QMessageBox::critical(this, "Error", "Unknown error checking event status.");
    }
}



void CronometerWindow::loadTodayTrial() {
    try {
        Trials::Repository trialsRepo(chronoDb.database());
        QDate today = QDate::currentDate();
        
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
        currentTrialId = selectedTrial.id;
        
        QString windowTitle = QString("%1 (Scheduled: %2)")
                                .arg(selectedTrial.name)
                                .arg(selectedTrial.scheduledDateTime.toString(eventMenuTimeFormat));
        
        setWindowTitle(windowTitle);
        
        qDebug() << "Loaded trial for today:" << selectedTrial.name 
                 << "ID:" << selectedTrial.id 
                 << "Scheduled:" << selectedTrial.scheduledDateTime;
                 
        // If there are multiple trials today, inform the user
        if (todayTrials.size() > 1) {
            QStringList trialNames;
            for (const auto& trial : todayTrials) {
                trialNames << QString("%1 (%2)")
                              .arg(trial.name)
                              .arg(trial.scheduledDateTime.toString("hh:mm"));
            }
            
            QMessageBox::information(this, "Multiple Trials Today", 
                QString("Multiple trials scheduled for today:\n\n%1\n\nSelected: %2")
                    .arg(trialNames.join("\n"))
                    .arg(selectedTrial.name));
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
    QString relativePath = settings.value("Path", "chronometer.db").toString();
    settings.setValue("Path", relativePath);
    settings.endGroup();
    
    // Build full path using the project root
    if (QDir::isRelativePath(relativePath)) {
        dbPath = projectRoot + "/" + relativePath;
    } else {
        dbPath = relativePath; // If absolute, use as is
    }
    
    // Load report settings
    settings.beginGroup("Reports");
    QString relativeReportPath = settings.value("OutputPath", "reports").toString();
    settings.setValue("OutputPath", relativeReportPath);
    settings.endGroup();
    
    // Build full path for reports
    if (QDir::isRelativePath(relativeReportPath)) {
        reportPath = projectRoot + "/" + relativeReportPath;
    } else {
        reportPath = relativeReportPath; // If absolute, use as is
    }
    
    // Create reports directory if it doesn't exist
    QDir reportDir(reportPath);
    if (!reportDir.exists()) {
        reportDir.mkpath(".");
        qDebug() << "Created reports directory:" << reportPath;
    }

    // read OpenTrialWindowDays
    settings.beginGroup("General");
    // Other settings can be loaded here in the future
    openTrialWindowDays = settings.value("OpenTrialWindowDays", "2").toInt();
    settings.endGroup();

    
    // Log loaded configuration
    qDebug() << "Project root detected:" << projectRoot;
    qDebug() << "Database path resolved to:" << dbPath;
    qDebug() << "Reports path resolved to:" << reportPath;
}

void CronometerWindow::loadEventsToMenu() {
    try {
        Trials::Repository trialsRepo(chronoDb.database());
        
        auto trialsResult = trialsRepo.getAllTrials();
        
        if (!trialsResult.has_value()) {
            qWarning() << "Error loading trials for menu:" << trialsResult.error();
            return;
        }
        
        loadedEvents = trialsResult.value();
        
        // Criar ou limpar submenu
        if (eventsSubmenu) {
            eventsSubmenu->clear();
        } else {
            eventsSubmenu = new QMenu("Select Event", this);
            
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
                eventMenu->addMenu(eventsSubmenu);
            }
        }
        
        // Load previously selected event from settings
        QSettings settings("settings.ini", QSettings::IniFormat);
        settings.beginGroup("General");
        QString previouslySelected = settings.value("SelectedEvent", "").toString();
        int previouslySelectedId = settings.value("SelectedEventId", -1).toInt();
        settings.endGroup();
        
        // Add events to submenu
        ui->btnStart->setEnabled(false);
        if (loadedEvents.isEmpty()) {
            QAction* noEventsAction = eventsSubmenu->addAction("No events available");
            noEventsAction->setEnabled(false);
        } else {
            for (const auto& trial : loadedEvents) {
                QString actionText = QString("%1").arg(trial.name);
                if (trial.scheduledDateTime.isValid()) {
                    actionText += QString(" (%1)").arg(trial.scheduledDateTime.toString(eventMenuTimeFormat));
                }
                
                QAction* eventAction = eventsSubmenu->addAction(actionText);
                eventAction->setData(trial.id); // Armazenar ID do trial
                eventAction->setCheckable(true);
                
                // Restaurar seleção anterior se existir
                if (trial.id == previouslySelectedId && !previouslySelected.isEmpty()) {
                    eventAction->setChecked(true);
                    currentTrialId = trial.id;
                    selectedEventName = trial.name;
                    
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
        
        qDebug() << "Loaded" << loadedEvents.size() << "events to submenu";
        
        // Update menus state after loading events
        updateMenusState();
        
    } catch (const std::exception& e) {
        qWarning() << "Exception loading events to menu:" << e.what();
    } catch (...) {
        qWarning() << "Unknown exception loading events to menu";
    }
}

void CronometerWindow::onEventSelected() {
    QAction* senderAction = qobject_cast<QAction*>(sender());
    if (!senderAction) return;
    
    // Do not allow changing trial if the timer is running
    if (started) {
        QMessageBox::warning(this, "Trial Running", 
            "You cannot change the event while the timer is running.\n\nStop the timer before selecting another event.");
        return;
    }
    
    int selectedTrialId = senderAction->data().toInt();
    
    // Find the selected trial
    auto it = std::find_if(loadedEvents.begin(), loadedEvents.end(),
                          [selectedTrialId](const Trials::TrialInfo& trial) {
                              return trial.id == selectedTrialId;
                          });
    
    if (it != loadedEvents.end()) {
        const auto& selectedTrial = *it;
        currentTrialId = selectedTrialId;
        selectedEventName = selectedTrial.name; // Save event name
        
        // Uncheck all actions in the submenu
        if (eventsSubmenu) {
            for (QAction* action : eventsSubmenu->actions()) {
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
        qDebug() << "Selected event name saved:" << selectedEventName;

        // Update initial state of the Start button
        updateStartButtonState();
        
        // Save the selection in settings to persist between sessions
        QSettings settings("settings.ini", QSettings::IniFormat);
        settings.beginGroup("General");
        settings.setValue("SelectedEvent", selectedEventName);
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
        auto existingTrialsResult = trialsRepo.getAllTrials();
        if (existingTrialsResult.has_value()) {
            QStringList existingEventNames;
            for (const auto& trial : existingTrialsResult.value()) {
                if (!trial.name.isEmpty()) {
                    existingEventNames.append(trial.name);
                }
            }
            newEventDialog.setExistingEventNames(existingEventNames);
        }

        auto status = newEventDialog.exec();
        if (status == QDialog::Accepted) {
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
            if (eventId != -1 && !started) {
                selectEventById(eventId);
            } else if (started) {
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
        if (!eventsSubmenu) {
            qWarning() << "Events submenu not initialized";
            return;
        }
        
        // Uncheck all actions first
        for (QAction* action : eventsSubmenu->actions()) {
            action->setChecked(false);
        }
        
        // Find and select the action corresponding to eventId
        for (QAction* action : eventsSubmenu->actions()) {
            if (action->data().toInt() == eventId) {
                action->setChecked(true);
                
                // Find the corresponding trial to update the window title
                auto it = std::find_if(loadedEvents.begin(), loadedEvents.end(),
                                      [eventId](const Trials::TrialInfo& trial) {
                                          return trial.id == eventId;
                                      });
                
                if (it != loadedEvents.end()) {
                    currentTrialId = it->id;
                    selectedEventName = it->name;
                    
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
    if (currentTrialId <= 0) {
        // Try to load today's event if none is selected
        loadTodayTrial();
        
        if (currentTrialId <= 0) {
            QMessageBox::StandardButton reply = QMessageBox::question(this, "No Active Event", 

                "No event is selected. Do you want to see participants of a specific event?",
                QMessageBox::Yes | QMessageBox::No);
                
            if (reply == QMessageBox::Yes) {
                // Show list of events to choose from
                showEventSelectionDialog();
                if (currentTrialId <= 0) {
                    return; // User canceled or did not select anything
                }
            } else {
                return;
            }
        }
    }
    
    try {
        Trials::Repository trialsRepo(chronoDb.database());
        auto trialResult = trialsRepo.getTrialById(currentTrialId);
        
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
        if (started) {
            QMessageBox::warning(this, "Timer Running", 
                "It is not possible to import participants while the timer is running.\n\nStop the timer before importing participants.");
            return;
        }
        
        // Check if there is a selected trial and if it already has participants
        if (currentTrialId > 0) {
            Registrations::Repository registrationsRepo(chronoDb.database());
            auto existingRegistrations = registrationsRepo.getRegistrationsByTrial(currentTrialId);
            
            if (existingRegistrations.has_value() && !existingRegistrations.value().isEmpty()) {
                auto reply = QMessageBox::question(this, "Existing Participants", 
                    QString("The current event already has %1 participant(s) registered.\n\nImporting new participants may cause conflicts.\n\nDo you want to continue anyway?")
                    .arg(existingRegistrations.value().size()),
                    QMessageBox::Yes | QMessageBox::No);
                    
                if (reply == QMessageBox::No) {
                    return;
                }
            }
        }
        
        Trials::Repository trialsRepo(chronoDb.database());
        auto trialsResult = trialsRepo.getAllTrials();
        
        if (!trialsResult.has_value()) {
            QMessageBox::warning(this, "Error", "Error loading events: " + trialsResult.error());
            return;
        }
        
        LoadParticipantsWindow loadDialog(chronoDb, this);
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
        Trials::Repository trialsRepo(chronoDb.database());
        auto trialsResult = trialsRepo.getAllTrials();
        
        if (!trialsResult.has_value() || trialsResult.value().isEmpty()) {
            QMessageBox::information(this, "Information", "No events registered in the system.");
            return;
        }
        
        QStringList eventNames;
        QVector<int> eventIds;
        
        for (const auto& trial : trialsResult.value()) {
            QString displayText = QString("%1 - %2")
                                .arg(trial.name)
                                .arg(trial.scheduledDateTime.toString("dd/MM/yyyy hh:mm"));
            eventNames.append(displayText);
            eventIds.append(trial.id);
        }
        
        bool ok;
        QString selectedEvent = QInputDialog::getItem(this, 
            "Select Event", 
            "Choose the event to view participants:",
            eventNames, 0, false, &ok);
            
        if (ok && !selectedEvent.isEmpty()) {
            int selectedIndex = eventNames.indexOf(selectedEvent);
            if (selectedIndex >= 0) {
                currentTrialId = eventIds[selectedIndex];
                
                // Update window title
                const auto& selectedTrial = trialsResult.value()[selectedIndex];
                QString windowTitle = QString("%1 (Scheduled: %2)")
                                    .arg(selectedTrial.name)
                                    .arg(selectedTrial.scheduledDateTime.toString(eventMenuTimeFormat));
                setWindowTitle(windowTitle);
            }
        }
        
    } catch (const std::exception& e) {
        qDebug() << "Error in showEventSelectionDialog:" << e.what();
        QMessageBox::critical(this, "Error", "Error loading events list.");
    }
}

void CronometerWindow::closeOpenedEvents() {
    try {
        Trials::Repository trialRepository(chronoDb.database());

        auto trials = trialRepository.getAllTrials();
        if (!trials) {
            qDebug() << "Error fetching all trials";
            return;
        }

        auto limitDate = QDate::currentDate().addDays(-openTrialWindowDays);

        const auto epochZero = Utils::DateTimeUtils::epochZero();
        const auto now = Utils::DateTimeUtils::now();
        
        // Filter trials that need to be closed and process each one
        std::for_each(trials.value().begin(), trials.value().end(), 
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
        Trials::Repository trialsRepo(chronoDb.database());
        
        auto runningTrialResult = trialsRepo.getRunningTrial(openTrialWindowDays);
        
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
        currentTrialId = runningTrial.id;
        selectedEventName = runningTrial.name;
        
        // Set the chronometer with the trial's startDateTime
        startTime = runningTrial.startDateTime;
        started = true;
        
        // Update interface
        setControlsStatus(started);
        ui->btnStart->setEnabled(true);  // Enable button to allow finishing the trial
        startCounterTimer();
        
        QString windowTitle = QString("%1 (RUNNING)")
                                .arg(runningTrial.name);
        setWindowTitle(windowTitle);
        
        // Mark event as selected in the menu
        if (eventsSubmenu) {
            for (QAction* action : eventsSubmenu->actions()) {
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
        QString message = QString("Chronometer automatically resumed for trial: %1\nStarted at: %2")
                           .arg(runningTrial.name)
                           .arg(runningTrial.startDateTime.toString("dd/MM/yyyy hh:mm:ss"));
        
        QMessageBox::information(this, "Trial Resumed", message);
        
        // Update menu states after automatically resuming trial
        updateMenusState();
        
    } catch (const std::exception& e) {
        qWarning() << "Exception checking running trial:" << e.what();
    } catch (...) {
        qWarning() << "Unknown exception checking running trial";
    }
}

void CronometerWindow::updateMenusState() {
    // Disable event selection if the chronometer is running
    if (eventsSubmenu) {
        eventsSubmenu->setEnabled(!started);
    }
    
    // Find the "Event" menu and disable "Load from file" if necessary
    QMenu* eventMenu = nullptr;
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
                if (currentTrialId > 0) {
                    try {
                        Registrations::Repository registrationsRepo(chronoDb.database());
                        auto existingRegistrations = registrationsRepo.getRegistrationsByTrial(currentTrialId);
                        hasParticipants = existingRegistrations.has_value() && !existingRegistrations.value().isEmpty();
                    } catch (...) {
                        // In case of error, keep enabled
                    }
                }
                
                // Disable if running or if there are already participants
                action->setEnabled(!started && !hasParticipants);
                
                if (hasParticipants) {
                    action->setToolTip("Import desabilitado: o evento atual já possui participantes cadastrados");
                } else if (started) {
                    action->setToolTip("Import desabilitado: cronômetro em execução");
                } else {
                    action->setToolTip("Importar participantes de arquivo Excel");
                }
                break;
            }
        }
    }
    
    // Control "Reports" menu -> Generate Excel
    QAction* generateExcelAction = findChild<QAction*>("actionGenerate_Excel");
    if (generateExcelAction) {
        bool canGenerate = canGenerateReport();
        generateExcelAction->setEnabled(canGenerate);
        
        if (!canGenerate) {
            if (currentTrialId <= 0) {
                generateExcelAction->setToolTip("Select an event to generate report");
            } else {
                generateExcelAction->setToolTip("Report can only be generated after the event is finished");
            }
        } else {
            generateExcelAction->setToolTip("Generate Excel report for the finished event");
        }
    }
}

bool CronometerWindow::canStartEvent(const QDateTime& scheduledDateTime) const {
    if (!scheduledDateTime.isValid()) {
        return false;
    }
    
    QDateTime currentDateTime = QDateTime::currentDateTime();
    
    // Check if the event is today
    if (scheduledDateTime.date() != currentDateTime.date()) {
        return false;
    }
    
    // Check if less than 1 hour remains until the event starts
    qint64 secondsToEvent = currentDateTime.secsTo(scheduledDateTime);
    
    // If the event has already passed or less than 1 hour (3600 seconds) remains, it can start
    return secondsToEvent <= 3600;
}

void CronometerWindow::updateStartButtonState() {
    // Only update if not running and a trial is selected
    if (started || currentTrialId <= 0) {
        return;
    }
    
    // Find the current trial in the loaded events
    auto it = std::find_if(loadedEvents.begin(), loadedEvents.end(),
                          [this](const Trials::TrialInfo& trial) {
                              return trial.id == currentTrialId;
                          });
    
    if (it != loadedEvents.end()) {
        ui->btnStart->setEnabled(canStartEvent(it->scheduledDateTime));
    }
}

bool CronometerWindow::canGenerateReport() const {
    if (currentTrialId <= 0) {
        return false;
    }
    
    try {
        Trials::Repository trialsRepo(chronoDb.database());
        auto trialResult = trialsRepo.getTrialById(currentTrialId);
        
        if (!trialResult.has_value()) {
            return false;
        }
        
        const auto& trial = trialResult.value();
        
        // Check if the event has endDateTime defined (event has finished)
        return trial.endDateTime.isValid() && trial.endDateTime != Utils::DateTimeUtils::epochZero();
        
    } catch (...) {
        return false;
    }
}
