# aGPIB.decls --
#
#	This file contains the declarations for all supported public
#	functions that are exported by the aGPIB library via the stubs table.
#	This file is used to generate the agpibDecls.h file.
#	
#
# Copyright (c) 1998-1999 by Scriptics Corporation.
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: $Id: $

library agpib

# Define the agpib interface with several sub interfaces:
#     aGPIBPlat	   - platform specific public
#     aGPIBInt	   - generic private
#     aGPIBPlatInt - platform specific private

interface aGPIB
hooks {aGPIBInt}

# Declare each of the functions in the public Tcl interface.  Note that
# an index should never be reused for a different function in order to
# preserve backwards compatibility.

declare 0 generic {
    int Agpib_Init (Tcl_Interp *interp)
}
declare 1 generic {
    int Agpib_SafeInit (Tcl_Interp *interp)
}

declare 2 generic {
    Tcl_Channel Agpib_CreateChannel (int board_index, int pad, int sad)
}

declare 3 generic {
    int Agpib_OpenObjCmd (ClientData notUsed, Tcl_Interp *interp,
	Tcl_Size objc, Tcl_Obj *CONST objv[])
}

declare 4 generic {
    int Agpib_TriggerObjCmd (ClientData notUsed, Tcl_Interp *interp,
	Tcl_Size objc, Tcl_Obj *CONST objv[])
}

interface aGPIBInt

declare 0 generic {
    int InitializeGpibSubSystem(Tcl_Interp *interp)
}

