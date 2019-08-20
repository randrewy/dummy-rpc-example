#pragma once
#include <future>
#include <mutex>
#include <unordered_map>
#include "rpc_connection.h"

struct INetRpc : public rpc::RpcInterface<INetRpc, MyRpcPayload> {
    INetRpc (IRpcConnection* rpcConnection) {
        connection = rpcConnection;
    }

    template<typename R>
    auto sendRpcPacket(rpc::RpcPacket<MyRpcPayload>&& packet) {
        if constexpr (!std::is_same_v<R, void>) {
            auto promise = std::make_shared<std::promise<R>>();
            auto future = promise->get_future();

            std::lock_guard<std::mutex> lk(continuations_mutex);
            continuations[packet.callId] = [promise = std::move(promise)](const void* resultPrt) mutable {
                const R* result = static_cast<const R*>(resultPrt);
                promise->set_value(*result);
            };

            connection->sendRpc(std::move(packet));
            return future;
        }

        connection->sendRpc(std::move(packet));
    }

    template<typename R>
    void onResultReturned(uint32_t callId, const R& result) {
        std::lock_guard<std::mutex> lk(continuations_mutex);

        auto pairIt = continuations.find(callId);
        if (pairIt != continuations.end()) {
            pairIt->second(&result);
            continuations.erase(pairIt);
        }
    }

protected:
    void onPacket (rpc::RpcPacket<MyRpcPayload>&& packet) {
        dispatch(packet);
    }

    std::unordered_map<uint32_t, std::function<void(const void*)>> continuations;
    std::mutex continuations_mutex;
    IRpcConnection* connection;
};
