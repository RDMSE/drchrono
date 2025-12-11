#include "dbmanager.h"
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>


QSqlDatabase DBManager::m_db;


DBManager::DBManager(const QString path) {
    if (!isOpen()) {
        m_db = QSqlDatabase::addDatabase("QSQLITE");
        m_db.setDatabaseName(path);

        if (isOpen()) {
            qDebug() << "[DB] Connection ok";
            init();
        } else {
            qDebug() << "[DB] Connection error: " << m_db.lastError().text();
        }
    } else {
        qDebug() << "[DB] Connected";
    }
}


DBManager::~DBManager() {
    if (m_db.open()) {
        m_db.close();
    }
}

void DBManager::init() {
    if(!isOpen()) {
        qWarning() << "[DB] Not possible to init the tables";
        return;
    }

    createTrialsTable();
    createGroupsTable();
    createEventsTable();
}

bool DBManager::isOpen() const{
    return m_db.open();
}

bool DBManager::createTrialsTable()
{
    QSqlQuery query(m_db);

    const QString sql = R"(
        CREATE TABLE IF NOT EXISTS trials (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            startDateTime TEXT,
            endDateTime TEXT,
            createdAt TEXT DEFAULT (datetime('now')),
            notes TEXT
        );
    )";

    if (!query.exec(sql)) {
        qWarning() << "[DB] Error creating trials:"
                   << query.lastError().text();
        return false;
    }

    return true;
}

bool DBManager::createGroupsTable()
{
    QSqlQuery query(m_db);

    const QString sql = R"(
        CREATE TABLE IF NOT EXISTS groups (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            trialId INTEGER NOT NULL,
            name TEXT NOT NULL,
            FOREIGN KEY(trialId) REFERENCES trials(id)
        );
    )";

    if (!query.exec(sql)) {
        qWarning() << "[DB] Falha ao criar groups:"
                   << query.lastError().text();
        return false;
    }

    return true;
}

bool DBManager::createEventsTable()
{
    QSqlQuery query(m_db);

    const QString sql = R"(
        CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            trialId INTEGER NOT NULL,
            groupName TEXT NOT NULL,
            placa TEXT NOT NULL,
            startTime TEXT NOT NULL,
            endTime TEXT,
            durationMs INTEGER,
            notes TEXT,
            FOREIGN KEY(trialId) REFERENCES trials(id)
        );
    )";

    if (!query.exec(sql)) {
        qWarning() << "[DB] Error creating events:"
                   << query.lastError().text();
        return false;
    }

    return true;
}


