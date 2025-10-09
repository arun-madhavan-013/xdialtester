/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <functional>
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include "json/json.h"

// Our websocket client
typedef websocketpp::client<websocketpp::config::asio_client> wsclient;
// Pointer to response
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING,
    ERROR_STATE
};

// Event callback type for notifications
using EventCallback = std::function<void(const Json::Value&)>;

class TransportHandler
{
    std::string m_wsUrl = "ws://127.0.0.1:9998/jsonrpc";
    websocketpp::connection_hdl m_wsHdl;
    wsclient m_client;

    std::atomic<ConnectionState> m_connectionState{ConnectionState::DISCONNECTED};
    std::mutex m_stateMutex;
    std::condition_variable m_stateChanged;

    std::atomic<uint32_t> m_requestIdCounter{1};

    std::function<void(bool)> m_conHandler;
    std::function<void(std::string)> m_msgHandler;
    EventCallback m_eventHandler;

public:
    TransportHandler() : m_conHandler(nullptr), m_msgHandler(nullptr), m_eventHandler(nullptr)
    {
    }

    void setConnectURL(const std::string &url)
    {
        m_wsUrl = url;
    }
    std::string getConnectURL()
    {
        return m_wsUrl;
    }

    bool isConnected()
    {
        return m_connectionState.load() == ConnectionState::CONNECTED;
    }

    ConnectionState getConnectionState() const
    {
        return m_connectionState.load();
    }

    std::string generateRequestId()
    {
        return std::to_string(m_requestIdCounter.fetch_add(1));
    }

    bool waitForConnection(std::chrono::milliseconds timeout);

    int initializeTransport();
    void registerConnectionHandler(std::function<void(bool)> callback);
    void registerMessageHandler(std::function<void(const std::string)> callback);
    void registerEventHandler(EventCallback callback);
    void connect();
    int sendMessage(std::string message);
    void disconnect();

private:
    void connected(websocketpp::connection_hdl hdl);
    void connectFailed(websocketpp::connection_hdl hdl);
    void processResponse(websocketpp::connection_hdl hdl, message_ptr msg);
    void disconnected(websocketpp::connection_hdl hdl);
};
