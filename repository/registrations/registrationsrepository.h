#pragma once

#include "registration.h"
#include <QSqlDatabase>
#include <QString>
#include <tl/expected.hpp>

namespace Registrations {

class Repository
{
public:
    explicit Repository(const QSqlDatabase& db);

    [[nodiscard]] tl::expected<Registration, QString> createRegistration(
        int trialId, 
        int athleteId, 
        const QString& plateCode, 
        int modalityId, 
        int categoryId
    ) const;
    [[nodiscard]] tl::expected<Registration, QString> getRegistrationById(int id) const;
    [[nodiscard]] tl::expected<Registration, QString> getRegistrationByPlateCode(int trialId, const QString& plateCode) const;
    [[nodiscard]] tl::expected<QVector<Registration>, QString> getRegistrationsByTrial(int trialId) const;
    [[nodiscard]] tl::expected<QVector<Registration>, QString> getAllRegistrations() const;
    [[nodiscard]] tl::expected<Registration, QString> updateRegistrationById(int id, const Registration& registration) const;
    [[nodiscard]] tl::expected<int, QString> deleteRegistrationById(int id) const;
    [[nodiscard]] tl::expected<int, QString> deleteRegistrationsByTrial(int trialId) const;

private:
    QSqlDatabase m_db;
    [[nodiscard]] tl::expected<void, QString> createRegistrationsTable() const;
};

};
