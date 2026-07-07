/* ----------------------------------------------------------------------
 *
 * aGPIBInt.hpp --
 *
 * 	'Asynchronous General Purpose Interface Bus' for Tcl.
 *
 *	This extension adds a new channel type to Tool Command Language
 * 	that allows for easy communication with devices plugged into
 * 	a GPIB bus.  Linux and Windows friendly.
 *
 *	This file contains the internal shared interface.
 *
 * ----------------------------------------------------------------------
 *
 * Copyright (c) 2026 David Gravereaux <davygrvy@pobox.com>
 *
 * See the file "license.terms" for information on usage and
 * redistribution of this file, and for a DISCLAIMER OF ALL
 * WARRANTIES.
 *
 * ----------------------------------------------------------------------
 * RCS: @(#) $Id: $
 * ----------------------------------------------------------------------
 */

#ifndef INCL_aGPIBInt_h_
#define INCL_aGPIBInt_h_

#include "aGPIB.h"


#if defined(__WIN32__)
#   include <ni4882.h>		/* The National Instruments interface. */

    /* Add some things that are missing */
    const char* gpib_error_string(int iberr);
    const int gpib_addr_max = 30;
#   define IbStbRQS		0x40
#   define GPIB_MAX_NUM_BOARDS	16
#else
#   include <gpib/ib.h>		/* The Linux-GPIB FOSS interface. */
#   include <errno.h>
#endif


#include <queue>

struct _GpibInfo {
    Tcl_Channel chan;   /* us! */
    int watchMask;           /* combo of TCL_READABLE, TCL_WRITABLE, or TCL_EXCEPTION */
    int board_desc;
    int ud;		/* device descriptor */
    Addr4882_t addr;	/* device address */
    int eot_mode;
    int term;		/* termination character */
    int timeout;
    std::queue<std::uint8_t> STB_Q;  /* TODO, needs to be threadsafe */
    Tcl_ThreadId thrd;	/* origin thread this channel belongs to */
};
typedef struct _GpibInfo GpibInfo;


#undef TCL_STORAGE_CLASS
#ifdef BUILD_agpib
#   define TCL_STORAGE_CLASS DLLEXPORT
#else
#   ifdef USE_IOCP_STUBS
#	define TCL_STORAGE_CLASS
#   else
#	define TCL_STORAGE_CLASS DLLIMPORT
#   endif
#endif

#include "aGPIBIntDecls.h"

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* #ifndef INCL_aGPIBInt_h_ */
