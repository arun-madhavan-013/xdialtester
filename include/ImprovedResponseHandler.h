/*
 * Improved ResponseHandler Architecture
 * Addresses race conditions and improves performance
 */

#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <future>

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

class ImprovedResponseHandler
{
    static ImprovedResponseHandler *mcp_INSTANCE;

    // Separate data structures for better concurrency
    std::unordered_map<int, std::unique_ptr<RequestContext>> m_pendingRequests;
    std::unordered_set<int> m_completedRequests;  // Fast lookup for completed requests
    std::vector<std::string> m_eventQueue;

    // Separate mutexes for different concerns
    std::mutex m_requestMutex;      // For request/response operations
    std::mutex m_eventMutex;        // For event queue operations

    // Separate condition variables
    std::condition_variable m_requestCV;  // For request/response notifications
    std::condition_variable m_eventCV;    // For event notifications

    std::thread *mp_eventThread;
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

protected:
    ImprovedResponseHandler() : mp_listener(nullptr), mp_eventThread(nullptr),
                               mp_cleanupThread(nullptr), m_runLoop(true) {}
    ~ImprovedResponseHandler() {}

public:
    static ImprovedResponseHandler *getInstance();
    void initialize();
    void shutdown();

    void handleEvent();
    void addMessageToResponseQueue(int msgId, const std::string& msg);
    void addMessageToEventQueue(const std::string& msg);
    void connectionEvent(bool connected);

    // Improved request handling with futures
    std::string getRequestStatus(int msgId, int timeoutMs = REQUEST_TIMEOUT_IN_MS);
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
    ImprovedResponseHandler(const ImprovedResponseHandler &) = delete;
    ImprovedResponseHandler &operator=(const ImprovedResponseHandler &) = delete;
};
