/* Target options
 * ==============
 *
 *
 */

// SYNCML CLIENT ENGINE LIBRARY WIN32
// ##################################

// define platform
// - we are on Windows
#ifndef _WIN32
  #define _WIN32 // needed for MWERKS
#endif
#ifndef WIN32
  #define WIN32  // needed for others
#endif

// Release version status
#undef RELEASE_VERSION
#define RELEASE_SYDEBUG 2 // extended DBG included
//#define OPTIONAL_SYDEBUG 1

// now include platform independent product options (which include global_options.h)
#include "product_options.h"


// Identification strings
#define CUST_SYNC_MAN       SYSYNC_OEM  // manufactured by ourselves
#define CUST_SYNC_MODEL     "SySync Client Library PROTO Win32"
#define CUST_SYNC_FIRMWARE  NULL  // no firmware
#define CUST_SYNC_HARDWARE  NULL  // no hardware

// String used to construct logfile names
#define TARGETID "sysynclib_win32"

// Configuration
// =============

// Default profile
#undef HARD_CODED_SERVER_URI
#undef HARD_CODED_DBNAMES
#define DEFAULT_SERVER_URI "http://www.synthesis.ch/sync"
#define DEFAULT_LOCALDB_PROFILE ""
#define DEFAULT_SERVER_USER ""
#define DEFAULT_SERVER_PASSWD ""
#define DEFAULT_ENCODING SML_WBXML
#define DEFAULT_TRANSPORT_USER NULL
#define DEFAULT_TRANSPORT_PASSWD NULL
#define DEFAULT_SOCKS_HOST NULL
#define DEFAULT_PROXY_HOST NULL
#define DEFAULT_DATASTORES_ENABLED false
#define DEFAULT_EVENTS_DAYSBEFORE 30
#define DEFAULT_EVENTS_DAYSAFTER 90
#define DEFAULT_EVENTS_LIMITED false
#define DEFAULT_EMAILS_LIMITED true
#define DEFAULT_EMAILS_HDRONLY true
#define DEFAULT_EMAILS_MAXKB 2
#define DEFAULT_EMAILS_ONLYLAST true
#define DEFAULT_EMAILS_DAYSBEFORE 10


// Eval limit options
// ==================

// Note: Hard expiration date settings moved to product_options.h

// - if defined, software will stop specified number of days after
//   first use
#undef EXPIRES_AFTER_DAYS // Engine does not have auto-demo

// Identification for update check and demo period
#define SYSER_VARIANT_CODE SYSER_VARIANT_PRO
#define SYSER_PRODUCT_CODE SYSER_PRODCODE_CLIENT_LIB_WIN32
#define SYSER_EXTRA_ID SYSER_EXTRA_ID_PROTO


// - if defined, software can be registered
#define SYSER_REGISTRATION 1
// - define allowed product codes
#define SYSER_PRODUCT_CODE_MAIN SYSER_PRODCODE_CLIENT_LIB_WIN32 // we need a sysync library license for windows
#define SYSER_PRODUCT_CODE_ALT1 SYSER_PRODCODE_CLIENT_ODBC_PRO_WIN32 // a PRO license for the Win console client is ok as well
#define SYSER_PRODUCT_CODE_ALT2 SYSER_PRODCODE_CLIENT_LIB_ALL
#define SYSER_PRODUCT_CODE_ALT3 SYSER_PRODCODE_CLIENT_LIB_DESK
#undef SYSER_PRODUCT_CODE_ALT4
// - define needed product flags
//   only licenses that are release date limited (or explicitly NOT release date limited,
//   or time limited) are allowed
#define SYSER_NEEDED_PRODUCT_FLAGS SYSER_PRODFLAG_MAXRELDATE
#define SYSER_FORBIDDEN_PRODUCT_FLAGS 0

// for this DBG variant, DLL plugins are always allowed
// (even with licenses that do not have SYSER_PRODFLAG_SERVER_SDKAPI set)
#define DLL_PLUGINS_ALWAYS_ALLOWED 1


// SySync options
// ==============

// - enhanced profile record
#define ENHANCED_PROFILES_2004 1
#define CLIENTFEATURES_2008 1

// - support for automatic syncing (timed, IPP, server alerted...)
#define AUTOSYNC_SUPPORT 1
// - support for intelligent push & poll (IPP)
//#define IPP_SUPPORT 1
//#define IPP_SUPPORT_ALWAYS 1 // regardless of license flags
// - support for timed sync
#define TIMEDSYNC_SUPPORT 1
// - support for WAP push alerted sync
#define SERVERALERT_SUPPORT 1

// - support for Proxy
#define PROXY_SUPPORT 1

// - support for multiple profiles
#define MULTI_PROFILE_SUPPORT 1

// - show progress events
#define PROGRESS_EVENTS 1

// - we do not need to squeeze code
#undef MINIMAL_CODE

// - script with regex support
#define SCRIPT_SUPPORT
#define REGEX_SUPPORT

// - client does not need target options
#undef SYSYNC_TARGET_OPTIONS

// - filters
#define OBJECT_FILTERING

// - client does not need superdatastores
#undef SUPERDATASTORES


// general options needed for email
#define EMAIL_FORMAT_SUPPORT 1
#define EMAIL_ATTACHMENT_SUPPORT 1
#define ARRAYFIELD_SUPPORT 1

// - if defined, stream field support will be included
#define STREAMFIELD_SUPPORT 1

// - if defined, semi-proprietary zipped-binary <data> for items (any type) can be used
//   (enabled on a by type basis in the config)
#define ZIPPED_BINDATA_SUPPORT 1


// - where to save application data by default (if not otherwise configured)
//   APPDATA_SUBDIR is a subdirectory of the user's "application data" dir.
#define APPDATA_SUBDIR "synthesis.ch\\SySyncLib"

// - if defined, code for incoming and outgoing SyncML dumping into (WB)XML logfiles is included
#define MSGDUMP 1


// Datastore Options
// -----------------

// - support DBApi tunnel
#define DBAPI_TUNNEL_SUPPORT 1

// - link in customimplds on top of binfile
#define BASED_ON_BINFILE_CLIENT 1
// - SQL and API datastores have changedetection, so build binfile for that
#define CHANGEDETECTION_AVAILABLE 1
// - we need a separate changelog mechanism
#define CHECKSUM_CHANGELOG 1
// - string localIDs with sufficiently large size
#define STRING_LOCALID_MAXLEN 256

// - supports ODBC, SQLite and DBApi
#define SQL_SUPPORT 1
#define ODBCAPI_SUPPORT 1
#define SQLITE_SUPPORT 1
// - master detail (array) tables supported
#define ARRAYDBTABLES_SUPPORT 1

// - if defined, SDK support is included
#define SDK_SUPPORT         1
#define PLUGIN_DLL          1 // external DLL allowed as well

// - define what SDK modules are linked in
//#define DBAPI_DEMO        1
#define   DBAPI_TEXT        1
//#define DBAPI_SILENT      1
#define   DBAPI_LOGGER      1
#define   DBAPI_SNOWWHITE   1
#define   JNI_SUPPORT       1
#define   FILEOBJ_SUPPORT   1
#define   ADAPTITEM_SUPPORT 1

#ifdef _WIN32
  #define  CSHARP_SUPPORT   1
#endif



/* eof */
