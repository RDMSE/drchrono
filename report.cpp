#include "report.h"
#include "xlsxdocument.h"
#include <QSqlQuery>
#include <QString>
#include <QSqlError>
#include <QColor>
#include <algorithm>
#include "utils/timeutils.h"

bool Report::exportExcel(const int trialId, const QString& outputFileName, const QSqlDatabase& db) {

    //const QString timeFormat = "hh:MM:ss";

    QXlsx::Document xlsx;

    // format
    QXlsx::Format headerFormat;
    headerFormat.setFontBold(true);
    headerFormat.setPatternBackgroundColor(Qt::lightGray);
    headerFormat.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    QXlsx::Format zebraWhite;
    zebraWhite.setPatternBackgroundColor(Qt::white);

    QXlsx::Format zebraGray;
    zebraGray.setPatternBackgroundColor(QColor(235,235,235));

    QVector<int> colWidth(5, 0);


    QSqlQuery groupQuery(db);

    QString sql = R"(
        SELECT DISTINCT (m.name || ' - ' || c.name) as groupName, c.name as categoryName, m.name as modalityName
        FROM results r
        INNER JOIN registrations reg ON r.registrationId = reg.id
        INNER JOIN categories c ON reg.categoryId = c.id
        INNER JOIN modalities m ON reg.modalityId = m.id
        WHERE reg.trialId = :trialId
        ORDER BY m.name, c.name
    )";

    groupQuery.prepare(sql);
    groupQuery.bindValue(":trialId", trialId);

    if (!groupQuery.exec()) {
        qWarning() << "Error finding groups" << groupQuery.lastError().text();
        return false;
    }

    bool firstSheet = true;
    while(groupQuery.next()) {
        QString groupName = groupQuery.value(0).toString();
        if (firstSheet) {
            xlsx.renameSheet(xlsx.currentSheet()->sheetName(), groupName);
            firstSheet = false;
        } else {
            xlsx.addSheet(groupName);
        }

        xlsx.selectSheet(groupName);

        // headers
        QStringList headers = {"Placa", "Inicio", "Fim", "Duracao", "Notas"};

        for (int c = 0; c < headers.size(); ++c) {
            xlsx.write(1, c + 1, headers[c], headerFormat);
            colWidth[c] = std::max(colWidth[c], static_cast<int>(headers[c].length()));
        }

        //freeze first line
        // xlsx.currentWorksheet()->freezePanes(2,1); // Método não disponível nesta versão da QXlsx

        QString categoryName = groupQuery.value(1).toString();
        QString modalityName = groupQuery.value(2).toString();
        
        QSqlQuery evQuery(db);
        sql = R"(
            SELECT r1.plateCode, r1.startTime, r1.endTime, r1.durationMs as duration, r1.notes
            FROM (
                SELECT reg.plateCode, r.startTime, r.endTime, r.durationMs, r.notes,
                       ROW_NUMBER() OVER (PARTITION BY reg.plateCode ORDER BY r.endTime DESC) as rn
                FROM results r
                INNER JOIN registrations reg ON r.registrationId = reg.id
                INNER JOIN athletes a ON reg.athleteId = a.id
                INNER JOIN categories c ON reg.categoryId = c.id
                INNER JOIN modalities m ON reg.modalityId = m.id
                WHERE reg.trialId = :trialId
                    AND c.name = :categoryName
                    AND m.name = :modalityName
            ) r1
            WHERE r1.rn = 1
            ORDER BY r1.durationMs
        )";

        evQuery.prepare(sql);
        evQuery.bindValue(":trialId", trialId);
        evQuery.bindValue(":categoryName", categoryName);
        evQuery.bindValue(":modalityName", modalityName);

        if (!evQuery.exec()) {
            qWarning() << "Error getting events:" << evQuery.lastError().text();
            continue;
        }

        int row = 2;
        bool zebra = false;
        while(evQuery.next()) {
            //QString duration = QTime::fromString(evQuery.value(3).toString(), Qt::DateFormat);
            QXlsx::Format& fmt = zebra ? zebraGray : zebraWhite;
            zebra = !zebra;

            QStringList values{
                evQuery.value(0).toString(),
                evQuery.value(1).toString(),
                evQuery.value(2).toString(),
                Utils::TimeFormatter::formatTimeShort(evQuery.value(3).toInt()),
                evQuery.value(4).toString()
            };

            for (int i = 0; i < values.length(); i++) {
                const QString& text = values[i];
                xlsx.write(row, i+1, text, fmt);
                colWidth[i] = std::max(colWidth[i], static_cast<int>(text.length()));
            }

            row++;
        }

        for (int i = 0; i < colWidth.size(); i++){
            const int w = static_cast<int>(colWidth[i] * 1.2);
            xlsx.setColumnWidth(i + 1, w);
        }
    }

    if (!xlsx.saveAs(outputFileName)) {
        qWarning() << "Error saving " << outputFileName;
        return false;
    }
    return true;
}

