/*
 *  File:         platform_timezones.cpp
 *
 *  Author:       Lukas Zeller / Patrick Ohly
 *
 *  Time zone dependent routines for Linux
 *
 *  Copyright (c) 2004-2009 by Synthesis AG (www.synthesis.ch)
 *
 *  2009-04-02 : Created by Lukas Zeller from timezones.cpp work by Patrick Ohly
 *
 */

// must be first in file, everything above is ignored by MVC compilers
#include "prefix_file.h"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined(HAVE_LIBICAL) && defined(EVOLUTION_COMPATIBILITY)
# ifndef _GNU_SOURCE
#  define _GNU_SOURCE 1
# endif
# include <dlfcn.h>
#endif

#include "timezones.h"
#include "vtimezone.h"

#ifdef HAVE_LIBICAL
# ifndef HANDLE_LIBICAL_MEMORY
#  define HANDLE_LIBICAL_MEMORY 1
# endif
# include <libical/ical.h>
#endif

#ifdef LINUX
  extern char *tzname[ 2 ];
  #ifndef BSD
    extern long  timezone;
  #endif
  extern int   daylight;
#endif

#ifdef EVOLUTION_COMPATIBILITY
// Avoid calling libical directly. This way libsynthesis does not
// depend on a specific version of libecal (which changed its soname
// frequently, without affecting libical much!) or libical. The user
// of libsynthesis has to make sure that the necessary ical functions
// can be found via dlsym(RTLD_DEFAULT) when
// loadSystemZoneDefinitions() is invoked during the initialization of
// a Synthesis engine.
static struct {
  icalarray *(* icaltimezone_get_builtin_timezones_p)();
  void *(* icalarray_element_at_p)(icalarray *array, unsigned index);
  icalcomponent *(* icaltimezone_get_component_p)(icaltimezone *zone);
  char *(* icalcomponent_as_ical_string_p)(icalcomponent *comp);
  bool must_free_strings;
} icalcontext;

static icalarray *ICALTIMEZONE_GET_BUILTIN_TIMEZONES()
{
  // Called once by loadSystemZoneDefinitions(), so initialize context
  // now. Redoing this on each invocation allows unloading libical
  // because we won't reuse stale pointers.
  memset(&icalcontext, 0, sizeof(icalcontext));
  icalcontext.icaltimezone_get_builtin_timezones_p =
    (typeof(icalcontext.icaltimezone_get_builtin_timezones_p))dlsym(RTLD_DEFAULT, "icaltimezone_get_builtin_timezones");
  icalcontext.icalarray_element_at_p =
    (typeof(icalcontext.icalarray_element_at_p))dlsym(RTLD_DEFAULT, "icalarray_element_at");
  icalcontext.icaltimezone_get_component_p =
    (typeof(icalcontext.icaltimezone_get_component_p))dlsym(RTLD_DEFAULT, "icaltimezone_get_component");
  icalcontext.icalcomponent_as_ical_string_p =
    (typeof(icalcontext.icalcomponent_as_ical_string_p))dlsym(RTLD_DEFAULT, "icalcomponent_as_ical_string_r");
  if (icalcontext.icalcomponent_as_ical_string_p) {
    // found icalcomponent_as_ical_string_r() which always requires freeing
    icalcontext.must_free_strings = TRUE;
  } else {
    // fall back to older icalcomponent_as_ical_string() which may or may not
    // require freeing the returned string; this can be determined by checking
    // for "ical_memfixes" (EDS libical)
    icalcontext.icalcomponent_as_ical_string_p =
      (typeof(icalcontext.icalcomponent_as_ical_string_p))dlsym(RTLD_DEFAULT, "icalcomponent_as_ical_string");
    icalcontext.must_free_strings = dlsym(RTLD_DEFAULT, "ical_memfixes") != NULL;
  }

  return icalcontext.icaltimezone_get_builtin_timezones_p ?
    icalcontext.icaltimezone_get_builtin_timezones_p() :
    NULL;
}

static void *ICALARRAY_ELEMENT_AT(icalarray *array, unsigned index)
{
  return icalcontext.icalarray_element_at_p ?
    icalcontext.icalarray_element_at_p(array, index) :
    NULL;
}

static icalcomponent *ICALTIMEZONE_GET_COMPONENT(icaltimezone *zone)
{
  return icalcontext.icaltimezone_get_component_p ?
    icalcontext.icaltimezone_get_component_p(zone) :
    NULL;
}

static char *ICALCOMPONENT_AS_ICAL_STRING(icalcomponent *comp)
{
  return icalcontext.icalcomponent_as_ical_string_p ?
    icalcontext.icalcomponent_as_ical_string_p(comp) :
    NULL;
}

static void ICAL_FREE(char *str)
{
  if (icalcontext.must_free_strings && str)
    free(str);
}

#else
// call functions directly
# define ICALTIMEZONE_GET_BUILTIN_TIMEZONES icaltimezone_get_builtin_timezones
# define ICALARRAY_ELEMENT_AT icalarray_element_at
# define ICALTIMEZONE_GET_COMPONENT icaltimezone_get_component

# if defined(HAVE_LIBICAL) && !defined(HAVE_LIBECAL)
#  // new-style libical _r version which requires freeing the string
#  define ICALCOMPONENT_AS_ICAL_STRING icalcomponent_as_ical_string_r
#  define ICAL_FREE(_x) free(_x)
# else
#  define ICALCOMPONENT_AS_ICAL_STRING icalcomponent_as_ical_string
#  ifdef LIBICAL_MEMFIXES
   // new-style Evolution libical: memory must be freed by caller
#   define ICAL_FREE(_x) free(_x)
#  else
#   define ICAL_FREE(_x) free(_x)
#  endif
# endif
#endif


namespace sysync {

/*! @brief platform specific loading of time zone definitions
 *  @return true if this list is considered complete (i.e. no built-in zones should be used additionally)
 *  @param[in/out] aGZones : the GZones object where system zones should be loaded into
 */
bool loadSystemZoneDefinitions(GZones* aGZones)
{
	// load zones from system here
	#ifdef HAVE_LIBICAL
  icalarray *builtin = ICALTIMEZONE_GET_BUILTIN_TIMEZONES();
  for (unsigned i = 0; builtin && i < builtin->num_elements; i++) {
    icaltimezone *zone = (icaltimezone *)ICALARRAY_ELEMENT_AT(builtin, i);
    if (!zone)
      continue;
    icalcomponent *comp = ICALTIMEZONE_GET_COMPONENT(zone);
    if (!comp)
      continue;
    char *vtimezone = ICALCOMPONENT_AS_ICAL_STRING(comp);
    if (!vtimezone)
      continue;
    tz_entry t;
    string dstName, stdName;
    if (VTIMEZONEtoTZEntry(
    	vtimezone,
      t,
      stdName,
      dstName,
      #ifdef SYDEBUG
        aGZones->getDbgLogger
      #endif
    )) {
      t.ident = "";
      t.dynYear = "";
      // expect Olson /<domain>/<version>/<location> TZIDs and
      // extract the trailing location (which might contain
      // several slashes, so a backwards search doesn't work)
      std::string::size_type off;
      if (t.name.size() > 2 &&
          t.name[0] == '/' &&
          (off = t.name.find('/', 1)) != t.name.npos &&
          (off = t.name.find('/', off + 1)) != t.name.npos) {
        t.location = t.name.substr(off + 1);
      }
      aGZones->tzP.push_back(t);
    }
    ICAL_FREE(vtimezone);
  }
	#endif // HAVE_LIBICAL
	// return true if this list is considered complete (i.e. no built-in zones should be used additionally)
  return false; // we need the built-in zones
} // loadSystemZoneDefinitions



/*! @brief get current system time zone
 *  @return true if successful
 *  @param[out] aContext : the time zone context representing the current system time zone.
 *  @param[in] aGZones : the GZones object.
 */
bool getSystemTimeZoneContext(timecontext_t &aContext, GZones* aGZones)
{
  tz_entry t;
  bool ok = true;

  #ifdef ANDROID
  // BFO_INCOMPLETE
  #else
  tzset();
  #endif
  t.name = tzname[ 0 ];
  if (strcmp( t.name.c_str(),tzname[ 1 ] )!=0) {
    t.name+= "/";
    t.name+= tzname[ 1 ];
  } // if

  //if (isDbg) PNCDEBUGPRINTFX( DBG_SESSION, ( "Timezone: %s", sName.c_str() ));

  // search based on name before potentially using offset search
  if (TimeZoneNameToContext( t.name.c_str(),aContext, aGZones ))
    return true; // found, done
	#if defined USE_TM_GMTOFF
  else {
    // We can use tm_gmtoff as fallback when the name computed above doesn't
    // match: identify offsets, then search based on offsets.
    time_t now = time(NULL);
    bool have_dst = false,
      have_std = false;
    // start searching for offsets today, moving ahead one week at a time
    int week = 0;
    do {
      struct tm tm;
      time_t day = now + 60 * 60 * 24 * 7 * week;
      localtime_r(&day, &tm);
      if (tm.tm_isdst) {
        if (!have_dst) {
          t.biasDST = tm.tm_gmtoff / 60;
          have_dst = true;
        }
      } else {
        if (!have_std) {
          t.bias = tm.tm_gmtoff / 60;
          have_std = true;
        }
      }
      week++;
    } while ((!have_std || !have_dst) && week <= 54);

    if (have_dst) {
      if (have_std) {
        // make biasDST relative to bias
        t.biasDST -= t.bias;
      } else {
        // daylight saving without standard?!
      }
    }
    // search for name based on offsets
    t.ident="o";
    if (FoundTZ(t, t.name, aContext, aGZones))
      goto done;
  }
	#endif // USE_TM_GMTOFF
  // not enough information to create a new time zone below, give up
  ok = false;
done:
  return ok;
} // getSystemTimeZoneContext





} // namespace sysync

/* eof */
