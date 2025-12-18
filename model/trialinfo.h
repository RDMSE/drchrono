#ifndef TRIALINFO_H
#define TRIALINFO_H

#include <QString>
#include <QDateTime>
#include "utils/timeutils.h"

namespace Trials {

struct TrialInfo {
    int id = 0;
    QString name = "";
    QDateTime scheduledDateTime = Utils::DateTimeUtils::epochZero();
    QDateTime startDateTime = Utils::DateTimeUtils::epochZero();
    QDateTime endDateTime = Utils::DateTimeUtils::epochZero();
};
};


#endif // TRIALINFO_H
