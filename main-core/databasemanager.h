#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include "sensordata.h"

class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();

    bool init(const QString &dbPath = QString());
    bool insertRecord(const SensorData &data);
    bool exportToCsv(const QString &filePath);
    int recordCount() const;

private:
    QSqlDatabase m_db;
    bool m_initialized;
};

#endif // DATABASEMANAGER_H
