/* --------------------------------------------------------------------
 *
 * aGPIBChan.cpp --
 *
 * 	'Asynchronous General Purpose Interface Buss' for Tcl.
 * 
 *	This extension adds a new channel type to Tool Command Language
 * 	that allows for easy and modern communication with devices
 * 	plugged into a GPIB buss.  Linux and Windows friendly.
 * 
 *	This file defines the channel interface to Tcl.
 *
 * --------------------------------------------------------------------
 * RCS: @(#) $Id: $
 * --------------------------------------------------------------------
 */

#include "aGPIB.h"
#include <string.h>

// globals to be moved later
static int                  InitializeGpibSubSystem(Tcl_Interp *interp);


/* local prototypes */
static Tcl_ExitProc	        GpibExitHandler;
static Tcl_ExitProc	        GpibThreadExitHandler;
static Tcl_EventSetupProc	GpibEventSetupProc;
static Tcl_EventCheckProc	GpibEventCheckProc;
static Tcl_EventProc		GpibEventProc;
static Tcl_EventDeleteProc	GpibRemovePendingEvents;
static Tcl_EventDeleteProc	GpibRemoveAllPendingEvents;

static GpibInfo *FindChannelFromAddr (int board_desc, GPIB::Addr4882_t address);
static void TranslateGpibErr2Tcl(Tcl_Channel chan, int ibErr);
static Tcl_ThreadCreateProc       GpibSRQNotifier;

static Tcl_DriverBlockModeProc    GpibBlockProc;
static Tcl_DriverCloseProc        GpibCloseProc;
static Tcl_DriverInputProc        GpibInputProc;
static Tcl_DriverGetOptionProc    GpibGetOptionProc;
static Tcl_DriverOutputProc       GpibOutputProc;
static Tcl_DriverSetOptionProc    GpibSetOptionProc;
static Tcl_DriverWatchProc        GpibWatchProc;
static Tcl_DriverThreadActionProc GpibThreadActionProc;

Tcl_ChannelType AgpibChannelType = {
    (char*)"gpib",	      /* Type name. */
    TCL_CHANNEL_VERSION_4,/* TIP #218. */
    GpibCloseProc,	      /* Close proc. */
    GpibInputProc,	      /* Input proc. */
    GpibOutputProc,	      /* Output proc. */
    NULL,                 /* Seek proc. */
    GpibSetOptionProc,    /* Set option proc. */
    GpibGetOptionProc,    /* Get option proc. */
    GpibWatchProc,        /* Set up notifier to watch this channel. */
    NULL,                 /* Get an OS handle from channel. */
    NULL,                 /* close2proc. */
    GpibBlockProc,        /* Set device into (non-)blocking mode. */
    NULL,                 /* flush proc. */
    NULL,                 /* handler proc. */
    NULL,                 /* wide seek */
    GpibThreadActionProc, /* TIP #218. */
};

/* file scope */
static GpibInfo *start = NULL;
static bool shutdown = false;


typedef struct {
    int board_desc;
    //GPIB::Addr4882_t *addressList[];
} BrdInfo;




/* --------------------------------------------------------------------
 *
 * GpibSRQNotifier --
 *
 *      This is our notifier routine to service interrupts.  It is run
 *      in a thread.
 *
 * 	params --
 *       clientData:
 *             a BrdInfo pointer that contains the board to run on.
 *
 * --------------------------------------------------------------------
 */


static Tcl_ThreadCreateType
GpibSRQNotifier (
    ClientData clientData)      /* The GPIB board to watch */
{
    BrdInfo *brdInfoPtr = (BrdInfo *) clientData;
    GpibInfo *infoPtr;
    short result;
    GPIB::Addr4882_t allBusAddresses[GPIB::gpib_addr_max+1];
    short statusList[GPIB::gpib_addr_max+1]; 

    /* Fill the array with all addresses */
    for (int i = 0; i < GPIB::gpib_addr_max; i++)
            allBusAddresses[i] = GPIB::MakeAddr(i+1, 0); /* 1 to 30 */
    allBusAddresses[GPIB::gpib_addr_max] = GPIB::NOADDR;

    /* Set a 3 second timeout */
    GPIB::ibtmo(brdInfoPtr->board_desc, GPIB::T3s);

    if (GPIB::ThreadIbsta() & GPIB::ERR) {
        /* Bad error */
        goto done;
    }

again:
    /* Sleep until timeout or any device pulls the physical SRQ line low */
    GPIB::WaitSRQ(brdInfoPtr->board_desc, &result);

    if (GPIB::ThreadIbsta() & GPIB::ERR) {
        /* Bad error or the board went offline (we can't trust result) */
        goto done;
    }

    switch (result) {
    case 0: /* Timed-out */
        if (shutdown) goto done;    /* clean exit */
        goto again;

    case 1: /* SRQ Asserted */
        /* Sweep every address on the buss. This reads and CLEARS the SRQ line
         * for BOTH known and unknown instruments. */
        GPIB::AllSpoll(brdInfoPtr->board_desc, allBusAddresses, statusList);

        /* Process the results */
        for (int i = 0; allBusAddresses[i] != GPIB::NOADDR; i++) {

            /* Does this address request service? (Bit 6 / 0x40 RQS Flag) */
            if (statusList[i] & GPIB::IbStbRQS) {

                /* Resolve to our channel */
                infoPtr = FindChannelFromAddr(brdInfoPtr->board_desc,
                        allBusAddresses[i]);

                if (infoPtr != NULL) {
                    /* Active channel found: Push the status byte,
                     * filter out RQS flag from STB. */
                    infoPtr->STB_Q.push_back(
                            (statusList[i] & ~GPIB::IbStbRQS));

                    /* Wake the Tcl notifier to service (at least)
                     * this channel's event source. */
                    Tcl_ThreadAlert(infoPtr->thrd);
                } else {
                    /* Rouge device disarmed! */
                    ((void)0);
                }
            }
        }
        goto again;
    }
    
done:
    TCL_THREAD_CREATE_RETURN;
}

static void
TranslateGpibErr2Tcl(
    Tcl_Channel chan,           /* The channel */
    int ibErr)                  /* The GPIB error code */
{
    Tcl_SetChannelError(chan, Tcl_NewStringObj(
	        GPIB::gpib_error_string(ibErr), -1));
}

static int
GpibCloseProc (
    ClientData instanceData,    /* The GPIB device state. */
    Tcl_Interp *interp)	        /* Unused. */
{
    GpibInfo *infoPtr = (GpibInfo *) instanceData;
    int errorCode = TCL_OK;
    int status;

    if (infoPtr != NULL) {
        infoPtr->flags |= AGPIB_CLOSING;

        status = GPIB::ibonl(infoPtr->ud, 0);
        
        if (status & GPIB::ERR) {
            Tcl_SetErrno(EIO);
            errorCode = Tcl_GetErrno();
        }

    }

    return errorCode;
}

static int
GpibInputProc (
    ClientData instanceData,  /* The GPIB device state. */
    char *buf,                /* Where to store data. */
    int toRead,               /* Maximum number of bytes to read. */
    int *errorCodePtr)        /* Where to store errno codes. */
{
    GpibInfo *infoPtr = (GpibInfo *) instanceData;
    *errorCodePtr = TCL_OK;
    int status;

    if ((infoPtr->flags & AGPIB_CLOSING) || TclInExit()) {
	    *errorCodePtr = ENOTCONN;
    	return -1;
    }

    GPIB::Receive(infoPtr->ud, infoPtr->addr, buf, (long)toRead, GPIB::STOPend/*infoPtr->term*/);
    status = GPIB::ThreadIbsta();

    if (status & GPIB::ERR) {
	    TranslateGpibErr2Tcl(infoPtr->chan, GPIB::ThreadIberr());
	    *errorCodePtr = Tcl_GetErrno();
	    return -1;
    }
 
    else if (status & GPIB::TIMO) {
	    if (infoPtr->mode == TCL_MODE_BLOCKING) {
	        Tcl_SetErrno(ETIMEDOUT);
	    } else {
	        Tcl_SetErrno(EWOULDBLOCK);
	    }
	    *errorCodePtr = Tcl_GetErrno();
	    return -1;
    }
    
    /* return how much we read */
    return GPIB::ThreadIbcnt();
}

static int
GpibOutputProc (
    ClientData instanceData,  /* The GPIB device state. */
    CONST char *buf,          /* Where to get data. */
    int toWrite,              /* Maximum number of bytes to write. */
    int *errorCodePtr)        /* Where to store errno codes. */
{
    GpibInfo *infoPtr = (GpibInfo *) instanceData;
    *errorCodePtr = TCL_OK;
    int status;


    if ((infoPtr->flags & AGPIB_CLOSING) || TclInExit()) {
	    *errorCodePtr = ENOTCONN;
	    return -1;
    }

    GPIB::Send(infoPtr->ud, infoPtr->addr, buf, (long)toWrite, GPIB::DABend/*infoPtr->eot_mode*/);
    status = GPIB::ThreadIbsta();

    if (status & GPIB::ERR) {
	    TranslateGpibErr2Tcl(infoPtr->chan, GPIB::ThreadIberr());
	    *errorCodePtr = Tcl_GetErrno();
	    return -1;
    }
    
    else if (status & GPIB::TIMO) {
	    if (infoPtr->mode == TCL_MODE_NONBLOCKING) {
	        Tcl_SetErrno(EWOULDBLOCK);
	    } else {
	        Tcl_SetErrno(ETIMEDOUT);
	    }
	    *errorCodePtr = Tcl_GetErrno();
	    return -1;
    }
    
    /* return how much we wrote */
    return GPIB::ThreadIbcnt();
}

static int
GpibSetOptionProc (
    ClientData instanceData,	/* The GPIB device state. */
    Tcl_Interp *interp,         /* For error reporting - can be NULL. */
    CONST char *optionName,     /* Name of the option to set. */
    CONST char *value)          /* New value for option. */
{
    GpibInfo *infoPtr = (GpibInfo *) instanceData;

    // TODO

    return Tcl_BadChannelOption(interp, optionName,
		"spoll timeout");
}

static int
GpibGetOptionProc (
    ClientData instanceData,    /* The GPIB device state. */
    Tcl_Interp *interp,		    /* For error reporting - can be NULL */
    CONST char *optionName,	    /* Name of the option to
				                 * retrieve the value for, or
                 			     * NULL to get all options and
				                 * their values. */
    Tcl_DString *dsPtr)		    /* Where to store the computed
				                 * value; initialized by caller. */
{
    GpibInfo *infoPtr = (GpibInfo *) instanceData;
    short int result;
    size_t len = 0;
    char buf[TCL_INTEGER_SPACE];

    if (optionName != (char *) NULL) {
        len = strlen(optionName);
    }

    if (len == 0 || !strncmp(optionName, "-spoll", len)) {
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-spoll");
        }
        ReadStatusByte(infoPtr->ud, infoPtr->addr, &result);
        sprintf(buf, "%hi", result);
        Tcl_DStringAppendElement(dsPtr, buf);
        if (len > 0) return TCL_OK;
    }

    if (len == 0 || !strncmp(optionName, "-timeout", len)) {
        if (len == 0) {
            Tcl_DStringAppendElement(dsPtr, "-timeout");
        }
        switch (infoPtr->timeout) {
        case GPIB::TNONE:
            Tcl_DStringAppendElement(dsPtr, "none"); break;
        case GPIB::T10us:
            Tcl_DStringAppendElement(dsPtr, "10us"); break;
        case GPIB::T30us:
            Tcl_DStringAppendElement(dsPtr, "30us"); break;
        case GPIB::T100us:
            Tcl_DStringAppendElement(dsPtr, "100us"); break;
        case GPIB::T300us:
            Tcl_DStringAppendElement(dsPtr, "300us"); break;
        case GPIB::T1ms:
            Tcl_DStringAppendElement(dsPtr, "1ms"); break;
        case GPIB::T3ms:
            Tcl_DStringAppendElement(dsPtr, "3ms"); break;
        case GPIB::T10ms:
            Tcl_DStringAppendElement(dsPtr, "10ms"); break;
        case GPIB::T30ms:
            Tcl_DStringAppendElement(dsPtr, "30ms"); break;
        case GPIB::T100ms:
            Tcl_DStringAppendElement(dsPtr, "100ms"); break;
        case GPIB::T300ms:
            Tcl_DStringAppendElement(dsPtr, "300ms"); break;
        case GPIB::T1s:
            Tcl_DStringAppendElement(dsPtr, "1s"); break;
        case GPIB::T3s:
            Tcl_DStringAppendElement(dsPtr, "3s"); break;
        case GPIB::T10s:
            Tcl_DStringAppendElement(dsPtr, "10s"); break;
        case GPIB::T30s:
            Tcl_DStringAppendElement(dsPtr, "30s"); break;
        case GPIB::T100s:
            Tcl_DStringAppendElement(dsPtr, "100s"); break;
        case GPIB::T300s:
            Tcl_DStringAppendElement(dsPtr, "300s"); break;
        case GPIB::T1000s:
            Tcl_DStringAppendElement(dsPtr, "1000s"); break;
        }
        if (len > 0) return TCL_OK;
    }

    if (len > 0) {
        return Tcl_BadChannelOption(interp, optionName, "spoll timeout");
    }

    return TCL_OK;
}

static void
GpibWatchProc (
    ClientData instanceData,    /* The GPIB device state. */
    int mask)                   /* Events of interest; an OR-ed
                                 * combination of TCL_READABLE,
                                 * TCL_WRITABLE and TCL_EXCEPTION. */
{
    //GpibInfo *infoPtr = (GpibInfo *) instanceData;

    // TODO
}

static int
GpibBlockProc (
    ClientData instanceData,    /* The GPIB device state. */
    int mode)                   /* TCL_MODE_BLOCKING or
                                 * TCL_MODE_NONBLOCKING. */
{
    GpibInfo *infoPtr = (GpibInfo *) instanceData;

    infoPtr->mode = mode;

    if (mode == TCL_MODE_NONBLOCKING) {
	    GPIB::ibtmo(infoPtr->ud, GPIB::T10us);
    } else {
	    GPIB::ibtmo(infoPtr->ud, infoPtr->timeout);
    }
    return 0;
}

static void
GpibThreadActionProc (
    ClientData instanceData,    /* The GPIB device state. */
    int action)
{
     GpibInfo *infoPtr = (GpibInfo *) instanceData;

    switch (action) {
        case TCL_CHANNEL_THREAD_INSERT:
            // Safely assign the new thread owner context
            infoPtr->thrd = Tcl_GetCurrentThread();
            break;

        case TCL_CHANNEL_THREAD_REMOVE:
            // Clear out the pointer so background processes 
            // know this thread no longer owns it
            infoPtr->thrd = NULL;
            break;
    }
}

int
InitializeGpibSubSystem(Tcl_Interp *interp)
{
    /* TODO */

    //  Nothing really do here, actually.  This is called once per
    //  Tcl interp loading via 'package require agpib'.
    //  Dynamic loading of ni488.dll could happen here on windows, I guess

    return TCL_OK;
}

static GpibInfo *
FindChannelFromAddr (
    int board_desc,
    GPIB::Addr4882_t addr)
{
    GpibInfo *temp;

    /* TODO: this needs work */
    for (temp = start; temp != NULL; temp = temp->next) {
        if ((temp->board_desc == board_desc) && (GetPAD(temp->addr) == GetPAD(addr))) {
            return temp;
        }
    }

    return NULL;
}

GpibInfo *
NewGPIBInfo()
{
    GpibInfo *infoPtr = ckalloc(sizeof(GpibInfo));

    /* splice it in */
    //Tcl_MutexLock()
    infoPtr->next = start;
    start = infoPtr;
    //Tcl_MutexUnlock()
     
    /* add defaults */
    infoPtr->chan = NULL;
    infoPtr->mode = TCL_MODE_NONBLOCKING;
    infoPtr->board_desc = 0;
    infoPtr->ud = 0;
    infoPtr->addr = 0;
    infoPtr->eot_mode = GPIB::DABend;
    infoPtr->timeout = GPIB::T300us;
    infoPtr->STB_Q = 0;   /* TODO, needs to be a threadsafe std::queue<short> or somesuch*/
    infoPtr->thrd = Tcl_GetCurrentThread();

    return inforPtr;
}

void
DeleteGPIBInfo(GpibInfo *infoPtr)
{
    /* unsplice it */
    //Tcl_MutexLock()
    //TODO
    //Tcl_MutexUnlock()
    ckfree(infoPtr);
}

/*
 *-----------------------------------------------------------------------
 * GpibEventSetupProc --
 *
 *  Happens before the event loop is to wait in the notifier.
 *
 *-----------------------------------------------------------------------
 */
static void
GpibEventSetupProc (
    ClientData clientData,
    int flags)
{
    //ThreadSpecificData *tsdPtr = InitSockets();
    Tcl_Time blockTime = {0, 0};

    if (!(flags & TCL_FILE_EVENTS)) {
        return;
    }

    /*
     * If any ready events exist now, don't let the notifier go into it's
     * wait state.  This function call is very inexpensive.
     */

     /* TODO */
    //if (IocpLLIsNotEmpty(tsdPtr->readySockets)) {
    //    Tcl_SetMaxBlockTime(&blockTime);
    //}
}

/*
 *-----------------------------------------------------------------------
 * GpibEventCheckProc --
 *
 *  Happens after the notifier has waited.
 *
 *-----------------------------------------------------------------------
 */
static void
GpibEventCheckProc (
    ClientData clientData,
    int flags)
{
#if 0
    ThreadSpecificData *tsdPtr = InitSockets();
    SocketInfo *infoPtr;
    SocketEvent *evPtr;
    int evCount;

    if (!(flags & TCL_FILE_EVENTS)) {
        /* Don't be greedy. */
        return;
    }

    /*
     * Sockets that are EOF, but not yet closed, are considered readable.
     * Because Tcl historically requires that EOF channels shall still
     * fire readable and writable events until closed and our alert
     * semantics are such that we'll never get repeat notifications after
     * EOF, we place this poll condition here.
     */

    /* TODO: evCount = IocpLLGetCount(tsdPtr->deadSockets); */

    /*
     * Do we have any jobs to queue?  Take a snapshot of the count as
     * of now.
     */

    evCount = IocpLLGetCount(tsdPtr->readySockets);

    while (evCount--) {
        EnterCriticalSection(&tsdPtr->readySockets->lock);
        infoPtr = IocpLLPopFront(tsdPtr->readySockets,
                IOCP_LL_NOLOCK | IOCP_LL_NODESTROY, 0);
        /*
         * Flop the markedReady toggle.  This is used to improve event
         * loop efficiency to avoid unneccesary events being queued into
         * the readySockets list.
         */
        if (infoPtr) InterlockedExchange(&infoPtr->markedReady, 0);
        LeaveCriticalSection(&tsdPtr->readySockets->lock);

        /*
         * Safety check. Somehow the count of what is and what actually
         * is, is less (!?)..  whatever...  
         */
        if (!infoPtr) continue;

        /*
         * The socket isn't ready to be serviced.  accept() in the Tcl
         * layer hasn't happened yet while reads on the new socket are
         * coming in or the socket is in the middle of doing an async
         * close.
         */
        if (infoPtr->channel == NULL) {
            continue;
        }

        evPtr = (SocketEvent *) ckalloc(sizeof(SocketEvent));
        evPtr->header.proc = GpibEventProc;
        evPtr->infoPtr = infoPtr;
        Tcl_QueueEvent((Tcl_Event *) evPtr, TCL_QUEUE_TAIL);
    }
#endif
}

/*
 *-----------------------------------------------------------------------
 * GpibEventProc --
 *
 *  Tcl's event loop is now servicing this.
 *
 *-----------------------------------------------------------------------
 */
static int
GpibEventProc (
    Tcl_Event *evPtr,               /* Event to service. */
    int flags)                      /* Flags that indicate what events to
                                     * handle, such as TCL_FILE_EVENTS. */
{
#if 0
    SocketInfo *infoPtr = ((SocketEvent *)evPtr)->infoPtr;
    int readyMask = 0;

    if (!(flags & TCL_FILE_EVENTS)) {
        /* Don't be greedy. */
        return 0;
    }

    /*
     * If an accept is ready, pop one only.  There might be more,
     * but this would be greedy with regards to the event loop.
     */
    if (infoPtr->readyAccepts != NULL) {
        IocpAcceptOne(infoPtr);
        return 1;
    }

    /*
     * If there is at least one entry on the infoPtr->llPendingRecv list,
     * and the watch mask is set to notify for readable events, the channel
     * is readable.
     */
    if (infoPtr->watchMask & TCL_READABLE &&
            IocpLLIsNotEmpty(infoPtr->llPendingRecv)) {
        readyMask |= TCL_READABLE;
    }

    /*
     * If the watch mask is set to notify for writable events, and
     * outstanding sends are less than the resource cap allowed for
     * this socket, the channel is writable.
     */
    if (infoPtr->watchMask & TCL_WRITABLE && infoPtr->llPendingRecv
            && infoPtr->outstandingSends < infoPtr->outstandingSendCap) {
        readyMask |= TCL_WRITABLE;
    }

    if (readyMask) {
        Tcl_NotifyChannel(infoPtr->channel, readyMask);
    } else {
        /* This was a useless queue.  I want to know why! */
        __asm nop;
    }
    return 1;
#endif
}


void
GpibExitHandler(ClientData clientData)
{
   // TODO
}
void
GpibThreadExitHandler(ClientData clientData)
{
   // TODO
}
void
GpibEventSetupProc(ClientData clientData, int flags)
{
   // TODO
}
void
GpibEventCheckProc(ClientData clientData, int flags)
{
   // TODO
}
int
GpibEventProc(Tcl_Event *evPtr, int flags)
{
   // TODO
   return 0;
}
int
GpibRemovePendingEvents(Tcl_Event *evPtr, ClientData clientData)
{
    // TODO
    return 0;
}
int
GpibRemoveAllPendingEvents(Tcl_Event *evPtr, ClientData clientData)
{
    // TODO
    return 0;
}
