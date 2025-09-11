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
const string SmartMonitor::LAUNCH_URL = "http://127.0.0.1:50050/lxresui/index.html#menu";
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
    tiface->registerDialRequests([&, this](DIALEVENTS dialEvent, const string &appName, const string &appId)
                                  { onDialEvent(dialEvent, appName, appId); });
}

void SmartMonitor::onDialEvent(DIALEVENTS dialEvent, const string &appName, const string &appId)
{
    LOGINFO("Received Dial Event: %d for app: %s with id: %s", dialEvent, appName.c_str(), appId.c_str());

}

void SmartMonitor::unRegisterForEvents()
{
    tiface->removeDialListener();

}

bool SmartMonitor::enableCasting()
{
    LOGTRACE("Enabling casting.. ");
    bool status = false;
    string result;
    tiface->isCastingEnabled(result);
    LOGTRACE("Casting status .. %s" , result.c_str());
    status = tiface->enableCasting();
    LOGTRACE("Casting result .. %s" ,(status?"true":"false"));
    return status;
}

bool SmartMonitor::getConnectStatus()
{
    LOGTRACE("Connect status is %s.. ", isConnected ? "true" : "false");
    return isConnected;
}