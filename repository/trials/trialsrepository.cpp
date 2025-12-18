#include "trialsrepository.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <algorithm>
#include <optional>
#include "utils/timeutils.h"

namespace Trials {

Trials::Repository::Repository(QSqlDatabase db) : m_db(db) {
    auto value = createTrialsTable();
    if (!value) {
        qFatal("Trials table creation failed: %s", value.error().toLocal8Bit().constData());
    }
}

tl::expected<void, QString> Trials::Repository::createTrialsTable()
{
    QSqlQuery query(m_db);

    const QString sql = R"(
        CREATE TABLE IF NOT EXISTS trials (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            scheduledDateTime TEXT,
            startDateTime TEXT,
            endDateTime TEXT,
            createdAt TEXT NOT NULL DEFAULT (datetime('now'))
        );
    )";

    if (!query.exec(sql)) {
        return tl::unexpected("[TR] Error creating trials:" + query.lastError().text());
    }

    return {};
}

tl::expected<Trials::TrialInfo, QString> Trials::Repository::createTrial(const QString& name, const QDateTime& schedTime) {
    if (name.isEmpty()) {
        return tl::unexpected("[TR]: invalid name");
    }

    QSqlQuery queryInsert(m_db);
    QString sql = R"(
        INSERT OR IGNORE INTO trials(name, scheduledDateTime) VALUES(:name, :schedTime)
    )";

    queryInsert.prepare(sql);
    queryInsert.bindValue(":name", name.trimmed());
    queryInsert.bindValue(":schedTime", schedTime.toString(Qt::ISODate));

    if (!queryInsert.exec()) {
        return tl::unexpected("[TR]: Error inserting " + name + " into trials table. Error: " + queryInsert.lastError().text());
    }

    sql = R"(
        SELECT
            id,
            name
        FROM trials
        WHERE name = :name
    )";

    QSqlQuery querySelect(m_db);
    querySelect.prepare(sql);
    querySelect.bindValue(":name", name.trimmed());

    if (!querySelect.exec() || !querySelect.next()) {
        return tl::unexpected("[TR]: Error fetching '" + name + "' from trials table. Error: " + querySelect.lastError().text());
    }

    return (Trials::TrialInfo) {
        .id = querySelect.value(0).toInt(),
        .name = querySelect.value(1).toString(),
        .scheduledDateTime = schedTime,
        .startDateTime = Utils::DateTimeUtils::epochZero(),
        .endDateTime = Utils::DateTimeUtils::epochZero()
    };
}

tl::expected<Trials::TrialInfo, QString> Trials::Repository::getTrialById(const int id) {
    const QString sql = R"(
        SELECT
            name,
            scheduledDateTime,
            startDateTime,
            endDateTime
        FROM trials
        WHERE id = :id
    )";

    QSqlQuery querySelect(m_db);
    querySelect.prepare(sql);
    querySelect.bindValue(":id", id);

    if (!querySelect.exec()) {
        return tl::unexpected("[TR]: Error fetching '" + QString::number(id) + "' from trials table. Error: " + querySelect.lastError().text());
    }

    if (querySelect.next()) {
        auto scheduledDateTime = querySelect.value(1).toString();
        auto startDateTime = querySelect.value(2).toString();
        auto endDateTime = querySelect.value(3).toString();

        return (Trials::TrialInfo) {
            .id = id,
            .name = querySelect.value(0).toString(),
            .scheduledDateTime = Utils::DateTimeUtils::fromStringOrDefault(scheduledDateTime),
            .startDateTime = Utils::DateTimeUtils::fromStringOrDefault(startDateTime),
            .endDateTime = Utils::DateTimeUtils::fromStringOrDefault(endDateTime),
        };
    }
    return {};
}

tl::expected<Trials::TrialInfo, QString> Trials::Repository::getTrialByName(const QString& name) {
    QSqlQuery querySelect(m_db);
    const QString sql = R"(
        SELECT
            id,
            scheduledDateTime,
            startDateTime,
            endDateTime
        FROM trials
        WHERE name = :name
    )";

    querySelect.prepare(sql);
    querySelect.bindValue(":name", name.trimmed());

    if (!querySelect.exec()) {
        return tl::unexpected("[TR]: Error fetching '" + name + "' from trials table. Error: " + querySelect.lastError().text());
    }

    if (querySelect.next()) {
        auto scheduledDateTime = querySelect.value(1).toString();
        auto startDateTime = querySelect.value(2).toString();
        auto endDateTime = querySelect.value(3).toString();

        return (Trials::TrialInfo) {
            .id = querySelect.value(0).toInt(),
            .name = name,
            .scheduledDateTime = Utils::DateTimeUtils::fromStringOrDefault(scheduledDateTime),
            .startDateTime = Utils::DateTimeUtils::fromStringOrDefault(startDateTime),
            .endDateTime = Utils::DateTimeUtils::fromStringOrDefault(endDateTime),
        };
    }
    return {};
}

tl::expected<QVector<Trials::TrialInfo>, QString> Trials::Repository::getAllTrials() {
    QSqlQuery querySelect(m_db);
    const QString sql = R"(
        SELECT
            id,
            name,
            scheduledDateTime,
            startDateTime,
            endDateTime
        FROM trials
        ORDER BY scheduledDateTime DESC
    )";

    if (!querySelect.exec(sql)) {
        return tl::unexpected("[TR]: Error fetching registers from trials table. Error: " + querySelect.lastError().text());
    }

    QVector<Trials::TrialInfo> results;
    while (querySelect.next()) {
        auto scheduledDateTime = querySelect.value(2).toString();
        auto startDateTime = querySelect.value(3).toString();
        auto endDateTime = querySelect.value(4).toString();

        results.push_back({
            .id = querySelect.value(0).toInt(),
            .name = querySelect.value(1).toString(),
            .scheduledDateTime = Utils::DateTimeUtils::fromStringOrDefault(scheduledDateTime),
            .startDateTime = Utils::DateTimeUtils::fromStringOrDefault(startDateTime),
            .endDateTime = Utils::DateTimeUtils::fromStringOrDefault(endDateTime),
        });
    }

    return results;
}

tl::expected<QVector<Trials::TrialInfo>, QString> Trials::Repository::getTrialsByDate(const QDate& date) {
    QSqlQuery querySelect(m_db);
    const QString sql = R"(
        SELECT
            id,
            name,
            scheduledDateTime,
            startDateTime,
            endDateTime
        FROM trials
        WHERE DATE(scheduledDateTime) = :date
        ORDER BY scheduledDateTime ASC
    )";

    querySelect.prepare(sql);
    querySelect.bindValue(":date", date.toString(Qt::ISODate));

    if (!querySelect.exec()) {
        return tl::unexpected("[TR]: Error fetching trials by date from trials table. Error: " + querySelect.lastError().text());
    }

    QVector<Trials::TrialInfo> results;
    while (querySelect.next()) {
        auto scheduledDateTime = querySelect.value(2).toString();
        auto startDateTime = querySelect.value(3).toString();
        auto endDateTime = querySelect.value(4).toString();

        results.push_back({
           .id = querySelect.value(0).toInt(),
           .name = querySelect.value(1).toString(),
            .scheduledDateTime = Utils::DateTimeUtils::fromStringOrDefault(scheduledDateTime),
            .startDateTime = Utils::DateTimeUtils::fromStringOrDefault(startDateTime),
            .endDateTime = Utils::DateTimeUtils::fromStringOrDefault(endDateTime),
        });
    }

    return results;
}

tl::expected<Trials::TrialInfo, QString> Trials::Repository::updateTrialById(const int id, const Trials::TrialInfo& trial) {
    QSqlQuery queryUpdate(m_db);
    QString conditional = R"(
        WHERE id = :id
    )";

    QStringList updateFields;
    
    if (Utils::DateTimeUtils::isValid(trial.endDateTime)) {
        updateFields << "endDateTime = :endDateTime";
    }
    if (Utils::DateTimeUtils::isValid(trial.startDateTime)) {
        updateFields << "startDateTime = :startDateTime";
    }
    if (Utils::DateTimeUtils::isValid(trial.scheduledDateTime)) {
        updateFields << "scheduledDateTime = :scheduledDateTime";
    }
    if (!trial.name.isEmpty()) {
        updateFields << "name = :name";
    }

    if (updateFields.isEmpty()) {
        return tl::unexpected("[TR]: invalid update entry");
    }

    QString sql = QString("UPDATE trials SET %1 WHERE id = :id")
                     .arg(updateFields.join(", "));

    qDebug() << "[TR] Generated SQL:" << sql;
    queryUpdate.prepare(sql);
    
    if (Utils::DateTimeUtils::isValid(trial.endDateTime)) {
        queryUpdate.bindValue(":endDateTime", trial.endDateTime.toString(Qt::ISODate));
        qDebug() << "[TR] Binding endDateTime:" << trial.endDateTime.toString(Qt::ISODate);
    }
    if (Utils::DateTimeUtils::isValid(trial.startDateTime)) {
        queryUpdate.bindValue(":startDateTime", trial.startDateTime.toString(Qt::ISODate));
        qDebug() << "[TR] Binding startDateTime:" << trial.startDateTime.toString(Qt::ISODate);
    }
    if (Utils::DateTimeUtils::isValid(trial.scheduledDateTime)) {
        queryUpdate.bindValue(":scheduledDateTime", trial.scheduledDateTime.toString(Qt::ISODate));
        qDebug() << "[TR] Binding scheduledDateTime:" << trial.scheduledDateTime.toString(Qt::ISODate);
    }
    if (!trial.name.isEmpty()) {
        queryUpdate.bindValue(":name", trial.name);
        qDebug() << "[TR] Binding name:" << trial.name;
    }

    queryUpdate.bindValue(":id", id);
    qDebug() << "[TR] Binding id:" << id;

    if (!queryUpdate.exec()) {
        qDebug() << "[TR] SQL execution failed. Last query:" << queryUpdate.lastQuery();
        qDebug() << "[TR] Bound values:" << queryUpdate.boundValues();
        return tl::unexpected("[TR]: Error updating '" + QString::number(id) + "' entry on trials table. Error: " + queryUpdate.lastError().text());
    }

    return getTrialById(id);
}

tl::expected<int, QString> Trials::Repository::deleteTrialById(const int id) {
    QSqlQuery queryUpdate(m_db);
    const QString sql = R"(
        DELETE FROM trials
        WHERE id = :id
    )";

    queryUpdate.prepare(sql);
    queryUpdate.bindValue(":id", id);

    if (!queryUpdate.exec()) {
        return tl::unexpected("[TR]: Error updating '" + QString::number(id) + "'entry on trials table. Error: " + queryUpdate.lastError().text());
    }

    return id;
}

tl::expected<int, QString> Trials::Repository::deleteTrialByName(const QString& name) {
    auto modality = getTrialByName(name);
    if (!modality.has_value())
        return tl::unexpected(modality.error());


    auto id = deleteTrialById(modality.value().id);
    if (!id.has_value()) {
        return tl::unexpected(id.error());
    }
    return id;
}


tl::expected<std::optional<Trials::TrialInfo>, QString> Trials::Repository::getRunningTrial(const int openTrialWindowDays) {
    // search all trials
    auto allTrialsResult = getAllTrials();
    if (!allTrialsResult.has_value()) {
        return tl::unexpected("[TR]: Error fetching all trials: " + allTrialsResult.error());
    }
    
    const auto& allTrials = allTrialsResult.value();
    
    // Limit date to consider a trial "within the window"
    QDate limitDate = QDate::currentDate().addDays(openTrialWindowDays);
    QDateTime epochZero = Utils::DateTimeUtils::epochZero();
    
    // Filter using STL algorithms
    auto runningTrialIt = std::find_if(allTrials.begin(), allTrials.end(), 
        [&limitDate, &epochZero](const Trials::TrialInfo& trial) {
            // Check if the trial meets the criteria:
            // 1. scheduledDateTime >= limitDate
            // 2. endDateTime == epoch zero (not finished)
            // 3. startDateTime != epoch zero (already started)
            return trial.scheduledDateTime.date() <= limitDate &&
                   trial.endDateTime == epochZero &&
                   trial.startDateTime != epochZero;
        });
    
    if (runningTrialIt != allTrials.end()) {
        return std::make_optional(*runningTrialIt);
    }
    
    return std::nullopt;
}

} // namespace Trials
