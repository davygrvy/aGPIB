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

#if TCL_MAJOR_VERSION < 9
    typedef int Tcl_Size;
#   define TCL_SIZE_MAX          INT_MAX
#   define TCL_SIZE_MODIFIER     ""
#   define Tcl_GetSizeIntFromObj Tcl_GetIntFromObj
#   define Tcl_NewSizeIntObj     Tcl_NewIntObj
#   define TCL_AUTO_LENGTH (-1)
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