/*
 * Copyright (C) 2009 Intel Corporation
 * Copyright (C) 2011 Symbio, Ville Nummela
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

#include <fstream>

#include <syncevo/LogRedirect.h>
#include <syncevo/GLibSupport.h>

#include "server.h"
#include "info-req.h"
#include "connection.h"
#include "bluez-manager.h"
#include "session.h"
#include "timeout.h"
#include "restart.h"
#include "client.h"

using namespace GDBusCXX;

SE_BEGIN_CXX

void Server::clientGone(Client *c)
{
    for(Clients_t::iterator it = m_clients.begin();
        it != m_clients.end();
        ++it) {
        if (it->second.get() == c) {
            SE_LOG_DEBUG(NULL, NULL, "D-Bus client %s has disconnected",
                         c->m_ID.c_str());
            autoTermUnref(it->second->getAttachCount());
            m_clients.erase(it);
            return;
        }
    }
    SE_LOG_DEBUG(NULL, NULL, "unknown client has disconnected?!");
}

std::string Server::getNextSession()
{
    // Make the session ID somewhat random. This protects to
    // some extend against injecting unwanted messages into the
    // communication.
    m_lastSession++;
    if (!m_lastSession) {
        m_lastSession++;
    }
    return StringPrintf("%u%u", rand(), m_lastSession);
}

vector<string> Server::getCapabilities()
{
    // Note that this is tested by test-dbus.py in
    // TestServer.testCapabilities, update the test when adding
    // capabilities.
    vector<string> capabilities;

    capabilities.push_back("ConfigChanged");
    capabilities.push_back("GetConfigName");
    capabilities.push_back("NamedConfig");
    capabilities.push_back("Notifications");
    capabilities.push_back("Version");
    capabilities.push_back("SessionFlags");
    capabilities.push_back("SessionAttach");
    capabilities.push_back("DatabaseProperties");
    return capabilities;
}

StringMap Server::getVersions()
{
    StringMap versions;

    versions["version"] = VERSION;
    versions["system"] = EDSAbiWrapperInfo();
    versions["backends"] = SyncSource::backendsInfo();
    return versions;
}

void Server::attachClient(const Caller_t &caller,
                          const boost::shared_ptr<Watch> &watch)
{
    boost::shared_ptr<Client> client = addClient(getConnection(),
                                                 caller,
                                                 watch);
    autoTermRef();
    client->increaseAttachCount();
}

void Server::detachClient(const Caller_t &caller)
{
    boost::shared_ptr<Client> client = findClient(caller);
    if (client) {
        autoTermUnref();
        client->decreaseAttachCount();
    }
}

void Server::setNotifications(bool enabled,
                              const Caller_t &caller,
                              const string & /* notifications */)
{
    boost::shared_ptr<Client> client = findClient(caller);
    if (client && client->getAttachCount()) {
        client->setNotificationsEnabled(enabled);
    } else {
        SE_THROW("client not attached, not allowed to change notifications");
    }
}

bool Server::notificationsEnabled()
{
    for(Clients_t::iterator it = m_clients.begin();
        it != m_clients.end();
        ++it) {
        if (!it->second->getNotificationsEnabled()) {
            return false;
        }
    }
    return true;
}

void Server::connect(const Caller_t &caller,
                     const boost::shared_ptr<Watch> &watch,
                     const StringMap &peer,
                     bool must_authenticate,
                     const std::string &session,
                     DBusObject_t &object)
{
    if (!session.empty()) {
        // reconnecting to old connection is not implemented yet
        throw std::runtime_error("not implemented");
    }
    std::string new_session = getNextSession();

    boost::shared_ptr<Connection> c(new Connection(*this,
                                                   getConnection(),
                                                   new_session,
                                                   peer,
                                                   must_authenticate));
    SE_LOG_DEBUG(NULL, NULL, "connecting D-Bus client %s with connection %s '%s'",
                 caller.c_str(),
                 c->getPath(),
                 c->m_description.c_str());

    boost::shared_ptr<Client> client = addClient(getConnection(),
                                                 caller,
                                                 watch);
    client->attach(c);
    c->activate();

    object = c->getPath();
}

void Server::startSessionWithFlags(const Caller_t &caller,
                                   const boost::shared_ptr<Watch> &watch,
                                   const std::string &server,
                                   const std::vector<std::string> &flags,
                                   DBusObject_t &object)
{
    boost::shared_ptr<Client> client = addClient(getConnection(),
                                                 caller,
                                                 watch);
    std::string new_session = getNextSession();
    boost::shared_ptr<Session> session = Session::createSession(*this,
                                                                "is this a client or server session?",
                                                                server,
                                                                new_session,
                                                                flags);
    client->attach(session);
    session->activate();
    enqueue(session);
    object = session->getPath();
}

void Server::checkPresence(const std::string &server,
                           std::string &status,
                           std::vector<std::string> &transports)
{
    return m_presence.checkPresence(server, status, transports);
}

void Server::getSessions(std::vector<DBusObject_t> &sessions)
{
    sessions.reserve(m_workQueue.size() + 1);
    if (m_activeSession) {
        sessions.push_back(m_activeSession->getPath());
    }
    BOOST_FOREACH(boost::weak_ptr<Session> &session, m_workQueue) {
        boost::shared_ptr<Session> s = session.lock();
        if (s) {
            sessions.push_back(s->getPath());
        }
    }
}

Server::Server(GMainLoop *loop,
               bool &shutdownRequested,
               boost::shared_ptr<Restart> &restart,
               const DBusConnectionPtr &conn,
               int duration) :
    DBusObjectHelper(conn,
                     "/org/syncevolution/Server",
                     "org.syncevolution.Server",
                     boost::bind(&Server::autoTermCallback, this)),
    m_loop(loop),
    m_shutdownRequested(shutdownRequested),
    m_restart(restart),
    m_lastSession(time(NULL)),
    m_activeSession(NULL),
    m_lastInfoReq(0),
    m_bluezManager(new BluezManager(*this)),
    sessionChanged(*this, "SessionChanged"),
    presence(*this, "Presence"),
    templatesChanged(*this, "TemplatesChanged"),
    configChanged(*this, "ConfigChanged"),
    infoRequest(*this, "InfoRequest"),
    logOutput(*this, "LogOutput"),
    m_presence(*this),
    m_connman(*this),
    m_networkManager(*this),
    m_autoSync(*this),
    m_autoTerm(m_loop, m_shutdownRequested, m_autoSync.preventTerm() ? -1 : duration), //if there is any task in auto sync, prevent auto termination
    m_parentLogger(LoggerBase::instance())
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    srand(tv.tv_usec);
    add(this, &Server::getCapabilities, "GetCapabilities");
    add(this, &Server::getVersions, "GetVersions");
    add(this, &Server::attachClient, "Attach");
    add(this, &Server::detachClient, "Detach");
    add(this, &Server::enableNotifications, "EnableNotifications");
    add(this, &Server::disableNotifications, "DisableNotifications");
    add(this, &Server::notificationAction, "NotificationAction");
    add(this, &Server::connect, "Connect");
    add(this, &Server::startSession, "StartSession");
    add(this, &Server::startSessionWithFlags, "StartSessionWithFlags");
    add(this, &Server::getConfigs, "GetConfigs");
    add(this, &Server::getConfig, "GetConfig");
    add(this, &Server::getReports, "GetReports");
    add(this, &Server::checkSource, "CheckSource");
    add(this, &Server::getDatabases, "GetDatabases");
    add(this, &Server::checkPresence, "CheckPresence");
    add(this, &Server::getSessions, "GetSessions");
    add(this, &Server::infoResponse, "InfoResponse");
    add(sessionChanged);
    add(templatesChanged);
    add(configChanged);
    add(presence);
    add(infoRequest);
    add(logOutput);

    LoggerBase::pushLogger(this);
    setLevel(LoggerBase::DEBUG);

    // Assume that Bluetooth is available. Neither ConnMan nor Network
    // manager can tell us about that. The "Bluetooth" ConnMan technology
    // is about IP connection via Bluetooth - not what we need.
    getPresenceStatus().updatePresenceStatus(true, PresenceStatus::BT_TRANSPORT);

    if (!m_connman.isAvailable() &&
        !m_networkManager.isAvailable()) {
        // assume that we are online if no network manager was found at all
        getPresenceStatus().updatePresenceStatus(true, PresenceStatus::HTTP_TRANSPORT);
    }
}

Server::~Server()
{
    // make sure all other objects are gone before destructing ourselves
    m_syncSession.reset();
    m_workQueue.clear();
    m_clients.clear();
    LoggerBase::popLogger();
}

void Server::fileModified()
{
    if (!m_shutdownSession) {
        string newSession = getNextSession();
        vector<string> flags;
        flags.push_back("no-sync");
        m_shutdownSession = Session::createSession(*this,
                                                   "",  "",
                                                   newSession,
                                                   flags);
        m_shutdownSession->setPriority(Session::PRI_AUTOSYNC);
        m_shutdownSession->startShutdown();
        enqueue(m_shutdownSession);
    }

    m_shutdownSession->shutdownFileModified();
}

void Server::run(LogRedirect &redirect)
{
    // This has the intended side effect that it loads everything into
    // memory which might be dynamically loadable, like backend
    // plugins.
    StringMap map = getVersions();
    SE_LOG_DEBUG(NULL, NULL, "D-Bus server ready to run, versions:");
    BOOST_FOREACH(const StringPair &entry, map) {
        SE_LOG_DEBUG(NULL, NULL, "%s: %s", entry.first.c_str(), entry.second.c_str());
    }

    // Now that everything is loaded, check memory map for files which we have to monitor.
    set<string> files;
    std::ifstream in("/proc/self/maps");
    while (!in.eof()) {
        string line;
        getline(in, line);
        size_t off = line.find('/');
        if (off != line.npos &&
            line.find(" r-xp ") != line.npos) {
            files.insert(line.substr(off));
        }
    }
    in.close();
    BOOST_FOREACH(const string &file, files) {
        try {
            SE_LOG_DEBUG(NULL, NULL, "watching: %s", file.c_str());
            boost::shared_ptr<SyncEvo::GLibNotify> notify(new GLibNotify(file.c_str(), boost::bind(&Server::fileModified, this)));
            m_files.push_back(notify);
        } catch (...) {
            // ignore errors for indidividual files
            Exception::handle();
        }
    }

    while (!m_shutdownRequested) {
        if (!m_activeSession ||
            !m_activeSession->readyToRun()) {
            g_main_loop_run(m_loop);
        }
        if (m_activeSession &&
            m_activeSession->readyToRun()) {
            // this session must be owned by someone, otherwise
            // it would not be set as active session
            boost::shared_ptr<Session> session = m_activeSessionRef.lock();
            if (!session) {
                throw runtime_error("internal error: session no longer available");
            }
            try {
                // ensure that the session doesn't go away
                m_syncSession.swap(session);
                m_activeSession->run(redirect);
            } catch (const std::exception &ex) {
                SE_LOG_ERROR(NULL, NULL, "%s", ex.what());
            } catch (...) {
                SE_LOG_ERROR(NULL, NULL, "unknown error");
            }
            session.swap(m_syncSession);
            dequeue(session.get());
        }

        if (!m_shutdownRequested && m_autoSync.hasTask()) {
            // if there is at least one pending task and no session is created for auto sync,
            // pick one task and create a session
            m_autoSync.startTask();
        }
        // Make sure check whether m_activeSession is owned by autosync
        // Otherwise activeSession is owned by AutoSyncManager but it never
        // be ready to run. Because methods of Session, like 'sync', are able to be
        // called when it is active.
        if (!m_shutdownRequested && m_autoSync.hasActiveSession())
        {
            // if the autosync is the active session, then invoke 'sync'
            // to make it ready to run
            m_autoSync.prepare();
        }
    }
}


/**
 * look up client by its ID
 */
boost::shared_ptr<Client> Server::findClient(const Caller_t &ID)
{
    for(Clients_t::iterator it = m_clients.begin();
        it != m_clients.end();
        ++it) {
        if (it->second->m_ID == ID) {
            return it->second;
        }
    }
    return boost::shared_ptr<Client>();
}

boost::shared_ptr<Client> Server::addClient(const DBusConnectionPtr &conn,
                                            const Caller_t &ID,
                                            const boost::shared_ptr<Watch> &watch)
{
    boost::shared_ptr<Client> client(findClient(ID));
    if (client) {
        return client;
    }
    client.reset(new Client(*this, ID));
    // add to our list *before* checking that peer exists, so
    // that clientGone() can remove it if the check fails
    m_clients.push_back(std::make_pair(watch, client));
    watch->setCallback(boost::bind(&Server::clientGone, this, client.get()));
    return client;
}


void Server::detach(Resource *resource)
{
    BOOST_FOREACH(const Clients_t::value_type &client_entry,
                  m_clients) {
        client_entry.second->detachAll(resource);
    }
}

void Server::enqueue(const boost::shared_ptr<Session> &session)
{
    WorkQueue_t::iterator it = m_workQueue.end();
    while (it != m_workQueue.begin()) {
        --it;
        if (it->lock()->getPriority() <= session->getPriority()) {
            ++it;
            break;
        }
    }
    m_workQueue.insert(it, session);

    checkQueue();
}

int Server::killSessions(const std::string &peerDeviceID)
{
    int count = 0;

    WorkQueue_t::iterator it = m_workQueue.begin();
    while (it != m_workQueue.end()) {
        boost::shared_ptr<Session> session = it->lock();
        if (session && session->getPeerDeviceID() == peerDeviceID) {
            SE_LOG_DEBUG(NULL, NULL, "removing pending session %s because it matches deviceID %s",
                         session->getSessionID().c_str(),
                         peerDeviceID.c_str());
            // remove session and its corresponding connection
            boost::shared_ptr<Connection> c = session->getStubConnection().lock();
            if (c) {
                c->shutdown();
            }
            it = m_workQueue.erase(it);
            count++;
        } else {
            ++it;
        }
    }

    if (m_activeSession &&
        m_activeSession->getPeerDeviceID() == peerDeviceID) {
        SE_LOG_DEBUG(NULL, NULL, "aborting active session %s because it matches deviceID %s",
                     m_activeSession->getSessionID().c_str(),
                     peerDeviceID.c_str());
        try {
            // abort, even if not necessary right now
            m_activeSession->abort();
        } catch (...) {
            // TODO: catch only that exception which indicates
            // incorrect use of the function
        }
        dequeue(m_activeSession);
        count++;
    }

    return count;
}

void Server::dequeue(Session *session)
{
    if (m_syncSession.get() == session) {
        // This is the running sync session.
        // It's not in the work queue and we have to
        // keep it active, so nothing to do.
        return;
    }

    for (WorkQueue_t::iterator it = m_workQueue.begin();
         it != m_workQueue.end();
         ++it) {
        if (it->lock().get() == session) {
            // remove from queue
            m_workQueue.erase(it);
            // session was idle, so nothing else to do
            return;
        }
    }

    if (m_activeSession == session) {
        // The session is releasing the lock, so someone else might
        // run now.
        session->setActive(false);
        sessionChanged(session->getPath(), false);
        m_activeSession = NULL;
        m_activeSessionRef.reset();
        checkQueue();
        return;
    }
}

void Server::checkQueue()
{
    if (m_activeSession) {
        // still busy
        return;
    }

    while (!m_workQueue.empty()) {
        boost::shared_ptr<Session> session = m_workQueue.front().lock();
        m_workQueue.pop_front();
        if (session) {
            // activate the session
            m_activeSession = session.get();
            m_activeSessionRef = session;
            session->setActive(true);
            sessionChanged(session->getPath(), true);
            //if the active session is changed, give a chance to quit the main loop
            //and make it ready to run if it is owned by AutoSyncManager.
            //Otherwise, server might be blocked.
            g_main_loop_quit(m_loop);
            return;
        }
    }
}

bool Server::sessionExpired(const boost::shared_ptr<Session> &session)
{
    SE_LOG_DEBUG(NULL, NULL, "session %s expired",
                 session->getSessionID().c_str());
    // don't call me again
    return false;
}

void Server::delaySessionDestruction(const boost::shared_ptr<Session> &session)
{
    SE_LOG_DEBUG(NULL, NULL, "delaying destruction of session %s by one minute",
                 session->getSessionID().c_str());
    addTimeout(boost::bind(&Server::sessionExpired,
                           session),
               60 /* 1 minute */);
}

bool Server::callTimeout(const boost::shared_ptr<Timeout> &timeout, const boost::function<bool ()> &callback)
{
    if (!callback()) {
        m_timeouts.remove(timeout);
        return false;
    } else {
        return true;
    }
}

void Server::addTimeout(const boost::function<bool ()> &callback,
                            int seconds)
{
    boost::shared_ptr<Timeout> timeout(new Timeout);
    m_timeouts.push_back(timeout);
    timeout->activate(seconds,
                      boost::bind(&Server::callTimeout,
                                  this,
                                  // avoid copying the shared pointer here,
                                  // otherwise the Timeout will never be deleted
                                  boost::ref(m_timeouts.back()),
                                  callback));
}

void Server::infoResponse(const Caller_t &caller,
                              const std::string &id,
                              const std::string &state,
                              const std::map<string, string> &response)
{
    InfoReqMap::iterator it = m_infoReqMap.find(id);
    // if not found, ignore
    if(it != m_infoReqMap.end()) {
        boost::shared_ptr<InfoReq> infoReq = it->second.lock();
        infoReq->setResponse(caller, state, response);
    }
}

boost::shared_ptr<InfoReq> Server::createInfoReq(const string &type,
                                                 const std::map<string, string> &parameters,
                                                 const Session *session)
{
    boost::shared_ptr<InfoReq> infoReq(new InfoReq(*this, type, parameters, session));
    boost::weak_ptr<InfoReq> item(infoReq) ;
    m_infoReqMap.insert(pair<string, boost::weak_ptr<InfoReq> >(infoReq->getId(), item));
    return infoReq;
}

std::string Server::getNextInfoReq()
{
    return StringPrintf("%u", ++m_lastInfoReq);
}

void Server::emitInfoReq(const InfoReq &req)
{
    infoRequest(req.getId(),
                req.getSessionPath(),
                req.getInfoStateStr(),
                req.getHandler(),
                req.getType(),
                req.getParam());
}

void Server::removeInfoReq(const InfoReq &req)
{
    // remove InfoRequest from hash map
    InfoReqMap::iterator it = m_infoReqMap.find(req.getId());
    if(it != m_infoReqMap.end()) {
        m_infoReqMap.erase(it);
    }
}

void Server::getDeviceList(SyncConfig::DeviceList &devices)
{
    //wait bluez or other device managers
    while(!m_bluezManager->isDone()) {
        g_main_loop_run(m_loop);
    }

    devices.clear();
    devices = m_syncDevices;
}

void Server::addPeerTempl(const string &templName,
                          const boost::shared_ptr<SyncConfig::TemplateDescription> peerTempl)
{
    std::string lower = templName;
    boost::to_lower(lower);
    m_matchedTempls.insert(MatchedTemplates::value_type(lower, peerTempl));
}

boost::shared_ptr<SyncConfig::TemplateDescription> Server::getPeerTempl(const string &peer)
{
    std::string lower = peer;
    boost::to_lower(lower);
    MatchedTemplates::iterator it = m_matchedTempls.find(lower);
    if(it != m_matchedTempls.end()) {
        return it->second;
    } else {
        return boost::shared_ptr<SyncConfig::TemplateDescription>();
    }
}

bool Server::getDevice(const string &deviceId, SyncConfig::DeviceDescription &device)
{
    SyncConfig::DeviceList::iterator syncDevIt;
    for(syncDevIt = m_syncDevices.begin(); syncDevIt != m_syncDevices.end(); ++syncDevIt) {
        if(boost::equals(syncDevIt->m_deviceId, deviceId)) {
            device = *syncDevIt;
            if(syncDevIt->m_pnpInformation) {
                device.m_pnpInformation = boost::shared_ptr<SyncConfig::PnpInformation>(
                    new SyncConfig::PnpInformation(syncDevIt->m_pnpInformation->m_vendor,
                                                   syncDevIt->m_pnpInformation->m_product));
            }
            return true;
        }
    }
    return false;
}

void Server::addDevice(const SyncConfig::DeviceDescription &device)
{
    SyncConfig::DeviceList::iterator it;
    for(it = m_syncDevices.begin(); it != m_syncDevices.end(); ++it) {
        if(boost::iequals(it->m_deviceId, device.m_deviceId)) {
            break;
        }
    }
    if(it == m_syncDevices.end()) {
        m_syncDevices.push_back(device);
        templatesChanged();
    }
}

void Server::removeDevice(const string &deviceId)
{
    SyncConfig::DeviceList::iterator syncDevIt;
    for(syncDevIt = m_syncDevices.begin(); syncDevIt != m_syncDevices.end(); ++syncDevIt) {
        if(boost::equals(syncDevIt->m_deviceId, deviceId)) {
            m_syncDevices.erase(syncDevIt);
            templatesChanged();
            break;
        }
    }
}

void Server::updateDevice(const string &deviceId,
                          const SyncConfig::DeviceDescription &device)
{
    SyncConfig::DeviceList::iterator it;
    for(it = m_syncDevices.begin(); it != m_syncDevices.end(); ++it) {
        if(boost::iequals(it->m_deviceId, deviceId)) {
            (*it) = device;
            templatesChanged();
            break;
        }
    }
}

void Server::messagev(Level level,
                      const char *prefix,
                      const char *file,
                      int line,
                      const char *function,
                      const char *format,
                      va_list args)
{
    // iterating over args in messagev() is destructive, must make a copy first
    va_list argsCopy;
    va_copy(argsCopy, args);
    m_parentLogger.messagev(level, prefix, file, line, function, format, args);
    string log = StringPrintfV(format, argsCopy);
    va_end(argsCopy);

    // prefix is used to set session path
    // for general server output, the object path field is dbus server
    // the object path can't be empty for object paths prevent using empty string.
    string strLevel = Logger::levelToStr(level);
    if(m_activeSession) {
        logOutput(m_activeSession->getPath(), strLevel, log);
    } else {
        logOutput(getPath(), strLevel, log);
    }
}

SE_END_CXX
