#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <QObject>
#include <QtMqtt/QMqttClient>

class mqttClient : public QObject
{
    Q_OBJECT
public:
    explicit mqttClient(QObject *parent = nullptr);
    ~mqttClient() = default;

    bool isConnected() const;
    void sendOneMessage();
    void packJsonFile();
    void handleJsonFile();

signals:
    // 当收到来自 MQTT 订阅主题的 JSON 消息时发射
    void jsonMessageReceived(const QByteArray &json);

private slots:
    void onConnected();
    void onMessageReceived(const QByteArray &message, const QMqttTopicName &topic);

private:
    QMqttClient *m_client;
};

#endif // MQTTCLIENT_H
