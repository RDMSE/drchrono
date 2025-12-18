#ifndef TIMEUTILS_H
#define TIMEUTILS_H

#include <QString>
#include <QDateTime>

namespace Utils {

class TimeFormatter
{
public:
    static QString formatTime(int durationMs);
    static QString formatTimeShort(int durationMs); // Sem centésimos para relatórios
};

class DateTimeUtils 
{
public:
    // Constantes úteis
    static QDateTime epochZero() { return QDateTime::fromSecsSinceEpoch(0); }
    static QDateTime now() { return QDateTime::currentDateTime(); }
    
    // Métodos utilitários
    static bool isValid(const QDateTime& dateTime) { return dateTime != epochZero(); }
    static bool isNull(const QDateTime& dateTime) { return dateTime == epochZero(); }
    
    // Conversões seguras
    static QDateTime fromStringOrDefault(const QString& dateTimeString, Qt::DateFormat format = Qt::ISODate) {
        if (dateTimeString.isEmpty()) return epochZero();
        QDateTime result = QDateTime::fromString(dateTimeString, format);
        return result.isValid() ? result : epochZero();
    }
    
    static QString toStringOrEmpty(const QDateTime& dateTime, Qt::DateFormat format = Qt::ISODate) {
        return isValid(dateTime) ? dateTime.toString(format) : QString();
    }
};

};

#endif // TIMEUTILS_H