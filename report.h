#ifndef REPORT_H
#define REPORT_H

#include <QString>
#include <QSqlDatabase>

class Report
{
public:
    Report() = delete;

    static bool exportExcel(int trialId, const QString& outputFileName, QSqlDatabase db);

private:
    static QString msToHHMMSS(qint64 ms)
    {
        qint64 hours = ms / (1000 * 60 * 60);
        int minutes = (ms / (1000 * 60)) % 60;
        int seconds = (ms / 1000) % 60;

        return QString("%1:%2:%3")
            .arg(hours,   2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }
};

#endif // REPORT_H
