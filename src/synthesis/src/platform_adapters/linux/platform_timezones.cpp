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


namespace sysync {

/*! @brief platform specific loading of time zone definitions
 *  @return true if this list is considered complete (i.e. no built-in zones should be used additionally)
 *  @param[in/out] aGZones : the GZones object where system zones should be loaded into
 */
bool loadSystemZoneDefinitions(GZones* aGZones)
{
	// load zones from system here
	#ifdef HAVE_LIBICAL
  icalarray *builtin = icaltimezone_get_builtin_timezones();
  for (unsigned i = 0; builtin && i < builtin->num_elements; i++) {
    icaltimezone *zone = (icaltimezone *)icalarray_element_at(builtin, i);
    if (!zone)
      continue;
    icalcomponent *comp = icaltimezone_get_component(zone);
    if (!comp)
      continue;
    char *vtimezone = icalcomponent_as_ical_string(comp);
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
      t.ident = "x";
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
		#ifdef LIBICAL_MEMFIXES
    // new-style Evolution libical: memory must be freed by caller
    free(vtimezone);
		#endif
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

  tzset();
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
