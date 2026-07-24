/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
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

/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright ARRIS Enterprises, Inc. 2015.
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
 * limitations under
*/


/**
* @defgroup devicesettings
* @{
* @defgroup ds
* @{
**/


#ifndef _DS_HDMIIN_HPP_
#define _DS_HDMIIN_HPP_

#include <stdint.h>
#include <vector>
#include <map>
#include <mutex>

#include "dsTypes.h"
#include "dsError.h"

// AIDL includes — hdmiinput HAL
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <utils/String16.h>
#include <com/rdk/hal/hdmiinput/IHDMIInputManager.h>
#include <com/rdk/hal/hdmiinput/IHDMIInput.h>
#include <com/rdk/hal/hdmiinput/IHDMIInputController.h>
#include <com/rdk/hal/hdmiinput/IHDMIInputControllerListener.h>
#include <com/rdk/hal/hdmiinput/BnHDMIInputControllerListener.h>
#include <com/rdk/hal/hdmiinput/IHDMIInputEventListener.h>
#include <com/rdk/hal/hdmiinput/BnHDMIInputEventListener.h>
#include <com/rdk/hal/hdmiinput/Capabilities.h>
#include <com/rdk/hal/hdmiinput/PlatformCapabilities.h>
#include <com/rdk/hal/hdmiinput/State.h>
#include <com/rdk/hal/hdmiinput/SignalState.h>
#include <com/rdk/hal/hdmiinput/VIC.h>
#include <com/rdk/hal/hdmiinput/HDMIVersion.h>
#include <com/rdk/hal/hdmiinput/HDCPStatus.h>
#include <com/rdk/hal/hdmiinput/HDCPProtocolVersion.h>

// AIDL includes — planecontrol HAL (for scaleVideo, selectZoomMode, selectPort plane mapping)
#include <com/rdk/hal/planecontrol/IPlaneControl.h>
#include <com/rdk/hal/planecontrol/SourcePlaneMapping.h>
#include <com/rdk/hal/planecontrol/SourceType.h>
#include <com/rdk/hal/planecontrol/Property.h>
#include <com/rdk/hal/planecontrol/PropertyKVPair.h>
#include <com/rdk/hal/planecontrol/PlaneCapabilities.h>

// Common AIDL property value type
#include <com/rdk/hal/PropertyValue.h>

/**
 * @file hdmiIn.hpp
 * @brief Structures and classes for HDMI Input are defined here
 * @ingroup hdmiIn
 */

static const int8_t  HDMI_IN_PORT_NONE           = -1;
// Primary video plane index used when routing HDMI input to the display via IPlaneControl
static const int32_t HDMI_IN_PRIMARY_PLANE_INDEX = 0;

namespace device
{

// Forward declarations — listener implementation classes are defined in hdmiIn.cpp
class HdmiInputControllerListenerImpl;
class HdmiInputEventListenerImpl;


/**
 * @class HdmiInput
 * @brief This class manages HDMI Input
 */
class HdmiInput  
{

public:
    static HdmiInput & getInstance();

    uint8_t getNumberOfInputs        () const;
    bool    isPresented              () const;
    bool    isActivePort             (int8_t Port) const;
    int8_t  getActivePort            () const;
    bool    isPortConnected          (int8_t Port) const;
    void    selectPort               (int8_t Port, bool requestAudioMix = false, int videoPlaneType = dsVideoPlane_PRIMARY, bool topMost = false) const;
    void    scaleVideo               (int32_t x, int32_t y, int32_t width, int32_t height) const;
    void    selectZoomMode           (int8_t zoomMode) const;
    void    pauseAudio               () const;
    void    resumeAudio              () const;
    std::string  getCurrentVideoMode () const;
    void getCurrentVideoModeObj (dsVideoPortResolution_t& resolution);
    void getEDIDBytesInfo (int iHdmiPort, std::vector<uint8_t> &edid) const;
    void getHDMISPDInfo (int iHdmiPort, std::vector<uint8_t> &data);
    void setEdidVersion (int iHdmiPort, int iEdidVersion);
    void getEdidVersion (int iHdmiPort, int *iEdidVersion);
    void getHdmiALLMStatus (int iHdmiPort, bool *allmStatus);
    void getSupportedGameFeatures (std::vector<std::string> &featureList);
    void getAVLatency(int *audio_latency,int *video_latency);
    void setEdid2AllmSupport(int iHdmiPort,bool allm_suppport);
    void getEdid2AllmSupport(int iHdmiPort, bool *allm_support);
    void setVRRSupport (int iHdmiPort, bool vrr_suppport);
    void getVRRSupport (int iHdmiPort, bool *vrr_suppport);
    void getVRRStatus (int iHdmiPort, dsHdmiInVrrStatus_t *vrrStatus);
    void getHdmiVersion (int iHdmiPort, dsHdmiMaxCapabilityVersion_t *capversion);
    dsError_t getHDMIARCPortId(int &portId);
private:
    HdmiInput();           /* default constructor — runs AIDL init sequence */
    virtual ~HdmiInput();  /* destructor — runs AIDL term sequence */

    // -------------------------------------------------------
    // Per-port AIDL context
    // -------------------------------------------------------
    struct PortContext {
        android::sp<::com::rdk::hal::hdmiinput::IHDMIInput>                      hdmiInput;
        android::sp<::com::rdk::hal::hdmiinput::IHDMIInputController>            controller;
        android::sp<::com::rdk::hal::hdmiinput::IHDMIInputControllerListener>    controllerListener;
        android::sp<::com::rdk::hal::hdmiinput::IHDMIInputEventListener>         eventListener;
        bool   isOpen{false};
        bool   isStarted{false};
        // State cached from AIDL controller listener callbacks
        bool   connectionState{false};
        int    signalState{-1};    // SignalState as int; -1 = UNKNOWN
        int    lastVIC{0};         // VIC as int; 0 = VIC0_UNAVAILABLE
        bool   vrrActive{false};
        double vrrFrameRate{0.0};
        // Cached EDID version last set via setEdidVersion() (tv_hdmi_edid_version_t as int)
        int    lastSetEdidVersion{0};
    };

    // -------------------------------------------------------
    // AIDL service lazy-init helpers
    // -------------------------------------------------------
    android::sp<::com::rdk::hal::hdmiinput::IHDMIInputManager> getHdmiInputManager();
    void initHdmiInputManager();
    android::sp<::com::rdk::hal::planecontrol::IPlaneControl>  getPlaneControl();
    void initPlaneControl();

    // -------------------------------------------------------
    // Per-port cache-update helpers — called by listener impls
    // -------------------------------------------------------
    void onPortConnectionChanged(int portId, bool connected);
    void onPortSignalChanged(int portId, int signalState);
    void onPortVICChanged(int portId, int vic);
    void onPortVRRChanged(int portId, bool vrrActive, double frameRate);

    // -------------------------------------------------------
    // AIDL service handles (lazy-init, protected by mAidlMutex)
    // -------------------------------------------------------
    android::sp<::com::rdk::hal::hdmiinput::IHDMIInputManager> mHdmiInputManager;
    android::sp<::com::rdk::hal::planecontrol::IPlaneControl>  mPlaneControl;

    // Per-port context keyed by HAL port index
    std::map<int, PortContext> mPortContexts;
    mutable std::mutex         mAidlMutex;

    // Currently active (started) port; -1 = none
    int mActivePortId{-1};

    // Listener implementation classes access private helpers
    friend class HdmiInputControllerListenerImpl;
    friend class HdmiInputEventListenerImpl;
};


}   /* namespace device */


#endif /* _DS_HDMIIN_HPP_ */


/** @} */
/** @} */

