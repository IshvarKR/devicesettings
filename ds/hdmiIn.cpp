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
 * @file hdmiIn.cpp
 * @brief Configuration of HDMI Input
 */
 


/**
* @defgroup devicesettings
* @{
* @defgroup ds
* @{
**/


#include <iostream>
#include <sstream>
#include <string>
#include <string.h>
#include <mutex>
#include <map>
#include "hdmiIn.hpp"
#include "illegalArgumentException.hpp"
#include "host.hpp"

#include "dslogger.h"
#include "dsError.h"
#include "dsTypes.h"
// dsHdmiIn.h retained for type definitions (dsHdmiInPort_t, dsHdmiInVrrStatus_t,
// dsHdmiMaxCapabilityVersion_t, etc.) pulled in via its included type headers.
// No HAL function calls remain in this AIDL prototype.
#include "dsHdmiIn.h"
#include "dsUtl.h"
#include "edid-parser.hpp"
#include "dsInternal.h"

using android::sp;
using android::defaultServiceManager;
using android::interface_cast;
using android::String16;
using android::ProcessState;
using namespace com::rdk::hal::hdmiinput;
using namespace com::rdk::hal::planecontrol;

namespace device
{

// ============================================================
// AIDL Listener Implementations (file-local)
// ============================================================

/**
 * Handles per-port runtime callbacks from IHDMIInputController:
 * connection state, signal state, VIC, VRR, and InfoFrame events.
[O */
class HdmiInputControllerListenerImpl
    : public ::com::rdk::hal::hdmiinput::BnHDMIInputControllerListener
{
public:
    HdmiInputControllerListenerImpl(HdmiInput* owner, int portId)
        : mOwner(owner), mPortId(portId) {}

    ::android::binder::Status onConnectionStateChanged(bool connectionState) override {
        mOwner->onPortConnectionChanged(mPortId, connectionState);
        return ::android::binder::Status::ok();
    }
    ::android::binder::Status onSignalStateChanged(
            ::com::rdk::hal::hdmiinput::SignalState signalState) override {
        mOwner->onPortSignalChanged(mPortId, static_cast<int>(signalState));
        return ::android::binder::Status::ok();
    }
    // Maps to dsHdmiInRegisterVideoModeUpdateCB — VIC cached for getCurrentVideoMode()
    ::android::binder::Status onVIChanged(
            ::com::rdk::hal::hdmiinput::VIC vic) override {
        mOwner->onPortVICChanged(mPortId, static_cast<int>(vic));
        return ::android::binder::Status::ok();
    }
    // Maps to dsHdmiInRegisterVRRChangeCB — data cached for getVRRStatus()
    ::android::binder::Status onVRRChanged(
            bool vrrActive, bool /*M_CONST*/, bool /*fastVActive*/, double frameRate) override {
        mOwner->onPortVRRChanged(mPortId, vrrActive, frameRate);
        return ::android::binder::Status::ok();
    }
    // InfoFrame callbacks — no direct dsHdmiIn HAL equivalent; stubbed for prototype
    ::android::binder::Status onAVIInfoFrame(
            const ::std::vector<uint8_t>& /*data*/) override {
        return ::android::binder::Status::ok();
    }
    ::android::binder::Status onAudioInfoFrame(
            const ::std::vector<uint8_t>& /*data*/) override {
        return ::android::binder::Status::ok();
    }
    ::android::binder::Status onSPDInfoFrame(
            const ::std::vector<uint8_t>& /*data*/) override {
        return ::android::binder::Status::ok();
    }
    ::android::binder::Status onDRMInfoFrame(
            const ::std::vector<uint8_t>& /*data*/) override {
        return ::android::binder::Status::ok();
    }
    ::android::binder::Status onVendorSpecificInfoFrame(
            const ::std::vector<uint8_t>& /*data*/) override {
        return ::android::binder::Status::ok();
    }
    ::android::binder::Status onHDCPStatusChanged(
            ::com::rdk::hal::hdmiinput::HDCPStatus /*hdcpStatus*/,
            ::com::rdk::hal::hdmiinput::HDCPProtocolVersion /*hdcpProtocolVersion*/) override {
        return ::android::binder::Status::ok();
    }
private:
    HdmiInput* mOwner;
    int        mPortId;
};

/**
 * Handles port-level state and EDID change events from IHDMIInput
 * (maps to dsHdmiInRegisterStatusChangeCB).
 */
class HdmiInputEventListenerImpl
    : public ::com::rdk::hal::hdmiinput::BnHDMIInputEventListener
{
public:
    HdmiInputEventListenerImpl(HdmiInput* owner, int portId)
        : mOwner(owner), mPortId(portId) {}

    ::android::binder::Status onStateChanged(
            ::com::rdk::hal::hdmiinput::State /*oldState*/,
            ::com::rdk::hal::hdmiinput::State /*newState*/) override {
        return ::android::binder::Status::ok();
    }
    ::android::binder::Status onEDIDChange(
            const ::std::vector<uint8_t>& /*edid*/) override {
        return ::android::binder::Status::ok();
    }
private:
    HdmiInput* mOwner;
    int        mPortId;
};

// ============================================================
// AIDL service init helpers
// ============================================================

void HdmiInput::initHdmiInputManager()
{
    ProcessState::self()->startThreadPool();
    sp<android::IServiceManager> sm = defaultServiceManager();
    if (!sm) {
        INT_ERROR("HdmiInput: failed to get IServiceManager");
        return;
    }
    mHdmiInputManager = interface_cast<IHDMIInputManager>(
        sm->getService(String16(IHDMIInputManager::serviceName().c_str())));
    if (!mHdmiInputManager) {
        INT_ERROR("HdmiInput: failed to acquire IHDMIInputManager service");
    }
}

sp<IHDMIInputManager> HdmiInput::getHdmiInputManager()
{
    std::lock_guard<std::mutex> lock(mAidlMutex);
    if (!mHdmiInputManager) {
        initHdmiInputManager();
    }
    return mHdmiInputManager;
}

void HdmiInput::initPlaneControl()
{
    sp<android::IServiceManager> sm = defaultServiceManager();
    if (!sm) {
        INT_ERROR("HdmiInput: failed to get IServiceManager for IPlaneControl");
        return;
    }
    mPlaneControl = interface_cast<IPlaneControl>(
        sm->getService(String16(IPlaneControl::serviceName().c_str())));
    if (!mPlaneControl) {
        INT_ERROR("HdmiInput: failed to acquire IPlaneControl service");
    }
}

sp<IPlaneControl> HdmiInput::getPlaneControl()
{
    std::lock_guard<std::mutex> lock(mAidlMutex);
    if (!mPlaneControl) {
        initPlaneControl();
    }
    return mPlaneControl;
}

// ============================================================
// Per-port cache-update helpers — called from listener callbacks
// ============================================================

void HdmiInput::onPortConnectionChanged(int portId, bool connected)
{
    std::lock_guard<std::mutex> lock(mAidlMutex);
    auto it = mPortContexts.find(portId);
    if (it != mPortContexts.end()) it->second.connectionState = connected;
}

void HdmiInput::onPortSignalChanged(int portId, int signalState)
{
    std::lock_guard<std::mutex> lock(mAidlMutex);
    auto it = mPortContexts.find(portId);
    if (it != mPortContexts.end()) it->second.signalState = signalState;
}

void HdmiInput::onPortVICChanged(int portId, int vic)
{
    std::lock_guard<std::mutex> lock(mAidlMutex);
    auto it = mPortContexts.find(portId);
    if (it != mPortContexts.end()) it->second.lastVIC = vic;
}

void HdmiInput::onPortVRRChanged(int portId, bool vrrActive, double frameRate)
{
    std::lock_guard<std::mutex> lock(mAidlMutex);
    auto it = mPortContexts.find(portId);
    if (it != mPortContexts.end()) {
        it->second.vrrActive    = vrrActive;
        it->second.vrrFrameRate = frameRate;
    }
}

// ============================================================
// VIC-to-resolution helpers (minimal set for prototype)
// ============================================================

static std::string vicToResolutionString(int vic)
{
    switch (vic) {
        case 1: case 2: case 3: return "480p59.94";
        case 4:                 return "720p59.94";
        case 5:                 return "1080i59.94";
        case 16:                return "1080p59.94";
        case 17: case 18:       return "576p50";
        case 19:                return "720p50";
        case 20:                return "1080i50";
        case 31:                return "1080p50";
        case 32:                return "1080p23.98";
        case 33:                return "1080p25";
        case 34:                return "1080p29.97";
        case 93:                return "3840x2160p23.98";
        case 94:                return "3840x2160p25";
        case 95:                return "3840x2160p29.97";
        case 96:                return "3840x2160p50";
        case 97:                return "3840x2160p59.94";
        default:                return "unknown";
    }
}

static void vicToResolutionObj(int vic, dsVideoPortResolution_t& res)
{
    memset(&res, 0, sizeof(res));
    switch (vic) {
        case 1: case 2: case 3:
            res.pixelResolution = dsVIDEO_PIXELRES_720x480;
            res.frameRate       = dsVIDEO_FRAMERATE_59dot94; break;
        case 4:
            res.pixelResolution = dsVIDEO_PIXELRES_1280x720;
            res.frameRate       = dsVIDEO_FRAMERATE_59dot94; break;
        case 5:
            res.pixelResolution = dsVIDEO_PIXELRES_1920x1080;
            res.interlaced      = true;
            res.frameRate       = dsVIDEO_FRAMERATE_59dot94; break;
        case 16:
            res.pixelResolution = dsVIDEO_PIXELRES_1920x1080;
            res.frameRate       = dsVIDEO_FRAMERATE_59dot94; break;
        case 17: case 18:
            res.pixelResolution = dsVIDEO_PIXELRES_720x576;
            res.frameRate       = dsVIDEO_FRAMERATE_50; break;
        case 19:
            res.pixelResolution = dsVIDEO_PIXELRES_1280x720;
            res.frameRate       = dsVIDEO_FRAMERATE_50; break;
        case 31:
            res.pixelResolution = dsVIDEO_PIXELRES_1920x1080;
            res.frameRate       = dsVIDEO_FRAMERATE_50; break;
        case 93: case 94: case 95: case 96: case 97:
            res.pixelResolution = dsVIDEO_PIXELRES_3840x2160;
            res.frameRate       = (vic >= 96) ? dsVIDEO_FRAMERATE_50 : dsVIDEO_FRAMERATE_25; break;
        default:
            res.pixelResolution = dsVIDEO_PIXELRES_1920x1080;
            res.frameRate       = dsVIDEO_FRAMERATE_60; break;
    }
}


/**
 * @fn  HdmiInput::HdmiInput()
 * @brief default constructor
 *
 * @param None
 *
 * @return None
 * @callergraph
 */
/**
 * Constructor — replaces dsHdmiInInit().
 * AIDL init sequence:
 *   1. IHDMIInputManager.getHDMIInputIds()                 — enumerate port IDs
 *   2. IHDMIInputManager.getHDMIInput(Id)                  — get per-port IHDMIInput
 *   3. IHDMIInput.registerEventListener(eventListener)     — state/EDID change events
 *   4. IHDMIInput.open(controllerListener)                 — obtain IHDMIInputController
 *      (port moves CLOSED → OPENING → READY; onConnectionStateChanged fires immediately)
 */
HdmiInput::HdmiInput()
    : mActivePortId(-1)
{
    sp<IHDMIInputManager> manager = getHdmiInputManager();
    if (!manager) {
        INT_ERROR("HdmiInput: IHDMIInputManager unavailable — AIDL init incomplete");
        return;
    }

    std::vector<IHDMIInput::Id> portIds;
    ::android::binder::Status st = manager->getHDMIInputIds(&portIds);
    if (!st.isOk()) {
        INT_ERROR("HdmiInput: getHDMIInputIds failed: %s", st.toString8().c_str());
        return;
    }

    for (const auto& id : portIds) {
        PortContext ctx;

        sp<IHDMIInput> hdmiInput;
        st = manager->getHDMIInput(id, &hdmiInput);
        if (!st.isOk() || !hdmiInput) {
            INT_ERROR("HdmiInput: getHDMIInput failed for port id=%d", id.value);
            continue;
        }
        ctx.hdmiInput = hdmiInput;

        // Register event listener (state + EDID change notifications)
        ctx.eventListener = new HdmiInputEventListenerImpl(this, id.value);
        bool regOk = false;
        st = hdmiInput->registerEventListener(ctx.eventListener, &regOk);
        if (!st.isOk() || !regOk) {
            INT_ERROR("HdmiInput: registerEventListener failed for port %d", id.value);
        }

        // Open the port to obtain the controller
        // onConnectionStateChanged fires during the OPENING transition
        ctx.controllerListener = new HdmiInputControllerListenerImpl(this, id.value);
        sp<IHDMIInputController> controller;
        st = hdmiInput->open(ctx.controllerListener, &controller);
        if (!st.isOk() || !controller) {
            INT_ERROR("HdmiInput: open() failed for port %d: %s",
                      id.value, st.toString8().c_str());
            bool dummy = false;
            hdmiInput->unregisterEventListener(ctx.eventListener, &dummy);
            continue;
        }
        ctx.controller = controller;
        ctx.isOpen     = true;

        std::lock_guard<std::mutex> lock(mAidlMutex);
        mPortContexts[id.value] = std::move(ctx);
        INT_INFO("HdmiInput: port %d opened (READY state)", id.value);
    }
}

/**
 * @fn  HdmiInput::~HdmiInput()
 * @brief destructor
 *
 * @param None
 *
 * @return None
 * @callergraph
 */
/**
 * Destructor — replaces dsHdmiInTerm().
 * AIDL term sequence per port:
 *   1. IHDMIInputController.stop()                        — STARTED → STOPPING → READY
 *   2. IHDMIInput.close(controller)                       — READY → CLOSING → CLOSED
 *   3. IHDMIInput.unregisterEventListener(eventListener)
 */
HdmiInput::~HdmiInput()
{
    std::lock_guard<std::mutex> lock(mAidlMutex);
    for (auto& kv : mPortContexts) {
        PortContext& ctx = kv.second;
        if (ctx.isStarted && ctx.controller) {
            ctx.controller->stop();
            ctx.isStarted = false;
        }
        if (ctx.isOpen && ctx.hdmiInput && ctx.controller) {
            bool closeOk = false;
            ctx.hdmiInput->close(ctx.controller, &closeOk);
            ctx.isOpen = false;
        }
        if (ctx.hdmiInput && ctx.eventListener) {
            bool dummy = false;
            ctx.hdmiInput->unregisterEventListener(ctx.eventListener, &dummy);
        }
        ctx.controller         = nullptr;
        ctx.hdmiInput          = nullptr;
        ctx.eventListener      = nullptr;
        ctx.controllerListener = nullptr;
    }
    mPortContexts.clear();
}

/**
 * @fn  HdmiInput::getInstance()
 * @brief This API is used to get the instance of the HDMI Input
 *
 * @param None
 *
 * @return Reference to the instance of HDMI Input class instance
 * @callergraph
 */
HdmiInput & HdmiInput::getInstance()
{
    static HdmiInput _singleton;
    return _singleton;
}

/**
 * @fn  HdmiInput::getNumberOfInputs()
 * @brief This API is used to get the number of HDMI Input ports on the set-top
 *
 * @param[in] None
 *
 * @return number of HDMI Inputs
 * @callergraph
 */
/**
 * Replaces dsHdmiInGetNumberOfInputs().
 * AIDL: IHDMIInputManager.getHDMIInputIds().size()
 */
uint8_t HdmiInput::getNumberOfInputs() const
{
    sp<IHDMIInputManager> manager =
        const_cast<HdmiInput*>(this)->getHdmiInputManager();
    if (!manager) throw Exception(dsERR_GENERAL);

    std::vector<IHDMIInput::Id> portIds;
    ::android::binder::Status st = manager->getHDMIInputIds(&portIds);
    if (!st.isOk()) throw Exception(dsERR_GENERAL);

    return static_cast<uint8_t>(portIds.size());
}

/**
 * @fn  HdmiInput::isPresented()
 * @brief This API is used to specify if HDMI Input is being
 *        presented via HDMI Out
 *
 * @param[in] None
 *
 * @return true if HDMI Input is being presetned.
 * @callergraph
 */
/**
 * Replaces dsHdmiInGetStatus().isPresented.
 * AIDL: true if any port is in STARTED state and has a live device connection.
 */
bool HdmiInput::isPresented() const
{
    std::lock_guard<std::mutex> lock(mAidlMutex);
    for (const auto& kv : mPortContexts) {
        if (kv.second.isStarted && kv.second.connectionState) return true;
    }
    return false;
}

/**
 * @fn  HdmiInput::isActivePort()
 * @brief This API is used to specify if the provided HDMI Input port is
 *        active (i.e. communicating with the set-top)
 *
 * @param[in] HDMI Input port
 *
 * @return true if the provided HDMI Input port is active.
 * @callergraph
 */
/**
 * Replaces dsHdmiInGetStatus().activePort == Port.
 * AIDL: compares against mActivePortId set during selectPort().
 */
bool HdmiInput::isActivePort(int8_t Port) const
{
    std::lock_guard<std::mutex> lock(mAidlMutex);
    return mActivePortId == static_cast<int>(Port);
}

/**
 * @fn  HdmiInput::getActivePort()
 * @brief This API is used to specify the active (i.e. communicating with
 *        the set-top) HDMI Input port
 *
 * @param[in] None
 *
 * @return the HDMI Input port which is currently active.
 * @callergraph
 */
/**
 * Replaces dsHdmiInGetStatus().activePort.
 * AIDL: returns mActivePortId set during selectPort().
 */
int8_t HdmiInput::getActivePort() const
{
    std::lock_guard<std::mutex> lock(mAidlMutex);
    return static_cast<int8_t>(mActivePortId);
}

/**
 * @fn  HdmiInput::isPortConnected()
 * @brief This API is used to specify if the prvided HDMI Input port is
 *        connected (i.e. HDMI Input devie is plugged into the set-top).
 *
 * @param[in] HDMI Input port
 *
 * @return true if the HDMI Input port is connected
 * @callergraph
 */
/**
 * Replaces dsHdmiInGetStatus().isPortConnected[Port].
 * AIDL: uses connectionState cached from onConnectionStateChanged().
 */
bool HdmiInput::isPortConnected(int8_t Port) const
{
    std::lock_guard<std::mutex> lock(mAidlMutex);
    auto it = mPortContexts.find(static_cast<int>(Port));
    if (it != mPortContexts.end()) return it->second.connectionState;
    return false;
}

/**
 * @fn  HdmiInput::selectPort()
 * @brief This API is used to select the HDMI In port to be presented
 *
 * @param[in] int8_t Port : -1 for No HDMI Input port to be presented
 *                           0..n for HDMI Input port (n) to be presented 
 *
 * @return None
 * @callergraph
 */
/**
 * Replaces dsHdmiInSelectPort().
 * AIDL sequence:
 *   1. Stop previously active port (if different) via IHDMIInputController.stop()
 *   2. Start the requested port via IHDMIInputController.start()
 *   3. Map port to video plane via IPlaneControl.setVideoSourceDestinationPlaneMapping()
 * Gap: requestAudioMix audio routing via IAudioMixerController is deferred for this prototype.
 */
void HdmiInput::selectPort (int8_t Port, bool requestAudioMix, int videoPlaneType, bool topMost) const
{
    if (requestAudioMix) {
        INT_INFO("HdmiInput::selectPort: requestAudioMix=true — IAudioMixerController routing not yet implemented in prototype");
    }

    // Port == -1 means deselect
    if (Port < 0) {
        std::lock_guard<std::mutex> lock(mAidlMutex);
        mActivePortId = -1;
        return;
    }

    // --- Step 1: Stop any previously active port (if different) ---
    {
        std::lock_guard<std::mutex> lock(mAidlMutex);
        if (mActivePortId >= 0 && mActivePortId != static_cast<int>(Port)) {
            auto prevIt = mPortContexts.find(mActivePortId);
            if (prevIt != mPortContexts.end() && prevIt->second.isStarted && prevIt->second.controller) {
                prevIt->second.controller->stop();
                prevIt->second.isStarted = false;
                INT_INFO("HdmiInput::selectPort: stopped previously active port %d", mActivePortId);
            }
        }
    }

    // --- Step 2: Start the requested port ---
    sp<IHDMIInputController> controller;
    bool needStart = false;
    {
        std::lock_guard<std::mutex> lock(mAidlMutex);
        auto it = mPortContexts.find(static_cast<int>(Port));
        if (it == mPortContexts.end() || !it->second.controller) {
            INT_ERROR("HdmiInput::selectPort: port %d not available", Port);
            throw Exception(dsERR_INVALID_PARAM);
        }
        controller = it->second.controller;
        needStart  = !it->second.isStarted;
        mActivePortId = static_cast<int>(Port);
    }

    if (needStart) {
        ::android::binder::Status st = controller->start();
        if (!st.isOk()) {
            INT_ERROR("HdmiInput::selectPort: start() failed for port %d: %s",
                      Port, st.toString8().c_str());
            throw Exception(dsERR_GENERAL);
        }
        std::lock_guard<std::mutex> lock(mAidlMutex);
        auto it = mPortContexts.find(static_cast<int>(Port));
        if (it != mPortContexts.end()) it->second.isStarted = true;
        INT_INFO("HdmiInput::selectPort: port %d started", Port);
    }

    // --- Step 3: Map HDMI port to video plane via IPlaneControl ---
    sp<IPlaneControl> planeCtrl = const_cast<HdmiInput*>(this)->getPlaneControl();
    if (!planeCtrl) {
        INT_ERROR("HdmiInput::selectPort: IPlaneControl unavailable");
        throw Exception(dsERR_GENERAL);
    }

    SourcePlaneMapping mapping;
    mapping.sourceType            = SourceType::HDMI;
    mapping.sourceIndex           = static_cast<int>(Port);
    mapping.destinationPlaneIndex = videoPlaneType; // PRIMARY=0, SECONDARY=1

    std::vector<SourcePlaneMapping> mappings = { mapping };
    bool mapResult = false;
    ::android::binder::Status st = planeCtrl->setVideoSourceDestinationPlaneMapping(mappings, &mapResult);
    if (!st.isOk() || !mapResult) {
        INT_ERROR("HdmiInput::selectPort: setVideoSourceDestinationPlaneMapping failed for port %d", Port);
        throw Exception(dsERR_GENERAL);
    }
    INT_INFO("HdmiInput::selectPort: port %d mapped to plane %d topMost=%d",
             Port, videoPlaneType, topMost);
}


/**
 * @fn  HdmiInput::scaleVideo()
 * @brief This API is used to scale the HDMI In video
 *
 * @param[in] int32_t x      : x coordinate for the video
 * @param[in] int32_t y      : y coordinate for the video
 * @param[in] int32_t width  : width of the video
 * @param[in] int32_t height : height of the video
 *
 * @return None
 * @callergraph
 */
/**
 * Replaces dsHdmiInScaleVideo().
 * AIDL: IPlaneControl.setPropertyMultiAtomic(primaryPlaneIndex, [X, Y, WIDTH, HEIGHT])
 */
void HdmiInput::scaleVideo (int32_t x, int32_t y, int32_t width, int32_t height) const
{
    sp<IPlaneControl> planeCtrl = const_cast<HdmiInput*>(this)->getPlaneControl();
    if (!planeCtrl) {
        INT_ERROR("HdmiInput::scaleVideo: IPlaneControl unavailable");
        throw Exception(dsERR_GENERAL);
    }

    auto makeKV = [](::com::rdk::hal::planecontrol::Property prop, int32_t val) {
        ::com::rdk::hal::planecontrol::PropertyKVPair kv;
        kv.property = prop;
        kv.propertyValue.set<::com::rdk::hal::PropertyValue::Tag::intValue>(val);
        return kv;
    };

    std::vector<::com::rdk::hal::planecontrol::PropertyKVPair> kvList = {
        makeKV(::com::rdk::hal::planecontrol::Property::X,      x),
        makeKV(::com::rdk::hal::planecontrol::Property::Y,      y),
        makeKV(::com::rdk::hal::planecontrol::Property::WIDTH,  width),
        makeKV(::com::rdk::hal::planecontrol::Property::HEIGHT, height),
    };

    bool result = false;
    ::android::binder::Status st = planeCtrl->setPropertyMultiAtomic(
        HDMI_IN_PRIMARY_PLANE_INDEX, kvList, &result);
    if (!st.isOk() || !result) {
        INT_ERROR("HdmiInput::scaleVideo: setPropertyMultiAtomic failed");
        throw Exception(dsERR_GENERAL);
    }
    INT_INFO("HdmiInput::scaleVideo: x=%d y=%d w=%d h=%d applied", x, y, width, height);
}

/**
 * @fn  HdmiInput::selectZoomMode()
 * @brief This API is used to select the HDMI In video zoom mode
 *
 * @param[in] int8_t zoomMoode : 0 for NONE
 *                               1 for FULL
 *
 * @return None
 * @callergraph
 */
/**
 * Replaces dsHdmiInSelectZoomMode().
 * AIDL: IPlaneControl.setProperty(primaryPlaneIndex, ASPECT_RATIO, zoomMode)
[I */
void HdmiInput::selectZoomMode (int8_t zoomMode) const
{
    sp<IPlaneControl> planeCtrl = const_cast<HdmiInput*>(this)->getPlaneControl();
    if (!planeCtrl) {
        INT_ERROR("HdmiInput::selectZoomMode: IPlaneControl unavailable");
        throw Exception(dsERR_GENERAL);
    }

    ::com::rdk::hal::PropertyValue aspectRatioVal;
    aspectRatioVal.set<::com::rdk::hal::PropertyValue::Tag::intValue>(
        static_cast<int32_t>(zoomMode));

    bool result = false;
    ::android::binder::Status st = planeCtrl->setProperty(
        HDMI_IN_PRIMARY_PLANE_INDEX,
        ::com::rdk::hal::planecontrol::Property::ASPECT_RATIO,
        aspectRatioVal,
        &result);
    if (!st.isOk() || !result) {
        INT_ERROR("HdmiInput::selectZoomMode: setProperty(ASPECT_RATIO) failed");
        throw Exception(dsERR_GENERAL);
    }
    INT_INFO("HdmiInput::selectZoomMode: zoomMode=%d applied", zoomMode);
}

static std::string getResolutionStr (dsVideoResolution_t resolution)
{
    std::string resolutionStr;

    switch (resolution)
    {
        case dsVIDEO_PIXELRES_720x480:
            resolutionStr = "480";
            break;

        case dsVIDEO_PIXELRES_720x576:
            resolutionStr = "576";
            break;

        case dsVIDEO_PIXELRES_1280x720:
            resolutionStr = "720";
            break;

        case dsVIDEO_PIXELRES_1366x768:
            resolutionStr = "1366x768";
            break;

        case dsVIDEO_PIXELRES_1920x1080:
            resolutionStr = "1080";
            break;

        case dsVIDEO_PIXELRES_3840x2160:
            resolutionStr = "3840x2160";
            break;

        case dsVIDEO_PIXELRES_4096x2160:
            resolutionStr = "4096x2160";
            break;

        default:
            resolutionStr = "unknown";
            break;
    }

    INT_INFO("ResolutionStr: %s", resolutionStr.c_str());
    return resolutionStr;
}

static std::string getFrameRateStr (dsVideoFrameRate_t frameRate)
{
    std::string FrameRateStr;

    switch (frameRate)
    {
        case dsVIDEO_FRAMERATE_24:
            FrameRateStr = "24";
            break;

        case dsVIDEO_FRAMERATE_25:
            FrameRateStr = "25";
            break;

        case dsVIDEO_FRAMERATE_30:
            FrameRateStr = "30";
            break;

        case dsVIDEO_FRAMERATE_60:
            FrameRateStr = "60";
            break;

        case dsVIDEO_FRAMERATE_23dot98:
            FrameRateStr = "23.98";
            break;

        case dsVIDEO_FRAMERATE_29dot97:
            FrameRateStr = "29.97";
            break;

        case dsVIDEO_FRAMERATE_50:
            FrameRateStr = "50";
            break;

        case dsVIDEO_FRAMERATE_59dot94:
            FrameRateStr = "59.94";
            break;

        case dsVIDEO_FRAMERATE_100:
            FrameRateStr = "100";
            break;

        case dsVIDEO_FRAMERATE_119dot88:
            FrameRateStr = "119.88";
            break;

        case dsVIDEO_FRAMERATE_120:
            FrameRateStr = "120";
            break;

        case dsVIDEO_FRAMERATE_200:
            FrameRateStr = "200";
            break;

        case dsVIDEO_FRAMERATE_239dot76:
            FrameRateStr = "239.76";
            break;

        case dsVIDEO_FRAMERATE_240:
            FrameRateStr = "240";
            break;

         default:
            // Not all video formats have a specified framerate.
            break;
    }

    INT_INFO("FrameRateStr: %s", FrameRateStr.c_str());
    return FrameRateStr;
}

static std::string getInterlacedStr (bool interlaced)
{
    std::string InterlacedStr = (interlaced) ? "i" : "p";
    INT_INFO("InterlacedStr: %s", InterlacedStr.c_str());
    return InterlacedStr;
}

static std::string CreateResolutionStr (const dsVideoPortResolution_t &resolution)
{
    INT_INFO("--->");

    std::string resolutionStr = getResolutionStr(resolution.pixelResolution);
    if(resolutionStr.compare("unknown") != 0){
    	resolutionStr = getResolutionStr(resolution.pixelResolution) +
                                getInterlacedStr(resolution.interlaced) +
                                getFrameRateStr(resolution.frameRate);
    }
    INT_INFO("<--- %s", resolutionStr.c_str());
    return resolutionStr;
}

/**
 * @fn  HdmiInput::getCurrentVideoMode()
 * @brief This API is used to get the current HDMI In video mode (resolution)
 *
 * @param[in] None
 *
 * @return HDMI Input video resolution string
 * @callergraph
 */
/**
 * Replaces dsHdmiInGetCurrentVideoMode().
 * AIDL: VIC cached from IHDMIInputControllerListener.onVIChanged() → vicToResolutionString()
 */
std::string HdmiInput::getCurrentVideoMode () const
{
    int vic = 0;
    {
        std::lock_guard<std::mutex> lock(mAidlMutex);
        if (mActivePortId >= 0) {
            auto it = mPortContexts.find(mActivePortId);
            if (it != mPortContexts.end()) vic = it->second.lastVIC;
        }
    }
    std::string resStr = vicToResolutionString(vic);
    INT_INFO("HdmiInput::getCurrentVideoMode: VIC=%d resolution=%s", vic, resStr.c_str());
    return resStr;
}

/**
 * Replaces dsHdmiInGetCurrentVideoMode() (struct variant).
 * AIDL: VIC cached from onVIChanged() → vicToResolutionObj()
 */
void HdmiInput::getCurrentVideoModeObj (dsVideoPortResolution_t& resolution)
{
    int vic = 0;
    {
        std::lock_guard<std::mutex> lock(mAidlMutex);
        if (mActivePortId >= 0) {
            auto it = mPortContexts.find(mActivePortId);
            if (it != mPortContexts.end()) vic = it->second.lastVIC;
        }
    }
    vicToResolutionObj(vic, resolution);
    INT_INFO("HdmiInput::getCurrentVideoModeObj: VIC=%d pixelRes=%d interlaced=%d frameRate=%d",
             vic, resolution.pixelResolution, resolution.interlaced, resolution.frameRate);
}

/**
 * Replaces dsGetEDIDBytesInfo().
 * AIDL: IHDMIInput.getEDID() → verify via edid_parser
 */
void HdmiInput::getEDIDBytesInfo (int iHdmiPort, std::vector<uint8_t> &edidArg) const
{
    INT_INFO("HdmiInput::getEDIDBytesInfo port=%d", iHdmiPort);

    sp<IHDMIInput> hdmiInput;
    {
        std::lock_guard<std::mutex> lock(mAidlMutex);
        auto it = mPortContexts.find(iHdmiPort);
        if (it == mPortContexts.end() || !it->second.hdmiInput) {
            throw Exception(dsERR_INVALID_PARAM, "port not found");
        }
        hdmiInput = it->second.hdmiInput;
    }

    std::vector<uint8_t> edid;
    bool ok = false;
    ::android::binder::Status st = hdmiInput->getEDID(&edid, &ok);
    if (!st.isOk() || !ok || edid.empty()) {
        INT_ERROR("HdmiInput::getEDIDBytesInfo: getEDID AIDL call failed for port %d", iHdmiPort);
        throw Exception(dsERR_GENERAL, "getEDID failed");
    }

    if (edid.size() > MAX_EDID_BYTES_LEN) {
        throw Exception(dsERR_OPERATION_NOT_SUPPORTED, "EDID length > MAX_EDID_BYTES_LEN");
    }
    if (edid_parser::EDID_STATUS_OK != edid_parser::EDID_Verify(edid.data(), edid.size())) {
        throw Exception(dsERR_GENERAL, "EDID verification failed");
    }

    edidArg = edid;
    INT_INFO("HdmiInput::getEDIDBytesInfo: port %d got %zu bytes", iHdmiPort, edid.size());
}

/**
 * Replaces dsGetHDMISPDInfo().
 * AIDL: IHDMIInput.getSPDInfoFrame() → raw bytes vector
 */
void HdmiInput::getHDMISPDInfo (int iHdmiPort, std::vector<uint8_t> &data) {
    INT_INFO("HdmiInput::getHDMISPDInfo port=%d", iHdmiPort);

    sp<IHDMIInput> hdmiInput;
    {
        std::lock_guard<std::mutex> lock(mAidlMutex);
        auto it = mPortContexts.find(iHdmiPort);
        if (it == mPortContexts.end() || !it->second.hdmiInput) {
            throw Exception(dsERR_INVALID_PARAM, "port not found");
        }
        hdmiInput = it->second.hdmiInput;
    }

    std::vector<uint8_t> spdData;
    ::android::binder::Status st = hdmiInput->getSPDInfoFrame(&spdData);
    if (!st.isOk()) {
        INT_ERROR("HdmiInput::getHDMISPDInfo: getSPDInfoFrame failed for port %d: %s",
                  iHdmiPort, st.toString8().c_str());
        throw Exception(dsERR_GENERAL, "getSPDInfoFrame failed");
    }

    data = spdData;
    INT_INFO("HdmiInput::getHDMISPDInfo: port %d got %zu bytes", iHdmiPort, data.size());
}

/**
 * Replaces dsSetEdidVersion().
 * AIDL sequence:
 *   1. IHDMIInput.getDefaultEDID(HDMIVersion) → fetch default EDID bytes for the requested version
 *   2. IHDMIInputController.setEDID(edid)     → apply the EDID on the port
 * Version mapping: tv_hdmi_edid_version_t 0=HDMI_EDID_VER_14 → HDMIVersion::HDMI_1_4
 *                                          1=HDMI_EDID_VER_20 → HDMIVersion::HDMI_2_0
 */
void HdmiInput::setEdidVersion (int iHdmiPort, int iEdidVersion) {
    INT_INFO("HdmiInput::setEdidVersion port=%d version=%d", iHdmiPort, iEdidVersion);

    sp<IHDMIInput> hdmiInput;
    sp<IHDMIInputController> controller;
    {
        std::lock_guard<std::mutex> lock(mAidlMutex);
        auto it = mPortContexts.find(iHdmiPort);
        if (it == mPortContexts.end() || !it->second.hdmiInput) {
            throw Exception(dsERR_INVALID_PARAM, "port not found");
        }
        hdmiInput  = it->second.hdmiInput;
        controller = it->second.controller;
    }
    if (!controller) throw Exception(dsERR_GENERAL, "no controller for port");

    HDMIVersion aidlVersion;
    switch (iEdidVersion) {
        case 0:  aidlVersion = HDMIVersion::HDMI_1_4; break; // HDMI_EDID_VER_14
        case 1:  aidlVersion = HDMIVersion::HDMI_2_0; break; // HDMI_EDID_VER_20
        default: aidlVersion = HDMIVersion::HDMI_2_0; break;
    }

    std::vector<uint8_t> defaultEdid;
    bool getOk = false;
    ::android::binder::Status st = hdmiInput->getDefaultEDID(aidlVersion, &defaultEdid, &getOk);
    if (!st.isOk() || !getOk || defaultEdid.empty()) {
        INT_ERROR("HdmiInput::setEdidVersion: getDefaultEDID failed for port %d version %d",
                  iHdmiPort, iEdidVersion);
        throw Exception(dsERR_GENERAL, "getDefaultEDID failed");
    }

    bool setOk = false;
    st = controller->setEDID(defaultEdid, &setOk);
    if (!st.isOk() || !setOk) {
        INT_ERROR("HdmiInput::setEdidVersion: setEDID failed for port %d", iHdmiPort);
        throw Exception(dsERR_GENERAL, "setEDID failed");
    }

    // Cache the version for getEdidVersion()
    {
        std::lock_guard<std::mutex> lock(mAidlMutex);
        auto it = mPortContexts.find(iHdmiPort);
        if (it != mPortContexts.end()) it->second.lastSetEdidVersion = iEdidVersion;
    }
    INT_INFO("HdmiInput::setEdidVersion: port %d EDID set to version %d", iHdmiPort, iEdidVersion);
}

/**
 * Replaces dsGetEdidVersion().
 * AIDL: returns the version cached during the last setEdidVersion() call.
 * If no version was set explicitly, defaults to 0 (HDMI_EDID_VER_14).
 */
void HdmiInput::getEdidVersion (int iHdmiPort, int *iEdidVersion) {
    INT_INFO("HdmiInput::getEdidVersion port=%d", iHdmiPort);
    {
        std::lock_guard<std::mutex> lock(mAidlMutex);
        auto it = mPortContexts.find(iHdmiPort);
        if (it == mPortContexts.end()) {
            throw Exception(dsERR_INVALID_PARAM, "port not found");
        }
        *iEdidVersion = it->second.lastSetEdidVersion;
    }
    INT_INFO("HdmiInput::getEdidVersion: port %d EDID version=%d", iHdmiPort, *iEdidVersion);
}

/**
 * Replaces dsHdmiInSetVRRSupport().
 * AIDL: fetch default EDID for HDMI 2.1 (VRR-capable) or 2.0 (VRR-disabled)
 *       via IHDMIInput.getDefaultEDID(), then apply via IHDMIInputController.setEDID().
 */
void HdmiInput::setVRRSupport(int iHdmiPort, bool vrrSupport)
{
    INT_INFO("HdmiInput::setVRRSupport port=%d vrrSupport=%d", iHdmiPort, vrrSupport);

    sp<IHDMIInput> hdmiInput;
    sp<IHDMIInputController> controller;
    {
        std::lock_guard<std::mutex> lock(mAidlMutex);
        auto it = mPortContexts.find(iHdmiPort);
        if (it == mPortContexts.end() || !it->second.hdmiInput) {
            throw Exception(dsERR_INVALID_PARAM, "port not found");
        }
        hdmiInput  = it->second.hdmiInput;
        controller = it->second.controller;
    }
    if (!controller) throw Exception(dsERR_GENERAL, "no controller for port");

    // VRR is signaled via HDMI 2.1 EDID; fall back to 2.0 if 2.1 not available
    HDMIVersion targetVersion = vrrSupport ? HDMIVersion::HDMI_2_1 : HDMIVersion::HDMI_2_0;
    std::vector<uint8_t> defaultEdid;
    bool getOk = false;
    ::android::binder::Status st = hdmiInput->getDefaultEDID(targetVersion, &defaultEdid, &getOk);
    if ((!st.isOk() || !getOk || defaultEdid.empty()) && vrrSupport) {
        // HDMI 2.1 EDID not available; fall back to 2.0
        targetVersion = HDMIVersion::HDMI_2_0;
        st = hdmiInput->getDefaultEDID(targetVersion, &defaultEdid, &getOk);
    }
    if (!st.isOk() || !getOk || defaultEdid.empty()) {
        INT_ERROR("HdmiInput::setVRRSupport: getDefaultEDID failed for port %d", iHdmiPort);
        throw Exception(dsERR_GENERAL, "getDefaultEDID failed");
    }

    bool setOk = false;
    st = controller->setEDID(defaultEdid, &setOk);
    if (!st.isOk() || !setOk) {
        INT_ERROR("HdmiInput::setVRRSupport: setEDID failed for port %d", iHdmiPort);
        throw Exception(dsERR_GENERAL, "setEDID failed");
    }
    INT_INFO("HdmiInput::setVRRSupport: port %d VRR=%d EDID applied", iHdmiPort, vrrSupport);
}

/**
 * Replaces dsHdmiInGetVRRSupport().
 * AIDL: IHDMIInput.getCapabilities().supportsVRR
 */
void HdmiInput::getVRRSupport (int iHdmiPort, bool *vrrSupport) {
    INT_INFO("HdmiInput::getVRRSupport port=%d", iHdmiPort);

    sp<IHDMIInput> hdmiInput;
    {
        std::lock_guard<std::mutex> lock(mAidlMutex);
        auto it = mPortContexts.find(iHdmiPort);
        if (it == mPortContexts.end() || !it->second.hdmiInput) {
            throw Exception(dsERR_INVALID_PARAM, "port not found");
        }
        hdmiInput = it->second.hdmiInput;
    }

    Capabilities caps;
    ::android::binder::Status st = hdmiInput->getCapabilities(&caps);
    if (!st.isOk()) {
        INT_ERROR("HdmiInput::getVRRSupport: getCapabilities failed for port %d", iHdmiPort);
        throw Exception(dsERR_GENERAL, "getCapabilities failed");
    }

    *vrrSupport = caps.supportsVRR;
    INT_INFO("HdmiInput::getVRRSupport: port %d vrrSupport=%d", iHdmiPort, *vrrSupport);
}

/**
 * Replaces dsHdmiInGetVRRStatus().
 * AIDL: data cached from IHDMIInputControllerListener.onVRRChanged() callback.
 * vrrType heuristic: if vrrActive and frameRate>0 → dsVRR_AMD_FREESYNC; else dsVRR_HDMI_VRR.
 */
void HdmiInput::getVRRStatus (int iHdmiPort, dsHdmiInVrrStatus_t *vrrStatus) {
    INT_INFO("HdmiInput::getVRRStatus port=%d", iHdmiPort);
    {
        std::lock_guard<std::mutex> lock(mAidlMutex);
        auto it = mPortContexts.find(iHdmiPort);
        if (it == mPortContexts.end()) {
            throw Exception(dsERR_INVALID_PARAM, "port not found");
        }
        const PortContext& ctx = it->second;
        if (ctx.vrrActive) {
            vrrStatus->vrrType = (ctx.vrrFrameRate > 0.0) ? dsVRR_AMD_FREESYNC : dsVRR_HDMI_VRR;
        } else {
            vrrStatus->vrrType = dsVRR_NONE;
        }
        vrrStatus->vrrAmdfreesyncFramerate_Hz = ctx.vrrFrameRate;
    }
    INT_INFO("HdmiInput::getVRRStatus: port %d vrrType=%d frameRate=%f",
             iHdmiPort, vrrStatus->vrrType, vrrStatus->vrrAmdfreesyncFramerate_Hz);
}

/**
 * Replaces dsGetAllmStatus().
 * AIDL: IHDMIInput.getCapabilities().supportsALLM
 */
void HdmiInput::getHdmiALLMStatus (int iHdmiPort, bool *allmStatus) {
    INT_INFO("HdmiInput::getHdmiALLMStatus port=%d", iHdmiPort);

    sp<IHDMIInput> hdmiInput;
    {
        std::lock_guard<std::mutex> lock(mAidlMutex);
        auto it = mPortContexts.find(iHdmiPort);
        if (it == mPortContexts.end() || !it->second.hdmiInput) {
            throw Exception(dsERR_INVALID_PARAM, "port not found");
        }
        hdmiInput = it->second.hdmiInput;
    }

    Capabilities caps;
    ::android::binder::Status st = hdmiInput->getCapabilities(&caps);
    if (!st.isOk()) {
        INT_ERROR("HdmiInput::getHdmiALLMStatus: getCapabilities failed for port %d", iHdmiPort);
        throw Exception(dsERR_GENERAL, "getCapabilities failed");
    }

    *allmStatus = caps.supportsALLM;
    INT_INFO("HdmiInput::getHdmiALLMStatus: port %d ALLM=%d", iHdmiPort, *allmStatus);
}

/**
 * Replaces dsGetSupportedGameFeaturesList().
 * AIDL: IHDMIInput.getCapabilities() flags + IHDMIInputManager.getCapabilities().freeSync tier.
 * Feature string mapping:
 *   supportsALLM       → "allm"
 *   supportsVRR        → "vrr_hdmi"
 *   supportsFreeSync   → "vrr_amd_freesync"
 *   freeSync==PREMIUM  → "vrr_amd_freesync_premium"
 */
void HdmiInput::getSupportedGameFeatures (std::vector<std::string> &featureList) {
    // Use first available port for global platform capabilities
    sp<IHDMIInput> hdmiInput;
    {
        std::lock_guard<std::mutex> lock(mAidlMutex);
        if (mPortContexts.empty()) throw Exception(dsERR_GENERAL, "no ports initialised");
        hdmiInput = mPortContexts.begin()->second.hdmiInput;
    }
    if (!hdmiInput) throw Exception(dsERR_GENERAL, "hdmiInput unavailable");

    Capabilities caps;
    ::android::binder::Status st = hdmiInput->getCapabilities(&caps);
    if (!st.isOk()) throw Exception(dsERR_GENERAL, "getCapabilities failed");

    featureList.clear();
    if (caps.supportsALLM)     featureList.emplace_back("allm");
    if (caps.supportsVRR)      featureList.emplace_back("vrr_hdmi");
    if (caps.supportsFreeSync) featureList.emplace_back("vrr_amd_freesync");

    // Check platform-level FreeSync tier
    sp<IHDMIInputManager> manager = const_cast<HdmiInput*>(this)->getHdmiInputManager();
    if (manager) {
        PlatformCapabilities platCaps;
        st = manager->getCapabilities(&platCaps);
        if (st.isOk()) {
            using FS = ::com::rdk::hal::hdmiinput::FreeSync;
            if (platCaps.freeSync == FS::FREESYNC_PREMIUM) {
                featureList.emplace_back("vrr_amd_freesync_premium");
            }
        }
    }

    INT_INFO("HdmiInput::getSupportedGameFeatures: %zu features", featureList.size());
}


/**
 * Replaces dsGetAVLatency().
 * AIDL video: IPlaneControl.getCapabilities()[primaryPlane].vsyncDisplayLatency (vsync periods).
 *             Converted to ms assuming 60 Hz (16 ms/frame) — adjust for platform if needed.
 * AIDL audio: IAudioMixerController.getProperty(LATENCY_MS) — deferred; stubbed as 0.
 */
void HdmiInput::getAVLatency (int *audio_output_delay, int *video_latency) {
    // --- Video latency via IPlaneControl ---
    sp<IPlaneControl> planeCtrl = const_cast<HdmiInput*>(this)->getPlaneControl();
    if (planeCtrl) {
        std::vector<::com::rdk::hal::planecontrol::PlaneCapabilities> planeCaps;
        ::android::binder::Status st = planeCtrl->getCapabilities(&planeCaps);
        if (st.isOk() && !planeCaps.empty()) {
            // vsyncDisplayLatency is in vsync periods; multiply by frame period (ms)
            *video_latency = planeCaps[HDMI_IN_PRIMARY_PLANE_INDEX].vsyncDisplayLatency * 16;
        } else {
            *video_latency = 0;
        }
    } else {
        *video_latency = 0;
    }

    // --- Audio latency: IAudioMixerController not yet implemented in prototype ---
    *audio_output_delay = 0;
    INT_INFO("HdmiInput::getAVLatency: videoLatency=%d audioLatency=%d",
             *video_latency, *audio_output_delay);
}

/**
 * Prototype gap: dsSetEdid2AllmSupport has no direct AIDL equivalent in the hdmiinput
 * HAL mapping (Mappings.csv row: "Not defined"). ALLM EDID advertisement is handled
 * via getDefaultEDID()/setEDID() for ALLM-capable HDMI versions; a dedicated setter
 * is not exposed on IHDMIInputController in this AIDL version.
 */
void HdmiInput::setEdid2AllmSupport(int iHdmiPort, bool allmSupport)
{
    INT_INFO("HdmiInput::setEdid2AllmSupport: port=%d allm=%d "
             "-- no AIDL equivalent (prototype gap; dsSetEdid2AllmSupport mapping=Not defined)",
             iHdmiPort, allmSupport);
    // No-op in AIDL prototype.
}

/**
 * Replaces dsGetEdid2AllmSupport().
 * AIDL: IHDMIInput.getCapabilities().supportsALLM
 */
void HdmiInput::getEdid2AllmSupport (int iHdmiPort, bool *allmSupport) {
    INT_INFO("HdmiInput::getEdid2AllmSupport port=%d", iHdmiPort);

    sp<IHDMIInput> hdmiInput;
    {
        std::lock_guard<std::mutex> lock(mAidlMutex);
        auto it = mPortContexts.find(iHdmiPort);
        if (it == mPortContexts.end() || !it->second.hdmiInput) {
            throw Exception(dsERR_INVALID_PARAM, "port not found");
        }
        hdmiInput = it->second.hdmiInput;
    }

    Capabilities caps;
    ::android::binder::Status st = hdmiInput->getCapabilities(&caps);
    if (!st.isOk()) {
        INT_ERROR("HdmiInput::getEdid2AllmSupport: getCapabilities failed for port %d", iHdmiPort);
        throw Exception(dsERR_GENERAL, "getCapabilities failed");
    }

    *allmSupport = caps.supportsALLM;
    INT_INFO("HdmiInput::getEdid2AllmSupport: port %d ALLM=%d", iHdmiPort, *allmSupport);
}

/**
 * Replaces dsGetHdmiVersion().
 * AIDL: IHDMIInput.getCapabilities().supportedVersions[] → max HDMIVersion
 *       mapped to dsHdmiMaxCapabilityVersion_t.
 * Version map: HDMI_1_3/1_4 → HDMI_COMPATIBILITY_VERSION_14
 *              HDMI_2_0     → HDMI_COMPATIBILITY_VERSION_20
 *              HDMI_2_1     → HDMI_COMPATIBILITY_VERSION_21
 */
void HdmiInput::getHdmiVersion (int iHdmiPort, dsHdmiMaxCapabilityVersion_t *capversion) {
    INT_INFO("HdmiInput::getHdmiVersion port=%d", iHdmiPort);

    sp<IHDMIInput> hdmiInput;
    {
        std::lock_guard<std::mutex> lock(mAidlMutex);
        auto it = mPortContexts.find(iHdmiPort);
        if (it == mPortContexts.end() || !it->second.hdmiInput) {
            throw Exception(dsERR_INVALID_PARAM, "port not found");
        }
        hdmiInput = it->second.hdmiInput;
    }

    Capabilities caps;
    ::android::binder::Status st = hdmiInput->getCapabilities(&caps);
    if (!st.isOk()) {
        INT_ERROR("HdmiInput::getHdmiVersion: getCapabilities failed for port %d", iHdmiPort);
        throw Exception(dsERR_GENERAL, "getCapabilities failed");
    }

    // Find highest supported HDMI version
    HDMIVersion maxVersion = HDMIVersion::HDMI_1_4;
    for (const auto& v : caps.supportedVersions) {
        if (static_cast<int>(v) > static_cast<int>(maxVersion)) maxVersion = v;
    }

    switch (maxVersion) {
        case HDMIVersion::HDMI_1_3:
        case HDMIVersion::HDMI_1_4: *capversion = HDMI_COMPATIBILITY_VERSION_14; break;
        case HDMIVersion::HDMI_2_0: *capversion = HDMI_COMPATIBILITY_VERSION_20; break;
        case HDMIVersion::HDMI_2_1: *capversion = HDMI_COMPATIBILITY_VERSION_21; break;
        default:                    *capversion = HDMI_COMPATIBILITY_VERSION_14; break;
    }
    INT_INFO("HdmiInput::getHdmiVersion: port %d dsVersion=%d", iHdmiPort, *capversion);
}

/**
 * Replaces dsGetHDMIARCPortId().
 * AIDL: iterate ports, check IHDMIInput.getCapabilities().supportsARC.
 * Returns first ARC-capable port index, or -1 with dsERR_GENERAL if none found.
 */
dsError_t HdmiInput::getHDMIARCPortId(int &portId) {
    // Snapshot the map to avoid holding the lock across AIDL calls
    std::map<int, sp<IHDMIInput>> portInputs;
    {
        std::lock_guard<std::mutex> lock(mAidlMutex);
        for (const auto& kv : mPortContexts) {
            portInputs[kv.first] = kv.second.hdmiInput;
        }
    }

    for (const auto& kv : portInputs) {
        if (!kv.second) continue;
        Capabilities caps;
        ::android::binder::Status st = kv.second->getCapabilities(&caps);
        if (st.isOk() && caps.supportsARC) {
            portId = kv.first;
            INT_INFO("HdmiInput::getHDMIARCPortId: ARC port found at index %d", portId);
            return dsERR_NONE;
        }
    }

    portId = -1;
    INT_INFO("HdmiInput::getHDMIARCPortId: no ARC-capable port found");
    return dsERR_GENERAL;
}

/**
 * Prototype gap: no AIDL equivalent for dsHdmiInPauseAudio in the hdmiinput HAL interface.
 * Audio routing/pause is a concern of IAudioMixerController (separate service scope),
 * which is not integrated in this prototype.
 */
void HdmiInput::pauseAudio() const
{
    INT_INFO("HdmiInput::pauseAudio: not implemented in AIDL prototype (no AIDL equivalent)");
}

/**
 * Prototype gap: no AIDL equivalent for dsHdmiInResumeAudio in the hdmiinput HAL interface.
 */
void HdmiInput::resumeAudio() const
{
    INT_INFO("HdmiInput::resumeAudio: not implemented in AIDL prototype (no AIDL equivalent)");
}

}


/** @} */
/** @} */

