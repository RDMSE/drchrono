#ifndef TRIALCONTROLLER_H
#define TRIALCONTROLLER_H

#include <QString>
#include <QSqlDatabase>
#include <QVector>
#include <optional>

struct GroupInfo {
    int id;
    QString name;
};
struct TrialInfo {
    int id;
    QString name;
    QString startDateTime;
    QString endDateTime;
};


class TrialController
{
public:
    explicit TrialController(const QSqlDatabase& db);

    int createOrLoadTrial(const QString& trialName);

    int getTrialId() const;

    std::optional<TrialInfo> getTrialInformationByName(const QString& name);

    QVector<GroupInfo> loadGroups();

    bool addGroup(const QString& name);

    bool registerEvent(
        const int trialId,
        const QString& group,
        const QString& placa,
        const QString& startTime,
        const QString& endTime,
        int durationMs,
        const QString& notes
    );

    QVector<GroupInfo> loadGroupsFromConfiguration(const QString& configPath);
    QString loadTrialNameFromConfiguration(const QString& configPath);

    std::optional<QVector<GroupInfo>> getTrialGroups();

    bool loadActiveTrial();

    bool startTrial();

    bool stopTrial();

    QString getActiveTrialName() const {
        return m_activeTrialName;
    }

    QString getActiveTrialStartDateTime() const {
        return m_activeTrialStartDateTime;
    }

private:
    QSqlDatabase m_db;
    int m_trialId = -1;
    QString m_activeTrialName = "";
    QString m_activeTrialStartDateTime = "";

};

#endif // TRIALCONTROLLER_H
