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
#include <string>
#include <vector>
#include <mutex>
#include <map>
#include <thread>
#include <condition_variable>

#include "EventUtils.h"
#include "EventListener.h"
class ResponseHandler
{
    static ResponseHandler *mcp_INSTANCE;

    std::vector<int> m_purgableIds;
    std::vector<std::string> m_eventQueue;
    std::map<int, std::string> m_msgMap;
    std::thread *mp_thandle;
    std::mutex m_mtx;
    std::condition_variable m_cv;
    bool m_runLoop;
    EventListener *mp_listener;

    void runEventLoop();

protected:
    ResponseHandler() : mp_listener(nullptr), mp_thandle(nullptr), m_runLoop(true){};
    ~ResponseHandler(){};

public:
    static ResponseHandler *getInstance();
    void initialize();
    void shutdown();

    void handleEvent();
    void addMessageToResponseQueue(int msgId, const std::string msg);
    void addMessageToEventQueue(const std::string msg);
    void connectionEvent(bool connected);
    std::string getRequestStatus(int msgId, int timeout = REQUEST_TIMEOUT_IN_MS);

    void registerEventListener(EventListener *listener)
    {
        mp_listener = listener;
    }
    // no copying allowed
    ResponseHandler(const ResponseHandler &) = delete;
    ResponseHandler &operator=(const ResponseHandler &) = delete;
};
