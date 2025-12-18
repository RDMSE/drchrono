#ifndef EXCELUTILS_H
#define EXCELUTILS_H

#include <QString>
#include <QSqlDatabase>
#include <QVariant>
#include <QVector>
#include <tl/expected.hpp>

namespace Utils {
class ExcelUtils
{
public: 
    // New method for reading Excel files
    static tl::expected<QVector<QVector<QVariant>>, QString> readExcelFile(const QString& filePath);

private:
    static bool parseExcelCell(const QVariant& cell, QString& result);
};
};
#endif // EXCELUTILS_H
