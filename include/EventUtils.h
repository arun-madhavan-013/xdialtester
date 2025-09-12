/*
 * If not stated otherwise in this file or this component's  LICENSE  file the
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
#include <string>
#include <algorithm>

#include <syslog.h>
using std::cout;
using std::endl;
using std::string;

#include "EventListener.h"

#define REQUEST_TIMEOUT_IN_MS 1000

// These will be used for memory events to differentiate between critical and low memory states.
template <typename T>
inline void dumpVector(const std::vector<T> &vect)
{
    std::cout << "[dumpVector] { ";
    for (auto msgId : vect)
    {
        std::cout << msgId << " ";
    }
    std::cout << "} " << std::endl;
}
template <typename K, typename V>
inline void dumpMap(std::map<K, V> &rmap)
{
    std::cout << "[dumpMap] { ";
    for (auto &t : rmap)
    {
        std::cout << "(" << t.first << " , " << t.second << ") ";
    }
    std::cout << "} " << std::endl;
}
// Unless we move to C++17, I don't see an option to do this properly
#include <cstring>
inline bool stringCompareIgnoreCase(const std::string &a, const std::string &b)
{
    if (a.length() == b.length())
        return strncasecmp(a.c_str(), b.c_str(), a.length()) == 0;
    return false;
}
extern bool debug;
bool isDebugEnabled();

bool getMessageId(const string &jsonMsg, int &msgId);
bool getEventId(const string &jsonMsg, string &evtName);
bool getDialEventParams(const string &jsonMsg, DialParams &params);

bool getParamFromResult(const string &jsonMsg, const string & param, string &value);

#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

//#define LOGTRACE(fmt, ...) do { fprintf(stderr, "TRACE [%s:%d] %s: " fmt "\n",  __FILENAME__, __LINE__, __FUNCTION__, ##__VA_ARGS__); fflush(stderr); } while (0)
#define LOGTRACE(fmt, ...) 

#define LOGINFO(fmt, ...) do { fprintf(stderr, "INFO [%s:%d] %s: " fmt "\n",  __FILENAME__, __LINE__, __FUNCTION__, ##__VA_ARGS__); fflush(stderr); } while (0)
#define LOGWARN(fmt, ...) do { fprintf(stderr, "WARN [%s:%d] %s: " fmt "\n",  __FILENAME__, __LINE__, __FUNCTION__, ##__VA_ARGS__); fflush(stderr); } while (0)
#define LOGERR(fmt, ...) do { fprintf(stderr, "ERROR [%s:%d] %s: " fmt "\n",  __FILENAME__ , __LINE__, __FUNCTION__, ##__VA_ARGS__); fflush(stderr); } while (0)
