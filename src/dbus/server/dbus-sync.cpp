/*
 * Copyright (C) 2011 Intel Corporation
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

#include "dbus-sync.h"
#include "session.h"
#include "dbus-transport-agent.h"
#include "server.h"

SE_BEGIN_CXX

DBusSync::DBusSync(const std::string &config,
                   Session &session) :
    DBusUserInterface(config),
    m_session(session)
{
    #ifdef USE_KDE_KWALLET
    //QCoreApplication *app;
    //if (!qApp) {
        //int argc = 1;
        //app = new QCoreApplication(argc, (char *[1]){ (char*) "syncevolution"});
    //}
    int argc = 1;
    static const char *prog = "syncevolution";
    static char *argv[] = { (char *)&prog, NULL };
    //if (!qApp) {
        //new QCoreApplication(argc, argv);
    //}
    KAboutData aboutData(// The program name used internally.
                         "syncevolution",
                         // The message catalog name
                         // If null, program name is used instead.
                         0,
                         // A displayable program name string.
                         ki18n("Syncevolution"),
                         // The program version string.
                         "1.0",
                         // Short description of what the app does.
                         ki18n("Lets Akonadi synchronize with a SyncML Peer"),
                         // The license this code is released under
                         KAboutData::License_GPL,
                         // Copyright Statement
                         ki18n("(c) 2010"),
                         // Optional text shown in the About box.
                         // Can contain any information desired.
                         ki18n(""),
                         // The program homepage string.
                         "http://www.syncevolution.org/",
                         // The bug report email address
                         "syncevolution@syncevolution.org");

    KCmdLineArgs::init(argc, argv, &aboutData);
    if (!kapp) {
        new KApplication;
        //To stop KApplication from spawning it's own DBus Service ... Will have to patch KApplication about this
        QDBusConnection::sessionBus().unregisterService("org.syncevolution.syncevolution-"+QString::number(getpid()));
    }
    #endif

}

boost::shared_ptr<TransportAgent> DBusSync::createTransportAgent()
{
    if (m_session.useStubConnection()) {
        // use the D-Bus Connection to send and receive messages
        boost::shared_ptr<TransportAgent> agent(new DBusTransportAgent(m_session.getServer().getLoop(),
                                                                       m_session,
                                                                       m_session.getStubConnection()));
        // We don't know whether we'll run as client or server.
        // But we as we cannot resend messages via D-Bus even if running as
        // client (API not designed for it), let's use the hard timeout
        // from RetryDuration here.
        int timeout = getRetryDuration();
        agent->setTimeout(timeout);
        return agent;
    } else {
        // no connection, use HTTP via libsoup/GMainLoop
        GMainLoop *loop = m_session.getServer().getLoop();
        boost::shared_ptr<TransportAgent> agent = SyncContext::createTransportAgent(loop);
        return agent;
    }
}

void DBusSync::displaySyncProgress(sysync::TProgressEventEnum type,
                                   int32_t extra1, int32_t extra2, int32_t extra3)
{
    SyncContext::displaySyncProgress(type, extra1, extra2, extra3);
    m_session.syncProgress(type, extra1, extra2, extra3);
}

void DBusSync::displaySourceProgress(sysync::TProgressEventEnum type,
                                     SyncSource &source,
                                     int32_t extra1, int32_t extra2, int32_t extra3)
{
    SyncContext::displaySourceProgress(type, source, extra1, extra2, extra3);
    m_session.sourceProgress(type, source, extra1, extra2, extra3);
}

void DBusSync::reportStepCmd(sysync::uInt16 stepCmd)
{
    switch(stepCmd) {
        case sysync::STEPCMD_SENDDATA:
        case sysync::STEPCMD_RESENDDATA:
        case sysync::STEPCMD_NEEDDATA:
            //sending or waiting data
            m_session.setStepInfo(true);
            break;
        default:
            // otherwise, processing
            m_session.setStepInfo(false);
            break;
    }
}

void DBusSync::syncSuccessStart()
{
    m_session.syncSuccessStart();
}

bool DBusSync::checkForSuspend()
{
    return m_session.isSuspend() || SyncContext::checkForSuspend();
}

bool DBusSync::checkForAbort()
{
    return m_session.isAbort() || SyncContext::checkForAbort();
}

int DBusSync::sleep(int intervals)
{
    time_t start = time(NULL);
    while (true) {
        g_main_context_iteration(NULL, false);
        time_t now = time(NULL);
        if (checkForSuspend() || checkForAbort()) {
            return  (intervals - now + start);
        }
        if (intervals - now + start <= 0) {
            return intervals - now +start;
        }
    }
}

string DBusSync::askPassword(const string &passwordName,
                             const string &descr,
                             const ConfigPasswordKey &key)
{
    string password = DBusUserInterface::askPassword(passwordName, descr, key);

    if(password.empty()) {
        password = m_session.askPassword(passwordName, descr, key);
    }
    return password;
}

SE_END_CXX