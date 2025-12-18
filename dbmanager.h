#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QString>
#include <QSqlDatabase>
#include <tl/expected.hpp>

class DBManager
{
public:
    DBManager(const QString path);
    ~DBManager();

    bool isOpen() const;

    bool open();

    inline QSqlDatabase database() const {
        return QSqlDatabase::database();
    }

private:
    QSqlDatabase m_db;

    void init();
    bool isValid() const;
};

#endif // DBMANAGER_H
