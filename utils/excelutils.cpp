#include "excelutils.h"
#include <QDebug>
#include <QFileInfo>
#include <cmath>
#include "xlsxdocument.h"
#include "xlsxworksheet.h"

tl::expected<QVector<QVector<QVariant>>, QString> Utils::ExcelUtils::readExcelFile(const QString& filePath)
{
    try {
        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists()) {
            return tl::unexpected("File not found: " + filePath);
        }
        
        if (!fileInfo.suffix().toLower().contains("xls")) {
            return tl::unexpected("Unsupported file format. Please use .xlsx or .xls files");
        }
        
        QXlsx::Document xlsx(filePath);
        if (!xlsx.load()) {
            return tl::unexpected("Error opening Excel file: " + filePath);
        }
        
        QXlsx::Worksheet* worksheet = xlsx.currentWorksheet();
        if (!worksheet) {
            return tl::unexpected("No worksheet found in Excel file");
        }
        
        QVector<QVector<QVariant>> result;
        
        // Get the range of cells with data
        auto dimension = worksheet->dimension();
        if (!dimension.isValid()) {
            return tl::unexpected("Worksheet is empty or contains invalid data");
        }
        
        // Read all rows with data
        for (int row = dimension.firstRow(); row <= dimension.lastRow(); ++row) {
            QVector<QVariant> rowData;
            bool hasData = false;
            
            for (int col = dimension.firstColumn(); col <= dimension.lastColumn(); ++col) {
                QVariant cellValue = worksheet->read(row, col);
                
                // Debug: Log raw cell value and type for plate code column (column 1, index 0)
                if (col == 1 && row <= 5) { // Log first few rows of plate code column
                    qDebug() << QString("Row %1, Col %2 - Type: %3, Raw: '%4'")
                               .arg(row).arg(col).arg(cellValue.metaType().name()).arg(cellValue.toString());
                }
                
                // Convert to string and trim
                QString stringValue;
                if (parseExcelCell(cellValue, stringValue)) {
                    hasData = true;
                }
                
                // Debug: Log processed value for plate code column
                if (col == 1 && row <= 5) {
                    qDebug() << QString("Row %1, Col %2 - Processed: '%3'")
                               .arg(row).arg(col).arg(stringValue);
                }
                
                rowData.append(QVariant(stringValue));
            }
            
            // Only add rows that have some data
            if (hasData) {
                result.append(rowData);
            }
        }
        
        if (result.isEmpty()) {
            return tl::unexpected("No data found in the worksheet");
        }
        
        qDebug() << QString("Excel file loaded successfully: %1 rows, %2 columns")
                   .arg(result.size())
                   .arg(result.first().size());
        
        return result;
        
    } catch (const std::exception& e) {
        return tl::unexpected(QString("Error processing Excel file: %1").arg(e.what()));
    } catch (...) {
        return tl::unexpected("Unknown error processing Excel file");
    }
}

bool Utils::ExcelUtils::parseExcelCell(const QVariant& cell, QString& result)
{
    if (cell.isNull() || !cell.isValid()) {
        result = "";
        return false;
    }
    
    QString stringValue;
    
    // Always try to get the string representation first
    QString originalString = cell.toString().trimmed();
    
    // Handle different data types properly
    switch (cell.typeId()) {
        case QMetaType::QString:
            stringValue = originalString;
            break;
            
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
            // Use original string if it looks like a formatted number (has leading zeros)
            if (originalString.length() > 1 && originalString.at(0) == '0' && originalString.toInt() > 0) {
                stringValue = originalString; // Preserve "001", "007", etc.
                qDebug() << QString("Preserving leading zeros from int: '%1'").arg(originalString);
            } else {
                stringValue = QString::number(cell.toLongLong());
            }
            break;
            
        case QMetaType::Double:
        {
            double value = cell.toDouble();
            
            // Check if original string had leading zeros (like "001" that became 1.0)
            if (originalString.length() > 1 && originalString.at(0) == '0' && 
                value == std::floor(value) && value > 0) {
                stringValue = originalString; // Preserve original format like "001"
                qDebug() << QString("Preserving leading zeros: '%1' (was double %2)")
                           .arg(originalString).arg(value);
            } else if (value == std::floor(value)) {
                stringValue = QString::number(static_cast<long long>(value));
            } else {
                stringValue = QString::number(value, 'f', 2);
            }
            break;
        }
        
        case QMetaType::Bool:
            stringValue = cell.toBool() ? "true" : "false";
            break;
            
        default:
            stringValue = originalString;
            break;
    }
    
    // Clean up common Excel formatting issues
    stringValue = stringValue.trimmed();
    
    // Remove any non-printable characters that might cause issues
    QString cleanedValue;
    for (const QChar& ch : stringValue) {
        if (ch.isPrint() || ch.isSpace()) {
            cleanedValue += ch;
        }
    }
    
    result = cleanedValue.trimmed();
    return !result.isEmpty();
}
