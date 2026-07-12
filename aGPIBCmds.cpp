/* ----------------------------------------------------------------------
 *
 * aGPIBCmds.cpp --
 *
 * 	'Asynchronous General Purpose Interface Bus' for Tcl.
 *
 *	This extension adds a new channel type to Tool Command Language
 * 	that allows for easy communication with devices plugged into
 * 	a GPIB bus.  Linux and Windows friendly.
 *
 *	This file contains the commands provided to Tcl.
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

#include "aGPIBInt.hpp"

int
Agpib_OpenObjCmd (
    ClientData notUsed,			/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    Tcl_Size objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[])		/* Argument objects. */
{
    static CONST char *openOptions[] = {
	"-brd", "-pad", "-sad", NULL
    };
    enum openOptions {
	OPEN_BRD, OPEN_PAD, OPEN_SAD
    };
    Tcl_Channel chan;
    int brd = 0, pad = 0, sad = 0;

    if (Tcl_GetIndexFromObj(interp, objv[2], openOptions, "asd", 0,
	    &modeIndex) != TCL_OK) {
	return TCL_ERROR;
    }



    chan = Agpib_CreateChannel(brd, pad, sad);
    if (chan == (Tcl_Channel) NULL && (ThreadIbsta() & ERR)) {
	Tcl_SetObjResult(interp,
		Tcl_NewStringObj(gpib_error_string(ThreadIberr()),-1));
        return TCL_ERROR;
    }

    /* Set initial fconfigs directly */

    /* 
     * All calls to Send()/Receive() address the bus directly, performs
     * the operation, and are synchronous until complete.  An empty buffer
     * returns EWOULDBLOCK to state the connection is still alive, but
     * there is nothing there rather than the generic layer thinking the
     * connection is EOF.  So, in the traditional sense, this isn't
     * exactly blocking, but hybrid.
     * 
     * TODO: We won't be using the true asynchronous calls ibwrta()/ibrda()
     * (at first) as they don't seem to add value for us by splitting
     * initiation and completion.  Trying to set blocking off will return
     * an error.
     */
    if (Tcl_SetChannelOption(interp, chan, "-blocking",
	    "yes") == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, chan);
        return TCL_ERROR;
    }

    /* Message based protocols such as us don't need this */
    if (Tcl_SetChannelOption(interp, chan, "-translation",
	    "none") == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, chan);
        return TCL_ERROR;
    }

    /* 488.2 uses 7-bit data */
    if (Tcl_SetChannelOption(interp, chan, "-encoding",
	    "ascii") == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, chan);
        return TCL_ERROR;
    }

    /* If this interp is deleted, the channel will be closed */
    Tcl_RegisterChannel(interp, chan);

    Tcl_SetObjResult(interp,
	    Tcl_NewStringObj(Tcl_GetChannelName(chan), TCL_AUTO_LENGTH));
    return TCL_OK;
}

int
Agpib_TriggerObjCmd (
    ClientData notUsed,			/* Not used. */
    Tcl_Interp *interp,			/* Current interpreter. */
    Tcl_Size objc,			/* Number of arguments. */
    Tcl_Obj *CONST objv[])		/* Argument objects. */
{
    return TCL_ERROR;
}

int
Agpib_ClearObjCmd(
    ClientData notUsed,			/* Not used. */
    Tcl_Interp* interp,			/* Current interpreter. */
    Tcl_Size objc,			/* Number of arguments. */
    Tcl_Obj* CONST objv[])		/* Argument objects. */
{
    return TCL_ERROR;
}
