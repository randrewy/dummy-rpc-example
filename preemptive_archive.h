#pragma once
#include "Archive/archive.h"

struct PreemptiveStorage {
    unsigned char* writeData;
    const unsigned char* readData;
    size_t totalSize = 0;

    size_t write(const unsigned char* ptr, size_t size) {
        std::memcpy(writeData, ptr, size);
        writeData += size;
        totalSize += size;
        return size;
    }

    void read(unsigned char* ptr, size_t size) {
        std::memcpy(ptr, readData, size);
        readData += size;
    }

    void preemptRead(const unsigned char* ptr) {
        readData = ptr;
    }

    void preemptWrite(unsigned char* ptr) {
        writeData = ptr;
        totalSize = 0;
    }
};

using PreemptiveArchive = archive::BinaryArchive<PreemptiveStorage, archive::storage_policy::Parent>;
using PreemptiveStream = archive::ArchiveStream<PreemptiveArchive, archive::Direction::Bidirectional>;
