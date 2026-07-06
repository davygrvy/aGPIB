/* --------------------------------------------------------------------
 *
 * aGPIB.cpp --
 *
 * 	'Asynchronous General Purpose Interface Bus' for Tcl.
 * 
 *	This extension adds a new channel type to Tool Command Language
 * 	that allows for easy communication with devices plugged into a
 *	GPIB bus.  Linux and Windows friendly.
 *
 * --------------------------------------------------------------------
 * RCS: @(#) $Id: $
 * --------------------------------------------------------------------
 */

#include "aGPIBInt.hpp"

/* A mess of stuff to make sure we get a good binary. */
#if defined(__WIN32__) && defined(_MSC_VER)
    // Only do this when MSVC++ is compiling us.
#   if defined(USE_TCL_STUBS)
	// Mark this .obj as needing tcl's Stubs library.
#	pragma comment(lib, "tclstub" \
		STRINGIFY(JOIN(TCL_MAJOR_VERSION,TCL_MINOR_VERSION)) ".lib")
#	if !defined(_MT) || !defined(_DLL) || defined(_DEBUG)
	    // This fixes an old bug with how the Stubs library was
	    // compiled. The requirement for msvcrt.lib from
	    // tclstubXX.lib should be removed.
#	    pragma comment(linker, "-nodefaultlib:msvcrt.lib")
#	endif
#   elif !defined(STATIC_BUILD)
	// Mark this .obj needing the tcl import library
#	pragma comment(lib, "tcl" \
		STRINGIFY(JOIN(TCL_MAJOR_VERSION,TCL_MINOR_VERSION)) ".lib")
#   endif
#   pragma comment (lib, "user32.lib")
#   pragma comment (lib, "kernel32.lib")
#   pragma comment (lib, "ni4882.obj")   /* not an import library :) */
#endif


#if defined(__WIN32__) && !defined(STATIC_BUILD)
HMODULE agpibModule = NULL;

BOOL APIENTRY 
DllMain (HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    switch (dwReason) {
	case DLL_PROCESS_ATTACH:
	    /* don't call DLL_THREAD_ATTACH; I don't care to know. */
	    DisableThreadLibraryCalls(hModule);
	    agpibModule = hModule;
	    break;
	case DLL_PROCESS_DETACH:
	    /* TODO */
	    break;
    }
    return TRUE;
}
#endif


int
Agpib_Init(Tcl_Interp *interp)
{
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
	return TCL_ERROR;
    }
#endif

    if (TCL_ERROR == InitializeGpibSubSystem(interp)) {
	return TCL_ERROR;
    }

    Tcl_CreateObjCommand(interp, "aGPIB::open", Agpib_OpenObjCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "aGPIB::trigger", Agpib_TriggerObjCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "aGPIB::clear", Agpib_ClearObjCmd, 0, 0);
    Tcl_PkgProvide(interp, "aGPIB", AGPIB_VERSION);
    return TCL_OK;
}


/*
 * Until told by someone else that this isn't correct, GPIB communication
 * is not protected.
 */
int
Agpib_SafeInit(Tcl_Interp *interp)
{
    return Agpib_Init(interp);
}


/* The NI library on Windows is missing this */
#if defined(__WIN32__)
const char* gpib_error_string(int error)
{
    static const char* error_descriptions[] =
    {
	    "EDVR 0: OS error",
	    "ECIC 1: Board not controller in charge",
	    "ENOL 2: No listeners",
	    "EADR 3: Improper addressing",
	    "EARG 4: Bad argument",
	    "ESAC 5: Board not system controller",
	    "EABO 6: Operation aborted",
	    "ENEB 7: Non-existant board",
	    "EDMA 8: DMA error",
	    "libgpib: Unknown error code 9",
	    "EOIP 10: IO operation in progress",
	    "ECAP 11: Capability does not exist",
	    "EFSO 12: File system error",
	    "libgpib: Unknown error code 13",
	    "EBUS 14: Bus error",
	    "ESTB 15: Lost status byte",
	    "ESRQ 16: Stuck service request",
	    "ECNF 17: Configuration file error",
	    "libgpib: Unknown error code 18",
	    "libgpib: Unknown error code 19",
	    "ETAB 20: Table problem",
    };
    static const int max_error_code = ETAB;

    if (error < 0 || error > max_error_code)
	return "libgpib: Unknown error code";

    return error_descriptions[error];
}
#endif