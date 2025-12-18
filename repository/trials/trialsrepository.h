#ifndef TRIALSREPOSITORY_H
#define TRIALSREPOSITORY_H

#include "trialinfo.h"
#include <QSqlDatabase>
#include <QString>
#include <tl/expected.hpp>
#include <QVector>


namespace Trials {

class Repository
{
public:
    explicit Repository(QSqlDatabase db);
    tl::expected<Trials::TrialInfo, QString> createTrial(const QString& name, const QDateTime& schedTime);
    tl::expected<Trials::TrialInfo, QString> getTrialById(const int id);
    tl::expected<Trials::TrialInfo, QString> getTrialByName(const QString& name);
    tl::expected<QVector<Trials::TrialInfo>, QString> getAllTrials();
    tl::expected<QVector<Trials::TrialInfo>, QString> getTrialsByDate(const QDate& date);
    tl::expected<QVector<Trials::TrialInfo>, QString> getTrialsAfter(const QDate& date);
    tl::expected<std::optional<Trials::TrialInfo>, QString> getRunningTrial(const int openTrialWindowDays);
    tl::expected<Trials::TrialInfo, QString> updateTrialById(const int id, const Trials::TrialInfo& trial);
    tl::expected<int, QString> deleteTrialById(const int id);
    tl::expected<int, QString> deleteTrialByName(const QString& name);

private:
    QSqlDatabase m_db;
    tl::expected<void, QString> createTrialsTable();

};

} // namespace Trials

#endif // TRIALSREPOSITORY_H
