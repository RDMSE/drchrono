#include "modalitiesrepository.h"
#include <QSqlQuery>
#include <QSqlError>

Modalities::Repository::Repository(QSqlDatabase db) : m_db(db) {
    auto value = createModalitiesTable();
    if (!value) {
        qFatal("Modalities table creation failed: %s", value.error().toLocal8Bit().constData());
    }
}

tl::expected<void, QString> Modalities::Repository::createModalitiesTable() {
    QSqlQuery query(m_db);

    const QString sql = R"(
        CREATE TABLE IF NOT EXISTS modalities (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE
        );
    )";

    if (!query.exec(sql)) {
        return tl::unexpected("[CR] Error creating modalities:"  + query.lastError().text());
    }

    return {};
}

tl::expected<Modalities::Modality, QString> Modalities::Repository::createModality(const QString& name) {
    if (name.isEmpty()) {
        return tl::unexpected("[CR]: invalid name");
    }

    QSqlQuery queryInsert(m_db);
    QString sql = R"(
        INSERT OR IGNORE INTO modalities(name) VALUES(:name)
    )";

    queryInsert.prepare(sql);
    queryInsert.bindValue(":name", name.trimmed());

    if (!queryInsert.exec()) {
        return tl::unexpected("[CR]: Error inserting " + name + " into modalities table. Error: " + queryInsert.lastError().text());
    }

    sql = R"(
        SELECT
            id,
            name
        FROM modalities
        WHERE name = :name
    )";

    QSqlQuery querySelect(m_db);
    querySelect.prepare(sql);
    querySelect.bindValue(":name", name.trimmed());

    if (!querySelect.exec() || !querySelect.next()) {
        return tl::unexpected("[CR]: Error fetching '" + name + "' from modalities table. Error: " + querySelect.lastError().text());
    }

    return (Modalities::Modality) {
        .id = querySelect.value(0).toInt(),
        .name = querySelect.value(1).toString()
    };
}

tl::expected<Modalities::Modality, QString> Modalities::Repository::getModalityById(const int id) {
    const QString sql = R"(
        SELECT
            name
        FROM modalities
        WHERE id = :id
    )";

    QSqlQuery querySelect(m_db);
    querySelect.prepare(sql);
    querySelect.bindValue(":id", id);

    if (!querySelect.exec() || !querySelect.next()) {
        return tl::unexpected("[CR]: Error fetching '" + QString::number(id) + "' from modalities table. Error: " + querySelect.lastError().text());
    }

    return (Modalities::Modality) {
        .id = id,
        .name = querySelect.value(0).toString()
    };

}

tl::expected<Modalities::Modality, QString> Modalities::Repository::getModalityByName(const QString& name) {
    QSqlQuery querySelect(m_db);
    const QString sql = R"(
        SELECT
            id
        FROM modalities
        WHERE name = :name
    )";

    querySelect.prepare(sql);
    querySelect.bindValue(":name", name);

    if (!querySelect.exec()) {
        return tl::unexpected("[CR]: Error fetching '" + name + "' from modalities table. Error: " + querySelect.lastError().text());
    }

    if (querySelect.next()) {
        return (Modalities::Modality) {
            .id = querySelect.value(0).toInt(),
            .name = name
        };
    }

    return {};
}

tl::expected<QVector<Modalities::Modality>, QString> Modalities::Repository::getAllModalities() {
    QSqlQuery querySelect(m_db);
    const QString sql = R"(
        SELECT
            id,
            name
        FROM modalities
    )";

    if (!querySelect.exec(sql)) {
        return tl::unexpected("[CR]: Error fetching registers from modalities table. Error: " + querySelect.lastError().text());
    }

    QVector<Modalities::Modality> results;
    while (querySelect.next()) {
        results.push_back({
            .id = querySelect.value(0).toInt(),
            .name = querySelect.value(1).toString()
        });
    }

    return results;
}
tl::expected<Modalities::Modality, QString> Modalities::Repository::updateModalityById(const int id, const Modalities::Modality& modality) {
    if (modality.name.isEmpty()) {
        return tl::unexpected("[CR]: invalid name");
    }

    QSqlQuery queryUpdate(m_db);
    const QString sql = R"(
        UPDATE modalities
        SET name = :name
        WHERE id = :id
    )";

    queryUpdate.prepare(sql);
    queryUpdate.bindValue(":name", modality.name);
    queryUpdate.bindValue(":id", id);

    if (!queryUpdate.exec()) {
        return tl::unexpected("[CR]: Error updating '" + QString::number(id) + "'entry on modalities table. Error: " + queryUpdate.lastError().text());
    }

    return (Modalities::Modality) {
        .id = id,
        .name = modality.name
    };
}

tl::expected<int, QString> Modalities::Repository::deleteModalityById(const int id) {
    QSqlQuery queryUpdate(m_db);
    const QString sql = R"(
        DELETE FROM modalities
        WHERE id = :id
    )";

    queryUpdate.prepare(sql);
    queryUpdate.bindValue(":id", id);

    if (!queryUpdate.exec()) {
        return tl::unexpected("[CR]: Error updating '" + QString::number(id) + "'entry on modalities table. Error: " + queryUpdate.lastError().text());
    }

    return id;
}

tl::expected<int, QString> Modalities::Repository::deleteModalityByName(const QString& name) {
    auto modality = getModalityByName(name);
    if (!modality.has_value())
        return tl::unexpected(modality.error());


    auto id = deleteModalityById(modality.value().id);
    if (!id.has_value()) {
        return tl::unexpected(id.error());
    }
    return id;
}
