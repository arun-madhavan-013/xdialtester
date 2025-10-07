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
    mp_thandle = new std::thread([this]
                                 { runEventLoop(); });
}

void ResponseHandler::runEventLoop()
{

    while (m_runLoop)
    {

        if (m_eventQueue.empty())
        {
            std::unique_lock<std::mutex> lock_guard(m_mtx);
            m_cv.wait(lock_guard);
        }
        if (!m_eventQueue.empty()) // New event ?
        {
            handleEvent();
            // See if any other response requests are waiting for lock.
            m_cv.notify_all();
        }
    }
    LOGTRACE("Exit");
}
string ResponseHandler::getRequestStatus(int msgId, int timeout)
{
    string response;
    LOGTRACE("Waiting for id %d", msgId);
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
            LOGTRACE("Unable to match any response ");
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
void ResponseHandler::shutdown()
{
    LOGTRACE("Enter");
    std::unique_lock<std::mutex> lock_guard(m_mtx);
    m_runLoop = false;
    m_cv.notify_all();
    LOGTRACE(" Exit");
}
void ResponseHandler::addMessageToResponseQueue(int msgId, const string msg)
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
void ResponseHandler::addMessageToEventQueue(const string msg)
{
    LOGTRACE("Adding event to queue");
    std::unique_lock<std::mutex> lock_guard(m_mtx);
    m_eventQueue.emplace_back(msg);
    m_cv.notify_all();
    LOGTRACE("Added event to queue");
}
void ResponseHandler::connectionEvent(bool connected)
{
    // This needs to be revisited.
}
