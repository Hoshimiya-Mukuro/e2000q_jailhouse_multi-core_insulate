#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QPushButton>
#include <QLabel>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include "sensordata.h"

QT_CHARTS_USE_NAMESPACE

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class mqttClient;
class ShmCommunicator;
class DatabaseManager;
class readingDataThread;

class MainWindow : public QMainWindow
{
    Q_OBJECT

    #define MAX_DISPLAY_POINTS 20

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateTime();
    void switchToChartMode();
    void switchToTableMode();
    void switchToLightChart();
    void showStatusDialog();

    // 从 MQTT 收到 JSON → 写入共享内存
    void onMqttMessageReceived(const QByteArray &json);
    // 从共享内存收到副核解析好的数据 → 更新 UI
    void onSensorDataReady(const SensorData &data);
    // 通信错误处理
    void onCommError(const QString &msg);

    void on_btnExport_clicked();

private:
    Ui::MainWindow *ui;
    QTimer *timer;

    // 核心模块
    mqttClient *m_mqtt;
    ShmCommunicator *m_shm;
    DatabaseManager *m_db;
    readingDataThread *m_sensorThread;

    // 导航按钮（程序化创建）
    QPushButton *btnLightChart;
    QPushButton *btnStatus;

    // 状态标签
    QLabel *m_statusLabel;

    // 图表相关 —— 温度
    QChartView *tempChartView;
    QChart *tempChart;
    QLineSeries *tempSeries;
    QValueAxis *tempAxisX;
    QValueAxis *tempAxisY;

    // 图表相关 —— 湿度
    QChartView *humidityChartView;
    QChart *humidityChart;
    QLineSeries *humiditySeries;
    QValueAxis *humidityAxisX;
    QValueAxis *humidityAxisY;

    // 图表相关 —— 光照
    QChartView *lightChartView;
    QChart *lightChart;
    QLineSeries *lightSeries;
    QValueAxis *lightAxisX;
    QValueAxis *lightAxisY;

    // 数据点计数器
    int m_dataCount;
    int m_lightDataCount;

    void initCharts();
    void initLightChart();
    void initTable();
    void initSharedMemory();
    void initDatabase();
    void updateStatusBar(const QString &text, bool ok);
    void appendTableRow(const SensorData &data);
};

#endif // MAINWINDOW_H
