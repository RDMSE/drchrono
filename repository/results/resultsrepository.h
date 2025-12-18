#ifndef RESULTSREPOSITORY_H
#define RESULTSREPOSITORY_H

#include "result.h"
#include <QSqlDatabase>
#include <QString>
#include <QDateTime>
#include <tl/expected.hpp>
#include <QVector>

namespace Results {

class Repository
{
public:
    explicit Repository(QSqlDatabase db);

    tl::expected<Results::Result, QString> createResult(
        int registrationId, 
        const QDateTime& startTime, 
        const QDateTime& endTime, 
        int durationMs, 
        const QString& notes = ""
    );
    tl::expected<Results::Result, QString> getResultById(const int id);
    tl::expected<Results::Result, QString> getResultByRegistration(int registrationId);
    tl::expected<QVector<Results::Result>, QString> getResultsByTrial(int trialId);
    tl::expected<QVector<Results::Result>, QString> getAllResults();
    tl::expected<Results::Result, QString> updateResultById(const int id, const Results::Result& result);
    tl::expected<int, QString> deleteResultById(const int id);
    tl::expected<int, QString> deleteResultsByRegistration(const int registrationId);

private:
    QSqlDatabase m_db;
    tl::expected<void, QString> createResultsTable();
};

};

#endif // RESULTSREPOSITORY_H