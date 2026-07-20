#ifndef HTTPFILESERVER_HPP
#define HTTPFILESERVER_HPP

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QByteArray>
#include <QHash>
#include <QWidget>

class HttpFileServer : public QObject {
    Q_OBJECT
public:
    explicit HttpFileServer(QObject *parent = nullptr);
    ~HttpFileServer();

    bool start(quint16 port, const QString &shareDir = QString());
    void stop();
    bool isRunning() const { return m_server && m_server->isListening(); }

    QString shareDirectory() const { return m_shareDir; }
    void setShareDirectory(const QString &dir);
    bool selectAndSetShareDirectory(QWidget *parent = nullptr);
    quint16 serverPort() const { return m_server ? m_server->serverPort() : 0; }

signals:
    void started();
    void stopped();
    void error(const QString &msg);
    void shareDirectoryChanged(const QString &newDir);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    QTcpServer *m_server = nullptr;
    QString m_shareDir;
    QHash<QTcpSocket*, QByteArray> m_buffers; 

    void processRequest(QTcpSocket *socket, const QByteArray &request);
    void sendFile(QTcpSocket *socket, const QString &filePath);
    void sendError(QTcpSocket *socket, int code, const QString &msg);
    void sendDirectoryListing(QTcpSocket *socket);
};

#endif 