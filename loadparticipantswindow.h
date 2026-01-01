#ifndef LOADPARTICIPANTSWINDOW_H
#define LOADPARTICIPANTSWINDOW_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QTableWidget>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QStandardPaths>
#include <QMap>
#include <QSettings>
#include "dbmanager.h"
#include "model/trialinfo.h"
#include "model/registration.h"
#include "model/athlete.h"
#include "model/category.h"
#include "model/modality.h"

QT_BEGIN_NAMESPACE
namespace Ui { class LoadParticipantsWindow; }
QT_END_NAMESPACE

struct ParticipantData {
    QString name;
    QString plateCode;
    QString category;
    QString modality;
    QString errorMessage = "";
    bool isValid() const { return errorMessage.isEmpty(); }
};

struct ConflictData {
    QString plateCode;
    // Database version
    QString dbName;
    QString dbCategory;
    QString dbModality;
    // Excel version  
    QString excelName;
    QString excelCategory;
    QString excelModality;
    // Resolution (true = keep Excel, false = keep DB)
    bool useExcelVersion = true;
    bool resolved = false;
};

class LoadParticipantsWindow : public QDialog
{
    Q_OBJECT

public:
    LoadParticipantsWindow(DBManager& dbManager, QWidget *parent = nullptr);
    ~LoadParticipantsWindow() override;

    void setAvailableTrials(const QVector<Trials::TrialInfo>& trials);
    void setActiveTrialId(int activeTrialId);

private slots:
    void selectFile();
    void loadPreview();
    void importParticipants();
    void onTrialSelected();

private:
    Ui::LoadParticipantsWindow *ui;
    DBManager& m_dbManager;
    
    // UI Components
    QVBoxLayout* m_mainLayout;
    QFormLayout* m_formLayout;
    QHBoxLayout* m_fileLayout;
    QHBoxLayout* m_buttonLayout;
    
    QComboBox* m_trialCombo;
    QLineEdit* m_filePathEdit;
    QPushButton* m_selectFileButton;
    QPushButton* m_previewButton;
    QPushButton* m_importButton;
    QPushButton* m_cancelButton;
    QTableWidget* m_previewTable;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    
    // Data
    QVector<Trials::TrialInfo> m_availableTrials;
    QVector<ParticipantData> m_participantsData;
    QVector<ConflictData> m_conflictsData;
    QVector<Athletes::Athlete> m_athletes;
    QVector<Categories::Category> m_categories;
    QVector<Modalities::Modality> m_modalities;
    QString m_selectedFilePath;
    int m_selectedTrialId;
    int m_activeTrialId;
    
    // Methods
    void setupUI();
    void loadBasicData();
    bool validateTrialForImport(const Trials::TrialInfo& trial);
    bool loadExcelFile(const QString& filePath);
    void updatePreviewTable();
    void validateParticipantsData();
    bool createAthleteIfNotExists(const QString& name, int& athleteId);
    int findOrCreateCategory(const QString& categoryName);
    int findOrCreateModality(const QString& modalityName);
    void resetForm();
    void setControlsEnabled(bool enabled);
    void detectConflicts();
    void showConflictsDialog();
    void updatePreviewTableWithConflicts();
};

#endif // LOADPARTICIPANTSWINDOW_H
