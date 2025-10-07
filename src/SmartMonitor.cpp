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

#include "SmartMonitor.h"
#include "EventUtils.h"
#include "thunder/ProtocolHandler.h"
#include <csignal>
#include <set>
#include <thread>

using namespace std;
using std::string;

SmartMonitor *SmartMonitor::_instance = nullptr;

const int THUNDER_TIMEOUT = 2000; // milliseconds

void SmartMonitor::handleTermSignal(int _signal)
{
    LOGINFO("Exiting from app..");

    unique_lock<std::mutex> ulock(m_lock);
    m_isActive = false;
    unRegisterForEvents();
    m_act_cv.notify_one();
}

void SmartMonitor::waitForTermSignal()
{
    LOGTRACE("Waiting for term signal.. ");
    thread termThread([&, this]()
                      {
    while (m_isActive)
    {
        unique_lock<std::mutex> ulock(m_lock);
        m_act_cv.wait(ulock);
    }

    LOGTRACE("[SmartMonitor::waitForTermSignal] Received term signal."); });
    termThread.join();
}
SmartMonitor::SmartMonitor() : m_isActive(false), isConnected(false)
{
    LOGTRACE("Constructor.. ");
    tiface = new ThunderInterface();
}
SmartMonitor::~SmartMonitor()
{
    LOGTRACE("Destructor.. ");

    tiface->shutdown();
    delete tiface;
    tiface = nullptr;
}
SmartMonitor *SmartMonitor::getInstance()
{
    LOGTRACE("Getting instance.. ");
    if (nullptr == _instance)
    {
        _instance = new SmartMonitor();
    }
    return _instance;
}
int SmartMonitor::initialize()
{
    LOGTRACE("Initializing new instance.. ");

    int status = false;

    {
        lock_guard<mutex> lkgd(m_lock);
        m_isActive = true;
    }
    signal(SIGTERM, [](int x)
           { SmartMonitor::getInstance()->handleTermSignal(x); });
    tiface->registerConnectStatusListener([&, this](bool connectionStatus)
                                          { isConnected = connectionStatus; });
    tiface->initialize();

    m_dialApps[YOUTUBE] = {DialApps::YOUTUBE, "YouTube", "unknown", "unknown"};
	m_dialApps[NETFLIX] = {DialApps::NETFLIX, "Netflix", "unknown", "unknown"};
	m_dialApps[AMAZON] = {DialApps::AMAZON, "Amazon", "unknown", "unknown"};

    status = true;
    return status;
}

void SmartMonitor::connectToThunder()
{
    LOGTRACE("Connecting to thunder.. ");
    tiface->connectToThunder();
}

void SmartMonitor::registerForEvents()
{
    LOGTRACE("Enter.. ");
    tiface->registerDialRequests([&, this](DIALEVENTS dialEvent, const DialParams & dialParams)
                                 { onDialEvent(dialEvent, dialParams); });
    tiface->registerRDKShellEvents([&, this](const std::string &event, const std::string &params)
								 { onRDKShellEvent(event, params); });
	tiface->addControllerStateChangeListener([&, this](const std::string &event, const std::string &params)
								 { onControllerStateChangeEvent(event, params); });
}

void SmartMonitor::onControllerStateChangeEvent(const std::string &event, const std::string &params)
{
	LOGINFO("Received Controller State Change Event: %s with params: %s", event.c_str(), params.c_str());
	// INFO [SmartMonitor.cpp:123] onControllerStateChangeEvent: Received Controller State Change Event: 1030.statechange with params: {"jsonrpc":"2.0","method":"1030.statechange","params":{"callsign":"Cobalt","state":"Activated","reason":"Shutdown"}}
	// extract callsign and state from params
	std::string callsign, state;
	if (!getValueOfKeyFromJson(params, "callsign", callsign)) {
		LOGERR("Failed to extract callsign from params: %s", params.c_str());
		return;
	}
	if (callsign == "")
	if (!getValueOfKeyFromJson(params, "state", state)) {
		LOGERR("Failed to extract state from params: %s", params.c_str());
		return;
	}
	std::string dialState = "unknown";
	if (convertPluginStateToDIALState(state, dialState)) {
		tiface->reportDIALAppState(callsign, "", dialState);
	} else {
		LOGERR("Failed to convert state %s for app %s", state.c_str(), callsign.c_str());
	}
}

void SmartMonitor::onRDKShellEvent(const std::string &event, const std::string &params)
{
	LOGINFO("Received RDKShell Event: %s with params: %s", event.c_str(), params.c_str());

	std::string actualEvent = event;
	size_t dotPos = event.find('.');
	if (dotPos != std::string::npos) {
		actualEvent = event.substr(dotPos + 1);
	}

	// Check if event is any of these
	static const std::set<std::string> validEvents = {
		"onApplicationActivated",
		"onApplicationLaunched",
		"onApplicationResumed",
		"onApplicationSuspended",
		"onApplicationTerminated",
		"onDestroyed",
		"onLaunched",
		"onSuspended",
		"onPluginSuspended"
	};

	if (validEvents.find(actualEvent) != validEvents.end()) {
		LOGINFO("Event %s is a valid RDKShell event.", actualEvent.c_str());
		std::string state = "stopped";
		// params has the following format: "params": {"client": "org.rdk.Netflix"}
		// extract client value as appName and set appId as empty string
		std::string appName;
		if (!getValueOfKeyFromJson(params, "client", appName)) {
			LOGERR("Failed to extract client from params: %s", params.c_str());
			return;
		}
		if (getPluginState(appName, state)) {
			tiface->reportDIALAppState(appName, "", state);
		}
	} else {
		LOGINFO("Event %s is not a monitored RDKShell event.", actualEvent.c_str());
	}
}

bool SmartMonitor::convertPluginStateToDIALState(const std::string &pluginState, std::string &dialState)
{
    bool status = true;
	if ((pluginState == "deactivated") || (pluginState == "deactivation") || (pluginState == "destroyed")
		|| (pluginState == "unavailable") || (pluginState == "activation") || (pluginState == "precondition")) {
		dialState = "stopped";
	} else if ((pluginState == "activated") || (pluginState == "resumed")) {
		dialState = "running";
	} else if ((pluginState == "suspended") || (pluginState == "hibernated")) {
		dialState = "suspended";
	} else if ((pluginState == "hidden") || (pluginState == "stopped")
			|| (pluginState == "running") || (pluginState == "suspended")) {
		// valid dial states: running, stopped, suspended, hidden. if pluginState is one of those, pass as is.
		dialState = pluginState;
	} else {
		LOGWARN("Unknown plugin state %s received.", pluginState.c_str());
		status = false;
	}
	return status;
}

void SmartMonitor::onDialEvent(DIALEVENTS dialEvent, const DialParams &dialParams)
{
	LOGINFO("Received Dial Event: %d for app: %s with id: %s", dialEvent,
			dialParams.appName.c_str(), dialParams.appId.c_str());

	std::string state = "unknown", dialState = "unknown";
	if (!getPluginState(dialParams.appName, state)) {
		LOGERR("Failed to get plugin state for app %s", dialParams.appName.c_str());
		return;
	}
	if (!convertPluginStateToDIALState(state, dialState)) {
		LOGERR("Failed to convert plugin state %s to DIAL state, set as UNKNOWN", state.c_str());
		dialState = "unknown";
	}

	if (APP_STATE_REQUEST_EVENT == dialEvent) {
		tiface->reportDIALAppState(dialParams.appName, dialParams.appId, dialState);
	} else if (APP_LAUNCH_REQUEST_EVENT == dialEvent) {
		if (dialState != "running") {
			if (!tiface->launchPremiumApp(dialParams.appName)) {
				LOGERR("Failed to launch app %s", dialParams.appName.c_str());
				return;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		} else {
			LOGINFO("App %s is already running, sending deep link request directly.", dialParams.appName.c_str());
		}
		if (!tiface->sendDeepLinkRequest(dialParams)) {
			LOGERR("Failed to send deep link request for app %s", dialParams.appName.c_str());
			return;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	} else if (APP_HIDE_REQUEST_EVENT == dialEvent) {
		if (dialState != "suspended") {
			if (!tiface->suspendPremiumApp(dialParams.appName)) {
				LOGERR("Failed to suspend app %s", dialParams.appName.c_str());
				return;
			}
		} else {
			LOGINFO("App %s is already suspended.", dialParams.appName.c_str());
		}
	} else if (APP_STOP_REQUEST_EVENT == dialEvent) {
		if (dialState != "stopped") {
			if (!tiface->shutdownPremiumApp(dialParams.appName)) {
				LOGERR("Failed to stop app %s", dialParams.appName.c_str());
				return;
			}
		} else {
			LOGINFO("App %s is already stopped.", dialParams.appName.c_str());
		}
	} else if (APP_RESUME_REQUEST_EVENT == dialEvent) {
		if (dialState != "running") {
			if (!tiface->launchPremiumApp(dialParams.appName)) {
				LOGERR("Failed to launch app %s", dialParams.appName.c_str());
				return;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	} else {
		LOGERR("Unknown event %d", dialEvent);
	}
}

bool SmartMonitor::getPluginState(const string &myapp, string &state)
{
	bool status = false;
	LOGTRACE("Getting plugin state for app %s.. ", myapp.c_str());
	if (myapp.empty()) {
		LOGERR("App name is empty.");
		return false;
	}
	// Use the cached state when available to reduce the calls to Thunder
	if ((myapp == "YouTube") && m_dialApps[YOUTUBE].pluginState != "unknown") {
		state = m_dialApps[YOUTUBE].dialState;
		return true;
	} else if ((myapp == "Netflix") && m_dialApps[NETFLIX].pluginState != "unknown") {
		state = m_dialApps[NETFLIX].dialState;
		return true;
	} else if ((myapp == "Amazon") && m_dialApps[AMAZON].pluginState != "unknown") {
		state = m_dialApps[AMAZON].dialState;
		return true;
	}

	if (tiface->getPluginState(myapp, state)) {
		status = true;
		if (myapp == "YouTube") {
			m_dialApps[YOUTUBE].pluginState = state;
			convertPluginStateToDIALState(state, m_dialApps[YOUTUBE].dialState);
		} else if (myapp == "Netflix") {
			m_dialApps[NETFLIX].pluginState = state;
			convertPluginStateToDIALState(state, m_dialApps[NETFLIX].dialState);
		} else if (myapp == "Amazon") {
			m_dialApps[AMAZON].pluginState = state;
			convertPluginStateToDIALState(state, m_dialApps[AMAZON].dialState);
		}
		for (int i = YOUTUBE; i < APPLIMIT; i++) {
			LOGINFO("Update App State Cache %s: pluginState=%s, dialState=%s",
				m_dialApps[i].appName.c_str(), m_dialApps[i].pluginState.c_str(), m_dialApps[i].dialState.c_str());
		}
	} else {
		LOGERR("Failed to get plugin state for app %s", myapp.c_str());
	}
	return status;
}

bool SmartMonitor::isAppRunning(const string &myapp)
{
    std::string callsign = (myapp == "YouTube") ? "Cobalt" : myapp;
    vector<std::string> apps = tiface->getActiveApplications();
    for (const string &app : apps)
    {
        if (stringCompareIgnoreCase(app, callsign))
            return true;
    }
    return false;
}

void SmartMonitor::unRegisterForEvents()
{
    tiface->removeDialListener();
	tiface->removeRDKShellListener();
	tiface->removeControllerStateChangeListener();
}

bool SmartMonitor::checkAndEnableCasting()
{
    LOGTRACE("Enabling casting.. ");
    bool status = false;
    string result;
    tiface->isCastingEnabled(result);
    LOGTRACE("Casting status .. %s", result.c_str());
    if (result == "false")
    {
        status = tiface->enableCasting();
        LOGTRACE("Casting result .. %s", (status ? "true" : "false"));
    }
    tiface->getFriendlyName(result);
    LOGTRACE("Friendly name is .. %s", result.c_str());
    return status;
}

bool SmartMonitor::registerDIALApps(const string &appCallsigns)
{
    LOGTRACE("Enabling Apps for DIAL casting.. ");
    bool status = false;
    string result;
    return tiface->registerXcastApps(appCallsigns);
}

bool SmartMonitor::getConnectStatus()
{
    LOGTRACE("Connect status is %s.. ", isConnected ? "true" : "false");
    return isConnected;
}

bool SmartMonitor::setStandbyBehaviour()
{
    LOGTRACE("Enabling standby behaviour as active.. ");
    bool status = false;
    string result;
    return tiface->setStandbyBehaviour();
}
