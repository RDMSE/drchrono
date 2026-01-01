#include "dbmanager.h"
#include <QSqlError>
#include <QSqlQuery>

DBManager::DBManager(const QString& path) {
    if (path.trimmed().isEmpty())
        return;

    if (!isValid()) {
        m_db = QSqlDatabase::addDatabase("QSQLITE");
        m_db.setDatabaseName(path);

        if (open()) {
            qDebug() << "[DB] Connection ok";
            init();
        } else {
            qDebug() << "[DB] Connection error: " << m_db.lastError().text();
        }
    } else {
        qDebug() << "[DB] Connected";
    }
}

bool DBManager::isValid() const {
    return m_db.isValid();
}

DBManager::~DBManager() {
    if (m_db.open()) {
        m_db.close();
    }
}

void DBManager::init() const {
    if(!isOpen()) {
        qFatal("[DB] Not possible to init the database");
        return;
    }

    QSqlQuery query(m_db);

    query.exec("PRAGMA foreign_keys = ON;");
    query.exec("PRAGMA journal_mode = WAL;");
    if (query.lastError().isValid()) {
        qCritical() << "DB init error:" << query.lastError().text();
    }

    qDebug() << "Database initialized successfully";
}

bool DBManager::isOpen() const{
    return m_db.isOpen();
}

bool DBManager::open() {
    return m_db.open();
}





