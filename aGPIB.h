/* ----------------------------------------------------------------------
 *
 * aGPIB.h --
 *
 *	Main header file for the shared stuff.
 *
 * ----------------------------------------------------------------------
 * RCS: @(#) $Id: $
 * ----------------------------------------------------------------------
 */

#ifndef INCL_aGPIB_h_
#define INCL_aGPIB_h_


#include "tcl.h"


#define AGPIB_MAJOR_VERSION	1
#define AGPIB_MINOR_VERSION	0
#define AGPIB_RELEASE_LEVEL	TCL_ALPHA_RELEASE
#define AGPIB_RELEASE_SERIAL	1

#define AGPIB_VERSION		"1.0"
#define AGPIB_PATCH_LEVEL	"1.0a1"


#ifndef RC_INVOKED

#ifdef __WINDOWS__
#   include <ni488.h>		/* The National Instruments interface. */
#else
#   include <gpib/ib.h>		/* The Linux-GPIB FOSS interface. */
#   include <errno.h>
#endif

#undef TCL_STORAGE_CLASS
#ifdef BUILD_agpib
#   define TCL_STORAGE_CLASS DLLEXPORT
#else
#   ifdef USE_AGPIB_STUBS
#	define TCL_STORAGE_CLASS
#   else
#	define TCL_STORAGE_CLASS DLLIMPORT
#   endif
#endif


/*
 * Fix the Borland bug that's in the EXTERN macro from tcl.h.
 */
#ifndef TCL_EXTERN
#   undef DLLIMPORT
#   undef DLLEXPORT
#   ifdef __cplusplus
#	define TCL_EXTERNC extern "C"
#   else
#	define TCL_EXTERNC extern
#   endif
#   if defined(STATIC_BUILD)
#	define DLLIMPORT
#	define DLLEXPORT
#	define TCL_EXTERN(RTYPE) TCL_EXTERNC RTYPE
#   elif (defined(__WIN32__) && ( \
	    defined(_MSC_VER) || (__BORLANDC__ >= 0x0550) || \
	    defined(__LCC__) || defined(__WATCOMC__) || \
	    (defined(__GNUC__) && defined(__declspec)) \
	)) || (defined(MAC_TCL) && FUNCTION_DECLSPEC)
#	define DLLIMPORT __declspec(dllimport)
#	define DLLEXPORT __declspec(dllexport)
#	define TCL_EXTERN(RTYPE) TCL_EXTERNC TCL_STORAGE_CLASS RTYPE
#   elif defined(__BORLANDC__)
#	define DLLIMPORT __import
#	define DLLEXPORT __export
	/* Pre-5.5 Borland requires the attributes be placed after the */
	/* return type instead. */
#	define TCL_EXTERN(RTYPE) TCL_EXTERNC RTYPE TCL_STORAGE_CLASS
#   else
#	define DLLIMPORT
#	define DLLEXPORT
#	define TCL_EXTERN(RTYPE) TCL_EXTERNC TCL_STORAGE_CLASS RTYPE
#   endif
#endif

struct _GpibInfo;

struct _GpibInfo {
    Tcl_Channel chan;   /* us! */
    int ud;		/* board or device descriptor */
    Addr4882_t addr;	/* device address */
    int eot_mode;
    int term;		/* termination character */
    int timeout;
    short STB_Q;
    Tcl_ThreadId thrd;	/* notifier thread this channel belongs to */
    struct _GpibInfo *next;	/* link chain */
};

typedef struct _GpibInfo GpibInfo;

/*
 * Include the public function declarations that are accessible via the
 * stubs table.
 */

#include "agpibDecls.h"

/*
 * Include platform specific public function declarations that are
 * accessible via the stubs table.
 */

#include "agpibPlatDecls.h"


#ifdef USE_AGPIB_STUBS
    TCL_EXTERNC CONST char *
	Agpib_InitStubs (Tcl_Interp *interp, CONST char *version,
		int exact);
#else
#   define Agpib_InitStubs(interp, version, exact) \
	Tcl_PkgRequire(interp, "aGPIB", version, exact)
#endif


#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif  /* #ifndef RC_INVOKED */
#endif /* #ifndef INCL_agpib_h_ */
