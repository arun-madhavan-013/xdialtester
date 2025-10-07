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
// #include "ConfigReader.h"
#include "thunder/ThunderInterface.h"
using std::string;

typedef enum { YOUTUBE, NETFLIX, AMAZON, APPLIMIT } DialApps;

typedef struct appDialState_t
{
	DialApps app;
	string dialState;
	string pluginState;
} appDialState_t;

class SmartMonitor
{

  // Variables for exiting on TERM signal.
  std::condition_variable m_act_cv;
  volatile bool m_isActive;
  volatile bool isConnected;
  std::mutex m_lock;
  appDialState_t m_dialApps[DialApps::APPLIMIT];

  //  MonitorConfig *config;

  static SmartMonitor *_instance;
  static const char *resCallsign;
  ThunderInterface *tiface;

  void onDialEvent(DIALEVENTS dialEvent, const DialParams &dialParams);
  void onRDKShellEvent(const std::string &event, const std::string &params);
  void onControllerStateChangeEvent(const std::string &event, const std::string &params);

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
  bool checkAndEnableCasting();
  bool registerDIALApps(const string &appCallsigns);
  bool getPluginState(const string &myapp, string &state);
  bool convertPluginStateToDIALState(const std::string &pluginState, std::string &dialState);
  bool isAppRunning(const string &myapp);
  bool setStandbyBehaviour();

  static SmartMonitor *getInstance();

  // no copying allowed
  SmartMonitor(const SmartMonitor &) = delete;
  SmartMonitor &operator=(const SmartMonitor &) = delete;
};
