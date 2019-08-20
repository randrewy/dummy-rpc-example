#pragma once
#include "Rpc/rpc.h"
#include "preemptive_archive.h"

struct MyRpcPayload {
    unsigned char buffer[1024];
    size_t size = 0;

    template<typename... Args>
    void serialize(Args&&... args) {
        PreemptiveArchive archive;
        archive.preemptWrite(buffer);

        (archive.serialize(args),...);
        size = archive.totalSize;
    }

    template<typename Tuple>
    decltype(auto) deserialize() const {
        PreemptiveArchive archive;
        archive.preemptRead(buffer);

        Tuple tuple;
        archive.deserialize(tuple);
        return tuple;
    }
};

archive::usize serialize_object(const MyRpcPayload& payload, PreemptiveArchive& archive) {
    return archive.serialize(payload.size)
            + archive.write(payload.buffer, payload.size);
}

void deserialize_object(MyRpcPayload& payload, PreemptiveArchive& archive) {
    archive.deserialize(payload.size);
    archive.read(payload.buffer, payload.size);
}

template<archive::Direction policy>
void stream_serialization (archive::ArchiveStream<PreemptiveArchive, policy>& stream, archive::ArgumentRef<rpc::RpcPacket<MyRpcPayload>, policy>& t) {
     stream & t.instanceId
             & t.functionId
             & t.callId
             & t.callType
             & t.payload;
}

class IRpcConnection {
public:
    virtual ~IRpcConnection() {}
    virtual void sendRpc(rpc::RpcPacket<MyRpcPayload>&& packet) = 0;
    std::function<void(rpc::RpcPacket<MyRpcPayload>&& packet)> onReceivedPacket;
};
