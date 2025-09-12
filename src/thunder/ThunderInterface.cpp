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

#include "ThunderInterface.h"
#include "ProtocolHandler.h"
#include "ResponseHandler.h"
#include "EventUtils.h"

// static bool debug = false;
void ThunderInterface::connected(bool connected)
{
    LOGTRACE("Connection update .. %s", connected ? "true" : "false");
    if (nullptr != m_connListener)
        m_connListener(connected);
}
void ThunderInterface::onMsgReceived(const string message)
{
    ResponseHandler *evtHandler = ResponseHandler::getInstance();
    LOGINFO(" %s", message.c_str());
    int msgId = 0;
    if (getMessageId(message, msgId))
    {
        evtHandler->addMessageToResponseQueue(msgId, message);
    }
    else
    {
        evtHandler->addMessageToEventQueue(message);
    }
}

int ThunderInterface::initialize()
{
    LOGTRACE(" Enter.");
    mp_handler->registerConnectionHandler([this](bool isConnected)
                                          { connected(isConnected); });
    mp_handler->registerMessageHandler([this](string message)
                                       { onMsgReceived(message); });

    ResponseHandler::getInstance()->registerEventListener(this);
    int status = mp_handler->initializeTransport();
    LOGTRACE(" Exit.");
    return status;
}

ThunderInterface::~ThunderInterface()
{
    LOGTRACE(" Enter.");

    if (mp_handler->isConnected())
    {
        mp_handler->disconnect();
        mp_thThread->join();
    }

    delete mp_handler;
    delete mp_thThread;
}
void ThunderInterface::setThunderConnectionURL(const std::string &wsurl)
{
    LOGTRACE(" Enter.");
    mp_handler->setConnectURL(wsurl);
}
void ThunderInterface::connectToThunder()
{
    LOGTRACE(" Enter.");
    if (mp_thThread != nullptr)
    {
        // Do we need to join and then delete ?
        delete mp_thThread;
    }
    auto &handler = mp_handler;
    mp_thThread = new std::thread([handler]
                                  { handler->connect(); });
}

bool ThunderInterface::enableCasting(bool enable )
{
    LOGTRACE("Enter.. ");
    bool status = false;
    int msgId = 0;
    ResponseHandler *evtHandler = ResponseHandler::getInstance();
    std::string jsonmsg = enableCastingToJson();

    if (mp_handler->sendMessage(jsonmsg) == 1) // Success
    {
         string response = evtHandler->getRequestStatus(msgId);
            convertResultStringToBool(response, status);
    }
    return status;
}

bool ThunderInterface::isCastingEnabled(string &result)
{
    LOGTRACE("Checking if casting is enabled.. ");
    bool status = false;
    int msgId = 0;

    ResponseHandler *evtHandler = ResponseHandler::getInstance();
    std::string jsonmsg = getThunderMethodToJson("org.rdk.Xcast.1.getEnabled", msgId);

    if (mp_handler->sendMessage(jsonmsg) == 1) // Success
    {
         string response = evtHandler->getRequestStatus(msgId);
         getParamFromResult(response, "enabled", result);
    }
    return status;
}
bool ThunderInterface::getFriendlyName(std::string &name)
{
    LOGTRACE("Getting friendly name.. ");
    bool status = false;
    int msgId = 0;

    ResponseHandler *evtHandler = ResponseHandler::getInstance();
    std::string jsonmsg = getThunderMethodToJson("org.rdk.System.getFriendlyName", msgId);

    if (mp_handler->sendMessage(jsonmsg) == 1) // Success
    {
         string response = evtHandler->getRequestStatus(msgId);
         getParamFromResult(response, "friendlyName", name);
    }
    return status;
}
bool ThunderInterface::enableYoutubeCasting()
{
    LOGTRACE("Enabling youtube casting.. ");
    bool status = false;
    int msgId = 0;

    ResponseHandler *evtHandler = ResponseHandler::getInstance();
    std::string jsonmsg = getYoutubeRegisterToJson( msgId);
    LOGINFO(" RRegistering Youtube  : %s", jsonmsg.c_str());
    if (mp_handler->sendMessage(jsonmsg) == 1) // Success
    {
         string response = evtHandler->getRequestStatus(msgId);
         convertResultStringToBool(response, status);
    }
    return status;
}

bool ThunderInterface::sendMessage(const string jsonmsg, int msgId, int timeout)
{
    bool status = false;
    ResponseHandler *evtHandler = ResponseHandler::getInstance();
    LOGINFO(" Request : %s", jsonmsg.c_str());

    if (mp_handler->sendMessage(jsonmsg) == 1) // Success
    {
        string response = evtHandler->getRequestStatus(msgId, timeout);
        convertResultStringToBool(response, status);
    }
    return status;
}
bool ThunderInterface::sendSubscriptionMessage(const string jsonmsg, int msgId, int timeout)
{
    int status = false;
    ResponseHandler *evtHandler = ResponseHandler::getInstance();
    LOGINFO(" Request : %s", jsonmsg.c_str());

    if (mp_handler->sendMessage(jsonmsg) == 1) // Success
    {
        string response = evtHandler->getRequestStatus(msgId, timeout);
        convertEventSubResponseToInt(response, status);
    }
    return status == 0;
}

void ThunderInterface::shutdown()
{
    mp_handler->disconnect();
    ResponseHandler::getInstance()->shutdown();
    mp_thThread->join();
}

void ThunderInterface::registerEvent(const std::string &event, bool isbinding)
{
    int msgId = 0;
    bool status = false;
    std::string callsign = "org.rdk.Xcast.1.";

    std::string jsonmsg;
    if (isbinding)
        jsonmsg = getSubscribeRequest(callsign, event, msgId);
    else
        jsonmsg = getUnSubscribeRequest(callsign, event, msgId);
    status = sendMessage(jsonmsg, msgId);

    LOGINFO(" Event %s, response  %d ", event.c_str(), status);
}
/**
 *
 * Implementation of event handlers
 */
// Do not call this directly. These are callback functions
void ThunderInterface::onDialEvents(DIALEVENTS dialEvent, const DialParams &dialParams)
{
    LOGINFO("%s  %s", dialParams.appName.c_str(), dialParams.appId.c_str());
    if (nullptr != m_dialListener)
        m_dialListener(dialEvent, dialParams);
}

void ThunderInterface::registerDialRequests(std::function<void(DIALEVENTS, const DialParams &)> callback)
{
    m_dialListener = callback;

    // Register for events
    registerEvent("onApplicationHideRequest", true);
    registerEvent("onApplicationLaunchRequest", true);
    registerEvent("onApplicationResumeRequest", true);
    registerEvent("onApplicationStateRequest", true);
    registerEvent("onApplicationStopRequest", true);
}

void ThunderInterface::removeDialListener()
{
    m_dialListener = nullptr;

    registerEvent("onApplicationHideRequest", false);
    registerEvent("onApplicationLaunchRequest", false);
    registerEvent("onApplicationResumeRequest", false);
    registerEvent("onApplicationStateRequest", false);
    registerEvent("onApplicationStopRequest", false);
}
std::vector<string> &ThunderInterface::getActiveApplications(int timeout)
{
    int id = 0;
    ResponseHandler *evtHandler = ResponseHandler::getInstance();
    m_appList.clear();
    string jsonmsg = getClientListToJson(id);
    LOGINFO("Clients request API : %s", jsonmsg.c_str());

    if (mp_handler->sendMessage(jsonmsg) == 1) // Success
    {
        string response = evtHandler->getRequestStatus(id);
        convertResultStringToArray(response, "clients", m_appList);
    }
    return m_appList;
}
bool ThunderInterface::setAppState(const std::string &appName, const std::string &appId, const std::string &state, int timeout)
{
    int id = 0;
    bool status = false;
    ResponseHandler *evtHandler = ResponseHandler::getInstance();
    string jsonmsg = setAppStateToJson(appName, appId, state, id);
    LOGINFO(" State change request API : %s", jsonmsg.c_str());

    if (mp_handler->sendMessage(jsonmsg) == 1) // Success
    {
        string response = evtHandler->getRequestStatus(id);
        convertResultStringToBool(response, status);
    }
    return status;
}
bool ThunderInterface::launchPremiumApp(const std::string &appName, int timeout)
{
    int id = 0;
    bool status = false;
    std::string callsign = (appName == "YouTube") ? "Cobalt" : appName;
    ResponseHandler *evtHandler = ResponseHandler::getInstance();
    string jsonmsg = launchAppToJson(callsign, id);
    LOGINFO(" Launch request API : %s", jsonmsg.c_str());

    if (mp_handler->sendMessage(jsonmsg) == 1) // Success
    {
        string response = evtHandler->getRequestStatus(id);
        convertResultStringToBool(response, status);
    }
    return status;
}

bool ThunderInterface::setStandbyBehaviour()
{
    LOGTRACE("Enabling standby behaviour as active.. ");
    bool status = false;
    int msgId = 0;
    ResponseHandler *evtHandler = ResponseHandler::getInstance();
    string jsonmsg = setStandbyBehaviourToJson(msgId);
    LOGINFO(" Standby active API : %s", jsonmsg.c_str());
    if (mp_handler->sendMessage(jsonmsg) == 1) // Success
    {
         string response = evtHandler->getRequestStatus(msgId);
         convertResultStringToBool(response, status);
    }
    return status;
}
bool ThunderInterface::shutdownPremiumApp(const std::string &appName, int timeout)
{
    int id = 0;
    bool status = false;
    std::string callsign = (appName == "YouTube") ? "Cobalt" : appName;
    ResponseHandler *evtHandler = ResponseHandler::getInstance();
    string jsonmsg = shutdownAppToJson(callsign, id);
    LOGINFO(" Stop request API : %s", jsonmsg.c_str());

    if (mp_handler->sendMessage(jsonmsg) == 1) // Success
    {
        string response = evtHandler->getRequestStatus(id);
        convertResultStringToBool(response, status);
    }
    return status;
}
bool ThunderInterface::sendDeepLinkRequest(const DialParams &dialParams)
{
    int id = 0;
    bool status = false;
    std::string callsign = (dialParams.appName == "YouTube") ? "Cobalt" : dialParams.appName;
    ResponseHandler *evtHandler = ResponseHandler::getInstance();
    string jsonmsg = sendDeepLinkToJson(dialParams, id);
    LOGINFO(" Deep link request API : %s", jsonmsg.c_str());

    if (mp_handler->sendMessage(jsonmsg) == 1) // Success
    {
        string response = evtHandler->getRequestStatus(id);
        convertResultStringToBool(response, status);
    }
    return status;
}

