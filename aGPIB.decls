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
#     agpibPlat	   - platform specific public
#     agpibInt	   - generic private
#     agpibPlatInt - platform specific private

interface agpib
hooks {agpibInt}

# Declare each of the functions in the public Tcl interface.  Note that
# an index should never be reused for a different function in order to
# preserve backwards compatibility.

declare 0 generic {
    int Agpib_Init (Tcl_Interp *interp)
}
declare 1 generic {
    int Agpib_SafeInit (Tcl_Interp *interp)
}

declare 3 generic {
    Tcl_Channel Agpib_OpenDevice (Tcl_Interp *interp,
	CONST char *port, CONST char *host, CONST char *myaddr,
	CONST char *myport, int async)
}

###  Some Win32 error stuff the core is missing.

declare 0 win {
    CONST char *Tcl_WinErrId (void)
}
declare 1 win {
    CONST char *Tcl_WinErrMsg (void)
}
declare 2 win {
    CONST char *Tcl_WinError (Tcl_Interp *interp)
}

interface agpibInt

declare 0 generic {
    Tcl_Obj * DecodeIpSockaddr (SocketInfo *info, LPSOCKADDR addr)
}

