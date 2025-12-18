#ifndef RESULT_H
#define RESULT_H

#include <QString>
#include <QDateTime>

namespace Results {
struct Result {
    int id;
    int registrationId;
    QDateTime startTime;
    QDateTime endTime;
    int durationMs;
    QString notes;
};
};

#endif // RESULT_H