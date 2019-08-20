#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include "rpc_base.h"
#include "client_server.h"
#include "preemptive_archive.h"
#include "example_rpc_interface.h"


enum class MessageId {
    PLAIN_TEXT,
    RPC_CALL,

    COUNT
};

using Session = client_server::Session<>;

struct RpcSession : public Session, public IRpcConnection {
    RpcSession(cz::spas::Service& service)
        : Session(service)
        , rpcInterface(this)
    {
        // RPC bindings
        rpcInterface.sendInt = [](int i) {
            std::cout << "Received int value = " << i << "\n";
        };

        rpcInterface.createAccount = [](int id, const std::string& name) {
            std::cout << "Account created: {" << id << ": " << name << "}\n";
        };

        rpcInterface.ping = [this]() { std::cout << "Got ping, sending pong\n"; rpcInterface.pong(); };
        rpcInterface.pong = []() { std::cout << "Got pong\n"; };
        rpcInterface.square = [](double d) { return d * d; };
    }

    void sendRpc(rpc::RpcPacket<MyRpcPayload>&& packet) override {
        NetMessage msg;
        PreemptiveStream stream;

        // potential problem here with payload size: use growing data storage or archive
        // that checks for overflow
        stream.getArchive().preemptWrite(reinterpret_cast<unsigned char*>(msg.payload()));
        stream << packet;

        msg.set_id(static_cast<uint16_t>(MessageId::RPC_CALL));
        msg.set_payload_size(stream.getArchive().totalSize);

        send(std::move(msg));
    }

    void onMessage(size_t id, NetMessage&& msg) override {
        if (static_cast<uint16_t>(MessageId::PLAIN_TEXT) == id) {
            std::cout << "message received [" << id << "]: '" << msg.payload() << "'\n";
        } else if (static_cast<uint16_t>(MessageId::RPC_CALL) == id) {
            rpc::RpcPacket<MyRpcPayload> packet;
            PreemptiveStream stream;

            stream.getArchive().preemptRead(reinterpret_cast<unsigned char*>(msg.payload()));
            stream >> packet;
            try {
                rpcInterface.dispatch(packet);
            } catch (std::exception& e) {
                std::cout << "Exception thrown in rpcHandler\n";
            }
        } else {
            std::cout << "Unknown message type received! " << id << "\n";
        }
    }

    ExampleRpc rpcInterface;
};


void client_loop() {
    cz::spas::Service service;
    auto session = std::make_shared<RpcSession>(service);
    session->connect("127.0.0.1", 7777);
    session->start();

    std::thread clientLoop([&service]() { service.run(); });

    std::string s;
    while (true) {
        std::cout << ">>";
        std::cin >> s;

        if (s == "sendInt") {
            int i;
            std::cin >> i;
            session->rpcInterface.sendInt(i);
        } else if (s == "createAccount") {
            int id; std::string name;
            std::cin >> id >> name;
            session->rpcInterface.createAccount(id, name);
        } else if (s == "ping") {
            session->rpcInterface.ping();
        } else if (s == "square") {
            double d;
            std::cin >> d;
            std::cout << session->rpcInterface.square(d).get() << "\n";
        } else {
            RpcSession::NetMessage msg;
            s += '\0';
            msg.from_payload(static_cast<uint16_t>(MessageId::PLAIN_TEXT), s.c_str(), s.size());
            session->send(std::move(msg));
        }
    }
    clientLoop.join();
}

void server_loop() {
    cz::spas::Service service;
    client_server::Server<RpcSession> server(service, 7777);
    service.run();
}

int main() {
    std::thread serverThread(server_loop);
    client_loop();
    serverThread.join();
}
