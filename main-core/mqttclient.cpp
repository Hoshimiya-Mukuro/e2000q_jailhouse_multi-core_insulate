#include "mqttclient.h"
#include <QDebug>

mqttClient::mqttClient(QObject *parent) : QObject(parent)
{
    m_client = new QMqttClient(this);
    m_client->setHostname("192.168.79.1");
    m_client->setPort(1883);
    m_client->setClientId("小六天下第一");

    connect(m_client, &QMqttClient::connected, this, &mqttClient::onConnected);
    connect(m_client, &QMqttClient::messageReceived, this, &mqttClient::onMessageReceived);
    connect(m_client, &QMqttClient::errorChanged, this, [](QMqttClient::ClientError error) {
        if (error != QMqttClient::NoError) {
            qDebug() << "MQTT 连接错误:" << static_cast<int>(error);
        }
    });
    connect(m_client, &QMqttClient::disconnected, this, []() {
        qDebug() << "MQTT 已断开连接";
    });
    m_client->connectToHost();
}

bool mqttClient::isConnected() const
{
    return m_client && m_client->state() == QMqttClient::Connected;
}

void mqttClient::onConnected()
{
    auto subscription = m_client->subscribe(QStringLiteral("test/topic"), 0);
    if (subscription) {
        qDebug() << "成功订阅主题：test/topic";
    } else {
        qDebug() << "订阅主题失败！";
    }
    sendOneMessage();
}

void mqttClient::sendOneMessage()
{
    QString topic = "test/topic";
    QString message = "66666666";
    m_client->publish(topic, message.toUtf8(), 1);
}

void mqttClient::onMessageReceived(const QByteArray &message, const QMqttTopicName &topic)
{
    qDebug() << "收到消息:" << message;
    qDebug() << "来自主题:" << topic.name();

    // 将收到的 JSON 消息通过信号转发出去，供主窗口写入共享内存
    emit jsonMessageReceived(message);
}

void mqttClient::packJsonFile()
{
    // 预留：将传感器数据打包为 JSON 字符串
}

void mqttClient::handleJsonFile()
{
    // 预留：处理接收到的 JSON 数据
}
