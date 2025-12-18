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
    bool valid = false;
    QString errorMessage = "";
};

class LoadParticipantsWindow : public QDialog
{
    Q_OBJECT

public:
    LoadParticipantsWindow(DBManager& dbManager, QWidget *parent = nullptr);
    ~LoadParticipantsWindow();

    void setAvailableTrials(const QVector<Trials::TrialInfo>& trials);

private slots:
    void selectFile();
    void loadPreview();
    void importParticipants();
    void onTrialSelected();

private:
    Ui::LoadParticipantsWindow *ui;
    DBManager& m_dbManager;
    
    // UI Components
    QVBoxLayout* mainLayout;
    QFormLayout* formLayout;
    QHBoxLayout* fileLayout;
    QHBoxLayout* buttonLayout;
    
    QComboBox* trialCombo;
    QLineEdit* filePathEdit;
    QPushButton* selectFileButton;
    QPushButton* previewButton;
    QPushButton* importButton;
    QPushButton* cancelButton;
    QTableWidget* previewTable;
    QProgressBar* progressBar;
    QLabel* statusLabel;
    
    // Data
    QVector<Trials::TrialInfo> availableTrials;
    QVector<ParticipantData> participantsData;
    QVector<Athletes::Athlete> athletes;
    QVector<Categories::Category> categories;
    QVector<Modalities::Modality> modalities;
    QString selectedFilePath;
    int selectedTrialId;
    
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
};

#endif // LOADPARTICIPANTSWINDOW_H