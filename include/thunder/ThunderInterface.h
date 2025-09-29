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
#include <map>
#include <mutex>
#include <thread>

#include "EventUtils.h"
#include "TransportHandler.h"
#include "EventListener.h"



class ThunderInterface : public EventListener
{
public:
    ThunderInterface() : m_isInitialized(false), m_connListener(nullptr), mp_thThread(nullptr)
    {
        mp_handler = new TransportHandler();
    }
    int initialize();

    void setThunderConnectionURL(const std::string &wsurl);
    void connectToThunder();

    void shutdown();
    ~ThunderInterface();

    // no copying allowed
    ThunderInterface(const ThunderInterface &) = delete;
    ThunderInterface &operator=(const ThunderInterface &) = delete;

    // Inherited from EventListener class
    void registerDialRequests(std::function<void(DIALEVENTS, const DialParams &)> callback) override;
	void registerRDKShellEvents(std::function<void(const std::string &, const DialParams &)> callback) override;

    void registerConnectStatusListener(std::function<void(bool)> callback)
    {
        m_connListener = callback;
    };
    void removeDialListener() override;
    bool enableCasting(bool enable = true);
    bool isCastingEnabled(std::string &result);
    bool getFriendlyName(std::string &name);
    bool registerXcastApps(std::string &appCallsigns);
    bool setStandbyBehaviour();
    std::vector<string> & getActiveApplications(int timeout = REQUEST_TIMEOUT_IN_MS);
    bool setAppState( const std::string &appName, const std::string &appId, const std::string &state, int timeout = REQUEST_TIMEOUT_IN_MS);
    bool reportDIALAppState(const std::string &appName, const std::string &appId, const std::string &state);
    bool launchPremiumApp(const std::string &appName, int timeout = REQUEST_TIMEOUT_IN_MS);
    bool shutdownPremiumApp(const std::string &appName, int timeout = REQUEST_TIMEOUT_IN_MS);
    bool sendDeepLinkRequest(const DialParams &dialParams);
private:
    TransportHandler *mp_handler;
    bool m_isInitialized;
    std::vector<std::string> m_appList;

    std::function<void(bool)> m_connListener;

    std::thread *mp_thThread;
    const std::string m_homeURL;

    void connected(bool connected);
    void onMsgReceived(const std::string message);
    void registerEvent(const std::string &event, bool isBinding);
    void registerEvent(const std::string &callsignWithVersion, const std::string &event, bool isBinding);
    bool sendMessage(const std::string jsonmsg, int msgId, int timeout = REQUEST_TIMEOUT_IN_MS);
    bool sendSubscriptionMessage(const std::string jsonmsg, int msgId, int timeout = REQUEST_TIMEOUT_IN_MS);

    void onDialEvents(DIALEVENTS dialEvent, const DialParams &dialParams) override;
	void onRDKShellEvents(const std::string &event, const std::string &params) override;
};
