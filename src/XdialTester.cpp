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

static const char *VERSION = "0.1.1";

int main(int argc, char *argv[])
{
    LOGINFO("Smart Monitor: %s" , VERSION);

    SmartMonitor *nappmgr = SmartMonitor::getInstance();
    nappmgr->initialize();
    
    do
    {
        nappmgr->connectToThunder();
         LOGINFO("Waiting for connection status ");
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    } while (!nappmgr->getConnectStatus());
    nappmgr->registerForEvents();
    nappmgr->enableCasting();
    

    nappmgr->waitForTermSignal();

    return 0;
}