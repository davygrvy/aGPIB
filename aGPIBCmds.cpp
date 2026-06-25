#include "aGPIB.h"

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


    chan = Agpib_CreateChannel(board_index, pad, sad);
    if (chan == (Tcl_Channel) NULL &&
		(GPIB::ThreadIbsta() & GPIB::ERR))
    {
	Tcl_SetErrno(EINVAL)
        return TCL_ERROR;
    }

    Tcl_RegisterChannel(interp, chan);            
    Tcl_AppendResult(interp, Tcl_GetChannelName(chan), NULL);

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