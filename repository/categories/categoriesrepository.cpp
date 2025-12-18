#include "categoriesrepository.h"
#include <QSqlQuery>
#include <QSqlError>

Categories::Repository::Repository(QSqlDatabase db) : m_db(db) {
    auto value = createCategoriesTable();
    if (!value) {
        qFatal("Categories table creation failed: %s", value.error().toLocal8Bit().constData());
    }
}

tl::expected<void, QString> Categories::Repository::createCategoriesTable() {
    QSqlQuery query(m_db);

    const QString sql = R"(
        CREATE TABLE IF NOT EXISTS categories (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE
        );
    )";

    if (!query.exec(sql)) {
        return tl::unexpected("[CR] Error creating categories:"  + query.lastError().text());
    }

    return {};
}

tl::expected<Categories::Category, QString> Categories::Repository::createCategory(const QString& name) {
    if (name.isEmpty()) {
        return tl::unexpected("[CR]: invalid name");
    }

    QSqlQuery queryInsert(m_db);
    QString sql = R"(
        INSERT OR IGNORE INTO categories(name) VALUES(:name)
    )";

    queryInsert.prepare(sql);
    queryInsert.bindValue(":name", name.trimmed());

    if (!queryInsert.exec()) {
        return tl::unexpected("[CR]: Error inserting " + name + " into categories table. Error: " + queryInsert.lastError().text());
    }

    sql = R"(
        SELECT
            id,
            name
        FROM categories
        WHERE name = :name
    )";

    QSqlQuery querySelect(m_db);
    querySelect.prepare(sql);
    querySelect.bindValue(":name", name.trimmed());

    if (!querySelect.exec() || !querySelect.next()) {
        return tl::unexpected("[CR]: Error fetching '" + name + "' from categories table. Error: " + querySelect.lastError().text());
    }

    return (Categories::Category) {
        .id = querySelect.value(0).toInt(),
        .name = querySelect.value(1).toString()
    };
}

tl::expected<Categories::Category, QString> Categories::Repository::getCategoryById(const int id) {
    const QString sql = R"(
        SELECT
            name
        FROM categories
        WHERE id = :id
    )";

    QSqlQuery querySelect(m_db);
    querySelect.prepare(sql);
    querySelect.bindValue(":id", id);

    if (!querySelect.exec() || !querySelect.next()) {
        return tl::unexpected("[CR]: Error fetching '" + QString::number(id) + "' from categories table. Error: " + querySelect.lastError().text());
    }

    return (Categories::Category) {
        .id = id,
        .name = querySelect.value(0).toString()
    };

}

tl::expected<Categories::Category, QString> Categories::Repository::getCategoryByName(const QString& name) {
    QSqlQuery querySelect(m_db);
    const QString sql = R"(
        SELECT
            id
        FROM categories
        WHERE name = :name
    )";

    querySelect.prepare(sql);
    querySelect.bindValue(":name", name);

    if (!querySelect.exec() || !querySelect.next()) {
        return tl::unexpected("[CR]: Error fetching '" + name + "' from categories table. Error: " + querySelect.lastError().text());
    }

    return (Categories::Category) {
        .id = querySelect.value(0).toInt(),
        .name = name
    };
}

tl::expected<QVector<Categories::Category>, QString> Categories::Repository::getAllCategories() {
    QSqlQuery querySelect(m_db);
    const QString sql = R"(
        SELECT
            id,
            name
        FROM categories
    )";

    if (!querySelect.exec(sql)) {
        return tl::unexpected("[CR]: Error fetching registers from categories table. Error: " + querySelect.lastError().text());
    }

    QVector<Categories::Category> results;
    while (querySelect.next()) {
        results.push_back({
            .id = querySelect.value(0).toInt(),
            .name = querySelect.value(1).toString()
        });
    }

    return results;
}
tl::expected<Categories::Category, QString> Categories::Repository::updateCategoryById(const int id, const Categories::Category& category) {
    if (category.name.isEmpty()) {
        return tl::unexpected("[CR]: invalid name");
    }

    QSqlQuery queryUpdate(m_db);
    const QString sql = R"(
        UPDATE categories
        SET name = :name
        WHERE id = :id
    )";

    queryUpdate.prepare(sql);
    queryUpdate.bindValue(":name", category.name);
    queryUpdate.bindValue(":id", id);

    if (!queryUpdate.exec()) {
        return tl::unexpected("[CR]: Error updating '" + QString::number(id) + "'entry on categories table. Error: " + queryUpdate.lastError().text());
    }

    return category;
}

tl::expected<int, QString> Categories::Repository::deleteCategoryById(const int id) {
    QSqlQuery queryUpdate(m_db);
    const QString sql = R"(
        DELETE FROM categories
        WHERE id = :id
    )";

    queryUpdate.prepare(sql);
    queryUpdate.bindValue(":id", id);

    if (!queryUpdate.exec()) {
        return tl::unexpected("[CR]: Error updating '" + QString::number(id) + "'entry on categories table. Error: " + queryUpdate.lastError().text());
    }

    return id;
}

tl::expected<int, QString> Categories::Repository::deleteCategoryByName(const QString& name) {
    auto category = getCategoryByName(name);
    if (!category.has_value())
        return tl::unexpected(category.error());


    auto id = deleteCategoryById(category.value().id);
    if (!id.has_value()) {
        return tl::unexpected(id.error());
    }
    return id;
}
