#ifndef REGISTRATIONSREPOSITORY_H
#define REGISTRATIONSREPOSITORY_H

#include "registration.h"
#include <QSqlDatabase>
#include <QString>
#include <tl/expected.hpp>
#include <QVector>

namespace Registrations {

class Repository
{
public:
    explicit Repository(QSqlDatabase db);

    tl::expected<Registrations::Registration, QString> createRegistration(
        int trialId, 
        int athleteId, 
        const QString& plateCode, 
        int modalityId, 
        int categoryId
    );
    tl::expected<Registrations::Registration, QString> getRegistrationById(const int id);
    tl::expected<Registrations::Registration, QString> getRegistrationByPlateCode(int trialId, const QString& plateCode);
    tl::expected<QVector<Registrations::Registration>, QString> getRegistrationsByTrial(int trialId);
    tl::expected<QVector<Registrations::Registration>, QString> getAllRegistrations();
    tl::expected<Registrations::Registration, QString> updateRegistrationById(const int id, const Registrations::Registration& registration);
    tl::expected<int, QString> deleteRegistrationById(const int id);
    tl::expected<int, QString> deleteRegistrationsByTrial(const int trialId);

private:
    QSqlDatabase m_db;
    tl::expected<void, QString> createRegistrationsTable();
};

};

#endif // REGISTRATIONSREPOSITORY_H