/*
 * Copyright (C) 2005-2009 Patrick Ohly <patrick.ohly@gmx.de>
 * Copyright (C) 2009 Intel Corporation
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

#ifndef INCL_EVOLUTIONCALENDARSOURCE
#define INCL_EVOLUTIONCALENDARSOURCE

#include "config.h"
#include "EvolutionSyncSource.h"
#include <syncevo/SmartPtr.h>

#include <boost/noncopyable.hpp>

#ifdef ENABLE_ECAL

#include <syncevo/declarations.h>

#ifdef USE_ECAL_CLIENT
SE_GOBJECT_TYPE(ECalClient)
SE_GOBJECT_TYPE(ECalClientView)
#endif

SE_BEGIN_CXX

/** 
 * Source type independent from ECal / ECalClient to abstract
 * the two different enums in the APIs.
 */
typedef enum {
	  EVOLUTION_CAL_SOURCE_TYPE_EVENTS,
	  EVOLUTION_CAL_SOURCE_TYPE_TASKS,
	  EVOLUTION_CAL_SOURCE_TYPE_MEMOS
} EvolutionCalendarSourceType;

inline bool IsCalObjNotFound(const GError *gerror) {
    return gerror &&
#ifdef USE_ECAL_CLIENT
        gerror->domain == E_CAL_CLIENT_ERROR &&
        gerror->code == E_CAL_CLIENT_ERROR_OBJECT_NOT_FOUND
#else
        gerror->domain == E_CALENDAR_ERROR &&
        gerror->code == E_CALENDAR_STATUS_OBJECT_NOT_FOUND
#endif
        ;
}

/**
 * Implements access to Evolution calendars, either
 * using the to-do item or events. Change tracking
 * is done by looking at the modification time stamp.
 * Recurring events and their detached recurrences are
 * handled as one item for the main event and one item
 * for each detached recurrence.
 */
class EvolutionCalendarSource : public EvolutionSyncSource,
    public SyncSourceLogging,
    private boost::noncopyable
{
  public:
    /**
     * @param    type        chooses which kind of calendar data to use:
     *                       EVOLUTION_CAL_SOURCE_TYPE_EVENTS,
     *                       EVOLUTION_CAL_SOURCE_TYPE_TASKS,
     *                       EVOLUTION_CAL_SOURCE_TYPE_MEMOS
     */
    EvolutionCalendarSource(EvolutionCalendarSourceType type,
                            const SyncSourceParams &params);
    virtual ~EvolutionCalendarSource() { close(); }

    //
    // implementation of SyncSource
    //
    virtual Databases getDatabases();
    virtual void open();
    virtual bool isEmpty();
    virtual void close(); 
    virtual std::string getMimeType() const { return "text/calendar"; }
    virtual std::string getMimeVersion() const { return "2.0"; }

    void getSynthesisInfo(SynthesisInfo &info,
                          XMLConfigFragments &fragments)
    {
        // All EDS calendar storages must suppport UID/RECURRENCE-ID,
        // it's part of the API. Therefore we can rely on it.
        EvolutionSyncSource::getSynthesisInfo(info, fragments);
        info.m_globalIDs = true;
    }

    /**
     * An item is identified in the calendar by
     * its UID (unique ID) and RID (recurrence ID).
     * The RID may be empty.
     *
     * This is turned into a SyncML LUID by
     * concatenating them: <uid>-rid<rid>.
     */
    class ItemID {
    public:
    ItemID(const string &uid, const string &rid) :
        m_uid(uid),
            m_rid(rid)
            {}
    ItemID(const char *uid, const char *rid):
        m_uid(uid ? uid : ""),
            m_rid(rid ? rid : "")
                {}
    ItemID(const string &luid);

        const string m_uid, m_rid;

        string getLUID() const;
        static string getLUID(const string &uid, const string &rid);
    };

    /**
     * Extract item ID from calendar item.  An icalcomponent must
     * refer to the VEVENT/VTODO/VJOURNAL component.
     */
    static ItemID getItemID(ECalComponent *ecomp);
    static ItemID getItemID(icalcomponent *icomp);

    /**
     * Extract modification string from calendar item.
     * @return empty string if no time was available
     */
    static string getItemModTime(ECalComponent *ecomp);
    static string getItemModTime(icalcomponent *icomp);

  protected:
    //
    // implementation of TrackingSyncSource callbacks
    //
    virtual void listAllItems(RevisionMap_t &revisions);
    virtual InsertItemResult insertItem(const string &uid, const std::string &item, bool raw);
    void readItem(const std::string &luid, std::string &item, bool raw);
    virtual void removeItem(const string &uid);

    // implementation of SyncSourceLogging callback
    virtual std::string getDescription(const string &luid);

  protected:
    /** valid after open(): the calendar that this source references */
#ifdef USE_ECAL_CLIENT
    ECalClientCXX m_calendar;
#else
    eptr<ECal, GObject> m_calendar;
    ECal *(*m_newSystem)(void);    /**< e_cal_new_system_calendar, etc. */
#endif
    string m_typeName;             /**< "calendar", "task list", "memo list" */
    EvolutionCalendarSourceType m_type;         /**< use events, tasks or memos? */

    // Convenience function for source type casting
#ifdef USE_ECAL_CLIENT
    ECalClientSourceType sourceType() const {
        return (ECalClientSourceType)m_type;
    }
#else
    ECalSourceType sourceType() const {
        return (ECalSourceType)m_type;
    }
#endif

    /**
     * retrieve the item with the given id - may throw exception
     *
     * caller has to free result
     */
    icalcomponent *retrieveItem(const ItemID &id);

    /** retrieve the item with the given luid as VCALENDAR string - may throw exception */
    string retrieveItemAsString(const ItemID &id);


    /** returns the type which the ical library uses for our components */
    icalcomponent_kind getCompType() {
        return m_type == EVOLUTION_CAL_SOURCE_TYPE_EVENTS ? ICAL_VEVENT_COMPONENT :
            m_type == EVOLUTION_CAL_SOURCE_TYPE_MEMOS ? ICAL_VJOURNAL_COMPONENT :
            ICAL_VTODO_COMPONENT;
    }

#ifndef USE_ECAL_CLIENT
    /** ECalAuthFunc which calls the authenticate() methods */
    static char *eCalAuthFunc(ECal *ecal,
                              const char *prompt,
                              const char *key,
                              gpointer user_data) {
        return ((EvolutionCalendarSource *)user_data)->authenticate(prompt, key);
    }

    /** actual implementation of ECalAuthFunc */
    char *authenticate(const char *prompt,
                       const char *key);
#endif

    /**
     * Returns the LUID of a calendar item.
     */
    string getLUID(ECalComponent *ecomp);

    /**
     * Extract modification string of an item stored in
     * the calendar.
     * @return empty string if no time was available
     */
    string getItemModTime(const ItemID &id);

    /**
     * Convert to string in canonical representation.
     */
    static string icalTime2Str(const struct icaltimetype &tt);

    /**
     * A set of all existing objects. Initialized in the last call to
     * listAllItems() and then updated as items get
     * added/removed. Used to decide how insertItem() has to be
     * implemented without the troublesome querying of the EDS
     * backend.
     */
    class LUIDs : public map< string, set<string> > {
    public:
        bool containsUID(const std::string &uid) const { return findUID(uid) != end(); }
        const_iterator findUID(const std::string &uid) const { return find(uid); }

        bool containsLUID(const ItemID &id) const;
        void insertLUID(const ItemID &id);
        void eraseLUID(const ItemID &id);
    } m_allLUIDs;

    /**
     * A list of ref-counted smart pointers to icalcomponents.
     * The list members can be copied; destroying the last instance
     * will destroy the smart pointer, which then calls
     * icalcomponent_free().
     */
    typedef list< boost::shared_ptr< eptr<icalcomponent> > > ICalComps_t;

    /**
     * Utility function which extracts all icalcomponents with
     * the given UID, stores them in a list and then removes
     * them from the calendar. Trying to remove a non-existant
     * UID is logged, but not an error. It simply returns an
     * empty list.
     *
     * Relies on m_allLUIDs, but does not update it. The caller must
     * ensure that the calendar remains in a consistent state.
     *
     * @param returnOnlyChildren    only return children in list, even if parent is also removed
     * @param ignoreNotFound        don't throw a STATUS_NOT_FOUND error when deleting fails with
     *                              a NOT_FOUND error
     */
    ICalComps_t removeEvents(const string &uid, bool returnOnlyChildren, bool ignoreNotFound = true);
};

SE_END_CXX

#endif // ENABLE_ECAL
#endif // INCL_EVOLUTIONSYNCSOURCE
