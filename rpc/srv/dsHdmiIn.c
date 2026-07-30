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
* @defgroup rpc
* @{
**/




/**
* @defgroup devicesettings
* @{
* @defgroup rpc
* @{
**/


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <dlfcn.h>
#include "dsHdmiIn.h"
#include "dsRpc.h"
#include "dsTypes.h"
#include "dsserverlogger.h"
#include "dsMgr.h"

#include "iarmUtil.h"
#include "libIARM.h"
#include "libIBus.h"
#include "rfcapi.h"
#include "safec_lib.h"

#include "dsInternal.h"

#define direct_list_top(list) ((list))
#define IARM_BUS_Lock(lock) pthread_mutex_lock(&fpLock)
#define IARM_BUS_Unlock(lock) pthread_mutex_unlock(&fpLock)
#define TVSETTINGS_DALS_RFC_PARAM "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.TvSettings.DynamicAutoLatency"
[O
static bool isDalsEnabled = false;
static int m_isInitialized = 0;
static int m_isPlatInitialized=0;
static pthread_mutex_t fpLock = PTHREAD_MUTEX_INITIALIZER;
static tv_hdmi_edid_version_t m_edidversion[dsHDMI_IN_PORT_MAX];
static bool m_edidallmsupport[dsHDMI_IN_PORT_MAX];
static bool m_vrrsupport[dsHDMI_IN_PORT_MAX];
static bool m_hdmiPortVrrCaps[dsHDMI_IN_PORT_MAX];
static uint8_t noOfSupportedHdmiInputs;
IARM_Result_t dsHdmiInMgr_init();
IARM_Result_t dsHdmiInMgr_term();
IARM_Result_t _dsHdmiInInit(void *arg);
IARM_Result_t _dsHdmiInTerm(void *arg);
IARM_Result_t _dsHdmiInLoadKsvs(void *arg);
IARM_Result_t _dsHdmiInGetNumberOfInputs(void *arg);
IARM_Result_t _dsHdmiInGetStatus(void *arg);
IARM_Result_t _dsHdmiInSelectPort(void *arg);
IARM_Result_t _dsHdmiInToggleHotPlug(void *arg);
IARM_Result_t _dsHdmiInLoadEdidData(void *arg);
IARM_Result_t _dsHdmiInSetRepeater(void *arg);
IARM_Result_t _dsHdmiInScaleVideo(void *arg);
IARM_Result_t _dsHdmiInSelectZoomMode(void *arg);
IARM_Result_t _dsHdmiInGetCurrentVideoMode(void *arg);
IARM_Result_t _dsGetEDIDBytesInfo (void *arg);
IARM_Result_t _dsGetHDMISPDInfo (void *arg);
IARM_Result_t _dsSetEdidVersion (void *arg);
IARM_Result_t _dsGetEdidVersion (void *arg);
IARM_Result_t _dsGetAllmStatus (void *arg);
IARM_Result_t _dsGetSupportedGameFeaturesList (void *arg);
IARM_Result_t _dsGetAVLatency (void *arg);
IARM_Result_t _dsSetEdid2AllmSupport (void *arg);
IARM_Result_t _dsGetEdid2AllmSupport (void *arg);
IARM_Result_t _dsSetVRRSupport (void *arg);
IARM_Result_t _dsGetVRRSupport (void *arg);
IARM_Result_t _dsGetVRRStatus (void *arg);
IARM_Result_t _dsGetHdmiVersion (void *arg);

static dsError_t setEdid2AllmSupport (dsHdmiInPort_t iHdmiPort, bool allmSupport);
static dsError_t setVRRSupport (dsHdmiInPort_t iHdmiPort, bool vrrSupport);
static dsError_t getVRRSupport (dsHdmiInPort_t iHdmiPort, bool *vrrSupport);
void _dsHdmiInConnectCB(dsHdmiInPort_t port, bool isPortConnected);
void _dsHdmiInSignalChangeCB(dsHdmiInPort_t port, dsHdmiInSignalStatus_t sigStatus);
void _dsHdmiInStatusChangeCB(dsHdmiInStatus_t inputStatus);
void _dsHdmiInVideoModeUpdateCB(dsHdmiInPort_t port, dsVideoPortResolution_t videoResolution);
void _dsHdmiInAllmChangeCB(dsHdmiInPort_t port, bool allm_mode);
void _dsHdmiInVRRChangeCB(dsHdmiInPort_t port, dsVRRType_t vrr_type);
void _dsHdmiInAviContentTypeChangeCB(dsHdmiInPort_t port, dsAviContentType_t content_type);
void _dsHdmiInAVLatencyChangeCB(int audio_latency, int video_latency);

static dsHdmiInCap_t hdmiInCap_gs;

#include <iostream>
#include "hostPersistence.hpp"
#include <sstream>
#include <mutex>
#include <map>
#include <vector>
#include <algorithm>

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
#include <com/rdk/hal/hdmiinput/SignalState.h>
#include <com/rdk/hal/hdmiinput/State.h>
#include <com/rdk/hal/hdmiinput/HDCPStatus.h>
#include <com/rdk/hal/hdmiinput/HDCPProtocolVersion.h>
#include <com/rdk/hal/hdmiinput/HDMIVersion.h>
#include <com/rdk/hal/planecontrol/IPlaneControl.h>
#include <com/rdk/hal/planecontrol/PlaneCapabilities.h>
#include <com/rdk/hal/planecontrol/Property.h>
#include <com/rdk/hal/planecontrol/PropertyKVPair.h>
#include <com/rdk/hal/planecontrol/SourcePlaneMapping.h>
#include <com/rdk/hal/planecontrol/SourceType.h>
#include <com/rdk/hal/PropertyValue.h>

using android::sp;
using android::defaultServiceManager;
using android::interface_cast;
using android::String16;
using android::ProcessState;
using namespace com::rdk::hal::hdmiinput;
using namespace com::rdk::hal::planecontrol;

#define HDMI_IN_PRIMARY_PLANE_INDEX 0

// Per-port AIDL runtime context
struct AidlPortCtx {
    sp<IHDMIInput>              hdmiInput;
    sp<IHDMIInputController>    controller;
    sp<IHDMIInputControllerListener> ctrlListener;
    sp<IHDMIInputEventListener> evtListener;
    bool isOpen{false};
    bool isStarted{false};
    bool connected{false};
    int  signalState{-1};
    int  lastVIC{0};
    bool vrrActive{false};
    double vrrFrameRate{0.0};
};

static sp<IHDMIInputManager>       s_aidlHdmiMgr;
static sp<IPlaneControl>           s_aidlPlaneCtrl;
static std::mutex                  s_aidlMutex;
static std::map<int, AidlPortCtx>  s_aidlPorts;
static int                         s_aidlActivePort{-1};
static uint8_t                     s_aidlPortCount{0};
static bool                        s_aidlPortArcCapable[dsHDMI_IN_PORT_MAX] = {};

static sp<IHDMIInputManager> getAidlHdmiMgr()
{
    std::lock_guard<std::mutex> lk(s_aidlMutex);
    if (!s_aidlHdmiMgr) {
        ProcessState::self()->startThreadPool();
        sp<android::IServiceManager> sm = defaultServiceManager();
        if (sm) {
            s_aidlHdmiMgr = interface_cast<IHDMIInputManager>(
                sm->getService(String16(IHDMIInputManager::serviceName().c_str())));
        }
    }
    return s_aidlHdmiMgr;
}

static sp<IPlaneControl> getAidlPlaneCtrl()
{
    std::lock_guard<std::mutex> lk(s_aidlMutex);
    if (!s_aidlPlaneCtrl) {
        sp<android::IServiceManager> sm = defaultServiceManager();
        if (sm) {
            s_aidlPlaneCtrl = interface_cast<IPlaneControl>(
                sm->getService(String16(IPlaneControl::serviceName().c_str())));
        }
    }
    return s_aidlPlaneCtrl;
}

static void vicToResolutionObj_srv(int vic, dsVideoPortResolution_t& res)
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

// ---- AIDL listener: per-port IHDMIInputController callbacks ----
class SrvHdmiCtrlListener
    : public ::com::rdk::hal::hdmiinput::BnHDMIInputControllerListener
{
public:
    explicit SrvHdmiCtrlListener(int portId) : m_portId(portId) {}

    ::android::binder::Status onConnectionStateChanged(bool connected) override {
        INT_INFO("[srv-aidl] port %d connected=%d\n", m_portId, connected);
        {
            std::lock_guard<std::mutex> lk(s_aidlMutex);
            auto it = s_aidlPorts.find(m_portId);
            if (it != s_aidlPorts.end()) it->second.connected = connected;
        }
        _dsHdmiInConnectCB((dsHdmiInPort_t)m_portId, connected);
        return ::android::binder::Status::ok();
    }

    ::android::binder::Status onSignalStateChanged(
            ::com::rdk::hal::hdmiinput::SignalState signalState) override {
        INT_INFO("[srv-aidl] port %d signalState=%d\n", m_portId, (int)signalState);
        {
            std::lock_guard<std::mutex> lk(s_aidlMutex);
            auto it = s_aidlPorts.find(m_portId);
            if (it != s_aidlPorts.end()) it->second.signalState = (int)signalState;
        }
        _dsHdmiInSignalChangeCB((dsHdmiInPort_t)m_portId,
            (dsHdmiInSignalStatus_t)(int)signalState);
        return ::android::binder::Status::ok();
    }

    ::android::binder::Status onVIChanged(
            ::com::rdk::hal::hdmiinput::VIC vic) override {
        INT_INFO("[srv-aidl] port %d VIC=%d\n", m_portId, (int)vic);
        dsVideoPortResolution_t res;
        vicToResolutionObj_srv((int)vic, res);
        {
            std::lock_guard<std::mutex> lk(s_aidlMutex);
            auto it = s_aidlPorts.find(m_portId);
            if (it != s_aidlPorts.end()) it->second.lastVIC = (int)vic;
        }
        _dsHdmiInVideoModeUpdateCB((dsHdmiInPort_t)m_portId, res);
        return ::android::binder::Status::ok();
    }

    ::android::binder::Status onVRRChanged(
            bool vrrActive, bool /*mConstActive*/, bool /*fastVActive*/,
            double frameRate) override {
        dsVRRType_t vrrType = vrrActive
            ? (frameRate > 0.0 ? dsVRR_AMD_FREESYNC : dsVRR_HDMI_VRR)
            : dsVRR_NONE;
        {
            std::lock_guard<std::mutex> lk(s_aidlMutex);
            auto it = s_aidlPorts.find(m_portId);
            if (it != s_aidlPorts.end()) {
                it->second.vrrActive    = vrrActive;
                it->second.vrrFrameRate = frameRate;
            }
        }
        _dsHdmiInVRRChangeCB((dsHdmiInPort_t)m_portId, vrrType);
        return ::android::binder::Status::ok();
    }

    ::android::binder::Status onAVIInfoFrame(const std::vector<uint8_t>&) override
        { return ::android::binder::Status::ok(); }
    ::android::binder::Status onAudioInfoFrame(const std::vector<uint8_t>&) override
        { return ::android::binder::Status::ok(); }
    ::android::binder::Status onSPDInfoFrame(const std::vector<uint8_t>&) override
        { return ::android::binder::Status::ok(); }
    ::android::binder::Status onDRMInfoFrame(const std::vector<uint8_t>&) override
        { return ::android::binder::Status::ok(); }
    ::android::binder::Status onVendorSpecificInfoFrame(const std::vector<uint8_t>&) override
        { return ::android::binder::Status::ok(); }
    ::android::binder::Status onHDCPStatusChanged(
            ::com::rdk::hal::hdmiinput::HDCPStatus,
            ::com::rdk::hal::hdmiinput::HDCPProtocolVersion) override
        { return ::android::binder::Status::ok(); }
private:
    int m_portId;
};

// ---- AIDL listener: port-level state / EDID change events ----
class SrvHdmiEvtListener
    : public ::com::rdk::hal::hdmiinput::BnHDMIInputEventListener
{
public:
    explicit SrvHdmiEvtListener(int portId) : m_portId(portId) {}

    ::android::binder::Status onStateChanged(
            ::com::rdk::hal::hdmiinput::State /*oldState*/,
            ::com::rdk::hal::hdmiinput::State newState) override {
        bool presented = (newState == ::com::rdk::hal::hdmiinput::State::STARTED);
        {
            std::lock_guard<std::mutex> lk(s_aidlMutex);
            if (presented) s_aidlActivePort = m_portId;
            else if (s_aidlActivePort == m_portId) s_aidlActivePort = -1;
        }
        dsHdmiInStatus_t status;
        memset(&status, 0, sizeof(status));
        status.activePort  = (dsHdmiInPort_t)m_portId;
        status.isPresented = presented;
        {
            std::lock_guard<std::mutex> lk(s_aidlMutex);
            auto it = s_aidlPorts.find(m_portId);
            if (it != s_aidlPorts.end() && m_portId < dsHDMI_IN_PORT_MAX)
                status.isPortConnected[m_portId] = it->second.connected;
        }
        _dsHdmiInStatusChangeCB(status);
        return ::android::binder::Status::ok();
    }
    ::android::binder::Status onEDIDChange(const std::vector<uint8_t>&) override
        { return ::android::binder::Status::ok(); }
private:
    int m_portId;
};

// ---- AIDL init: replaces dsHdmiInInit() + dlopen callback registrations ----
static void aidlHdmiInInit()
{
    sp<IHDMIInputManager> mgr = getAidlHdmiMgr();
    if (!mgr) {
        INT_ERROR("[srv-aidl] IHDMIInputManager unavailable\n");
        return;
    }

    std::vector<IHDMIInput::Id> portIds;
    if (!mgr->getHDMIInputIds(&portIds).isOk()) {
        INT_ERROR("[srv-aidl] getHDMIInputIds failed\n");
        return;
    }
    s_aidlPortCount = (uint8_t)portIds.size();

    for (const auto& id : portIds) {
        int portIdx = id.value;
        sp<IHDMIInput> hdmiInput;
        if (!mgr->getHDMIInput(id, &hdmiInput).isOk() || !hdmiInput) {
            INT_ERROR("[srv-aidl] getHDMIInput failed for port %d\n", portIdx);
            continue;
        }
        AidlPortCtx ctx;
        ctx.hdmiInput = hdmiInput;

        Capabilities caps;
        if (hdmiInput->getCapabilities(&caps).isOk() && portIdx < dsHDMI_IN_PORT_MAX) {
            s_aidlPortArcCapable[portIdx] = caps.supportsARC;
            m_hdmiPortVrrCaps[portIdx]    = caps.supportsVRR;
        }

        ctx.evtListener = sp<SrvHdmiEvtListener>::make(portIdx);
        bool regOk = false;
        hdmiInput->registerEventListener(ctx.evtListener, &regOk);

        ctx.ctrlListener = sp<SrvHdmiCtrlListener>::make(portIdx);
        sp<IHDMIInputController> ctrl;
        if (hdmiInput->open(ctx.ctrlListener, &ctrl).isOk() && ctrl) {
            ctx.controller = ctrl;
            ctx.isOpen     = true;
        }

        std::lock_guard<std::mutex> lk(s_aidlMutex);
        s_aidlPorts[portIdx] = std::move(ctx);
        INT_INFO("[srv-aidl] port %d initialised\n", portIdx);
    }
}

// ---- AIDL term: replaces dsHdmiInTerm() ----
static void aidlHdmiInTerm()
{
    std::lock_guard<std::mutex> lk(s_aidlMutex);
    for (auto& kv : s_aidlPorts) {
        AidlPortCtx& ctx = kv.second;
        if (ctx.isStarted && ctx.controller) {
            ctx.controller->stop();
            ctx.isStarted = false;
        }
        if (ctx.isOpen && ctx.hdmiInput && ctx.controller) {
            bool ok = false;
            ctx.hdmiInput->close(ctx.controller, &ok);
            ctx.isOpen = false;
        }
        if (ctx.hdmiInput && ctx.evtListener) {
            bool ok = false;
            ctx.hdmiInput->unregisterEventListener(ctx.evtListener, &ok);
        }
    }
    s_aidlPorts.clear();
    s_aidlHdmiMgr   = nullptr;
    s_aidlPlaneCtrl = nullptr;
    s_aidlPortCount = 0;
}

using namespace std;

void getDynamicAutoLatencyConfig()
{
     RFC_ParamData_t param = {0};
     WDMP_STATUS status = getRFCParameter((char*)"dssrv", TVSETTINGS_DALS_RFC_PARAM, &param);
     INT_DEBUG("DALS Feature Enable = [ %s ] \n", param.value);
     if(WDMP_SUCCESS == status && (strncasecmp(param.value,"true",4) == 0)) {
         isDalsEnabled = true;
         INT_INFO("Value of isDalsEnabled = [ %d ] \n", isDalsEnabled);
     }
     else {
         INT_ERROR("Fetching RFC for DALS failed or DALS is disabled\n");
     }
}

static dsError_t isHdmiARCPort (int iPort, bool* isArcEnabled) {
[I    if (!s_aidlPorts.empty()) {
        if (iPort >= 0 && iPort < dsHDMI_IN_PORT_MAX) {
            *isArcEnabled = s_aidlPortArcCapable[iPort];
            INT_INFO("[srv-aidl] isHdmiARCPort port %d arc=%d\n", iPort, *isArcEnabled);
            return dsERR_NONE;
        }
        return dsERR_INVALID_PARAM;
    }
    dsError_t eRet = dsERR_GENERAL; 

    typedef bool (*dsIsHdmiARCPort_t)(int iPortArg, bool *boolArg);
    static dsIsHdmiARCPort_t dsIsHdmiARCPortFunc = 0;
    if (dsIsHdmiARCPortFunc == 0) {
       void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
       if (dllib) {
            dsIsHdmiARCPortFunc = (dsIsHdmiARCPort_t) dlsym(dllib, "dsIsHdmiARCPort");
            if(dsIsHdmiARCPortFunc == 0) {
                INT_INFO("%s:%d dsIsHdmiARCPort (int) is not defined %s\r\n", __FUNCTION__,__LINE__, dlerror());
                eRet = dsERR_GENERAL;
            }
            else {
                INT_DEBUG("%s:%d dsIsHdmiARCPort dsIsHdmiARCPortFunc loaded\r\n", __FUNCTION__,__LINE__);
            }
            dlclose(dllib);
        }
        else {
            INT_ERROR("%s:%d dsIsHdmiARCPort  Opening RDK_DSHAL_NAME[%s] failed %s\r\n", 
                   __FUNCTION__,__LINE__, RDK_DSHAL_NAME, dlerror());  //CID 168096 - Print Args
            eRet = dsERR_GENERAL;
        }
    }
    if (0 != dsIsHdmiARCPortFunc) { 
        dsIsHdmiARCPortFunc (iPort, isArcEnabled);
        INT_INFO("%s: dsIsHdmiARCPort port %d isArcEnabled:%d\r\n", __FUNCTION__, iPort, *isArcEnabled);
    }
    else {
        INT_INFO("%s: dsIsHdmiARCPort  dsIsHdmiARCPortFunc = %p\n", __FUNCTION__, dsIsHdmiARCPortFunc);
    }
    return eRet;
}

static dsError_t getEDIDBytesInfo (dsHdmiInPort_t iHdmiPort, unsigned char *edid, int *length) {
    if (!s_aidlPorts.empty()) {
        sp<IHDMIInput> hi;
        {
            std::lock_guard<std::mutex> lk(s_aidlMutex);
            auto it = s_aidlPorts.find((int)iHdmiPort);
            if (it == s_aidlPorts.end() || !it->second.hdmiInput) return dsERR_INVALID_PARAM;
            hi = it->second.hdmiInput;
        }
        std::vector<uint8_t> edidVec;
        bool ok = false;
        if (!hi->getEDID(&edidVec, &ok).isOk() || !ok || edidVec.empty()) {
            INT_ERROR("[srv-aidl] getEDID failed for port %d\n", (int)iHdmiPort);
            return dsERR_GENERAL;
        }
        *length = (int)edidVec.size();
        memcpy(edid, edidVec.data(), *length);
        INT_INFO("[srv-aidl] getEDIDBytesInfo port %d len=%d\n", (int)iHdmiPort, *length);
        return dsERR_NONE;
    }
    dsError_t eRet = dsERR_GENERAL;
    typedef dsError_t (*dsGetEDIDBytesInfo_t)(dsHdmiInPort_t iHdmiPort, unsigned char *edid, int *length);
    static dsGetEDIDBytesInfo_t dsGetEDIDBytesInfoFunc = 0;
    if (dsGetEDIDBytesInfoFunc == 0) {
       void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
       if (dllib) {
            dsGetEDIDBytesInfoFunc = (dsGetEDIDBytesInfo_t) dlsym(dllib, "dsGetEDIDBytesInfo");
            if(dsGetEDIDBytesInfoFunc == 0) {
                INT_INFO("%s:%d dsGetEDIDBytesInfo (int) is not defined %s\r\n", __FUNCTION__,__LINE__, dlerror());
                eRet = dsERR_GENERAL;
            }
            else {
                INT_INFO("%s:%d dsGetEDIDBytesInfoFunc loaded\r\n", __FUNCTION__,__LINE__);
            }
            dlclose(dllib);
        }
        else {
            INT_ERROR("%s:%d dsGetEDIDBytesInfo  Opening RDK_DSHAL_NAME [%s] failed %s\r\n",
                   __FUNCTION__,__LINE__, RDK_DSHAL_NAME, dlerror());
            eRet = dsERR_GENERAL;
        }
    }
    if (0 != dsGetEDIDBytesInfoFunc) {
        INT_INFO("%s:%d Entering dsGetEDIDBytesInfoFunc\r\n", __FUNCTION__,__LINE__);
        eRet = dsGetEDIDBytesInfoFunc (iHdmiPort, edid, length);
        INT_INFO("[srv] %s: dsGetEDIDBytesInfoFunc eRet: %d data len: %d \r\n", __FUNCTION__,eRet, *length);
    }
    return eRet;
}

static dsError_t getHDMISPDInfo (dsHdmiInPort_t iHdmiPort, unsigned char *spd) {
    if (!s_aidlPorts.empty()) {
        sp<IHDMIInput> hi;
        {
            std::lock_guard<std::mutex> lk(s_aidlMutex);
            auto it = s_aidlPorts.find((int)iHdmiPort);
            if (it == s_aidlPorts.end() || !it->second.hdmiInput) return dsERR_INVALID_PARAM;
            hi = it->second.hdmiInput;
        }
        std::vector<uint8_t> spdVec;
        bool ok = false;
        if (!hi->getSPDInfoFrame(&spdVec, &ok).isOk() || !ok || spdVec.empty()) {
            INT_ERROR("[srv-aidl] getSPDInfoFrame failed for port %d\n", (int)iHdmiPort);
            return dsERR_GENERAL;
        }
        size_t copyLen = std::min(spdVec.size(), (size_t)HDMI_SRC_PRODUCT_DESC_MAX_LEN);
        memcpy(spd, spdVec.data(), copyLen);
        INT_INFO("[srv-aidl] getHDMISPDInfo port %d len=%zu\n", (int)iHdmiPort, copyLen);
        return dsERR_NONE;
    }
    dsError_t eRet = dsERR_GENERAL;
    typedef dsError_t (*dsGetHDMISPDInfo_t)(dsHdmiInPort_t iHdmiPort, unsigned char *data);
    static dsGetHDMISPDInfo_t dsGetHDMISPDInfoFunc = 0;
    if (dsGetHDMISPDInfoFunc == 0) {
       void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
       if (dllib) {
            dsGetHDMISPDInfoFunc = (dsGetHDMISPDInfo_t) dlsym(dllib, "dsGetHDMISPDInfo");
            if(dsGetHDMISPDInfoFunc == 0) {
                INT_INFO("%s:%d dsGetHDMISPDInfo (int) is not defined %s\r\n", __FUNCTION__,__LINE__, dlerror());
                eRet = dsERR_GENERAL;
            }
            else {
                INT_DEBUG("%s:%d dsGetHDMISPDInfoFunc loaded\r\n", __FUNCTION__,__LINE__);
            }
            dlclose(dllib);
        }
        else {
            INT_INFO("%s:%d dsGetHDMISPDInfo  Opening RDK_DSHAL_NAME [%s] failed %s\r\n",
                   __FUNCTION__,__LINE__, RDK_DSHAL_NAME, dlerror());
            eRet = dsERR_GENERAL;
        }
    }
    if (0 != dsGetHDMISPDInfoFunc) {
        eRet = dsGetHDMISPDInfoFunc (iHdmiPort, spd);
        INT_INFO("[srv] %s: dsGetHDMISPDInfoFunc eRet: %d \r\n", __FUNCTION__,eRet);
    }
    else {
        INT_INFO("%s:  dsGetHDMISPDInfoFunc = %p\n", __FUNCTION__, dsGetHDMISPDInfoFunc);
    }
    return eRet;
}

static dsError_t setEdidVersion (dsHdmiInPort_t iHdmiPort, tv_hdmi_edid_version_t iEdidVersion) {
    dsError_t eRet = dsERR_GENERAL;
    typedef dsError_t (*dsSetEdidVersion_t)(dsHdmiInPort_t iHdmiPort, tv_hdmi_edid_version_t iEdidVersion);
    static dsSetEdidVersion_t dsSetEdidVersionFunc = 0;
    char edidVer[2];
    sprintf(edidVer,"%d\0",iEdidVersion);

    if (dsSetEdidVersionFunc == 0) {
       void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
       if (dllib) {
            dsSetEdidVersionFunc = (dsSetEdidVersion_t) dlsym(dllib, "dsSetEdidVersion");
            if(dsSetEdidVersionFunc == 0) {
                INT_INFO("%s:%d dsSetEdidVersion (int) is not defined %s\r\n", __FUNCTION__, __LINE__, dlerror());
            }
            else {
                INT_DEBUG("%s:%d dsSetEdidVersionFunc loaded\r\n", __FUNCTION__, __LINE__);
            }
            dlclose(dllib);
        }
        else {
            INT_ERROR("%s:%d dsSetEdidVersion  Opening RDK_DSHAL_NAME [%s] failed %s\r\n",
                   __FUNCTION__, __LINE__, RDK_DSHAL_NAME, dlerror());
        }
    }

    if (0 != dsSetEdidVersionFunc) {
        eRet = dsSetEdidVersionFunc (iHdmiPort, iEdidVersion);
        if (eRet == dsERR_NONE) {
           int port_no = (int)iHdmiPort;
			if((port_no  >= 0) && (port_no < noOfSupportedHdmiInputs))
			{
                std::string port_edidVer = "HDMI"+std::to_string(port_no)+".edidversion";
                device::HostPersistence::getInstance().persistHostProperty(port_edidVer, edidVer);
                INT_INFO("Port HDMI%d: Persist EDID Version: %d\n", port_no, iEdidVersion);
			}
		   else
	       {
		        INT_INFO("Invalid port number to update in persistence\n");
	       }

            // Whenever there is a change in edid version to 2.0, ensure the edid allm and vrr support is updated with latest value
            if(iEdidVersion == HDMI_EDID_VER_20)
            {
               INT_INFO("As the version is changed to 2.0, we are updating the allm and vrr bit in edid\n");
               setEdid2AllmSupport(iHdmiPort,m_edidallmsupport[iHdmiPort]);
			   setVRRSupport(iHdmiPort,m_vrrsupport[iHdmiPort]);
            }
        }
        INT_INFO("[srv] %s: dsSetEdidVersionFunc eRet: %d \r\n", __FUNCTION__, eRet);
    }
    else {
        INT_INFO("%s:  dsSetEdidVersionFunc = %p\n", __FUNCTION__, dsSetEdidVersionFunc);
    }
    return eRet;
}

static dsError_t getEdidVersion (dsHdmiInPort_t iHdmiPort, int *iEdidVersion) {
    dsError_t eRet = dsERR_GENERAL;
    typedef dsError_t (*dsGetEdidVersion_t)(dsHdmiInPort_t iHdmiPort, tv_hdmi_edid_version_t *iEdidVersion);
    static dsGetEdidVersion_t dsGetEdidVersionFunc = 0;
    if (dsGetEdidVersionFunc == 0) {
       void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
       if (dllib) {
            dsGetEdidVersionFunc = (dsGetEdidVersion_t) dlsym(dllib, "dsGetEdidVersion");
            if(dsGetEdidVersionFunc == 0) {
                INT_INFO("%s:%d dsGetEdidVersion (int) is not defined %s\r\n", __FUNCTION__, __LINE__, dlerror());
            }
            else {
                INT_DEBUG("%s:%d dsGetEdidVersionFunc loaded\r\n", __FUNCTION__, __LINE__);
            }
            dlclose(dllib);
        }
        else {
            INT_ERROR("%s:%d dsGetEdidVersion  Opening RDK_DSHAL_NAME [%s] failed %s\r\n",
                   __FUNCTION__, __LINE__, RDK_DSHAL_NAME, dlerror());
        }
    }
    if (0 != dsGetEdidVersionFunc) {
        tv_hdmi_edid_version_t EdidVersion;
        eRet = dsGetEdidVersionFunc (iHdmiPort, &EdidVersion);
        int tmp = static_cast<int>(EdidVersion);
        *iEdidVersion = tmp;
        INT_INFO("[srv] %s: dsGetEdidVersionFunc eRet: %d \r\n", __FUNCTION__, eRet);
    }
    else {
        INT_INFO("%s:  dsGetEdidVersionFunc = %p\n", __FUNCTION__, dsGetEdidVersionFunc);
    }
    return eRet;
}

static dsError_t getAllmStatus (dsHdmiInPort_t iHdmiPort, bool *allmStatus) {
    if (!s_aidlPorts.empty()) {
        sp<IHDMIInput> hi;
        {
            std::lock_guard<std::mutex> lk(s_aidlMutex);
            auto it = s_aidlPorts.find((int)iHdmiPort);
            if (it == s_aidlPorts.end() || !it->second.hdmiInput) return dsERR_INVALID_PARAM;
            hi = it->second.hdmiInput;
        }
        Capabilities caps;
        if (!hi->getCapabilities(&caps).isOk()) {
            INT_ERROR("[srv-aidl] getCapabilities failed for port %d\n", (int)iHdmiPort);
            return dsERR_GENERAL;
        }
        *allmStatus = caps.supportsALLM;
        INT_INFO("[srv-aidl] getAllmStatus port %d allm=%d\n", (int)iHdmiPort, *allmStatus);
        return dsERR_NONE;
    }
    dsError_t eRet = dsERR_GENERAL;
    typedef dsError_t (*dsGetAllmStatus_t)(dsHdmiInPort_t iHdmiPort, bool *allmStatus);
    static dsGetAllmStatus_t dsGetAllmStatusFunc = 0;
    if (dsGetAllmStatusFunc == 0) {
       void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
       if (dllib) {
            dsGetAllmStatusFunc = (dsGetAllmStatus_t) dlsym(dllib, "dsGetAllmStatus");
            if(dsGetAllmStatusFunc == 0) {
                INT_INFO("%s:%d dsGetAllmStatus (int) is not defined %s\r\n", __FUNCTION__, __LINE__, dlerror());
            }
            else {
                INT_DEBUG("%s:%d dsGetAllmStatusFunc loaded\r\n", __FUNCTION__, __LINE__);
            }
            dlclose(dllib);
        }
        else {
            INT_ERROR("%s:%d dsGetAllmStatus  Opening RDK_DSHAL_NAME [%s] failed %s\r\n",
                   __FUNCTION__, __LINE__, RDK_DSHAL_NAME, dlerror());
        }
    }
    if (0 != dsGetAllmStatusFunc) {
        eRet = dsGetAllmStatusFunc (iHdmiPort, allmStatus);
        INT_INFO("[srv] %s: dsGetAllmStatusFunc eRet: %d \r\n", __FUNCTION__, eRet);
    }
    else {
        INT_INFO("%s:  dsGetAllmStatusFunc = %p\n", __FUNCTION__, dsGetAllmStatusFunc);
    }
    return eRet;
}

static dsError_t getSupportedGameFeaturesList (dsSupportedGameFeatureList_t *fList) {
    if (!s_aidlPorts.empty()) {
        sp<IHDMIInput> hi;
        {
            std::lock_guard<std::mutex> lk(s_aidlMutex);
            auto it = s_aidlPorts.begin();
            if (it == s_aidlPorts.end() || !it->second.hdmiInput) return dsERR_INVALID_PARAM;
            hi = it->second.hdmiInput;
        }
        Capabilities caps;
        if (!hi->getCapabilities(&caps).isOk()) {
            INT_ERROR("[srv-aidl] getCapabilities failed\n");
            return dsERR_GENERAL;
        }
        fList->gameFeatureCount = 0;
        if (caps.supportsALLM) {
            strncpy(fList->gameFeatureList[fList->gameFeatureCount++], "allm",
                sizeof(fList->gameFeatureList[0]) - 1);
        }
        if (caps.supportsVRR) {
            strncpy(fList->gameFeatureList[fList->gameFeatureCount++], "vrr_hdmi",
                sizeof(fList->gameFeatureList[0]) - 1);
        }
        if (caps.supportsFreeSync) {
            strncpy(fList->gameFeatureList[fList->gameFeatureCount++], "freeSync",
                sizeof(fList->gameFeatureList[0]) - 1);
        }
        INT_INFO("[srv-aidl] getSupportedGameFeaturesList count=%d\n", fList->gameFeatureCount);
        return dsERR_NONE;
    }
    dsError_t eRet = dsERR_GENERAL;
    typedef dsError_t (*dsGetSupportedGameFeaturesList_t)(dsSupportedGameFeatureList_t *fList);
    static dsGetSupportedGameFeaturesList_t dsGetSupportedGameFeaturesListFunc = 0;
    if (dsGetSupportedGameFeaturesListFunc == 0) {
       void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
       if (dllib) {
            dsGetSupportedGameFeaturesListFunc = (dsGetSupportedGameFeaturesList_t) dlsym(dllib, "dsGetSupportedGameFeaturesList");
            if(dsGetSupportedGameFeaturesListFunc == 0) {
                INT_INFO("%s:%d dsGetSupportedGameFeaturesList (int) is not defined %s\r\n", __FUNCTION__, __LINE__, dlerror());
            }
            else {
                INT_DEBUG("%s:%d dsGetSupportedGameFeaturesList loaded\r\n", __FUNCTION__, __LINE__);
            }
            dlclose(dllib);
        }
        else {
            INT_ERROR("%s:%d dsGetSupportedGameFeaturesList  Opening RDK_DSHAL_NAME [%s] failed %s\r\n",
                   __FUNCTION__, __LINE__, RDK_DSHAL_NAME, dlerror());
        }
    }
    if (0 != dsGetSupportedGameFeaturesListFunc) {
        eRet = dsGetSupportedGameFeaturesListFunc (fList);
        INT_INFO("[srv] %s: dsGetSupportedGameFeaturesListFunc eRet: %d \r\n", __FUNCTION__, eRet);
    }
    else {
        INT_INFO("%s:  dsGetSupportedGameFeaturesListFunc = %p\n", __FUNCTION__, dsGetSupportedGameFeaturesListFunc);
    }
    return eRet;
}

static dsError_t getAVLatency_hal (int *audio_latency, int *video_latency)
{
    if (!s_aidlPorts.empty()) {
        sp<IPlaneControl> pc = getAidlPlaneCtrl();
        if (!pc) {
            INT_ERROR("[srv-aidl] IPlaneControl unavailable\n");
            *audio_latency = 0; *video_latency = 0;
            return dsERR_NONE;
        }
        std::vector<::com::rdk::hal::planecontrol::PlaneCapabilities> planeCaps;
        bool ok = false;
        if (!pc->getCapabilities(&planeCaps, &ok).isOk() || !ok || planeCaps.empty()) {
            INT_ERROR("[srv-aidl] PlaneControl getCapabilities failed\n");
            *audio_latency = 0; *video_latency = 0;
            return dsERR_NONE;
        }
        int latencyMs = planeCaps[0].vsyncDisplayLatency * 16;
        *video_latency = latencyMs;
        *audio_latency = 0;
        INT_INFO("[srv-aidl] getAVLatency audio=0 video=%d\n", *video_latency);
        return dsERR_NONE;
    }
   dsError_t eRet = dsERR_GENERAL;
    typedef dsError_t (*dsGetAVLatency_t)(int *audio_latency, int *video_latency);
    static dsGetAVLatency_t dsGetAVLatencyFunc = 0;
    if (dsGetAVLatencyFunc == 0) {
       void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
       if (dllib) {
            dsGetAVLatencyFunc = (dsGetAVLatency_t) dlsym(dllib, "dsGetAVLatency");
            if(dsGetAVLatencyFunc == 0) {
                INT_INFO("%s:%d dsGetAVLatency (int) is not defined %s\r\n", __FUNCTION__, __LINE__, dlerror());
            }
            else {
                INT_DEBUG("%s:%d dsGetAVLatencyFunc loaded\r\n", __FUNCTION__, __LINE__);
            }
            dlclose(dllib);
        }
        else {
            INT_INFO("%s:%d dsGetAVLatency  Opening RDK_DSHAL_NAME [%s] failed %s\r\n",
                   __FUNCTION__, __LINE__, RDK_DSHAL_NAME, dlerror());
        }
    }
    if (0 != dsGetAVLatencyFunc) {
        eRet = dsGetAVLatencyFunc (audio_latency, video_latency);
        INT_INFO("[srv] %s: dsGetAVLatencyFunc eRet: %d \r\n", __FUNCTION__, eRet);
    }
    else {
        INT_INFO("%s:  dsGetAVLatencyFunc = %p\n", __FUNCTION__, dsGetAVLatencyFunc);
               }
       return eRet;
}

static dsError_t getHdmiVersion (dsHdmiInPort_t iHdmiPort, dsHdmiMaxCapabilityVersion_t  *capversion) {
    if (!s_aidlPorts.empty()) {
        sp<IHDMIInput> hi;
        {
            std::lock_guard<std::mutex> lk(s_aidlMutex);
            auto it = s_aidlPorts.find((int)iHdmiPort);
            if (it == s_aidlPorts.end() || !it->second.hdmiInput) return dsERR_INVALID_PARAM;
            hi = it->second.hdmiInput;
        }
        Capabilities caps;
        if (!hi->getCapabilities(&caps).isOk()) {
            INT_ERROR("[srv-aidl] getCapabilities failed for port %d\n", (int)iHdmiPort);
            return dsERR_GENERAL;
        }
        *capversion = HDMI_COMPATIBILITY_VERSION_14;
        for (const auto& v : caps.supportedVersions) {
            if (v == HDMIVersion::HDMI_2_1 && *capversion < HDMI_COMPATIBILITY_VERSION_21) {
                *capversion = HDMI_COMPATIBILITY_VERSION_21;
            } else if (v == HDMIVersion::HDMI_2_0 && *capversion < HDMI_COMPATIBILITY_VERSION_20) {
                *capversion = HDMI_COMPATIBILITY_VERSION_20;
            }
        }
        INT_INFO("[srv-aidl] getHdmiVersion port %d version=%d\n", (int)iHdmiPort, *capversion);
        return dsERR_NONE;
    }
    dsError_t eRet = dsERR_GENERAL;
    typedef dsError_t (*dsGetHdmiVersion_t)(dsHdmiInPort_t iHdmiPort, dsHdmiMaxCapabilityVersion_t  *capversion);
    static dsGetHdmiVersion_t dsGetHdmiVersionFunc = 0;
    if (dsGetHdmiVersionFunc == 0) {
       void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
       if (dllib) {
            dsGetHdmiVersionFunc = (dsGetHdmiVersion_t) dlsym(dllib, "dsGetHdmiVersion");
            if(dsGetHdmiVersionFunc == 0) {
                INT_INFO("%s:%d dsGetHdmiVersion (int) is not defined %s\r\n", __FUNCTION__,__LINE__, dlerror());
                eRet = dsERR_GENERAL;
            }
            else {
                INT_INFO("%s:%d dsGetHdmiVersionFunc loaded\r\n", __FUNCTION__,__LINE__);
            }
            dlclose(dllib);
        }
        else {
            INT_ERROR("%s:%d dsGetHdmiVersion  Opening RDK_DSHAL_NAME [%s] failed %s\r\n",
                   __FUNCTION__,__LINE__, RDK_DSHAL_NAME, dlerror());
            eRet = dsERR_GENERAL;
        }
    }
    if (0 != dsGetHdmiVersionFunc) {
        eRet = dsGetHdmiVersionFunc (iHdmiPort, capversion);
        INT_INFO("[srv] %s: dsGetHdmiVersionFunc eRet: %d \r\n", __FUNCTION__,eRet);
    }
    return eRet;
}

IARM_Result_t dsHdmiInMgr_init()
{
    _dsHdmiInInit(NULL);

    IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsHdmiInInit, _dsHdmiInInit);
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t dsHdmiInMgr_term()
{
    _dsHdmiInTerm(NULL);
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsHdmiInInit(void *arg)
{
    IARM_BUS_Lock(lock);
    INT_INFO("%s:%d ---> m_isInitialized=%d, m_isPlatInitialized=%d \n",
                   __PRETTY_FUNCTION__,__LINE__, m_isInitialized, m_isPlatInitialized);

    getDynamicAutoLatencyConfig();

    if (PROFILE_TV == profileType)
    {
        INT_INFO("[%d][%s]: its TV Profile\r\n", __LINE__, __FUNCTION__);
        if (!m_isPlatInitialized)
        {
            aidlHdmiInInit();
        }
        m_isPlatInitialized++;
    }

    if (!m_isInitialized)
    {
        if (PROFILE_TV == profileType && s_aidlPorts.empty())
        {
            INT_INFO("[%d][%s]: its TV Profile (HAL path)\r\n", __LINE__, __FUNCTION__);
            dsHdmiInRegisterConnectCB(_dsHdmiInConnectCB);

            typedef dsError_t (*dsHdmiInRegisterSignalChangeCB_t)(dsHdmiInSignalChangeCB_t CBFunc);
            static dsHdmiInRegisterSignalChangeCB_t signalChangeCBFunc = 0;
            if (signalChangeCBFunc == 0) {
                void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
                if (dllib) {
                    signalChangeCBFunc = (dsHdmiInRegisterSignalChangeCB_t) dlsym(dllib, "dsHdmiInRegisterSignalChangeCB");
                    if(signalChangeCBFunc == 0) {
                        INT_INFO("dsHdmiInRegisterSignalChangeCB(dsHdmiInSignalChangeCB_t) is not defined\r\n");
                    }
                    dlclose(dllib);
                }
                else {
                    INT_ERROR("Opening RDK_DSHAL_NAME [%s] failed\r\n", RDK_DSHAL_NAME);
                }
            }
    
            if(signalChangeCBFunc) {
                 signalChangeCBFunc(_dsHdmiInSignalChangeCB);
            }
    
            typedef dsError_t (*dsHdmiInRegisterStatusChangeCB_t)(dsHdmiInStatusChangeCB_t CBFunc);
            static dsHdmiInRegisterStatusChangeCB_t statusChangeCBFunc = 0;
            if (statusChangeCBFunc == 0) {
                void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
                if (dllib) {
                    statusChangeCBFunc = (dsHdmiInRegisterStatusChangeCB_t) dlsym(dllib, "dsHdmiInRegisterStatusChangeCB");
                    if(statusChangeCBFunc == 0) {
                        INT_INFO("dsHdmiInRegisterStatusChangeCB(dsHdmiInStatusChangeCB_t) is not defined\r\n");
                    }
                    dlclose(dllib);
                }
                else {
                    INT_ERROR("Opening RDK_DSHAL_NAME [%s] failed\r\n", RDK_DSHAL_NAME);
                }
            }
    
            if(statusChangeCBFunc) {
                 statusChangeCBFunc(_dsHdmiInStatusChangeCB);
            }
    
            typedef dsError_t (*dsHdmiInRegisterVideoModeUpdateCB_t)(dsHdmiInVideoModeUpdateCB_t CBFunc);
            static dsHdmiInRegisterVideoModeUpdateCB_t videoModeUpdateCBFunc = 0;
            if (videoModeUpdateCBFunc == 0) {
                void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
                if (dllib) {
                    videoModeUpdateCBFunc = (dsHdmiInRegisterVideoModeUpdateCB_t) dlsym(dllib, "dsHdmiInRegisterVideoModeUpdateCB");
                    if(statusChangeCBFunc == 0) {
                        INT_INFO("dsHdmiInRegisterStatusChangeCB(dsHdmiInStatusChangeCB_t) is not defined\r\n");
                    }
                    dlclose(dllib);
                }
                else {
                    INT_ERROR("Opening RDK_DSHAL_NAME [%s] failed\r\n", RDK_DSHAL_NAME);
                }
            }
    
            if(videoModeUpdateCBFunc) {
                 videoModeUpdateCBFunc(_dsHdmiInVideoModeUpdateCB);
            }
    
            typedef dsError_t (*dsHdmiInRegisterAllmChangeCB_t)(dsHdmiInAllmChangeCB_t CBFunc);
            static dsHdmiInRegisterAllmChangeCB_t allmChangeCBFunc = 0;
            if (allmChangeCBFunc == 0) {
                void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
                if (dllib) {
                    allmChangeCBFunc = (dsHdmiInRegisterAllmChangeCB_t) dlsym(dllib, "dsHdmiInRegisterAllmChangeCB");
                    if(statusChangeCBFunc == 0) {
                        INT_INFO("dsHdmiInRegisterAllmChangeCB(dsHdmiInAllmChangeCB_t) is not defined\r\n");
                    }
                    dlclose(dllib);
                }
                else {
                    INT_ERROR("Opening RDK_DSHAL_NAME [%s] failed\r\n", RDK_DSHAL_NAME);
                }
            }
    
            if(allmChangeCBFunc) {
                allmChangeCBFunc(_dsHdmiInAllmChangeCB);
            }
	    typedef dsError_t (*dsHdmiInRegisterVRRChangeCB_t)(dsHdmiInVRRChangeCB_t CBFunc);
            static dsHdmiInRegisterVRRChangeCB_t vrrChangeCBFunc = 0;
            if (vrrChangeCBFunc == 0) {
                void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
                if (dllib) {
                    vrrChangeCBFunc = (dsHdmiInRegisterVRRChangeCB_t) dlsym(dllib, "dsHdmiInRegisterVRRChangeCB");
                    if(vrrChangeCBFunc == 0) {
                        INT_INFO("dsHdmiInRegisterVRRChangeCB(dsHdmiInVRRChangeCB_t) is not defined\r\n");
                    }
                    dlclose(dllib);
                }
                else {
                    INT_ERROR("Opening RDK_DSHAL_NAME [%s] failed\r\n", RDK_DSHAL_NAME);
                }
            }

            if(vrrChangeCBFunc) {
                vrrChangeCBFunc(_dsHdmiInVRRChangeCB);
            }
            typedef dsError_t (*dsHdmiInRegisterAviContentTypeChangeCB_t)(dsHdmiInAviContentTypeChangeCB_t CBFunc);
            static dsHdmiInRegisterAviContentTypeChangeCB_t contentTypeChangeCBFunc = 0;
            if (contentTypeChangeCBFunc == 0) {
                void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
                if (dllib) {
                    contentTypeChangeCBFunc = (dsHdmiInRegisterAviContentTypeChangeCB_t) dlsym(dllib, "dsHdmiInRegisterAviContentTypeChangeCB");
                    if(contentTypeChangeCBFunc == 0) {
                        INT_INFO("dsHdmiInRegisterContentTypeChangeCB(dsHdmiInContentTypeChangeCB_t) is not defined\r\n");
                    }
                    dlclose(dllib);
                }
                else {
                    INT_ERROR("Opening RDK_DSHAL_NAME [%s] failed\r\n", RDK_DSHAL_NAME);
                }
            }
    
            if(contentTypeChangeCBFunc) {
                contentTypeChangeCBFunc(_dsHdmiInAviContentTypeChangeCB);
            }
    
    	typedef dsError_t (*dsHdmiInRegisterAVLatencyChangeCB_t)(dsAVLatencyChangeCB_t CBFunc);
            static dsHdmiInRegisterAVLatencyChangeCB_t AVLatencyCBFunc = 0;
            if (AVLatencyCBFunc == 0) {
                void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
                if (dllib) {
                    AVLatencyCBFunc = (dsHdmiInRegisterAVLatencyChangeCB_t) dlsym(dllib, "dsHdmiInRegisterAVLatencyChangeCB");
                    if(AVLatencyCBFunc == 0) {
                        INT_INFO("dsRegisterAVLatencyChangeCB(dsAVLatencyChangeCB_t) is not defined\r\n");
                    }
                    dlclose(dllib);
                }
                else {
                    INT_ERROR("Opening RDK_DSHAL_NAME [%s] failed\r\n", RDK_DSHAL_NAME);
                }
            }
    
            if(AVLatencyCBFunc && isDalsEnabled) {
                AVLatencyCBFunc(_dsHdmiInAVLatencyChangeCB);
           }
    
        }

        IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsHdmiInTerm,                  _dsHdmiInTerm);
        IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsHdmiInGetNumberOfInputs,     _dsHdmiInGetNumberOfInputs);
        IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsHdmiInGetStatus,             _dsHdmiInGetStatus);
        IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsHdmiInSelectPort,            _dsHdmiInSelectPort);
        IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsHdmiInScaleVideo,            _dsHdmiInScaleVideo);
        IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsHdmiInSelectZoomMode,        _dsHdmiInSelectZoomMode);
        IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsHdmiInGetCurrentVideoMode,   _dsHdmiInGetCurrentVideoMode);
        IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsGetEDIDBytesInfo,              _dsGetEDIDBytesInfo);
        IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsGetHDMISPDInfo,              _dsGetHDMISPDInfo);
        IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsSetEdidVersion,              _dsSetEdidVersion);
        IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsGetEdidVersion,              _dsGetEdidVersion);
        IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsGetAllmStatus,               _dsGetAllmStatus);
        IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsGetSupportedGameFeaturesList,_dsGetSupportedGameFeaturesList);
	IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsGetAVLatency,  _dsGetAVLatency);
        IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsSetEdid2AllmSupport,  _dsSetEdid2AllmSupport);
        IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsGetEdid2AllmSupport,  _dsGetEdid2AllmSupport);
        IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsSetVRRSupport,  _dsSetVRRSupport);
        IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsGetVRRSupport,  _dsGetVRRSupport);
        IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsGetVRRStatus,               _dsGetVRRStatus);
	IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_dsGetHdmiVersion,  _dsGetHdmiVersion);

        int itr = 0;
        bool isARCCapable = false;
       
        if (!s_aidlPorts.empty()) {
            noOfSupportedHdmiInputs = s_aidlPortCount;
        } else if (PROFILE_TV == profileType) {
            dsHdmiInGetNumberOfInputs(&noOfSupportedHdmiInputs);
        }
        INT_INFO("Number of Inputs:%d \n",noOfSupportedHdmiInputs);
      
        for (itr = 0; itr < noOfSupportedHdmiInputs; itr++) {
            isARCCapable = false;
            isHdmiARCPort (itr, &isARCCapable);
            hdmiInCap_gs.isPortArcCapable[itr] = isARCCapable; 
        }

        // Getting the edidallmEnable value from persistence upon bootup
        std::string _EdidAllmSupport("TRUE");

        for (int i = 0 ; i < noOfSupportedHdmiInputs; i++)
        {
            std::string port_edidAllmSupport = "HDMI"+std::to_string(i)+".edidallmEnable";
            try {
                _EdidAllmSupport = device::HostPersistence::getInstance().getProperty(port_edidAllmSupport);
                if(_EdidAllmSupport == "TRUE")
                    m_edidallmsupport[i] = true;
                else
                    m_edidallmsupport[i] = false;
		            }
            catch(...) {
                try {
                    INT_INFO("Port %d: Exception in Getting the HDMI%d EDID allm support from persistence storage. Try system default...\r\n", i,i);
                    _EdidAllmSupport = device::HostPersistence::getInstance().getDefaultProperty(port_edidAllmSupport);
                    if(_EdidAllmSupport == "TRUE")
                        m_edidallmsupport[i] = true;
                    else
                        m_edidallmsupport[i] = false;                }
                catch(...) {
                    INT_INFO("Port %d: Exception in Getting the HDMI%d EDID allm support from system default..... \r\n", i,i);
                    m_edidallmsupport[i] = true;
                }
            }
        }

        std::string _EdidVersion("1");
              for (int i = 0 ; i < noOfSupportedHdmiInputs; i++)
        {
            std::string port_edidVer = "HDMI"+std::to_string(i)+".edidversion";
            try {
                _EdidVersion = device::HostPersistence::getInstance().getProperty(port_edidVer);
                m_edidversion[i] = static_cast<tv_hdmi_edid_version_t>(atoi (_EdidVersion.c_str()));
            }
            catch(...) {
                try {
                    INT_INFO("Port %d: Exception in Getting the HDMI%d EDID version from persistence storage. Try system default...\r\n", i,i);
                    _EdidVersion = device::HostPersistence::getInstance().getDefaultProperty(port_edidVer);
                    m_edidversion[i] = static_cast<tv_hdmi_edid_version_t>(atoi (_EdidVersion.c_str()));
                }
                catch(...) {
                    INT_INFO("Port %d: Exception in Getting the HDMI%d EDID version from system default..... \r\n", i,i);
                    m_edidversion[i] = HDMI_EDID_VER_20;
                }
            }
        }
       
        // Getting the edidvrrEnable value from persistence upon bootup
        std::string _VRRSupport("TRUE");

	for (int i = 0 ; i < noOfSupportedHdmiInputs; i++)
        {
            std::string port_vrrSupport = "HDMI"+std::to_string(i)+".vrrEnable";
            try {
                _VRRSupport = device::HostPersistence::getInstance().getProperty(port_vrrSupport);
                if(_VRRSupport == "TRUE")
                    m_vrrsupport[i] = true;
                else
                    m_vrrsupport[i] = false;
            }
            catch(...) {
                try {
                    INT_INFO("Port %d: Exception in Getting the HDMI%d VRR support from persistence storage. Try system default...\r\n", i,i);
                    _VRRSupport = device::HostPersistence::getInstance().getDefaultProperty(port_vrrSupport);
                    if(_VRRSupport == "TRUE")
                        m_vrrsupport[i] = true;
                    else
                        m_vrrsupport[i] = false;                }
                catch(...) {
                    INT_INFO("Port %d: Exception in Getting the HDMI%d VRR support from system default..... \r\n", i,i);
                    m_vrrsupport[i] = false;
                }
            }
        }
	
        for (itr = 0; itr < noOfSupportedHdmiInputs; itr++) {
            if (getVRRSupport(static_cast<dsHdmiInPort_t>(itr), &m_hdmiPortVrrCaps[itr]) >= 0) {
                INT_INFO("Port HDMI%d: VRR capability : %d\n", itr, m_hdmiPortVrrCaps[itr]);
            }
            if (setEdidVersion (static_cast<dsHdmiInPort_t>(itr), m_edidversion[itr]) >= 0) {
                INT_INFO("Port HDMI%d: Initialized EDID Version : %d\n", itr, m_edidversion[itr]);
            }
        }
        m_isInitialized = 1;
    }

    IARM_BUS_Unlock(lock);

    return IARM_RESULT_SUCCESS;
}


IARM_Result_t _dsHdmiInTerm(void *arg)
{
    _DEBUG_ENTER();

    IARM_BUS_Lock(lock);
    if (PROFILE_TV == profileType)
    {
        INT_INFO("[%d][%s]: its TV Profile\r\n", __LINE__, __FUNCTION__);
        if (m_isPlatInitialized)
        {
            m_isPlatInitialized--;
            if (!m_isPlatInitialized)
            {
                if (!s_aidlPorts.empty()) {
                    aidlHdmiInTerm();
                } else {
                    dsHdmiInTerm();
                }
            }
        }
    }
    IARM_BUS_Unlock(lock);

    return IARM_RESULT_SUCCESS;
}


IARM_Result_t _dsHdmiInGetNumberOfInputs(void *arg)
{
    _DEBUG_ENTER();

    dsHdmiInGetNumberOfInputsParam_t *param = (dsHdmiInGetNumberOfInputsParam_t *)arg;

    IARM_BUS_Lock(lock);

    if (!s_aidlPorts.empty()) {
        param->numHdmiInputs = s_aidlPortCount;
        param->result = dsERR_NONE;
    } else if (PROFILE_TV == profileType) {
        INT_INFO("[%d][%s]: its TV Profile\r\n", __LINE__, __FUNCTION__);
        param->result = dsHdmiInGetNumberOfInputs(&param->numHdmiInputs);
    } else {
        INT_INFO("[%d][%s]: its Other Profile\r\n", __LINE__, __FUNCTION__);
        param->result = dsERR_GENERAL;
    }

    IARM_BUS_Unlock(lock);

    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsHdmiInGetStatus(void *arg)
{
    _DEBUG_ENTER();

    dsHdmiInGetStatusParam_t *param= (dsHdmiInGetStatusParam_t *)arg;

    IARM_BUS_Lock(lock);

    if (!s_aidlPorts.empty()) {
        memset(&param->status, 0, sizeof(param->status));
        {
            std::lock_guard<std::mutex> lk(s_aidlMutex);
            param->status.activePort  = (dsHdmiInPort_t)s_aidlActivePort;
            param->status.isPresented = (s_aidlActivePort >= 0);
            for (auto& kv : s_aidlPorts) {
                int idx = kv.first;
                if (idx >= 0 && idx < dsHDMI_IN_PORT_MAX)
                    param->status.isPortConnected[idx] = kv.second.connected;
            }
        }
        param->result = dsERR_NONE;
    } else if (PROFILE_TV == profileType) {
        INT_INFO("[%d][%s]: its TV Profile\r\n", __LINE__, __FUNCTION__);
        param->result = dsHdmiInGetStatus(&param->status);
    } else {
        INT_INFO("[%d][%s]: its Other Profile\r\n", __LINE__, __FUNCTION__);
        param->result = dsERR_GENERAL;
    }

    IARM_BUS_Unlock(lock);

    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsHdmiInSelectPort(void *arg)
{
    _DEBUG_ENTER();

    dsHdmiInSelectPortParam_t *param = (dsHdmiInSelectPortParam_t *)arg;

    IARM_BUS_Lock(lock);

    if (PROFILE_TV == profileType)
    {
        INT_INFO("[%d][%s]: its TV Profile\r\n", __LINE__, __FUNCTION__);
        param->result = dsHdmiInSelectPort(param->port,param->requestAudioMix, param->videoPlaneType,param->topMostPlane);
    }
    else
    {
        INT_INFO("[%d][%s]: its Other Profile\r\n", __LINE__, __FUNCTION__);
        param->result = dsERR_GENERAL;
    }

    IARM_BUS_Unlock(lock);

    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsHdmiInScaleVideo(void *arg)
{
    _DEBUG_ENTER();

    IARM_BUS_Lock(lock);
    dsHdmiInScaleVideoParam_t *param = (dsHdmiInScaleVideoParam_t *)arg;

    if (!s_aidlPorts.empty()) {
        sp<IPlaneControl> pc = getAidlPlaneCtrl();
        if (!pc) {
            param->result = dsERR_GENERAL;
        } else {
            std::vector<::com::rdk::hal::planecontrol::PropertyKVPair> kvList;
            ::com::rdk::hal::planecontrol::PropertyKVPair kv;
            kv.property = ::com::rdk::hal::planecontrol::Property::X;
            kv.propertyValue.set<::com::rdk::hal::PropertyValue::Tag::intValue>(param->videoRect.x);
            kvList.push_back(kv);
            kv.property = ::com::rdk::hal::planecontrol::Property::Y;
            kv.propertyValue.set<::com::rdk::hal::PropertyValue::Tag::intValue>(param->videoRect.y);
            kvList.push_back(kv);
            kv.property = ::com::rdk::hal::planecontrol::Property::WIDTH;
            kv.propertyValue.set<::com::rdk::hal::PropertyValue::Tag::intValue>(param->videoRect.width);
            kvList.push_back(kv);
            kv.property = ::com::rdk::hal::planecontrol::Property::HEIGHT;
            kv.propertyValue.set<::com::rdk::hal::PropertyValue::Tag::intValue>(param->videoRect.height);
            kvList.push_back(kv);
            bool result = false;
            ::android::binder::Status st = pc->setPropertyMultiAtomic(
                HDMI_IN_PRIMARY_PLANE_INDEX, kvList, &result);
            param->result = (st.isOk() && result) ? dsERR_NONE : dsERR_GENERAL;
        }
    } else if (PROFILE_TV == profileType) {
        INT_INFO("[%d][%s]: its TV Profile\r\n", __LINE__, __FUNCTION__);
        param->result = dsHdmiInScaleVideo(param->videoRect.x, param->videoRect.y, param->videoRect.width, param->videoRect.height);
    } else {
        INT_INFO("[%d][%s]: its Other Profile\r\n", __LINE__, __FUNCTION__);
        param->result = dsERR_GENERAL;
    }

    IARM_BUS_Unlock(lock);

    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsHdmiInSelectZoomMode(void *arg)
{
    _DEBUG_ENTER();
    IARM_BUS_Lock(lock);
    dsHdmiInSelectZoomModeParam_t *param = (dsHdmiInSelectZoomModeParam_t *)arg;

    if (!s_aidlPorts.empty()) {
        sp<IPlaneControl> pc = getAidlPlaneCtrl();
        if (!pc) {
            param->result = dsERR_GENERAL;
        } else {
            ::com::rdk::hal::PropertyValue aspectRatioVal;
            aspectRatioVal.set<::com::rdk::hal::PropertyValue::Tag::intValue>(
                (int32_t)param->zoomMode);
            bool result = false;
            ::android::binder::Status st = pc->setProperty(
                HDMI_IN_PRIMARY_PLANE_INDEX,
                ::com::rdk::hal::planecontrol::Property::ASPECT_RATIO,
                aspectRatioVal, &result);
            param->result = (st.isOk() && result) ? dsERR_NONE : dsERR_GENERAL;
        }
    } else if (PROFILE_TV == profileType) {
        INT_INFO("[%d][%s]: its TV Profile\r\n", __LINE__, __FUNCTION__);
        param->result = dsHdmiInSelectZoomMode(param->zoomMode);
    } else {
        INT_INFO("[%d][%s]: its Other Profile\r\n", __LINE__, __FUNCTION__);
        param->result = dsERR_GENERAL;
    }

    IARM_BUS_Unlock(lock);

    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsHdmiInGetCurrentVideoMode(void *arg)
{
    _DEBUG_ENTER();

    dsHdmiInGetResolutionParam_t *param = (dsHdmiInGetResolutionParam_t *)arg;

    IARM_BUS_Lock(lock);

    if (!s_aidlPorts.empty()) {
        int vic = 0;
        {
            std::lock_guard<std::mutex> lk(s_aidlMutex);
            if (s_aidlActivePort >= 0) {
                auto it = s_aidlPorts.find(s_aidlActivePort);
                if (it != s_aidlPorts.end()) vic = it->second.lastVIC;
            }
        }
        vicToResolutionObj_srv(vic, param->resolution);
        param->result = dsERR_NONE;
    } else if (PROFILE_TV == profileType) {
        INT_INFO("[%d][%s]: its TV Profile\r\n", __LINE__, __FUNCTION__);
        param->result = dsHdmiInGetCurrentVideoMode(&param->resolution);
    } else {
        INT_INFO("[%d][%s]: its Other Profile\r\n", __LINE__, __FUNCTION__);
        param->result = dsERR_GENERAL;
    }

    IARM_BUS_Unlock(lock);

    return IARM_RESULT_SUCCESS;
}

void _dsHdmiInConnectCB(dsHdmiInPort_t port, bool isPortConnected)
{
    IARM_Bus_DSMgr_EventData_t hdmi_in_hpd_eventData;
 
    INT_INFO("%s:%d - HDMI In hotplug update!!!!!!..Port: %d, isPort: %d\r\n",__PRETTY_FUNCTION__,__LINE__, port, isPortConnected);
    hdmi_in_hpd_eventData.data.hdmi_in_connect.port = port;
    hdmi_in_hpd_eventData.data.hdmi_in_connect.isPortConnected = isPortConnected;
			
    IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
	                        (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_HDMI_IN_HOTPLUG,
	                        (void *)&hdmi_in_hpd_eventData, 
	                        sizeof(hdmi_in_hpd_eventData));
           
}

void _dsHdmiInSignalChangeCB(dsHdmiInPort_t port, dsHdmiInSignalStatus_t sigStatus)
{
    IARM_Bus_DSMgr_EventData_t hdmi_in_sigStatus_eventData;

    INT_INFO("%s:%d - HDMI In signal status change update!!!!!! Port: %d, Signal Status: %d\r\n", __PRETTY_FUNCTION__,__LINE__,port, sigStatus);
    hdmi_in_sigStatus_eventData.data.hdmi_in_sig_status.port = port;
    hdmi_in_sigStatus_eventData.data.hdmi_in_sig_status.status = sigStatus;

    IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
			        (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_HDMI_IN_SIGNAL_STATUS,
			        (void *)&hdmi_in_sigStatus_eventData,
			        sizeof(hdmi_in_sigStatus_eventData));

}

void _dsHdmiInStatusChangeCB(dsHdmiInStatus_t inputStatus)
{
    IARM_Bus_DSMgr_EventData_t hdmi_in_status_eventData;

    INT_INFO("%s:%d - HDMI In status change update!!!!!! Port: %d, isPresented: %d\r\n", __PRETTY_FUNCTION__,__LINE__, inputStatus.activePort, inputStatus.isPresented);
    hdmi_in_status_eventData.data.hdmi_in_status.port = inputStatus.activePort;
    hdmi_in_status_eventData.data.hdmi_in_status.isPresented = inputStatus.isPresented;

    IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_HDMI_IN_STATUS,
                                (void *)&hdmi_in_status_eventData,
[O                                sizeof(hdmi_in_status_eventData));

}

void _dsHdmiInVideoModeUpdateCB(dsHdmiInPort_t port, dsVideoPortResolution_t videoResolution)
{
    IARM_Bus_DSMgr_EventData_t hdmi_in_videoMode_eventData;

    INT_INFO("%s:%d - HDMI In video mode info  update, Port: %d, Pixel Resolution: %d, Interlaced: %d, Frame Rate: %d \n", __PRETTY_FUNCTION__,__LINE__,port, videoResolution.pixelResolution, videoResolution.interlaced, videoResolution.frameRate);
    hdmi_in_videoMode_eventData.data.hdmi_in_video_mode.port = port;
    hdmi_in_videoMode_eventData.data.hdmi_in_video_mode.resolution.pixelResolution = videoResolution.pixelResolution;
    hdmi_in_videoMode_eventData.data.hdmi_in_video_mode.resolution.interlaced = videoResolution.interlaced;
    hdmi_in_videoMode_eventData.data.hdmi_in_video_mode.resolution.frameRate = videoResolution.frameRate;


    IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_HDMI_IN_VIDEO_MODE_UPDATE,
                                (void *)&hdmi_in_videoMode_eventData,
                                sizeof(hdmi_in_videoMode_eventData));

}

void _dsHdmiInAllmChangeCB(dsHdmiInPort_t port, bool allm_mode)
{
    IARM_Bus_DSMgr_EventData_t hdmi_in_allmMode_eventData;

    INT_INFO("%s:%d - HDMI In ALLM Mode update!!!!!! Port: %d, ALLM Mode: %d\r\n", __FUNCTION__,__LINE__,port, allm_mode);
    hdmi_in_allmMode_eventData.data.hdmi_in_allm_mode.port = port;
    hdmi_in_allmMode_eventData.data.hdmi_in_allm_mode.allm_mode = allm_mode;

    IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_HDMI_IN_ALLM_STATUS,
                                (void *)&hdmi_in_allmMode_eventData,
                                sizeof(hdmi_in_allmMode_eventData));

}

void _dsHdmiInVRRChangeCB(dsHdmiInPort_t port, dsVRRType_t vrr_type)
{
    IARM_Bus_DSMgr_EventData_t hdmi_in_vrrMode_eventData;

    INT_INFO("%s:%d - HDMI In VRR Mode update!!!!!! Port: %d, VRR TYPE: %d\r\n", __FUNCTION__,__LINE__,port, vrr_type);
    hdmi_in_vrrMode_eventData.data.hdmi_in_vrr_mode.port = port;
    hdmi_in_vrrMode_eventData.data.hdmi_in_vrr_mode.vrr_type = vrr_type;

    IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_HDMI_IN_VRR_STATUS,
                                (void *)&hdmi_in_vrrMode_eventData,
                                sizeof(hdmi_in_vrrMode_eventData));

}

void _dsHdmiInAviContentTypeChangeCB(dsHdmiInPort_t port, dsAviContentType_t avi_content_type)
{
    IARM_Bus_DSMgr_EventData_t hdmi_in_contentType_eventData;

    INT_INFO("%s:%d - HDMI In Content Type update!!!!!! Port: %d, content type: %d\r\n", __FUNCTION__,__LINE__,port, avi_content_type);
    hdmi_in_contentType_eventData.data.hdmi_in_content_type.port = port;
    hdmi_in_contentType_eventData.data.hdmi_in_content_type.aviContentType = avi_content_type;

    IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_HDMI_IN_AVI_CONTENT_TYPE,
                                (void *)&hdmi_in_contentType_eventData,
                                sizeof(hdmi_in_contentType_eventData));
}

void _dsHdmiInAVLatencyChangeCB(int audio_latency, int video_latency)
{
    IARM_Bus_DSMgr_EventData_t hdmi_in_av_latency_eventData;

    hdmi_in_av_latency_eventData.data.hdmi_in_av_latency.audio_output_delay = audio_latency;
    hdmi_in_av_latency_eventData.data.hdmi_in_av_latency.video_latency = video_latency;
    INT_INFO("%s:%d - HDMI In AV Latency update!!!!!! audio_latency: %d, video latency: %d\r\n", __FUNCTION__,__LINE__,audio_latency,video_latency);
    IARM_Bus_BroadcastEvent(IARM_BUS_DSMGR_NAME,
                                (IARM_EventId_t)IARM_BUS_DSMGR_EVENT_HDMI_IN_AV_LATENCY,
                                (void *)&hdmi_in_av_latency_eventData,
                                sizeof(hdmi_in_av_latency_eventData));

}

IARM_Result_t _dsGetEDIDBytesInfo (void *arg) 
{
    errno_t rc = -1;
    dsError_t eRet = dsERR_GENERAL;

    dsGetEDIDBytesInfoParam_t *param = (dsGetEDIDBytesInfoParam_t *) arg;
    memset (param->edid, '\0', MAX_EDID_BYTES_LEN);
    unsigned char edidArg[MAX_EDID_BYTES_LEN] = {0};
    IARM_BUS_Lock(lock);
    eRet = getEDIDBytesInfo (param->iHdmiPort, edidArg, &(param->length));
    param->result = eRet;
    INT_INFO("[srv] %s: getEDIDBytesInfo eRet: %d\r\n", __FUNCTION__, param->result);
    if (eRet == dsERR_NONE && param->length > 0 && param->length <= MAX_EDID_BYTES_LEN) {//Make sure the result was true, and there is a valid length.
        rc = memcpy_s(param->edid,sizeof(param->edid), edidArg, param->length);
        if(rc!=EOK)
        {
		ERR_CHK(rc);
        }
    }
    IARM_BUS_Unlock(lock);
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsGetHDMISPDInfo(void *arg)
{
    errno_t rc = -1;
    _DEBUG_ENTER();
    INT_DEBUG("%s:%d [srv] _dsGetHDMISPDInfo \n", __PRETTY_FUNCTION__,__LINE__);

    dsGetHDMISPDInfoParam_t *param = (dsGetHDMISPDInfoParam_t *)arg;

    IARM_BUS_Lock(lock);

    memset (param->spdInfo, '\0', sizeof(struct dsSpd_infoframe_st));
    unsigned char spdArg[sizeof(struct dsSpd_infoframe_st)] = {0};
    param->result = getHDMISPDInfo(param->iHdmiPort, spdArg);
    INT_INFO("[srv] %s: dsGetHDMISPDInfo eRet: %d\r\n", __FUNCTION__, param->result);
    if (param->result == dsERR_NONE) {
            rc = memcpy_s(param->spdInfo,sizeof(param->spdInfo), spdArg, sizeof(struct dsSpd_infoframe_st));
            if(rc!=EOK)
            {
                    ERR_CHK(rc);
            }
    }

    IARM_BUS_Unlock(lock);

    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsSetEdidVersion (void *arg)
{
    _DEBUG_ENTER();

    dsEdidVersionParam_t *param = (dsEdidVersionParam_t *) arg;
    IARM_BUS_Lock(lock);
    param->result = setEdidVersion (param->iHdmiPort, param->iEdidVersion);
    m_edidversion[param->iHdmiPort]=param->iEdidVersion;
    INT_INFO("[srv] %s: dsSetEdidVersion Port: %d EDID: %d eRet: %d\r\n", __FUNCTION__, param->iHdmiPort,  param->iEdidVersion, param->result);
    IARM_BUS_Unlock(lock);
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsGetEdidVersion (void *arg)
{
    int edidVer = -1;
    _DEBUG_ENTER();

    dsEdidVersionParam_t *param = (dsEdidVersionParam_t *) arg;
    IARM_BUS_Lock(lock);
    param->result = getEdidVersion (param->iHdmiPort, &edidVer);
    param->iEdidVersion = static_cast<tv_hdmi_edid_version_t>(edidVer);
    INT_INFO("[srv] %s: dsGetEdidVersion edidVer: %d\r\n", __FUNCTION__, param->iEdidVersion);
    IARM_BUS_Unlock(lock);
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsGetAllmStatus (void *arg)
{
    bool allmStatus = false;
    _DEBUG_ENTER();

    dsAllmStatusParam_t *param = (dsAllmStatusParam_t *) arg;
    IARM_BUS_Lock(lock);
    param->result = getAllmStatus (param->iHdmiPort, &allmStatus);
    param->allmStatus = allmStatus;
    INT_INFO("[srv] %s: dsGetAllmStatus allmStatus: %d\r\n", __FUNCTION__, param->allmStatus);
    IARM_BUS_Unlock(lock);
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsGetSupportedGameFeaturesList (void *arg)
{
    dsSupportedGameFeatureList_t    fList;
    _DEBUG_ENTER();

    dsSupportedGameFeatureListParam_t *param = (dsSupportedGameFeatureListParam_t *) arg;
    IARM_BUS_Lock(lock);
    param->result = getSupportedGameFeaturesList (&fList);
    param->featureList.gameFeatureCount = fList.gameFeatureCount;
    strncpy(param->featureList.gameFeatureList,fList.gameFeatureList,MAX_PROFILE_LIST_BUFFER_LEN);

    INT_INFO("%s: Total number of supported game features: %d\n",__FUNCTION__, fList.gameFeatureCount);
    INT_INFO("%s: Supported Game Features List: %s\n",__FUNCTION__, fList.gameFeatureList);

    IARM_BUS_Unlock(lock);
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsGetAVLatency (void *arg)
{
    _DEBUG_ENTER();
    int audio_latency;
    int video_latency;
    dsTVAudioVideoLatencyParam_t *param = (dsTVAudioVideoLatencyParam_t *) arg;
    IARM_BUS_Lock(lock);

    param->result = getAVLatency_hal(&audio_latency,&video_latency);
    param->video_latency = video_latency;
    param->audio_output_delay = audio_latency;
    INT_INFO("[srv] %s: _dsGetAVLatency AVLatency_params: %d : %d\r\n", __FUNCTION__, param->video_latency, param->audio_output_delay);
    IARM_BUS_Unlock(lock);
    return IARM_RESULT_SUCCESS;
}

void updateEdidAllmBitValuesInPersistence(dsHdmiInPort_t iHdmiPort, bool allmSupport)
{
      INT_INFO("[srv]: Updating values of edid allm bit in persistence\n");
      int port_no = (int)iHdmiPort;
	  if((port_no  >= 0) && (port_no < noOfSupportedHdmiInputs))
	  {
          std::string port_edidAllmSupport = "HDMI"+std::to_string(port_no)+".edidallmEnable";
          device::HostPersistence::getInstance().persistHostProperty(port_edidAllmSupport, allmSupport ? "TRUE" : "FALSE");
          INT_INFO("Port HDMI%d: Persist EDID Allm Bit: %d\n", port_no, allmSupport);
	  }
	  else
	  {
		  INT_INFO("Invalid port number to update in persistence\n");
	  }
		  
}

static dsError_t setEdid2AllmSupport (dsHdmiInPort_t iHdmiPort, bool allmSupport) {
    dsError_t eRet = dsERR_GENERAL;
    typedef dsError_t (*dsSetEdid2AllmSupport_t)(dsHdmiInPort_t iHdmiPort, bool allmSupport);
    static dsSetEdid2AllmSupport_t dsSetEdid2AllmSupportFunc = 0;

    if (dsSetEdid2AllmSupportFunc == 0) {
       void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
       if (dllib) {
            dsSetEdid2AllmSupportFunc = (dsSetEdid2AllmSupport_t) dlsym(dllib, "dsSetEdid2AllmSupport");
            if(dsSetEdid2AllmSupportFunc == 0) {
                INT_INFO("%s:%d dsSetEdid2AllmSupport (int,bool) is not defined %s\r\n", __FUNCTION__, __LINE__, dlerror());
            }
            else {
                INT_DEBUG("%s:%d dsSetEdid2AllmSupport loaded\r\n", __FUNCTION__, __LINE__);
            }
            dlclose(dllib);
        }
        else {
            INT_ERROR("%s:%d dsSetEdid2AllmSupport  Opening RDK_DSHAL_NAME [%s] failed %s\r\n",
                   __FUNCTION__, __LINE__, RDK_DSHAL_NAME, dlerror());
        }
    }
    INT_INFO("setEdid2AllmSupport to ds-hal:  EDID Allm Bit: %d\n", allmSupport);
    if (0 != dsSetEdid2AllmSupportFunc) {
        eRet = dsSetEdid2AllmSupportFunc (iHdmiPort, allmSupport);
        INT_INFO("[srv] %s: dsSetEdid2AllmSupportFunc eRet: %d \r\n", __FUNCTION__, eRet);
    }
    else {
        INT_INFO("%s:  dsSetEdid2AllmSupportFunc = %p\n", __FUNCTION__, dsSetEdid2AllmSupportFunc);
    }
    return eRet;
}

IARM_Result_t _dsSetEdid2AllmSupport (void *arg)
{
    _DEBUG_ENTER();

    dsEdidAllmSupportParam_t *param = (dsEdidAllmSupportParam_t *) arg;
    IARM_BUS_Lock(lock);
    param->result = dsERR_NONE;
    INT_INFO("[srv] :  In _dsSetEdid2AllmSupport, checking m_ediversion of port %d : %d\n",param->iHdmiPort,m_edidversion[param->iHdmiPort]);
    if(m_edidversion[param->iHdmiPort] == HDMI_EDID_VER_20)//if the edidver is 2.0, then only set the allm bit in edid
    {
        param->result = setEdid2AllmSupport (param->iHdmiPort, param->allmSupport);
    }
    INT_INFO("[srv] %s: dsSetEdid2AllmSupport Port: %d AllmSupport: %d eRet: %d\r\n", __FUNCTION__, param->iHdmiPort,  param->allmSupport, param->result);
    if(param->result == dsERR_NONE) 
    {
        updateEdidAllmBitValuesInPersistence(param->iHdmiPort,param->allmSupport);
        m_edidallmsupport[param->iHdmiPort] = param->allmSupport;
    }   
    IARM_BUS_Unlock(lock);
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsGetEdid2AllmSupport (void *arg)
{
    _DEBUG_ENTER();
    bool allmSupport = false;
    dsEdidAllmSupportParam_t *param = (dsEdidAllmSupportParam_t *) arg;
    IARM_BUS_Lock(lock);
    param->result =  dsERR_NONE;
    // getEdid2AllmSupport will return the latest allm bit value of the specified port(which is written to persistence)
    // irrespective of the edid version, the latest value is returned.
    param->allmSupport = m_edidallmsupport[param->iHdmiPort];
    INT_INFO("[srv] %s: dsGetEdid2AllmSupport : %d for port %d\r\n", __FUNCTION__, param->allmSupport,param->iHdmiPort);
    IARM_BUS_Unlock(lock);
    return IARM_RESULT_SUCCESS;
}

void updateVRRBitValuesInPersistence(dsHdmiInPort_t iHdmiPort, bool vrrSupport)
{
      INT_INFO("[srv]: Updating values of vrr bit in persistence\n");
	  int port_no = (int)iHdmiPort;
	  if((port_no  >= 0) && (port_no < noOfSupportedHdmiInputs))
	  {	  
          std::string port_vrrSupport = "HDMI"+std::to_string(port_no)+".vrrEnable";
          device::HostPersistence::getInstance().persistHostProperty(port_vrrSupport, vrrSupport ? "TRUE" : "FALSE");
[I          INT_INFO("Port HDMI%d: Persist EDID VRR Bit: %d\n", port_no, vrrSupport);
	  }
	  else
	  {
		  INT_INFO("Invalid port number to update in persistence\n");
	  }
}

static dsError_t setVRRSupport (dsHdmiInPort_t iHdmiPort, bool vrrSupport) {
    dsError_t eRet = dsERR_GENERAL;
    if (!m_hdmiPortVrrCaps[iHdmiPort]) {
	    return dsERR_OPERATION_NOT_SUPPORTED;
    }
    typedef dsError_t (*dsHdmiInSetVRRSupport_t)(dsHdmiInPort_t iHdmiPort, bool vrrSupport);
    static dsHdmiInSetVRRSupport_t dsHdmiInSetVRRSupportFunc = 0;

    if (dsHdmiInSetVRRSupportFunc == 0) {
       void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
       if (dllib) {
            dsHdmiInSetVRRSupportFunc = (dsHdmiInSetVRRSupport_t) dlsym(dllib, "dsHdmiInSetVRRSupport");
            if(dsHdmiInSetVRRSupportFunc == 0) {
                INT_INFO("%s:%d dsHdmiInSetVRRSupport (int,bool) is not defined %s\r\n", __FUNCTION__, __LINE__, dlerror());
            }
            else {
                INT_DEBUG("%s:%d dsHdmiInSetVRRSupport loaded\r\n", __FUNCTION__, __LINE__);
            }
            dlclose(dllib);
        }
        else {
            INT_ERROR("%s:%d dsHdmiInSetVRRSupport  Opening RDK_DSHAL_NAME [%s] failed %s\r\n",
                   __FUNCTION__, __LINE__, RDK_DSHAL_NAME, dlerror());
        }
    }
    INT_INFO("setVRRSupport to ds-hal:  EDID VRR Bit: %d\n", vrrSupport);
    if (0 != dsHdmiInSetVRRSupportFunc) {
        eRet = dsHdmiInSetVRRSupportFunc (iHdmiPort, vrrSupport);
        INT_INFO("[srv] %s: dsHdmiInSetVRRSupportFunc eRet: %d \r\n", __FUNCTION__, eRet);
    }
    else {
        INT_INFO("%s:  dsHdmiInSetVRRSupportFunc = %p\n", __FUNCTION__, dsHdmiInSetVRRSupportFunc);
    }
    return eRet;
}

IARM_Result_t _dsSetVRRSupport (void *arg)
{
    _DEBUG_ENTER();

    dsVRRSupportParam_t *param = (dsVRRSupportParam_t *) arg;
    IARM_BUS_Lock(lock);
    param->result = dsERR_NONE;
    INT_INFO("[srv] :  In _dsSetVRRSupport, checking m_ediversion of port %d : %d\n",param->iHdmiPort,m_edidversion[param->iHdmiPort]);
    if(m_edidversion[param->iHdmiPort] == HDMI_EDID_VER_20)//if the edidver is 2.0, then only set the VRR bit in edid
    {
        param->result = setVRRSupport (param->iHdmiPort, param->vrrSupport);
    }
    INT_INFO("[srv] %s: dsSetVRRSupport Port: %d vrrSupport: %d eRet: %d\r\n", __FUNCTION__, param->iHdmiPort,  param->vrrSupport, param->result);
    if(param->result == dsERR_NONE && m_hdmiPortVrrCaps[param->iHdmiPort])// update the persistence only for VRR supported ports
    {
        updateVRRBitValuesInPersistence(param->iHdmiPort,param->vrrSupport);
        m_vrrsupport[param->iHdmiPort] = param->vrrSupport;
    }
    IARM_BUS_Unlock(lock);
    return IARM_RESULT_SUCCESS;
}

static dsError_t getVRRSupport (dsHdmiInPort_t iHdmiPort, bool *vrrSupport) {
    if (!s_aidlPorts.empty()) {
        if ((int)iHdmiPort >= 0 && (int)iHdmiPort < dsHDMI_IN_PORT_MAX) {
            *vrrSupport = m_hdmiPortVrrCaps[(int)iHdmiPort];
            INT_INFO("[srv-aidl] getVRRSupport port %d vrr=%d\n", (int)iHdmiPort, *vrrSupport);
            return dsERR_NONE;
        }
        return dsERR_INVALID_PARAM;
    }
    dsError_t eRet = dsERR_GENERAL;
    typedef dsError_t (*dsHdmiInGetVRRSupport_t)(dsHdmiInPort_t iHdmiPort, bool *vrrSupport);
    static dsHdmiInGetVRRSupport_t dsHdmiInGetVRRSupportFunc = 0;

    if (dsHdmiInGetVRRSupportFunc == 0) {
       void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
       if (dllib) {
            dsHdmiInGetVRRSupportFunc = (dsHdmiInGetVRRSupport_t) dlsym(dllib, "dsHdmiInGetVRRSupport");
            if(dsHdmiInGetVRRSupportFunc == 0) {
                INT_INFO("%s:%d dsHdmiInGetVRRSupport (int,bool) is not defined %s\r\n", __FUNCTION__, __LINE__, dlerror());
            }
            else {
                INT_DEBUG("%s:%d dsHdmiInGetVRRSupport loaded\r\n", __FUNCTION__, __LINE__);
            }
            dlclose(dllib);
        }
        else {
            INT_ERROR("%s:%d dsHdmiInGetVRRSupport  Opening RDK_DSHAL_NAME [%s] failed %s\r\n",
                   __FUNCTION__, __LINE__, RDK_DSHAL_NAME, dlerror());
        }
    }
    if (0 != dsHdmiInGetVRRSupportFunc) {
        eRet = dsHdmiInGetVRRSupportFunc (iHdmiPort, vrrSupport);
        INT_INFO("[srv] %s: dsHdmiInGetVRRSupportFunc eRet: %d \r\n", __FUNCTION__, eRet);
    }
    else {
        INT_INFO("%s:  dsHdmiInGetVRRSupportFunc = %p\n", __FUNCTION__, dsHdmiInGetVRRSupportFunc);
    }
    return eRet;
}

IARM_Result_t _dsGetVRRSupport (void *arg)
{
    _DEBUG_ENTER();
    bool vrrSupport = false;
    dsVRRSupportParam_t *param = (dsVRRSupportParam_t *) arg;
    IARM_BUS_Lock(lock);
    param->result =  dsERR_NONE;
    // getVRRSupport will return the latest vrr bit value of the specified port(which is written to persistence)
    // irrespective of the edid version, the latest value is returned.
    param->vrrSupport = m_vrrsupport[param->iHdmiPort];
    INT_INFO("[srv] %s: dsGetVRRSupport : %d for port %d\r\n", __FUNCTION__, param->vrrSupport,param->iHdmiPort);
    IARM_BUS_Unlock(lock);
    return IARM_RESULT_SUCCESS;
}


static dsError_t getVRRStatus (dsHdmiInPort_t iHdmiPort, dsHdmiInVrrStatus_t *vrrStatus) {
    if (!s_aidlPorts.empty()) {
        std::lock_guard<std::mutex> lk(s_aidlMutex);
        auto it = s_aidlPorts.find((int)iHdmiPort);
        if (it == s_aidlPorts.end()) return dsERR_INVALID_PARAM;
        const AidlPortCtx& ctx = it->second;
        vrrStatus->vrrType = ctx.vrrActive
            ? (ctx.vrrFrameRate > 0.0 ? dsVRR_AMD_FREESYNC : dsVRR_HDMI_VRR)
            : dsVRR_NONE;
        vrrStatus->vrrAmdfreesyncFramerate_Hz = ctx.vrrActive ? ctx.vrrFrameRate : 0.0;
        INT_INFO("[srv-aidl] getVRRStatus port %d type=%d\n", (int)iHdmiPort, vrrStatus->vrrType);
        return dsERR_NONE;
    }
    dsError_t eRet = dsERR_GENERAL;
    typedef dsError_t (*dsHdmiInGetVRRStatus_t)(dsHdmiInPort_t iHdmiPort, dsHdmiInVrrStatus_t *vrrStatus);
    static dsHdmiInGetVRRStatus_t dsHdmiInGetVRRStatusFunc = 0;
    if (dsHdmiInGetVRRStatusFunc == 0) {
       void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
       if (dllib) {
            dsHdmiInGetVRRStatusFunc = (dsHdmiInGetVRRStatus_t) dlsym(dllib, "dsHdmiInGetVRRStatus");
            if(dsHdmiInGetVRRStatusFunc == 0) {
                INT_INFO("%s:%d dsHdmiInGetVRRStatus (int) is not defined %s\r\n", __FUNCTION__, __LINE__, dlerror());
            }
            else {
                INT_DEBUG("%s:%d dsHdmiInGetVRRStatusFunc loaded\r\n", __FUNCTION__, __LINE__);
            }
            dlclose(dllib);
        }
        else {
            INT_ERROR("%s:%d dsHdmiInGetVRRStatus  Opening RDK_DSHAL_NAME [%s] failed %s\r\n",
                   __FUNCTION__, __LINE__, RDK_DSHAL_NAME, dlerror());
        }
    }
    if (0 != dsHdmiInGetVRRStatusFunc) {
        eRet = dsHdmiInGetVRRStatusFunc (iHdmiPort, vrrStatus);
        INT_INFO("[srv] %s: dsHdmiInGetVRRStatusFunc eRet: %d \r\n", __FUNCTION__, eRet);
    }
    else {
        INT_INFO("%s:  dsHdmiInGetVRRStatusFunc = %p\n", __FUNCTION__, dsHdmiInGetVRRStatusFunc);
    }
    return eRet;
}

IARM_Result_t _dsGetVRRStatus (void *arg)
{
    dsHdmiInVrrStatus_t vrrStatus = {dsVRR_NONE,0};
    _DEBUG_ENTER();

    dsVRRStatusParam_t *param = (dsVRRStatusParam_t *) arg;
    IARM_BUS_Lock(lock);
    param->result = getVRRStatus (param->iHdmiPort, &vrrStatus);
    param->vrrStatus.vrrType = vrrStatus.vrrType;
    param->vrrStatus.vrrAmdfreesyncFramerate_Hz = vrrStatus.vrrAmdfreesyncFramerate_Hz;
    INT_INFO("[srv] %s: dsGetVRRStatus vrrType: %d vrrAmdfreesyncFramerate_Hz: %f\r\n", __FUNCTION__, param->vrrStatus.vrrType, param->vrrStatus.vrrAmdfreesyncFramerate_Hz);
    IARM_BUS_Unlock(lock);
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t _dsGetHdmiVersion (void *arg)
{
    dsError_t eRet = dsERR_GENERAL;
    dsHdmiVersionParam_t *param = (dsHdmiVersionParam_t *) arg;
    dsHdmiMaxCapabilityVersion_t capVersion;
    IARM_BUS_Lock(lock);
    eRet = getHdmiVersion (param->iHdmiPort, &capVersion);
    param->iCapVersion = capVersion;
    param->result = eRet;
    INT_INFO("[srv] %s: getHdmiVersion is %d, eRet: %d\r\n", __FUNCTION__,param->iCapVersion, param->result);
    IARM_BUS_Unlock(lock);
    return IARM_RESULT_SUCCESS;
}
/** @} */
/** @} */

