#ifndef FILETRANSFER_HPP
#define FILETRANSFER_HPP

#include <QObject> 
#include <QVector>
#include <QString>
#include <QFile>
#include <QMap>



class QTcpServer;  
class QTcpSocket;

struct ClientFileContext {
    QString tempFilePath;
    QString metaFilePath;     
    QFile* file = nullptr;
    QFile* metafile = nullptr;
    QString finalFileName;     
    quint64 totalSize = 0;     
    quint64 receivedBytes = 0;
    QByteArray recvBuffer; 
    bool isCompleted = false;  
};
class FileTransfer : public QObject {
    Q_OBJECT
private:
    QTcpServer* ts = nullptr;                  // 服务端
    QVector<QTcpSocket*> clients;              // 客户端列表
    QMap<QTcpSocket*, ClientFileContext> clientTemp; // 接收端缓存
    int maxConnections;                        // 最大连接数
    int connections = 0;                       // 实际连接数

    bool m_isClient = false;
    QTcpSocket* m_clientSocket = NULL;

    QFile* m_sendFile = nullptr;               // 正在发送的文件
    quint64 m_sendOffset = 0;                  // 当前读取偏移
    quint64 m_sendTotal = 0;                   // 文件总大小
    int m_sendSeq = 0;                         // 当前块序号
    QByteArray m_sendBaseName;                 // 文件名（不含扩展）
    QByteArray m_sendExt;                      // 扩展名
    QVector<QTcpSocket*> m_sendTargets;        // 本次发送的目标套接字列表
    bool m_sending = false;                    // 是否正在发送

    void save_file(QTcpSocket* client);
    void sendNextBlock();
public:
    FileTransfer(QObject* parent = nullptr, const quint16 port = 0, const int Connections = 8, bool is_server = true);
    void onNewConnection();
    void onClientDisconnected();
    void send(const QString file_path);
    void onReadyRead();
    QString OpenFileDialog();
    int getConnections();

    bool connectToServer(const QString& ip, quint16 port);
    bool isClientMode() const { return m_isClient; }
};




#endif