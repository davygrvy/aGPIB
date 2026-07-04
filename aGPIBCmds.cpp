#include "aGPIB.hpp"

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


    chan = Agpib_CreateChannel(brd, pad, sad);
    if (chan == (Tcl_Channel) NULL &&
		(ThreadIbsta() & ERR))
    {
	Tcl_SetObjResult(interp,
		Tcl_NewStringObj(gpib_error_string(ThreadIberr()),-1));
        return TCL_ERROR;
    }

    /* Set initial fconfigs directly */

    /* 
     * All calls to Send()/Receive() address the bus directly, performs
     * the operation, and blocks until complete.
     * 
     * We won't be using ibwrta()/ibrda() as they don't seem to add value
     * for us by splitting initiation and completion.
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

    /* If the interp is deleted, the channel will be closed */
    Tcl_RegisterChannel(interp, chan);

    Tcl_SetObjResult(interp,
	    Tcl_NewStringObj(Tcl_GetChannelName(chan), -1));
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