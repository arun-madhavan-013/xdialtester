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

#include <iostream>
#include <chrono>
#include <systemd/sd-daemon.h>

#include "SmartMonitor.h"
#include "EventUtils.h"

static const char *VERSION = "1.0.6";

#ifndef GIT_SHORT_SHA
#define GIT_SHORT_SHA "unknown"
#endif

/***
 * Main entry point for the application
 * Usage: xdialtester --enable-apps=app1,app2,app3
 */
int main(int argc, char *argv[])
{
    LOGINFO("Smart Monitor: %s (%s)" , VERSION, GIT_SHORT_SHA);
    string appCallsigns = "YouTube,Netflix,Amazon";
    if (argc > 1) {
	    string arg = argv[1];
	    // parse command line --enable-apps=app1,app2,app3
	    if (arg.find("--enable-apps=") != string::npos) {
		    string apps = arg.substr(arg.find("=") + 1);
			appCallsigns = apps;
	    }
    }

    SmartMonitor *smon = SmartMonitor::getInstance();
    smon->initialize();

    do
    {
        smon->connectToThunder();
         LOGINFO("Waiting for connection status ");
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    } while (!smon->getConnectStatus());
    smon->registerForEvents();
    smon->setStandbyBehaviour();
    smon->checkAndEnableCasting();
	LOGINFO("Enabling DIAL apps: %s", appCallsigns.c_str());
    smon->registerDIALApps(appCallsigns);
    smon->waitForTermSignal();

    return 0;
}
