/*
 * Copyright (C) 2010 Intel Corporation
 */

#ifndef INCL_CALDAVSOURCE
#define INCL_CALDAVSOURCE

#include <config.h>

#ifdef ENABLE_DAV

#include "WebDAVSource.h"
#include <syncevo/MapSyncSource.h>
#include <syncevo/eds_abi_wrapper.h>
#include <syncevo/SmartPtr.h>

#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>

#include <syncevo/declarations.h>
SE_BEGIN_CXX

class CalDAVSource : public WebDAVSource,
    public SubSyncSource,
    public SyncSourceLogging
{
 public:
    CalDAVSource(const SyncSourceParams &params, const boost::shared_ptr<SyncEvo::Neon::Settings> &settings);

    /* implementation of SyncSourceSerialize interface */
    virtual const char *getMimeType() const { return "text/calendar"; }
    virtual const char *getMimeVersion() const { return "2.0"; }

    /* implementation of SubSyncSource interface */
    virtual void listAllSubItems(SubRevisionMap_t &revisions);
    virtual SubItemResult insertSubItem(const std::string &uid, const std::string &subid,
                                        const std::string &item);
    virtual void readSubItem(const std::string &uid, const std::string &subid, std::string &item);
    virtual std::string removeSubItem(const string &uid, const std::string &subid);
    virtual void flushItem(const string &uid);
    virtual std::string getSubDescription(const string &uid, const string &subid);

    // implementation of SyncSourceLogging callback
    virtual std::string getDescription(const string &luid);

    /**
     * Dump each resource item unmodified into the given directory.
     * The ConfigNode stores the luid/etag mapping.
     */
    void backupData(const SyncSource::Operations::ConstBackupInfo &oldBackup,
                    const SyncSource::Operations::BackupInfo &newBackup,
                    BackupReport &report);

    /**
     * Restore database from data stored in backupData(). Will be
     * called inside open()/close() pair. beginSync() is *not* called.
     */
    void restoreData(const SyncSource::Operations::ConstBackupInfo &oldBackup,
                     bool dryrun,
                     SyncSourceReport &report);

    // disambiguate getSynthesisAPI()
    SDKInterface *getSynthesisAPI() const { return SubSyncSource::getSynthesisAPI(); }

 protected:
    // implementation of WebDAVSource callbacks
    virtual bool typeMatches(const StringMap &props) const;
    virtual std::string homeSetProp() const { return "urn:ietf:params:xml:ns:caldav:calendar-home-set"; }
    virtual std::string contentType() const { return "text/calendar; charset=utf-8"; }
    virtual std::string suffix() const { return ".ics"; }

 private:
    /**
     * Information about each merged item.
     */
    class Event : boost::noncopyable {
    public:
        Event() :
            m_sequence(0),
            m_lastmodtime(0)
        {}

        /** the ID used by WebDAVSource */
        std::string m_DAVluid;

        /** the iCalendar 2.0 UID */
        std::string m_UID;

        /** revision string in WebDAVSource */
        std::string m_etag;

        /** maximum sequence number of any sub item */
        long m_sequence;

        /** maximum modification time of any sub item */
        time_t m_lastmodtime;

        /**
         * the list of simplified RECURRENCE-IDs (without time zone,
         * see icalTime2Str()), empty string for VEVENT without
         * RECURRENCE-ID
         */
        std::set<std::string> m_subids;

        /**
         * parsed VCALENDAR component representing the current
         * state of the item as it exists on the WebDAV server,
         * must be kept up-to-date as we make changes, may be NULL
         */
        eptr<icalcomponent> m_calendar;

        /** date-time as string, without time zone */
        static std::string icalTime2Str(const icaltimetype &tt);

        /** RECURRENCE-ID, empty if none */
        static std::string getSubID(icalcomponent *icomp);

        /** SEQUENCE number, 0 if none */
        static int getSequence(icalcomponent *icomp);
        static void setSequence(icalcomponent *icomp, int sequenceval);

        /** UID, empty if none */
        static std::string getUID(icalcomponent *icomp);
        static void setUID(icalcomponent *icomp, const std::string &uid);

        /** rename RECURRENCE-ID to X-SYNCEVOLUTION-RECURRENCE-ID and vice versa */
        static void escapeRecurrenceID(std::string &data);
        static void unescapeRecurrenceID(std::string &data);
    };

    /**
     * A cache of information about each merged item. Maps from
     * WebDAVSource local ID to Event. Items in the cache are in the
     * format as expected by the local side, with RECURRENCE-ID.
     *
     * This is not necessarily how the data is sent to the server:
     * - RECURRENCE-ID in an item which has no master event
     *   is replaced by X-SYNCEVOLUTION-RECURRENCE-ID because
     *   Google gets confused by a single detached event without
     *   parent (Event::escapeRecurrenceID()).
     *
     * When retrieving an EVENT from the server this is substituted
     * again before parsing (depends on server preserving X-
     * extensions, see Event::unescapeRecurrenceID()).
     */
    class EventCache : public std::map<std::string, boost::shared_ptr<Event> >
    {
      public:
        EventCache() : m_initialized(false) {}
        bool m_initialized;

        iterator findByUID(const std::string &uid);
    } m_cache;

    Event &loadItem(const std::string &davLUID);
    Event &loadItem(Event &event);

    /** callback for listAllSubItems: parse and add new item */
    int appendItem(SubRevisionMap_t &revisions,
                   std::string &href,
                   std::string &etag,
                   std::string &data);

    /** callback for backupData(): dump into backup */
    int backupItem(ItemCache &cache,
                   std::string &href,
                   std::string &etag,
                   std::string &data);
};

SE_END_CXX

#endif // ENABLE_DAV
#endif // INCL_CALDAVSOURCE
