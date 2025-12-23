#ifndef MODALITIESREPOSITORY_H
#define MODALITIESREPOSITORY_H

#include "modality.h"
#include <QSqlDatabase>
#include <QString>
#include <tl/expected.hpp>

namespace Modalities {

class Repository
{
public:
    explicit Repository(const QSqlDatabase& db);

    [[nodiscard]] tl::expected<Modality, QString> createModality(const QString& name) const;
    [[nodiscard]] tl::expected<Modality, QString> getModalityById(int id) const;
    [[nodiscard]] tl::expected<Modality, QString> getModalityByName(const QString& name) const;
    [[nodiscard]] tl::expected<QVector<Modality>, QString> getAllModalities() const;
    [[nodiscard]] tl::expected<Modality, QString> updateModalityById(int id, const Modality& modality) const;
    [[nodiscard]] tl::expected<int, QString> deleteModalityById(int id) const;
    [[nodiscard]] tl::expected<int, QString> deleteModalityByName(const QString& name) const;

private:
    QSqlDatabase m_db;
    [[nodiscard]] tl::expected<void, QString> createModalitiesTable() const;
};

};


#endif // MODALITIESREPOSITORY_H
