/*
 * Copyright (C) 2007-2009 Patrick Ohly <patrick.ohly@gmx.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef INCL_MAEMOCALENDARSOURCE
#define INCL_MAEMOCALENDARSOURCE

#include <syncevo/TrackingSyncSource.h>

#ifdef ENABLE_MAEMO_CALENDAR

#include <CMulticalendar.h>
#include <ICalConverter.h>

#include <memory>
#include <boost/noncopyable.hpp>

#include <syncevo/declarations.h>
SE_BEGIN_CXX

/**
 * Implement access to Maemo calendar.
 * Change tracking is done by using the last-modified time
 * as TrackingSyncSource revisions. While it might be possible
 * to improve performance by using the getAllAdded/Modified
 * etc methods, it would make the change tracking less robust,
 * so it doesn't seem worth it.
 */
class MaemoCalendarSource : public TrackingSyncSource, public SyncSourceLogging, private boost::noncopyable
{
  public:
    MaemoCalendarSource(int EntryType, int EntryFormat,
                        const SyncSourceParams &params);


 protected:
    /* implementation of SyncSource interface */
    virtual void open();
    virtual bool isEmpty();
    virtual void close();
    virtual Databases getDatabases();
    virtual std::string getMimeType() const;
    virtual std::string getMimeVersion() const;
    virtual void getSynthesisInfo(SynthesisInfo &info,
                                  XMLConfigFragments &fragments);

    /* implementation of TrackingSyncSource interface */
    virtual void listAllItems(RevisionMap_t &revisions);
    virtual InsertItemResult insertItem(const string &luid, const std::string &item, bool raw);
    void readItem(const std::string &luid, std::string &item, bool raw);
    virtual void removeItem(const string &luid);

    /* implementation of SyncSourceLogging interface */
    virtual std::string getDescription(const string &luid);
 
 private:
    CMulticalendar *mc; /**< multicalendar */
    CCalendar *cal;   /**< calendar */
    int entry_type;   /**< entry type */
    int entry_format; /**< entry format */
    ICalConverter *conv; /**< converter */

    string get_revision(CComponent * c);
};

SE_END_CXX

#endif // ENABLE_MAEMO_CALENDAR
#endif // INCL_MAEMOCALENDARSOURCE
