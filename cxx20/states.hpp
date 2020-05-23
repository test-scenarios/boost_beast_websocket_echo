#pragma once
#include "config.hpp"
#include "util/async_queue.hpp"

#include <deque>
#include <iostream>
#include <queue>
#include <string_view>

namespace project
{
    /// The read state.
    ///
    /// Responsibilites:
    /// - Read messages and call on_msg (a function call) when a message has
    /// been received
    /// @exception will throw a system_error if the websocket closes or there is
    /// a transport error
    template < class NextLayer, class OnMessage >
    net::awaitable< void >
    websocket_rx_state(websocket::stream< NextLayer > &s, OnMessage &&on_msg)
    try
    {
        beast::flat_buffer rxbuffer;
        while (1)
        {
            auto bytes   = co_await s.async_read(rxbuffer, net::use_awaitable);
            auto message = beast::buffers_to_string(rxbuffer.data());
            std::cout << " received: " << message << "\n";
            rxbuffer.consume(message.size());
            on_msg(std::move(message));
        }
    }
    catch (system_error &)
    {
        if (auto r = s.reason(); r)
            std::cout << "connection closed with : " << r.reason << " : "
                      << r.code << std::endl;
    }

    /// Transmit state (orthogonal region)
    ///
    /// The transmit state cycles through the states initial_state -> sending ->
    /// initial_state It maintains a message transmit buffer
    struct chat_tx_state
    {
        chat_tx_state(net::executor exec)
        : txqueue(exec)
        {
        }

        beast_fun_times::util::async_queue< std::string > txqueue;
    };

    /// Run the transmit state until the tx queue is stopped
    ///
    template < class Transport >
    net::awaitable< void >
    run_state(chat_tx_state &state, websocket::stream< Transport > &stream)
    {
        for (;;)
        {
            co_await stream.async_write(
                net::buffer(
                    co_await state.txqueue.async_pop(net::use_awaitable)),
                net::use_awaitable);
        }
    }

    /// Chat state data that does not depend on transport type
    struct chat_state_base
    {
        error_code ec;
        enum
        {
            initial_state,
            handshaking,
            chatting,
            exit_state,
        } state = initial_state;

    };

    /// Chat state data dependent on underlying transport (and therefore
    /// executor type)
    template < class Transport >
    struct chat_state : chat_state_base
    {
        using executor_type = typename Transport::executor_type;
        using stream_type   = websocket::stream< Transport >;

        chat_state(Transport t)
        : stream(std::move(t))
        , tx_state(get_executor())
        {
        }

        auto
        get_executor()
        {
            return stream.get_executor();
        }

        /// Coroutine to notify this state and any substates of a server-side
        /// error. net::error::operation_aborted is the correct code to use for
        /// a SIGNINT response
        net::awaitable< void >
        notify_error(error_code nec)
        {
            assert(nec);
            if (!ec)
            {
                ec = nec;
                switch (state)
                {
                case chat_state_base::initial_state:
                    break;
                case chat_state_base::handshaking:
                    stream.next_layer().cancel();
                    tx_state.txqueue.stop();
                    break;
                case chat_state_base::chatting:
                    tx_state.txqueue.stop();
                    co_await stream.async_close(
                        websocket::close_reason("shutting down"),
                        net::use_awaitable);
                    break;
                case chat_state_base::exit_state:
                    break;
                }
            }
        }

        /// Coroutine which notifies the state that there is a new "transmit
        /// message" event
        net::awaitable< void >
        notify_send(std::string message)
        {
            tx_state.txqueue.push(std::move(message));
            switch (state)
            {
            case chat_state_base::initial_state:
                break;
            case chat_state_base::handshaking:
                break;
            case chat_state_base::chatting:
                co_await run_state(tx_state, stream, ec);
                break;
            case chat_state_base::exit_state:
                break;
            }
        }

        stream_type stream;

        // substates
        chat_tx_state tx_state;

    };

    /// Coroutine which runs the chat state
    /// \tparam Transport
    /// \tparam OnConnected
    /// \tparam OnMessage
    /// \param state reference to state data
    /// \param on_connected callback to be called if handshake is successful
    /// \param on_message callback to be called on every message received
    /// \return net::awaitable<void>
    template < class Transport, class OnConnected, class OnMessage >
    net::awaitable< void >
    run_state(chat_state< Transport > &state,
              OnConnected &&           on_connected,
              OnMessage &&             on_message)
    try
    {
        assert(state.state == chat_state_base::initial_state);
        state.state = chat_state_base::handshaking;
        co_await state.stream.async_accept(net::use_awaitable);
        if (state.ec)
            throw system_error(state.ec);

        state.state = chat_state_base::chatting;
        on_connected();
        co_await websocket_rx_state(state.stream,
                                    std::forward< OnMessage >(on_message));

        state.state = chat_state_base::exit_state;
        co_return;
    }
    catch (...)
    {
        state.state = chat_state_base::exit_state;
        throw;
    }
}   // namespace project