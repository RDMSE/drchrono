#ifndef EVENTAGGREGATE_H
#define EVENTAGGREGATE_H

#include "trialinfo.h"
#include "athlete.h"
#include "category.h" 
#include "modality.h"
#include "registration.h"
#include "result.h"
#include "trialaggregate.h"
#include "repository/athletes/athletesrepository.h"
#include "repository/categories/categoriesrepository.h"
#include "repository/modalities/modalitiesrepository.h"
#include "repository/trials/trialsrepository.h"
#include "repository/registrations/registrationsrepository.h"
#include "repository/results/resultsrepository.h"
#include <QString>
#include <QVector>
#include <QDateTime>
#include <QMap>
#include <memory>
#include <optional>
#include <QSqlDatabase>
#include <tl/expected.hpp>

namespace Aggregates {

struct EventStatistics {
    int totalTrials;
    int totalRegistrations;
    int totalFinishedResults;
    int totalPendingResults;
    QVector<QString> topAthletes; // Top 3 com mais participações
    QMap<QString, int> categoriesCount;
    QMap<QString, int> modalitiesCount;
};

struct CrossTrialRanking {
    Athletes::Athlete athlete;
    QVector<RankingEntry> trialResults; // Resultados em diferentes provas
    int totalParticipations;
    QString bestTime;
    double averageTime; // Média em milissegundos
};

class EventAggregate
{
public:
    explicit EventAggregate(const QSqlDatabase &db, const std::shared_ptr<Athletes::Repository> &athletesRepo,
                   const std::shared_ptr<Categories::Repository> &categoriesRepo,
                   const std::shared_ptr<Modalities::Repository> &modalitiesRepo,
                   const std::shared_ptr<Trials::Repository> &trialsRepo,
                   const std::shared_ptr<Registrations::Repository> &registrationsRepo,
                   const std::shared_ptr<Results::Repository> &resultsRepo);

    [[nodiscard]] tl::expected<EventStatistics, QString> getEventStatistics() const;
    [[nodiscard]] tl::expected<QVector<Trials::TrialInfo>, QString> getAllTrials() const;
    [[nodiscard]] tl::expected<QVector<CrossTrialRanking>, QString> getCrossTrialRanking() const;
    [[nodiscard]] tl::expected<QVector<Athletes::Athlete>, QString> getTopParticipatingAthletes(int limit = 10) const;
    
    // Create new trial
    [[nodiscard]] tl::expected<Trials::TrialInfo, QString> createTrial(
        const QString& trialName,
        const QDateTime& scheduledDateTime
    ) const;
    
    // Search athletes by criteria
    [[nodiscard]] tl::expected<QVector<Athletes::Athlete>, QString> searchAthletes(const QString& namePattern) const;
    
    // Consolidated reports
    [[nodiscard]] tl::expected<QString, QString> generateEventReport() const; // CSV or text
    
    // Event validations
    [[nodiscard]] tl::expected<QVector<QString>, QString> validateEventIntegrity() const;

private:
    QSqlDatabase m_db;
    std::shared_ptr<Athletes::Repository> m_athletesRepo;
    std::shared_ptr<Categories::Repository> m_categoriesRepo;
    std::shared_ptr<Modalities::Repository> m_modalitiesRepo;
    std::shared_ptr<Trials::Repository> m_trialsRepo;
    std::shared_ptr<Registrations::Repository> m_registrationsRepo;
    std::shared_ptr<Results::Repository> m_resultsRepo;
    
    std::unique_ptr<TrialAggregate> m_trialAggregate;
};

};

#endif // EVENTAGGREGATE_H