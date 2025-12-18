#include "eventaggregate.h"
#include "utils/timeutils.h"
#include <QSqlQuery>
#include <QSqlError>
#include <algorithm>
#include <QDebug>

Aggregates::EventAggregate::EventAggregate(
    QSqlDatabase db,
    std::shared_ptr<Athletes::Repository> athletesRepo,
    std::shared_ptr<Categories::Repository> categoriesRepo,
    std::shared_ptr<Modalities::Repository> modalitiesRepo,
    std::shared_ptr<Trials::Repository> trialsRepo,
    std::shared_ptr<Registrations::Repository> registrationsRepo,
    std::shared_ptr<Results::Repository> resultsRepo)
    : m_db(db)
    , m_athletesRepo(athletesRepo)
    , m_categoriesRepo(categoriesRepo)
    , m_modalitiesRepo(modalitiesRepo)
    , m_trialsRepo(trialsRepo)
    , m_registrationsRepo(registrationsRepo)
    , m_resultsRepo(resultsRepo)
{
    m_trialAggregate = std::make_unique<TrialAggregate>(
        db, athletesRepo, categoriesRepo, modalitiesRepo, 
        trialsRepo, registrationsRepo, resultsRepo
    );
}

tl::expected<Aggregates::EventStatistics, QString> Aggregates::EventAggregate::getEventStatistics() {
    EventStatistics stats;
    QSqlQuery query(m_db);

    // General statistics in one query
    const QString sqlStats = R"(
        SELECT 
            COUNT(DISTINCT t.id) as total_trials,
            COUNT(DISTINCT reg.id) as total_registrations,
            COUNT(DISTINCT r.id) as finished_results
        FROM trials t
        LEFT JOIN registrations reg ON t.id = reg.trialId
        LEFT JOIN results r ON reg.id = r.registrationId
    )";

    if (!query.exec(sqlStats) || !query.next()) {
        return tl::unexpected("[EA] Error getting event statistics: " + query.lastError().text());
    }

    stats.totalTrials = query.value(0).toInt();
    stats.totalRegistrations = query.value(1).toInt();
    stats.totalFinishedResults = query.value(2).toInt();
    stats.totalPendingResults = stats.totalRegistrations - stats.totalFinishedResults;

    // Top athletes by participation
    const QString sqlTopAthletes = R"(
        SELECT a.name, COUNT(*) as participations
        FROM registrations reg
        JOIN athletes a ON reg.athleteId = a.id
        GROUP BY a.id, a.name
        ORDER BY participations DESC
        LIMIT 3
    )";

    if (query.exec(sqlTopAthletes)) {
        while (query.next()) {
            stats.topAthletes.append(QString("%1 (%2 trials)")
                                   .arg(query.value(0).toString())
                                   .arg(query.value(1).toInt()));
        }
    }

    // Distribution by category
    const QString sqlCategories = R"(
        SELECT c.name, COUNT(*) as count
        FROM registrations reg
        JOIN categories c ON reg.categoryId = c.id
        GROUP BY c.id, c.name
        ORDER BY count DESC
    )";

    if (query.exec(sqlCategories)) {
        while (query.next()) {
            stats.categoriesCount[query.value(0).toString()] = query.value(1).toInt();
        }
    }

    // Distribution by modality
    const QString sqlModalities = R"(
        SELECT m.name, COUNT(*) as count
        FROM registrations reg
        JOIN modalities m ON reg.modalityId = m.id
        GROUP BY m.id, m.name
        ORDER BY count DESC
    )";

    if (query.exec(sqlModalities)) {
        while (query.next()) {
            stats.modalitiesCount[query.value(0).toString()] = query.value(1).toInt();
        }
    }

    return stats;
}

tl::expected<QVector<Trials::TrialInfo>, QString> Aggregates::EventAggregate::getAllTrials() {
    return m_trialsRepo->getAllTrials();
}

tl::expected<QVector<Aggregates::CrossTrialRanking>, QString> Aggregates::EventAggregate::getCrossTrialRanking() {
    QSqlQuery query(m_db);
    
    // simplified query - formatting done in C++
    const QString sql = R"(
        SELECT 
            a.id as athlete_id,
            a.name as athlete_name,
            COUNT(DISTINCT reg.trialId) as total_participations,
            MIN(r.durationMs) as best_time_ms,
            AVG(CAST(r.durationMs AS REAL)) as average_time
        FROM athletes a
        JOIN registrations reg ON a.id = reg.athleteId
        JOIN results r ON reg.id = r.registrationId
        GROUP BY a.id, a.name
        HAVING COUNT(DISTINCT reg.trialId) > 0
        ORDER BY total_participations DESC, average_time ASC
    )";

    if (!query.exec(sql)) {
        return tl::unexpected("[EA] Error getting cross-trial ranking: " + query.lastError().text());
    }

    QVector<CrossTrialRanking> crossRanking;
    
    while (query.next()) {
        CrossTrialRanking ranking;
        
        ranking.athlete = Athletes::Athlete {
            .id = query.value(0).toInt(),
            .name = query.value(1).toString()
        };
        
        ranking.totalParticipations = query.value(2).toInt();
        ranking.averageTime = query.value(4).toDouble();
        ranking.bestTime = Utils::TimeFormatter::formatTime(query.value(3).toInt());
        
        // Fetch detailed results of this athlete in all trials
        QSqlQuery detailQuery(m_db);
        const QString detailSql = R"(
            SELECT 
                ROW_NUMBER() OVER (PARTITION BY reg.trialId ORDER BY r.durationMs ASC) as position,
                a.id as athlete_id, a.name as athlete_name,
                c.id as category_id, c.name as category_name,
                m.id as modality_id, m.name as modality_name,
                reg.plateCode,
                r.id as result_id, r.registrationId, r.startTime, r.endTime, r.durationMs, r.notes
            FROM results r
            JOIN registrations reg ON r.registrationId = reg.id  
            JOIN athletes a ON reg.athleteId = a.id
            JOIN categories c ON reg.categoryId = c.id
            JOIN modalities m ON reg.modalityId = m.id
            WHERE a.id = :athleteId
            ORDER BY reg.trialId, r.durationMs ASC
        )";
        
        detailQuery.prepare(detailSql);
        detailQuery.bindValue(":athleteId", ranking.athlete.id);
        
        if (detailQuery.exec()) {
            while (detailQuery.next()) {
                Athletes::Athlete athlete { .id = detailQuery.value(1).toInt(), .name = detailQuery.value(2).toString() };
                Categories::Category category { .id = detailQuery.value(3).toInt(), .name = detailQuery.value(4).toString() };
                Modalities::Modality modality { .id = detailQuery.value(5).toInt(), .name = detailQuery.value(6).toString() };
                Results::Result result {
                    .id = detailQuery.value(8).toInt(), .registrationId = detailQuery.value(9).toInt(),
                    .startTime = QDateTime::fromString(detailQuery.value(10).toString(), Qt::ISODate),
                    .endTime = QDateTime::fromString(detailQuery.value(11).toString(), Qt::ISODate),
                    .durationMs = detailQuery.value(12).toInt(), .notes = detailQuery.value(13).toString()
                };

                ranking.trialResults.append({
                    .position = detailQuery.value(0).toInt(), .athlete = athlete, .category = category,
                    .modality = modality, .plateCode = detailQuery.value(7).toString(),
                    .result = result, .formattedTime = Utils::TimeFormatter::formatTime(result.durationMs)
                });
            }
        }
        
        crossRanking.append(ranking);
    }

    return crossRanking;
}

tl::expected<QVector<Athletes::Athlete>, QString> Aggregates::EventAggregate::getTopParticipatingAthletes(int limit) {
    auto crossRankingResult = getCrossTrialRanking();
    if (!crossRankingResult) {
        return tl::unexpected(crossRankingResult.error());
    }

    QVector<Athletes::Athlete> topAthletes;
    int count = 0;
    
    for (const auto& ranking : crossRankingResult.value()) {
        if (count >= limit) break;
        topAthletes.append(ranking.athlete);
        count++;
    }

    return topAthletes;
}

tl::expected<Trials::TrialInfo, QString> Aggregates::EventAggregate::createTrial(
    const QString& trialName,
    const QDateTime& scheduledDateTime) {
    
    return m_trialsRepo->createTrial(trialName, scheduledDateTime);
}

tl::expected<QVector<Athletes::Athlete>, QString> Aggregates::EventAggregate::searchAthletes(const QString& namePattern) {
    auto allAthletesResult = m_athletesRepo->getAllAthletes();
    if (!allAthletesResult) {
        return tl::unexpected(allAthletesResult.error());
    }

    QVector<Athletes::Athlete> matches;
    QString pattern = namePattern.toLower();

    for (const auto& athlete : allAthletesResult.value()) {
        if (athlete.name.toLower().contains(pattern)) {
            matches.append(athlete);
        }
    }

    return matches;
}

tl::expected<QString, QString> Aggregates::EventAggregate::generateEventReport() {
    auto statsResult = getEventStatistics();
    if (!statsResult) {
        return tl::unexpected("Error getting statistics: " + statsResult.error());
    }

    auto stats = statsResult.value();
    QString report;

    report += "=== EVENT REPORT ===\n\n";
    report += QString("Total Trials: %1\n").arg(stats.totalTrials);
    report += QString("Total Registrations: %1\n").arg(stats.totalRegistrations);
    report += QString("Finished Results: %1\n").arg(stats.totalFinishedResults);
    report += QString("Pending Results: %1\n\n").arg(stats.totalPendingResults);
    report += "Top Athletes by Participation:\n";
    for (const auto& athlete : stats.topAthletes) {
        report += QString("- %1\n").arg(athlete);
    }
    report += "\n";

    report += "Distribution by Category:\n";
    for (auto it = stats.categoriesCount.begin(); it != stats.categoriesCount.end(); ++it) {
        report += QString("- %1: %2 registrations\n").arg(it.key(), QString::number(it.value()));
    }
    report += "\n";

    report += "Distribution by Modality:\n";
    for (auto it = stats.modalitiesCount.begin(); it != stats.modalitiesCount.end(); ++it) {
        report += QString("- %1: %2 inscrições\n").arg(it.key(), QString::number(it.value()));
    }

    return report;
}

tl::expected<QVector<QString>, QString> Aggregates::EventAggregate::validateEventIntegrity() {
    QVector<QString> issues;
    QSqlQuery query(m_db);

    // Verify orphan registrations (without valid athlete)
    const QString sqlOrphanAthletes = R"(
        SELECT reg.id, reg.athleteId
        FROM registrations reg
        LEFT JOIN athletes a ON reg.athleteId = a.id
        WHERE a.id IS NULL
    )";

    if (query.exec(sqlOrphanAthletes)) {
        while (query.next()) {
            issues.append(QString("Inscrição %1: Atleta ID %2 não encontrado")
                         .arg(query.value(0).toInt()).arg(query.value(1).toInt()));
        }
    }

    // Verify orphan registrations (without valid category)
    const QString sqlOrphanCategories = R"(
        SELECT reg.id, reg.categoryId
        FROM registrations reg
        LEFT JOIN categories c ON reg.categoryId = c.id
        WHERE c.id IS NULL
    )";

    if (query.exec(sqlOrphanCategories)) {
        while (query.next()) {
            issues.append(QString("Inscrição %1: Categoria ID %2 não encontrada")
                         .arg(query.value(0).toInt()).arg(query.value(1).toInt()));
        }
    }

    // Verify orphan registrations (without valid modality)
    const QString sqlOrphanModalities = R"(
        SELECT reg.id, reg.modalityId
        FROM registrations reg
        LEFT JOIN modalities m ON reg.modalityId = m.id
        WHERE m.id IS NULL
    )";

    if (query.exec(sqlOrphanModalities)) {
        while (query.next()) {
            issues.append(QString("Inscrição %1: Modalidade ID %2 não encontrada")
                         .arg(query.value(0).toInt()).arg(query.value(1).toInt()));
        }
    }

    // Verify orphan results (without valid registration)
    const QString sqlOrphanResults = R"(
        SELECT r.id, r.registrationId
        FROM results r
        LEFT JOIN registrations reg ON r.registrationId = reg.id
        WHERE reg.id IS NULL
    )";

    if (query.exec(sqlOrphanResults)) {
        while (query.next()) {
            issues.append(QString("Resultado %1: Inscrição ID %2 não encontrada")
                         .arg(query.value(0).toInt()).arg(query.value(1).toInt()));
        }
    }

    // Verify duplicate plates by trial
    const QString sqlDuplicatePlates = R"(
        SELECT trialId, plateCode, COUNT(*) as count
        FROM registrations
        GROUP BY trialId, plateCode
        HAVING COUNT(*) > 1
    )";

    if (query.exec(sqlDuplicatePlates)) {
        while (query.next()) {
            issues.append(QString("Placa '%1' usada %2 vezes na prova ID %3")
                         .arg(query.value(1).toString())
                         .arg(query.value(2).toInt())
                         .arg(query.value(0).toInt()));
        }
    }

    return issues;
}