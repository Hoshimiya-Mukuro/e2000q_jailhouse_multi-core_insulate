#include "databasemanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
    , m_initialized(false)
{
}

DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::init(const QString &dbPath)
{
    QString path = dbPath;
    if (path.isEmpty()) {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        path = dir + QStringLiteral("/sensor_data.db");
    }

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    m_db.setDatabaseName(path);

    if (!m_db.open()) {
        qDebug() << "DatabaseManager: 无法打开数据库:" << m_db.lastError().text();
        return false;
    }

    QSqlQuery query(m_db);
    bool ok = query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS sensor_readings ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  device_id TEXT NOT NULL,"
        "  timestamp INTEGER NOT NULL,"
        "  temperature REAL,"
        "  humidity REAL,"
        "  light INTEGER,"
        "  status TEXT"
        ")"));

    if (!ok) {
        qDebug() << "DatabaseManager: 创建表失败:" << query.lastError().text();
        return false;
    }

    m_initialized = true;
    qDebug() << "DatabaseManager: 数据库已就绪, 路径:" << path;
    return true;
}

bool DatabaseManager::insertRecord(const SensorData &data)
{
    if (!m_initialized) return false;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO sensor_readings "
        "(device_id, timestamp, temperature, humidity, light, status) "
        "VALUES (:device_id, :timestamp, :temp, :hum, :light, :status)"));

    query.bindValue(QStringLiteral(":device_id"), QString::fromUtf8(data.deviceId));
    query.bindValue(QStringLiteral(":timestamp"), static_cast<qlonglong>(data.timestamp));
    query.bindValue(QStringLiteral(":temp"), data.temperature);
    query.bindValue(QStringLiteral(":hum"), data.humidity);
    query.bindValue(QStringLiteral(":light"), data.light);
    query.bindValue(QStringLiteral(":status"), QString::fromUtf8(data.status));

    if (!query.exec()) {
        qDebug() << "DatabaseManager: 插入记录失败:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::exportToCsv(const QString &filePath)
{
    if (!m_initialized) return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "DatabaseManager: 无法创建导出文件:" << filePath;
        return false;
    }

    QTextStream stream(&file);
    // BOM for Excel compatibility
    stream << "\xEF\xBB\xBF";
    stream << QStringLiteral("ID,设备ID,时间戳,温度(°C),湿度(%RH),光照,状态\n");

    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral("SELECT * FROM sensor_readings ORDER BY id"))) {
        while (query.next()) {
            stream << query.value(0).toInt() << ","
                   << query.value(1).toString() << ","
                   << query.value(2).toLongLong() << ","
                   << query.value(3).toDouble() << ","
                   << query.value(4).toDouble() << ","
                   << query.value(5).toInt() << ","
                   << query.value(6).toString() << "\n";
        }
    }

    file.close();
    qDebug() << "DatabaseManager: 数据已导出至" << filePath;
    return true;
}

int DatabaseManager::recordCount() const
{
    if (!m_initialized) return -1;

    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM sensor_readings")) && query.next()) {
        return query.value(0).toInt();
    }
    return -1;
}
