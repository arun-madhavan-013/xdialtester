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
#include <string>
#include <vector>
using namespace std;

#include "EventListener.h"
#include "json/json.h"

// Structure to hold app configuration data
struct AppConfig {
    std::string name;
    std::string baseurl;
    std::string deeplinkmethod;
};

// Global app configuration list
extern std::vector<AppConfig> g_appConfigList;

string getSubscribeRequest(const string &callsignWithVer, const string &event, int &id);
string getUnSubscribeRequest(const string &callsignWithVer, const string &event, int &id);
string getMemoryLimitRequest(int lowMem, int criticalMem, int &id);
string enableCastingToJson(bool enable, int &id);
string setFriendlyNameToJson(const std::string &name, int &id);
string getRegisterAppToJson(int &id, const string &appCallsigns);
string getThunderMethodToJson(const string &method, int &id);
string isCastingEnabledToJson(int &);
string setStandbyBehaviourToJson(int &id);
bool parseJson(const string &jsonMsg, Json::Value &root);
bool getParamObjectFromJsonString(const std::string &input, Json::Value &jObjOut);
bool convertResultStringToArray(const string &root, const string key, vector<string> &arr);
bool convertResultStringToBool(const string &root, bool &);
bool convertResultStringToBool(const string &jsonMsg, const string &key, bool &response);
bool isJsonRpcResultNull(const string &jsonMsg);
bool convertEventSubResponseToInt(const string &root, int &);
bool getValueOfKeyFromJson(const string &jsonMsg, const string &key, string &value);
bool isValidJsonResponse(const string &response);
string getClientListToJson(int &id);
string setAppStateToJson(const string &appName, const string &appId, const string &state, int &id);
string launchAppToJson(const string &appName, int &id);
string suspendAppToJson(const string &appName, int &id);
string shutdownAppToJson(const string &appName, int &id);
string sendDeepLinkToJson(const DialParams &dialParams, int &id);
