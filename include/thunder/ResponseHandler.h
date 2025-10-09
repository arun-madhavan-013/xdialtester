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
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <future>
#include <memory>

#include "EventUtils.h"
#include "EventListener.h"

// Request state tracking
enum class RequestState {
    PENDING,
    COMPLETED,
    TIMEOUT,
    CANCELLED
};

// Request context for tracking individual requests
struct RequestContext {
    int msgId;
    std::string response;
    RequestState state;
    std::chrono::steady_clock::time_point createdAt;
    std::promise<std::string> promise;

    RequestContext(int id) : msgId(id), state(RequestState::PENDING),
                           createdAt(std::chrono::steady_clock::now()) {}
};

class ResponseHandler
{
    static ResponseHandler *mcp_INSTANCE;

    // Data structures
    std::vector<std::string> m_eventQueue;
    std::unordered_map<int, std::unique_ptr<RequestContext>> m_pendingRequests;
    std::unordered_set<int> m_completedRequests;

    // Mutexes for thread safety
    mutable std::mutex m_requestMutex;     // For request/response operations (mutable for const methods)
    std::mutex m_eventMutex;       // For event queue operations

    // Condition variables
    std::condition_variable m_requestCV; // For request/response notifications
    std::condition_variable m_eventCV;   // For event notifications

    std::thread *mp_thandle;
    std::thread *mp_cleanupThread;

    bool m_runLoop;
    EventListener *mp_listener;

    // Configuration
    static constexpr std::chrono::seconds CLEANUP_INTERVAL{30};
    static constexpr std::chrono::seconds MAX_REQUEST_AGE{300}; // 5 minutes

    void runEventLoop();
    void runCleanupLoop();
    void cleanupExpiredRequests();
    void processEvent(const std::string& eventMsg);
    std::string extractParamsFromJsonRpc(const std::string& jsonRpcMsg);

    // Core methods
    std::string getRequestStatus(int msgId, int timeout);
    void addMessageToResponseQueue(int msgId, const std::string& msg);

protected:
    ResponseHandler() : mp_thandle(nullptr), mp_cleanupThread(nullptr), m_runLoop(true),
                       mp_listener(nullptr) {}
    ~ResponseHandler() {}

public:
    static ResponseHandler *getInstance();
    void initialize();
    void shutdown();

    void handleEvent();
    void addMessageToResponseQueue(int msgId, const std::string& msg);
    void addMessageToEventQueue(const std::string& msg);
    void connectionEvent(bool connected);
    std::string getRequestStatus(int msgId, int timeout = REQUEST_TIMEOUT_IN_MS);

    // Async operations
    std::future<std::string> getRequestAsync(int msgId);
    bool cancelRequest(int msgId);

    // Statistics and monitoring
    size_t getPendingRequestCount() const;
    size_t getCompletedRequestCount() const;
    void clearCompletedRequests();

    void registerEventListener(EventListener *listener) {
        mp_listener = listener;
    }

    // no copying allowed
    ResponseHandler(const ResponseHandler &) = delete;
    ResponseHandler &operator=(const ResponseHandler &) = delete;
};
