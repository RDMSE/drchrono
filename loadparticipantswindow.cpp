#include "loadparticipantswindow.h"
#include "repository/registrations/registrationsrepository.h"
#include "repository/athletes/athletesrepository.h"
#include "repository/categories/categoriesrepository.h"
#include "repository/modalities/modalitiesrepository.h"
#include "utils/excelutils.h"
#include <QDebug>
#include <QDateTime>
#include <QApplication>
#include <QFileInfo>
#include <QSettings>
#include <QFile>
#include <QCoreApplication>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QWidget>
#include <QMessageBox>

LoadParticipantsWindow::LoadParticipantsWindow(DBManager& dbManager, QWidget *parent)
    : QDialog(parent)
    , m_dbManager(dbManager)
    , mainLayout(nullptr)
    , formLayout(nullptr)
    , fileLayout(nullptr)
    , buttonLayout(nullptr)
    , trialCombo(nullptr)
    , filePathEdit(nullptr)
    , selectFileButton(nullptr)
    , previewButton(nullptr)
    , importButton(nullptr)
    , cancelButton(nullptr)
    , previewTable(nullptr)
    , progressBar(nullptr)
    , statusLabel(nullptr)
    , selectedTrialId(-1)
    , activeTrialId(-1)
{
    setupUI();
    loadBasicData();
}

LoadParticipantsWindow::~LoadParticipantsWindow()
{
}

void LoadParticipantsWindow::setupUI()
{
    setWindowTitle("Load Participants from Excel");
    setModal(true);
    resize(900, 700);

    mainLayout = new QVBoxLayout(this);
    
    // Form layout for inputs
    formLayout = new QFormLayout();
    
    // Trial selection
    trialCombo = new QComboBox();
    trialCombo->setMinimumWidth(300);
    connect(trialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &LoadParticipantsWindow::onTrialSelected);
    formLayout->addRow("Event:", trialCombo);
    
    // File selection
    fileLayout = new QHBoxLayout();
    filePathEdit = new QLineEdit();
    filePathEdit->setPlaceholderText("Select an Excel file (.xlsx)");
    filePathEdit->setReadOnly(true);
    
    selectFileButton = new QPushButton("Select File");
    selectFileButton->setEnabled(false); // Initially disabled until event is selected
    connect(selectFileButton, &QPushButton::clicked, this, &LoadParticipantsWindow::selectFile);
    
    fileLayout->addWidget(filePathEdit);
    fileLayout->addWidget(selectFileButton);
    formLayout->addRow("Excel File:", fileLayout);
    
    mainLayout->addLayout(formLayout);
    
    // Preview button
    previewButton = new QPushButton("Preview Data");
    previewButton->setEnabled(false);
    connect(previewButton, &QPushButton::clicked, this, &LoadParticipantsWindow::loadPreview);
    mainLayout->addWidget(previewButton);
    
    // Preview table
    previewTable = new QTableWidget();
    previewTable->setColumnCount(5);
    QStringList headers = {"Name", "Plate Code", "Category", "Modality", "Status"};
    previewTable->setHorizontalHeaderLabels(headers);
    previewTable->setAlternatingRowColors(true);
    previewTable->horizontalHeader()->setStretchLastSection(true);
    previewTable->horizontalHeader()->resizeSection(0, 200);
    previewTable->horizontalHeader()->resizeSection(1, 120);
    previewTable->horizontalHeader()->resizeSection(2, 120);
    previewTable->horizontalHeader()->resizeSection(3, 120);
    mainLayout->addWidget(previewTable);
    
    // Progress bar
    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    mainLayout->addWidget(progressBar);
    
    // Status label
    statusLabel = new QLabel("");
    statusLabel->setStyleSheet("color: #666; font-style: italic;");
    mainLayout->addWidget(statusLabel);
    
    // Buttons
    buttonLayout = new QHBoxLayout();
    
    importButton = new QPushButton("Import Participants");
    importButton->setEnabled(false);
    importButton->setStyleSheet("font-weight: bold; background-color: #4CAF50; color: white;");
    connect(importButton, &QPushButton::clicked, this, &LoadParticipantsWindow::importParticipants);
    
    cancelButton = new QPushButton("Cancel");
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(importButton);
    buttonLayout->addWidget(cancelButton);
    
    mainLayout->addLayout(buttonLayout);
    
    setLayout(mainLayout);
}

void LoadParticipantsWindow::loadBasicData()
{
    try {
        // Load categories
        Categories::Repository categoriesRepo(m_dbManager.database());
        auto categoriesResult = categoriesRepo.getAllCategories();
        if (categoriesResult.has_value()) {
            categories = categoriesResult.value();
        }
        
        // Load modalities
        Modalities::Repository modalitiesRepo(m_dbManager.database());
        auto modalitiesResult = modalitiesRepo.getAllModalities();
        if (modalitiesResult.has_value()) {
            modalities = modalitiesResult.value();
        }
        
        // Load athletes
        Athletes::Repository athletesRepo(m_dbManager.database());
        auto athletesResult = athletesRepo.getAllAthletes();
        if (athletesResult.has_value()) {
            athletes = athletesResult.value();
        }
        
    } catch (const std::exception& e) {
        qDebug() << "Error loading basic data:" << e.what();
        QMessageBox::warning(this, "Error", "Error loading basic system data");
    }
}

void LoadParticipantsWindow::setAvailableTrials(const QVector<Trials::TrialInfo>& trials)
{
    availableTrials.clear();
    trialCombo->clear();
    
    QDateTime now = Utils::DateTimeUtils::now();
    
    // Add placeholder
    trialCombo->addItem("Select an event...", -1);
    
    int activeTrialIndex = -1;
    
    // Filter trials - only present and future events
    for (const auto& trial : trials) {
        if (validateTrialForImport(trial)) {
            availableTrials.append(trial);
            QString displayText = QString("%1 - %2")
                                .arg(trial.name)
                                .arg(trial.scheduledDateTime.toString("dd/MM/yyyy hh:mm"));
            trialCombo->addItem(displayText, trial.id);
            
            // Check if this is the active trial
            if (trial.id == activeTrialId) {
                activeTrialIndex = trialCombo->count() - 1; // Last added item
            }
        }
    }
    
    // Auto-select the active trial if found
    if (activeTrialIndex >= 0) {
        trialCombo->setCurrentIndex(activeTrialIndex);
        selectFileButton->setEnabled(true);
        statusLabel->setText("Active event auto-selected. You can now choose the Excel file.");
        statusLabel->setStyleSheet("color: #4CAF50;"); // Green for success
    } else if (availableTrials.isEmpty()) {
        statusLabel->setText("No future events available for import.");
        statusLabel->setStyleSheet("color: #f44336;");
        trialCombo->setEnabled(false);
        selectFileButton->setEnabled(false);
    } else {
        statusLabel->setText(QString("Found %1 available events. Please select an event first.").arg(availableTrials.size()));
        statusLabel->setStyleSheet("color: #FF9800;"); // Orange to indicate action needed
        trialCombo->setEnabled(true);
        selectFileButton->setEnabled(false); // Keep disabled until event is selected
    }
}

bool LoadParticipantsWindow::validateTrialForImport(const Trials::TrialInfo& trial)
{
    QDateTime now = Utils::DateTimeUtils::now();
    
    // Only allow import for events scheduled for today or future
    return trial.scheduledDateTime >= now.date().startOfDay();
}

void LoadParticipantsWindow::selectFile()
{
    // Validate that an event is selected first
    if (trialCombo->currentData().toInt() <= 0) {
        QMessageBox::warning(this, "Error", "Please select an event before choosing the Excel file.");
        return;
    }
    
    // Get settings file path
    QString settingsPath = "settings.ini";
    if (!QFile::exists(settingsPath)) {
        settingsPath = QCoreApplication::applicationDirPath() + "/settings.ini";
    }
    
    // Get last used path from settings, fallback to Documents
    QSettings settings(settingsPath, QSettings::IniFormat);
    QString lastPath = settings.value("LoadParticipants/LastFilePath", 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Select Excel file",
        lastPath,
        "Excel Files (*.xlsx *.xls);;All Files (*)"
    );
    
    if (!fileName.isEmpty()) {
        selectedFilePath = fileName;
        filePathEdit->setText(fileName);

        // Save last used path
        QSettings saveSettings(settingsPath, QSettings::IniFormat);
        saveSettings.setValue("LoadParticipants/LastFilePath", QFileInfo(fileName).absolutePath());

        // Enable preview button if trial is also selected
        previewButton->setEnabled(trialCombo->currentData().toInt() > 0);
        
        statusLabel->setText("File selected. Click 'Preview Data' to continue.");
        statusLabel->setStyleSheet("color: #2196F3;");
    }
}

void LoadParticipantsWindow::loadPreview()
{
    if (selectedFilePath.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please select an Excel file first.");
        return;
    }
    
    if (trialCombo->currentData().toInt() <= 0) {
        QMessageBox::warning(this, "Error", "Please select an event first.");
        return;
    }
    
    setControlsEnabled(false);
    statusLabel->setText("Loading Excel file...");
    statusLabel->setStyleSheet("color: #FF9800;");
    
    QApplication::processEvents();
    
    if (loadExcelFile(selectedFilePath)) {
        validateParticipantsData();
        updatePreviewTable();
        
        // Check if there are valid participants and duplicates
        int validCount = 0;
        int duplicateCount = 0;
        int totalErrors = 0;
        
        for (const auto& participant : participantsData) {
            if (participant.isValid()) {
                validCount++;
            } else {
                totalErrors++;
                if (participant.errorMessage.contains("duplicate plate", Qt::CaseInsensitive)) {
                    duplicateCount++;
                }
            }
        }
        
        if (validCount > 0 && duplicateCount == 0) {
            importButton->setEnabled(true);
            statusLabel->setText(QString("File loaded! %1 valid participants found.")
                               .arg(validCount));
            statusLabel->setStyleSheet("color: #4CAF50;");
        } else if (duplicateCount > 0) {
            importButton->setEnabled(false);
            statusLabel->setText(QString("ERROR: %1 duplicate plate codes detected! Please correct the file before importing.")
                               .arg(duplicateCount));
            statusLabel->setStyleSheet("color: #f44336; font-weight: bold;");
        } else {
            importButton->setEnabled(false);
            statusLabel->setText(QString("No valid participants found. %1 errors detected.")
                               .arg(totalErrors));
            statusLabel->setStyleSheet("color: #f44336;");
        }
    } else {
        statusLabel->setText("Error loading Excel file.");
        statusLabel->setStyleSheet("color: #f44336;");
    }
    
    setControlsEnabled(true);
}

bool LoadParticipantsWindow::loadExcelFile(const QString& filePath)
{
    try {
        participantsData.clear();
        
        // Use ExcelUtils to read the file
        auto excelData = Utils::ExcelUtils::readExcelFile(filePath);
        
        if (!excelData.has_value()) {
            QMessageBox::warning(this, "Error", "Error reading Excel file: " + excelData.error());
            return false;
        }
        
        const auto& rows = excelData.value();
        
        // Skip header row (assume first row is header)
        for (int i = 1; i < rows.size(); ++i) {
            const auto& row = rows[i];
            
            if (row.size() < 4) {
                continue; // Skip incomplete rows
            }
            
            ParticipantData participant{
                .name      = row[1].toString().trimmed(), // Coluna B - Nome
                .plateCode = row[0].toString().trimmed(), // Coluna A - Código da Placa
                .category  = row[3].toString().trimmed(), // Coluna D - Categoria
                .modality  = row[2].toString().trimmed(), // Coluna C - Modalidade
            };
            
            if (!participant.name.isEmpty() && !participant.plateCode.isEmpty()) {
                participantsData.append(participant);
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        qDebug() << "Error loading Excel file:" << e.what();
        QMessageBox::warning(this, "Error", "Error processing Excel file");
        return false;
    }
}

void LoadParticipantsWindow::validateParticipantsData()
{
    // First pass: basic validation
    for (auto& participant : participantsData) {
        participant.errorMessage = "";
        
        // Validate name
        if (participant.name.isEmpty()) {
            participant.errorMessage = "Name is required";
            continue;
        }
        
        // Validate plate code
        if (participant.plateCode.isEmpty()) {
            participant.errorMessage = "Plate code is required";
            continue;
        }
        
        // Validate category
        if (participant.category.isEmpty()) {
            participant.errorMessage = "Category is required";
            continue;
        }
        
        // Validate modality
        if (participant.modality.isEmpty()) {
            participant.errorMessage = "Modality is required";
            continue;
        }
    }
    
    // Second pass: check for duplicate plate codes
    QMap<QString, QVector<int>> plateCodeMap; // plateCode -> list of indices
    
    for (int i = 0; i < participantsData.size(); ++i) {
        const auto& participant = participantsData[i];
        if (!participant.plateCode.isEmpty()) {
            plateCodeMap[participant.plateCode.toUpper()].append(i);
        }
    }
    
    // Mark duplicates as invalid
    for (auto it = plateCodeMap.begin(); it != plateCodeMap.end(); ++it) {
        const QString& plateCode = it.key();
        const QVector<int>& indices = it.value();
        
        if (indices.size() > 1) {
            // Mark all occurrences as invalid
            for (int index : indices) {
                participantsData[index].errorMessage = QString("Duplicate plate (%1 occurrences)").arg(indices.size());
            }
            qDebug() << QString("Found duplicate plate code '%1' in %2 rows").arg(plateCode, QString::number(indices.size()));
        }
    }
    
    // Detect conflicts with existing registrations
    detectConflicts();
}

void LoadParticipantsWindow::updatePreviewTable()
{
    previewTable->setRowCount(participantsData.size());
    
    for (int row = 0; row < participantsData.size(); ++row) {
        const auto& participant = participantsData[row];
        
        previewTable->setItem(row, 0, new QTableWidgetItem(participant.name));
        previewTable->setItem(row, 1, new QTableWidgetItem(participant.plateCode));
        previewTable->setItem(row, 2, new QTableWidgetItem(participant.category));
        previewTable->setItem(row, 3, new QTableWidgetItem(participant.modality));
        
        QString status = participant.isValid() ? "Valid" : participant.errorMessage;
        QTableWidgetItem* statusItem = new QTableWidgetItem(status);
        
        // Color coding based on validation
        QColor rowColor;
        if (participant.isValid()) {
            rowColor = QColor(200, 255, 200); // Light green
        } else if (participant.errorMessage.contains("duplicate plate", Qt::CaseInsensitive)) {
            rowColor = QColor(255, 150, 150); // Darker red for duplicates
        } else {
            rowColor = QColor(255, 200, 200); // Light red for other errors
        }
        
        // Apply color to entire row for better visibility
        for (int col = 0; col < 5; ++col) {
            if (previewTable->item(row, col)) {
                previewTable->item(row, col)->setBackground(rowColor);
            }
        }
        
        statusItem->setBackground(rowColor);
        previewTable->setItem(row, 4, statusItem);
        
        // Make items read-only
        for (int col = 0; col < 5; ++col) {
            if (previewTable->item(row, col)) {
                previewTable->item(row, col)->setFlags(previewTable->item(row, col)->flags() & ~Qt::ItemIsEditable);
            }
        }
    }
}

void LoadParticipantsWindow::importParticipants()
{
    // Double check by getting current selection again
    int currentIndex = trialCombo->currentIndex();
    int refreshedTrialId = -1;
    if (currentIndex > 0 && currentIndex <= availableTrials.size()) {
        refreshedTrialId = availableTrials[currentIndex - 1].id;
    }
    
    int trialId = selectedTrialId;
    qDebug() << "[LoadParticipants] === IMPORT DEBUG ===";
    qDebug() << "[LoadParticipants] Stored trial ID:" << trialId;
    qDebug() << "[LoadParticipants] Refreshed trial ID:" << refreshedTrialId;
    qDebug() << "[LoadParticipants] ComboBox index:" << currentIndex;
    qDebug() << "[LoadParticipants] ComboBox text:" << trialCombo->currentText();
    qDebug() << "[LoadParticipants] ComboBox data:" << trialCombo->currentData().toInt();
    qDebug() << "[LoadParticipants] Available trials count:" << availableTrials.size();
    
    // Use refreshed value to be absolutely sure
    if (refreshedTrialId > 0) {
        trialId = refreshedTrialId;
    }
    
    if (trialId <= 0) {
        QMessageBox::warning(this, "Error", "Please select a valid event.");
        return;
    }
    
    // Count valid participants
    int validCount = 0;
    for (const auto& participant : participantsData) {
        if (participant.isValid()) {
            validCount++;
        }
    }
    
    if (validCount == 0) {
        QMessageBox::information(this, "Information", "There are no valid participants to import.");
        return;
    }
    
    // Confirm import
    auto reply = QMessageBox::question(this, "Confirm Import",
                                     QString("Do you want to import %1 participants to the selected event?")
                                     .arg(validCount),
                                     QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    setControlsEnabled(false);
    progressBar->setVisible(true);
    progressBar->setRange(0, validCount);
    progressBar->setValue(0);
    
    int imported = 0;
    int errors = 0;
    
    try {
        Athletes::Repository athletesRepo(m_dbManager.database());
        Categories::Repository categoriesRepo(m_dbManager.database());
        Modalities::Repository modalitiesRepo(m_dbManager.database());
        Registrations::Repository registrationsRepo(m_dbManager.database());
        
        for (int i = 0; i < participantsData.size(); ++i) {
            const auto& participant = participantsData[i];
            
            if (!participant.isValid()) {
                continue;
            }
            
            statusLabel->setText(QString("Importing: %1").arg(participant.name));
            QApplication::processEvents();
            
            try {
                // Check if this participant has a conflict resolution
                ConflictData* conflictInfo = nullptr;
                QString participantName = participant.name.trimmed().toLower();
                
                for (auto& conflict : conflictsData) {
                    if (conflict.dbName.trimmed().toLower() == participantName) {
                        conflictInfo = &conflict;
                        break;
                    }
                }
                
                // If conflict exists and user chose to keep database version, skip import
                if (conflictInfo && conflictInfo->resolved && !conflictInfo->useExcelVersion) {
                    qDebug() << QString("[LoadParticipants] Skipping %1 - keeping database version").arg(participant.name);
                    imported++; // Count as "imported" (no change needed)
                    continue;
                }
                
                // Get or create athlete
                int athleteId = -1;
                if (!createAthleteIfNotExists(participant.name, athleteId)) {
                    qDebug() << "Error creating athlete " << participant.name;
                    errors++;
                    continue;
                }
                
                // Get or create category
                int categoryId = findOrCreateCategory(participant.category);
                if (categoryId <= 0) {
                    qDebug() << "Error creating category " << participant.category;
                    errors++;
                    continue;
                }
                
                // Get or create modality
                int modalityId = findOrCreateModality(participant.modality);
                if (modalityId <= 0) {
                    qDebug() << "Error creating modality " << participant.modality;
                    errors++;
                    continue;
                }
                
                // Check if this is a conflict that needs updating
                if (conflictInfo && conflictInfo->resolved && conflictInfo->useExcelVersion) {
                    // Update existing registration
                    qDebug() << QString("[LoadParticipants] Updating registration for %1 with Excel version").arg(participant.name);
                    
                    // Find existing registration by athlete and trial
                    auto existingResult = registrationsRepo.getRegistrationsByTrial(trialId);
                    if (existingResult.has_value()) {
                        for (const auto& existing : existingResult.value()) {
                            if (existing.athleteId == athleteId) {
                                // Create updated registration object
                                Registrations::Registration updatedReg = existing;
                                updatedReg.plateCode = participant.plateCode;
                                updatedReg.modalityId = modalityId;
                                updatedReg.categoryId = categoryId;
                                
                                // Update the registration
                                auto updateResult = registrationsRepo.updateRegistrationById(existing.id, updatedReg);
                                
                                if (updateResult.has_value()) {
                                    imported++;
                                    qDebug() << QString("[LoadParticipants] Successfully updated registration ID %1").arg(existing.id);
                                } else {
                                    qDebug() << "[LoadParticipants] Error updating registration:" << updateResult.error();
                                    errors++;
                                }
                                break;
                            }
                        }
                    }
                } else {
                    // Create new registration (no conflict or new participant)
                    qDebug() << QString("[LoadParticipants] Creating new registration: trialId=%1, athleteId=%2, plateCode=%3, modalityId=%4, categoryId=%5")
                               .arg(trialId).arg(athleteId).arg(participant.plateCode).arg(modalityId).arg(categoryId);
                               
                    auto registrationResult = registrationsRepo.createRegistration(
                        trialId,
                        athleteId,
                        participant.plateCode,
                        modalityId,
                        categoryId
                    );
                    
                    if (registrationResult.has_value()) {
                        imported++;
                        qDebug() << QString("[LoadParticipants] Successfully created registration ID %1").arg(registrationResult.value().id);
                    } else {
                        qDebug() << "[LoadParticipants] Error creating registration:" << registrationResult.error();
                        errors++;
                    }
                }
                
            } catch (const std::exception& e) {
                qDebug() << "Error processing participant:" << participant.name << e.what();
                errors++;
            }
            
            progressBar->setValue(progressBar->value() + 1);
        }
        
    } catch (const std::exception& e) {
        qDebug() << "General import error:" << e.what();
        QMessageBox::critical(this, "Error", "Error during import: " + QString(e.what()));
    }
    
    progressBar->setVisible(false);
    setControlsEnabled(true);
    
    // Show results
    QString resultMessage = QString("Import completed!\nImported: %1\nErrors: %2")
                          .arg(imported)
                          .arg(errors);
    
    if (errors > 0) {
        QMessageBox::warning(this, "Import Completed", resultMessage);
    } else {
        QMessageBox::information(this, "Import Completed", resultMessage);
    }
    
    if (imported > 0) {
        accept(); // Close dialog
    }
}

bool LoadParticipantsWindow::createAthleteIfNotExists(const QString& name, int& athleteId)
{
    try {
        Athletes::Repository athletesRepo(m_dbManager.database());
        
        // Check if athlete already exists
        auto existingResult = athletesRepo.getAthleteByName(name);
        if (existingResult.has_value() && !existingResult.value().name.isEmpty()) {
            athleteId = existingResult.value().id;
            return true;
        }
        
        // Create new athlete
        auto createResult = athletesRepo.createAthlete(name);
        if (createResult.has_value()) {
            athleteId = createResult.value().id;
            return true;
        }
        
        qDebug() << "Error creating athlete:" << createResult.error();
        return false;
        
    } catch (const std::exception& e) {
        qDebug() << "Error in createAthleteIfNotExists:" << e.what();
        return false;
    }
}

int LoadParticipantsWindow::findOrCreateCategory(const QString& categoryName)
{
    try {
        Categories::Repository categoriesRepo(m_dbManager.database());
        
        // Check existing categories
        for (const auto& category : categories) {
            if (category.name.compare(categoryName, Qt::CaseInsensitive) == 0) {
                return category.id;
            }
        }
        
        // Create new category
        auto createResult = categoriesRepo.createCategory(categoryName);
        if (createResult.has_value()) {
            // Update local cache
            categories.append(createResult.value());
            return createResult.value().id;
        }
        
        qDebug() << "Error creating category:" << createResult.error();
        return -1;
        
    } catch (const std::exception& e) {
        qDebug() << "Error in findOrCreateCategory:" << e.what();
        return -1;
    }
}

int LoadParticipantsWindow::findOrCreateModality(const QString& modalityName)
{
    try {
        Modalities::Repository modalitiesRepo(m_dbManager.database());
        
        // Check existing modalities
        for (const auto& modality : modalities) {
            if (modality.name.compare(modalityName, Qt::CaseInsensitive) == 0) {
                return modality.id;
            }
        }
        
        // Create new modality
        auto createResult = modalitiesRepo.createModality(modalityName);
        if (createResult.has_value()) {
            // Update local cache
            modalities.append(createResult.value());
            return createResult.value().id;
        }
        
        qDebug() << "Error creating modality:" << createResult.error();
        return -1;
        
    } catch (const std::exception& e) {
        qDebug() << "Error in findOrCreateModality:" << e.what();
        return -1;
    }
}

void LoadParticipantsWindow::onTrialSelected()
{
    int currentIndex = trialCombo->currentIndex();
    
    // Index 0 is the placeholder "Select an event...", so actual trials start at index 1
    if (currentIndex > 0 && currentIndex <= availableTrials.size()) {
        selectedTrialId = availableTrials[currentIndex - 1].id;
    } else {
        selectedTrialId = -1;
    }
    
    bool eventSelected = selectedTrialId > 0;
    
    qDebug() << "[LoadParticipants] Trial selected - Index:" << currentIndex << "ID:" << selectedTrialId << "Text:" << trialCombo->currentText();
    qDebug() << "[LoadParticipants] ComboBox data:" << trialCombo->currentData().toInt();
    
    // Enable/disable file selection based on event selection
    selectFileButton->setEnabled(eventSelected);
    
    // Enable preview button if both event and file are selected
    previewButton->setEnabled(eventSelected && !selectedFilePath.isEmpty());
    importButton->setEnabled(false);
    
    previewTable->setRowCount(0);
    participantsData.clear();
    
    if (eventSelected) {
        statusLabel->setText("Event selected. Please select an Excel file to continue.");
        statusLabel->setStyleSheet("color: #2196F3;");
    } else {
        statusLabel->setText("Please select an event first.");
        statusLabel->setStyleSheet("color: #f44336;");
        
        // Clear file selection when no event is selected
        selectedFilePath.clear();
        filePathEdit->clear();
    }
}

void LoadParticipantsWindow::resetForm()
{
    selectedFilePath.clear();
    filePathEdit->clear();
    trialCombo->setCurrentIndex(0);
    selectedTrialId = -1;
    previewTable->setRowCount(0);
    participantsData.clear();
    previewButton->setEnabled(false);
    importButton->setEnabled(false);
    statusLabel->setText("");
}

void LoadParticipantsWindow::setControlsEnabled(bool enabled)
{
    trialCombo->setEnabled(enabled);
    
    // Only enable file selection if controls are enabled AND an event is selected
    selectFileButton->setEnabled(enabled && trialCombo->currentData().toInt() > 0);
    
    previewButton->setEnabled(enabled && trialCombo->currentData().toInt() > 0 && !selectedFilePath.isEmpty());
    importButton->setEnabled(enabled && importButton->isEnabled());
}

void LoadParticipantsWindow::setActiveTrialId(int activeTrialId)
{
    this->activeTrialId = activeTrialId;
}

void LoadParticipantsWindow::detectConflicts()
{
    conflictsData.clear();
    
    if (selectedTrialId <= 0) {
        return;
    }
    
    try {
        // Get existing registrations for this trial
        Registrations::Repository registrationsRepo(m_dbManager.database());
        auto existingResult = registrationsRepo.getRegistrationsByTrial(selectedTrialId);
        
        if (!existingResult.has_value()) {
            return; // No existing registrations, no conflicts
        }
        
        auto existingRegistrations = existingResult.value();
        
        // Create map of existing participants by name (case insensitive)
        QMap<QString, Registrations::Registration> existingMap;
        for (const auto& reg : existingRegistrations) {
            // Get athlete name
            Athletes::Repository athletesRepo(m_dbManager.database());
            auto athleteResult = athletesRepo.getAthleteById(reg.athleteId);
            if (athleteResult.has_value()) {
                QString athleteName = athleteResult.value().name.trimmed().toLower();
                existingMap[athleteName] = reg;
            }
        }
        
        // Check each participant from Excel for conflicts
        for (const auto& participant : participantsData) {
            if (!participant.isValid()) continue;
            
            QString participantName = participant.name.trimmed().toLower();
            
            if (existingMap.contains(participantName)) {
                // Found conflict - get existing data
                auto existingReg = existingMap[participantName];
                
                // Get existing category and modality names
                QString existingCategory, existingModality;
                
                Categories::Repository categoriesRepo(m_dbManager.database());
                auto catResult = categoriesRepo.getCategoryById(existingReg.categoryId);
                if (catResult.has_value()) {
                    existingCategory = catResult.value().name;
                }
                
                Modalities::Repository modalitiesRepo(m_dbManager.database());
                auto modResult = modalitiesRepo.getModalityById(existingReg.modalityId);
                if (modResult.has_value()) {
                    existingModality = modResult.value().name;
                }
                
                // Check if there are actual differences
                bool hasDifferences = (
                    existingReg.plateCode.trimmed().toLower() != participant.plateCode.trimmed().toLower() ||
                    existingCategory.trimmed().toLower() != participant.category.trimmed().toLower() ||
                    existingModality.trimmed().toLower() != participant.modality.trimmed().toLower()
                );
                
                if (hasDifferences) {
                    ConflictData conflict;
                    conflict.plateCode = participant.plateCode;
                    
                    // Database version
                    conflict.dbName = participant.name; // Same name
                    conflict.dbCategory = existingCategory;
                    conflict.dbModality = existingModality;
                    
                    // Excel version
                    conflict.excelName = participant.name;
                    conflict.excelCategory = participant.category;
                    conflict.excelModality = participant.modality;
                    
                    conflict.useExcelVersion = true; // Default to Excel version
                    conflict.resolved = false;
                    
                    conflictsData.append(conflict);
                }
            }
        }
        
        // If conflicts found, show conflicts dialog
        if (!conflictsData.isEmpty()) {
            showConflictsDialog();
        }
        
    } catch (const std::exception& e) {
        qDebug() << "Error detecting conflicts:" << e.what();
    }
}

void LoadParticipantsWindow::showConflictsDialog()
{
    if (conflictsData.isEmpty()) {
        return;
    }
    
    QDialog conflictDialog(this);
    conflictDialog.setWindowTitle("Resolve Registration Conflicts");
    conflictDialog.setModal(true);
    conflictDialog.resize(800, 400);
    
    QVBoxLayout* layout = new QVBoxLayout(&conflictDialog);
    
    QLabel* headerLabel = new QLabel(QString("Found %1 participants that already exist with different data.\nChoose which version to keep:").arg(conflictsData.size()));
    headerLabel->setWordWrap(true);
    headerLabel->setStyleSheet("font-weight: bold; color: #FF9800; margin-bottom: 10px;");
    layout->addWidget(headerLabel);
    
    // Create table for conflicts
    QTableWidget* conflictTable = new QTableWidget();
    conflictTable->setColumnCount(6);
    QStringList headers = {"Name", "Database Version", "Excel Version", "Choose", "", ""};
    conflictTable->setHorizontalHeaderLabels(headers);
    conflictTable->setRowCount(conflictsData.size());
    
    for (int row = 0; row < conflictsData.size(); ++row) {
        const auto& conflict = conflictsData[row];
        
        // Name
        conflictTable->setItem(row, 0, new QTableWidgetItem(conflict.dbName));
        
        // Database version
        QString dbVersion = QString("Plate: %1\nCategory: %2\nModality: %3")
            .arg(conflict.plateCode, conflict.dbCategory, conflict.dbModality);
        QTableWidgetItem* dbItem = new QTableWidgetItem(dbVersion);
        dbItem->setBackground(QColor(255, 235, 235)); // Light red
        conflictTable->setItem(row, 1, dbItem);
        
        // Excel version  
        QString excelVersion = QString("Plate: %1\nCategory: %2\nModality: %3")
            .arg(conflict.plateCode, conflict.excelCategory, conflict.excelModality);
        QTableWidgetItem* excelItem = new QTableWidgetItem(excelVersion);
        excelItem->setBackground(QColor(235, 255, 235)); // Light green
        conflictTable->setItem(row, 2, excelItem);
        
        // Radio buttons for choice
        QWidget* choiceWidget = new QWidget();
        QHBoxLayout* choiceLayout = new QHBoxLayout(choiceWidget);
        
        QRadioButton* keepDbRadio = new QRadioButton("Keep Database");
        QRadioButton* keepExcelRadio = new QRadioButton("Keep Excel");
        keepExcelRadio->setChecked(true); // Default to Excel
        
        choiceLayout->addWidget(keepDbRadio);
        choiceLayout->addWidget(keepExcelRadio);
        choiceWidget->setLayout(choiceLayout);
        
        conflictTable->setCellWidget(row, 3, choiceWidget);
        
        // Store radio buttons for later access
        conflictTable->setProperty(QString("dbRadio_%1").arg(row).toLatin1(), QVariant::fromValue(keepDbRadio));
        conflictTable->setProperty(QString("excelRadio_%1").arg(row).toLatin1(), QVariant::fromValue(keepExcelRadio));
    }
    
    conflictTable->resizeRowsToContents();
    conflictTable->resizeColumnsToContents();
    layout->addWidget(conflictTable);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* resolveButton = new QPushButton("Apply Changes");
    QPushButton* cancelButton = new QPushButton("Cancel Import");
    
    resolveButton->setStyleSheet("font-weight: bold; background-color: #4CAF50; color: white;");
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(resolveButton);
    buttonLayout->addWidget(cancelButton);
    
    layout->addLayout(buttonLayout);
    
    // Connect buttons
    connect(resolveButton, &QPushButton::clicked, [&conflictDialog, conflictTable, this]() {
        // Read choices and update conflictsData
        for (int row = 0; row < conflictsData.size(); ++row) {
            QRadioButton* excelRadio = conflictTable->property(QString("excelRadio_%1").arg(row).toLatin1()).value<QRadioButton*>();
            if (excelRadio) {
                conflictsData[row].useExcelVersion = excelRadio->isChecked();
                conflictsData[row].resolved = true;
            }
        }
        conflictDialog.accept();
    });
    
    connect(cancelButton, &QPushButton::clicked, &conflictDialog, &QDialog::reject);
    
    // Show dialog and update preview if accepted
    if (conflictDialog.exec() == QDialog::Accepted) {
        updatePreviewTableWithConflicts();
    } else {
        // User cancelled - don't proceed with import
        statusLabel->setText("Import cancelled due to unresolved conflicts.");
        statusLabel->setStyleSheet("color: #f44336;");
        importButton->setEnabled(false);
    }
}

void LoadParticipantsWindow::updatePreviewTableWithConflicts()
{
    updatePreviewTable();
    
    if (conflictsData.isEmpty()) {
        return;
    }
    
    // Highlight conflicts in the preview table
    for (int row = 0; row < previewTable->rowCount(); ++row) {
        QTableWidgetItem* nameItem = previewTable->item(row, 0);
        if (!nameItem) continue;
        
        QString participantName = nameItem->text().trimmed().toLower();
        
        // Check if this participant has a conflict
        for (const auto& conflict : conflictsData) {
            if (conflict.dbName.trimmed().toLower() == participantName) {
                // Highlight the row in yellow to indicate conflict resolution
                for (int col = 0; col < previewTable->columnCount(); ++col) {
                    QTableWidgetItem* item = previewTable->item(row, col);
                    if (item) {
                        item->setBackground(QColor(255, 255, 0, 100)); // Light yellow
                    }
                }
                
                // Update status
                QTableWidgetItem* statusItem = previewTable->item(row, 4);
                if (statusItem) {
                    if (conflict.resolved) {
                        statusItem->setText(conflict.useExcelVersion ? "Excel Version" : "Database Version");
                        statusItem->setBackground(conflict.useExcelVersion ? 
                            QColor(235, 255, 235) : QColor(255, 235, 235));
                    } else {
                        statusItem->setText("Conflict - Needs Resolution");
                        statusItem->setBackground(QColor(255, 255, 0));
                    }
                }
                break;
            }
        }
    }
    
    // Update status message
    int resolvedConflicts = 0;
    for (const auto& conflict : conflictsData) {
        if (conflict.resolved) resolvedConflicts++;
    }
    
    statusLabel->setText(QString("Conflicts resolved: %1/%2. Ready to import.")
        .arg(resolvedConflicts).arg(conflictsData.size()));
    statusLabel->setStyleSheet("color: #4CAF50;");
    
    importButton->setEnabled(resolvedConflicts == conflictsData.size());
}
