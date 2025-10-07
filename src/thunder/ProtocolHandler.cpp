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
#include <memory>
#include <sstream>
#include "json/json.h"

#include "ProtocolHandler.h"
#include "EventUtils.h"

// Forward declaration for ThunderInterface
class ThunderInterface;

// External reference to get app configurations from ThunderInterface
extern std::vector<AppConfig> g_appConfigList;

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
    Json::StreamWriterBuilder builder;
    builder["indentation"] = ""; // No indentation, similar to FastWriter
    std::ostringstream os;
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &os);
    return os.str();
}

string getThunderMethodToJson(const string &method, int &id)
{
    Json::Value root;
    addVersion(root, id);

    root["method"] = method;

    return getStringFromJson(root);
}

string getRegisterAppToJson(int &id, const string &appCallsigns)
{
    Json::Value root;
    addVersion(root, id);

    root["method"] = "org.rdk.Xcast.1.registerApplications";

    Json::Value params;
    Json::Value applications(Json::arrayValue);

	if (appCallsigns.find("YouTube") != string::npos) {
		// hardcoding youtube app details for now
		Json::Value app1;
		app1["name"] = "YouTube";
		app1["prefix"] = "myYoutube";
		Json::Value cors(Json::arrayValue);
		cors.append(".youtube.com");
		app1["cors"] = cors;
		Json::Value properties;
		properties["allowStop"] = true;
		app1["properties"] = properties;

		applications.append(app1);

		Json::Value app2;
		app2["name"] = "YouTubeTV";
		app2["prefix"] = "myYouTubeTV";
		Json::Value cors2(Json::arrayValue);
		cors2.append(".youtube.com");
		app2["cors"] = cors2;
		Json::Value properties2;
		properties2["allowStop"] = true;
		app2["properties"] = properties2;

		applications.append(app2);
	}
	if (appCallsigns.find("Netflix") != string::npos) {
		// hardcoding netflix app details for now
		Json::Value app1;
		app1["name"] = "Netflix";
		app1["prefix"] = "myNetflix";
		Json::Value cors(Json::arrayValue);
		cors.append(".netflix.com");
		app1["cors"] = cors;
		Json::Value properties;
		properties["allowStop"] = true;
		app1["properties"] = properties;

		applications.append(app1);
	}

	if (appCallsigns.find("Amazon") != string::npos) {
		// hardcoding amazon app details for now
		Json::Value app1;
		app1["name"] = "AmazonInstantVideo";
		app1["prefix"] = "myPrimeVideo";
		Json::Value cors(Json::arrayValue);
		cors.append(".amazon.com");
		app1["cors"] = cors;
		Json::Value properties;
		properties["allowStop"] = true;
		app1["properties"] = properties;

		applications.append(app1);
	}

    params["applications"] = applications;

    root["params"] = params;

    return getStringFromJson(root);
}
string enableCastingToJson(bool enable, int &id)
{
    Json::Value root;
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

/*
    Expecting some thing like
    {"jsonrpc":"2.0","id":4,"result":{"clients":["vol_overlay","amazon","residentapp"],"success":true}
    and key in this case clients
*/

string getClientListToJson(int &id)
{
    Json::Value root;
    addVersion(root, id);

    root["method"] = "org.rdk.RDKShell.1.getClients";

    return getStringFromJson(root);
}

bool parseJson(const string &jsonMsg, Json::Value &root)
{
    // Check for empty JSON message
    if (jsonMsg.empty()) {
        LOGERR("Cannot parse empty JSON message");
        return false;
    }

    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string errs;

    bool parsingSuccessful = reader->parse(
        jsonMsg.c_str(),
        jsonMsg.c_str() + jsonMsg.size(),
        &root,
        &errs);
    if (!parsingSuccessful)
    {
        LOGERR("Failed to parse the json message: %s, error: %s", jsonMsg.c_str(), errs.c_str());
    }
    return parsingSuccessful;
}

bool getResultObject(const string &jsonMsg, Json::Value &result)
{
    Json::Value root;
    if (!parseJson(jsonMsg, root))
        return false;
    result = root["result"];
    return result.isObject();
}

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
    bool status = false;
    Json::Value root;

    if (parseJson(jsonMsg, root))
    {
        if (!root["id"].empty())
        {
            msgId = root["id"].asInt();
            status = true;
        }
    }
    return status;
}
bool getEventId(const string &jsonMsg, string &evtName)
{
    bool status = false;
        Json::Value root;
    if (parseJson(jsonMsg, root))
    {
        if (!root["method"].empty())
        {
            evtName = root["method"].asString();
            status = true;
        }
    }
    return status;
}

bool getDialEventParams(const string &jsonMsg, DialParams &params)
{
    Json::Value root;
    bool status = false;

    if (parseJson(jsonMsg, root))
    {
        if (root["params"].isObject())
        {
            Json::Value jparams = root["params"];
            params.appName = jparams["applicationName"].asString();
            if (!jparams["applicationId"].isNull())
                params.appId = jparams["applicationId"].asString();
            if (!jparams["strPayLoad"].isNull())
                params.strPayLoad = jparams["strPayLoad"].asString();
            if (!jparams["strQuery"].isNull())
                params.strQuery = jparams["strQuery"].asString();
            if (!jparams["strAddDataUrl"].isNull())
                params.strAddDataUrl = jparams["strAddDataUrl"].asString();
            status = true;
        }
    }
    return status;
}

bool getValueOfKeyFromJson(const string &jsonMsg, const string &key, string &value)
{
	bool status = false;
	Json::Value root;
	if (parseJson(jsonMsg, root))
	{
		if (!root[key].isNull())
		{
			value = root[key].asString();
			status = true;
		}
	}
	return status;
}

bool getParamFromResult(const string &jsonMsg, const string &param, string &value)
{
    bool status = false;
    Json::Value root;
    if (parseJson(jsonMsg, root))
    {
        if (root["result"].isObject())
        {
            Json::Value params = root["result"];
            value = params[param].asString();
            status = true;
        }
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

string setAppStateToJson(const string &appName, const string &appId, const string &state, int &id)
{
    Json::Value root;
    addVersion(root, id);

    root["method"] = "org.rdk.Xcast.1.setApplicationState";
    root["params"]["applicationName"] = appName;
    root["params"]["applicationId"] = appId;
    root["params"]["state"] = state;
    root["params"]["error"] = "none";

    return getStringFromJson(root);
}

string launchAppToJson(const string &appName, int &id)
{
    Json::Value root;
    addVersion(root, id);

    root["method"] = "org.rdk.RDKShell.1.launch";
    root["params"]["callsign"] = appName;
    root["params"]["type"] = appName;

    return getStringFromJson(root);
}
string setStandbyBehaviourToJson(int &id)
{
    Json::Value root;
    addVersion(root, id);

    root["method"] = "org.rdk.Xcast.1.setStandbyBehavior";
    root["params"]["standbybehavior"] = "active";

    return getStringFromJson(root);
}

string suspendAppToJson(const string &appName, int &id)
{
    Json::Value root;
    addVersion(root, id);

    root["method"] = "org.rdk.RDKShell.1.suspend";
    root["params"]["callsign"] = appName;

    return getStringFromJson(root);
}

string shutdownAppToJson(const string &appName, int &id)
{
    Json::Value root;
    addVersion(root, id);

    root["method"] = "org.rdk.RDKShell.1.destroy";
    root["params"]["callsign"] = appName;

    return getStringFromJson(root);
}

string sendDeepLinkToJson(const DialParams &dialParams, int &id)
{
    Json::Value root;
    addVersion(root, id);

    string method;
    string url;
    bool found = false;
    string netflixIIDInfo = "source_type=12&iid=99a5fb82";

    for (const auto& appConfig : g_appConfigList) {
        if (appConfig.name == dialParams.appName) {
            method = appConfig.deeplinkmethod;
            url = appConfig.baseurl;
            found = true;
            break;
        }
    }

    if (!found) {
        LOGERR("App configuration not found for %s", dialParams.appName.c_str());
        return "";
    }

    if (method.empty()) {
        LOGERR("Deeplink method not configured for app %s", dialParams.appName.c_str());
        return "";
    }

    root["method"] = method;

    // Check if base URL already has parameters
    bool hasParams = (url.find('?') != string::npos);

    if (!dialParams.strPayLoad.empty()) {
        url.append(hasParams ? "&" : "?").append(dialParams.strPayLoad);
        hasParams = true;
    }

    if (!dialParams.strQuery.empty()) {
        url.append(hasParams ? "&" : "?").append(dialParams.strQuery);
        hasParams = true;
    }

    if (!dialParams.strAddDataUrl.empty()) {
        url.append(hasParams ? "&" : "?").append(dialParams.strAddDataUrl);
        hasParams = true;
    }

    if (dialParams.appName == "Netflix") {
        url.append(hasParams ? "&" : "?").append(netflixIIDInfo);
    }

    root["params"] = url;

    LOGINFO("Generated deeplink for %s: method=%s, url=%s",
           dialParams.appName.c_str(), method.c_str(), url.c_str());

    return getStringFromJson(root);
}
