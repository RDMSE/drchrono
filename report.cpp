#include "report.h"
#include <QXlsx/header/xlsxdocument.h>
#include <QSqlQuery>
#include <QString>
#include <QSqlError>
#include <algorithm>

bool Report::exportExcel(int trialId, const QString& outputFileName, QSqlDatabase db) {

    if (trialId == -1) return false;

    const QString timeFormat = "hh:MM:ss";

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
        SELECT DISTINCT groupName
        FROM events
        WHERE trialId = :trialId
        ORDER BY groupName
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

        QSqlQuery evQuery(db);
        sql = R"(
            SELECT e1.placa, e1.startTime, e1.endTime, e1.durationMs as duration, e1.notes
            FROM events e1
            INNER JOIN (
                SELECT placa, MAX(endTime) AS lastEnd
                FROM events
                WHERE 1 = 1
                    AND trialId = :trialId
                    AND groupName = :grp
                GROUP BY placa
            ) e2 ON e1.placa = e2.placa AND e1.endTime = e2.lastEnd
            WHERE 1 = 1
                AND e1.trialId = :trialId
                AND e1.groupName = :grp
            ORDER BY e1.durationMs
        )";

        evQuery.prepare(sql);
        evQuery.bindValue(":trialId", trialId);
        evQuery.bindValue(":grp", groupName);

        if (!evQuery.exec()) {
            qWarning() << "Erro getting events:" << evQuery.lastError().text();
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
                Report::msToHHMMSS(evQuery.value(3).toInt()),
                evQuery.value(4).toString()
            };

            for (int i = 0; i < values.length(); i++) {
                QString text = values[i];
                xlsx.write(row, i+1, text, fmt);
                colWidth[i] = std::max(colWidth[i], static_cast<int>(text.length()));
            }

            row++;
        }

        for (int i = 0; i < colWidth.size(); i++){
            int w = colWidth[i] * 1.2;
            xlsx.setColumnWidth(i + 1, w);
        }
    }

    if (!xlsx.saveAs(outputFileName)) {
        qWarning() << "Error saving " << outputFileName;
        return false;
    }
    return true;
}

