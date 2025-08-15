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

/* local prototypes */
static int			InitializeGpibSubSystem();
static Tcl_ExitProc		GpibExitHandler;
static Tcl_ExitProc		GpibThreadExitHandler;
static Tcl_EventSetupProc	GpibEventSetupProc;
static Tcl_EventCheckProc	GpibEventCheckProc;
static Tcl_EventProc		GpibEventProc;
static Tcl_EventDeleteProc	GpibRemovePendingEvents;
static Tcl_EventDeleteProc	GpibRemoveAllPendingEvents;

static GpibInfo *FindChannelFromAddr (int board_desc, Addr4882_t address);
static void TranslateGpibErr2Tcl(Tcl_Channel chan, int errVal);
static Tcl_ThreadCreateProc	GpibSRQNotifier;

static Tcl_DriverBlockModeProc	GpibBlockProc;
static Tcl_DriverCloseProc	GpibCloseProc;
static Tcl_DriverInputProc	GpibInputProc;
static Tcl_DriverGetOptionProc	GpibGetOptionProc;
static Tcl_DriverOutputProc	GpibOutputProc;
static Tcl_DriverSetOptionProc	GpibSetOptionProc;
static Tcl_DriverWatchProc	GpibWatchProc;
static Tcl_DriverThreadActionProc GpibThreadActionProc;

Tcl_ChannelType AgpibChannelType = {
    (char*)"gpib",	    /* Type name. */
    TCL_CHANNEL_VERSION_4,  /* TIP #218. */
    GpibCloseProc,	    /* Close proc. */
    GpibInputProc,	    /* Input proc. */
    GpibOutputProc,	    /* Output proc. */
    NULL,		    /* Seek proc. */
    GpibSetOptionProc,	    /* Set option proc. */
    GpibGetOptionProc,	    /* Get option proc. */
    GpibWatchProc,	    /* Set up notifier to watch this channel. */
    NULL,	    	    /* Get an OS handle from channel. */
    NULL,		    /* close2proc. */
    GpibBlockProc,	    /* Set device into (non-)blocking mode. */
    NULL,		    /* flush proc. */
    NULL,		    /* handler proc. */
    NULL,		    /* wide seek */
    GpibThreadActionProc,   /* TIP #218. */
};

typedef struct {
    int board_desc;
    Addr4882_t *addressList[];
} BrdInfo;

static GpibInfo *start;

static GpibInfo *
FindChannelFromAddr (
    int board_desc,
    Addr4882_t address)
{
    //GpibInfo *temp;

    /* TODO */
    return NULL;
}

static Tcl_ThreadCreateType
GpibSRQNotifier (
    ClientData clientData)
{
    BrdInfo *brdInfoPtr = (BrdInfo *) clientData;
    GpibInfo *infoPtr;
    short result, status, index;

again:
    /* Wait on this board */
    WaitSRQ(brdInfoPtr->board_desc, &result);

    switch (result) {
	case 0:  /* timed-out */

	    /* are we done? */
	    if (0) goto done;
	    goto again;

	case 1:  /* SRQ of this board has been asserted */

	    /*
	     * What device is responsible and grab its status byte
	     */
	    FindRQS(brdInfoPtr->board_desc, *(brdInfoPtr->addressList), &status);
	    index = ThreadIbcnt();
	    if (index != 0) {

		/*
		 * Resolve the channel info struct from the gpib address.
		 */
		infoPtr = FindChannelFromAddr(brdInfoPtr->board_desc,
			*(brdInfoPtr->addressList)[index-1]);

		/* save the status byte to the device's STB queue */
		infoPtr->STB_Q.push_back(status);

		/*
		 * Cause the Tcl notifier in the channel's target thread to
		 * check its event sources.
		 */
		Tcl_ThreadAlert(infoPtr->thrd);

	    } else {
		/* device not known or not open..  bad juju going on here */
		asm ("nop");
	    }
	    goto again;
    }
done:
    TCL_THREAD_CREATE_RETURN;
}

static void
TranslateGpibErr2Tcl(
    Tcl_Channel chan,
    int errVal)
{
    const char *msg = NULL;

    switch (errVal) {
	case EFSO:
	case EDVR:
	    /*
	     * A system call or file system call has failed. ibcnt/ibcntl will
	     * be set to the value of errno.
	     */
	    Tcl_SetErrno(ThreadIbcnt());  /* force this for windows, just in case */
	    return;

	case ECIC:
	    msg = "Your interface board needs to be Controller-In-Charge, but "
	     	  "is not.";
	    break;

	case ENOL:
	    msg = "You have attempted to write data or command bytes, but "
		  "there are no listeners currently addressed.";
	    break;

	case EADR:
	    msg = "The interface board has failed to address itself properly "
		  "before starting an i/o operation.";
	    break;

	case EARG:
	    /*
	     * One or more arguments to the function call were invalid.
	     */
	    Tcl_SetErrno(EINVAL);
	    return;

	case ESAC:
	    msg = "The interface board needs to be system controller, but "
		  "is not.";
	    break;

	case EABO:
	    msg = "A read or write of data bytes has been aborted, possibly "
		  "due to a timeout or reception of a device clear command.";
	    break;

	case ENEB:
	    msg = "The GPIB interface board does not exist, its driver is "
		  "not loaded, or it is not configured properly.";
	    break;

	case EDMA:
	    msg = "DMA error.";
	    break;

	case EOIP:
	    msg = "Function call can not proceed due to an asynchronous "
		  "IO operation (ibrda(), ibwrta(), or ibcmda()) in progress.";
	    break;

	case ECAP:
	    msg = "Incapable of executing function call, due the GPIB board "
		  "lacking the capability, or the capability being disabled in "
		  "software.";
	    break;

	case EBUS:
	    msg = "An attempt to write command bytes to the bus has timed out.";
	    break;

	case ESTB:
	    msg = "One or more serial poll status bytes have been lost. "
		  "This can occur due to too many status bytes accumulating "
		  "(through automatic serial polling) without being read.";
	    break;

	case ESRQ:
	    msg = "The serial poll request service line is stuck on. This "
		  "can occur if a physical device on the bus requests "
		  "service, but its GPIB address has not been opened (via "
		  "ibdev() for example) by any process. Thus the automatic "
		  "serial polling routines are unaware of the device's "
		  "existence and will never serial poll it.";
	    break;

	case ETAB:
	    msg = "This error can be returned by ibevent(), FindLstn(), or "
		  "FindRQS(). See their descriptions for more information.";
	    break;
    }
    Tcl_SetChannelError(chan, Tcl_NewStringObj(msg, -1));
    return;
}

static int
GpibCloseProc (
    ClientData instanceData,	/* The GPIB device state. */
    Tcl_Interp *interp)		/* Unused. */
{
    //GpibInfo *infoPtr = (GpibInfo *) instanceData;
    int errorCode = 0;

    /* TODO */

    return errorCode;
}

static int
GpibInputProc (
    ClientData instanceData,	/* The GPIB device state. */
    char *buf,			/* Where to store data. */
    int toRead,			/* Maximum number of bytes to read. */
    int *errorCodePtr)		/* Where to store errno codes. */
{
    GpibInfo *infoPtr = (GpibInfo *) instanceData;
    *errorCodePtr = 0;

    Receive(infoPtr->ud, infoPtr->addr, buf, (long)toRead, infoPtr->term);

    if (ThreadIbsta() && ERR) {
	TranslateGpibErr2Tcl(infoPtr->chan, ThreadIberr());
	*errorCodePtr = Tcl_GetErrno();
	return -1;
    } else if (ThreadIbsta() && TIMO) {
	/* TODO: not sure on this */
	Tcl_SetErrno(EWOULDBLOCK);
	*errorCodePtr = Tcl_GetErrno();
	return -1;
    } else {
	/* return how much we read */
	return ThreadIbcnt();
    }
}

static int
GpibOutputProc (
    ClientData instanceData,	/* The GPIB device state. */
    CONST char *buf,		/* Where to get data. */
    int toWrite,		/* Maximum number of bytes to write. */
    int *errorCodePtr)		/* Where to store errno codes. */
{
    GpibInfo *infoPtr = (GpibInfo *) instanceData;
    *errorCodePtr = 0;


    if (0/*TclInExit() || infoPtr->flags & IOCP_CLOSING*/) {
	*errorCodePtr = ENOTCONN;
	return -1;
    }

    Send(infoPtr->ud, infoPtr->addr, buf, (long)toWrite, infoPtr->eot_mode);

    if (ThreadIbsta() && ERR) {
	TranslateGpibErr2Tcl(infoPtr->chan, ThreadIberr());
	*errorCodePtr = Tcl_GetErrno();
	return -1;
    } else if (ThreadIbsta() && TIMO) {
	/* TODO: not sure on this */
	Tcl_SetErrno(EWOULDBLOCK);
	*errorCodePtr = Tcl_GetErrno();
	return -1;
    } else {
	/* return how much we wrote */
	return ThreadIbcnt();
    }
}

static int
GpibSetOptionProc (
    ClientData instanceData,	/* The GPIB device state. */
    Tcl_Interp *interp,		/* For error reporting - can be NULL. */
    CONST char *optionName,	/* Name of the option to set. */
    CONST char *value)		/* New value for option. */
{
    //GpibInfo *infoPtr = (GpibInfo *) instanceData;

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
    ClientData instanceData,	/* The GPIB device state. */
    int mask)			/* Events of interest; an OR-ed
				 * combination of TCL_READABLE,
				 * TCL_WRITABLE and TCL_EXCEPTION. */
{
    //GpibInfo *infoPtr = (GpibInfo *) instanceData;
}

static int
GpibBlockProc (
    ClientData instanceData,	/* The GPIB device state. */
    int mode)			/* TCL_MODE_BLOCKING or
                                 * TCL_MODE_NONBLOCKING. */
{
    GpibInfo *infoPtr = (GpibInfo *) instanceData;

    //if (!initialized) return 0;

    if (mode == TCL_MODE_NONBLOCKING) {
	ibtmo(infoPtr->ud, infoPtr->timeout);
    } else {
	ibtmo(infoPtr->ud, TNONE);
    }
    return 0;
}

#ifdef TCL_CHANNEL_VERSION_4
static void
GpibThreadActionProc (ClientData instanceData, int action)
{
    GpibInfo *infoPtr = (GpibInfo *) instanceData;

    //if (initialized) {
	/* This lock is to prevent IocpZapTclNotifier() from accessing
	* infoPtr->tsdHome */
	//EnterCriticalSection(&infoPtr->tsdLock);
	switch (action) {
	case TCL_CHANNEL_THREAD_INSERT:
	    infoPtr->thrd = Tcl_GetCurrentThread();
	    break;
	case TCL_CHANNEL_THREAD_REMOVE:
	    /* Unable to turn off RQS checking, therefore don't do anything and hope we don't have a collision */
	    break;
	}
	//LeaveCriticalSection(&infoPtr->tsdLock);
    //}
}
#endif

int
InitializeGpibSubSystem()
{
    // TODO
    return 0;
}
void
GpibExitHandler(ClientData clientData)
{
}
void
GpibThreadExitHandler(ClientData clientData)
{
}
void
GpibEventSetupProc(ClientData clientData, int flags)
{
}
void
GpibEventCheckProc(ClientData clientData, int flags)
{
}
int
GpibEventProc(Tcl_Event *evPtr, int flags)
{
    return 0;
}
int
GpibRemovePendingEvents(Tcl_Event *evPtr, ClientData clientData)
{
    return 0;
}
int
GpibRemoveAllPendingEvents(Tcl_Event *evPtr, ClientData clientData)
{
    return 0;
}
