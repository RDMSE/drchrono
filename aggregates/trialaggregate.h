#ifndef TRIALAGGREGATE_H
#define TRIALAGGREGATE_H

#include "trialinfo.h"
#include "athlete.h"
#include "category.h"
#include "modality.h"
#include "registration.h"
#include "result.h"
#include "repository/athletes/athletesrepository.h"
#include "repository/categories/categoriesrepository.h"
#include "repository/modalities/modalitiesrepository.h"
#include "repository/trials/trialsrepository.h"
#include "repository/registrations/registrationsrepository.h"
#include "repository/results/resultsrepository.h"
#include <QString>
#include <QVector>
#include <QDateTime>
#include <memory>
#include <optional>
#include <QSqlDatabase>
#include <tl/expected.hpp>

namespace Aggregates {

struct RegistrationDetail {
    Registrations::Registration registration;
    Athletes::Athlete athlete;
    Categories::Category category;
    Modalities::Modality modality;
    std::optional<Results::Result> result;
};

struct TrialSummary {
    Trials::TrialInfo trial;
    QVector<RegistrationDetail> registrations;
    int totalRegistrations;
    int finishedCount;
    int pendingCount;
    QDateTime fastestTime;
    QString fastestAthlete;
};

struct RankingEntry {
    int position;
    Athletes::Athlete athlete;
    Categories::Category category;  
    Modalities::Modality modality;
    QString plateCode;
    Results::Result result;
    QString formattedTime;
};

class TrialAggregate
{
public:
    explicit TrialAggregate(
        QSqlDatabase db,
        std::shared_ptr<Athletes::Repository> athletesRepo,
        std::shared_ptr<Categories::Repository> categoriesRepo,
        std::shared_ptr<Modalities::Repository> modalitiesRepo,
        std::shared_ptr<Trials::Repository> trialsRepo,
        std::shared_ptr<Registrations::Repository> registrationsRepo,
        std::shared_ptr<Results::Repository> resultsRepo
    );

    tl::expected<TrialSummary, QString> getTrialSummary(int trialId);
    tl::expected<QVector<RegistrationDetail>, QString> getTrialRegistrations(int trialId);
    tl::expected<QVector<RankingEntry>, QString> getTrialRanking(int trialId);
    tl::expected<QVector<RankingEntry>, QString> getRankingByCategory(int trialId, int categoryId);
    tl::expected<QVector<RankingEntry>, QString> getRankingByModality(int trialId, int modalityId);
    
    tl::expected<RegistrationDetail, QString> registerAthleteForTrial(
        int trialId,
        const QString& athleteName,
        const QString& plateCode,
        const QString& categoryName,
        const QString& modalityName
    );
    
    tl::expected<Results::Result, QString> recordResult(
        int trialId,
        const QString& plateCode,
        const QDateTime& startTime,
        const QDateTime& endTime,
        const QString& notes = ""
    );

private:
    QSqlDatabase m_db;
    std::shared_ptr<Athletes::Repository> m_athletesRepo;
    std::shared_ptr<Categories::Repository> m_categoriesRepo;
    std::shared_ptr<Modalities::Repository> m_modalitiesRepo;
    std::shared_ptr<Trials::Repository> m_trialsRepo;
    std::shared_ptr<Registrations::Repository> m_registrationsRepo;
    std::shared_ptr<Results::Repository> m_resultsRepo;

    QString formatTime(int durationMs);
    int calculateDuration(const QDateTime& start, const QDateTime& end);
};

};

#endif // TRIALAGGREGATE_H