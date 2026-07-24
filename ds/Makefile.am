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


lib_LTLIBRARIES = libds.la

AIDL_CPPFLAGS = -I$(includedir)/rdk-halif-aidl \
        -I$(includedir)/binder_sdk

AIDL_LIBS = -L$(libdir)/rdk-halif-aidl \
        -lhdmiinput-vcurrent-cpp \
        -lplanecontrol-vcurrent-cpp \
        -lcommon-vcurrent-cpp \
        -lbinder \
        -lutils

libds_la_CPPFLAGS = -I ./include \
                    -I=$(includedir)/rdk/ds-hal \
                    -I=$(includedir)/rdk/iarmbus \
            -I=$(includedir)/rdk/halif/ds-hal \
            $(AIDL_CPPFLAGS)

libds_la_CXXFLAGS = -g -fPIC -D_REENTRANT -Wall -std=c++0x

libds_la_LDFLAGS = -L../rpc/cli/.libs/ $(DS_LDFLAG)

libds_la_LIBADD = -ldshalcli \
          -lIARMBus \
          $(AIDL_LIBS)

libds_la_SOURCES = aspectRatio.cpp \
                   audioStereoMode.cpp \
                   iarmProxy.cpp \
                   pixelResolution.cpp \
                   videoOutputPortConfig.cpp \
                   audioCompression.cpp \
                   dslogger.cpp \
                   iarmProxy.hpp \
                   sleepMode.cpp \
                   videoOutputPortConfig.hpp \
                   audioEncoding.cpp \
                   frameRate.cpp \
                   stereoScopicMode.cpp \
                   videoOutputPort.cpp \
                   audioOutputPortConfig.cpp \
                   frontPanelConfig.cpp \
                   init.cpp \
                   videoDeviceConfig.cpp \
                   videoOutputPortType.cpp \
                   audioOutputPortConfig.hpp \
                   frontPanelIndicator.cpp \
                   videoDeviceConfig.hpp \
                   videoResolution.cpp \
                   hdmiIn.cpp \
                   audioOutputPort.cpp \
                   frontPanelTextDisplay.cpp \
                   videoDevice.cpp \
                   audioOutputPortType.cpp \
                   host.cpp \
                   manager.cpp \
                   videoDFC.cpp

