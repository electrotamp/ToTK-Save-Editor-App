/*
    TotK Save Editor — Switch startup (libnx userAppInit/userAppExit).
    Based on borealis switch_wrapper.c. Core service startup is always needed;
    the NXLink console connection is enabled only in diagnostic builds.
*/

#include <stdio.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <switch.h>
#if defined(TOTK_NXLINK_DEBUG)
#include <switch/runtime/nxlink.h>
#include <unistd.h>
#endif

#include "util/totk_log_c.h"

#if defined(TOTK_NXLINK_DEBUG)
extern struct in_addr __nxlink_host;
static int nxlink_sock = -1;
#endif

#ifndef TOTK_BUILD_ID
#define TOTK_BUILD_ID __DATE__ " " __TIME__
#endif

void userAppInit()
{
    totk_logf("userAppInit build=%s", TOTK_BUILD_ID);
    appletLockExit();

    SocketInitConfig cfg = *(socketGetDefaultInitConfig());
    AppletType at        = appletGetAppletType();
    if (at == AppletType_Application || at == AppletType_SystemApplication)
    {
        cfg.num_bsd_sessions = 12;
        cfg.sb_efficiency    = 8;
        socketInitialize(&cfg);
    }
    else
    {
        cfg.num_bsd_sessions = 2;
        cfg.sb_efficiency    = 1;
        socketInitialize(&cfg);
    }

#if defined(TOTK_NXLINK_DEBUG)
    if (__nxlink_host.s_addr) {
        nxlink_sock = nxlinkStdio();
        if (nxlink_sock >= 0) {
            totk_logf("nxlink connected");
        } else {
            totk_logf("nxlinkStdio failed");
        }
    } else {
        totk_logf("nxlink skipped (not launched via NetLoader)");
    }
#endif

    fsdevMountSdmc();
    totk_log_session_start();
    romfsInit();
    plInitialize(PlServiceType_User);
    setsysInitialize();
    setInitialize();
    psmInitialize();
    nifmInitialize(NifmServiceType_User);
    lblInitialize();
}

void userAppExit()
{
    totk_logf("userAppExit build=%s", TOTK_BUILD_ID);

    lblExit();
    nifmExit();
    psmExit();
    setExit();
    setsysExit();
    plExit();
    romfsExit();

#if defined(TOTK_NXLINK_DEBUG)
    if (nxlink_sock >= 0)
        close(nxlink_sock);
#endif

    socketExit();
    appletUnlockExit();
}
