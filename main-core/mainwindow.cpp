#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mqttclient.h"
#include "shmcommunicator.h"
#include "databasemanager.h"
#include "readingdatathread.h"

#include <QDateTime>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QMessageBox>
#include <QFileDialog>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_mqtt(nullptr)
    , m_shm(nullptr)
    , m_db(nullptr)
    , m_sensorThread(nullptr)
    , btnLightChart(nullptr)
    , btnStatus(nullptr)
    , m_statusLabel(nullptr)
    , tempChartView(nullptr), tempChart(nullptr), tempSeries(nullptr)
    , tempAxisX(nullptr), tempAxisY(nullptr)
    , humidityChartView(nullptr), humidityChart(nullptr), humiditySeries(nullptr)
    , humidityAxisX(nullptr), humidityAxisY(nullptr)
    , lightChartView(nullptr), lightChart(nullptr), lightSeries(nullptr)
    , lightAxisX(nullptr), lightAxisY(nullptr)
    , m_dataCount(0)
    , m_lightDataCount(0)
{
    ui->setupUi(this);

    initTable();
    initCharts();
    initLightChart();
    initSharedMemory();
    initDatabase();

    // ---- 创建 MQTT 客户端（成员变量，不再是局部变量） ----
    m_mqtt = new mqttClient(this);
    connect(m_mqtt, &mqttClient::jsonMessageReceived,
            this, &MainWindow::onMqttMessageReceived);

    // ---- 创建传感器数据读取线程 ----
    m_sensorThread = new readingDataThread(m_shm, this);
    connect(m_sensorThread, &readingDataThread::dataReady,
            this, &MainWindow::onSensorDataReady);
    connect(m_sensorThread, &readingDataThread::commError,
            this, &MainWindow::onCommError);
    m_sensorThread->start();

    // ---- 定时器：仅更新时间 ----
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateTime);
    timer->start(1000);
    updateTime();

    // ---- 导航按钮信号 ----
    connect(ui->btnChartMode, &QPushButton::clicked, this, &MainWindow::switchToChartMode);
    connect(ui->btnTableMode, &QPushButton::clicked, this, &MainWindow::switchToTableMode);
    connect(btnLightChart, &QPushButton::clicked, this, &MainWindow::switchToLightChart);
    connect(btnStatus, &QPushButton::clicked, this, &MainWindow::showStatusDialog);

    // ---- 状态标签 ----
    m_statusLabel = new QLabel(QStringLiteral("● 共享内存已就绪"), this);
    m_statusLabel->setStyleSheet(
        "background-color: #27AE60; color: white; padding: 3px 8px; border-radius: 3px;");
    // 插入到 topBarLayout 中（topBarSpacer 之后、statusLabel 之前）
    QHBoxLayout *topLayout = qobject_cast<QHBoxLayout *>(
        ui->topBarWidget->layout());
    if (topLayout) {
        // 找到 statusLabel 并在此之前插入
        int insertIdx = topLayout->count();
        for (int i = 0; i < topLayout->count(); i++) {
            QWidget *w = topLayout->itemAt(i)->widget();
            if (w && w->objectName() == QStringLiteral("statusLabel")) {
                insertIdx = i;
                break;
            }
        }
        topLayout->insertWidget(insertIdx, m_statusLabel);
    }
}

MainWindow::~MainWindow()
{
    if (m_sensorThread) {
        m_sensorThread->stop();
        m_sensorThread->quit();
        m_sensorThread->wait(3000);
    }
    delete ui;
}

// ==================== 初始化 ====================

void MainWindow::initTable()
{
    QStringList headers = {"时间", "温度", "湿度", "光照", "设备ID", "状态"};
    ui->dataTable->setColumnCount(6);
    ui->dataTable->setHorizontalHeaderLabels(headers);
    ui->dataTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void MainWindow::initCharts()
{
    // ---- 温度图表 ----
    tempSeries = new QLineSeries();
    tempChart = new QChart();
    tempChart->legend()->hide();
    tempChart->addSeries(tempSeries);

    tempAxisX = new QValueAxis();
    tempAxisX->setLabelFormat("%d");
    tempAxisX->setRange(0, MAX_DISPLAY_POINTS);
    tempAxisX->setTitleText("数据点");
    tempAxisX->setTickInterval(1);
    tempAxisX->setMinorTickCount(0);

    tempAxisY = new QValueAxis();
    tempAxisY->setRange(20, 30);
    tempAxisY->setTitleText("温度 (°C)");

    tempChart->addAxis(tempAxisX, Qt::AlignBottom);
    tempChart->addAxis(tempAxisY, Qt::AlignLeft);
    tempSeries->attachAxis(tempAxisX);
    tempSeries->attachAxis(tempAxisY);

    tempChartView = new QChartView(tempChart);
    tempChartView->setRenderHint(QPainter::Antialiasing);

    if (!ui->tempContainer->layout()) {
        ui->tempContainer->setLayout(new QVBoxLayout());
    }
    ui->tempContainer->layout()->setContentsMargins(0, 0, 0, 0);
    ui->tempContainer->layout()->addWidget(tempChartView);

    // ---- 湿度图表 ----
    humiditySeries = new QLineSeries();
    humidityChart = new QChart();
    humidityChart->legend()->hide();
    humidityChart->addSeries(humiditySeries);

    humidityAxisX = new QValueAxis();
    humidityAxisX->setLabelFormat("%d");
    humidityAxisX->setRange(0, MAX_DISPLAY_POINTS);
    humidityAxisX->setTitleText("数据点");
    humidityAxisX->setTickInterval(1);
    humidityAxisX->setMinorTickCount(0);

    humidityAxisY = new QValueAxis();
    humidityAxisY->setRange(40, 80);
    humidityAxisY->setTitleText("湿度 (%RH)");

    humidityChart->addAxis(humidityAxisX, Qt::AlignBottom);
    humidityChart->addAxis(humidityAxisY, Qt::AlignLeft);
    humiditySeries->attachAxis(humidityAxisX);
    humiditySeries->attachAxis(humidityAxisY);

    humidityChartView = new QChartView(humidityChart);
    humidityChartView->setRenderHint(QPainter::Antialiasing);

    if (!ui->humidityContainer->layout()) {
        ui->humidityContainer->setLayout(new QVBoxLayout());
    }
    ui->humidityContainer->layout()->setContentsMargins(0, 0, 0, 0);
    ui->humidityContainer->layout()->addWidget(humidityChartView);
}

void MainWindow::initLightChart()
{
    // 在 stackedWidget 的第 3 页（index 2）创建光照图表
    QWidget *lightPage = ui->stackedWidget->widget(2);
    if (!lightPage) return;

    lightSeries = new QLineSeries();
    lightChart = new QChart();
    lightChart->legend()->hide();
    lightChart->setTitle("光照实时曲线");
    lightChart->addSeries(lightSeries);

    lightAxisX = new QValueAxis();
    lightAxisX->setLabelFormat("%d");
    lightAxisX->setRange(0, MAX_DISPLAY_POINTS);
    lightAxisX->setTitleText("数据点");
    lightAxisX->setTickInterval(1);
    lightAxisX->setMinorTickCount(0);

    lightAxisY = new QValueAxis();
    lightAxisY->setRange(0, 1200);
    lightAxisY->setTitleText("光照 (lux)");

    lightChart->addAxis(lightAxisX, Qt::AlignBottom);
    lightChart->addAxis(lightAxisY, Qt::AlignLeft);
    lightSeries->attachAxis(lightAxisX);
    lightSeries->attachAxis(lightAxisY);

    lightChartView = new QChartView(lightChart);
    lightChartView->setRenderHint(QPainter::Antialiasing);

    QVBoxLayout *lightLayout = new QVBoxLayout(lightPage);
    lightLayout->setContentsMargins(4, 4, 4, 4);

    // 标题标签
    QLabel *lightTitle = new QLabel(QStringLiteral("💡 光照实时曲线"), lightPage);
    lightTitle->setFont(QFont(lightTitle->font().family(), 11, QFont::Bold));
    lightLayout->addWidget(lightTitle);

    // 图表面板
    QWidget *lightContainer = new QWidget(lightPage);
    lightContainer->setStyleSheet("background-color: #f0f0f0; border: 1px solid #ccc;");
    QVBoxLayout *containerLayout = new QVBoxLayout(lightContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->addWidget(lightChartView);
    lightLayout->addWidget(lightContainer, 1);

    // 数值标签
    QLabel *lightValue = new QLabel(QStringLiteral("800 lux"), lightPage);
    lightValue->setObjectName("labelLightValue");
    lightValue->setFont(QFont(lightValue->font().family(), 18, QFont::Bold));
    lightValue->setAlignment(Qt::AlignCenter);
    lightLayout->addWidget(lightValue);

    // 状态标签
    QLabel *lightStatus = new QLabel(QStringLiteral("光照当前状态：正常"), lightPage);
    lightStatus->setObjectName("labelLightStats");
    lightStatus->setMaximumHeight(28);
    lightStatus->setStyleSheet(
        "background-color: rgb(143, 240, 164); color: white; padding: 5px; border-radius: 3px;");
    lightStatus->setAlignment(Qt::AlignCenter);
    lightLayout->addWidget(lightStatus);

    // 添加导航按钮到 modeSwitchLayout
    btnLightChart = new QPushButton(QStringLiteral("💡 光照图表"), this);
    btnLightChart->setMinimumSize(110, 32);
    btnLightChart->setMaximumSize(110, 32);
    btnLightChart->setStyleSheet(
        "background-color: #95A5A6; color: white; padding: 4px;");

    btnStatus = new QPushButton(QStringLiteral("📡 设备状态"), this);
    btnStatus->setMinimumSize(110, 32);
    btnStatus->setMaximumSize(110, 32);
    btnStatus->setStyleSheet(
        "background-color: #8E44AD; color: white; padding: 4px;");

    // 将按钮插入到 modeSwitchLayout 中（在 spacer 之前）
    // 通过查找包含 btnChartMode 的布局来定位 modeSwitchLayout
    QHBoxLayout *modeLayout = nullptr;
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout *>(
        ui->centralWidget->layout());
    if (mainLayout) {
        for (int i = 0; i < mainLayout->count(); i++) {
            QLayout *sub = mainLayout->itemAt(i)->layout();
            if (sub) {
                // 检查此布局是否包含 btnChartMode
                for (int j = 0; j < sub->count(); j++) {
                    QWidget *w = sub->itemAt(j)->widget();
                    if (w && w->objectName() == QStringLiteral("btnChartMode")) {
                        modeLayout = qobject_cast<QHBoxLayout *>(sub);
                        break;
                    }
                }
                if (modeLayout) break;
            }
        }
    }
    if (modeLayout) {
        // 找到 spacer 的索引并在此之前插入
        int spacerIdx = -1;
        for (int i = 0; i < modeLayout->count(); i++) {
            if (modeLayout->itemAt(i)->spacerItem()) {
                spacerIdx = i;
                break;
            }
        }
        if (spacerIdx >= 0) {
            modeLayout->insertWidget(spacerIdx, btnLightChart);
            modeLayout->insertWidget(spacerIdx + 1, btnStatus);
        } else {
            modeLayout->addWidget(btnLightChart);
            modeLayout->addWidget(btnStatus);
        }
    }
}

void MainWindow::initSharedMemory()
{
    m_shm = new ShmCommunicator(this);
    if (m_shm->init()) {
        qDebug() << "共享内存初始化成功";
        updateStatusBar(QStringLiteral("● 共享内存已就绪"), true);
    } else {
        qDebug() << "共享内存初始化失败，请检查 /dev/mem";
        updateStatusBar(QStringLiteral("● 共享内存未就绪"), false);
    }
}

void MainWindow::initDatabase()
{
    m_db = new DatabaseManager(this);
    if (m_db->init()) {
        qDebug() << "数据库初始化成功，当前记录数:" << m_db->recordCount();
    } else {
        qDebug() << "数据库初始化失败";
    }
}

// ==================== 导航切换 ====================

void MainWindow::switchToChartMode()
{
    ui->stackedWidget->setCurrentIndex(0);
    ui->btnChartMode->setStyleSheet(
        "background-color: #3498DB; color: white; padding: 8px; font-weight: bold;");
    ui->btnTableMode->setStyleSheet(
        "background-color: #95A5A6; color: white; padding: 8px;");
    btnLightChart->setStyleSheet(
        "background-color: #95A5A6; color: white; padding: 4px;");
}

void MainWindow::switchToTableMode()
{
    ui->stackedWidget->setCurrentIndex(1);
    ui->btnTableMode->setStyleSheet(
        "background-color: #3498DB; color: white; padding: 8px; font-weight: bold;");
    ui->btnChartMode->setStyleSheet(
        "background-color: #95A5A6; color: white; padding: 8px;");
    btnLightChart->setStyleSheet(
        "background-color: #95A5A6; color: white; padding: 4px;");
}

void MainWindow::switchToLightChart()
{
    ui->stackedWidget->setCurrentIndex(2);
    btnLightChart->setStyleSheet(
        "background-color: #3498DB; color: white; padding: 4px; font-weight: bold;");
    ui->btnChartMode->setStyleSheet(
        "background-color: #95A5A6; color: white; padding: 8px;");
    ui->btnTableMode->setStyleSheet(
        "background-color: #95A5A6; color: white; padding: 8px;");
}

// ==================== 核心数据流 ====================

void MainWindow::onMqttMessageReceived(const QByteArray &json)
{
    // MQTT 收到 JSON 字符串 → 写入共享内存 B→A 通道，供副核处理
    if (!m_shm || !m_shm->isInitialized()) {
        qDebug() << "共享内存未就绪，无法转发 MQTT 消息到副核";
        return;
    }

    uint32_t len = static_cast<uint32_t>(json.size());
    bool ok = m_shm->sendToSecondary(
        reinterpret_cast<const uint8_t *>(json.constData()), len);

    if (ok) {
        qDebug() << "JSON 消息已写入共享内存 (" << len << "字节)";
        updateStatusBar(QStringLiteral("● 数据转发中"), true);
    } else {
        qDebug() << "共享内存写入失败（缓冲区满或消息过长）";
        updateStatusBar(QStringLiteral("● 缓冲区满"), false);
    }
}

void MainWindow::onSensorDataReady(const SensorData &data)
{
    // 副核解析完成的数据 → 更新 UI 图表、表格、标签
    m_dataCount++;
    m_lightDataCount++;

    // ---- 更新温度图表 ----
    tempSeries->append(m_dataCount, data.temperature);
    if (tempSeries->count() > MAX_DISPLAY_POINTS)
        tempSeries->remove(0);

    double tMin = 20.0, tMax = 30.0;
    if (data.temperature < tMin) tMin = data.temperature - 5.0;
    if (data.temperature > tMax) tMax = data.temperature + 5.0;
    tempAxisY->setRange(tMin, tMax);

    if (m_dataCount < MAX_DISPLAY_POINTS)
        tempAxisX->setRange(0, MAX_DISPLAY_POINTS);
    else
        tempAxisX->setRange(m_dataCount - MAX_DISPLAY_POINTS, m_dataCount);

    // ---- 更新湿度图表 ----
    humiditySeries->append(m_dataCount, data.humidity);
    if (humiditySeries->count() > MAX_DISPLAY_POINTS)
        humiditySeries->remove(0);

    double hMin = 40.0, hMax = 80.0;
    if (data.humidity < hMin) hMin = data.humidity - 10.0;
    if (data.humidity > hMax) hMax = data.humidity + 10.0;
    humidityAxisY->setRange(hMin, hMax);

    if (m_dataCount < MAX_DISPLAY_POINTS)
        humidityAxisX->setRange(0, MAX_DISPLAY_POINTS);
    else
        humidityAxisX->setRange(m_dataCount - MAX_DISPLAY_POINTS, m_dataCount);

    // ---- 更新光照图表 ----
    lightSeries->append(m_lightDataCount, data.light);
    if (lightSeries->count() > MAX_DISPLAY_POINTS)
        lightSeries->remove(0);

    if (m_lightDataCount < MAX_DISPLAY_POINTS)
        lightAxisX->setRange(0, MAX_DISPLAY_POINTS);
    else
        lightAxisX->setRange(m_lightDataCount - MAX_DISPLAY_POINTS,
                             m_lightDataCount);

    // ---- 更新数值标签 ----
    ui->labelTempValue->setText(
        QString::number(data.temperature, 'f', 1) + " °C");
    ui->labelHumidityValue->setText(
        QString::number(data.humidity, 'f', 1) + " %RH");

    // 更新光照页面数值标签
    QWidget *lightPage = ui->stackedWidget->widget(2);
    if (lightPage) {
        QLabel *lightValue = lightPage->findChild<QLabel *>("labelLightValue");
        if (lightValue)
            lightValue->setText(QString::number(data.light) + " lux");
    }

    // ---- 更新状态标签 ----
    QString tempStatus = (data.temperature > 30.0 || data.temperature < 15.0)
        ? "⚠️ 异常" : "正常";
    QString humStatus = (data.humidity > 80.0 || data.humidity < 30.0)
        ? "⚠️ 异常" : "正常";
    QString lightStatusStr = (data.light > 1000 || data.light < 100)
        ? "⚠️ 异常" : "正常";

    ui->TemplabelStats->setText("温度当前状态：" + tempStatus);
    ui->humiditylabelStats->setText("湿度当前状态：" + humStatus);

    if (lightPage) {
        QLabel *lightStats = lightPage->findChild<QLabel *>("labelLightStats");
        if (lightStats)
            lightStats->setText("光照当前状态：" + lightStatusStr);
    }

    // ---- 更新状态标签颜色 ----
    auto statusStyle = [](const QString &s) -> QString {
        if (s.contains("异常"))
            return "background-color: #E74C3C; color: white; padding: 5px; border-radius: 3px;";
        return "background-color: rgb(143, 240, 164); color: white; padding: 5px; border-radius: 3px;";
    };
    ui->TemplabelStats->setStyleSheet(statusStyle(tempStatus));
    ui->humiditylabelStats->setStyleSheet(statusStyle(humStatus));
    if (lightPage) {
        QLabel *lightStats = lightPage->findChild<QLabel *>("labelLightStats");
        if (lightStats)
            lightStats->setStyleSheet(statusStyle(lightStatusStr));
    }

    // ---- 更新表格 ----
    appendTableRow(data);

    // ---- 持久化到 SQLite ----
    if (m_db && m_db->insertRecord(data)) {
        // 静默写入成功
    }

    updateStatusBar(QStringLiteral("● 数据更新中"), true);
}

void MainWindow::onCommError(const QString &msg)
{
    qDebug() << "通信错误:" << msg;
    updateStatusBar(QStringLiteral("● 通信错误"), false);
}

// ==================== 状态显示 ====================

void MainWindow::showStatusDialog()
{
    QString status;
    status += QStringLiteral("========== 设备通信状态 ==========\n\n");

    // MQTT 状态
    if (m_mqtt) {
        status += QStringLiteral("[MQTT 客户端]\n");
        status += QStringLiteral("  连接状态: %1\n")
            .arg(m_mqtt->isConnected() ? "已连接" : "未连接");
    } else {
        status += QStringLiteral("[MQTT 客户端] 未创建\n");
    }
    status += "\n";

    // 共享内存状态
    if (m_shm) {
        status += QStringLiteral("[共享内存]\n");
        status += m_shm->statusInfo();
    } else {
        status += QStringLiteral("[共享内存] 未创建\n");
    }
    status += "\n";

    // 数据库状态
    if (m_db) {
        status += QStringLiteral("[数据库]\n");
        int count = m_db->recordCount();
        status += QStringLiteral("  记录数: %1\n").arg(count >= 0 ? count : -1);
    }

    // 线程状态
    status += QStringLiteral("\n[读取线程] ");
    status += (m_sensorThread && m_sensorThread->isRunning())
        ? "运行中" : "未运行";

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QStringLiteral("设备通信状态"));
    msgBox.setText(status);
    msgBox.exec();
}

void MainWindow::updateStatusBar(const QString &text, bool ok)
{
    if (m_statusLabel) {
        m_statusLabel->setText(text);
        m_statusLabel->setStyleSheet(
            ok
            ? "background-color: #27AE60; color: white; padding: 3px 8px; border-radius: 3px;"
            : "background-color: #E74C3C; color: white; padding: 3px 8px; border-radius: 3px;");
    }
}

// ==================== 表格更新 ====================

void MainWindow::appendTableRow(const SensorData &data)
{
    ui->dataTable->insertRow(0);
    ui->dataTable->setItem(0, 0,
        new QTableWidgetItem(QDateTime::currentDateTime().toString("hh:mm:ss")));
    ui->dataTable->setItem(0, 1,
        new QTableWidgetItem(QString::number(data.temperature, 'f', 1)));
    ui->dataTable->setItem(0, 2,
        new QTableWidgetItem(QString::number(data.humidity, 'f', 1)));
    ui->dataTable->setItem(0, 3,
        new QTableWidgetItem(QString::number(data.light)));
    ui->dataTable->setItem(0, 4,
        new QTableWidgetItem(QString::fromUtf8(data.deviceId)));
    ui->dataTable->setItem(0, 5,
        new QTableWidgetItem(QString::fromUtf8(data.status)));

    // 最多保留 10 行
    if (ui->dataTable->rowCount() > 10) {
        ui->dataTable->removeRow(10);
    }
}

// ==================== 导出 ====================

void MainWindow::on_btnExport_clicked()
{
    if (!m_db) {
        QMessageBox::warning(this, "导出失败", "数据库未初始化，无法导出。");
        return;
    }

    QString defaultPath = QDateTime::currentDateTime().toString(
        "yyyy-MM-dd_hh-mm-ss") + "_sensor_data.csv";
    QString filePath = QFileDialog::getSaveFileName(
        this, "导出数据", defaultPath, "CSV 文件 (*.csv)");

    if (filePath.isEmpty()) return;

    if (m_db->exportToCsv(filePath)) {
        QMessageBox::information(this, "导出成功",
            "数据已成功导出至:\n" + filePath);
    } else {
        QMessageBox::warning(this, "导出失败",
            "导出数据时发生错误，请检查文件权限。");
    }
}

// ==================== 时间更新 ====================

void MainWindow::updateTime()
{
    ui->timeLabel->setText(
        QDateTime::currentDateTime().toString("⏱ yyyy-MM-dd hh:mm:ss"));
}
