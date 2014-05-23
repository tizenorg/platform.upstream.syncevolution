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

#include <syncevo/LogRedirect.h>

#include "session-helper.h"
#include "dbus-callbacks.h"
#include "cmdline-wrapper.h"

#include <syncevo/SuspendFlags.h>
#include <syncevo/ForkExec.h>

#include <boost/foreach.hpp>

SE_BEGIN_CXX

static void dumpString(const std::string &output)
{
    fputs(output.c_str(), stdout);
}

/**
 * Same logging approach as in Server class: pretend that we
 * have reference counting for the SessionHelper class and
 * use mutex locking to prevent dangling pointers.
 */
class SessionHelperLogger : public Logger
{
    Handle m_parentLogger;
    boost::shared_ptr<SessionHelper> m_helper;
    Level m_dbusLogLevel;

public:
    SessionHelperLogger(const boost::shared_ptr<SessionHelper> &helper):
        m_parentLogger(Logger::instance()),
        m_helper(helper),
        m_dbusLogLevel(DEBUG)
    {
    }

    void setDBusLogLevel(Level level) { m_dbusLogLevel = level; }
    Level getDBusLogLevel() const { return m_dbusLogLevel; }

    virtual void remove() throw ()
    {
        RecMutex::Guard guard = lock();
        m_helper.reset();
    }

    virtual void messagev(const MessageOptions &options,
                          const char *format,
                          va_list args)
    {
        RecMutex::Guard guard = lock();
        static bool dbg = getenv("SYNCEVOLUTION_DEBUG");

        if (dbg) {
            // Let parent LogRedirect or utility function handle the
            // output *in addition* to logging via D-Bus. That way
            // it'll be visible via our stdout/stderr (= console)
            // right away.
            //
            // In non-debug mode, nothing should get written to
            // stdout/stder, which got redirected into a pipe
            // read by our parend. In that mode, the parent will include
            // our output in its own output streams.
            va_list argsCopy;
            va_copy(argsCopy, args);
            if (m_parentLogger) {
                m_parentLogger.messagev(options, format, argsCopy);
            } else {
                formatLines(options.m_level, DEBUG,
                            options.m_processName,
                            options.m_prefix,
                            format, argsCopy,
                            boost::bind(dumpString, _1));
            }
            va_end(argsCopy);
        } else if (m_parentLogger) {
            // Pass through to parent, but marked in such a way that
            // only the DLT and syslog logger will react to it, but
            // not LogStdout. That's necessary because
            // syncevo-dbus-server handles stdout for us.
            va_list argsCopy;
            va_copy(argsCopy, args);
            MessageOptions buffer(options);
            buffer.m_flags |= MessageOptions::ONLY_GLOBAL_LOG;
            m_parentLogger.messagev(buffer, format, argsCopy);
            va_end(argsCopy);
        }

        if (m_helper &&
            options.m_level <= m_dbusLogLevel) {
            // send to parent
            string log = StringPrintfV(format, args);
            string strLevel = Logger::levelToStr(options.m_level);
            try {
                m_helper->emitLogOutput(strLevel, log, options.m_processName ? *options.m_processName : getProcessName());
            } catch (...) {
                // Give up forwarding output.
                m_helper.reset();
            }
        }
    }
};

SessionHelper::SessionHelper(GMainLoop *loop,
                             const GDBusCXX::DBusConnectionPtr &conn,
                             const boost::shared_ptr<ForkExecChild> &forkexec) :
    GDBusCXX::DBusObjectHelper(conn,
                               std::string(SessionCommon::HELPER_PATH) + "/" + forkexec->getInstance(),
                               SessionCommon::HELPER_IFACE,
                               GDBusCXX::DBusObjectHelper::Callback_t(), // we don't care about a callback per message
                               true), // direct connection, close it when done
    m_loop(loop),
    m_conn(conn),
    m_forkexec(forkexec),
    m_logger(new SessionHelperLogger(boost::shared_ptr<SessionHelper>(this, NopDestructor()))),
    emitLogOutput(*this, "LogOutput"),
    emitSyncProgress(*this, "SyncProgress"),
    emitSourceProgress(*this, "SourceProgress"),
    emitSourceSynced(*this, "SourceSynced"),
    emitWaiting(*this, "Waiting"),
    emitSyncSuccessStart(*this, "SyncSuccessStart"),
    emitConfigChanged(*this, "ConfigChanged"),
    emitPasswordRequest(*this, "PasswordRequest"),
    emitMessage(*this, "Message"),
    emitShutdown(*this, "Shutdown")
{
    add(this, &SessionHelper::sync, "Sync");
    add(this, &SessionHelper::restore, "Restore");
    add(this, &SessionHelper::execute, "Execute");
    add(this, &SessionHelper::setFreeze, "SetFreeze");
    add(this, &SessionHelper::passwordResponse, "PasswordResponse");
    add(this, &SessionHelper::storeMessage, "StoreMessage");
    add(this, &SessionHelper::connectionState, "ConnectionState");
    add(emitLogOutput);
    add(emitSyncProgress);
    add(emitSourceProgress);
    add(emitSourceSynced);
    add(emitWaiting);
    add(emitSyncSuccessStart);
    add(emitConfigChanged);
    add(emitPasswordRequest);
    add(emitMessage);
    add(emitShutdown);
}

void SessionHelper::setDBusLogLevel(Logger::Level level)
{
    m_logger->setDBusLogLevel(level);
}

Logger::Level SessionHelper::getDBusLogLevel() const
{
    return m_logger->getDBusLogLevel();
}

void SessionHelper::activate()
{
    GDBusCXX::DBusObjectHelper::activate();
    m_pushLogger.reset(m_logger);
}

SessionHelper::~SessionHelper()
{
    m_pushLogger.reset();
    m_logger.reset();
}

void SessionHelper::run()
{
    SuspendFlags &s = SuspendFlags::getSuspendFlags();
    while (true) {
        if (s.getState() != SuspendFlags::NORMAL) {
            SE_LOG_DEBUG(NULL, "terminating because of suspend or abort signal");
            break;
        }
        if (m_operation &&
            m_operation()) {
            SE_LOG_DEBUG(NULL, "terminating as requested by operation");
            break;
        }
        g_main_loop_run(m_loop);
    }
}

bool SessionHelper::connected()
{
    return m_forkexec && m_forkexec->getState() == ForkExecChild::CONNECTED;
}

void SessionHelper::sync(const SessionCommon::SyncParams &params,
                         const boost::shared_ptr< GDBusCXX::Result2<bool, SyncReport> > &result)
{
    m_operation = boost::bind(&SessionHelper::doSync, this, params, result);
    g_main_loop_quit(m_loop);
}

bool SessionHelper::doSync(const SessionCommon::SyncParams &params,
                           const boost::shared_ptr< GDBusCXX::Result2<bool, SyncReport> > &result)
{
    try {
        m_sync.reset(new DBusSync(params, *this));
        SyncReport report;
        SyncMLStatus status = m_sync->sync(&report);
        if (status) {
            // Clear the abort signal, to allow the process to send
            // out the D-Bus response. Our parent will signal us again
            // after it received the response.
            // TODO
            SE_THROW_EXCEPTION_STATUS(StatusException,
                                      "sync failed",
                                      status);
        }
        result->done(true, report);
    } catch (...) {
        dbusErrorCallback(result);
    }
    m_sync.reset();

    // quit helper
    return true;
}

void SessionHelper::restore(const std::string &configName,
                            const string &dir, bool before, const std::vector<std::string> &sources,
                            const boost::shared_ptr< GDBusCXX::Result1<bool> > &result)
{
    m_operation = boost::bind(&SessionHelper::doRestore, this, configName, dir, before, sources, result);
    g_main_loop_quit(m_loop);
}

bool SessionHelper::doRestore(const std::string &configName,
                              const string &dir, bool before, const std::vector<std::string> &sources,
                              const boost::shared_ptr< GDBusCXX::Result1<bool> > &result)
{
    try {
        SessionCommon::SyncParams params;
        params.m_config = configName;
        DBusSync sync(params, *this);
        if (!sources.empty()) {
            BOOST_FOREACH(const std::string &source, sources) {
                FilterConfigNode::ConfigFilter filter;
                filter["sync"] = InitStateString("two-way", true);
                sync.setConfigFilter(false, source, filter);
            }
            // disable other sources
            FilterConfigNode::ConfigFilter disabled;
            disabled["sync"] = InitStateString("disabled", true);
            sync.setConfigFilter(false, "", disabled);
        }
        sync.restore(dir,
                     before ?
                     SyncContext::DATABASE_BEFORE_SYNC :
                     SyncContext::DATABASE_AFTER_SYNC);
        result->done(true);
    } catch (...) {
        dbusErrorCallback(result);
    }

    // quit helper
    return true;
}


void SessionHelper::execute(const vector<string> &args, const map<string, string> &vars,
                            const boost::shared_ptr< GDBusCXX::Result1<bool> > &result)
{
    m_operation = boost::bind(&SessionHelper::doExecute, this, args, vars, result);
    g_main_loop_quit(m_loop);
}

bool SessionHelper::doExecute(const vector<string> &args, const map<string, string> &vars,
                              const boost::shared_ptr< GDBusCXX::Result1<bool> > &result)
{
    try {
        CmdlineWrapper cmdline(*this, args, vars);
        if (!cmdline.parse()) {
            SE_THROW_EXCEPTION(DBusSyncException, "arguments parsing error");
        }
        bool success = false;

        // a command line operation can be many things, tell parent
        SessionCommon::RunOperation op;
        op = cmdline.isSync() ? SessionCommon::OP_SYNC :
            cmdline.isRestore() ? SessionCommon::OP_RESTORE :
            SessionCommon::OP_CMDLINE;
        emitSyncProgress(sysync::PEV_CUSTOM_START, op, 0, 0);

        try {
            success = cmdline.run();
        } catch (...) {
            if (cmdline.configWasModified()) {
                emitConfigChanged();
            }
            throw;
        }
        if (cmdline.configWasModified()) {
            emitConfigChanged();
        }
        result->done(success);
    } catch (...) {
        dbusErrorCallback(result);
    }

    // quit helper
    return true;
}

void SessionHelper::passwordResponse(bool timedOut, bool aborted, const std::string &password)
{
    if (m_sync) {
        m_sync->passwordResponse(timedOut, aborted, password);
    } else {
        SE_LOG_DEBUG(NULL, "discarding obsolete password response");
    }
}

bool SessionHelper::setFreeze(bool frozen)
{
    return m_sync ?
        m_sync->setFreeze(frozen) :
        false;
}



SE_END_CXX
