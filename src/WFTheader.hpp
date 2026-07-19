#ifndef WFTHEADER_HPP
#define WFTHEADER_HPP

#include <QtGlobal>
#pragma pack(push, 1)

class WFTheader {
private: 
    char protocol_name[32] = "WIFI_FileTransfer";
    quint64 offset;           // 字节偏移量
    quint64 data_size;        // 设定数据块大小
    quint64 real_size = 0;    // 实际数据块大小
    quint64 total_size = 0;   // 原始文件大小
    
public:
    int seq;                  // 块序号
    char file_name[128];      // 原始文件名
    char ext[8];              // 原始后缀
    static constexpr int MAGIC = 0x57465448;
    static constexpr int HEADERSIZE = 208;

    WFTheader(quint64 offset = 0, int seq = 0, const char* fname = "", const char* fext = "");

    void setSize(quint64 size);
    void setRealSize(quint64 size);
    void setOffset(quint64 offset);
    void setTotalSize(quint64 size);   

    quint64 getOffset() const;
    quint64 getSize() const;
    quint64 getRealSize() const;
    quint64 getTotalSize() const;     

    char* getName();
};

#pragma pack(pop)
#endif