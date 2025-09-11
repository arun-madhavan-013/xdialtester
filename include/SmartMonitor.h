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

#include <iostream>
#include <cstring>
#include <mutex>
#include <map>
#include <condition_variable>
//#include "ConfigReader.h"
#include "thunder/ThunderInterface.h"
using std::string;

class SmartMonitor
{


    // Variables for exiting on TERM signal.
    std::condition_variable m_act_cv;
    volatile bool m_isActive;
    volatile bool isConnected;
    std::mutex m_lock;

  //  MonitorConfig *config;

    static SmartMonitor *_instance;
    static const char *resCallsign;
    ThunderInterface *tiface;

    const static string LAUNCH_URL;
    std::string m_activeApp;
    void onDialEvent(DIALEVENTS dialEvent, const string &appName, const string &appId);
    
    SmartMonitor();
    ~SmartMonitor();
    void handleTermSignal(int _sig);

public:
    int initialize();
    void connectToThunder();

   
    void registerForEvents();

    void unRegisterForEvents();
    void waitForTermSignal();
    bool getConnectStatus();
    bool enableCasting();

    static SmartMonitor *getInstance();

    // no copying allowed
    SmartMonitor(const SmartMonitor &) = delete;
    SmartMonitor &operator=(const SmartMonitor &) = delete;
};