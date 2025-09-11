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
#include <thread>

#include <iostream>
using std::cout;
using std::endl;

bool tdebug = false;
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
        cout << "[TransportHandler::initialize] " << "Connecting to " << m_wsUrl << endl;
    }
    catch (const std::exception &e)
    {
        cout << "[TransportHandler::initialize] " << e.what() << endl;
        status = -1;
    }
    catch (websocketpp::lib::error_code e)
    {
        cout << "[TransportHandler::initialize] " << e.message() << endl;
        status = -2;
    }
    catch (...)
    {
        cout << "[TransportHandler::initialize] other exception" << endl;
        status = -3;
    }
    return status;
}

void TransportHandler::connect()
{
    // Create a connection to the given URI and queue it for connection once
    // the event loop starts
    websocketpp::lib::error_code ec;
    wsclient::connection_ptr con = m_client.get_connection(m_wsUrl, ec);
    m_client.connect(con);

    // Start the ASIO io_service run loop
    m_client.run();
}

int TransportHandler::sendMessage(std::string message)
{
    if (tdebug)
        cout << "[TransportHandler::sendMessage] Sending " << message << endl;
    if (m_isConnected)
        m_client.send(m_wsHdl, message, websocketpp::frame::opcode::text);
    return m_isConnected ? 1 : -1;
}
void TransportHandler::disconnect()
{
    m_client.close(m_wsHdl, websocketpp::close::status::normal, "");
}
void TransportHandler::connected(websocketpp::connection_hdl hdl)
{
    if (tdebug)
        cout << "[TransportHandler::connected] Connected. Ready to send message" << endl;
    m_wsHdl = hdl;
    m_isConnected = true;
    if (nullptr != m_conHandler)
        m_conHandler(true);
}
void TransportHandler::connectFailed(websocketpp::connection_hdl hdl)
{
    if (tdebug)
        cout << "[TransportHandler::connectFailed] Connection failed..." << endl;
    if (nullptr != m_conHandler)
        m_conHandler(false);
}
void TransportHandler::processResponse(websocketpp::connection_hdl hdl, message_ptr msg)
{
    if (tdebug)
        cout << "[TransportHandler::processResponse] " << msg->get_payload() << endl;
    if (nullptr != m_msgHandler)
        m_msgHandler(msg->get_payload());
}
void TransportHandler::disconnected(websocketpp::connection_hdl hdl)
{
    m_isConnected = false;
}
void TransportHandler::registerConnectionHandler(std::function<void(bool)> callback)
{
    m_conHandler = callback;
}
void TransportHandler::registerMessageHandler(std::function<void(const std::string)> callback)
{
    m_msgHandler = callback;
}