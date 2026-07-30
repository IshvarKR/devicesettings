##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2016 RDK Management
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
##########################################################################


SUBDIRS =
DIST_SUBDIRS =

INCLUDE_FILES = -I=$(includedir)/rdk/halif/ds-hal \
                -I ../include \
                -I=$(includedir)/rdk/iarmbus

lib_LTLIBRARIES = libdshalsrv.la
AIDL_CPPFLAGS_SRV = -I$(includedir)/rdk-halif-aidl \
        -I$(includedir)/binder_sdk

AIDL_LIBS_SRV = -L$(libdir)/rdk-halif-aidl \
        -lhdmiinput-vcurrent-cpp \
        -lplanecontrol-vcurrent-cpp \
        -lcommon-vcurrent-cpp \
        -lbinder \
        -lutils

libdshalsrv_la_CPPFLAGS = $(INCLUDE_FILES) $(AIDL_CPPFLAGS_SRV)
libdshalsrv_la_CXXFLAGS= -std=c++0x -g -fPIC -D_REENTRANT -Wall
libdshalsrv_la_CFLAGS = -x c++ -g -fPIC -D_REENTRANT -Wall
libdshalsrv_la_SOURCES = dsHost.cpp hostPersistence.cpp dsAudio.c dsDisplay.c dsFPD.c dsMgr.c dsVideoDevice.c dsVideoPort.c dsserverlogger.c \
                         dsConfigs.c dsAudioConfig.c dsVideoPortConfig.c dsVideoDeviceConfig.c dsCompositeIn.c dsHdmiIn.c
libdshalsrv_la_LIBADD = -ldl -ltelemetry_msgsender $(AIDL_LIBS_SRV)

