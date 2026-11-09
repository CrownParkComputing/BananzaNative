/* (bananza) nn::streamplay: GameShare / GameChat streaming over the stpl:u service, which the host does not serve.
   Donkey Kong Bananza initialises it at boot (GetRequiredMemorySize, Initialize) and polls its connection state; left
   to the SDK, CreateUserServiceByHipc talks to sm: over IPC, gets the empty answer and aborts. The host streams to
   nobody: the library is "ready", no session is ever open, no guest ever connects, and the connection-state event
   is bound but never signalled (after BananaNative's hle_misc.c, which plays the same game on Android). Every other
   nn::streamplay free function is a generated stub (nx2/standalone.py OS_NAMESPACES). */
#include "hos.h"
#include <string.h>

void hos_system_event_init(uint64_t key, int autoclear);   /* nn_os_sync.c */

/* GetRequiredMemorySize(StreamPlayType, StreamingMode, int) -> size: one page; the host keeps no state in it */
HLE(_ZN2nn10streamplay21GetRequiredMemorySizeENS0_14StreamPlayTypeENS0_13StreamingModeEi) { RET(0x1000); }
/* Initialize(StreamPlayType, StreamingMode, void* work, size_t) -> Result: ready, no session */
HLE(_ZN2nn10streamplay10InitializeENS0_14StreamPlayTypeENS0_13StreamingModeEPvm) {
    hos_log("streamplay: initialised (type %d, mode %d): the host streams to nobody", (int)ARG(0), (int)ARG(1));
    RET(0);
}
HLE(_ZN2nn10streamplay8FinalizeEv) {}
HLE(_ZN2nn10streamplay12CloseSessionEv) { RET(0); }
HLE(_ZN2nn10streamplay14StartStreamingEv) { RET(0); }
/* OpenSessionWithMatchmakeSupport(SessionInfo*, MatchmakeConfig const&) -> Result: no session can open */
HLE(_ZN2nn10streamplay31OpenSessionWithMatchmakeSupportEPNS0_11SessionInfoERKNS0_15MatchmakeConfigE) { RET(0x2A0A); }
HLE(_ZN2nn10streamplay24CreateIndividualDisplaysEi) {}
HLE(_ZN2nn10streamplay25DestroyIndividualDisplaysEv) {}
HLE(_ZN2nn10streamplay21GetNativeWindowHandleENS0_7GuestIdE) { RET(0); }

/* GetConnectionStateChangedEvent() -> os::SystemEvent*: a host-owned event, bound once, never signalled */
HLE(_ZN2nn10streamplay30GetConnectionStateChangedEventEv) {
    static uint64_t ev;
    if (!ev) {
        ev = hos_sys_alloc(0x80, 16);
        hos_system_event_init(ev, 0);
    }
    RET(ev);
}
/* GetConnectionState(ConnectionState* out): 0, not connected */
HLE(_ZN2nn10streamplay18GetConnectionStateEPNS0_15ConnectionStateE) { if (ARG(0)) *(uint32_t *)GP(ARG(0)) = 0; }

/* the guest pad configuration the game sets is kept for its getters; there is never a guest to read */
static uint32_t stpl_styles, stpl_hold, stpl_features;
HLE(_ZN2nn10streamplay29SetSupportedGuestNpadStyleSetENS_4util10BitFlagSetILi32ENS_3hid12NpadStyleTagEEE) { stpl_styles = (uint32_t)ARG(0); }
HLE(_ZN2nn10streamplay29GetSupportedGuestNpadStyleSetEv) { RET(stpl_styles); }
HLE(_ZN2nn10streamplay23SetGuestNpadJoyHoldTypeENS_3hid15NpadJoyHoldTypeE) { stpl_hold = (uint32_t)ARG(0); }
HLE(_ZN2nn10streamplay23GetGuestNpadJoyHoldTypeEv) { RET(stpl_hold); }
HLE(_ZN2nn10streamplay25SetSupportedHidFeatureSetENS_4util10BitFlagSetILi32ENS0_13HidFeatureTagEEE) { stpl_features = (uint32_t)ARG(0); }
/* GetGuestPadStyleSet(GuestPadId const&) -> BitFlagSet: no styles, there is no guest */
HLE(_ZN2nn10streamplay19GetGuestPadStyleSetERKNS0_10GuestPadIdE) { RET(0); }
/* GetGuestPadStates<T>(GuestPadState<T>* out, int count, GuestPadId const&) -> int read: none */
HLE(_ZN2nn10streamplay17GetGuestPadStatesEPNS0_13GuestPadStateINS_3hid16NpadFullKeyStateEEEiRKNS0_10GuestPadIdE) { RET(0); }
HLE(_ZN2nn10streamplay17GetGuestPadStatesEPNS0_13GuestPadStateINS_3hid16NpadJoyDualStateEEEiRKNS0_10GuestPadIdE) { RET(0); }
HLE(_ZN2nn10streamplay17GetGuestPadStatesEPNS0_13GuestPadStateINS_3hid16NpadJoyLeftStateEEEiRKNS0_10GuestPadIdE) { RET(0); }
HLE(_ZN2nn10streamplay17GetGuestPadStatesEPNS0_13GuestPadStateINS_3hid17NpadHandheldStateEEEiRKNS0_10GuestPadIdE) { RET(0); }
HLE(_ZN2nn10streamplay17GetGuestPadStatesEPNS0_13GuestPadStateINS_3hid17NpadJoyRightStateEEEiRKNS0_10GuestPadIdE) { RET(0); }
HLE(_ZN2nn10streamplay27GetGuestSixAxisSensorStatesEPNS0_23GuestSixAxisSensorStateEiRKNS0_24GuestSixAxisSensorHandleE) { RET(0); }
HLE(_ZN2nn10streamplay28GetGuestSixAxisSensorHandlesEPNS0_24GuestSixAxisSensorHandleEiRKNS0_10GuestPadIdENS_4util10BitFlagSetILi32ENS_3hid12NpadStyleTagEEE) { RET(0); }
HLE(_ZN2nn10streamplay30GetGuestVibrationDeviceHandlesEPNS0_26GuestVibrationDeviceHandleEiRKNS0_10GuestPadIdENS_4util10BitFlagSetILi32ENS_3hid12NpadStyleTagEEE) { RET(0); }
HLE(_ZN2nn10streamplay27GetGuestVibrationDeviceInfoEPNS_3hid19VibrationDeviceInfoERKNS0_26GuestVibrationDeviceHandleE) { RET(0x2A0A); }
HLE(_ZN2nn10streamplay18GetGuestJcmsHandleERKNS0_10GuestPadIdERKNS_3hid8JcmsTypeE) { RET(0); }
HLE(_ZN2nn10streamplay18GetGuestJcmsStatesEPNS0_14GuestJcmsStateEiRKNS0_15GuestJcmsHandleE) { RET(0); }
/* GuestVibrationTarget: a rumble target with no guest behind it */
HLE(_ZN2nn10streamplay20GuestVibrationTargetC1Ev) { if (ARG(0)) memset(GP(ARG(0)), 0, 16); }
HLE(_ZN2nn10streamplay20GuestVibrationTarget29SetGuestVibrationDeviceHandleERKNS0_26GuestVibrationDeviceHandleE) {}
HLE(_ZN2nn10streamplay20GuestVibrationTarget31UnsetGuestVibrationDeviceHandleEv) {}
