#ifndef CATEGORIESREPOSITORY_H
#define CATEGORIESREPOSITORY_H

#include "category.h"
#include <QSqlDatabase>
#include <QString>
#include <tl/expected.hpp>
#include <QVector>

namespace Categories {

class Repository
{
public:
    explicit Repository(QSqlDatabase db);

    tl::expected<Categories::Category, QString> createCategory(const QString& name);
    tl::expected<Categories::Category, QString> getCategoryById(const int id);
    tl::expected<Categories::Category, QString> getCategoryByName(const QString& name);
    tl::expected<QVector<Categories::Category>, QString> getAllCategories();
    tl::expected<Categories::Category, QString> updateCategoryById(const int id, const Categories::Category& category);
    tl::expected<int, QString> deleteCategoryById(const int id);
    tl::expected<int, QString> deleteCategoryByName(const QString& name);

private:
    QSqlDatabase m_db;
    tl::expected<void, QString> createCategoriesTable();
};


};


#endif // CATEGORIESREPOSITORY_H
