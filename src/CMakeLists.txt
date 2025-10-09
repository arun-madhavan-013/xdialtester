# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2022 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Get Git short SHA
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHORT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# If Git command fails, set a default value
if(NOT GIT_SHORT_SHA)
    set(GIT_SHORT_SHA "unknown")
endif()

set(SECURITY_FLAGS "-fstack-protector-strong -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security")
set(LINKER_FLAGS "-Wl,-z,relro -Wl,-z,now")
set(WARNING_FLAGS "-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fPIC -D_REENTRANT -Werror ${WARNING_FLAGS} ${SECURITY_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie ${LINKER_FLAGS}")

add_executable(${TARGET}
   XdialTester.cpp
   SmartMonitor.cpp
   thunder/ThunderInterface.cpp
   thunder/TransportHandler.cpp
   thunder/ProtocolHandler.cpp
   thunder/ResponseHandler.cpp
)

# Add compile definition for Git SHA
target_compile_definitions(${TARGET} PRIVATE GIT_SHORT_SHA="${GIT_SHORT_SHA}")

find_package(PkgConfig)
find_package(jsoncpp)
find_package(websocketpp)


target_link_libraries(${TARGET}
        pthread jsoncpp systemd
)

set_target_properties(${TARGET} PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED YES
        )

install(TARGETS ${TARGET} DESTINATION bin)
