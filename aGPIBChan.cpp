/* ----------------------------------------------------------------------
 *
 * aGPIBChan.cpp --
 *
 * 	'Asynchronous General Purpose Interface Bus' for Tcl.
 * 
 *	This extension adds a new channel type to Tool Command Language
 * 	that allows for easy and modern communication with devices
 * 	plugged into a GPIB buss.  Linux and Windows friendly.
 * 
 *	This file defines the channel interface to Tcl.
 *
 * ----------------------------------------------------------------------
 *
 * Copyright (c) 2026 David Gravereaux <davygrvy@pobox.com>
 *
 * See the file "license.terms" for information on usage and
 * redistribution of this file, and for a DISCLAIMER OF ALL
 * WARRANTIES.
 *
* -----------------------------------------------------------------------
 * RCS: @(#) $Id: $
 * ----------------------------------------------------------------------
 */

#include "aGPIBInt.hpp"
#include <string.h>


/* local prototypes */
static Tcl_ExitProc		    AgpibExitHandler;
static Tcl_ExitProc		    AgpibThreadExitHandler;
static Tcl_EventSetupProc	    AgpibEventSetupProc;
static Tcl_EventCheckProc	    AgpibEventCheckProc;
static Tcl_EventProc		    AgpibEventProc;
static Tcl_EventDeleteProc	    AgpibRemovePendingEvents;
static Tcl_EventDeleteProc	    AgpibRemoveAllPendingEvents;

static GpibInfo *		    FindChannelFromAddr (int board_desc,
					    Addr4882_t address);
static void			    TranslateGpibErr2Tcl(Tcl_Channel chan,
					    int ibErr);
static Tcl_ThreadCreateProc	    AgpibSRQNotifier;
void				    ZapTclNotifier (GpibInfo *infoPtr,
					    short stb);

static Tcl_DriverBlockModeProc	    AgpibBlockProc;
static Tcl_DriverClose2Proc	    AgpibClose2Proc;
static Tcl_DriverInputProc	    AgpibInputProc;
static Tcl_DriverGetOptionProc	    AgpibGetOptionProc;
static Tcl_DriverOutputProc	    AgpibOutputProc;
static Tcl_DriverSetOptionProc	    AgpibSetOptionProc;
static Tcl_DriverWatchProc	    AgpibWatchProc;
static Tcl_DriverThreadActionProc   AgpibThreadActionProc;

Tcl_ChannelType AgpibChannelType = {
    (char*)"gpib",	    /* Type name. */
    TCL_CHANNEL_VERSION_5,  /* TIP #562 (mandatory) */
    NULL,		    /* Close proc. */
    AgpibInputProc,	    /* Input proc. */
    AgpibOutputProc,	    /* Output proc. */
    NULL,		    /* Seek proc. */
    AgpibSetOptionProc,	    /* Set option proc. */
    AgpibGetOptionProc,	    /* Get option proc. */
    AgpibWatchProc,	    /* Set up notifier to watch this channel. */
    NULL,		    /* Get an OS handle from channel. */
    AgpibClose2Proc,	    /* close2proc. */
    AgpibBlockProc,	    /* Set device into (non-)blocking mode. */
    NULL,		    /* flush proc. */
    NULL,		    /* handler proc. */
    NULL,		    /* wide seek */
    AgpibThreadActionProc,  /* thread move proc */
    NULL,		    /* truncate proc */
};

/* file scope */
static GpibInfo *start = NULL;
static bool shutdown = false;


typedef struct {
    int board_desc;
    //Addr4882_t *addressList[];
} BrdInfo;

int
InitializeGpibSubSystem(
    Tcl_Interp* interp)
{
    /* TODO */

    //  This is called once per
    //  Tcl interp loading via 'package require aGPIB'.

    return TCL_OK;
}


Tcl_Channel
Agpib_CreateChannel (
    int board_index,
    int pad,
    int sad)
{
    GpibInfo *infoPtr;
    Tcl_Channel chan;
    int result;
    char channelName[4 + TCL_INTEGER_SPACE];

    // TODO
    infoPtr = NewGPIBInfo();

    infoPtr->ud = ibdev(board_index, pad, sad, T300ms, 1, 0);
    if (result == -1) {
	// TODO
	return NULL;
    }

	//TODO start notifier thread, if needed.
    {
	Tcl_ThreadId id;
	Tcl_CreateThread(&id, AgpibSRQNotifier, (ClientData) BrdInfo, );
    }

    snprintf(channelName, 4 + TCL_INTEGER_SPACE, "gpib%u", infoPtr->ud);

    /*
     * TODO: !!!BUG!!!
     * The only acceptable watch mask is TCL_EXCEPTION.  We are catching
     * interrupts, not read or write notifications.  Due to how the
     * channel interface is structured, this extension would be the first
     * one to every want to set a fileevent script on exception events.
     * This will require a patch to Tcl to add the third event type.
     * 
     * https://www.tcl-lang.org/man/tcl9.0/TclCmd/fileevent.html
     * 
     * In the future, if we add the "exception" event to [fileevent],
     * readable and writable could then become the result of the
     * asyncronous calls ibrda() and ibwrta() which I don't see as being
     * particularly useful to us.  Splitting a read/write operation into
     * two halves (initiation and completion) doesn't add value that I
     * can see.  With GPIB, We get actual notifications to read and
     * write from the STB bit mask [bit 5 for 488.2 MAV and bit 6 for
     * ESB/OPC, respectively].  A notification to collect the read bytes
     * with possible error or the result of the last write call is
     * limited in its usefulness as I see it.
     */
#if TCL_MAJOR_VERSION < 9
    return Tcl_CreateChannel(&AgpibChannelType, channelName,
	    (ClientData) infoPtr, TCL_READABLE);
#else
    /* TIP #758: allow TCL_EXCEPTION as a watch mask */
    return Tcl_CreateChannel(&AgpibChannelType, channelName,
        (ClientData)infoPtr, TCL_EXCEPTION);
#endif
}


/* --------------------------------------------------------------------
 *
 * AgpibSRQNotifier --
 *
 *      This is our notifier routine to service interrupts.  It is run
 *      in a thread, one per board.
 *
 * --------------------------------------------------------------------
 */

static Tcl_ThreadCreateType
AgpibSRQNotifier (
    ClientData clientData)      /* The GPIB board to watch */
{
    BrdInfo *brdInfoPtr = (BrdInfo *) clientData;
    GpibInfo *infoPtr;
    short result;
    Addr4882_t allBusAddresses[gpib_addr_max+1];
    short statusList[gpib_addr_max+1]; 

    /* Fill the array with all addresses */
    for (int i = 0; i < gpib_addr_max; i++)
            allBusAddresses[i] = MakeAddr(i+1, 0); /* 1 to 30 */
    allBusAddresses[gpib_addr_max] = NOADDR;

    /* Set a 3 second timeout */
    ibtmo(brdInfoPtr->board_desc, T3s);
    if (ThreadIbsta() & ERR) {
        /* Bad error */
        goto done;
    }

    /* Disable automatic polling (The NI 488.2 driver does this by
     * default, so unset) as this is what we are doing here ourselves. */
    ibconfig(brdInfoPtr->board_desc, IbcAUTOPOLL, 0);
    if (ThreadIbsta() & ERR) {
        /* Bad error */
        goto done;
    }

again:
    /* Sleep until timeout or any device pulls the physical SRQ line low */
    WaitSRQ(brdInfoPtr->board_desc, &result);
    if (ThreadIbsta() & ERR) {
        /* Bad error or the board went offline (we can't trust result) */
        goto done;
    }

    switch (result) {
    case 0: /* Timed-out */
        if (shutdown) goto done;    /* clean exit */
        goto again;

    case 1: /* SRQ Asserted */
        /* Sweep every address on the bus . This reads and CLEARS the SRQ line
         * for BOTH known and unknown instruments. */
        AllSpoll(brdInfoPtr->board_desc, allBusAddresses, statusList);

        /* Process the results */
        for (int i = 0; allBusAddresses[i] != NOADDR; i++) {

            /* Does this address request service? (Bit 6 / 0x40 RQS Flag) */
            if (statusList[i] & IbStbRQS) {

                /* Resolve to our channel */
                infoPtr = FindChannelFromAddr(brdInfoPtr->board_desc,
                        allBusAddresses[i]);

                if ((infoPtr != NULL)
#if TCL_MAJOR_VERSION < 9
                        && (infoPtr->watchMask & TCL_READABLE)) {
#else
			/* TIP #758: allow TCL_EXCEPTION as a watch mask */
                        && (infoPtr->watchMask & TCL_EXCEPTION)) {
#endif
                    /* Active channel found: Push the status byte,
                     * and alert Tcl */
                    ZapTclNotifier(infoPtr, statusList[i]);
                } else {
                    /* Rouge device disarmed! */
                    ;
                }
            }
        }
        goto again;
    }
    
done:
    TCL_THREAD_CREATE_RETURN;
}

static void
ZapTclNotifier (
    GpibInfo *infoPtr,
    short stb)
{
    infoPtr->STB_Q.push_back((std::uint8_t)stb);

    // TODO: ready list in target notifier thread.
    xxx.readyDevices.push_back(infoPtr);

    /* Wake the Tcl notifier to service (at least)
     * this channel type's event source. */
    Tcl_ThreadAlert(infoPtr->thrd);
}

static void
TranslateGpibErr2Tcl(
    Tcl_Channel chan,           /* The channel */
    int ibErr)                  /* The GPIB error code */
{
    Tcl_SetChannelError(chan, Tcl_NewStringObj(
	        gpib_error_string(ibErr), TCL_AUTO_LENGTH));
}

static int
AgpibClose2Proc (
    ClientData instanceData,    /* The GPIB device state. */
    Tcl_Interp *interp,	        /* Unused. */
    int flags)
{
    GpibInfo *infoPtr = (GpibInfo *) instanceData;
    int errorCode = TCL_OK;
    int status;

    if (infoPtr != NULL && flags == 0) {
        infoPtr->flags |= AGPIB_CLOSING;

        status = ibonl(infoPtr->ud, 0);

        if (status & ERR) {
            Tcl_SetErrno(EIO);
            errorCode = Tcl_GetErrno();
	    if (interp != NULL) {
		Tcl_SetObjResult(interp,
			Tcl_NewStringObj(Tcl_ErrnoMsg(errorCode), TCL_AUTO_LENGTH));
	    }
        }

    }

    return errorCode;
}

static int
AgpibInputProc (
    ClientData instanceData,  /* The GPIB device state. */
    char *buf,                /* Where to store data. */
    int toRead,               /* Maximum number of bytes to read. */
    int *errorCodePtr)        /* Where to store errno codes. */
{
    GpibInfo *infoPtr = (GpibInfo *) instanceData;
    *errorCodePtr = TCL_OK;
    int status;

    if ((infoPtr->flags & AGPIB_CLOSING)) {
	    *errorCodePtr = ENOTCONN;
    	return -1;
    }

    Receive(infoPtr->ud, infoPtr->addr, buf, (long)toRead, STOPend/*infoPtr->term*/);
    status = ThreadIbsta();

    if (status & ERR) {
	    TranslateGpibErr2Tcl(infoPtr->chan, ThreadIberr());
	    *errorCodePtr = Tcl_GetErrno();
	    return -1;
    }
 
    else if (status & TIMO) {
	    if (infoPtr->mode == TCL_MODE_BLOCKING) {
	        Tcl_SetErrno(ETIMEDOUT);
	    } else {
	        Tcl_SetErrno(EWOULDBLOCK);
	    }
	    *errorCodePtr = Tcl_GetErrno();
	    return -1;
    }
    
    /* return how much we read */
    return ThreadIbcnt();
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


    if ((infoPtr->flags & AGPIB_CLOSING)) {
	    *errorCodePtr = ENOTCONN;
	    return -1;
    }

    Send(infoPtr->ud, infoPtr->addr, buf, (long)toWrite, DABend/*infoPtr->eot_mode*/);
    status = ThreadIbsta();

    if (status & ERR) {
	    TranslateGpibErr2Tcl(infoPtr->chan, ThreadIberr());
	    *errorCodePtr = Tcl_GetErrno();
	    return -1;
    }
    
    else if (status & TIMO) {
	    if (infoPtr->mode == TCL_MODE_NONBLOCKING) {
	        Tcl_SetErrno(EWOULDBLOCK);
	    } else {
	        Tcl_SetErrno(ETIMEDOUT);
	    }
	    *errorCodePtr = Tcl_GetErrno();
	    return -1;
    }
    
    /* return how much we wrote */
    return ThreadIbcnt();
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
    ClientData instanceData,	/* The GPIB device state. */
    Tcl_Interp *interp,		/* For error reporting - can be NULL */
    CONST char *optionName,	/* Name of the option to
				 * retrieve the value for, or
                 		 * NULL to get all options and
				 * their values. */
    Tcl_DString *dsPtr)		/* Where to store the computed
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
        case TNONE:
            Tcl_DStringAppendElement(dsPtr, "none"); break;
        case T10us:
            Tcl_DStringAppendElement(dsPtr, "10us"); break;
        case T30us:
            Tcl_DStringAppendElement(dsPtr, "30us"); break;
        case T100us:
            Tcl_DStringAppendElement(dsPtr, "100us"); break;
        case T300us:
            Tcl_DStringAppendElement(dsPtr, "300us"); break;
        case T1ms:
            Tcl_DStringAppendElement(dsPtr, "1ms"); break;
        case T3ms:
            Tcl_DStringAppendElement(dsPtr, "3ms"); break;
        case T10ms:
            Tcl_DStringAppendElement(dsPtr, "10ms"); break;
        case T30ms:
            Tcl_DStringAppendElement(dsPtr, "30ms"); break;
        case T100ms:
            Tcl_DStringAppendElement(dsPtr, "100ms"); break;
        case T300ms:
            Tcl_DStringAppendElement(dsPtr, "300ms"); break;
        case T1s:
            Tcl_DStringAppendElement(dsPtr, "1s"); break;
        case T3s:
            Tcl_DStringAppendElement(dsPtr, "3s"); break;
        case T10s:
            Tcl_DStringAppendElement(dsPtr, "10s"); break;
        case T30s:
            Tcl_DStringAppendElement(dsPtr, "30s"); break;
        case T100s:
            Tcl_DStringAppendElement(dsPtr, "100s"); break;
        case T300s:
            Tcl_DStringAppendElement(dsPtr, "300s"); break;
        case T1000s:
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
    GpibInfo *infoPtr = (GpibInfo *) instanceData;

    infoPtr->watchMask = mask;
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
	/* Non-blocking is invalid */

	/* 
	 * All calls to Send()/Receive() address the bus directly and perform
	 * the operation and block until complete.
	 * 
	 * We won't be using ibwrta()/ibrda() as they don't seem to add value
	 * for us by splitting initiation and completion.
	 */
	return EINVAL;
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

static GpibInfo *
FindChannelFromAddr (
    int board_desc,
    Addr4882_t addr)
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
    infoPtr->watchMask = 0;
    infoPtr->mode = TCL_MODE_BLOCKING;
    infoPtr->board_desc = 0;
    infoPtr->ud = 0;
    infoPtr->addr = 0;
    infoPtr->eot_mode = DABend;
    infoPtr->timeout = T300us;
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
     * If any ready events exist now, don't let the notifier go into its
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
	/* TIP #758: allow TCL_EXCEPTION as a watch mask */
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
