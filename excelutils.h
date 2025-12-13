#ifndef EXCELUTILS_H
#define EXCELUTILS_H

#include <QString>
#include <QSqlDatabase>

class ExcelUtils
{
public:
    ExcelUtils() = delete;

    static bool importExcelGroupsAndLabels(const QString& outputFileName, QSqlDatabase db);
};

#endif // EXCELUTILS_H
