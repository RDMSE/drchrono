#include "trialaggregate.h"
#include "utils/timeutils.h"
#include <QTime>
#include <algorithm>
#include <QSqlQuery>
#include <QSqlError>
#include <utility>

Aggregates::TrialAggregate::TrialAggregate(
    const QSqlDatabase& db,
    std::shared_ptr<Athletes::Repository> athletesRepo,
    std::shared_ptr<Categories::Repository> categoriesRepo,
    std::shared_ptr<Modalities::Repository> modalitiesRepo,
    std::shared_ptr<Trials::Repository> trialsRepo,
    std::shared_ptr<Registrations::Repository> registrationsRepo,
    std::shared_ptr<Results::Repository> resultsRepo)
    : m_db(db)
    , m_athletesRepo(std::move(athletesRepo))
    , m_categoriesRepo(std::move(categoriesRepo))
    , m_modalitiesRepo(std::move(modalitiesRepo))
    , m_trialsRepo(std::move(trialsRepo))
    , m_registrationsRepo(std::move(registrationsRepo))
    , m_resultsRepo(std::move(resultsRepo))
{
}

tl::expected<Aggregates::TrialSummary, QString> Aggregates::TrialAggregate::getTrialSummary(int trialId) {
    QSqlQuery query(m_db);
    
    // A single query to fetch trial + statistics
    const QString sql = R"(
        SELECT 
            t.id, t.name, t.scheduledDateTime, t.startDateTime, t.endDateTime,
            COUNT(DISTINCT reg.id) as total_registrations,
            COUNT(DISTINCT r.id) as finished_count,
            COUNT(DISTINCT reg.id) - COUNT(DISTINCT r.id) as pending_count,
            MIN(r.durationMs) as best_duration,
            (SELECT a.name FROM results r2 
             JOIN registrations reg2 ON r2.registrationId = reg2.id 
             JOIN athletes a ON reg2.athleteId = a.id 
             WHERE reg2.trialId = t.id AND r2.durationMs = MIN(r.durationMs) 
             LIMIT 1) as fastest_athlete,
            (SELECT r2.endTime FROM results r2 
             JOIN registrations reg2 ON r2.registrationId = reg2.id 
             WHERE reg2.trialId = t.id AND r2.durationMs = MIN(r.durationMs) 
             LIMIT 1) as fastest_time
        FROM trials t
        LEFT JOIN registrations reg ON t.id = reg.trialId
        LEFT JOIN results r ON reg.id = r.registrationId
        WHERE t.id = :trialId
        GROUP BY t.id
    )";

    query.prepare(sql);
    query.bindValue(":trialId", trialId);

    if (!query.exec() || !query.next()) {
        return tl::unexpected("[TA] Error getting trial summary: " + query.lastError().text());
    }

    // Fetch detailed registrations
    auto registrationsResult = getTrialRegistrations(trialId);
    if (!registrationsResult) {
        return tl::unexpected("Error getting registrations: " + registrationsResult.error());
    }

    const Trials::TrialInfo trial {
        .id = query.value(0).toInt(),
        .name = query.value(1).toString(),
        .scheduledDateTime = QDateTime::fromString(query.value(2).toString(), Qt::ISODate),
        .startDateTime = QDateTime::fromString(query.value(3).toString(), Qt::ISODate),
        .endDateTime = QDateTime::fromString(query.value(4).toString(), Qt::ISODate)
    };

    return TrialSummary{
        .trial = trial,
        .registrations = registrationsResult.value(),
        .totalRegistrations = query.value(5).toInt(),
        .finishedCount = query.value(6).toInt(),
        .pendingCount = query.value(7).toInt(),
        .fastestTime = QDateTime::fromString(query.value(9).toString(), Qt::ISODate),
        .fastestAthlete = query.value(8).toString()
    };
}

tl::expected<QVector<Aggregates::RegistrationDetail>, QString> Aggregates::TrialAggregate::getTrialRegistrations(int trialId) const {
    QSqlQuery query(m_db);
    
    // A single query with JOINs to fetch all data
    const QString sql = R"(
        SELECT 
            reg.id, reg.trialId, reg.athleteId, reg.plateCode, reg.modalityId, reg.categoryId,
            a.name as athlete_name,
            c.name as category_name,
            m.name as modality_name,
            r.id as result_id, r.startTime, r.endTime, r.durationMs, r.notes
        FROM registrations reg
        JOIN athletes a ON reg.athleteId = a.id
        JOIN categories c ON reg.categoryId = c.id
        JOIN modalities m ON reg.modalityId = m.id
        LEFT JOIN results r ON reg.id = r.registrationId
        WHERE reg.trialId = :trialId
        ORDER BY reg.plateCode
    )";

    query.prepare(sql);
    query.bindValue(":trialId", trialId);

    if (!query.exec()) {
        return tl::unexpected("[TA] Error getting trial registrations: " + query.lastError().text());
    }

    QVector<RegistrationDetail> details;
    
    while (query.next()) {
        const Registrations::Registration registration {
            .id = query.value(0).toInt(),
            .trialId = query.value(1).toInt(),
            .athleteId = query.value(2).toInt(),
            .plateCode = query.value(3).toString(),
            .modalityId = query.value(4).toInt(),
            .categoryId = query.value(5).toInt()
        };

        const Athletes::Athlete athlete {
            .id = registration.athleteId,
            .name = query.value(6).toString()
        };

        const Categories::Category category {
            .id = registration.categoryId,
            .name = query.value(7).toString()
        };

        const Modalities::Modality modality {
            .id = registration.modalityId,
            .name = query.value(8).toString()
        };

        std::optional<Results::Result> result;
        if (!query.value(9).isNull()) {
            result = Results::Result {
                .id = query.value(9).toInt(),
                .registrationId = registration.id,
                .startTime = QDateTime::fromString(query.value(10).toString(), Qt::ISODate),
                .endTime = QDateTime::fromString(query.value(11).toString(), Qt::ISODate),
                .durationMs = query.value(12).toInt(),
                .notes = query.value(13).toString()
            };
        }

        details.append({
            .registration = registration,
            .athlete = athlete,
            .category = category,
            .modality = modality,
            .result = result
        });
    }

    return details;
}

tl::expected<QVector<Aggregates::RankingEntry>, QString> Aggregates::TrialAggregate::getTrialRanking(const int trialId) const {
    QSqlQuery query(m_db);
    
    // Simplified query with ROW_NUMBER for automatic positioning
    const QString sql = R"(
        SELECT 
            ROW_NUMBER() OVER (ORDER BY r.durationMs ASC) as position,
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
        WHERE reg.trialId = :trialId
        ORDER BY r.durationMs ASC
    )";

    query.prepare(sql);
    query.bindValue(":trialId", trialId);

    if (!query.exec()) {
        return tl::unexpected("[TA] Error getting trial ranking: " + query.lastError().text());
    }

    QVector<RankingEntry> ranking;
    
    while (query.next()) {
        const Athletes::Athlete athlete {
            .id = query.value(1).toInt(),
            .name = query.value(2).toString()
        };

        const Categories::Category category {
            .id = query.value(3).toInt(),
            .name = query.value(4).toString()
        };

        const Modalities::Modality modality {
            .id = query.value(5).toInt(),
            .name = query.value(6).toString()
        };

        const Results::Result result {
            .id = query.value(8).toInt(),
            .registrationId = query.value(9).toInt(),
            .startTime = QDateTime::fromString(query.value(10).toString(), Qt::ISODate),
            .endTime = QDateTime::fromString(query.value(11).toString(), Qt::ISODate),
            .durationMs = query.value(12).toInt(),
            .notes = query.value(13).toString()
        };

        ranking.append({
            .position = query.value(0).toInt(),
            .athlete = athlete,
            .category = category,
            .modality = modality,
            .plateCode = query.value(7).toString(),
            .result = result,
            .formattedTime = Utils::TimeFormatter::formatTime(result.durationMs)
        });
    }

    return ranking;
}

tl::expected<QVector<Aggregates::RankingEntry>, QString> Aggregates::TrialAggregate::getRankingByCategory(const int trialId, const int categoryId) const {
    QSqlQuery query(m_db);
    
    // Simplified query filtering by category
    const QString sql = R"(
        SELECT 
            ROW_NUMBER() OVER (ORDER BY r.durationMs ASC) as position,
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
        WHERE reg.trialId = :trialId AND reg.categoryId = :categoryId
        ORDER BY r.durationMs ASC
    )";

    query.prepare(sql);
    query.bindValue(":trialId", trialId);
    query.bindValue(":categoryId", categoryId);

    if (!query.exec()) {
        return tl::unexpected("[TA] Error getting category ranking: " + query.lastError().text());
    }

    QVector<RankingEntry> ranking;
    
    while (query.next()) {
        const Athletes::Athlete athlete { .id = query.value(1).toInt(), .name = query.value(2).toString() };
        const Categories::Category category { .id = query.value(3).toInt(), .name = query.value(4).toString() };
        const Modalities::Modality modality { .id = query.value(5).toInt(), .name = query.value(6).toString() };
        const Results::Result result {
            .id = query.value(8).toInt(), .registrationId = query.value(9).toInt(),
            .startTime = QDateTime::fromString(query.value(10).toString(), Qt::ISODate),
            .endTime = QDateTime::fromString(query.value(11).toString(), Qt::ISODate),
            .durationMs = query.value(12).toInt(), .notes = query.value(13).toString()
        };

        ranking.append({ .position = query.value(0).toInt(), .athlete = athlete, .category = category,
                        .modality = modality, .plateCode = query.value(7).toString(),
                        .result = result, .formattedTime = Utils::TimeFormatter::formatTime(result.durationMs) });
    }

    return ranking;
}

tl::expected<QVector<Aggregates::RankingEntry>, QString> Aggregates::TrialAggregate::getRankingByModality(const int trialId, const int modalityId) const {
    QSqlQuery query(m_db);
    
    // Simplified query filtering by modality
    const QString sql = R"(
        SELECT 
            ROW_NUMBER() OVER (ORDER BY r.durationMs ASC) as position,
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
        WHERE reg.trialId = :trialId AND reg.modalityId = :modalityId
        ORDER BY r.durationMs ASC
    )";

    query.prepare(sql);
    query.bindValue(":trialId", trialId);
    query.bindValue(":modalityId", modalityId);

    if (!query.exec()) {
        return tl::unexpected("[TA] Error getting modality ranking: " + query.lastError().text());
    }

    QVector<RankingEntry> ranking;
    
    while (query.next()) {
        const Athletes::Athlete athlete { .id = query.value(1).toInt(), .name = query.value(2).toString() };
        const Categories::Category category { .id = query.value(3).toInt(), .name = query.value(4).toString() };
        const Modalities::Modality modality { .id = query.value(5).toInt(), .name = query.value(6).toString() };
        const Results::Result result {
            .id = query.value(8).toInt(), .registrationId = query.value(9).toInt(),
            .startTime = QDateTime::fromString(query.value(10).toString(), Qt::ISODate),
            .endTime = QDateTime::fromString(query.value(11).toString(), Qt::ISODate),
            .durationMs = query.value(12).toInt(), .notes = query.value(13).toString()
        };

        ranking.append({ .position = query.value(0).toInt(), .athlete = athlete, .category = category,
                        .modality = modality, .plateCode = query.value(7).toString(),
                        .result = result, .formattedTime = Utils::TimeFormatter::formatTime(result.durationMs) });
    }

    return ranking;
}

tl::expected<Aggregates::RegistrationDetail, QString> Aggregates::TrialAggregate::registerAthleteForTrial(
    int trialId,
    const QString& athleteName,
    const QString& plateCode,
    const QString& categoryName,
    const QString& modalityName) const {

    // Verify if the trial exists
    if (auto trialResult = m_trialsRepo->getTrialById(trialId); !trialResult) {
        return tl::unexpected("Trial not found: " + trialResult.error());
    }

    // Create/find athlete
    auto athleteResult = m_athletesRepo->createAthlete(athleteName);
    if (!athleteResult) {
        // Try to find if already exists
        athleteResult = m_athletesRepo->getAthleteByName(athleteName);
        if (!athleteResult) {
            return tl::unexpected("Error creating/finding athlete: " + athleteResult.error());
        }
    }

    // Create/find category
    auto categoryResult = m_categoriesRepo->createCategory(categoryName);
    if (!categoryResult) {
        categoryResult = m_categoriesRepo->getCategoryByName(categoryName);
        if (!categoryResult) {
            return tl::unexpected("Error creating/finding category: " + categoryResult.error());
        }
    }

    // Create/find modality
    auto modalityResult = m_modalitiesRepo->createModality(modalityName);
    if (!modalityResult) {
        modalityResult = m_modalitiesRepo->getModalityByName(modalityName);
        if (!modalityResult) {
            return tl::unexpected("Error creating/finding modality: " + modalityResult.error());
        }
    }

    // Create registration
    auto registrationResult = m_registrationsRepo->createRegistration(
        trialId,
        athleteResult->id,
        plateCode,
        modalityResult->id,
        categoryResult->id
    );

    if (!registrationResult) {
        return tl::unexpected("Error creating registration: " + registrationResult.error());
    }

    return RegistrationDetail{
        .registration = registrationResult.value(),
        .athlete = athleteResult.value(),
        .category = categoryResult.value(),
        .modality = modalityResult.value(),
        .result = std::nullopt
    };
}

tl::expected<Results::Result, QString> Aggregates::TrialAggregate::recordResult(
    const int trialId,
    const QString& plateCode,
    const QDateTime& startTime,
    const QDateTime& endTime,
    const QString& notes) const {

    // Fetch registration by plate code
    auto registrationResult = m_registrationsRepo->getRegistrationByPlateCode(trialId, plateCode);
    if (!registrationResult) {
        return tl::unexpected("Registration not found for plate " + plateCode + ": " + registrationResult.error());
    }

    // Calculate duration
    int durationMs = calculateDuration(startTime, endTime);
    if (durationMs <= 0) {
        return tl::unexpected("Invalid time duration");
    }

    // Create result
    auto resultResult = m_resultsRepo->createResult(
        registrationResult->id,
        startTime,
        endTime,
        durationMs,
        notes
    );

    if (!resultResult) {
        return tl::unexpected("Error creating result: " + resultResult.error());
    }

    return resultResult.value();
}


int Aggregates::TrialAggregate::calculateDuration(const QDateTime& start, const QDateTime& end) {
    if (!start.isValid() || !end.isValid() || start >= end) {
        return -1;
    }
    return static_cast<int>(start.msecsTo(end));
}