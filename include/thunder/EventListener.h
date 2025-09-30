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
#include <functional>

enum DIALEVENTS
{
    APP_LAUNCH_REQUEST_EVENT,
    APP_HIDE_REQUEST_EVENT,
    APP_RESUME_REQUEST_EVENT,
    APP_STOP_REQUEST_EVENT,
    APP_STATE_REQUEST_EVENT
};

typedef struct _dialParams
{
    std::string appName;
    std::string appId;
    std::string strPayLoad;
    std::string strQuery;
    std::string strAddDataUrl;
} DialParams;

class EventListener
{
protected:
    std::function<void(DIALEVENTS, const DialParams &)> m_dialListener;
    std::function<void(const std::string &, const std::string &)> m_rdkShellListener;

public:
    virtual void registerDialRequests(std::function<void(DIALEVENTS, const DialParams &)> callback) = 0;
    virtual void registerRDKShellEvents(std::function<void(const std::string &, const std::string &)> callback) = 0;

    virtual void removeDialListener() = 0;
    virtual void removeRDKShellListener() = 0;

    // Do not call this directly. These are callback functions
    virtual void onDialEvents(DIALEVENTS dialEvent, const DialParams &dialParams) = 0;
	virtual void onRDKShellEvents(const std::string &event, const std::string &params) = 0;
};
