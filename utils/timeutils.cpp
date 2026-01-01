#include "timeutils.h"

QString Utils::TimeFormatter::formatTime(int durationMs) {
    if (durationMs < 0) return "Invalid";
    
    const int hours = durationMs / 3600000;
    const int minutes = (durationMs % 3600000) / 60000;
    const int seconds = (durationMs % 60000) / 1000;
    const int centiseconds = (durationMs % 1000) / 10;
    
    if (hours > 0) {
        return QString("%1:%2:%3.%4")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'))
            .arg(centiseconds, 2, 10, QChar('0'));
    }
    
    return QString("%1:%2.%3")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(centiseconds, 2, 10, QChar('0'));
}

QString Utils::TimeFormatter::formatTimeShort(int durationMs) {
    if (durationMs < 0) return "Invalid";
    
    const int hours = durationMs / 3600000;
    const int minutes = (durationMs % 3600000) / 60000;
    const int seconds = (durationMs % 60000) / 1000;
    
    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
    
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}