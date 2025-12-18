#include "resultsrepository.h"
#include <QSqlQuery>
#include <QSqlError>

Results::Repository::Repository(QSqlDatabase db) : m_db(db) {
    auto value = createResultsTable();
    if (!value) {
        qFatal("Results table creation failed: %s", value.error().toLocal8Bit().constData());
    }
}

tl::expected<void, QString> Results::Repository::createResultsTable() {
    QSqlQuery query(m_db);

    QString sql = R"(
        CREATE TABLE IF NOT EXISTS results (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            registrationId INTEGER NOT NULL,
            startTime TEXT NOT NULL,
            endTime TEXT,
            durationMs INTEGER,
            notes TEXT,

            FOREIGN KEY (registrationId)
                REFERENCES registrations(id)
                ON DELETE CASCADE
        );
    )";

    if (!query.exec(sql)) {
        return tl::unexpected("[ResR] Error creating results: " + query.lastError().text());
    }

    sql = R"(
        CREATE INDEX IF NOT EXISTS idx_results_registration ON results(registrationId);
    )";

    if (!query.exec(sql)) {
        return tl::unexpected("[ResR] Error creating index idx_results_registration: " + query.lastError().text());
    }

    return {};
}

tl::expected<Results::Result, QString> Results::Repository::createResult(
    int registrationId, 
    const QDateTime& startTime, 
    const QDateTime& endTime, 
    int durationMs, 
    const QString& notes) {

    if (!startTime.isValid()) {
        return tl::unexpected("[ResR] Invalid start time");
    }

    QSqlQuery queryInsert(m_db);
    QString sql = R"(
        INSERT INTO results(registrationId, startTime, endTime, durationMs, notes) 
        VALUES(:registrationId, :startTime, :endTime, :durationMs, :notes)
    )";

    queryInsert.prepare(sql);
    queryInsert.bindValue(":registrationId", registrationId);
    queryInsert.bindValue(":startTime", startTime.toString(Qt::ISODate));
    queryInsert.bindValue(":endTime", endTime.isValid() ? endTime.toString(Qt::ISODate) : QVariant());
    queryInsert.bindValue(":durationMs", durationMs);
    queryInsert.bindValue(":notes", notes);

    if (!queryInsert.exec()) {
        return tl::unexpected("[ResR] Error inserting result: " + queryInsert.lastError().text());
    }

    int newId = queryInsert.lastInsertId().toInt();
    
    return (Results::Result) {
        .id = newId,
        .registrationId = registrationId,
        .startTime = startTime,
        .endTime = endTime,
        .durationMs = durationMs,
        .notes = notes
    };
}

tl::expected<Results::Result, QString> Results::Repository::getResultById(const int id) {
    const QString sql = R"(
        SELECT registrationId, startTime, endTime, durationMs, notes
        FROM results
        WHERE id = :id
    )";

    QSqlQuery querySelect(m_db);
    querySelect.prepare(sql);
    querySelect.bindValue(":id", id);

    if (!querySelect.exec() || !querySelect.next()) {
        return tl::unexpected("[ResR] Error fetching result with id " + QString::number(id) + ": " + querySelect.lastError().text());
    }

    return (Results::Result) {
        .id = id,
        .registrationId = querySelect.value(0).toInt(),
        .startTime = QDateTime::fromString(querySelect.value(1).toString(), Qt::ISODate),
        .endTime = querySelect.value(2).isNull() ? QDateTime() : QDateTime::fromString(querySelect.value(2).toString(), Qt::ISODate),
        .durationMs = querySelect.value(3).toInt(),
        .notes = querySelect.value(4).toString()
    };
}

tl::expected<Results::Result, QString> Results::Repository::getResultByRegistration(int registrationId) {
    const QString sql = R"(
        SELECT id, startTime, endTime, durationMs, notes
        FROM results
        WHERE registrationId = :registrationId
    )";

    QSqlQuery querySelect(m_db);
    querySelect.prepare(sql);
    querySelect.bindValue(":registrationId", registrationId);

    if (!querySelect.exec() || !querySelect.next()) {
        return tl::unexpected("[ResR] Error fetching result for registration " + QString::number(registrationId) + ": " + querySelect.lastError().text());
    }

    return (Results::Result) {
        .id = querySelect.value(0).toInt(),
        .registrationId = registrationId,
        .startTime = QDateTime::fromString(querySelect.value(1).toString(), Qt::ISODate),
        .endTime = querySelect.value(2).isNull() ? QDateTime() : QDateTime::fromString(querySelect.value(2).toString(), Qt::ISODate),
        .durationMs = querySelect.value(3).toInt(),
        .notes = querySelect.value(4).toString()
    };
}

tl::expected<QVector<Results::Result>, QString> Results::Repository::getResultsByTrial(int trialId) {
    const QString sql = R"(
        SELECT r.id, r.registrationId, r.startTime, r.endTime, r.durationMs, r.notes
        FROM results r
        INNER JOIN registrations reg ON r.registrationId = reg.id
        WHERE reg.trialId = :trialId
        ORDER BY r.durationMs ASC
    )";

    QSqlQuery querySelect(m_db);
    querySelect.prepare(sql);
    querySelect.bindValue(":trialId", trialId);

    if (!querySelect.exec()) {
        return tl::unexpected("[ResR] Error fetching results for trial " + QString::number(trialId) + ": " + querySelect.lastError().text());
    }

    QVector<Results::Result> results;
    while (querySelect.next()) {
        results.push_back({
            .id = querySelect.value(0).toInt(),
            .registrationId = querySelect.value(1).toInt(),
            .startTime = QDateTime::fromString(querySelect.value(2).toString(), Qt::ISODate),
            .endTime = querySelect.value(3).isNull() ? QDateTime() : QDateTime::fromString(querySelect.value(3).toString(), Qt::ISODate),
            .durationMs = querySelect.value(4).toInt(),
            .notes = querySelect.value(5).toString()
        });
    }

    return results;
}

tl::expected<QVector<Results::Result>, QString> Results::Repository::getAllResults() {
    const QString sql = R"(
        SELECT id, registrationId, startTime, endTime, durationMs, notes
        FROM results
        ORDER BY durationMs ASC
    )";

    QSqlQuery querySelect(m_db);
    querySelect.prepare(sql);

    if (!querySelect.exec()) {
        return tl::unexpected("[ResR] Error fetching all results: " + querySelect.lastError().text());
    }

    QVector<Results::Result> results;
    while (querySelect.next()) {
        results.push_back({
            .id = querySelect.value(0).toInt(),
            .registrationId = querySelect.value(1).toInt(),
            .startTime = QDateTime::fromString(querySelect.value(2).toString(), Qt::ISODate),
            .endTime = querySelect.value(3).isNull() ? QDateTime() : QDateTime::fromString(querySelect.value(3).toString(), Qt::ISODate),
            .durationMs = querySelect.value(4).toInt(),
            .notes = querySelect.value(5).toString()
        });
    }

    return results;
}

tl::expected<Results::Result, QString> Results::Repository::updateResultById(const int id, const Results::Result& result) {
    if (!result.startTime.isValid()) {
        return tl::unexpected("[ResR] Invalid start time");
    }

    QSqlQuery queryUpdate(m_db);
    const QString sql = R"(
        UPDATE results
        SET registrationId = :registrationId, startTime = :startTime, endTime = :endTime, 
            durationMs = :durationMs, notes = :notes
        WHERE id = :id
    )";

    queryUpdate.prepare(sql);
    queryUpdate.bindValue(":id", id);
    queryUpdate.bindValue(":registrationId", result.registrationId);
    queryUpdate.bindValue(":startTime", result.startTime.toString(Qt::ISODate));
    queryUpdate.bindValue(":endTime", result.endTime.isValid() ? result.endTime.toString(Qt::ISODate) : QVariant());
    queryUpdate.bindValue(":durationMs", result.durationMs);
    queryUpdate.bindValue(":notes", result.notes);

    if (!queryUpdate.exec()) {
        return tl::unexpected("[ResR] Error updating result " + QString::number(id) + ": " + queryUpdate.lastError().text());
    }

    return result;
}

tl::expected<int, QString> Results::Repository::deleteResultById(const int id) {
    QSqlQuery queryDelete(m_db);
    const QString sql = R"(
        DELETE FROM results
        WHERE id = :id
    )";

    queryDelete.prepare(sql);
    queryDelete.bindValue(":id", id);

    if (!queryDelete.exec()) {
        return tl::unexpected("[ResR] Error deleting result " + QString::number(id) + ": " + queryDelete.lastError().text());
    }

    return id;
}

tl::expected<int, QString> Results::Repository::deleteResultsByRegistration(const int registrationId) {
    QSqlQuery queryDelete(m_db);
    const QString sql = R"(
        DELETE FROM results
        WHERE registrationId = :registrationId
    )";

    queryDelete.prepare(sql);
    queryDelete.bindValue(":registrationId", registrationId);

    if (!queryDelete.exec()) {
        return tl::unexpected("[ResR] Error deleting results for registration " + QString::number(registrationId) + ": " + queryDelete.lastError().text());
    }

    return registrationId;
}