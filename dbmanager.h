#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QString>
#include <QSqlDatabase>

class DBManager
{
public:
    DBManager(const QString path);
    ~DBManager();

    bool isOpen() const;

private:
    static QSqlDatabase m_db;

    void init();
    bool createTrialsTable();
    bool createGroupsTable();
    bool createEventsTable();
};

#endif // DBMANAGER_H
