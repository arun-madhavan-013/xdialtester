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

#include <chrono>
#include <algorithm>
#include "ResponseHandler.h"
#include "EventUtils.h"
// static bool debug = false;
ResponseHandler *ResponseHandler::mcp_INSTANCE{nullptr};

ResponseHandler *ResponseHandler::getInstance()
{
    if (ResponseHandler::mcp_INSTANCE == nullptr)
    {
        ResponseHandler::mcp_INSTANCE = new ResponseHandler();
        ResponseHandler::mcp_INSTANCE->initialize();
    }
    return ResponseHandler::mcp_INSTANCE;
}

void ResponseHandler::handleEvent()
{
    LOGTRACE("Enter");
    // Get the first event, then go back.

    if (m_eventQueue.empty())
    {
        LOGTRACE("Empty Queue : exit");
        return;
    }

    string eventMsg;
    string eventName;
    // Limit mutex lifetime.
    {
        std::unique_lock<std::mutex> lock_guard(m_mtx);
        eventMsg = m_eventQueue[0];
        m_eventQueue.erase(m_eventQueue.begin());
    }

    if (mp_listener == nullptr)
    {
        LOGTRACE("No listeners : exit");
        return;
    }
    DialParams dialParams;
    if (getEventId(eventMsg, eventName))
    { // Compare against events

        if (eventName.find("onApplicationHideRequest") != string::npos)
        {
            if (getDialEventParams(eventMsg, dialParams))
                mp_listener->onDialEvents(APP_HIDE_REQUEST_EVENT, dialParams);
        }
        else if (eventName.find("onApplicationLaunchRequest") != string::npos)
        {
            if (getDialEventParams(eventMsg, dialParams))
                mp_listener->onDialEvents(APP_LAUNCH_REQUEST_EVENT, dialParams);
        }
        else if (eventName.find("onApplicationResumeRequest") != string::npos)
        {
            if (getDialEventParams(eventMsg, dialParams))
                mp_listener->onDialEvents(APP_RESUME_REQUEST_EVENT, dialParams);
        }
        else if (eventName.find("onApplicationStopRequest") != string::npos)
        {
            if (getDialEventParams(eventMsg, dialParams))
                mp_listener->onDialEvents(APP_STOP_REQUEST_EVENT, dialParams);
        }
        else if (eventName.find("onApplicationStateRequest") != string::npos)
        {
            if (getDialEventParams(eventMsg, dialParams))
                mp_listener->onDialEvents(APP_STATE_REQUEST_EVENT, dialParams);
        }
        // RDKShell events
		else if (eventName.find("onApplicationActivated") != string::npos ||
				 eventName.find("onApplicationLaunched") != string::npos ||
				 eventName.find("onApplicationResumed") != string::npos ||
				 eventName.find("onApplicationSuspended") != string::npos ||
				 eventName.find("onApplicationTerminated") != string::npos ||
				 eventName.find("onDestroyed") != string::npos ||
				 eventName.find("onLaunched") != string::npos ||
				 eventName.find("onSuspended") != string::npos ||
				 eventName.find("onPluginSuspended") != string::npos)
		{
			mp_listener->onRDKShellEvents(eventName, eventMsg);
		}
		// Controller State Change events
		else if (eventName.find("statechange") != string::npos)
		{
			mp_listener->onControllerStateChangeEvents(eventName, eventMsg);
		}
		else
		{
			LOGERR("Unrecognized event %s ", eventName.c_str());
		}
    } // Here end if(getEventId(eventMsg,eventName))
    else
    {
        LOGERR("Event Queue has a non-event message %s", eventMsg.c_str());
    }
    LOGINFO(" Exit");
}

void ResponseHandler::initialize()
{
    mp_thandle = new std::thread([this] { runEventLoop(); });

    if (m_useImprovedLogic) {
        mp_cleanupThread = new std::thread([this] { runCleanupLoop(); });
    }
}

void ResponseHandler::runEventLoop()
{
    if (m_useImprovedLogic) {
        // Improved event loop with separate mutex for events
        while (m_runLoop) {
            std::unique_lock<std::mutex> lock(m_eventMutex);

            // Wait for events
            m_eventCV.wait(lock, [this] { return !m_eventQueue.empty() || !m_runLoop; });

            if (!m_runLoop) break;

            if (!m_eventQueue.empty()) {
                // Process all queued events
                auto events = std::move(m_eventQueue);
                m_eventQueue.clear();
                lock.unlock();

                // Process events without holding lock
                for (const auto& event : events) {
                    processEvent(event);
                }
            }
        }
    } else {
        // Legacy event loop
        while (m_runLoop) {
            if (m_eventQueue.empty()) {
                std::unique_lock<std::mutex> lock_guard(m_mtx);
                m_cv.wait(lock_guard);
            }
            if (!m_eventQueue.empty()) {
                handleEvent();
                m_cv.notify_all();
            }
        }
    }
    LOGTRACE("Exit");
}
string ResponseHandler::getRequestStatus(int msgId, int timeout)
{
    if (m_useImprovedLogic) {
        return getRequestStatusImproved(msgId, timeout);
    } else {
        return getRequestStatusLegacy(msgId, timeout);
    }
}

string ResponseHandler::getRequestStatusLegacy(int msgId, int timeout)
{
    string response;
    LOGTRACE("Waiting for id %d with timeout %d", msgId, timeout);
    dumpMap(m_msgMap);

    std::unique_lock<std::mutex> lock_guard(m_mtx);
    auto now = std::chrono::system_clock::now();
    if (m_msgMap.find(msgId) != m_msgMap.end())
    {
        response = m_msgMap[msgId];
        m_msgMap.erase(msgId);
    }
    else if (m_cv.wait_until(lock_guard, now + std::chrono::milliseconds(timeout)) != std::cv_status::timeout)
    {
        if (debug)
        {
            dumpMap(m_msgMap);
        }

        if (m_msgMap.find(msgId) != m_msgMap.end())
        {
            response = m_msgMap[msgId];
            m_msgMap.erase(msgId);
        }
        else
        {
            m_purgableIds.push_back(msgId);
            LOGTRACE("Unable to match any response");
        }
    }
    else
    {
        LOGTRACE("Request timed out... %d ", msgId);
        m_purgableIds.push_back(msgId);
    }
    // Two threads are competing for event notification. Let us update the other one.
    m_cv.notify_all();
    return response;
}

string ResponseHandler::getRequestStatusImproved(int msgId, int timeout)
{
    LOGTRACE("Waiting for request id %d with timeout %d ms (improved)", msgId, timeout);

    std::unique_lock<std::mutex> lock(m_requestMutex);

    // Check if request already exists and is completed
    auto it = m_pendingRequests.find(msgId);
    if (it != m_pendingRequests.end()) {
        if (it->second->state == RequestState::COMPLETED) {
            std::string response = std::move(it->second->response);
            m_pendingRequests.erase(it);
            return response;
        }
    } else {
        // Create new request context
        auto context = std::make_unique<RequestContext>(msgId);
        m_pendingRequests[msgId] = std::move(context);
        it = m_pendingRequests.find(msgId);
    }

    // Wait for response with timeout
    auto future = it->second->promise.get_future();
    lock.unlock(); // Release lock while waiting

    auto status = future.wait_for(std::chrono::milliseconds(timeout));

    lock.lock(); // Reacquire lock

    if (status == std::future_status::ready) {
        // Response received
        try {
            std::string response = future.get();
            m_pendingRequests.erase(msgId);
            return response;
        } catch (const std::exception& e) {
            LOGERR("Exception getting response for id %d: %s", msgId, e.what());
        }
    } else {
        // Timeout occurred
        LOGTRACE("Request %d timed out", msgId);
        auto it = m_pendingRequests.find(msgId);
        if (it != m_pendingRequests.end()) {
            it->second->state = RequestState::TIMEOUT;
            // Don't erase immediately - let cleanup thread handle it
        }
    }

    return ""; // Empty response indicates timeout or error
}
void ResponseHandler::shutdown()
{
    LOGTRACE("Enter");

    // Signal shutdown
    m_runLoop = false;

    if (m_useImprovedLogic) {
        // Wake up all waiting threads
        {
            std::lock_guard<std::mutex> lock(m_requestMutex);
            m_requestCV.notify_all();
        }
        {
            std::lock_guard<std::mutex> lock(m_eventMutex);
            m_eventCV.notify_all();
        }

        // Wait for cleanup thread
        if (mp_cleanupThread && mp_cleanupThread->joinable()) {
            mp_cleanupThread->join();
            delete mp_cleanupThread;
            mp_cleanupThread = nullptr;
        }
    } else {
        std::unique_lock<std::mutex> lock_guard(m_mtx);
        m_cv.notify_all();
    }

    // Wait for event thread
    if (mp_thandle && mp_thandle->joinable()) {
        mp_thandle->join();
        delete mp_thandle;
        mp_thandle = nullptr;
    }

    LOGTRACE("Exit");
}
void ResponseHandler::addMessageToResponseQueue(int msgId, const string msg)
{
    if (m_useImprovedLogic) {
        addMessageToResponseQueueImproved(msgId, msg);
    } else {
        addMessageToResponseQueueLegacy(msgId, msg);
    }
}

void ResponseHandler::addMessageToResponseQueueLegacy(int msgId, const string& msg)
{
    LOGTRACE("Enter");

    if (!m_purgableIds.empty()) // find on empty vector cause core dump
    {
        auto index = std::find(m_purgableIds.begin(), m_purgableIds.end(), msgId);
        if (index != m_purgableIds.end())
        {
            if (debug)
                dumpVector(m_purgableIds);
            LOGTRACE("Event response arrived late. Discarding %s", msg.c_str());
            m_purgableIds.erase(index);
            return;
        }
    }
    LOGTRACE(" Adding to message queue.");
    std::unique_lock<std::mutex> lock_guard(m_mtx);
    m_msgMap.emplace(std::make_pair(msgId, msg));
    m_cv.notify_all();
}

void ResponseHandler::addMessageToResponseQueueImproved(int msgId, const string& msg)
{
    LOGTRACE("Adding response for id %d (improved)", msgId);

    std::lock_guard<std::mutex> lock(m_requestMutex);

    auto it = m_pendingRequests.find(msgId);
    if (it != m_pendingRequests.end()) {
        // Found pending request
        if (it->second->state == RequestState::PENDING) {
            it->second->response = msg;
            it->second->state = RequestState::COMPLETED;

            // Fulfill the promise to wake up waiters
            try {
                it->second->promise.set_value(msg);
            } catch (const std::exception& e) {
                LOGERR("Exception setting promise for id %d: %s", msgId, e.what());
            }
        } else {
            LOGTRACE("Response for id %d arrived but request is in state %d",
                    msgId, static_cast<int>(it->second->state));
        }
    } else {
        // No pending request - this is a late response
        LOGTRACE("Late response for id %d - no pending request found", msgId);
    }
}
void ResponseHandler::addMessageToEventQueue(const string msg)
{
    LOGTRACE("Adding event to queue");

    if (m_useImprovedLogic) {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        m_eventQueue.emplace_back(msg);
        m_eventCV.notify_one();
    } else {
        std::unique_lock<std::mutex> lock_guard(m_mtx);
        m_eventQueue.emplace_back(msg);
        m_cv.notify_all();
    }

    LOGTRACE("Added event to queue");
}
void ResponseHandler::connectionEvent(bool connected)
{
    // This needs to be revisited.
}

// New improved methods
std::future<std::string> ResponseHandler::getRequestAsync(int msgId)
{
    std::lock_guard<std::mutex> lock(m_requestMutex);

    auto it = m_pendingRequests.find(msgId);
    if (it != m_pendingRequests.end()) {
        return it->second->promise.get_future();
    }

    // Create new request context
    auto context = std::make_unique<RequestContext>(msgId);
    auto future = context->promise.get_future();
    m_pendingRequests[msgId] = std::move(context);

    return future;
}

bool ResponseHandler::cancelRequest(int msgId)
{
    std::lock_guard<std::mutex> lock(m_requestMutex);

    auto it = m_pendingRequests.find(msgId);
    if (it != m_pendingRequests.end() && it->second->state == RequestState::PENDING) {
        it->second->state = RequestState::CANCELLED;
        // Fulfill promise with empty response to unblock waiters
        try {
            it->second->promise.set_value("");
        } catch (const std::exception& e) {
            // Promise might already be fulfilled
        }
        m_pendingRequests.erase(it);
        return true;
    }

    return false;
}

void ResponseHandler::processEvent(const std::string& eventMsg)
{
    if (mp_listener == nullptr) {
        LOGTRACE("No listeners - skipping event");
        return;
    }

    std::string eventName;
    if (!getEventId(eventMsg, eventName)) {
        LOGERR("Failed to extract event name from: %s", eventMsg.c_str());
        return;
    }

    DialParams dialParams;

    // Handle DIAL events
    if (eventName.find("onApplicationHideRequest") != std::string::npos) {
        if (getDialEventParams(eventMsg, dialParams))
            mp_listener->onDialEvents(APP_HIDE_REQUEST_EVENT, dialParams);
    }
    else if (eventName.find("onApplicationLaunchRequest") != std::string::npos) {
        if (getDialEventParams(eventMsg, dialParams))
            mp_listener->onDialEvents(APP_LAUNCH_REQUEST_EVENT, dialParams);
    }
    else if (eventName.find("onApplicationResumeRequest") != std::string::npos) {
        if (getDialEventParams(eventMsg, dialParams))
            mp_listener->onDialEvents(APP_RESUME_REQUEST_EVENT, dialParams);
    }
    else if (eventName.find("onApplicationStopRequest") != std::string::npos) {
        if (getDialEventParams(eventMsg, dialParams))
            mp_listener->onDialEvents(APP_STOP_REQUEST_EVENT, dialParams);
    }
    else if (eventName.find("onApplicationStateRequest") != std::string::npos) {
        if (getDialEventParams(eventMsg, dialParams))
            mp_listener->onDialEvents(APP_STATE_REQUEST_EVENT, dialParams);
    }
    // Handle RDKShell events
    else if (eventName.find("onApplicationActivated") != std::string::npos ||
             eventName.find("onApplicationLaunched") != std::string::npos ||
             eventName.find("onApplicationResumed") != std::string::npos ||
             eventName.find("onApplicationSuspended") != std::string::npos ||
             eventName.find("onApplicationTerminated") != std::string::npos ||
             eventName.find("onDestroyed") != std::string::npos ||
             eventName.find("onLaunched") != std::string::npos ||
             eventName.find("onSuspended") != std::string::npos ||
             eventName.find("onPluginSuspended") != std::string::npos) {
        mp_listener->onRDKShellEvents(eventName, eventMsg);
    }
    // Handle Controller State Change events
    else if (eventName.find("statechange") != std::string::npos) {
        mp_listener->onControllerStateChangeEvents(eventName, eventMsg);
    }
    else {
        LOGERR("Unrecognized event: %s", eventName.c_str());
    }
}

void ResponseHandler::runCleanupLoop()
{
    LOGTRACE("Cleanup loop started");

    while (m_runLoop) {
        std::this_thread::sleep_for(CLEANUP_INTERVAL);

        if (!m_runLoop) break;

        cleanupExpiredRequests();
    }

    LOGTRACE("Cleanup loop exited");
}

void ResponseHandler::cleanupExpiredRequests()
{
    std::lock_guard<std::mutex> lock(m_requestMutex);

    auto now = std::chrono::steady_clock::now();
    auto it = m_pendingRequests.begin();

    while (it != m_pendingRequests.end()) {
        auto age = now - it->second->createdAt;

        if (age > MAX_REQUEST_AGE || it->second->state != RequestState::PENDING) {
            LOGTRACE("Cleaning up request %d (age: %lld seconds, state: %d)",
                    it->first,
                    std::chrono::duration_cast<std::chrono::seconds>(age).count(),
                    static_cast<int>(it->second->state));

            // Fulfill promise if still pending to unblock any waiters
            if (it->second->state == RequestState::PENDING) {
                try {
                    it->second->promise.set_value(""); // Empty response for cleanup
                } catch (const std::exception& e) {
                    // Promise might already be fulfilled
                }
            }

            it = m_pendingRequests.erase(it);
        } else {
            ++it;
        }
    }
}

size_t ResponseHandler::getPendingRequestCount() const
{
    std::lock_guard<std::mutex> lock(m_requestMutex);
    return m_pendingRequests.size();
}

size_t ResponseHandler::getCompletedRequestCount() const
{
    std::lock_guard<std::mutex> lock(m_requestMutex);
    return m_completedRequests.size();
}

void ResponseHandler::clearCompletedRequests()
{
    std::lock_guard<std::mutex> lock(m_requestMutex);
    m_completedRequests.clear();
}
