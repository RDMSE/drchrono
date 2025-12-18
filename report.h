#ifndef REPORT_H
#define REPORT_H

#include <QString>
#include <QSqlDatabase>

class Report
{
public:
    Report() = delete;

    static bool exportExcel(int trialId, const QString& outputFileName, QSqlDatabase db);
};

#endif // REPORT_H
