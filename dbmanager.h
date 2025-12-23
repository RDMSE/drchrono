#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QSqlDatabase>

class DBManager
{
public:
    explicit DBManager(const QString& path);
    ~DBManager();

    [[nodiscard]] bool isOpen() const;

    bool open();

    [[nodiscard]] static QSqlDatabase database() {
        return QSqlDatabase::database();
    }

private:
    QSqlDatabase m_db;

    void init() const;
    [[nodiscard]] bool isValid() const;
};

#endif // DBMANAGER_H
