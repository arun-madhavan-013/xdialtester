/*
 * Improved ResponseHandler Implementation
 * Addresses race conditions and improves performance
 */

#include "ImprovedResponseHandler.h"
#include "thunder/ProtocolHandler.h"
#include <algorithm>
#include <sstream>
#include <memory>
#include "json/json.h"

ImprovedResponseHandler* ImprovedResponseHandler::mcp_INSTANCE{nullptr};

ImprovedResponseHandler* ImprovedResponseHandler::getInstance()
{
    if (ImprovedResponseHandler::mcp_INSTANCE == nullptr)
    {
        ImprovedResponseHandler::mcp_INSTANCE = new ImprovedResponseHandler();
        ImprovedResponseHandler::mcp_INSTANCE->initialize();
    }
    return ImprovedResponseHandler::mcp_INSTANCE;
}

void ImprovedResponseHandler::initialize()
{
    mp_eventThread = new std::thread([this] { runEventLoop(); });
    mp_cleanupThread = new std::thread([this] { runCleanupLoop(); });
}

void ImprovedResponseHandler::shutdown()
{
    LOGTRACE("Enter");

    // Signal shutdown
    m_runLoop = false;

    // Wake up all waiting threads
    {
        std::lock_guard<std::mutex> lock(m_requestMutex);
        m_requestCV.notify_all();
    }
    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        m_eventCV.notify_all();
    }

    // Wait for threads to finish
    if (mp_eventThread && mp_eventThread->joinable()) {
        mp_eventThread->join();
        delete mp_eventThread;
        mp_eventThread = nullptr;
    }

    if (mp_cleanupThread && mp_cleanupThread->joinable()) {
        mp_cleanupThread->join();
        delete mp_cleanupThread;
        mp_cleanupThread = nullptr;
    }

    LOGTRACE("Exit");
}

std::string ImprovedResponseHandler::getRequestStatus(int msgId, int timeoutMs)
{
    LOGTRACE("Waiting for request id %d with timeout %d ms", msgId, timeoutMs);

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

    auto status = future.wait_for(std::chrono::milliseconds(timeoutMs));

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

std::future<std::string> ImprovedResponseHandler::getRequestAsync(int msgId)
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

bool ImprovedResponseHandler::cancelRequest(int msgId)
{
    std::lock_guard<std::mutex> lock(m_requestMutex);

    auto it = m_pendingRequests.find(msgId);
    if (it != m_pendingRequests.end() && it->second->state == RequestState::PENDING) {
        it->second->state = RequestState::CANCELLED;
        // Fulfill promise with empty response to unblock waiters
        it->second->promise.set_value("");
        m_pendingRequests.erase(it);
        return true;
    }

    return false;
}

void ImprovedResponseHandler::addMessageToResponseQueue(int msgId, const std::string& msg)
{
    LOGTRACE("Adding response for id %d", msgId);

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

void ImprovedResponseHandler::addMessageToEventQueue(const std::string& msg)
{
    LOGTRACE("Adding event to queue");

    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_eventQueue.emplace_back(msg);
    m_eventCV.notify_one();

    LOGTRACE("Added event to queue");
}

void ImprovedResponseHandler::runEventLoop()
{
    LOGTRACE("Event loop started");

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

    LOGTRACE("Event loop exited");
}

void ImprovedResponseHandler::processEvent(const std::string& eventMsg)
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

    // Extract just the params object as JSON string
    std::string paramsJson = extractParamsFromJsonRpc(eventMsg);

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
    // Handle RDKShell events - pass event name and extracted params JSON
    else if (eventName.find("onApplicationActivated") != std::string::npos ||
             eventName.find("onApplicationLaunched") != std::string::npos ||
             eventName.find("onApplicationResumed") != std::string::npos ||
             eventName.find("onApplicationSuspended") != std::string::npos ||
             eventName.find("onApplicationTerminated") != std::string::npos ||
             eventName.find("onDestroyed") != std::string::npos ||
             eventName.find("onLaunched") != std::string::npos ||
             eventName.find("onSuspended") != std::string::npos ||
             eventName.find("onPluginSuspended") != std::string::npos) {
        mp_listener->onRDKShellEvents(eventName, paramsJson);
    }
    // Handle Controller State Change events - pass event name and extracted params JSON
    else if (eventName.find("statechange") != std::string::npos) {
        mp_listener->onControllerStateChangeEvents(eventName, paramsJson);
    }
    else {
        LOGERR("Unrecognized event: %s", eventName.c_str());
    }
}

std::string ImprovedResponseHandler::extractParamsFromJsonRpc(const std::string& jsonRpcMsg)
{
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string errs;

    bool parsingSuccessful = reader->parse(
        jsonRpcMsg.c_str(),
        jsonRpcMsg.c_str() + jsonRpcMsg.size(),
        &root,
        &errs);

    if (!parsingSuccessful) {
        LOGERR("Failed to parse JSON-RPC message: %s", jsonRpcMsg.c_str());
        return "{}"; // Return empty JSON object
    }

    if (root["params"].isObject()) {
        // Convert the params object back to string
        Json::StreamWriterBuilder writerBuilder;
        writerBuilder["indentation"] = "";
        std::ostringstream os;
        std::unique_ptr<Json::StreamWriter> writer(writerBuilder.newStreamWriter());
        writer->write(root["params"], &os);
        return os.str();
    }

    return "{}"; // Return empty JSON object if no params
}

void ImprovedResponseHandler::runCleanupLoop()
{
    LOGTRACE("Cleanup loop started");

    while (m_runLoop) {
        std::this_thread::sleep_for(CLEANUP_INTERVAL);

        if (!m_runLoop) break;

        cleanupExpiredRequests();
    }

    LOGTRACE("Cleanup loop exited");
}

void ImprovedResponseHandler::cleanupExpiredRequests()
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

size_t ImprovedResponseHandler::getPendingRequestCount() const
{
    std::lock_guard<std::mutex> lock(m_requestMutex);
    return m_pendingRequests.size();
}

size_t ImprovedResponseHandler::getCompletedRequestCount() const
{
    std::lock_guard<std::mutex> lock(m_requestMutex);
    return m_completedRequests.size();
}

void ImprovedResponseHandler::clearCompletedRequests()
{
    std::lock_guard<std::mutex> lock(m_requestMutex);
    m_completedRequests.clear();
}
