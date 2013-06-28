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

#ifndef INCL_EVOLUTIONCONTACTSOURCE
#define INCL_EVOLUTIONCONTACTSOURCE

#include "config.h"
#include "EvolutionSyncSource.h"
#include <syncevo/SmartPtr.h>
#include <syncevo/GLibSupport.h>

#include <boost/noncopyable.hpp>

#ifdef ENABLE_EBOOK

#include <set>

#include <syncevo/declarations.h>

#ifdef USE_EDS_CLIENT
SE_GOBJECT_TYPE(EBookClient)
SE_GOBJECT_TYPE(EBookClientView)
#endif
SE_GOBJECT_TYPE(EContact)

SE_BEGIN_CXX

class ContactCache;

/**
 * Implements access to Evolution address books.
 */
class EvolutionContactSource : public EvolutionSyncSource,
    public SyncSourceLogging,
    private boost::noncopyable
{
  public:
    EvolutionContactSource(const SyncSourceParams &params,
                           EVCardFormat vcardFormat = EVC_FORMAT_VCARD_30);
    virtual ~EvolutionContactSource();

    //
    // implementation of SyncSource
    //
    virtual Databases getDatabases();
    virtual void open();
    virtual bool isEmpty();
    virtual void close();
    virtual std::string getMimeType() const;
    virtual std::string getMimeVersion() const;
   
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

    // need to override native format: it is always vCard 3.0
    void getSynthesisInfo(SynthesisInfo &info,
                          XMLConfigFragments &fragments)
    {
        EvolutionSyncSource::getSynthesisInfo(info, fragments);
        info.m_profile = "\"vCard\", 2";
        info.m_native = "vCard30EDS";
        // Replace normal vCard30 and vCard21 types with the
        // EDS flavors which apply EDS specific transformations *before*
        // letting the engine process the incoming item. This ensures
        // that during a slow sync, modified (!) incoming item and
        // DB item really match. Otherwise the engine compares unmodified
        // incoming item and modified DB item, finding a mismatch caused
        // by the transformations, and writes an item which ends up being
        // identical to the one which is in the DB.
        boost::replace_all(info.m_datatypes, "vCard30", "vCard30EDS");
        boost::replace_all(info.m_datatypes, "vCard21", "vCard21EDS");
        // Redundant when the same transformations are already applied to
        // incoming items. But disabling it does not improve performance much,
        // so keep it enabled just to be on the safe side.
        info.m_beforeWriteScript = "$VCARD_BEFOREWRITE_SCRIPT_EVOLUTION;";
        info.m_afterReadScript = "$VCARD_AFTERREAD_SCRIPT_EVOLUTION;";
    }

#ifdef USE_EDS_CLIENT
    virtual const char *sourceExtension() const { return E_SOURCE_EXTENSION_ADDRESS_BOOK; }
    virtual ESourceCXX refSystemDB() const { return ESourceCXX(e_source_registry_ref_builtin_address_book(EDSRegistryLoader::getESourceRegistry()), TRANSFER_REF); }
#endif

  private:
    /** extract REV string for contact, throw error if not found */
    std::string getRevision(const std::string &uid);

    /** valid after open(): the address book that this source references */
#ifdef USE_EDS_CLIENT
    EBookClientCXX m_addressbook;
    enum AccessMode {
        SYNCHRONOUS,
        BATCHED,
        DEFAULT
    } m_accessMode;
    InitState<int> m_asyncOpCounter;

    enum AsyncStatus {
        MODIFYING, /**< insert or update request sent */
        REVISION,  /**< asked for revision */
        DONE       /**< finished successfully or with failure, depending on m_gerror */
    };

    struct Pending {
        std::string m_name;
        EContactCXX m_contact;
        std::string m_uid;
        std::string m_revision;
        AsyncStatus m_status;
        GErrorCXX m_gerror;

        Pending() : m_status(MODIFYING) {}
    };
    typedef std::list< boost::shared_ptr<Pending> >PendingContainer_t;

    /**
     * Batched "contact add/update" operations.
     * Delete is not batched because we need per-item status
     * information - see removeItem().
     */
    PendingContainer_t m_batchedAdd;
    PendingContainer_t m_batchedUpdate;
    InitState<int> m_numRunningOperations;

    InsertItemResult checkBatchedInsert(const boost::shared_ptr<Pending> &pending);
    void completedAdd(const boost::shared_ptr<PendingContainer_t> &batched, gboolean success, /* const GStringListFreeCXX &uids */ GSList *uids, const GError *gerror) throw ();
    void completedUpdate(const boost::shared_ptr<PendingContainer_t> &batched, gboolean success, const GError *gerror) throw ();
    virtual void flushItemChanges();
    virtual void finishItemChanges();

    // Read-ahead of item data.
    boost::shared_ptr<ContactCache> m_contactCache, m_contactCacheNext;
    int m_cacheMisses, m_cacheStalls;
    int m_contactReads; /**< number of readItemAsKey() calls */
    int m_contactsFromDB; /**< number of contacts requested from DB (including ones not found) */
    int m_contactQueries; /**< total number of e_book_client_get_contacts() calls */

    ReadAheadOrder m_readAheadOrder;
    ReadAheadItems m_nextLUIDs;

    void checkCacheForError(boost::shared_ptr<ContactCache> &cache);
    void invalidateCachedContact(const std::string &luid);
    void invalidateCachedContact(boost::shared_ptr<ContactCache> &cache, const std::string &luid);
    bool getContact(const string &luid, EContact **contact, GErrorCXX &gerror);
    bool getContactFromCache(const string &luid, EContact **contact, GErrorCXX &gerror);
    enum ReadingMode
    {
        START,    /**< luid is needed, must be read  */
        CONTINUE  /**< luid is from old request, find next ones */
    };
    boost::shared_ptr<ContactCache> startReading(const std::string &luid, ReadingMode mode);
    void completedRead(const boost::weak_ptr<ContactCache> &cachePtr, gboolean success, GSList *contactsPtr, const GError *gerror) throw();
    void logCacheStats(Logger::Level level);

    // Use the information provided to us to implement read-ahead efficiently.
    virtual void setReadAheadOrder(ReadAheadOrder order,
                                   const ReadAheadItems &luids);
    virtual void getReadAheadOrder(ReadAheadOrder &order,
                                   ReadAheadItems &luids);

#else
    eptr<EBook, GObject> m_addressbook;
#endif

    /** the format of vcards that new items are expected to have */
    const EVCardFormat m_vcardFormat;

    /**
     * a list of Evolution vcard properties which have to be encoded
     * as X-SYNCEVOLUTION-* when sending to server in 2.1 and decoded
     * back when receiving.
     */
    static const class extensions : public set<string> {
      public:
        extensions() : prefix("X-SYNCEVOLUTION-") {
            this->insert("FBURL");
            this->insert("CALURI");
        }

        const string prefix;
    } m_vcardExtensions;

    /**
     * a list of properties which SyncEvolution (in contrast to
     * the server) will only store once in each contact
     */
    static const class unique : public set<string> {
      public:
        unique () {
            insert("X-AIM");
            insert("X-GROUPWISE");
            insert("X-ICQ");
            insert("X-YAHOO");
            insert("X-EVOLUTION-ANNIVERSARY");
            insert("X-EVOLUTION-ASSISTANT");
            insert("X-EVOLUTION-BLOG-URL");
            insert("X-EVOLUTION-FILE-AS");
            insert("X-EVOLUTION-MANAGER");
            insert("X-EVOLUTION-SPOUSE");
            insert("X-EVOLUTION-VIDEO-URL");
            insert("X-MOZILLA-HTML");
            insert("FBURL");
            insert("CALURI");
        }
    } m_uniqueProperties;
};

SE_END_CXX

#endif // ENABLE_EBOOK
#endif // INCL_EVOLUTIONCONTACTSOURCE
