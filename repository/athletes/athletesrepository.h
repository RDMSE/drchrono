#ifndef ATHLETESREPOSITORY_H
#define ATHLETESREPOSITORY_H

#include <QSqlDatabase>
#include <QString>
#include <tl/expected.hpp>
#include "athlete.h"
#include <QVector>

namespace Athletes {

class Repository
{
public:
    explicit Repository(QSqlDatabase db);

    tl::expected<Athletes::Athlete, QString> createAthlete(const QString& name);
    tl::expected<Athletes::Athlete, QString> getAthleteById(const int id);
    tl::expected<Athletes::Athlete, QString> getAthleteByName(const QString& name);
    tl::expected<QVector<Athletes::Athlete>, QString> getAllAthletes();
    tl::expected<Athletes::Athlete, QString> updateAthleteById(int id, const Athletes::Athlete& athlete);
    tl::expected<int, QString> deleteAthleteById(const int id);
    tl::expected<int, QString> deleteAthleteByName(const QString& name);

private:
    QSqlDatabase m_db;
    tl::expected<void, QString> createAthletesTable();
};

};

#endif // ATHLETESREPOSITORY_H
