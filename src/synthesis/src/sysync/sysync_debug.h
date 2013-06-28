/*
 *  File:         sysync_debug.h
 *
 *  Author:			  Lukas Zeller (luz@synthesis.ch)
 *
 *  Global debug related definitions
 *
 *  Copyright (c) 2004-2009 by Synthesis AG (www.synthesis.ch)
 *
 *  2004-08-31 : luz : created
 *
 */

#ifndef SYSYNC_DEBUG_H
#define SYSYNC_DEBUG_H

#include "generic_types.h"


#ifdef __cplusplus
using namespace std;
namespace sysync {
#endif

#ifndef __PALM_OS__
  #if defined __MACH__ && !defined __GNUC__           /* used for va_list support */
    #include <mw_stdarg.h>
  #else
    #include <stdarg.h>
  #endif
#endif

#ifdef __cplusplus
  #define SYSYCDECL extern "C"
#else
  #define SYSYCDECL
#endif

/* REMARK: do not define it again for C
/// @todo get rid of all GDEBUG references later
#warning "get rid of all GDEBUG references later"
#define GDEBUG getDbgMask() // use accessor, depends on context which one is used
*/

// non-class debug output functions
SYSYCDECL void DebugVPrintf(uInt32 mask, const char *format, va_list args);
SYSYCDECL void DebugPrintf(const char *text, ...);
SYSYCDECL void DebugPuts(uInt32 mask, const char *text);
SYSYCDECL uInt32 getDbgMask(void);
#ifdef __cplusplus
class TDebugLogger;
TDebugLogger *getDbgLogger(void);
#endif

// debug level masks
// - main categories
#define DBG_HOT        0x00000001    // hot information
#define DBG_ERROR      0x00000002    // Error conditions
// - specialized info categories
#define DBG_PROTO      0x00000010    // directly SyncML protocol related info
#define DBG_SESSION    0x00000020    // more internal Session management stuff
#define DBG_ADMIN      0x00000040    // administrative data management
#define DBG_DATA       0x00000080    // content data management (but needs DBG_USERDATA to show actual data)
#define DBG_REMOTEINFO 0x00000100    // information we get about remote party (e.g. devInf or remote rule)
#define DBG_PARSE      0x00000200    // parsing external formats into internal ones
#define DBG_GEN        0x00000400    // generate external formats from internal ones
#define DBG_SCRIPTEXPR 0x00000800    // Script expression details
#define DBG_SCRIPTS    0x00001000    // Script execution
#define DBG_TRANSP     0x00002000    // transport debug (outside SySync, e.g. ISAPI)
#define DBG_REST       0x00008000    // everything not categorized, includes all DEBUGPRINTF
// - technical insider categories
#define DBG_LOCK       0x00010000    // Thread lock info
#define DBG_OBJINST    0x00020000    // object instantiation and deletion
#define DBG_PROFILE    0x00040000    // Execution time and memory profiling
// - external library debug info categories
#define DBG_RTK_SML    0x00100000    // SyncML Toolkit SML messages
#define DBG_RTK_XPT    0x00200000    // SyncML Toolkit XPT messages

// - flags to select more info, usually combined with other data
#define DBG_USERDATA   0x01000000    // user content data
#define DBG_DBAPI      0x02000000    // DB API data (such as SQL statements)
#define DBG_PLUGIN     0x04000000    // plugin debug messages (all plugins)
#define DBG_FILTER     0x08000000    // Details on filtering
#define DBG_MATCH      0x10000000    // Details on content matching (but no user data unless USERDATA is set as well)
#define DBG_CONFLICT   0x20000000    // Details on conflict resolution, such as data merge
// - more details
#define DBG_DETAILS    0x40000000    // show more detailed data
#define DBG_EXOTIC     0x80000000    // show even most exotic details

// - useful sets
#define DBG_ALL        0xFFFFFFFF
#define DBG_MINIMAL    (DBG_ERROR+DBG_HOT)
#define DBG_NORMAL     (DBG_MINIMAL  +DBG_DATA+DBG_ADMIN+DBG_PROTO+DBG_REMOTEINFO)
#define DBG_EXTENDED   (DBG_NORMAL   +DBG_TRANSP+DBG_DBAPI+DBG_PLUGIN+DBG_SESSION+DBG_PARSE+DBG_GEN+DBG_DETAILS+DBG_FILTER+DBG_CONFLICT+DBG_MATCH+DBG_USERDATA+DBG_RTK_SML)
#define DBG_MAXIMAL    ((DBG_EXTENDED +DBG_EXOTIC+DBG_SCRIPTS) & ~DBG_MATCH) // DBG_MATCH explicitly excluded, as it causes O(N^2) output with DBG_EXOTIC

// - special sets (internally used)
#define DBG_ALLDB      (DBG_DATA+DBG_ADMIN+DBG_DBAPI+DBG_PLUGIN) // what used to be "DBG_DB" (more or less)
#define DBG_TOOL       (DBG_HOT+DBG_ERROR+DBG_ALLDB+DBG_SCRIPTS+DBG_TRANSP)  // what is shown for sysytest/sysytool in verbose mode


// output to console macro
#ifdef CONSOLEINFO
  #define CONSOLEPUTS(m) ConsolePuts(m)
  #define CONSOLEPRINTF(m) ConsolePrintf m
#else
  #define CONSOLEPUTS(m)
  #define CONSOLEPRINTF(m)
#endif


// debug output macros (prevents unnecessary printf argument
//   calculations when SYDEBUG is UNDEFined).

#ifndef DIRECT_APPBASE_GLOBALACCESS
  // without global access, all NC variants are always disabled
  #define PNCDEBUGPUTSX(lvl,m)
  #define PNCDEBUGPUTSXX(lvl,m,s,p)
  #define PNCDEBUGPRINTFX(lvl,m)
  #define PNCDEBUGVPRINTFX(lvl,f,a)
#endif

#ifdef SYDEBUG
  // "Public" debug info
  // Debug structure
  #define PDEBUGBLOCKFMT(m) getDbgLogger()->DebugOpenBlockExpanded m
  #define PDEBUGBLOCKFMTCOLL(m) getDbgLogger()->DebugOpenBlockCollapsed m
  #define PDEBUGBLOCKDESC(n,d) getDbgLogger()->DebugOpenBlock(n,d)
  #define PDEBUGBLOCKDESCCOLL(n,d) getDbgLogger()->DebugOpenBlock(n,d,true)
  #define PDEBUGBLOCK(n) getDbgLogger()->DebugOpenBlock(n)
  #define PDEBUGBLOCKCOLL(n) getDbgLogger()->DebugOpenBlock(n,NULL,true)
  #define PDEBUGENDBLOCK(n) getDbgLogger()->DebugCloseBlock(n)
  // current-class context debug output
  #define PDEBUGPUTSX(lvl,m) { if (((lvl) & getDbgMask()) == (lvl)) getDbgLogger()->DebugPuts(lvl,m); }
  #define PDEBUGPUTSXX(lvl,m,s,p) { if (((lvl) & getDbgMask()) == (lvl)) getDbgLogger()->DebugPuts(lvl,m,s,p); }
  #define PDEBUGPUTS(m) PDEBUGPUTSX(DBG_REST,m)
  #define PDEBUGVPRINTFX(lvl,f,a) { if (((lvl) & getDbgMask()) == (lvl)) getDbgLogger()->DebugVPrintf(lvl,f,a); }
  #define PDEBUGPRINTFX(lvl,m) { if (((lvl) & getDbgMask()) == (lvl)) getDbgLogger()->setNextMask(lvl).DebugPrintfLastMask m; }
  #define PDEBUGPRINTF(m) PDEBUGPRINTFX(DBG_REST,m)
  #define PPOINTERTEST(p,m) if (!p) getDbgLogger()->setNextMask(DBG_ERROR).DebugPrintfLastMask m
  #define PDEBUGTEST(lvl) (((lvl) & getDbgMask()) == (lvl))
  #define PDEBUGMASK getDbgMask()
  // direct output to a logger
  #define PLOGDEBUGBLOCKFMT(lo,m) (lo)->DebugOpenBlockExpanded m
  #define PLOGDEBUGBLOCKFMTCOLL(lo,m) (lo)->DebugOpenBlockCollapsed m
  #define PLOGDEBUGBLOCKDESC(lo,n,d) (lo)->DebugOpenBlock(n,d)
  #define PLOGDEBUGBLOCKDESCCOLL(lo,n,d) (lo)->DebugOpenBlock(n,d,true)
  #define PLOGDEBUGBLOCK(lo,n) (lo)->DebugOpenBlock(n)
  #define PLOGDEBUGBLOCKCOLL(lo,n) (lo)->DebugOpenBlock(n,NULL,true)
  #define PLOGDEBUGENDBLOCK(lo,n) (lo)->DebugCloseBlock(n)
  #define PLOGDEBUGPUTSX(lo,lvl,m) { if ((lo) &&((lvl) & (lo)->getMask()) == (lvl)) (lo)->DebugPuts(lvl,m); }
  #define PLOGDEBUGPUTSXX(lo,lvl,m,s,p) { if ((lo) &&((lvl) & (lo)->getMask()) == (lvl)) (lo)->DebugPuts(lvl,m,s,p); }
  #define PLOGDEBUGVPRINTFX(lo,lvl,f,a) { if ((lo) &&((lvl) & (lo)->getMask()) == (lvl)) (lo)->DebugVPrintf(lvl,f,a); }
  #define PLOGDEBUGPRINTFX(lo,lvl,m) { if ((lo) &&((lvl) & (lo)->getMask()) == (lvl)) (lo)->setNextMask(lvl).DebugPrintfLastMask m; }
  // non-class context or C-level debug output
  #ifdef DIRECT_APPBASE_GLOBALACCESS
    #ifdef __cplusplus
      #define PNCDEBUGPUTSX(lvl,m) { if (((lvl) & sysync::getDbgMask()) == (lvl)) sysync::DebugPuts(m); }
      #define PNCDEBUGPRINTFX(lvl,m) { if (((lvl) & sysync::getDbgMask()) == (lvl)) sysync::DebugPrintf m; }
      #define PNCDEBUGVPRINTFX(lvl,f,a) { if (((lvl) & sysync::getDbgMask()) == (lvl)) sysync::DebugVPrintf(lvl,f,a); }
    #else
      #define PNCDEBUGPUTSX(lvl,m) { if (((lvl) & getDbgMask()) == (lvl)) DebugPuts(m); }
      #define PNCDEBUGPRINTFX(lvl,m) { if (((lvl) & getDbgMask()) == (lvl)) DebugPrintf m; }
      #define PNCDEBUGVPRINTFX(lvl,f,a) { if (((lvl) & getDbgMask()) == (lvl)) DebugVPrintf(lvl,f,a); }
    #endif
  #endif
  // specified object-context debug output
  #define POBJDEBUGPRINTFX(obj,lvl,m) { if ((obj) && (((lvl) & (obj)->getDbgMask()) == (lvl))) (obj)->getDbgLogger()->setNextMask(lvl).DebugPrintfLastMask m; }
  #define POBJDEBUGPUTSX(obj,lvl,m) { if ((obj) && (((lvl) & (obj)->getDbgMask()) == (lvl))) (obj)->getDbgLogger()->DebugPuts(lvl,m); }
  #define POBJDEBUGPUTSXX(obj,lvl,m,s,p) { if ((obj) && (((lvl) & (obj)->getDbgMask()) == (lvl))) (obj)->getDbgLogger()->DebugPuts(lvl,m,s,p); }
  #define POBJDEBUGPRINTF(obj,m) POBJDEBUGPRINTFX(obj,DBG_REST,m)
  #define POBJDEBUGTEST(obj,lvl) ((obj) && (((lvl) & (obj)->getDbgMask()) == (lvl)))
  // get current logger
  #define GETDBGLOGGER (getDbgLogger())
  #define OBJGETDBGLOGGER(obj) (obj->getDbgLogger())

#else
  #define PDEBUGBLOCKFMT(m)
  #define PDEBUGBLOCKFMTCOLL(m)
  #define PDEBUGBLOCKDESC(n,d)
  #define PDEBUGBLOCKDESCCOLL(n,d)
  #define PDEBUGBLOCK(n)
  #define PDEBUGBLOCKCOLL(n)
  #define PDEBUGENDBLOCK(n)
  #define PDEBUGPUTSX(lvl,m)
  #define PDEBUGPUTSXX(lvl,m,s,p)
  #define PDEBUGPUTS(m)
  #define PDEBUGVPRINTFX(lvl,f,a)
  #define PDEBUGPRINTFX(lvl,m)
  #define PDEBUGPRINTF(m)
  #define PPOINTERTEST(p,m)
  #define PDEBUGTEST(lvl) false
  #define PDEBUGMASK 0
  #define PLOGDEBUGBLOCKFMT(lo,m)
  #define PLOGDEBUGBLOCKFMTCOLL(lo,m)
  #define PLOGDEBUGBLOCKDESC(lo,n,d)
  #define PLOGDEBUGBLOCKDESCCOLL(lo,n,d)
  #define PLOGDEBUGBLOCK(lo,n)
  #define PLOGDEBUGBLOCKCOLL(lo,n)
  #define PLOGDEBUGENDBLOCK(lo,n)
  #define PLOGDEBUGPUTSX(lo,lvl,m)
  #define PLOGDEBUGPUTSXX(lo,lvl,m,s,p)
  #define PLOGDEBUGVPRINTFX(lo,lvl,f,a)
  #define PLOGDEBUGPRINTFX(lo,lvl,m)
  #ifdef DIRECT_APPBASE_GLOBALACCESS
    #define PNCDEBUGPUTSX(lvl,m)
    #define PNCDEBUGPUTSXX(lvl,m,s,p)
    #define PNCDEBUGPRINTFX(lvl,m)
    #define PNCDEBUGVPRINTFX(lvl,f,a)
  #endif
  //#define PTHREADDEBUGPRINTFX(lvl,m)
  #define POBJDEBUGPRINTFX(obj,lvl,m)
  #define POBJDEBUGPUTSX(obj,lvl,m)
  #define POBJDEBUGPUTSXX(obj,lvl,m,s,p)
  #define POBJDEBUGPRINTF(obj,m)
  #define POBJDEBUGTEST(obj,lvl) false
  // get current logger
  #define GETDBGLOGGER NULL
  #define OBJGETDBGLOGGER(obj) NULL
#endif
#if SYDEBUG>1
  // full debugging, including private debug info
  #define DEBUGPUTSX(lvl,m) PDEBUGPUTSX(lvl,m)
  #define DEBUGPUTSXX(lvl,m,s,p) PDEBUGPUTSXX(lvl,m,s,p)
  #define DEBUGPUTS(m) PDEBUGPUTS(m)
  #define DEBUGVPRINTFX(lvl,f,a) PDEBUGVPRINTFX(lvl,f,a)
  #define DEBUGPRINTFX(lvl,m) PDEBUGPRINTFX(lvl,m)
  #define DEBUGPRINTF(m) PDEBUGPRINTF(m)
  #define POINTERTEST(p,m) PPOINTERTEST(p,m)
  #define DEBUGTEST(lvl) PDEBUGTEST(lvl)
  #define DEBUGMASK PDEBUGMASK
  #define NCDEBUGPUTSX(lvl,m) PNCDEBUGPUTSX(lvl,m)
  #define NCDEBUGPUTSXX(lvl,m,s,p) PNCDEBUGPUTSXX(lvl,m,s,p)
  #define NCDEBUGPRINTFX(lvl,m) PNCDEBUGPRINTFX(lvl,m)
  #define NCDEBUGVPRINTFX(lvl,f,a) PNCDEBUGVPRINTFX(lvl,f,a)
  #define LOGDEBUGBLOCKFMT(lo,m) PLOGDEBUGBLOCKFMT(lo,m)
  #define LOGDEBUGBLOCKFMTCOLL(lo,m) PLOGDEBUGBLOCKFMTCOLL(lo,m)
  #define LOGDEBUGBLOCKDESC(lo,n,d) PLOGDEBUGBLOCKDESC(lo,n,d)
  #define LOGDEBUGBLOCKDESCCOLL(lo,n,d) PLOGDEBUGBLOCKDESCCOLL(lo,n,d)
  #define LOGDEBUGBLOCK(lo,n)	PLOGDEBUGBLOCK(lo,n)
  #define LOGDEBUGBLOCKCOLL(lo,n)	PLOGDEBUGBLOCKCOLL(lo,n)
  #define LOGDEBUGENDBLOCK(lo,n) PLOGDEBUGENDBLOCK(lo,n)
  #define LOGDEBUGPUTSX(lo,lvl,m) PLOGDEBUGPUTSX(lo,lvl,m)
  #define LOGDEBUGPUTSXX(lo,lvl,m,s,p) PLOGDEBUGPUTSXX(lo,lvl,m,s,p)
  #define LOGDEBUGVPRINTFX(lo,lvl,f,a) PLOGDEBUGVPRINTFX(lo,lvl,f,a)
  #define LOGDEBUGPRINTFX(lo,lvl,m) PLOGDEBUGPRINTFX(lo,lvl,m)
  //#define THREADDEBUGPRINTFX(lvl,m) PTHREADDEBUGPRINTFX(lvl,m)
  #define OBJDEBUGPRINTFX(obj,lvl,m) POBJDEBUGPRINTFX(obj,lvl,m)
  #define OBJDEBUGPUTSX(obj,lvl,m) POBJDEBUGPUTSX(obj,lvl,m)
  #define OBJDEBUGPUTSXX(obj,lvl,m,s,p) POBJDEBUGPUTSXX(obj,lvl,m,s,p)
  #define OBJDEBUGPRINTF(obj,m) POBJDEBUGPRINTF(obj,m)
  #define OBJDEBUGTEST(obj,lvl) POBJDEBUGTEST(obj,lvl)
#else
  // public or no debugging, none or only P-variants are active
  #define DEBUGPUTSX(lvl,m)
  #define DEBUGPUTSXX(lvl,m,s,p)
  #define DEBUGPUTS(m)
  #define DEBUGVPRINTFX(lvl,f,a)
  #define DEBUGPRINTFX(lvl,m)
  #define DEBUGPRINTF(m)
  #define POINTERTEST(p,m)
  #define DEBUGTEST(lvl) false
  #define DEBUGMASK 0
  #define NCDEBUGPUTSX(lvl,m)
  #define NCDEBUGPUTSXX(lvl,m,s,p)
  #define NCDEBUGPRINTFX(lvl,m)
  #define NCDEBUGVPRINTFX(lvl,f,a)
  #define LOGDEBUGBLOCKFMT(lo,m)
  #define LOGDEBUGBLOCKFMTCOLL(lo,m)
  #define LOGDEBUGBLOCKDESC(lo,n,d)
  #define LOGDEBUGBLOCKDESCCOLL(lo,n,d)
  #define LOGDEBUGBLOCK(lo,n)
  #define LOGDEBUGBLOCKCOLL(lo,n)
  #define LOGDEBUGENDBLOCK(lo,n)
  #define LOGDEBUGPUTSX(lo,lvl,m)
  #define LOGDEBUGPUTSXX(lo,lvl,m,s,p)
  #define LOGDEBUGVPRINTFX(lo,lvl,f,a)
  #define LOGDEBUGPRINTFX(lo,lvl,m)
  //#define THREADDEBUGPRINTFX(lvl,m)
  #define OBJDEBUGPRINTFX(obj,lvl,m)
  #define OBJDEBUGPUTSX(obj,lvl,m)
  #define OBJDEBUGPUTSXX(obj,lvl,m,s,p)
  #define OBJDEBUGPRINTF(obj,m)
  #define OBJDEBUGTEST(obj,lvl) false
#endif



// debug String macro (for omitting text from non-debug code)
#ifdef SHORTDEBUGTEXTS
#define DEBUGSHORT(l,s) s
#else
#define DEBUGSHORT(l,s) l
#endif



// debug String macro (for omitting text from non-debug code)
#ifdef SYDEBUG
#define DEBUGTEXT(m,c) m
#else
#define DEBUGTEXT(m,c) c
#endif


// "enhanced" status generation in debug mode:
// adds String Item to Status with description
#ifdef SYDEBUG
#define ADDDEBUGITEM(s,m) { if (PDEBUGTEST(DBG_HOT)) s.addItemString(m); }
#else
#define ADDDEBUGITEM(s,m)
#endif

#ifdef __cplusplus
} // namespace sysync
#endif

#endif // SYSYNC_DEBUG_H

// eof

