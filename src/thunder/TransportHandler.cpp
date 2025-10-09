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

#include "TransportHandler.h"
#include "EventUtils.h"
#include <thread>
#include <string>
#include <memory>
#include "json/json.h"

#include <iostream>

int TransportHandler::initializeTransport()
{

    int status = 0;
    try
    {

        m_client.clear_access_channels(websocketpp::log::alevel::frame_header);
        m_client.clear_access_channels(websocketpp::log::alevel::frame_payload);

        // Clear all logs
        //  m_client.clear_error_channels(websocketpp::log::elevel::none);

        // set logging policy if needed
        // m_client.set_access_channels(websocketpp::log::alevel::all);
        // m_client.set_error_channels(websocketpp::log::elevel::all);

        // Initialize ASIO
        m_client.init_asio();

        // Register our handlers
        m_client.set_open_handler([&, this](websocketpp::connection_hdl hdl)
                                  { connected(hdl); });

        m_client.set_fail_handler([&, this](websocketpp::connection_hdl hdl)
                                  { connectFailed(hdl); });
        m_client.set_message_handler([&, this](websocketpp::connection_hdl hdl, message_ptr msg)
                                     { processResponse(hdl, msg); });
        m_client.set_close_handler([&, this](websocketpp::connection_hdl hdl)
                                   { disconnected(hdl); });
        LOGTRACE("[TransportHandler::initialize] Connecting to %s", m_wsUrl.c_str());
    }
    catch (const std::exception &e)
    {
        LOGERR("[TransportHandler::initialize] %s", e.what());
        status = -1;
    }
    catch (websocketpp::lib::error_code e)
    {
        LOGERR("[TransportHandler::initialize] %s", e.message().c_str());
        status = -2;
    }
    catch (...)
    {
        LOGERR("[TransportHandler::initialize] other exception");
        status = -3;
    }
    return status;
}

void TransportHandler::connect()
{
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_connectionState.store(ConnectionState::CONNECTING);
    }

    websocketpp::lib::error_code ec;
    wsclient::connection_ptr con = m_client.get_connection(m_wsUrl, ec);
    m_client.connect(con);

    m_client.run();
}

int TransportHandler::sendMessage(std::string message)
{
    if (tdebug)
        LOGTRACE("[TransportHandler::sendMessage] Sending %s", message.c_str());

    bool connected = (m_connectionState.load() == ConnectionState::CONNECTED);
    if (connected)
        m_client.send(m_wsHdl, message, websocketpp::frame::opcode::text);
    return connected ? 1 : -1;
}
void TransportHandler::disconnect()
{
    m_client.close(m_wsHdl, websocketpp::close::status::normal, "");
}
void TransportHandler::connected(websocketpp::connection_hdl hdl)
{
    if (tdebug)
        LOGTRACE("[TransportHandler::connected] Connected. Ready to send message");
    m_wsHdl = hdl;

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_connectionState.store(ConnectionState::CONNECTED);
    }
    m_stateChanged.notify_all();

    if (nullptr != m_conHandler)
        m_conHandler(true);
}
void TransportHandler::connectFailed(websocketpp::connection_hdl hdl)
{
    (void)hdl;

    if (tdebug)
        LOGERR("[TransportHandler::connectFailed] Connection failed...");

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_connectionState.store(ConnectionState::ERROR_STATE);
    }
    m_stateChanged.notify_all();

    if (nullptr != m_conHandler)
        m_conHandler(false);
}
void TransportHandler::processResponse(websocketpp::connection_hdl hdl, message_ptr msg)
{
    (void)hdl;

    if (tdebug)
        LOGTRACE("[TransportHandler::processResponse] %s", msg->get_payload().c_str());

    Json::Value message;
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string errors;

    std::string payload = msg->get_payload();
    bool parsingSuccessful = reader->parse(
        payload.c_str(),
        payload.c_str() + payload.size(),
        &message,
        &errors);

    if (parsingSuccessful) {
        if (message.isMember("id")) {
            if (nullptr != m_msgHandler) {
                m_msgHandler(msg->get_payload());
            }
        } else if (message.isMember("method")) {
            if (nullptr != m_eventHandler) {
                m_eventHandler(message);
            }
            if (tdebug) {
                LOGTRACE("[TransportHandler::processResponse] Event notification: %s",
                        message.get("method", "unknown").asString().c_str());
            }
        } else {
            if (tdebug) {
                LOGERR("[TransportHandler::processResponse] Unknown message format: %s",
                       msg->get_payload().c_str());
            }
        }
    } else {
        if (tdebug) {
            LOGERR("[TransportHandler::processResponse] JSON parsing failed: %s", errors.c_str());
        }
        if (nullptr != m_msgHandler) {
            m_msgHandler(msg->get_payload());
        }
    }
}
void TransportHandler::disconnected(websocketpp::connection_hdl hdl)
{
    (void)hdl;

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_connectionState.store(ConnectionState::DISCONNECTED);
    }
    m_stateChanged.notify_all();

    if (tdebug)
        LOGTRACE("[TransportHandler::disconnected] Connection closed");
}
void TransportHandler::registerConnectionHandler(std::function<void(bool)> callback)
{
    m_conHandler = callback;
}
void TransportHandler::registerMessageHandler(std::function<void(const std::string)> callback)
{
    m_msgHandler = callback;
}

void TransportHandler::registerEventHandler(EventCallback callback)
{
    m_eventHandler = callback;
}

bool TransportHandler::waitForConnection(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(m_stateMutex);

    return m_stateChanged.wait_for(lock, timeout, [this]() {
        ConnectionState state = m_connectionState.load();
        return state == ConnectionState::CONNECTED || state == ConnectionState::ERROR_STATE;
    });
}
