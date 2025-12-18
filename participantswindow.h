#ifndef PARTICIPANTSWINDOW_H
#define PARTICIPANTSWINDOW_H

#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QMessageBox>
#include <QHeaderView>
#include <algorithm>
#include "dbmanager.h"
#include "model/trialinfo.h"
#include "model/registration.h"
#include "model/athlete.h"
#include "model/category.h"
#include "model/modality.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ParticipantsWindow; }
QT_END_NAMESPACE

class ParticipantsWindow : public QDialog
{
    Q_OBJECT

public:
    ParticipantsWindow(DBManager& dbManager, QWidget *parent = nullptr);
    ~ParticipantsWindow();

    void setCurrentTrial(const Trials::TrialInfo& trial);

private slots:
    void loadParticipants();
    void onCategoryTabChanged(int index);

private:
    Ui::ParticipantsWindow *ui;
    DBManager& m_dbManager;
    
    // UI Components
    QTabWidget* tabWidget;
    QVBoxLayout* mainLayout;
    QHBoxLayout* headerLayout;
    QLabel* eventLabel;
    
    // Data
    Trials::TrialInfo currentTrial;
    QVector<Registrations::Registration> registrations;
    QVector<Athletes::Athlete> athletes;
    QVector<Categories::Category> categories;
    QVector<Modalities::Modality> modalities;
    
    // Methods
    void setupUI();
    void loadBasicData();
    void createCategoryTabs();
    void populateTable(QTableWidget* table, const QVector<Registrations::Registration>& categoryRegistrations);
    void populateCombinationTable(QTableWidget* table, const QVector<Registrations::Registration>& combinationRegistrations);
    QVector<Registrations::Registration> getRegistrationsByCategory(int categoryId);
    QVector<Registrations::Registration> getRegistrationsByCategoryAndModality(int categoryId, int modalityId);
    QString getAthleteName(int athleteId);
    QString getModalityName(int modalityId);
    QString getCategoryName(int categoryId);
};

#endif // PARTICIPANTSWINDOW_H