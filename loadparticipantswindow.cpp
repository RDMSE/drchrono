#include "loadparticipantswindow.h"
#include "repository/trials/trialsrepository.h"
#include "repository/registrations/registrationsrepository.h"
#include "repository/athletes/athletesrepository.h"
#include "repository/categories/categoriesrepository.h"
#include "repository/modalities/modalitiesrepository.h"
#include "utils/excelutils.h"
#include <QDebug>
#include <QDateTime>
#include <QApplication>

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
{
    setupUI();
    loadBasicData();
}

LoadParticipantsWindow::~LoadParticipantsWindow()
{
}

void LoadParticipantsWindow::setupUI()
{
    setWindowTitle("Carregar Participantes do Excel");
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
    formLayout->addRow("Evento:", trialCombo);
    
    // File selection
    fileLayout = new QHBoxLayout();
    filePathEdit = new QLineEdit();
    filePathEdit->setPlaceholderText("Selecione um arquivo Excel (.xlsx)");
    filePathEdit->setReadOnly(true);
    
    selectFileButton = new QPushButton("Selecionar Arquivo");
    selectFileButton->setEnabled(false); // Initially disabled until event is selected
    connect(selectFileButton, &QPushButton::clicked, this, &LoadParticipantsWindow::selectFile);
    
    fileLayout->addWidget(filePathEdit);
    fileLayout->addWidget(selectFileButton);
    formLayout->addRow("Arquivo Excel:", fileLayout);
    
    mainLayout->addLayout(formLayout);
    
    // Preview button
    previewButton = new QPushButton("Visualizar Dados");
    previewButton->setEnabled(false);
    connect(previewButton, &QPushButton::clicked, this, &LoadParticipantsWindow::loadPreview);
    mainLayout->addWidget(previewButton);
    
    // Preview table
    previewTable = new QTableWidget();
    previewTable->setColumnCount(5);
    QStringList headers = {"Nome", "Código da Placa", "Categoria", "Modalidade", "Status"};
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
    
    importButton = new QPushButton("Importar Participantes");
    importButton->setEnabled(false);
    importButton->setStyleSheet("font-weight: bold; background-color: #4CAF50; color: white;");
    connect(importButton, &QPushButton::clicked, this, &LoadParticipantsWindow::importParticipants);
    
    cancelButton = new QPushButton("Cancelar");
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
        QMessageBox::warning(this, "Erro", "Erro ao carregar dados básicos do sistema");
    }
}

void LoadParticipantsWindow::setAvailableTrials(const QVector<Trials::TrialInfo>& trials)
{
    availableTrials.clear();
    trialCombo->clear();
    
    QDateTime now = Utils::DateTimeUtils::now();
    
    // Add placeholder
    trialCombo->addItem("Select an event...", -1);
    
    // Filter trials - only present and future events
    for (const auto& trial : trials) {
        if (validateTrialForImport(trial)) {
            availableTrials.append(trial);
            QString displayText = QString("%1 - %2")
                                .arg(trial.name)
                                .arg(trial.scheduledDateTime.toString("dd/MM/yyyy hh:mm"));
            trialCombo->addItem(displayText, trial.id);
        }
    }
    
    if (availableTrials.isEmpty()) {
        statusLabel->setText("Não há eventos futuros disponíveis para importação.");
        statusLabel->setStyleSheet("color: #f44336;");
        trialCombo->setEnabled(false);
        selectFileButton->setEnabled(false);
    } else {
        statusLabel->setText(QString("Encontrados %1 eventos disponíveis. Selecione um evento primeiro.").arg(availableTrials.size()));
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
        QMessageBox::warning(this, "Erro", "Selecione um evento antes de escolher o arquivo Excel.");
        return;
    }
    
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Selecionar arquivo Excel",
        documentsPath,
        "Arquivos Excel (*.xlsx *.xls);;Todos os arquivos (*)"
    );
    
    if (!fileName.isEmpty()) {
        selectedFilePath = fileName;
        filePathEdit->setText(fileName);
        
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
            if (participant.valid) {
                validCount++;
            } else {
                totalErrors++;
                if (participant.errorMessage.contains("duplicada")) {
                    duplicateCount++;
                }
            }
        }
        
        if (validCount > 0 && duplicateCount == 0) {
            importButton->setEnabled(true);
            statusLabel->setText(QString("Arquivo carregado! %1 participantes válidos encontrados.")
                               .arg(validCount));
            statusLabel->setStyleSheet("color: #4CAF50;");
        } else if (duplicateCount > 0) {
            importButton->setEnabled(false);
            statusLabel->setText(QString("ERRO: %1 placas duplicadas detectadas! Corrija o arquivo antes de importar.")
                               .arg(duplicateCount));
            statusLabel->setStyleSheet("color: #f44336; font-weight: bold;");
        } else {
            importButton->setEnabled(false);
            statusLabel->setText(QString("Nenhum participante válido encontrado. %1 erros detectados.")
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
        QMessageBox::warning(this, "Erro", "Erro ao processar arquivo Excel");
        return false;
    }
}

void LoadParticipantsWindow::validateParticipantsData()
{
    // First pass: basic validation
    for (auto& participant : participantsData) {
        participant.valid = true;
        participant.errorMessage = "";
        
        // Validate name
        if (participant.name.isEmpty()) {
            participant.valid = false;
            participant.errorMessage = "Nome obrigatório";
            continue;
        }
        
        // Validate plate code
        if (participant.plateCode.isEmpty()) {
            participant.valid = false;
            participant.errorMessage = "Código da placa obrigatório";
            continue;
        }
        
        // Validate category
        if (participant.category.isEmpty()) {
            participant.valid = false;
            participant.errorMessage = "Categoria obrigatória";
            continue;
        }
        
        // Validate modality
        if (participant.modality.isEmpty()) {
            participant.valid = false;
            participant.errorMessage = "Modalidade obrigatória";
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
                participantsData[index].valid = false;
                participantsData[index].errorMessage = QString("Placa duplicada (%1 ocorrências)").arg(indices.size());
            }
            qDebug() << QString("Found duplicate plate code '%1' in %2 rows").arg(plateCode, QString::number(indices.size()));
        }
    }
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
        
        QString status = participant.valid ? "Válido" : participant.errorMessage;
        QTableWidgetItem* statusItem = new QTableWidgetItem(status);
        
        // Color coding based on validation
        QColor rowColor;
        if (participant.valid) {
            rowColor = QColor(200, 255, 200); // Light green
        } else if (participant.errorMessage.contains("duplicada")) {
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
        QMessageBox::warning(this, "Erro", "Selecione um evento válido.");
        return;
    }
    
    // Count valid participants
    int validCount = 0;
    for (const auto& participant : participantsData) {
        if (participant.valid) {
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
            
            if (!participant.valid) {
                continue;
            }
            
            statusLabel->setText(QString("Importing: %1").arg(participant.name));
            QApplication::processEvents();
            
            try {
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
                
                // Create registration
                qDebug() << QString("[LoadParticipants] Creating registration: trialId=%1, athleteId=%2, plateCode=%3, modalityId=%4, categoryId=%5")
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
        statusLabel->setText("Evento selecionado. Selecione um arquivo Excel para continuar.");
        statusLabel->setStyleSheet("color: #2196F3;");
    } else {
        statusLabel->setText("Selecione um evento primeiro.");
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
