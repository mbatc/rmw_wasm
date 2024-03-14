#include <string>
#include <optional>

#include "rcutils/logging_macros.h"

#include <emscripten/emscripten.h>
#include <emscripten/val.h>

#include "wasm_cpp/context.hpp"
#include "wasm_cpp/subscriber.hpp"
#include "wasm_cpp/participant.hpp"


namespace wasm_cpp
{
    Subscriber::Subscriber(const std::string & topic_name)
        : Participant(topic_name, "subscriber")
    {
        RCUTILS_LOG_DEBUG_NAMED("wasm_cpp", "trace Subscriber::Subscriber()");

        get_global_context()->register_subscriber(this);
    }

    Subscriber::~Subscriber()
    {
        RCUTILS_LOG_DEBUG_NAMED("wasm_cpp", "trace Subscriber::~Subscriber()");

        get_global_context()->unregister_subscriber(this);
    }

    std::string Subscriber::get_message()
    {
        RCUTILS_LOG_DEBUG_NAMED("wasm_cpp", "trace Subscriber::get_message()");

        std::scoped_lock guard {m_lock};

        if (m_messages.size() == 0) {
            return "";
        }

        std::string message = m_messages.front();
        m_messages.pop();

        return message;
    }

    bool Subscriber::has_message() const
    {
        RCUTILS_LOG_DEBUG_NAMED("wasm_cpp", "trace Subscriber::has_message()");

        std::scoped_lock guard {m_lock};
        return m_messages.size() > 0;
    }

    void Subscriber::push_message(const std::string &message)
    {
        RCUTILS_LOG_DEBUG_NAMED("wasm_cpp", "trace Subscriber::push_message()");

        std::unique_lock guard {m_lock};
        m_messages.push(message);
        auto callbacks = m_message_cb; // Copy callbacks so we can unlock before calling them
        guard.unlock();

        for (auto &cb : callbacks)
            cb.second();
    }

    int64_t Subscriber::add_message_cb(std::function<void()> cb)
    {
        static int64_t next_cb_id = 0;

        std::scoped_lock guard {m_lock};
        int64_t id = next_cb_id++;
        m_message_cb.emplace_back(id++, cb);
        return id;
    }

    void Subscriber::remove_message_cb(int64_t id)
    {
        std::scoped_lock guard {m_lock};
        for (auto it = m_message_cb.begin(); it != m_message_cb.end(); ++it)
            if (it->first == id)
                it = m_message_cb.erase(it);
    }
} // namespace wasm_cpp
