#ifndef INCL_aGPIBInt_h_
#define INCL_aGPIBInt_h_

#include "aGPIB.h"

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

#include "aGPIBIntDecls.h"

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* #ifndef INCL_aGPIBInt_h_ */