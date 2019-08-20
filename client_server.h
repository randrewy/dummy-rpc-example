#pragma once
#include <memory>
#include "czspas/source/crazygaze/spas/spas.h"

namespace client_server {

namespace defaults {

template<size_t size = 1024>
struct DefaultArrayBuffer {
    char* data() {
        return buffer;
    }

    void resize(size_t newSize) {
        assert(newSize < size);
    }
protected:
    char buffer[size];
};

} // namespace defaults

struct Header {
    // uint16_t id;
    // uint16_t size;
    enum { header_size = sizeof (uint16_t) + sizeof (uint16_t) };

    static uint16_t& get_id(char* header_data) {
        return *reinterpret_cast<uint16_t*>(header_data);
    }
    static uint16_t get_id(const char* header_data) {
        return *reinterpret_cast<const uint16_t*>(header_data);
    }

    static uint16_t get_payload_size(const char* header_data) {
        return *reinterpret_cast<const uint16_t*>(header_data + sizeof (uint16_t));
    }
    static uint16_t& get_payload_size(char* header_data) {
        return *reinterpret_cast<uint16_t*>(header_data + sizeof (uint16_t));
    }
};

template<typename BufferType = defaults::DefaultArrayBuffer<>>
struct NetMessage : BufferType {
    using Buffer = BufferType;
    using Buffer::resize;
    using Buffer::data;

    void read_header(const char* header) {
        resize(Header::header_size);

        std::memcpy(data(), header, Header::get_payload_size(header) + Header::header_size);
    }

    void from_data(const char* dataPtr) {
        const size_t payloadSize = Header::get_payload_size(dataPtr);
        const size_t dataSize = payloadSize + Header::header_size;
        resize(dataSize);

        std::memcpy(data(), dataPtr, dataSize);
    }

    void from_payload(uint16_t id, const char* payload, size_t size) {
        resize(size + Header::header_size);

        Header::get_id(data()) = id;
        Header::get_payload_size(data()) = static_cast<uint16_t>(size);
        std::memcpy(data() + Header::header_size, payload, size);
    }

    char* payload() { return data() + Header::header_size; }

    void set_id(uint16_t id) { Header::get_id(data()) = id; }
    void set_payload_size(size_t size) { Header::get_payload_size(data()) = size; }

    uint16_t payload_size() { return Header::get_payload_size(data()); }
    uint16_t id() { return Header::get_id(data()); }
    uint16_t size() { return payload_size() + Header::header_size; }
};


template <typename NetMessageType = NetMessage<>>
class Session : public std::enable_shared_from_this<Session<NetMessageType>>
{
public:
    using NetMessage = NetMessageType;

    Session(cz::spas::Service& service)
        : socket(service)
    {}

    virtual ~Session() {
        auto addr = socket.getPeerAddr();
        printf("Finishing session with %s:%d\n", addr.first.c_str(), addr.second);
    }

    void connect(const char* host, int port) {
        auto ec = socket.connect(host, port);
        if (ec) {
            printf("Error connecting: %s\n", ec.msg());
        }
    }

    void start() {
        auto addr = socket.getPeerAddr();
        printf("Starting session with %s:%d\n", addr.first.c_str(), addr.second);

        read_header_and_body();
    }

    virtual void send (NetMessage&& msg) {
        std::lock_guard<std::mutex> lk(write_queue_mutex);

        const bool write_in_progress = !write_queue.empty();
        write_queue.emplace_back(std::move(msg));
        if (!write_in_progress) {
            do_write();
        }
    }

    virtual void onMessage (size_t, NetMessage&&) {}

protected:

    void read_header_and_body() {
        auto self(this->shared_from_this());

        read_message.resize(Header::header_size);
        socket.asyncReceiveSome(read_message.data(), Header::header_size, -1, [this, self](const cz::spas::Error& ec, size_t)
        {
            if (ec) {
                printf("Error on reading header: %s\n", ec.msg());
                return;
            }
            read_body();
        });
    }

    void read_body() {
        auto self(this->shared_from_this());
        const auto size = read_message.size();
        const auto id = read_message.id();
        const auto payloadSize = read_message.payload_size();

        read_message.resize(size);
        socket.asyncReceiveSome(read_message.payload(), payloadSize, -1,
                                [this, self, id](const cz::spas::Error& ec, size_t) {
            if (ec) {
                printf("Error on reading body: %s\n", ec.msg());
                return;
            }

            NetMessage msg = std::move(read_message);
            onMessage(id, std::move(msg));

            // loop
            read_header_and_body();
        });
    }

    void do_write() {
        auto self(this->shared_from_this());
        asyncSend(socket, write_queue.front().data(), write_queue.front().size(), -1, [this, self](const cz::spas::Error& ec, size_t)
        {
            if (!ec) {
                std::lock_guard<std::mutex> lk(write_queue_mutex);
                write_queue.pop_front();
                if (!write_queue.empty()) {
                    do_write();
                }
            } else {
                printf("Error on writing: %s\n", ec.msg());
            }
        });
    }


    template<typename T> friend class Server;

    cz::spas::Socket socket;

    NetMessage read_message;
    std::deque<NetMessage> write_queue;
    std::mutex write_queue_mutex;
};

template<typename Session = Session<>>
class Server {
public:
    Server(cz::spas::Service& service, int port)
        : acceptor(service)
    {
        auto ec = acceptor.listen(port);
        if (ec) {
            throw std::runtime_error(ec.msg());
        }

        do_accept();
    }

    virtual std::shared_ptr<Session> createSession(cz::spas::Service& service) {
        return std::make_shared<Session>(service);
    }

private:
    void do_accept()
    {
        auto session = createSession(acceptor.getService());
        acceptor.asyncAccept(session->socket, -1, [this, session](const cz::spas::Error& ec)
        {
            if (ec) {
                throw std::runtime_error(ec.msg());
            }

            session->start();
            do_accept();
        });
    }

    cz::spas::Acceptor acceptor;
};

} // namespace client_server
