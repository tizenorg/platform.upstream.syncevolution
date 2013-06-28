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

#include "config.h"
#include <stddef.h>
#include <iostream>
#include <memory>
using namespace std;

#include <libgen.h>
#ifdef HAVE_GLIB
#include <glib-object.h>
#endif

#include <syncevo/Cmdline.h>
#include "EvolutionSyncSource.h"
#include <syncevo/SyncContext.h>
#include <syncevo/LogRedirect.h>
#include "CmdlineSyncClient.h"

#include <dlfcn.h>
#include <signal.h>

#include <syncevo/declarations.h>

#ifdef DBUS_SERVICE

#include <gdbus-cxx-bridge.h>

struct SourceStatus {
    string m_mode;
    string m_status;
    uint32_t m_error;
};
template<> struct dbus_traits<SourceStatus> :
public dbus_struct_traits<SourceStatus,
       dbus_member<SourceStatus, string, &SourceStatus::m_mode,
       dbus_member<SourceStatus, string, &SourceStatus::m_status,
       dbus_member_single<SourceStatus, uint32_t, &SourceStatus::m_error> > > >
{};
#endif

SE_BEGIN_CXX

#if defined(ENABLE_MAEMO) && defined (ENABLE_EBOOK)

// really override the symbol, even if redefined by EDSAbiWrapper
#undef e_contact_new_from_vcard
extern "C" EContact *e_contact_new_from_vcard(const char *vcard)
{
    static typeof(e_contact_new_from_vcard) *impl;

    if (!impl) {
        impl = (typeof(impl))dlsym(RTLD_NEXT, "e_contact_new_from_vcard");
    }

    // Old versions of EDS-DBus parse_changes_array() call
    // e_contact_new_from_vcard() with a pointer which starts
    // with a line break; Evolution is not happy with that and
    // refuses to parse it. This code forwards until it finds
    // the first non-whitespace, presumably the BEGIN:VCARD.
    while (*vcard && isspace(*vcard)) {
        vcard++;
    }

    return impl ? impl(vcard) : NULL;
}
#endif

/**
 * This is a class derived from Cmdline. The purpose
 * is to implement the factory method 'createSyncClient' to create
 * new implemented 'CmdlineSyncClient' objects.
 */
class KeyringSyncCmdline : public Cmdline {
 public:
    KeyringSyncCmdline(int argc, const char * const * argv, ostream &out, ostream &err):
        Cmdline(argc, argv, out, err) 
    {}
    /**
     * create a user implemented sync client.
     */
    SyncContext* createSyncClient() {
        return new CmdlineSyncClient(m_server, true, m_keyring);
    }
};

#ifdef DBUS_SERVICE
class RemoteSession;
typedef map<string, StringMap> Config_t;

/**
 * Act as a dbus server. All requests to dbus server
 * are passed through this class.
 */
class RemoteDBusServer : public DBusRemoteObject
{
public:
    RemoteDBusServer();

    virtual const char *getDestination() const {return "org.syncevolution";}
    virtual const char *getPath() const {return "/org/syncevolution/Server";}
    virtual const char *getInterface() const {return "org.syncevolution.Server";}
    virtual DBusConnection *getConnection() const {return m_conn.get();}
    GMainLoop *getLoop() { return m_loop; }

    /** 
     * Check whether the server is started and can be attached.
     * Printing an error message is optional, some callers might
     * prefer a different kind of error handling.
     */
    bool checkStarted(bool printError = true);

    /**
     * execute arguments from command line
     * @param args the arguments of command line
     * @param config the config name parsed from arguments if has
     * @param runSync arguments to run a sync
     * @return true if successfully
     */
    bool execute(const vector<string> &args, const string &config, bool runSync);

    /**
     * To implement the feature of '--monitor' option, monitor a
     * given config if there is a session running.
     * If config is empty, then peak a running session to monitor.
     * @param config the config name parsed from arguments if has
     * @return true if successfully
     */ 
    bool monitor(const string &config);

    /**
     * To implement the feature of '--status' without a server.
     * get and print all running sessions in the dbus server
     */
    bool runningSessions();

    /** whether the dbus call(s) has/have completed */
    bool done() { return m_replyTotal == m_replyCounter; }

    /** one reply returns. Increase reply counter. */
    void replyInc();

    /** set whether there is an error */
    void setResult(bool result) { m_result = result; }

    /** call 'Server.InfoResponse' */
    void infoResponse(const string &id, const string &state, const StringMap &resp);

private:
    /** call 'Attach' until it returns */
    void attachSync();

    /** 
     * callback of 'Server.Attach':
     * also set up a watch and add watch callback when the daemon is gone,
     * then do version check before returning
     */
    void attachCb(const boost::shared_ptr<Watch> &watch, const string &error);

    /**
     * second half of attaching: check version and print warning
     */
    void versionCb(const StringMap &versions, const string &error);

    /** callback of 'Server.GetSessions' */
    void getSessionsCb(const vector<string> &sessions, const string &error);

    /** callback of 'Server.SessionChanged' */
    void sessionChangedCb(const DBusObject_t &object, bool active);

    /** callback of 'Server.LogOutput' */
    void logOutputCb(const DBusObject_t &object, const string &level, const string &log);

    /** callback of 'Server.InfoRequest' */
    void infoReqCb(const string &,
                   const DBusObject_t &,
                   const string &,
                   const string &,
                   const string &,
                   const StringMap &);

    /** callback of Server.InfoResponse */
    void infoResponseCb(const string &error);

    /** callback of calling 'Server.StartSession' */
    void startSessionCb(const DBusObject_t &session, const string &error);

    /** update active session vector according to 'SessionChanged' signal */
    void updateSessions(const string &session, bool active);

    /** check m_session is active */
    bool isActive();

    /** get all running sessions. Used internally. */
    void getRunningSessions();

    /** called when daemon has gone */
    void daemonGone();

    /** set the total number of replies we must wait */
    void resetReplies(int total = 1) 
    { 
        m_replyTotal = total;
        m_replyCounter = 0; 
    }

    /** signal handler for 'CTRL-C' */
    static void handleSignal(int sig);

    // session used for signal handler, 
    // used to call 'suspend' and 'abort'
    static boost::weak_ptr<RemoteSession> g_session;

    // the main loop
    GMainLoop *m_loop;
    // connection
    DBusConnectionPtr m_conn;
    // whether client can attach to the daemon. 
    // It is also used to indicate whether daemon is ready to use.
    bool m_attached;
    // error flag
    bool m_result;
    // config name
    string m_configName;
    // active session object path
    boost::shared_ptr<string> m_activeSession;
    // session created or monitored
    boost::shared_ptr<RemoteSession> m_session;
    // active sessions after listening to 'SessionChanged' signals
    vector<string> m_activeSessions;
    // all sessions in dbus server
    vector<boost::shared_ptr<RemoteSession> >  m_sessions;
    // the number of total dbus calls  
    unsigned int m_replyTotal;
    // the number of returned dbus calls 
    unsigned int m_replyCounter;
    // sessions which are running
    vector<boost::weak_ptr<RemoteSession> > m_runSessions;
    // listen to dbus server signal 'SessionChanged'
    SignalWatch2<DBusObject_t, bool> m_sessionChanged;
    // listen to dbus server signal 'LogOutput'
    SignalWatch3<DBusObject_t, string, string> m_logOutput;
    // listen to dbus server signal 'InfoRequest'
    SignalWatch6<string, 
                 DBusObject_t,
                 string,
                 string,
                 string,
                 StringMap > m_infoReq; 

    /** watch daemon whether it is gone */
    boost::shared_ptr<Watch> m_daemonWatch;
};

/**
 * Act as a session. All requests to a session are passed
 * through this class.
 */
class RemoteSession : public DBusRemoteObject
{
public:
    RemoteSession(RemoteDBusServer &server, const std::string &path);
    virtual const char *getDestination() const {return "org.syncevolution";}
    virtual const char *getPath() const {return m_path.c_str();}
    virtual const char *getInterface() const {return "org.syncevolution.Session";}
    virtual DBusConnection *getConnection() const {return m_server.getConnection();}
    RemoteDBusServer &getServer() { return m_server; }

    /**
     * call 'Execute' method of 'Session' in dbus server
     * without waiting for return
     */
    void executeAsync(const vector<string> &args);

    /**
     * call 'GetStatus' method of 'Session' in dbus server
     * without waiting for return
     */
    void getStatusAsync();

    /**
     * call 'Suspend' method of 'Session' in dbus server
     * without waiting for return
     */
    void suspendAsync();

    /**
     * call 'Abort' method of 'Session' in dbus server
     * without waiting for return
     */
    void abortAsync();

    /**
     * call 'GetConfig' method of 'Session' in dbus server
     * without waiting for return
     */
    void getConfigAsync();

    /** get config name of this session */
    string configName() { return m_configName; }

    /** status 'done' is sent by session */
    bool statusDone() { return boost::iequals(m_status, "done"); }

    /** get current status */
    string status() { return m_status; }

    /** set the flag to indicate the session is running sync */
    void setRunSync(bool runSync) { m_runSync = runSync; }

    /** monitor status of the sesion until it is done */
    void monitorSync();

    /** pass through logoutput and print them if m_output is true */
    void logOutput(Logger::Level level, const string &log);

    /** set whether to print output */
    void setOutput(bool output) { m_output = output; }

    /** process signals from daemon */
    void infoReq(const string &id,
                 const DBusObject_t &session,
                 const string &state,
                 const string &handler,
                 const string &type,
                 const StringMap &params);

    /** remove InfoReq objects from map */
    void removeInfoReq(const string &id);

    typedef std::map<std::string, SourceStatus> SourceStatuses_t;

private:
    /**
     * InfoReq to handle info requests from daemon and
     * call 'Server.InfoResponse' to send its response
     */
    class InfoReq
    {
        /** the session reference */
        RemoteSession &m_session;
        /** the id of InfoRequest */
        string m_id;
        /** the type of InfoRequest */
        string m_type;
        /** the response map sent to the daemon*/
        StringMap m_resp;

        /** InfoRequest state */
        enum State {
            INIT, // init
            WORKING, //'working'
            RESPONSE, // 'response'
            DONE // 'done'
        };
        /** the current state of InfoRequest */
        State m_state;
    public:
        InfoReq(RemoteSession &session, const string &id, const string &type);

        /**
         * process the info request dispatched by session
         */
        void process(const string &id,
                     const DBusObject_t &session,
                     const string &state,
                     const string &handler,
                     const string &type,
                     const StringMap &params);
    };

    /** callback of calling 'Session.Execute' */
    void executeCb(const string &error);

    /** callback of 'Session.GetStatus' */
    void getStatusCb(const string &status,
                     uint32_t errorCode,
                     const SourceStatuses_t &sourceStatus,
                     const string &error);

    /** callback of 'Session.GetConfig' */
    void getConfigCb(const Config_t &config, const string &error);

    /** callback of 'Session.StatusChanged' */
    void statusChangedCb(const string &status,
                         uint32_t errorCode,
                         const SourceStatuses_t &sourceStatus);

    /** callback of 'Session.Suspend' */
    void suspendCb(const string &);

    /** callback of 'Session.Abort' */
    void abortCb(const string &);

    /**
     * implement requirements from info req. Called by InfoReq.
     */
    void handleInfoReq(const string &type, const StringMap &params, StringMap &resp);

    /** dbus server */
    RemoteDBusServer &m_server;

    /* whether to log output */
    bool m_output;

    /** object path */
    string m_path;

    /** config name of the session */
    string m_configName;

    /** current status */
    string m_status;
    
    /** session is running sync */
    bool m_runSync;

    /** signal watch 'StatusChanged' */
    SignalWatch3<std::string, uint32_t, SourceStatuses_t> m_statusChanged;

    /** InfoReq map. store all infoReq belongs to this session */
    map<string, boost::shared_ptr<InfoReq> > m_infoReqs;
};

/**
 * Get current known environment variables, which might be used
 * in executing command line arguments. This is only necessary
 * when using dbus daemon.
 * @param vars the returned environment variables
 */
static void getEnvVars(map<string, string> &vars);

#endif

extern "C"
int main( int argc, char **argv )
{
    // Intercept stderr and route it through our logging.
    // stdout is printed normally. Deconstructing it when
    // leaving main() does one final processing of pending
    // output.
    LogRedirect redirect(false);

#if defined(HAVE_GLIB)
    // this is required when using glib directly or indirectly
    g_type_init();
    g_thread_init(NULL);
    g_set_prgname("syncevolution");
#endif

    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    // Expand PATH to cover the directory we were started from?
    // This might be needed to find normalize_vcard.
    char *exe = strdup(argv[0]);
    if (strchr(exe, '/') ) {
        char *dir = dirname(exe);
        string path;
        char *oldpath = getenv("PATH");
        if (oldpath) {
            path += oldpath;
            path += ":";
        }
        path += dir;
        setenv("PATH", path.c_str(), 1);
    }
    free(exe);

    try {
        if (getenv("SYNCEVOLUTION_DEBUG")) {
            LoggerBase::instance().setLevel(Logger::DEBUG);
        }

        /*
         * don't log errors to cerr: LogRedirect cannot distinguish
         * between our valid error messages and noise from other
         * libs, therefore it would get suppressed (logged at
         * level DEVELOPER, while output is at most INFO)
         */
        KeyringSyncCmdline cmdline(argc, argv, std::cout, std::cout);
        vector<string> parsedArgs;
        if(!cmdline.parse(parsedArgs)) {
            return 1;
        }

        if (cmdline.dontRun()) {
            return 0;
        }

        Cmdline::Bool useDaemon = cmdline.useDaemon();

        if(cmdline.monitor()) {

#ifdef DBUS_SERVICE
            // monitor a session
            RemoteDBusServer server;
            if(server.checkStarted() && server.monitor(cmdline.getConfigName())) {
                return 0;
            }
            return 1;
#else
            SE_LOG_ERROR(NULL, NULL, "this syncevolution binary was compiled without support for monitoring a background sync");
            return 1;
#endif
        } else if(cmdline.status() && 
                  cmdline.getConfigName().empty()) {

#ifdef DBUS_SERVICE
            // '--status' and no server name, try to get running sessions 
            RemoteDBusServer server;
            if(server.checkStarted() && server.runningSessions()) {
                return 0;
            }
            return 1;
#else
            SE_LOG_SHOW(NULL, NULL, "this syncevolution binary was compiled without support for monitoring a background sync");
            return 1;
#endif
        } else if (useDaemon ||
                   !useDaemon.wasSet()) {
#ifdef DBUS_SERVICE
            RemoteDBusServer server;

            // Running execute() without the server available will print errors.
            // Avoid that unless the user explicitly asked for the daemon.
            bool result = server.checkStarted(false);
            if (useDaemon.wasSet() || result) {
                return !server.execute(parsedArgs, cmdline.getConfigName(), cmdline.isSync());
            } else {
                // User didn't select --use-daemon and thus doesn't need to know about it
                // not being available.
                // SE_LOG_SHOW(NULL, NULL, "WARNING: cannot run syncevolution as daemon. "
                //             "Trying to run it without daemon.");
            }
#else
            if (useDaemon.wasSet()) {
                SE_LOG_SHOW(NULL, NULL, "ERROR: this syncevolution binary was compiled without support of daemon. "
                            "Either run syncevolution with '--use-daemon=no' or without that option."); 
                return 1;
            }
#endif
        } 

        // if forcing not using daemon or trying to use daemon with failures,
        // run arguments in the process
        if (!useDaemon.wasSet() ||
            !useDaemon) {
            EDSAbiWrapperInit();

            /*
             * don't log errors to cerr: LogRedirect cannot distinguish
             * between our valid error messages and noise from other
             * libs, therefore it would get suppressed (logged at
             * level DEVELOPER, while output is at most INFO)
             */
            if (cmdline.run()) {
                return 0;
            } else {
                return 1;
            }
        }
    } catch ( const std::exception &ex ) {
        SE_LOG_ERROR(NULL, NULL, "%s", ex.what());
    } catch (...) {
        SE_LOG_ERROR(NULL, NULL, "unknown error");
    }

    return 1;
}

#ifdef DBUS_SERVICE
/********************** RemoteDBusServer implementation **************************/
RemoteDBusServer::RemoteDBusServer()
    :m_attached(false), m_result(true),
     m_replyTotal(0), m_replyCounter(0),
     m_sessionChanged(*this,"SessionChanged"),
     m_logOutput(*this, "LogOutput"),
     m_infoReq(*this, "InfoRequest")
{
    m_loop = g_main_loop_new (NULL, FALSE);
    m_conn = b_dbus_setup_bus(DBUS_BUS_SESSION, NULL, true, NULL);

    if(m_conn) {
        //check whether we can attach to the daemon
        //also set up the daemon watch when attaching to server
        attachSync();
        if(m_attached) {
            m_sessionChanged.activate(boost::bind(&RemoteDBusServer::sessionChangedCb, this, _1, _2));
            m_logOutput.activate(boost::bind(&RemoteDBusServer::logOutputCb, this, _1, _2, _3));
            m_infoReq.activate(boost::bind(&RemoteDBusServer::infoReqCb, this, _1, _2, _3, _4, _5, _6));
        }
    }
}

bool RemoteDBusServer::checkStarted(bool printError)
{
    if(!m_attached) {
        if (printError) {
            SE_LOG_ERROR(NULL, NULL, "SyncEvolution D-Bus server not available.");
        }
        return false;
    }
    return true;
}

void RemoteDBusServer::attachSync()
{
    resetReplies();
    DBusClientCall1<boost::shared_ptr<Watch> > attach(*this, "Attach");
    attach(boost::bind(&RemoteDBusServer::attachCb, this, _1, _2));
    while(!done()) {
        g_main_loop_run(m_loop);
    }
}

void RemoteDBusServer::attachCb(const boost::shared_ptr<Watch> &watch, const string &error)
{
    if(error.empty()) {
        //if attach is successful, watch server whether it is gone
        m_daemonWatch = watch;
        m_daemonWatch->setCallback(boost::bind(&RemoteDBusServer::daemonGone,this));

        // don't print error information, leave it to caller
        m_attached = true;

        // do a version check now before calling replyInc()
        DBusClientCall1< StringMap > getVersions(*this, "GetVersions");
        getVersions(boost::bind(&RemoteDBusServer::versionCb, this, _1, _2));
    } else {
        // done with attach phase, skip version check
        replyInc();
    }
}

void RemoteDBusServer::versionCb(const StringMap &versions,
                                 const string &error)
{
    replyInc();
    if (!error.empty()) {
        SE_LOG_DEBUG(NULL, NULL, "Server.GetVersions(): %s", error.c_str());
    } else {
        StringMap::const_iterator it = versions.find("version");
        if (it != versions.end() &&
            it->second != VERSION) {
            SE_LOG_INFO(NULL, NULL,
                        "proceeding despite version mismatch between command line client 'syncevolution' and 'syncevo-dbus-server' (%s != %s)",
                        it->second.c_str(),
                        VERSION);
        }
    }
}

void RemoteDBusServer::logOutputCb(const DBusObject_t &object,
                                   const string &level,
                                   const string &log)
{
    if (m_session && 
        (boost::equals(object, getPath()) ||
         boost::equals(object, m_session->getPath()))) {
        m_session->logOutput(Logger::strToLevel(level.c_str()), log);
    }
}

void RemoteDBusServer::infoReqCb(const string &id,
                                 const DBusObject_t &session,
                                 const string &state,
                                 const string &handler,
                                 const string &type,
                                 const StringMap &params)
{
    // if m_session is null, just ignore
    if(m_session) {
        m_session->infoReq(id, session, state, handler, type, params);
    }
}

void RemoteDBusServer::infoResponse(const string &id,
                                    const string &state,
                                    const StringMap &resp)
{
    //call Server.InfoResponse
    DBusClientCall0 call(*this, "InfoResponse");
    call(id, state, resp, boost::bind(&RemoteDBusServer::infoResponseCb, this, _1));
}

void RemoteDBusServer::infoResponseCb(const string &error)
{
    replyInc();
    if(!error.empty()) {
        SE_LOG_ERROR(NULL, NULL, "information response failed.");
        m_result = false;
    }
    g_main_loop_quit(m_loop);
}

void RemoteDBusServer::sessionChangedCb(const DBusObject_t &object, bool active)
{
    // update active sessions if needed
    updateSessions(object, active);
    g_main_loop_quit(m_loop);
}

void RemoteDBusServer::daemonGone()
{
    //print error info and exit
    SE_LOG_ERROR(NULL, NULL, "Background sync daemon has gone.");
    exit(1);
}

/**
 * Don't hang onto a shared_ptr here!
 *
 * RemoteSessions contain a reference to the
 * RemoteDBusServer which created them. Once that
 * server destructs, all sessions must have been
 * deleted earlier, otherwise they'll call a destructed
 * object.
 */
boost::weak_ptr<RemoteSession> RemoteDBusServer::g_session;
void RemoteDBusServer::handleSignal(int sig)
{
    SyncContext::handleSignal(sig);
    boost::shared_ptr<RemoteSession> session = g_session.lock();
    if (session) {
        const SuspendFlags &flags = SyncContext::getSuspendFlags(); 
        if(flags.state == SuspendFlags::CLIENT_SUSPEND) {
            session->suspendAsync();
        } else if(flags.state == SuspendFlags::CLIENT_ABORT) {
            session->abortAsync();
        }
    }
}

bool RemoteDBusServer::execute(const vector<string> &args, const string &peer, bool runSync)
{
    //the basic workflow is:
    //1) start a session 
    //2) waiting for the session becomes active
    //3) execute 'arguments' once it is active

    // start a new session
    DBusClientCall1<DBusObject_t> startSession(*this, "StartSessionWithFlags");
    std::vector<std::string> flags;
    if (!runSync) {
        flags.push_back("no-sync");
    }
    startSession(peer, flags, boost::bind(&RemoteDBusServer::startSessionCb, this, _1, _2));

    // wait until 'StartSession' returns
    resetReplies();
    while(!done()) {
        g_main_loop_run(m_loop);
    }

    if(m_session) {
        m_session->setRunSync(true);

        //if session is not active, just wait
        while(!isActive()) {
            g_main_loop_run(m_loop);
        }
        // Logger::Level level = LoggerBase::instance().getLevel();
        // LoggerBase::instance().setLevel(Logger::DEBUG);
        resetReplies();
        m_session->executeAsync(args);

        while(!done()) {
            g_main_loop_run(m_loop);
        }

        //if encoutering errors, return
        if(!m_result) {
            return m_result;
        }

        //g_session is used to pass 'abort' or 'suspend' commands
        //make sure session is ready to run
        g_session = m_session;

        //set up signal handlers to send 'suspend' or 'abort' to dbus server
        //only do this once session is executing and can suspend and abort
        struct sigaction new_action, old_action;
        struct sigaction old_term_action;

        if(runSync) {
            memset(&new_action, 0, sizeof(new_action));
            new_action.sa_handler = handleSignal;
            sigemptyset(&new_action.sa_mask);
            sigaction(SIGINT, NULL, &old_action);
            if (old_action.sa_handler == SIG_DFL) {
                sigaction(SIGINT, &new_action, NULL);
            }

            sigaction(SIGTERM, NULL, &old_term_action);
            if (old_term_action.sa_handler == SIG_DFL) {
                sigaction(SIGTERM, &new_action, NULL);
            }   
        }

        //wait until status is 'done'
        while(!m_session->statusDone()) {
            g_main_loop_run(m_loop);
        }

        if(runSync) {
            sigaction (SIGINT, &old_action, NULL);
            sigaction (SIGTERM, &old_term_action, NULL);
        }

        //reset session
        g_session.reset();
        //restore logging level
        // LoggerBase::instance().setLevel(level);
        m_session->setRunSync(false);
    }
    return m_result;
}

void RemoteDBusServer::startSessionCb(const DBusObject_t &sessionPath, const string &error)
{
    replyInc();
    if(!error.empty()) {
        SE_LOG_ERROR(NULL, NULL, "starting D-Bus session failed: %s", error.c_str());
        if (error.find("org.freedesktop.DBus.Error.UnknownMethod") != error.npos) {
            SE_LOG_INFO(NULL, NULL, "syncevo-dbus-server is most likely too old");
        }
        m_result = false;
        g_main_loop_quit(m_loop);
        return;
    }
    m_session.reset(new RemoteSession(*this, sessionPath));
    g_main_loop_quit(m_loop);
}

bool RemoteDBusServer::isActive()
{
    /** if current session is active and then start to call 'Execute' method */
    if(m_session) {
        BOOST_FOREACH(const string &session, m_activeSessions) {
            if(boost::equals(m_session->getPath(), session.c_str())) {
                return true;
            }
        }
    }
    return false;
}

void RemoteDBusServer::getRunningSessions()
{
    //get all sessions
    DBusClientCall1<vector<string> > sessions(*this, "GetSessions");
    sessions(boost::bind(&RemoteDBusServer::getSessionsCb, this, _1, _2));
    resetReplies();
    while(!done()) {
        g_main_loop_run(m_loop);
    }

    // get status of each session
    resetReplies(m_sessions.size());
    BOOST_FOREACH(boost::shared_ptr<RemoteSession> &session, m_sessions) {
        session->getStatusAsync();
    }

    // waiting for all sessions 'GetStatus'
    while(!done()) {
        g_main_loop_run(m_loop);
    }

    // collect running sessions
    BOOST_FOREACH(boost::shared_ptr<RemoteSession> &session, m_sessions) {
        if(boost::istarts_with(session->status(), "running")) {
            m_runSessions.push_back(boost::weak_ptr<RemoteSession>(session));
        }
    }
}

bool RemoteDBusServer::runningSessions()
{
    //the basic working flow is:
    //1) get all sessions
    //2) check each session and collect running sessions
    //3) get config name of running sessions and print them
    getRunningSessions();

    if(m_runSessions.empty()) {
        SE_LOG_SHOW(NULL, NULL, "Background sync daemon is idle.");
    } else {
        SE_LOG_SHOW(NULL, NULL, "Running session(s): ");

        resetReplies(m_runSessions.size());
        BOOST_FOREACH(boost::weak_ptr<RemoteSession> &session, m_runSessions) {
            boost::shared_ptr<RemoteSession> lock = session.lock();
            if(lock) {
                lock->getConfigAsync();
            }
        }

        //wait for 'GetConfig' returns
        while(!done()) {
            g_main_loop_run(m_loop);
        }

        // print all running sessions
        BOOST_FOREACH(boost::weak_ptr<RemoteSession> &session, m_runSessions) {
            boost::shared_ptr<RemoteSession> lock = session.lock();
            if(!lock->configName().empty()) {
                SE_LOG_SHOW(NULL, NULL, "   %s (%s)", lock->configName().c_str(), lock->getPath());
            }
        }
    }
    return m_result;
}

void RemoteDBusServer::getSessionsCb(const vector<string> &sessions, const string &error)
{
    replyInc();
    if(!error.empty()) {
        SE_LOG_ERROR(NULL, NULL, "getting session failed: %s", error.c_str());
        m_result = false;
        g_main_loop_quit(m_loop);
        return;
    }

    //create local objects for sessions
    BOOST_FOREACH(const DBusObject_t &value, sessions) {
        boost::shared_ptr<RemoteSession> session(new RemoteSession(*this, value));
        m_sessions.push_back(session);
    }
}

void RemoteDBusServer::updateSessions(const string &session, bool active)
{
    if(active) {
        //add it into active list
        m_activeSessions.push_back(session);
    } else {
        //if inactive, remove it from active list
        for(vector<string>::iterator it = m_activeSessions.begin();
                it != m_activeSessions.end(); ++it) {
            if(boost::equals(session, *it)) {
                m_activeSessions.erase(it);
                break;
            }
        }
    }
}

void RemoteDBusServer::replyInc()
{
    // increase counter and check whether all replies are returned
    m_replyCounter++;
    if(done()) {
        g_main_loop_quit(m_loop);
    }
}

bool RemoteDBusServer::monitor(const string &peer)
{
    //the basic working flow is:
    //1) get all sessions
    //2) check each session and collect running sessions
    //3) peak one session with the given peer and monitor it
    getRunningSessions();
    if(peer.empty()) {
        //peak the first running sessions
        BOOST_FOREACH(boost::weak_ptr<RemoteSession> &session, m_runSessions) {
            boost::shared_ptr<RemoteSession> lock = session.lock();
            if(lock) {
                m_session = lock;
                resetReplies();
                m_session->getConfigAsync();
                while(!done()) {
                    g_main_loop_run(m_loop);
                }
                m_session->monitorSync();
                return m_result;
            }
        }
        //if no running session
        SE_LOG_SHOW(NULL, NULL, "Background sync daemon is idle, no session available to be be monitored.");
    } else {
        string peerNorm = SyncConfig::normalizeConfigString(peer);

        // get config names of running sessions
        resetReplies(m_runSessions.size());
        BOOST_FOREACH(boost::weak_ptr<RemoteSession> &session, m_runSessions) {
            boost::shared_ptr<RemoteSession> lock = session.lock();
            lock->getConfigAsync();
        }
        //wait for 'GetConfig' returns
        while(!done()) {
            g_main_loop_run(m_loop);
        }

        //find a session with the given name
        vector<boost::shared_ptr<RemoteSession> >::iterator it = m_sessions.begin();
        while(it != m_sessions.end()) {
            string tempNorm = (*it)->configName();
            if (peerNorm == tempNorm) {
                m_session = *it;
                //monitor the session status
                m_session->monitorSync();
                return m_result;
            }
            it++;
        }
        SE_LOG_SHOW(NULL, NULL, "'%s' is not running.", peer.c_str());
    }
    return m_result;
}


/********************** RemoteSession implementation **************************/
RemoteSession::RemoteSession(RemoteDBusServer &server,
        const string &path)
    :m_server(server), m_output(false), m_path(path), m_runSync(false),
    m_statusChanged(*this, "StatusChanged")
{
    m_statusChanged.activate(boost::bind(&RemoteSession::statusChangedCb, this, _1, _2, _3));
}

void RemoteSession::executeAsync(const vector<string> &args)
{
    //start to print outputs
    m_output = true;
    map<string, string> vars;
    getEnvVars(vars);
    DBusClientCall0 call(*this, "Execute");
    call(args, vars, boost::bind(&RemoteSession::executeCb, this, _1));
}

void RemoteSession::executeCb(const string &error)
{
    m_server.replyInc();
    if(!error.empty()) {
        SE_LOG_ERROR(NULL, NULL, "running the command line inside the D-Bus server failed");
        m_server.setResult(false);
        //end to print outputs
        m_output = false;
        return;
    }
}

void RemoteSession::statusChangedCb(const string &status,
        uint32_t errorCode,
        const SourceStatuses_t &sourceStatus)
{
    m_status = status;

    if (errorCode) {
        m_server.setResult(false);
        g_main_loop_quit(m_server.getLoop());
    }

    if(status == "done") {
        //if session is done, quit the loop
        g_main_loop_quit(m_server.getLoop());
        m_output = false;
    }
}

void RemoteSession::getStatusAsync()
{
    DBusClientCall3<string, uint32_t, SourceStatuses_t> call(*this, "GetStatus");
    call(boost::bind(&RemoteSession::getStatusCb, this, _1, _2, _3, _4));
}

void RemoteSession::getStatusCb(const string &status,
        uint32_t errorCode,
        const SourceStatuses_t &sourceStatus,
        const string &error)
{
    m_server.replyInc();
    if(!error.empty()) {
        //ignore the error
        return;
    }
    m_status = status;
}

void RemoteSession::getConfigAsync()
{
    DBusClientCall1<Config_t> call(*this, "GetConfig");
    call(false, boost::bind(&RemoteSession::getConfigCb, this, _1, _2));
}

void RemoteSession::getConfigCb(const Config_t &config, const string &error)
{
    m_server.replyInc();
    if(!error.empty()) {
        //ignore the error
        return;
    }
    // set config name
    Config_t::const_iterator it = config.find("");
    if(it != config.end()) {
        StringMap global = it->second;
        StringMap::iterator git = global.find("configName");
        if(git != global.end()) {
            m_configName = git->second;
        }
    }
}

void RemoteSession::suspendAsync()
{
    DBusClientCall0 suspend(*this, "Suspend");
    suspend(boost::bind(&RemoteSession::suspendCb, this, _1));
}

void RemoteSession::suspendCb(const string &error)
{
    //avoid logging messages in handleSignal
    SyncContext::printSignals();
    if(!error.empty()) {
        m_server.setResult(false);
    }
}

void RemoteSession::abortCb(const string &error)
{
    //avoid logging messages in handleSignal
    SyncContext::printSignals();
    if(!error.empty()) {
        m_server.setResult(false);
    }
}

void RemoteSession::abortAsync()
{
    DBusClientCall0 abort(*this, "Abort");
    abort(boost::bind(&RemoteSession::abortCb, this, _1));
}

void RemoteSession::logOutput(Logger::Level level, const string &log)
{
    if(m_output) {
        SE_LOG(level, NULL, NULL, "%s", log.c_str());
    }
}

void RemoteSession::monitorSync()
{
    m_output = true;
    // Logger::Level level = LoggerBase::instance().getLevel();
    // LoggerBase::instance().setLevel(Logger::DEBUG);
    SE_LOG(Logger::SHOW, NULL, NULL, "Monitoring '%s' (%s)\n", m_configName.c_str(), getPath());

    while(!statusDone()) {
        g_main_loop_run(m_server.getLoop());
    }

    SE_LOG(Logger::SHOW, NULL, NULL, "Monitoring done");
    // LoggerBase::instance().setLevel(level);
    m_output = false;
}

void RemoteSession::infoReq(const string &id,
                            const DBusObject_t &session,
                            const string &state,
                            const string &handler,
                            const string &type,
                            const StringMap &params)
{
    //if command line runs a sync, then try to handle req
    if (m_runSync && boost::iequals(session, getPath())) {
        //only handle password now
        if (boost::iequals("password", type)) {
            map<string, boost::shared_ptr<InfoReq> >::iterator it = m_infoReqs.find(id);
            if (it != m_infoReqs.end()) {
                it->second->process(id, session, state, handler, type, params);
            } else {
                boost::shared_ptr<InfoReq> passwd(new InfoReq(*this, id, type));
                m_infoReqs[id] = passwd;
                passwd->process(id, session, state, handler, type, params);
            }
        } 
    }
}

void RemoteSession::handleInfoReq(const string &type, const StringMap &params, StringMap &resp)
{
    if (boost::iequals(type, "password")) {
        char buffer[256];

        string descr;
        StringMap::const_iterator it = params.find("description");
        if (it != params.end()) {
            descr = it->second;
        }
        printf("Enter password for %s: ", descr.c_str());
        fflush(stdout);
        if (fgets(buffer, sizeof(buffer), stdin) &&
                strcmp(buffer, "\n")) {
            size_t len = strlen(buffer);
            if (len && buffer[len - 1] == '\n') {
                buffer[len - 1] = 0;
            }
            resp["password"] = string(buffer);
        } else {
            SE_LOG_ERROR(NULL, NULL, "could not read password for %s", descr.c_str());
        }
    }
}

void RemoteSession::removeInfoReq(const string &id) 
{
    map<string, boost::shared_ptr<InfoReq> >::iterator it = m_infoReqs.find(id);
    if (it != m_infoReqs.end()) {
        m_infoReqs.erase(it);
    }
}

/********************** InfoReq implementation **************************/
RemoteSession::InfoReq::InfoReq(RemoteSession &session,
                                const string &id,
                                const string &type)
    :m_session(session), m_id(id), m_type(type), m_state(INIT)
{
}

void RemoteSession::InfoReq::process(const string &id,
                                     const DBusObject_t &session,
                                     const string &state,
                                     const string &handler,
                                     const string &type,
                                     const StringMap &params)
{
    //only handle info belongs to this InfoReq
    if (boost::equals(m_id, id)) {
        //check the state and response if necessary
        if (m_state == INIT && boost::iequals("request", state)) {
            m_session.getServer().infoResponse(m_id, "working", StringMap());
            m_state = WORKING;
            m_session.handleInfoReq(type, params, m_resp);
        } else if ((m_state == WORKING) && boost::iequals("waiting", state)) {
            m_session.getServer().infoResponse(m_id, "response", m_resp);
            m_state = RESPONSE;
        } else if (boost::iequals("done", state)) {
            //if request is 'done', remove it
            m_session.removeInfoReq(m_id);
        }
    }
}

void getEnvVars(map<string, string> &vars)
{
    //environment variables used to run command line
    static const char *varNames[] = {
        "http_proxy",
        "HOME",
        "PATH",
        "SYNCEVOLUTION_BACKEND_DIR",
        "SYNCEVOLUTION_DEBUG",
        "SYNCEVOLUTION_GNUTLS_DEBUG",
        "SYNCEVOLUTION_TEMPLATE_DIR",
        "SYNCEVOLUTION_XML_CONFIG_DIR",
        "SYNC_EVOLUTION_EVO_CALENDAR_DELAY",
        "XDG_CACHE_HOME",
        "XDG_CONFIG_HOME",
        "XDG_DATA_HOME"
    };

    for (unsigned int i = 0; i < sizeof(varNames) / sizeof(const char*); i++) {
        const char *value;
        //get values of environment variables if they are set
        if ((value = getenv(varNames[i])) != NULL) {
            vars.insert(make_pair(varNames[i], value));
        }
    }
}

#endif

SE_END_CXX
