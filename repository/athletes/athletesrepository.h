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
    explicit Repository(const QSqlDatabase& db);

    [[nodiscard]] tl::expected<Athlete, QString> createAthlete(const QString& name) const;
    [[nodiscard]] tl::expected<Athlete, QString> getAthleteById(int id) const;
    [[nodiscard]] tl::expected<Athlete, QString> getAthleteByName(const QString& name) const;
    [[nodiscard]] tl::expected<QVector<Athlete>, QString> getAllAthletes() const;
    [[nodiscard]] tl::expected<Athlete, QString> updateAthleteById(int id, const Athlete& athlete) const;
    [[nodiscard]] tl::expected<int, QString> deleteAthleteById(int id) const;
    [[nodiscard]] tl::expected<int, QString> deleteAthleteByName(const QString& name) const;

private:
    QSqlDatabase m_db;
    [[nodiscard]] tl::expected<void, QString> createAthletesTable() const;
};

};

#endif // ATHLETESREPOSITORY_H
