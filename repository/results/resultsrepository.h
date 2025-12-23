#pragma once

#include "result.h"
#include <QSqlDatabase>
#include <QString>
#include <QDateTime>
#include <tl/expected.hpp>

namespace Results {

class Repository
{
public:
    explicit Repository(const QSqlDatabase& db);

    [[nodiscard]] tl::expected<Result, QString> createResult(
        int registrationId, 
        const QDateTime& startTime, 
        const QDateTime& endTime, 
        int durationMs, 
        const QString& notes = ""
    ) const;
    [[nodiscard]] tl::expected<Result, QString> getResultById(int id) const;
    [[nodiscard]] tl::expected<Result, QString> getResultByRegistration(int registrationId) const;
    [[nodiscard]] tl::expected<QVector<Result>, QString> getResultsByTrial(int trialId) const;
    [[nodiscard]] tl::expected<QVector<Result>, QString> getAllResults() const;
    [[nodiscard]] tl::expected<Result, QString> updateResultById(int id, const Result& result) const;
    [[nodiscard]] tl::expected<int, QString> deleteResultById(int id) const;
    [[nodiscard]] tl::expected<int, QString> deleteResultsByRegistration(int registrationId) const;

private:
    QSqlDatabase m_db;
    [[nodiscard]] tl::expected<void, QString> createResultsTable() const;
};

};
