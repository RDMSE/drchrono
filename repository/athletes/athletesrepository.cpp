#include "athletesrepository.h"
#include <QSqlQuery>
#include <QSqlError>

Athletes::Repository::Repository(QSqlDatabase db) : m_db(db) {
    auto value = createAthletesTable();
    if (!value) {
        qFatal("Athletes table creation failed: %s", value.error().toLocal8Bit().constData());
    }
}

tl::expected<void, QString> Athletes::Repository::createAthletesTable() {
    QSqlQuery query(m_db);

    const QString sql = R"(
        CREATE TABLE IF NOT EXISTS athletes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL
        );
    )";

    if (!query.exec(sql)) {
        return tl::unexpected("[AR] Error creating athletes:"  + query.lastError().text());
    }

    return {};
}

tl::expected<Athletes::Athlete, QString> Athletes::Repository::createAthlete(const QString& name) {
    if (name.isEmpty()) {
        return tl::unexpected("[AR]: invalid name");
    }

    QSqlQuery queryInsert(m_db);
    QString sql = R"(
        INSERT OR IGNORE INTO athletes(name) VALUES(:name)
    )";

    queryInsert.prepare(sql);
    queryInsert.bindValue(":name", name.trimmed());

    if (!queryInsert.exec()) {
        return tl::unexpected("[AR]: Error inserting " + name + " into athletes table. Error: " + queryInsert.lastError().text());
    }

    sql = R"(
        SELECT
            id,
            name
        FROM athletes
        WHERE name = :name
    )";

    QSqlQuery querySelect(m_db);
    querySelect.prepare(sql);
    querySelect.bindValue(":name", name.trimmed());

    if (!querySelect.exec() || !querySelect.next()) {
        return tl::unexpected("[AR]: Error fetching '" + name + "' from athletes table. Error: " + querySelect.lastError().text());
    }

    return (Athletes::Athlete) {
        .id = querySelect.value(0).toInt(),
        .name = querySelect.value(1).toString()
    };
}

tl::expected<Athletes::Athlete, QString> Athletes::Repository::getAthleteById(const int id) {

    const QString sql = R"(
        SELECT
            name
        FROM athletes
        WHERE id = :id
    )";

    QSqlQuery querySelect(m_db);
    querySelect.prepare(sql);
    querySelect.bindValue(":id", id);

    if (!querySelect.exec() || !querySelect.next()) {
        return tl::unexpected("[AR]: Error fetching '" + QString::number(id) + "' from athletes table. Error: " + querySelect.lastError().text());
    }

    return (Athletes::Athlete) {
        .id = id,
        .name = querySelect.value(0).toString()
    };
}

tl::expected<Athletes::Athlete, QString> Athletes::Repository::getAthleteByName(const QString& name) {
    QSqlQuery querySelect(m_db);
    const QString sql = R"(
        SELECT
            id
        FROM athletes
        WHERE name = :name
    )";

    querySelect.prepare(sql);
    querySelect.bindValue(":name", name);

    if (!querySelect.exec()) {
        return tl::unexpected("[AR]: Error fetching '" + name + "' from athletes table. Error: " + querySelect.lastError().text());
    }

    if (querySelect.next()) {
        return (Athletes::Athlete) {
            .id = querySelect.value(0).toInt(),
            .name = name
        };
    }

    return {};
}

tl::expected<QVector<Athletes::Athlete>, QString> Athletes::Repository::getAllAthletes() {
    QSqlQuery querySelect(m_db);
    const QString sql = R"(
        SELECT
            id,
            name
        FROM athletes
    )";


    if (!querySelect.exec(sql)) {
        return tl::unexpected("[AR]: Error fetching registers from athletes table. Error: " + querySelect.lastError().text());
    }

    QVector<Athletes::Athlete> results;
    while (querySelect.next()) {
        results.push_back({
            .id = querySelect.value(0).toInt(),
            .name = querySelect.value(1).toString()
        });
    }

    return results;
}

tl::expected<Athletes::Athlete, QString> Athletes::Repository::updateAthleteById(int id, const Athletes::Athlete& athlete) {

    if (athlete.name.isEmpty()) {
        return tl::unexpected("[AR]: invalid name");
    }

    QSqlQuery queryUpdate(m_db);
    const QString sql = R"(
        UPDATE athletes
        SET name = :name
        WHERE id = :id
    )";

    queryUpdate.prepare(sql);
    queryUpdate.bindValue(":id", id);
    queryUpdate.bindValue(":name", athlete.name);

    if (!queryUpdate.exec()) {
        return tl::unexpected("[AR]: Error updating '" + QString::number(athlete.id) + "'entry on athletes table. Error: " + queryUpdate.lastError().text());
    }

    return athlete;
}

tl::expected<int, QString> Athletes::Repository::deleteAthleteById(const int id) {
    QSqlQuery queryUpdate(m_db);
    const QString sql = R"(
        DELETE FROM athletes
        WHERE id = :id
    )";

    queryUpdate.prepare(sql);
    queryUpdate.bindValue(":id", id);

    if (!queryUpdate.exec()) {
        return tl::unexpected("[AR]: Error updating '" + QString::number(id) + "'entry on athletes table. Error: " + queryUpdate.lastError().text());
    }

    return id;
}


tl::expected<int, QString> Athletes::Repository::deleteAthleteByName(const QString& name) {

    auto athlete = getAthleteByName(name);
    if (!athlete.has_value())
        return tl::unexpected(athlete.error());


    auto id = deleteAthleteById(athlete.value().id);
    if (!id.has_value()) {
        return tl::unexpected(id.error());
    }
    return id;
}


