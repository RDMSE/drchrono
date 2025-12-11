#ifndef TRIALCONTROLLER_H
#define TRIALCONTROLLER_H

#include <QString>
#include <QSqlDatabase>
#include <QVector>

struct GroupInfo {
    int id;
    QString name;
};


class TrialController
{
public:
    explicit TrialController(QSqlDatabase& db);

    int createOrLoadTrial(const QString& trialName);

    int getTrialId() const;

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

    QVector<GroupInfo> getTrialGroups();

private:
    QSqlDatabase m_db;
    int m_trialId = -1;
};

#endif // TRIALCONTROLLER_H
