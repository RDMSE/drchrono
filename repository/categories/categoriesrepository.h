#ifndef CATEGORIESREPOSITORY_H
#define CATEGORIESREPOSITORY_H

#include "category.h"
#include <QSqlDatabase>
#include <QString>
#include <tl/expected.hpp>

namespace Categories {

class Repository
{
public:
    explicit Repository(const QSqlDatabase& db);

    [[nodiscard]] tl::expected<Category, QString> createCategory(const QString& name) const;
    [[nodiscard]] tl::expected<Category, QString> getCategoryById(int id) const;
    [[nodiscard]] tl::expected<Category, QString> getCategoryByName(const QString& name) const;
    [[nodiscard]] tl::expected<QVector<Category>, QString> getAllCategories() const;
    [[nodiscard]] tl::expected<Category, QString> updateCategoryById(int id, const Category& category) const;
    [[nodiscard]] tl::expected<int, QString> deleteCategoryById(int id) const;
    [[nodiscard]] tl::expected<int, QString> deleteCategoryByName(const QString& name) const;

private:
    QSqlDatabase m_db;
    [[nodiscard]] tl::expected<void, QString> createCategoriesTable() const;
};


};


#endif // CATEGORIESREPOSITORY_H
