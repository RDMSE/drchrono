#ifndef MODALITIESREPOSITORY_H
#define MODALITIESREPOSITORY_H

#include "modality.h"
#include <QSqlDatabase>
#include <QString>
#include <tl/expected.hpp>
#include <QVector>

namespace Modalities {

class Repository
{
public:
    explicit Repository(QSqlDatabase db);

    tl::expected<Modalities::Modality, QString> createModality(const QString& name);
    tl::expected<Modalities::Modality, QString> getModalityById(const int id);
    tl::expected<Modalities::Modality, QString> getModalityByName(const QString& name);
    tl::expected<QVector<Modalities::Modality>, QString> getAllModalities();
    tl::expected<Modalities::Modality, QString> updateModalityById(const int id, const Modalities::Modality& modality);
    tl::expected<int, QString> deleteModalityById(const int id);
    tl::expected<int, QString> deleteModalityByName(const QString& name);

private:
    QSqlDatabase m_db;
    tl::expected<void, QString> createModalitiesTable();
};

};


#endif // MODALITIESREPOSITORY_H
