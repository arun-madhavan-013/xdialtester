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
#include <random>
#include <systemd/sd-daemon.h>

#include "SmartMonitor.h"
#include "EventUtils.h"

// Global debug variables - check environment variable or command line flag
bool debug = (getenv("SMDEBUG") != NULL);
bool tdebug = (getenv("SMDEBUG") != NULL);
bool traceEnabled = (getenv("SMDEBUG") != NULL && std::string(getenv("SMDEBUG")) == "TRACE");

static const char *VERSION = "1.1.2";

#ifndef GIT_SHORT_SHA
#define GIT_SHORT_SHA "unknown"
#endif

// Generate 8-digit random number for default friendly name
std::string generateDefaultFriendlyName() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10000000, 99999999);
    return "RDKE-" + std::to_string(dis(gen));
}

/***
 * Main entry point for the application
 * Usage: xdialtester --enable-apps=app1,app2,app3 [--enable-debug] [--enable-trace] [--friendlyname=myDevice12345]
 */
int main(int argc, char *argv[])
{
    LOGINFO("Smart Monitor: %s (%s)" , VERSION, GIT_SHORT_SHA);
    string appCallsigns = "YouTube,Netflix,Amazon";
    string friendlyname = generateDefaultFriendlyName();
    if (argc > 1) {
		for (int i = 1; i < argc; i++) {
		    string arg = argv[i];
		    if (arg.find("--enable-apps=") != string::npos) {
			    string apps = arg.substr(arg.find("=") + 1);
				appCallsigns = apps;
		    } else if (arg == "--enable-debug") {
			    debug = true;
			    tdebug = true;
			    LOGINFO("Debug mode enabled");
		    } else if (arg == "--enable-trace") {
			    traceEnabled = true;
			    LOGINFO("Trace logging enabled");
			} else if (arg.find("--friendlyname=") != string::npos) {
				friendlyname = arg.substr(arg.find("=") + 1);
		    } else {
			    LOGERR("Invalid argument %s. Usage: xdialtester --enable-apps=app1,app2,app3 [--enable-debug] [--enable-trace] [--friendlyname=myDevice12345]", arg.c_str());
			    return -1;
		    }
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
    smon->checkAndEnableCasting(friendlyname);
	LOGINFO("Enabling DIAL apps: %s", appCallsigns.c_str());
    smon->registerDIALApps(appCallsigns);
    smon->waitForTermSignal();

    return 0;
}
