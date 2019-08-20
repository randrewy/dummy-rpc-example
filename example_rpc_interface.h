#pragma once
#include "rpc_connection.h"
#include "rpc_base.h"

struct ExampleRpc : INetRpc {
    ExampleRpc(IRpcConnection* connection) : INetRpc(connection) {}

    Rpc<void(int value)> sendInt = this;
    Rpc<void(int id, const std::string& name)> createAccount = this;
    Rpc<void()> ping = this;
    Rpc<void()> pong = this;

    Rpc<double(double value)> square = this;
};
