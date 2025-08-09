/* --------------------------------------------------------------------
 *
 * aGPIB.c --
 *
 * 	'Asynchronous General Purpose Interface Buss' for Tcl.
 * 
 *	This extension adds a new channel type to Tool Command Language
 * 	that allows for easy and modern communication with devices
 * 	plugged into a GPIB buss.  Linux and Windows friendly.
 *
 * --------------------------------------------------------------------
 * RCS: @(#) $Id: $
 * --------------------------------------------------------------------
 */

#include "aGPIB.h"

/* Globals */
int initialized = 0;
#ifdef __WIN32__
HMODULE agpibModule = '\0';
#endif

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
#endif


#if defined(__WIN32__) && !defined(STATIC_BUILD)
BOOL APIENTRY 
DllMain (HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
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

    if (!initialized) {
	initialized = 1;
	/* TODO */
    }

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

