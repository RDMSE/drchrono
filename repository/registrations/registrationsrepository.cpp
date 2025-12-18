#include "registrationsrepository.h"
#include <QSqlQuery>
#include <QSqlError>

Registrations::Repository::Repository(QSqlDatabase db) : m_db(db) {
    auto value = createRegistrationsTable();
    if (!value) {
        qFatal("Registrations table creation failed: %s", value.error().toLocal8Bit().constData());
    }
}

tl::expected<void, QString> Registrations::Repository::createRegistrationsTable() {
    QSqlQuery query(m_db);

    QString sql = R"(
        CREATE TABLE IF NOT EXISTS registrations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            trialId INTEGER NOT NULL,
            athleteId INTEGER NOT NULL,
            plateCode TEXT NOT NULL,
            modalityId INTEGER,
            categoryId INTEGER,

            FOREIGN KEY (trialId) REFERENCES trials(id) ON DELETE CASCADE,
            FOREIGN KEY (athleteId) REFERENCES athletes(id),
            FOREIGN KEY (modalityId) REFERENCES modalities(id),
            FOREIGN KEY (categoryId) REFERENCES categories(id),

            UNIQUE (trialId, plateCode)
        );
    )";

    if (!query.exec(sql)) {
        return tl::unexpected("[RR] Error creating registrations: " + query.lastError().text());
    }

    sql = R"(
        CREATE INDEX IF NOT EXISTS idx_registrations_trial ON registrations(trialId);
    )";

    if (!query.exec(sql)) {
        return tl::unexpected("[RR] Error creating index idx_registrations_trial: " + query.lastError().text());
    }

    return {};
}

tl::expected<Registrations::Registration, QString> Registrations::Repository::createRegistration(
    int trialId, 
    int athleteId, 
    const QString& plateCode, 
    int modalityId, 
    int categoryId) {

    if (plateCode.isEmpty()) {
        return tl::unexpected("[RR] Invalid plate code");
    }

    QSqlQuery queryInsert(m_db);
    QString sql = R"(
        INSERT INTO registrations(trialId, athleteId, plateCode, modalityId, categoryId) 
        VALUES(:trialId, :athleteId, :plateCode, :modalityId, :categoryId)
    )";

    queryInsert.prepare(sql);
    queryInsert.bindValue(":trialId", trialId);
    queryInsert.bindValue(":athleteId", athleteId);
    queryInsert.bindValue(":plateCode", plateCode.trimmed());
    queryInsert.bindValue(":modalityId", modalityId);
    queryInsert.bindValue(":categoryId", categoryId);

    if (!queryInsert.exec()) {
        return tl::unexpected("[RR] Error inserting registration: " + queryInsert.lastError().text());
    }

    int newId = queryInsert.lastInsertId().toInt();
    
    return (Registrations::Registration) {
        .id = newId,
        .trialId = trialId,
        .athleteId = athleteId,
        .plateCode = plateCode.trimmed(),
        .modalityId = modalityId,
        .categoryId = categoryId
    };
}

tl::expected<Registrations::Registration, QString> Registrations::Repository::getRegistrationById(const int id) {
    const QString sql = R"(
        SELECT trialId, athleteId, plateCode, modalityId, categoryId
        FROM registrations
        WHERE id = :id
    )";

    QSqlQuery querySelect(m_db);
    querySelect.prepare(sql);
    querySelect.bindValue(":id", id);

    if (!querySelect.exec() || !querySelect.next()) {
        return tl::unexpected("[RR] Error fetching registration with id " + QString::number(id) + ": " + querySelect.lastError().text());
    }

    return (Registrations::Registration) {
        .id = id,
        .trialId = querySelect.value(0).toInt(),
        .athleteId = querySelect.value(1).toInt(),
        .plateCode = querySelect.value(2).toString(),
        .modalityId = querySelect.value(3).toInt(),
        .categoryId = querySelect.value(4).toInt()
    };
}

tl::expected<Registrations::Registration, QString> Registrations::Repository::getRegistrationByPlateCode(int trialId, const QString& plateCode) {
    const QString sql = R"(
        SELECT id, athleteId, modalityId, categoryId
        FROM registrations
        WHERE trialId = :trialId AND plateCode = :plateCode
    )";

    QSqlQuery querySelect(m_db);
    querySelect.prepare(sql);
    querySelect.bindValue(":trialId", trialId);
    querySelect.bindValue(":plateCode", plateCode);

    if (!querySelect.exec() || !querySelect.next()) {
        return tl::unexpected("[RR] Error fetching registration with plate " + plateCode + ": " + querySelect.lastError().text());
    }

    return (Registrations::Registration) {
        .id = querySelect.value(0).toInt(),
        .trialId = trialId,
        .athleteId = querySelect.value(1).toInt(),
        .plateCode = plateCode,
        .modalityId = querySelect.value(2).toInt(),
        .categoryId = querySelect.value(3).toInt()
    };
}

tl::expected<QVector<Registrations::Registration>, QString> Registrations::Repository::getRegistrationsByTrial(int trialId) {
    const QString sql = R"(
        SELECT id, athleteId, plateCode, modalityId, categoryId
        FROM registrations
        WHERE trialId = :trialId
        ORDER BY plateCode
    )";

    QSqlQuery querySelect(m_db);
    querySelect.prepare(sql);
    querySelect.bindValue(":trialId", trialId);

    if (!querySelect.exec()) {
        return tl::unexpected("[RR] Error fetching registrations for trial " + QString::number(trialId) + ": " + querySelect.lastError().text());
    }

    QVector<Registrations::Registration> results;
    while (querySelect.next()) {
        results.push_back({
            .id = querySelect.value(0).toInt(),
            .trialId = trialId,
            .athleteId = querySelect.value(1).toInt(),
            .plateCode = querySelect.value(2).toString(),
            .modalityId = querySelect.value(3).toInt(),
            .categoryId = querySelect.value(4).toInt()
        });
    }

    return results;
}

tl::expected<QVector<Registrations::Registration>, QString> Registrations::Repository::getAllRegistrations() {
    const QString sql = R"(
        SELECT id, trialId, athleteId, plateCode, modalityId, categoryId
        FROM registrations
        ORDER BY trialId, plateCode
    )";

    QSqlQuery querySelect(m_db);
    querySelect.prepare(sql);

    if (!querySelect.exec()) {
        return tl::unexpected("[RR] Error fetching all registrations: " + querySelect.lastError().text());
    }

    QVector<Registrations::Registration> results;
    while (querySelect.next()) {
        results.push_back({
            .id = querySelect.value(0).toInt(),
            .trialId = querySelect.value(1).toInt(),
            .athleteId = querySelect.value(2).toInt(),
            .plateCode = querySelect.value(3).toString(),
            .modalityId = querySelect.value(4).toInt(),
            .categoryId = querySelect.value(5).toInt()
        });
    }

    return results;
}

tl::expected<Registrations::Registration, QString> Registrations::Repository::updateRegistrationById(const int id, const Registrations::Registration& registration) {
    if (registration.plateCode.isEmpty()) {
        return tl::unexpected("[RR] Invalid plate code");
    }

    QSqlQuery queryUpdate(m_db);
    const QString sql = R"(
        UPDATE registrations
        SET trialId = :trialId, athleteId = :athleteId, plateCode = :plateCode, 
            modalityId = :modalityId, categoryId = :categoryId
        WHERE id = :id
    )";

    queryUpdate.prepare(sql);
    queryUpdate.bindValue(":id", id);
    queryUpdate.bindValue(":trialId", registration.trialId);
    queryUpdate.bindValue(":athleteId", registration.athleteId);
    queryUpdate.bindValue(":plateCode", registration.plateCode.trimmed());
    queryUpdate.bindValue(":modalityId", registration.modalityId);
    queryUpdate.bindValue(":categoryId", registration.categoryId);

    if (!queryUpdate.exec()) {
        return tl::unexpected("[RR] Error updating registration " + QString::number(id) + ": " + queryUpdate.lastError().text());
    }

    return registration;
}

tl::expected<int, QString> Registrations::Repository::deleteRegistrationById(const int id) {
    QSqlQuery queryDelete(m_db);
    const QString sql = R"(
        DELETE FROM registrations
        WHERE id = :id
    )";

    queryDelete.prepare(sql);
    queryDelete.bindValue(":id", id);

    if (!queryDelete.exec()) {
        return tl::unexpected("[RR] Error deleting registration " + QString::number(id) + ": " + queryDelete.lastError().text());
    }

    return id;
}

tl::expected<int, QString> Registrations::Repository::deleteRegistrationsByTrial(const int trialId) {
    QSqlQuery queryDelete(m_db);
    const QString sql = R"(
        DELETE FROM registrations
        WHERE trialId = :trialId
    )";

    queryDelete.prepare(sql);
    queryDelete.bindValue(":trialId", trialId);

    if (!queryDelete.exec()) {
        return tl::unexpected("[RR] Error deleting registrations for trial " + QString::number(trialId) + ": " + queryDelete.lastError().text());
    }

    return trialId;
}