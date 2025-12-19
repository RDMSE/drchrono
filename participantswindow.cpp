#include "participantswindow.h"
#include "repository/registrations/registrationsrepository.h"
#include "repository/athletes/athletesrepository.h"
#include "repository/categories/categoriesrepository.h"
#include "repository/modalities/modalitiesrepository.h"
#include <QDebug>

ParticipantsWindow::ParticipantsWindow(DBManager& dbManager, QWidget *parent)
    : QDialog(parent)
    , m_dbManager(dbManager)
    , tabWidget(nullptr)
    , mainLayout(nullptr)
    , headerLayout(nullptr)
    , eventLabel(nullptr)
{
    setupUI();
    loadBasicData();
}

ParticipantsWindow::~ParticipantsWindow()
{
}

void ParticipantsWindow::setupUI()
{
    setWindowTitle("Registered Participants");
    setModal(true);
    resize(800, 600);

    mainLayout = new QVBoxLayout(this);
    
    // Header layout
    headerLayout = new QHBoxLayout();
    eventLabel = new QLabel("Event: No event selected");
    eventLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    
    headerLayout->addWidget(eventLabel);
    
    mainLayout->addLayout(headerLayout);
    
    // Tab widget
    tabWidget = new QTabWidget();
    mainLayout->addWidget(tabWidget);
    
    setLayout(mainLayout);
}

void ParticipantsWindow::loadBasicData()
{
    try {
        // Load categories
        Categories::Repository categoriesRepo(m_dbManager.database());
        auto categoriesResult = categoriesRepo.getAllCategories();
        if (categoriesResult.has_value()) {
            categories = categoriesResult.value();
            qDebug() << "[ParticipantsWindow] Loaded" << categories.size() << "categories";
        } else {
            qDebug() << "[ParticipantsWindow] Error loading categories:" << categoriesResult.error();
        }
        
        // Load modalities
        Modalities::Repository modalitiesRepo(m_dbManager.database());
        auto modalitiesResult = modalitiesRepo.getAllModalities();
        if (modalitiesResult.has_value()) {
            modalities = modalitiesResult.value();
            qDebug() << "[ParticipantsWindow] Loaded" << modalities.size() << "modalities";
        } else {
            qDebug() << "[ParticipantsWindow] Error loading modalities:" << modalitiesResult.error();
        }
        
        // Load athletes
        Athletes::Repository athletesRepo(m_dbManager.database());
        auto athletesResult = athletesRepo.getAllAthletes();
        if (athletesResult.has_value()) {
            athletes = athletesResult.value();
            qDebug() << "[ParticipantsWindow] Loaded" << athletes.size() << "athletes";
        } else {
            qDebug() << "[ParticipantsWindow] Error loading athletes:" << athletesResult.error();
        }
        
    } catch (const std::exception& e) {
        qDebug() << "Error loading basic data:" << e.what();
        QMessageBox::warning(this, "Error", "Error loading basic system data");
    }
}

void ParticipantsWindow::setCurrentTrial(const Trials::TrialInfo& trial)
{
    currentTrial = trial;
    qDebug() << "[ParticipantsWindow] Setting current trial:" << trial.name << "ID:" << trial.id;
    eventLabel->setText(QString("Evento: %1 - %2")
                       .arg(trial.name)
                       .arg(trial.scheduledDateTime.toString("dd/MM/yyyy hh:mm")));
    loadParticipants();
}

void ParticipantsWindow::loadParticipants()
{
    if (currentTrial.id <= 0) {
        QMessageBox::information(this, "Information", "No event selected");
        return;
    }
    
    try {
        Registrations::Repository registrationsRepo(m_dbManager.database());
        auto registrationsResult = registrationsRepo.getRegistrationsByTrial(currentTrial.id);
        
        if (registrationsResult.has_value()) {
            registrations = registrationsResult.value();
            qDebug() << QString("Loaded %1 registrations for trial %2").arg(registrations.size()).arg(currentTrial.id);
            createCategoryTabs();
        } else {
            qDebug() << "Error loading registrations:" << registrationsResult.error();
            QMessageBox::warning(this, "Erro", "Erro ao carregar registrações: " + registrationsResult.error());
        }
        
    } catch (const std::exception& e) {
        qDebug() << "Error loading participants:" << e.what();
        QMessageBox::warning(this, "Error", "Error loading participants");
    }
}

void ParticipantsWindow::createCategoryTabs()
{
    qDebug() << "[ParticipantsWindow] Creating category tabs...";
    // Clear existing tabs
    tabWidget->clear();
    
    if (categories.isEmpty()) {
        qDebug() << "[ParticipantsWindow] No categories found, showing 'no data' tab";
        QLabel* noDataLabel = new QLabel("No categories found");
        noDataLabel->setAlignment(Qt::AlignCenter);
        tabWidget->addTab(noDataLabel, "No data");
        return;
    }
    
    qDebug() << "[ParticipantsWindow] Processing" << categories.size() << "categories";
    
    // Sort categories alphabetically
    auto sortedCategories = categories;
    std::sort(sortedCategories.begin(), sortedCategories.end(), [](const Categories::Category& a, const Categories::Category& b) {
        return a.name < b.name;
    });
    
    // Create tabs for each category that has registrations
    int tabsCreated = 0;
    for (const auto& category : sortedCategories) {
        auto categoryRegistrations = getRegistrationsByCategory(category.id);
        
        if (!categoryRegistrations.isEmpty()) {
            qDebug() << QString("[ParticipantsWindow] Creating tab for category: '%1': %2 registrations")
                       .arg(category.name, QString::number(categoryRegistrations.size()));
            
            QTableWidget* table = new QTableWidget();
            // QStringList headers = {"Modalidade", "Código da Placa", "Nome do Atleta"};
            table->setColumnCount(headers.size());
            table->setHorizontalHeaderLabels(headers);
            
            // Configure table
            table->setSelectionBehavior(QAbstractItemView::SelectRows);
            table->setAlternatingRowColors(true);
            table->horizontalHeader()->setStretchLastSection(true);
            table->horizontalHeader()->resizeSection(0, 80);
            table->horizontalHeader()->resizeSection(1, 250);
            table->horizontalHeader()->resizeSection(2, 150);
            table->horizontalHeader()->resizeSection(3, 120);
            
            populateTable(table, categoryRegistrations);
            
            tabWidget->addTab(table, QString("%1 (%2)")
                             .arg(category.name)
                             .arg(categoryRegistrations.size()));
            
            tabsCreated++;
        }
    }
    
    qDebug() << "[ParticipantsWindow] Created" << tabsCreated << "tabs with data";
    if (tabWidget->count() == 0) {
        qDebug() << "[ParticipantsWindow] No tabs with data, showing 'no participants' message";
        QLabel* noDataLabel = new QLabel("Nenhum participante registrado neste evento");
        noDataLabel->setAlignment(Qt::AlignCenter);
        noDataLabel->setStyleSheet("color: #666; font-size: 14px;");
        tabWidget->addTab(noDataLabel, "Sem dados");
    }
}

void ParticipantsWindow::populateTable(QTableWidget* table, const QVector<Registrations::Registration>& categoryRegistrations)
{
    qDebug() << "[ParticipantsWindow] Populating table with" << categoryRegistrations.size() << "registrations";
    table->setRowCount(categoryRegistrations.size());
    
    for (int row = 0; row < categoryRegistrations.size(); ++row) {
        const auto& registration = categoryRegistrations[row];
        qDebug() << QString("[ParticipantsWindow] Row %1: Registration ID %2, Athlete ID %3, Plate: %4")
                   .arg(row).arg(registration.id).arg(registration.athleteId).arg(registration.plateCode);
        
        // Number (row + 1)
        // table->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        
        // Modality name
        QString modalityName = getModalityName(registration.modalityId);
        table->setItem(row, getHeaderIndex("Modalidade"), new QTableWidgetItem(modalityName));

        // Plate code
        table->setItem(row, getHeaderIndex("Código da Placa"), new QTableWidgetItem(registration.plateCode));

        // Athlete name
        QString athleteName = getAthleteName(registration.athleteId);
        table->setItem(row, getHeaderIndex("Nome do Atleta"), new QTableWidgetItem(athleteName));
        
        
        
        // Make items read-only
        for (int col = 0; col < headers.size(); ++col) {
            if (table->item(row, col)) {
                table->item(row, col)->setFlags(table->item(row, col)->flags() & ~Qt::ItemIsEditable);
            }
        }
    }
}

QVector<Registrations::Registration> ParticipantsWindow::getRegistrationsByCategory(int categoryId)
{
    QVector<Registrations::Registration> result;
    
    for (const auto& registration : registrations) {
        if (registration.categoryId == categoryId) {
            result.append(registration);
        }
    }
    
    // Order by modality and then by plate number
    std::sort(result.begin(), result.end(), [this](const Registrations::Registration& a, const Registrations::Registration& b) {
        // First by modality
        QString modalityA = getModalityName(a.modalityId);
        QString modalityB = getModalityName(b.modalityId);
        
        if (modalityA != modalityB) {
            return modalityA < modalityB;
        }
        
        // If modality is the same, order by plate number
        // Convert plate to number if possible, otherwise order alphabetically
        bool okA, okB;
        int plateNumA = a.plateCode.toInt(&okA);
        int plateNumB = b.plateCode.toInt(&okB);
        
        if (okA && okB) {
            return plateNumA < plateNumB;
        } else {
            return a.plateCode < b.plateCode;
        }
    });
    
    qDebug() << QString("[ParticipantsWindow] Found %1 registrations for category ID %2 (sorted by modality and plate)")
               .arg(result.size()).arg(categoryId);
    return result;
}

QVector<Registrations::Registration> ParticipantsWindow::getRegistrationsByCategoryAndModality(int categoryId, int modalityId)
{
    QVector<Registrations::Registration> result;
    
    for (const auto& registration : registrations) {
        if (registration.categoryId == categoryId && registration.modalityId == modalityId) {
            result.append(registration);
        }
    }
    
    // Order by plate number
    std::sort(result.begin(), result.end(), [](const Registrations::Registration& a, const Registrations::Registration& b) {
        // Convert plate to number if possible, otherwise order alphabetically
        bool okA, okB;
        int plateNumA = a.plateCode.toInt(&okA);
        int plateNumB = b.plateCode.toInt(&okB);
        
        if (okA && okB) {
            return plateNumA < plateNumB;
        } else {
            return a.plateCode < b.plateCode;
        }
    });
    
    qDebug() << QString("[ParticipantsWindow] Found %1 registrations for category ID %2, modality ID %3")
               .arg(result.size()).arg(categoryId).arg(modalityId);
    return result;
}

void ParticipantsWindow::populateCombinationTable(QTableWidget* table, const QVector<Registrations::Registration>& combinationRegistrations)
{
    qDebug() << "[ParticipantsWindow] Populating combination table with" << combinationRegistrations.size() << "registrations";
    table->setRowCount(combinationRegistrations.size());
    
    for (int row = 0; row < combinationRegistrations.size(); ++row) {
        const auto& registration = combinationRegistrations[row];
        qDebug() << QString("[ParticipantsWindow] Row %1: Registration ID %2, Athlete ID %3, Plate: %4")
                   .arg(row).arg(registration.id).arg(registration.athleteId).arg(registration.plateCode);
        
        // Number (row + 1)
        table->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        
        // Athlete name
        QString athleteName = getAthleteName(registration.athleteId);
        table->setItem(row, 1, new QTableWidgetItem(athleteName));
        
        // Plate code
        table->setItem(row, 2, new QTableWidgetItem(registration.plateCode));
        
        // Make items read-only
        for (int col = 0; col < 3; ++col) {
            if (table->item(row, col)) {
                table->item(row, col)->setFlags(table->item(row, col)->flags() & ~Qt::ItemIsEditable);
            }
        }
    }
}

QString ParticipantsWindow::getAthleteName(int athleteId)
{
    for (const auto& athlete : athletes) {
        if (athlete.id == athleteId) {
            return athlete.name;
        }
    }
    return QString("ID: %1").arg(athleteId);
}

QString ParticipantsWindow::getModalityName(int modalityId)
{
    for (const auto& modality : modalities) {
        if (modality.id == modalityId) {
            return modality.name;
        }
    }
    return QString("ID: %1").arg(modalityId);
}

QString ParticipantsWindow::getCategoryName(int categoryId)
{
    for (const auto& category : categories) {
        if (category.id == categoryId) {
            return category.name;
        }
    }
    return QString("ID: %1").arg(categoryId);
}

void ParticipantsWindow::onCategoryTabChanged(int index)
{
    // Handle tab change if needed
    Q_UNUSED(index);
}

