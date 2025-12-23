#pragma once
#include "trialinfo.h"
#include <QSqlDatabase>
#include <QString>
#include <tl/expected.hpp>


namespace Trials {

class Repository
{
public:
    explicit Repository(const QSqlDatabase& db);
    [[nodiscard]] tl::expected<TrialInfo, QString> createTrial(const QString& name, const QDateTime& schedTime) const;
    [[nodiscard]] tl::expected<TrialInfo, QString> getTrialById(int id) const;
    [[nodiscard]] tl::expected<TrialInfo, QString> getTrialByName(const QString& name) const;
    [[nodiscard]] tl::expected<QVector<TrialInfo>, QString> getAllTrials() const;
    [[nodiscard]] tl::expected<QVector<TrialInfo>, QString> getTrialsByDate(const QDate& date) const;
    [[nodiscard]] tl::expected<std::optional<TrialInfo>, QString> getRunningTrial(int openTrialWindowDays) const;
    [[nodiscard]] tl::expected<TrialInfo, QString> updateTrialById(int id, const TrialInfo& trial) const;
    [[nodiscard]] tl::expected<int, QString> deleteTrialById(int id) const;
    [[nodiscard]] tl::expected<int, QString> deleteTrialByName(const QString& name) const;

private:
    QSqlDatabase m_db;
    [[nodiscard]] tl::expected<void, QString> createTrialsTable() const;

};

} // namespace Trials

