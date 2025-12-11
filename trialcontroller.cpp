#include "trialcontroller.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSettings>
#include <QDateTime>

TrialController::TrialController(const QSqlDatabase& db): m_db(db) { }

int TrialController::createOrLoadTrial(const QString& trialName) {
    QSqlQuery insertQuery(m_db);

    // tries to create the trial case it doesn't exist
    QString sql = R"(
        INSERT OR IGNORE INTO trials(name) VALUES(:name)
    )";
    insertQuery.prepare(sql);
    insertQuery.bindValue(":name", trialName);

    if (!insertQuery.exec()) {
        qWarning() << "[DB] Error creating trail " << trialName;
        return -1;
    }

    QSqlQuery selectQuery(m_db);
    // find trial ID
    sql = R"(
        SELECT  id,
                startDateTime,
                endDateTime
        FROM trials
        WHERE name=:name
    )";
    selectQuery.prepare(sql);
    selectQuery.bindValue(":name", trialName);
    if (!selectQuery.exec() /*|| !selectQuery.next()*/) {
        qWarning() << "[DB] error loading trial:" << selectQuery.lastError().text();
        return -1;
    }

    if(selectQuery.next())
        m_trialId = selectQuery.value(0).toInt();

    return m_trialId;
}

QVector<GroupInfo> TrialController::getTrialGroups() {
    QSqlQuery query(m_db);

    QString sql = R"(
        SELECT gr.id, gr.name FROM groups gr WHERE trialId = :id
    )";

    query.prepare(sql);
    query.bindValue(":id", m_trialId);

    if (!query.exec()) {
        qWarning() << "[DB] Error reading trail groups";
        return {};
    }

    QVector<GroupInfo> result;

    while(query.next()) {
        result.push_back({
            .id = query.value(0).toInt(),
            .name = query.value(1).toString()
        });
    }

    return result;
}

int TrialController::getTrialId() const {
    return m_trialId;
}

QVector<GroupInfo> TrialController::loadGroups() {
    QVector<GroupInfo> groups;

    QSqlQuery query(m_db);
    query.prepare("SELECT id, name FROM group WHERE trialId=:id");
    query.bindValue(":id", getTrialId());
    if (!query.exec()) {
        qWarning() << "[DB] error loading groups: " << query.lastError().text();
        return {};
    }

    while(query.next()) {
        groups.push_back({
            .id = query.value(0).toInt(),
            .name = query.value(1).toString()
        });
    }

    return groups;
}

QVector<GroupInfo> TrialController::loadGroupsFromConfiguration(const QString& configPath) {
    QSettings settings(configPath, QSettings::IniFormat);

    QStringList groups = settings.value("Groups").toString().split(",", Qt::SkipEmptyParts);
    std::transform(groups.begin(), groups.end(), groups.begin(), [](const QString &s){ return s.trimmed();});

    QString trial = settings.value("Trial").toString().trimmed();

    createOrLoadTrial(trial);
    auto existingGroups = getTrialGroups();

    QSet<QString> existingGroupNames;
    for (const auto& g: existingGroups) {
        existingGroupNames.insert(g.name);
    }

    for(const auto& group: groups) {
        if (!existingGroupNames.contains(group)) {
            addGroup(group);
        }
    }

    return getTrialGroups();
}

QString TrialController::loadTrialNameFromConfiguration(const QString& configPath) {
    QSettings settings(configPath, QSettings::IniFormat);
    return settings.value("Trial").toString().trimmed();
}

bool TrialController::addGroup(const QString& name) {
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO groups(trialId, name) VALUES (:id, :name)");
    query.bindValue(":id", getTrialId());
    query.bindValue(":name", name);

    if (!query.exec()) {
        qWarning() << "[DB] error adding group:" << query.lastError().text();
        return false;
    }

    return true;
}

bool TrialController::registerEvent(
    const int trialId,
    const QString& group,
    const QString& placa,
    const QString& startTime,
    const QString& endTime,
    int durationMs,
    const QString& notes
) {
    QSqlQuery query(m_db);

    query.prepare(R"(
        INSERT INTO events(trialId, groupName, placa, startTime, endTime, durationMs, notes)
        VALUES(:id, :group, :placa, :start, :end, :duration, :notes)
    )");

    query.bindValue(":id", trialId);
    query.bindValue(":group", group);
    query.bindValue(":placa", placa);
    query.bindValue(":start", startTime);
    query.bindValue(":end", endTime);
    query.bindValue(":duration", durationMs);
    query.bindValue(":notes", notes);

    if (!query.exec()) {
        qWarning() << "[DB] error registreing event:" << query.lastError().text();
        return false;
    }

    return true;
}

bool TrialController::loadActiveTrial() {
    QSqlQuery query(m_db);
    QString sql = R"(
        SELECT  id,
                name,
                startDateTime
        FROM trials
        WHERE startDateTime IS NOT NULL
            AND endDateTime IS NULL
        ORDER BY id DESC
        LIMIT 1;
    )";

    query.prepare(sql);

    if (!query.exec()) {
        qWarning() << "[DB] Error on active trail verification:" << query.lastError().text();
        return false;
    }

    if (!query.next())
        return false;

    m_trialId = query.value(0).toInt();
    m_activeTrialName = query.value(1).toString();
    m_activeTrialStartDateTime = query.value(2).toString();

    return true;
}

bool TrialController::startTrial() {
    QSqlQuery query(m_db);
    QString sql = R"(
        UPDATE trials
        SET startDateTime = :start
        WHERE id = :id
    )";

    query.prepare(sql);
    query.bindValue(":start", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":id", m_trialId);

    if (!query.exec()) {
        qWarning() << "[DB] Error starting trial:" << query.lastError().text();
        return false;
    }

    return true;
}

bool TrialController::stopTrial() {
    QSqlQuery query(m_db);
    QString sql = R"(
        UPDATE trials
        SET endDateTime = :end
        WHERE id = :id
    )";

    query.prepare(sql);
    query.bindValue(":end", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":id", m_trialId);

    if (!query.exec()) {
        qWarning() << "[DB] Error stopping trial:" << query.lastError().text();
        return false;
    }

    return true;
}
