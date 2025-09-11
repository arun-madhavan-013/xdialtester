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

#include "json/json.h"

#include "ProtocolHandler.h"
#include "EventUtils.h"

static int event_id = 1001;
string getSubscribtionRequest(const string &callsign, const string &event, bool subscribe, int &id, int eventId = -1);

void addVersion(Json::Value &root, int &id)
{
    root["jsonrpc"] = "2.0";
    id = event_id;
    root["id"] = std::to_string(id);
    event_id++;
}

string getStringFromJson(Json::Value &root)
{
    Json::FastWriter writer;
    const string json_request = writer.write(root);
    return json_request;
}
//{"applications":[{"names":["YouTube"],"cors":[".youtube.com"],"properties":{"allowStop":false}},{"names":["YouTubeTV"],"cors":[".youtube.com"],"properties":{"allowStop":false}}]}

string getThunderMethodToJson(const string &method, int &id)
{
    Json::Value root;
    addVersion(root, id);

    root["method"] = method;

    return getStringFromJson(root);
}
string enableCastingToJson(bool enable )
{
    Json::Value root;
    int id = 0;
    addVersion(root, id);

    root["method"] = "org.rdk.Xcast.1.setEnabled";

    Json::Value params;
    params["enabled"] = enable;

    root["params"] = params;

    return getStringFromJson(root);
}
string isCastingEnabledToJson(int &id)
{
    return getThunderMethodToJson("org.rdk.Xcast.1.getEnabled", id);
}

string getSubscribtionRequest(const string &callsign, const string &event, bool subscribe, int &id, int eventId)
{
    Json::Value root;
    addVersion(root, id);

    string method = callsign;
    method = method + (subscribe ? "register" : "unregister");
    root["method"] = method;

    Json::Value params;
    params["event"] = event;
    if (eventId == -1)
    {
        params["id"] = std::to_string(event_id);
        event_id++;
    }
    else
        params["id"] = std::to_string(eventId);

    root["params"] = params;
    return getStringFromJson(root);
}

string getSubscribeRequest(const string &callsign, const string &event, int &id)
{
    return getSubscribtionRequest(callsign, event, true, id);
}

string getUnSubscribeRequest(const string &callsignWithVer, const string &event, int &id)
{
    return getSubscribtionRequest(callsignWithVer, event, false, id);
}
string getClientList(int &id)
{
    Json::Value root;
    addVersion(root, id);

    root["method"] = "org.rdk.RDKShell.1.getClients";

    return getStringFromJson(root);
}

bool getResultObject(const string &jsonMsg, Json::Value &result)
{
    Json::Reader reader;
    Json::Value root;

    reader.parse(jsonMsg, root);
    result = root["result"];
    return result.isObject();
}
/*
    Expecting some thing like
    {"jsonrpc":"2.0","id":4,"result":{"clients":["vol_overlay","amazon","residentapp"],"success":true}
    and key in this case clients
*/
bool convertResultStringToArray(const string &jsonMsg, const string key, std::vector<string> &arr)
{
    Json::Value result;
    bool status = false;
    if (!getResultObject(jsonMsg, result))
        return status;

    Json::Value clients = result[key];
    if (clients.isArray())
    {
        for (auto &x : clients)
        {
            arr.emplace_back(x.asString());
        }
        status = true;
    }
    return status;
}
/*
    Expecting some thing like
    {"jsonrpc":"2.0","id":1002,"result":{"launchType":"activate","success":true}}
    and in this case key is success
*/
bool convertResultStringToBool(const string &jsonMsg, bool &response)
{
    Json::Value result;
    bool status = false;

    if (!getResultObject(jsonMsg, result))
        return status;

    Json::Value bstat = result["status"];

    if (bstat.isBool())
    {
        response = bstat.asBool();
        status = true;
    }
    return status;
}
/*
    Expecting some thing like
    {"jsonrpc":"2.0","id":1001,"result":0}
*/
bool convertEventSubResponseToInt(const string &jsonMsg, int &response)
{
    Json::Value result;
    bool status = false;

    getResultObject(jsonMsg, result);

    if (result.isInt())
    {
        response = result.asInt();
        status = true;
    }
    return status;
}

/// Implementation of EventUtils.h

bool getMessageId(const string &jsonMsg, int &msgId)
{
    Json::Reader reader;
    Json::Value root;
    bool status = false;

    msgId = -1;
    reader.parse(jsonMsg, root);
    if (!root["id"].empty())
    {
        msgId = root["id"].asInt();
        status = true;
    }
    return status;
}
bool getEventId(const string &jsonMsg, string &evtName)
{
    Json::Reader reader;
    Json::Value root;
    bool status = false;

    reader.parse(jsonMsg, root);
    if (!root["method"].empty())
    {
        evtName = root["method"].asString();
        status = true;
    }
    return status;
}


bool getDialEventParams(const string &jsonMsg, string &client, string &type)
{
    Json::Reader reader;
    Json::Value root;
    bool status = false;

    reader.parse(jsonMsg, root);
    if (root["params"].isObject())
    {
        Json::Value params = root["params"];
        client = params["applicationName"].asString();
        type = params["applicationId"].asString();
        status = true;
    }
    return status;
}
bool getParamFromResult(const string &jsonMsg, const string & param, string &value)
{
    Json::Reader reader;
    Json::Value root;
    bool status = false;

    reader.parse(jsonMsg, root);
    if (root["params"].isObject())
    {
        Json::Value params = root["params"];
        value = params[param].asString();
        status = true;
    }
    return status;
}


bool isDebugEnabled()
{
    if (getenv("SMDEBUG") != NULL)
    {
        LOGINFO("Enabling debug mode.. ");
        return true;
    }
    return false;
}