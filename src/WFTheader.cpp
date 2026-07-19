#include "WFTheader.hpp"
#include <cstring>

WFTheader::WFTheader(quint64 offset, int seq, const char* fname, const char* fext) {
    this->seq = seq;
    this->offset = offset;
    data_size = 131072; // 128KB
    real_size = 0;
    total_size = 0;
    std::strncpy(file_name, fname, 127);
    file_name[127] = '\0';
    std::strncpy(ext, fext, 7);
    ext[7] = '\0';
}

void WFTheader::setSize(quint64 size) { data_size = size; }
void WFTheader::setRealSize(quint64 size) { real_size = size; }
void WFTheader::setOffset(quint64 offset) { this->offset = offset; }
void WFTheader::setTotalSize(quint64 size) { total_size = size; }

quint64 WFTheader::getOffset() const { return offset; }
quint64 WFTheader::getSize() const { return data_size; }
quint64 WFTheader::getRealSize() const { return real_size; }
quint64 WFTheader::getTotalSize() const { return total_size; }

char* WFTheader::getName() { return protocol_name; }