#include "aGPIB.h"

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