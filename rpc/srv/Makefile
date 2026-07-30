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
RM          := rm -rf 
CFLAGS      += -g -fPIC -D_REENTRANT -Wall
LIBNAME     := dshalsrv
LIBNAMEFULL := lib$(LIBNAME).so
INSTALL     := $(PWD)/install
OBJS        := $(patsubst %.cpp,%.o,$(wildcard *.cpp))
OBJS        += $(patsubst %.c,%.o,$(wildcard *.c))
ifneq ($(STANDALONE_BUILD_ENABLED),y)
INCLUDE     := -I$(PWD)                         \
               -I$(PWD)/hal/include             \
               -I$(PWD)/hal/src             \
               -I$(PWD)/rpc/include  
else
INCLUDE     := -I$(PWD)                         \
			   -I$(PWD)/hal/include             \
			   -I$(PWD)/rpc/include   
endif        


AIDL_INCLUDE_DIR   ?= $(includedir)/rdk-halif-aidl
BINDER_INCLUDE_DIR ?= $(includedir)/binder_sdk
AIDL_LIB_DIR       ?= $(libdir)/rdk-halif-aidl
BINDER_LIB_DIR     ?= $(libdir)
AIDL_INCLUDE  := -I$(AIDL_INCLUDE_DIR) -I$(BINDER_INCLUDE_DIR)
AIDL_LDFLAGS ?= -L$(AIDL_LIB_DIR) -L$(BINDER_LIB_DIR) \
                -lhdmiinput-vcurrent-cpp -lplanecontrol-vcurrent-cpp \
                -lcommon-vcurrent-cpp -lbinder -lutils

INCLUDE      += $(HAL_INCLUDE)
INCLUDE	     += -I=$(includedir)/rdk/ds-hal
INCLUDE      += $(AIDL_INCLUDE)

CFLAGS      += $(INCLUDE)

CFLAGS      +=$(HAL_BUILDFLAGS)

# Add RDK_DSHAL_NAME for non-yocto builds
ifeq (,$(findstring -DYOCTO_BUILD,$(CFLAGS)))
CFLAGS      +=-DRDK_DSHAL_NAME=\"libdshal.so\"
endif

all: install
	@echo "Build Finished...."

library: $(OBJS)
	@echo "Building $(LIBNAMEFULL) ...."
	$(CXX) $(OBJS) $(CFLAGS) $(LDFLAGS) $(AIDL_LDFLAGS) -shared -o $(LIBNAMEFULL)

%.o: %.c
	@echo "Building $@ ...."
	$(CXX) -c $<  $(CFLAGS) -o $@

%.o: %.cpp
	@echo "Building $@ ...."
	$(CXX) -std=c++0x -c $<  $(CFLAGS) -o $@

install: library
	@echo "installling...!"
	@cp -f $(LIBNAMEFULL) $(INSTALL)/lib

uninstall:
	@$(RM) $(INSTALL)/lib/$(LIBNAMEFULL)

clean:
	@echo "Cleaning the directory..."
	@$(RM) $(OBJS) $(LIBNAMEFULL)



