#ifndef REGISTRATION_H
#define REGISTRATION_H

#include <QString>

namespace Registrations {
struct Registration {
    int id;
    int trialId;
    int athleteId;
    QString plateCode;
    int modalityId;
    int categoryId;
};
};

#endif // REGISTRATION_H