/* (bananza, after BananaNative's hle_offline.c, hle_net.c, hle_misc.c, hle_time.c, hle_hid.c) what a game asks the
   console's online and peripheral services, answered offline: no network, no delivery data, no linked account, one DLC
   when the dump has it. The generated stubs answered 0 ("success") with nothing behind it; these answer as the console
   does without a connection, so the game takes its offline paths. */
#include "hos.h"
#include <string.h>
#include <time.h>

#define RES_FAIL 0x2A2u   /* the generic failure the cut services return */

int hos_aoc_count(void);   /* nn_fs.c */
/* nn::aoc: CountAddOnContent() -> int; ListAddOnContent(int* out, int offset, int max) -> int written */
HLE(_ZN2nn3aoc17CountAddOnContentEv) { RET(hos_aoc_count()); }
HLE(_ZN2nn3aoc16ListAddOnContentEPiii) {
    const int n = hos_aoc_count(), off = (int)ARG(1), max = (int)ARG(2);
    int w = 0;
    for (int i = off; i < n && w < max; i++) G32(ARG(0) + 4 * (uint64_t)w++) = i + 1;   /* (indices from 1) */
    RET(w);
}
/* nn::bcat: data delivery: none */
HLE(_ZN2nn4bcat10InitializeEv) {}
HLE(_ZN2nn4bcat25MountDeliveryCacheStorageEv) { RET(RES_FAIL); }
HLE(_ZN2nn4bcat27UnmountDeliveryCacheStorageEv) {}
/* nn::nifm: offline */
HLE(_ZN2nn4nifm18IsNetworkAvailableEv) { RET(0); }
HLE(_ZN2nn4nifm27SubmitNetworkRequestAndWaitEv) {}
HLE(_ZN2nn4nifm31HandleNetworkRequestErrorResultEv) { RET(0); }
/* nn::account: no Nintendo Account linked */
HLE(_ZN2nn7account29LoadCachedNintendoAccountInfoEPNS0_25CachedNintendoAccountInfoERKNS0_10UserHandleEPvm) { RET(RES_FAIL); }
/* nn::time::ToPosixTime(int* count, PosixTime* out, int max, CalendarTime const&): the host's own zone (mktime) */
HLE(_ZN2nn4time11ToPosixTimeEPiPNS0_9PosixTimeEiRKNS0_12CalendarTimeE) {
    const uint8_t *ct = (const uint8_t *)GP(ARG(3));
    struct tm tm = {0};
    tm.tm_year = (int)(int16_t)(ct[0] | ct[1] << 8) - 1900, tm.tm_mon = (int8_t)ct[2] - 1, tm.tm_mday = (int8_t)ct[3];
    tm.tm_hour = (int8_t)ct[4], tm.tm_min = (int8_t)ct[5], tm.tm_sec = (int8_t)ct[6], tm.tm_isdst = -1;
    const time_t t = mktime(&tm);
    if (ARG(0)) G32(ARG(0)) = (int)ARG(2) > 0;
    if ((int)ARG(2) > 0 && ARG(1)) G64(ARG(1)) = (uint64_t)(int64_t)t;
    RET(0);
}
/* nn::hid: the Joy-Con 2 mouse sensor (no sensor here) and the vibration device of a handle */
HLE(_ZN2nn3hid13GetJcmsHandleERKjRKNS0_8JcmsTypeE) { RET((G32(ARG(0)) & 0xff) | (G32(ARG(1)) & 0xff) << 8); }
HLE(_ZN2nn3hid13GetJcmsStatesEPNS0_9JcmsStateEiRKNS0_10JcmsHandleE) { RET(0); }
/* GetVibrationDeviceInfo(VibrationDeviceInfo* {u32 type, u32 position}, handle const&): a linear resonant actuator, left or right */
HLE(_ZN2nn3hid22GetVibrationDeviceInfoEPNS0_19VibrationDeviceInfoERKNS0_21VibrationDeviceHandleE) {
    const uint32_t h = G32(ARG(1));
    G32(ARG(0)) = 1;
    G32(ARG(0) + 4) = 1 + (h >> 16 & 1);
}
HLE(_ZN2nn2oe24SetAlbumImageOrientationENS_5album16ImageOrientationE) {}
