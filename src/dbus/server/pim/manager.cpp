/*
 * Copyright (C) 2012 Intel Corporation
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

#include "manager.h"
#include "individual-traits.h"
#include "../resource.h"
#include "../client.h"
#include "../session.h"

#include <boost/scoped_ptr.hpp>

SE_BEGIN_CXX

static const char * const MANAGER_SERVICE = "org._01.pim.contacts";
static const char * const MANAGER_PATH = "/org/01/pim/contacts";
static const char * const MANAGER_IFACE = "org._01.pim.contacts.Manager";
static const char * const MANAGER_ERROR_ABORTED = "org._01.pim.contacts.Manager.Aborted";
static const char * const MANAGER_ERROR_BAD_STATUS = "org._01.pim.contacts.Manager.BadStatus";
static const char * const AGENT_IFACE = "org._01.pim.contacts.ViewAgent";
static const char * const CONTROL_IFACE = "org._01.pim.contacts.ViewControl";

/**
 * Name prefix for SyncEvolution config contexts used by PIM manager.
 * Used in combination with the uid string provided by the PIM manager
 * client, like this:
 *
 * eds@pim-manager-<uid> source 'eds' syncs with target-config@pim-manager-<uid>
 * source 'remote' for PBAP.
 *
 * eds@pim-manager-<uid> source 'local' syncs with a SyncML peer directly.
 */
static const char * const MANAGER_PREFIX = "pim-manager-";
static const char * const MANAGER_LOCAL_CONFIG = "eds";
static const char * const MANAGER_LOCAL_SOURCE = "local";
static const char * const MANAGER_REMOTE_CONFIG = "target-config";
static const char * const MANAGER_REMOTE_SOURCE = "remote";

Manager::Manager(const boost::shared_ptr<Server> &server) :
    DBusObjectHelper(server->getConnection(),
                     MANAGER_PATH,
                     MANAGER_IFACE),
    m_server(server)
{
    // Prevent automatic shut down during idle times, because we need
    // to keep our unified address book available.
    m_server->autoTermRef();
}

Manager::~Manager()
{
    // Clear the pending queue before self-desctructing, because the
    // entries hold pointers to this instance.
    m_pending.clear();
    m_server->autoTermUnref();
}

void Manager::init()
{
    // TODO: restore sort order
    m_sortOrder = "first/last";
    initFolks();
    initSorting();

    add(this, &Manager::start, "Start");
    add(this, &Manager::stop, "Stop");
    add(this, &Manager::setSortOrder, "SetSortOrder");
    add(this, &Manager::getSortOrder, "GetSortOrder");
    add(this, &Manager::search, "Search");
    add(this, &Manager::setPeer, "SetPeer");
    add(this, &Manager::removePeer, "RemovePeer");
    add(this, &Manager::syncPeer, "SyncPeer");
    add(this, &Manager::stopSync, "StopSync");
    add(this, &Manager::getAllPeers, "GetAllPeers");

    // Ready, make it visible via D-Bus.
    activate();

    // Claim MANAGER_SERVICE name on connection.
    // We don't care about the result.
    GDBusCXX::DBusConnectionPtr(getConnection()).ownNameAsync(MANAGER_SERVICE,
                                                              boost::function<void (bool)>());
}

void Manager::initFolks()
{
    m_folks = IndividualAggregator::create();
}

void Manager::initSorting()
{
    // TODO: mirror m_sortOrder in m_folks main view
}

boost::shared_ptr<Manager> Manager::create(const boost::shared_ptr<Server> &server)
{
    boost::shared_ptr<Manager> manager(new Manager(server));
    manager->m_self = manager;
    manager->init();
    return manager;
}

boost::shared_ptr<GDBusCXX::DBusObjectHelper> CreateContactManager(const boost::shared_ptr<Server> &server)
{
    return Manager::create(server);
}

void Manager::start()
{
    m_folks->start();
}

void Manager::stop()
{
    // TODO: if there are no active searches, then recreate aggregator
    if (true) {
        initFolks();
        initSorting();
    }
}

void Manager::setSortOrder(const std::string &order)
{
    // TODO: check string and change order,
    // store persistently
    m_sortOrder = order;
    initSorting();
}

/**
 * Connects a normal IndividualView to a D-Bus client.
 * Provides the org.01.pim.contacts.ViewControl API.
 */
class ViewResource : public Resource, public GDBusCXX::DBusObjectHelper
{
    static unsigned int m_counter;

    boost::weak_ptr<ViewResource> m_self;
    GDBusCXX::DBusRemoteObject m_viewAgent;
    boost::shared_ptr<IndividualView> m_view;
    boost::weak_ptr<Client> m_owner;
    GDBusCXX::DBusClientCall0 m_contactsModified,
        m_contactsAdded,
        m_contactsRemoved;

    ViewResource(const boost::shared_ptr<IndividualView> view,
                 const boost::shared_ptr<Client> &owner,
                 GDBusCXX::connection_type *connection,
                 const GDBusCXX::Caller_t &ID,
                 const GDBusCXX::DBusObject_t &agentPath) :
        GDBusCXX::DBusObjectHelper(connection,
                                   StringPrintf("%s/view%d", MANAGER_PATH, m_counter++),
                                   CONTROL_IFACE),
        m_viewAgent(connection,
                    agentPath,
                    AGENT_IFACE,
                    ID),
        m_view(view),
        m_owner(owner),
        // use ViewAgent interface
        m_contactsModified(m_viewAgent, "ContactsModified"),
        m_contactsAdded(m_viewAgent, "ContactsAdded"),
        m_contactsRemoved(m_viewAgent, "ContactsRemoved")
    {}

    /**
     * Invokes one of m_contactsModified/Added/Removed. A failure of
     * the asynchronous call indicates that the client is dead and
     * that its view can be purged.
     */
    void sendChange(const GDBusCXX::DBusClientCall0 &call,
                    int start, int count)
    {
        call.start(boost::bind(ViewResource::sendDone,
                               m_self,
                               _1));
    }

    /**
     * Used as callback for sending changes to the ViewAgent. Only
     * holds weak references and thus does not prevent deleting view
     * or client.
     */
    static void sendDone(const boost::weak_ptr<ViewResource> &self,
                         const std::string &error)
    {
        if (!error.empty()) {
            // remove view because it is no longer needed
            SE_LOG_DEBUG(NULL, NULL, "ViewAgent method call failed, deleting view: %s", error.c_str());
            boost::shared_ptr<ViewResource> r = self.lock();
            if (r) {
                r->close();
            }
        }
    }

    void init(boost::shared_ptr<ViewResource> self)
    {
        m_self = self;

        // activate D-Bus interface
        add(this, &ViewResource::readContacts, "ReadContacts");
        add(this, &ViewResource::close, "Close");
        add(this, &ViewResource::refineSearch, "RefineSearch");
        activate();

        // TODO: aggregate changes into batches
        m_view->m_modifiedSignal.connect(IndividualView::ChangeSignal_t::slot_type(&ViewResource::sendChange,
                                                                                   this,
                                                                                   boost::cref(m_contactsModified),
                                                                                   _1,
                                                                                   1).track(self));
        m_view->m_addedSignal.connect(IndividualView::ChangeSignal_t::slot_type(&ViewResource::sendChange,
                                                                                this,
                                                                                boost::cref(m_contactsAdded),
                                                                                _1,
                                                                                1).track(self));
        m_view->m_removedSignal.connect(IndividualView::ChangeSignal_t::slot_type(&ViewResource::sendChange,
                                                                                  this,
                                                                                  boost::cref(m_contactsRemoved),
                                                                                  _1,
                                                                                  1).track(self));

    }

public:
    static boost::shared_ptr<ViewResource> create(const boost::shared_ptr<IndividualView> &view,
                                                  const boost::shared_ptr<Client> &owner,
                                                  GDBusCXX::connection_type *connection,
                                                  const GDBusCXX::Caller_t &ID,
                                                  const GDBusCXX::DBusObject_t &agentPath)
    {
        boost::shared_ptr<ViewResource> viewResource(new ViewResource(view, owner,
                                                                      connection,
                                                                      ID,
                                                                      agentPath));
        viewResource->init(viewResource);
        return viewResource;
    }

    /** ViewControl.ReadContacts() */
    void readContacts(int start, int count, std::vector<FolksIndividualCXX> &contacts)
    {
        m_view->readContacts(start, count, contacts);
    }

    /** ViewControl.Close() */
    void close()
    {
        // Removing the resource from its owner will drop the last
        // reference and delete it when we return.
        boost::shared_ptr<ViewResource> r = m_self.lock();
        if (r) {
            boost::shared_ptr<Client> c = m_owner.lock();
            if (c) {
                c->detach(r.get());
            }
        }
    }

    /** ViewControl.RefineSearch() */
    void refineSearch(const StringMap &filter)
    {
        // TODO
        SE_THROW("not implemented");
    }
};
unsigned int ViewResource::m_counter;

GDBusCXX::DBusObject_t Manager::search(const GDBusCXX::Caller_t &ID,
                                       const boost::shared_ptr<GDBusCXX::Watch> &watch,
                                       const StringMap &filter,
                                       const GDBusCXX::DBusObject_t &agentPath)
{
    // Create and track view which is owned by the caller.
    boost::shared_ptr<Client> client = m_server->addClient(ID, watch);

    boost::shared_ptr<IndividualView> view;
    // TODO: parse filter
    view = m_folks->getMainView();

    boost::shared_ptr<ViewResource> viewResource(ViewResource::create(view,
                                                                      client,
                                                                      getConnection(),
                                                                      ID,
                                                                      agentPath));
    client->attach(boost::shared_ptr<Resource>(viewResource));

    // created local resource
    return viewResource->getPath();
}

void Manager::runInSession(const std::string &config,
                           Server::SessionFlags flags,
                           const boost::shared_ptr<GDBusCXX::Result> &result,
                           const boost::function<void (const boost::shared_ptr<Session> &session)> &callback)
{
    try {
        boost::shared_ptr<Session> session = m_server->startInternalSession(config,
                                                                            flags,
                                                                            boost::bind(&Manager::doSession,
                                                                                        this,
                                                                                        _1,
                                                                                        result,
                                                                                        callback));
        if (session->getSyncStatus() == Session::SYNC_QUEUEING) {
            // Must continue to wait instead of dropping the last reference.
            m_pending.push_back(std::make_pair(result, session));
        }
    } catch (...) {
        // Tell caller about specific error.
        dbusErrorCallback(result);
    }
}

void Manager::doSession(const boost::weak_ptr<Session> &weakSession,
                        const boost::shared_ptr<GDBusCXX::Result> &result,
                        const boost::function<void (const boost::shared_ptr<Session> &session)> &callback)
{
    try {
        boost::shared_ptr<Session> session = weakSession.lock();
        if (!session) {
            // Destroyed already?
            return;
        }
        // Drop permanent reference, session will be destroyed when we
        // return.
        m_pending.remove(std::make_pair(result, session));

        // Now run the operation.
        callback(session);
    } catch (...) {
        // Tell caller about specific error.
        dbusErrorCallback(result);
    }
}

void Manager::setPeer(const boost::shared_ptr<GDBusCXX::Result0> &result,
                      const std::string &uid, const StringMap &properties)
{
    runInSession(StringPrintf("@%s%s",
                              MANAGER_PREFIX,
                              uid.c_str()),
                 Server::SESSION_FLAG_NO_SYNC,
                 result,
                 boost::bind(&Manager::doSetPeer, this, _1, result, uid, properties));
}

static const char * const PEER_KEY_PROTOCOL = "protocol";
static const char * const PEER_SYNCML_PROTOCOL = "SyncML";
static const char * const PEER_PBAP_PROTOCOL = "PBAP";
static const char * const PEER_FILES_PROTOCOL = "files";
static const char * const PEER_KEY_TRANSPORT = "transport";
static const char * const PEER_BLUETOOTH_TRANSPORT = "Bluetooth";
static const char * const PEER_IP_TRANSPORT = "IP";
static const char * const PEER_DEF_TRANSPORT = PEER_BLUETOOTH_TRANSPORT;
static const char * const PEER_KEY_ADDRESS = "address";
static const char * const PEER_KEY_DATABASE = "database";

static std::string GetEssential(const StringMap &properties, const char *key,
                                bool allowEmpty = false)
{
    InitStateString entry = GetWithDef(properties, key);
    if (!entry.wasSet() ||
        (!allowEmpty && entry.empty())) {
        SE_THROW(StringPrintf("peer config: '%s' must be set%s",
                              key,
                              allowEmpty ? "" : " to a non-empty value"));
    }
    return entry;
}

void Manager::doSetPeer(const boost::shared_ptr<Session> &session,
                        const boost::shared_ptr<GDBusCXX::Result0> &result,
                        const std::string &uid, const StringMap &properties)
{
    // The session is active now, we have exclusive control over the
    // databases and the config. Create or update config.
    std::string protocol = GetEssential(properties, PEER_KEY_PROTOCOL);
    std::string transport = GetWithDef(properties, PEER_KEY_TRANSPORT, PEER_DEF_TRANSPORT);
    std::string address = GetEssential(properties, PEER_KEY_ADDRESS);
    std::string database = GetWithDef(properties, PEER_KEY_DATABASE);

    std::string localDatabaseName = MANAGER_PREFIX + uid;
    std::string context = StringPrintf("@%s%s", MANAGER_PREFIX, uid.c_str());

    SE_LOG_DEBUG(NULL, NULL, "%s: creating config for protocol %s",
                 uid.c_str(),
                 protocol.c_str());

    if (protocol == PEER_PBAP_PROTOCOL) {
        if (!database.empty()) {
            SE_THROW(StringPrintf("peer config: %s=%s: choosing database not supported for %s=%s",
                                  PEER_KEY_ADDRESS, address.c_str(),
                                  PEER_KEY_PROTOCOL, protocol.c_str()));
        }
        if (transport != PEER_BLUETOOTH_TRANSPORT) {
            SE_THROW(StringPrintf("peer config: %s=%s: only transport %s is supported for %s=%s",
                                  PEER_KEY_TRANSPORT, transport.c_str(),
                                  PEER_BLUETOOTH_TRANSPORT,
                                  PEER_KEY_PROTOCOL, protocol.c_str()));
        }
    }

    if (protocol == PEER_PBAP_PROTOCOL ||
        protocol == PEER_FILES_PROTOCOL) {
        // Create or set local config.
        boost::shared_ptr<SyncConfig> config(new SyncConfig(MANAGER_LOCAL_CONFIG + context));
        config->setDefaults();
        config->prepareConfigForWrite();
        config->setSyncURL("local://" + context);
        config->setPeerIsClient(true);
        boost::shared_ptr<PersistentSyncSourceConfig> source(config->getSyncSourceConfig(MANAGER_LOCAL_SOURCE));
        source->setBackend("evolution-contacts");
        source->setDatabaseID(localDatabaseName);
        source->setSync("local-cache");
        source->setURI(MANAGER_REMOTE_SOURCE);
        config->flush();
        // Ensure that database exists.
        SyncSourceParams params(MANAGER_LOCAL_SOURCE,
                                config->getSyncSourceNodes(MANAGER_LOCAL_SOURCE),
                                config,
                                context);
        boost::scoped_ptr<SyncSource> syncSource(SyncSource::createSource(params));
        SyncSource::Databases databases = syncSource->getDatabases();
        bool found = false;
        BOOST_FOREACH (const SyncSource::Database &database, databases) {
            if (database.m_uri == localDatabaseName) {
                found = true;
                break;
            }
        }
        if (!found) {
            syncSource->createDatabase(SyncSource::Database(localDatabaseName, localDatabaseName));
        }

        // Now also create target config, in the same context.
        config.reset(new SyncConfig(MANAGER_REMOTE_CONFIG + context));
        config->setDefaults();
        config->prepareConfigForWrite();
        source = config->getSyncSourceConfig(MANAGER_REMOTE_SOURCE);
        if (protocol == PEER_PBAP_PROTOCOL) {
            // PBAP
            source->setDatabaseID("obex-bt://" + address);
            source->setBackend("pbap");
        } else {
            // Local sync with files on the target side.
            // Format is hard-coded to vCard 3.0.
            source->setDatabaseID("file://" + address);
            source->setDatabaseFormat("text/vcard");
            source->setBackend("file");
        }
        config->flush();
    } else {
        SE_THROW(StringPrintf("peer config: %s=%s not supported",
                              PEER_KEY_PROTOCOL,
                              protocol.c_str()));
    }

    // Report success.
    SE_LOG_DEBUG(NULL, NULL, "%s: created config for protocol %s",
                 uid.c_str(),
                 protocol.c_str());
    result->done();
}

Manager::PeersMap Manager::getAllPeers()
{
    PeersMap peers;

    SyncConfig::ConfigList configs = SyncConfig::getConfigs();
    std::string prefix = StringPrintf("%s@%s",
                                      MANAGER_LOCAL_CONFIG,
                                      MANAGER_PREFIX);

    BOOST_FOREACH (const StringPair &entry, configs) {
        if (boost::starts_with(entry.first, prefix)) {
            // One of our configs.
            std::string uid = entry.first.substr(prefix.size());
            StringMap &properties = peers[uid];
            // Extract relevant properties from configs.
            SyncConfig localConfig(entry.first);
            InitState< std::vector<std::string> > syncURLs = localConfig.getSyncURL();
            std::string syncURL;
            if (!syncURLs.empty()) {
                syncURL = syncURLs[0];
            }
            if (boost::starts_with(syncURL, "local://")) {
                // Look at target source to determine protocol.
                SyncConfig targetConfig(StringPrintf("%s@%s%s",
                                                     MANAGER_REMOTE_CONFIG,
                                                     MANAGER_PREFIX,
                                                     uid.c_str()));
                boost::shared_ptr<PersistentSyncSourceConfig> source(targetConfig.getSyncSourceConfig(MANAGER_REMOTE_SOURCE));
                std::string backend = source->getBackend();
                std::string database = source->getDatabaseID();
                if (backend == "PBAP Address Book") {
                    properties[PEER_KEY_PROTOCOL] = PEER_PBAP_PROTOCOL;
                    if (boost::starts_with(database, "obex-bt://")) {
                        properties[PEER_KEY_ADDRESS] = database.substr(strlen("obex-bt://"));
                    }
                } else if (backend == "file") {
                    properties[PEER_KEY_PROTOCOL] = PEER_FILES_PROTOCOL;
                    if (boost::starts_with(database, "file://")) {
                        properties[PEER_KEY_ADDRESS] = database.substr(strlen("file://"));
                    }
                }
            }
        }
    }

    return peers;
}


void Manager::removePeer(const boost::shared_ptr<GDBusCXX::Result0> &result,
                         const std::string &uid)
{
    runInSession(StringPrintf("@%s%s",
                              MANAGER_PREFIX,
                              uid.c_str()),
                 Server::SESSION_FLAG_NO_SYNC,
                 result,
                 boost::bind(&Manager::doRemovePeer, this, _1, result, uid));
}

void Manager::doRemovePeer(const boost::shared_ptr<Session> &session,
                           const boost::shared_ptr<GDBusCXX::Result0> &result,
                           const std::string &uid)
{
    // Remove database. This is expected to be noticed by libfolks
    // without us having to tell it.
    m_enabledPeers.erase(uid);

    std::string localDatabaseName = MANAGER_PREFIX + uid;
    std::string context = StringPrintf("@%s%s", MANAGER_PREFIX, uid.c_str());

    // Access config via context (includes sync and target config).
    boost::shared_ptr<SyncConfig> config(new SyncConfig(context));

    // Remove database, if it exists.
    if (config->exists(CONFIG_LEVEL_CONTEXT)) {
        boost::shared_ptr<PersistentSyncSourceConfig> source(config->getSyncSourceConfig(MANAGER_LOCAL_SOURCE));
        SyncSourceNodes nodes = config->getSyncSourceNodes(MANAGER_LOCAL_SOURCE);
        if (nodes.dataConfigExists()) {
            SyncSourceParams params(MANAGER_LOCAL_SOURCE,
                                    nodes,
                                    config,
                                    context);
            boost::scoped_ptr<SyncSource> syncSource(SyncSource::createSource(params));
            SyncSource::Databases databases = syncSource->getDatabases();
            bool found = false;
            BOOST_FOREACH (const SyncSource::Database &database, databases) {
                if (database.m_uri == localDatabaseName) {
                    found = true;
                    break;
                }
            }
            if (found) {
                syncSource->deleteDatabase(localDatabaseName);
            }
        }
    }

    // Remove entire context, just in case. Placing the code here also
    // ensures that nothing except the config itself has the config
    // nodes open, which would prevent removing them. For the same
    // reason the SyncConfig is recreated: to clear all references to
    // sources that were opened via it.
    config.reset(new SyncConfig(context));
    config->remove();
    config->flush();

    // Report success.
    result->done();
}

void Manager::syncPeer(const boost::shared_ptr<GDBusCXX::Result0> &result,
                       const std::string &uid)
{
    runInSession(StringPrintf("%s@%s%s",
                              MANAGER_LOCAL_CONFIG,
                              MANAGER_PREFIX,
                              uid.c_str()),
                 Server::SESSION_FLAG_NO_SYNC,
                 result,
                 boost::bind(&Manager::doSyncPeer, this, _1, result, uid));
}

static void doneSyncPeer(const boost::shared_ptr<GDBusCXX::Result0> &result,
                         SyncMLStatus status)
{
    if (status == STATUS_OK ||
        status == STATUS_HTTP_OK) {
        result->done();
    } else if (status == (SyncMLStatus)sysync::LOCERR_USERABORT) {
        result->failed(GDBusCXX::dbus_error(MANAGER_ERROR_ABORTED, "running sync aborted, probably by StopSync()"));
    } else {
        result->failed(GDBusCXX::dbus_error(MANAGER_ERROR_BAD_STATUS, Status2String(status)));
    }
}

void Manager::doSyncPeer(const boost::shared_ptr<Session> &session,
                         const boost::shared_ptr<GDBusCXX::Result0> &result,
                         const std::string &uid)
{
    // After sync(), the session is tracked as the active sync session
    // by the server. It was removed from our own m_pending list by
    // doSession().
    session->sync("", SessionCommon::SourceModes_t());
    // Relay result to caller when done.
    session->m_doneSignal.connect(boost::bind(doneSyncPeer, result, _1));
}

void Manager::stopSync(const boost::shared_ptr<GDBusCXX::Result0> &result,
                       const std::string &uid)
{
    // Fully qualified peer config name. Only used for sync sessions
    // and thus good enough to identify them.
    std::string syncConfigName = StringPrintf("%s@%s%s",
                                              MANAGER_LOCAL_CONFIG,
                                              MANAGER_PREFIX,
                                              uid.c_str());

    // Remove all pending sessions of the peer. Make a complete
    // copy of the list, to avoid issues with modifications of the
    // underlying list while we iterate over it.
    BOOST_FOREACH (const Pending_t::value_type &entry, Pending_t(m_pending)) {
        std::string configName = entry.second->getConfigName();
        if (configName == syncConfigName) {
            entry.first->failed(GDBusCXX::dbus_error(MANAGER_ERROR_ABORTED, "pending sync aborted by StopSync()"));
            m_pending.remove(entry);
        }
    }

    // Stop the currently running sync if it is for the peer.
    // It may or may not complete, depending on what it is currently
    // doing. We'll check in doneSyncPeer().
    boost::shared_ptr<Session> session = m_server->getSyncSession();
    bool aborting = false;
    if (session) {
        std::string configName = session->getConfigName();
        if (configName == syncConfigName) {
            // Return to caller later, when aborting is done.
            session->abortAsync(SimpleResult(boost::bind(&GDBusCXX::Result0::done, result),
                                             createDBusErrorCb(result)));
            aborting = true;
        }
    }
    if (!aborting) {
        result->done();
    }
}

SE_END_CXX
