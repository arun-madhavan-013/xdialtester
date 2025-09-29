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
#include <csignal>
#include <thread>

using namespace std;
using std::string;
bool debug = isDebugEnabled();

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
}

void SmartMonitor::onRDKShellEvent(const std::string &event, const std::string &params)
{
	LOGINFO("Received RDKShell Event: %s with params: %s", event.c_str(), params.c_str());
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

	if (validEvents.find(event) != validEvents.end()) {
		LOGINFO("Event %s is a valid RDKShell event.", event.c_str());
		if (event == "onApplicationLaunched") {
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
		}
	} else {
		LOGINFO("Event %s is not a monitored RDKShell event.", event.c_str());
	}
}

void SmartMonitor::onDialEvent(DIALEVENTS dialEvent, const DialParams &dialParams)
{
	LOGINFO("Received Dial Event: %d for app: %s with id: %s", dialEvent,
	        dialParams.appName.c_str(), dialParams.appId.c_str());

	bool running = isAppRunning(dialParams.appName);
	if (APP_STATE_REQUEST_EVENT == dialEvent) {
		std::string state = "stopped";
		if (getPluginState(dialParams.appName, state)) {
			tiface->reportDIALAppState(dialParams.appName, dialParams.appId, state);
		}
	} else if (APP_LAUNCH_REQUEST_EVENT == dialEvent) {
		if (!running) tiface->launchPremiumApp(dialParams.appName);
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		tiface->sendDeepLinkRequest(dialParams);
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		tiface->reportDIALAppState(dialParams.appName, dialParams.appId, "running");
	} else if (APP_HIDE_REQUEST_EVENT == dialEvent ||
	           APP_RESUME_REQUEST_EVENT == dialEvent) {
		if (!tiface->suspendPremiumApp(dialParams.appName)) {
			LOGERR("Failed to suspend app %s", dialParams.appName.c_str());
		}
		tiface->reportDIALAppState(dialParams.appName, dialParams.appId, "hidden");
	} else if (APP_STOP_REQUEST_EVENT == dialEvent) {
		if (running) tiface->shutdownPremiumApp(dialParams.appName);
		tiface->reportDIALAppState(dialParams.appName, dialParams.appId, "stopped");
	}

	else {
		LOGERR("Unknown event %d", dialEvent);
	}
}

bool SmartMonitor::getPluginState(const string &myapp, const string &state)
{
	return tiface->getPluginState(myapp, state);
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
