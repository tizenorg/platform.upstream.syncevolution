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

#ifndef INCL_SUSPENDFLAGS
#define INCL_SUSPENDFLAGS

#include <signal.h>
#include <stdint.h>
#include <boost/smart_ptr.hpp>
#include <boost/function.hpp>
#include <boost/signals2.hpp>

#include <syncevo/Logging.h>

#include <syncevo/declarations.h>
SE_BEGIN_CXX

/**
 * A singleton which is responsible for signal handling.  Supports
 * "SIGINT = suspend sync" and "two quick successive SIGINT = abort"
 * semantic. SIGTERM is always aborting.
 *
 * Can be polled and in addition, flags state changes by writing to a
 * file descriptor for integration into an event loop.
 *
 * All methods are thread-safe. activate() and deactivate() need to
 * modify global process state and should only be used when it is safe
 * to do so.
 */
class SuspendFlags
{
 public:
    /** SIGINT twice within this amount of seconds aborts the sync */
    static const time_t ABORT_INTERVAL = 2; 
    enum State
    {
        /** keep sync running */
        NORMAL,
        /** suspend sync */
        SUSPEND,
        /** abort sync */
        ABORT,

        /** suspend sync request received again (only written to event FD, not returned by getState()) */
        SUSPEND_AGAIN,
        /** abort sync request received again (only written to event FD, not returned by getState()) */
        ABORT_AGAIN,

        ABORT_MAX
    };

    /** access to singleton */
    static SuspendFlags &getSuspendFlags();

    /**
     * Current status. It is a combination of several indicators:
     * - state set via signals (cannot be reset)
     * - "suspend" while requested with suspend() (resets when no longer needed)
     * - "abort" while requested with aborted() (also resets)
     *
     * The overall state is the maximum (NORMAL < SUSPEND < ABORT).
     */
    State getState() const;

    /**
     * Returns or-ed mask of all signals handled so far.
     * See activate().
     */
    uint32_t getReceivedSignals() const;

    /**
     * Checks for status changes and returns true iff status is ABORT.
     */
    bool isAborted();
    /**
     * Checks for status changes and returns true iff status is SUSPEND.
     */
    bool isSuspended();
    /**
     * Checks for status changes and returns true iff status is NORMAL.
     */
    bool isNormal();

    /**
     * Checks for status changes and throws a "aborting as requested by user" StatusException with
     * LOCERR_USERABORT as status code if the current state is not
     * NORMAL. In other words, suspend and abort are both
     * treated like an abort request.
     */
    void checkForNormal();

    /**
     * Users of this class can read a single char for each received
     * signal from this file descriptor. The char is the State that
      * was entered by that signal. This can be used to be notified
     * immediately about changes, without having to poll.
     *
     * -1 if not activated.
     */
    int getEventFD() const { return m_receiverFD; }

    class Guard {
    public:
        virtual ~Guard() { getSuspendFlags().deactivate(); }
    };

    /**
     * Allocate file descriptors, set signal handlers for the chosen
     * signals (SIGINT and SIGTERM by default). Once the returned
     * guard is freed, it will automatically deactivate signal
     * handling.
     *
     * Additional signals like SIGURG or SIGIO may also be used. It is
     * unlikely that any library used by SyncEvolution occupies these
     * signals for its own use.
     *
     * Only SIGINT and SIGTERM influence the overall State. All
     * received signals, including SIGINT and SIGTERM, are recorded
     * and can be retrieved via getReceivedSignals().
     *
     * It is possible to call activate multiple times. All following
     * calls do nothing except creating a new reference to the same
     * guard.  In particular they cannot add or remove handled
     * signals.
     */
    boost::shared_ptr<Guard> activate(uint32_t sigmask = (1<<SIGINT)|(1<<SIGTERM));

    /**
     * Retrieve state changes pushed into pipe by signal
     * handler and write user-visible messages for them.
     * Guaranteed to not block. Triggers the m_stateChanged
     * Boost signal.
     */
    void printSignals();

    /**
     * Triggered inside the main thread when the state
     * changes. Either printSignals() needs to be called
     * directly or a glib watch must be activated which
     * does that.
     */
    typedef boost::signals2::signal<void (SuspendFlags &)> StateChanged_t;
    StateChanged_t m_stateChanged;

    /**
     * React to a SIGINT or SIGTERM.
     *
     * Installed as signal handler by activate() if no other signal
     * handler was set. May also be called by other signal handlers,
     * regardless whether activated or not.
     */
    static void handleSignal(int sig);

    class StateBlocker {};

    /**
     * Requests a state change to "suspend". The request
     * remains active and affects getState() until
     * the returned StateBlocker is destructed, i.e.,
     * the last reference is dropped.
     *
     * A state change will be pushed into the pipe if it really
     * changed as part of taking the suspend lock.
     */
    boost::shared_ptr<StateBlocker> suspend();

    /**
     * Same as suspend(), except that it asks for an abort.
     */
    boost::shared_ptr<StateBlocker> abort();

    /** log level of the "aborting" messages */
    Logger::Level getLevel() const;
    void setLevel(Logger::Level level);

 private:
    SuspendFlags();
    ~SuspendFlags();

    Logger::Level m_level;

    /** free file descriptors, restore signal handlers */
    void deactivate();

    /** state as observed by signal handler */
    State m_state;

    /** or-ed bit mask of all received signals */
    uint32_t m_receivedSignals;

    /** time is measured inside signal handler */
    time_t m_lastSuspend;

    int m_senderFD, m_receiverFD;
    // For the sake of simplicity we only support signals in the 1-31 range.
    uint32_t m_activeSignals;
    struct sigaction m_oldSignalHandlers[32];

    boost::weak_ptr<Guard> m_guard;
    boost::weak_ptr<StateBlocker> m_suspendBlocker, m_abortBlocker;
    boost::shared_ptr<StateBlocker> block(boost::weak_ptr<StateBlocker> &blocker);
};


SE_END_CXX
#endif // INCL_SUSPENDFLAGS
