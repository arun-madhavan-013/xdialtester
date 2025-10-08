/*
 * Test program for improved ResponseHandler
 * Validates race condition fixes and performance improvements
 */

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include "ResponseHandler.h"

class TestEventListener : public EventListener {
public:
    void registerDialRequests(std::function<void(DIALEVENTS, const DialParams &)> callback) override {}
    void registerRDKShellEvents(std::function<void(const std::string &, const std::string &)> callback) override {}
    void addControllerStateChangeListener(std::function<void(const std::string &, const std::string &)> callback) override {}
    void removeDialListener() override {}
    void removeRDKShellListener() override {}
    void removeControllerStateChangeListener() override {}
    void onDialEvents(DIALEVENTS dialEvent, const DialParams &dialParams) override {}
    void onRDKShellEvents(const std::string &event, const std::string &params) override {}
    void onControllerStateChangeEvents(const std::string &event, const std::string &params) override {}
};

void testConcurrentRequests(bool useImproved) {
    ResponseHandler* handler = ResponseHandler::getInstance();
    handler->setUseImprovedLogic(useImproved);

    std::cout << "Testing " << (useImproved ? "IMPROVED" : "LEGACY") << " ResponseHandler..." << std::endl;

    const int NUM_THREADS = 10;
    const int REQUESTS_PER_THREAD = 100;
    std::vector<std::thread> threads;
    std::vector<int> successCounts(NUM_THREADS, 0);
    std::vector<int> timeoutCounts(NUM_THREADS, 0);

    auto startTime = std::chrono::steady_clock::now();

    // Create worker threads that send requests
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> delay(1, 10);

            for (int i = 0; i < REQUESTS_PER_THREAD; i++) {
                int msgId = t * 1000 + i;

                // Simulate response arriving after some delay
                std::thread responder([handler, msgId]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    std::string response = "{\"result\":\"success\",\"id\":" + std::to_string(msgId) + "}";
                    handler->addMessageToResponseQueue(msgId, response);
                });
                responder.detach();

                // Request with timeout
                std::string response = handler->getRequestStatus(msgId, 200);
                if (!response.empty()) {
                    successCounts[t]++;
                } else {
                    timeoutCounts[t]++;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(delay(gen)));
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    int totalSuccess = 0, totalTimeout = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        totalSuccess += successCounts[i];
        totalTimeout += timeoutCounts[i];
    }

    std::cout << "Results:" << std::endl;
    std::cout << "  Total Requests: " << (NUM_THREADS * REQUESTS_PER_THREAD) << std::endl;
    std::cout << "  Successful: " << totalSuccess << std::endl;
    std::cout << "  Timeouts: " << totalTimeout << std::endl;
    std::cout << "  Duration: " << duration.count() << " ms" << std::endl;
    std::cout << "  Requests/sec: " << (totalSuccess * 1000.0 / duration.count()) << std::endl;

    if (useImproved) {
        std::cout << "  Pending Requests: " << handler->getPendingRequestCount() << std::endl;
        std::cout << "  Completed Requests: " << handler->getCompletedRequestCount() << std::endl;
    }

    std::cout << std::endl;
}

void testMemoryBehavior() {
    ResponseHandler* handler = ResponseHandler::getInstance();
    handler->setUseImprovedLogic(true);

    std::cout << "Testing memory behavior..." << std::endl;

    // Create many requests that will timeout
    for (int i = 0; i < 1000; i++) {
        handler->getRequestStatus(10000 + i, 10); // Short timeout
    }

    std::cout << "After 1000 timeout requests:" << std::endl;
    std::cout << "  Pending: " << handler->getPendingRequestCount() << std::endl;

    // Wait for cleanup
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "After cleanup delay:" << std::endl;
    std::cout << "  Pending: " << handler->getPendingRequestCount() << std::endl;
    std::cout << std::endl;
}

int main() {
    // Initialize
    TestEventListener listener;
    ResponseHandler* handler = ResponseHandler::getInstance();
    handler->registerEventListener(&listener);

    // Test legacy implementation
    testConcurrentRequests(false);

    // Test improved implementation
    testConcurrentRequests(true);

    // Test memory behavior
    testMemoryBehavior();

    // Shutdown
    handler->shutdown();

    std::cout << "All tests completed!" << std::endl;
    return 0;
}
