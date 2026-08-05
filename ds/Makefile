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
CFLAGS      += -std=c++0x  -g -fPIC -D_REENTRANT -Wall
LIBNAME     := ds
LIBNAMEFULL := lib$(LIBNAME).so
LIBNAMECLI  := lib$(LIBNAME)-cli.so
INSTALL     := $(PWD)/install
OBJS        := $(patsubst %.cpp,%.o,$(wildcard *.cpp))
# In future add a conditional to choose implementation class based on build configuration
OBJS        += iarm/IarmImpl.o
#$(PWD)/config should be first include path
# to override generic configs
ifneq ($(STANDALONE_BUILD_ENABLED),y)
INCLUDE     := -I$(PWD)                                 \
               -I$(LOG4C_INSTALL_DIR)/include           \
               -I$(PWD)/config                       \
    		   -I$(PWD)/hal/include                     \
    		   -I$(PWD)/hal/src                \
    		   -I$(PWD)/ds/include                      \
    		   -I$(PWD)/ds                              \
    		   -I$(PWD)/rpc/include                      
else
INCLUDE     := -I$(PWD)                                 \
               -I$(LOG4C_INSTALL_DIR)/include           \
               -I$(PWD)/config                       \
    		   -I$(PWD)/hal/include                     \
    		   -I$(PWD)/ds/include                      \
    		   -I$(PWD)/ds                              \
    		   -I$(PWD)/rpc/include
endif
              


INCLUDE      += $(HAL_INCLUDE)
CFLAGS       +=$(HAL_BUILDFLAGS)

CFLAGS      += $(INCLUDE)


all: install
	@echo "Build Finished...."

library: $(OBJS)
	@echo "Building $(LIBNAMEFULL) ...."
	$(CXX) $(OBJS) $(CFLAGS) $(DSHAL_LDFLAGS) -L$(INSTALL)/lib -lIARMBus -ldshalcli -shared -o $(LIBNAMEFULL)
	$(CXX) $(OBJS) $(CFLAGS) -L$(INSTALL)/lib -lIARMBus -ldshalcli -shared -o $(LIBNAMECLI)

%.o: %.cpp
	@echo "Building $@ ...."
	$(CXX) -c $<  $(CFLAGS) -o $@

install: library
	@echo "installling...!"
	@cp -f $(LIBNAMEFULL) $(INSTALL)/lib

uninstall:
	@$(RM) $(INSTALL)/lib/$(LIBNAMEFULL)

clean:
	@echo "Cleaning the directory..."
	@$(RM) $(OBJS) $(LIBNAMEFULL)


