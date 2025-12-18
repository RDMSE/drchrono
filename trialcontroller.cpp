#include "trialcontroller.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSettings>
#include <QDateTime>
#include <QXlsx/header/xlsxdocument.h>
#include <QDebug>

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
    if (!selectQuery.exec()) {
        qWarning() << "[DB] error loading trial:" << selectQuery.lastError().text();
        return -1;
    }

    if(selectQuery.next())
        m_trialId = selectQuery.value(0).toInt();

    m_activeTrialName = trialName;

    return m_trialId;
}

    std::optional<QVector<GroupInfo>> TrialController::getTrialGroups() {
    QSqlQuery query(m_db);

    QString sql = R"(
        SELECT gr.id, gr.name FROM groups gr WHERE trialId = :id
    )";

    query.prepare(sql);
    query.bindValue(":id", m_trialId);

    if (!query.exec()) {
        qWarning() << "[DB] Error reading trail groups";
        return std::nullopt;
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

std::optional<TrialInfo> TrialController::getTrialInformationByName(const QString& name) {
    QSqlQuery selectQuery(m_db);
    QString sql = R"(
        SELECT id, name, startDateTime, endDateTime
        FROM trials
        WHERE name = :name
    )";

    selectQuery.prepare(sql);
    selectQuery.bindValue(":name", name);

    if (!selectQuery.exec() || !selectQuery.next()) {
        qWarning() << "[DB] Error fetching " << name << " trial";
        return std::nullopt;
    }

    const TrialInfo info = {
        .id             = selectQuery.value(0).toInt(),
        .name           = selectQuery.value(1).toString().trimmed(),
        .startDateTime  = selectQuery.value(2).toString().trimmed(),
        .endDateTime    = selectQuery.value(3).toString().trimmed()
    };

    return info;
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
    if (existingGroups.has_value()) {
        for (const auto& g: existingGroups.value()) {
            existingGroupNames.insert(g.name);
        }
    }

    for (const auto& group: groups) {
        if (!existingGroupNames.contains(group)) {
            addGroup(group);
        }
    }

    auto savedGroups = getTrialGroups();
    if (!savedGroups.has_value())
        return {};

    return savedGroups.value();
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
    QSqlQuery updateQuery(m_db);
    QString sql = R"(
        UPDATE trials
        SET
            startDateTime = :start,
            endDateTime = NULL
        WHERE 1 = 1
            AND id = :id
    )";

    updateQuery.prepare(sql);
    updateQuery.bindValue(":start", QDateTime::currentDateTime().toString(Qt::ISODate));
    updateQuery.bindValue(":id", m_trialId);

    if (!updateQuery.exec()) {
        qWarning() << "[DB] Error starting trial:" << updateQuery.lastError().text();
        return false;
    }

    QSqlQuery selectQuery(m_db);
    sql = R"(
        SELECT name, startDateTime FROM trials WHERE id=:id
    )";
    selectQuery.prepare(sql);
    selectQuery.bindValue(":id", m_trialId);

    if (!selectQuery.exec() || !selectQuery.next()) {
        qWarning() << "[DB] Error loading trial name:" << selectQuery.lastError().text();
        return false;
    }

    m_activeTrialName = selectQuery.value(0).toString();
    m_activeTrialStartDateTime = selectQuery.value(1).toString();

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

QVector<GroupInfo> TrialController::loadGroupsFromXlsx(const QString& xlsxPath) {
    QXlsx::Document xlsx(xlsxPath);
    
    // When constructed with a path, Document automatically loads the file
    // Check if the document was loaded successfully by checking if we can read from it
    QVariant testRead = xlsx.read(1, 1);
    if (testRead.isNull()) {
        qWarning() << "[XLSX] Failed to load or read file:" << xlsxPath;
        return {};
    }

    // Read trial name from first sheet, cell A1 (assuming format: "Trial: <name>")
    QString trialCell = testRead.toString().trimmed();
    QString trialName;
    
    if (trialCell.startsWith("Trial:", Qt::CaseInsensitive)) {
        trialName = trialCell.mid(6).trimmed(); // Remove "Trial:" prefix
    } else {
        qWarning() << "[XLSX] Trial name not found in expected format (A1 should be 'Trial: <name>')";
        return {};
    }

    // Create or load the trial
    createOrLoadTrial(trialName);
    
    // Get existing groups to avoid duplicates
    auto existingGroups = getTrialGroups();
    QSet<QString> existingGroupNames;
    if (existingGroups.has_value()) {
        for (const auto& g: existingGroups.value()) {
            existingGroupNames.insert(g.name);
        }
    }

    // Read groups starting from row 3 (assuming row 2 is header "Groups")
    // Expected format:
    // A1: Trial: <trial_name>
    // A2: Groups
    // A3: Group1
    // A4: Group2
    // ...
    
    int row = 3;
    QVector<QString> newGroups;
    
    while (true) {
        QVariant cellValue = xlsx.read(row, 1);
        if (cellValue.isNull() || cellValue.toString().trimmed().isEmpty()) {
            break; // Stop when we hit an empty cell
        }
        
        QString groupName = cellValue.toString().trimmed();
        if (!groupName.isEmpty() && !existingGroupNames.contains(groupName)) {
            addGroup(groupName);
            newGroups.append(groupName);
        }
        row++;
    }

    qDebug() << "[XLSX] Loaded" << newGroups.size() << "new groups from" << xlsxPath;

    // Return all groups for this trial
    auto savedGroups = getTrialGroups();
    if (!savedGroups.has_value())
        return {};

    return savedGroups.value();
}

