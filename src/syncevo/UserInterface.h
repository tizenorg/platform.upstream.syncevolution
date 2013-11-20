/*
 * Copyright (C) 2008-2009 Patrick Ohly <patrick.ohly@gmx.de>
 * Copyright (C) 2009-12 Intel Corporation
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
#ifndef INCL_USERINTERFACE
# define INCL_USERINTERFACE

#include <string>

#include <boost/signals2.hpp>

#include <syncevo/util.h>

#include <syncevo/declarations.h>
SE_BEGIN_CXX

/**
 * This struct wraps keys for storing passwords
 * in configuration system. Some fields might be empty
 * for some passwords. Each field might have different 
 * meaning for each password. Fields using depends on
 * what user actually wants.
 */
struct ConfigPasswordKey {
 public:
    ConfigPasswordKey() : port(0) {}
    std::string toString() const;

    /** the user for the password */
    std::string user;
    /** the server for the password */
    std::string server;
    /** the domain name */
    std::string domain;
    /** the remote object */
    std::string object;
    /** the network protocol */
    std::string protocol;
    /** the authentication type */
    std::string authtype;
    /** the network port */
    unsigned int port;

    /** a description of the password, for error messages */
    std::string description;
};

/**
 * This interface has to be provided to let SyncEvolution interact
 * with the user. Possible implementations are:
 * - command line: use platform secret storage, ask for password via stdin
 * - D-Bus server: use platform secret storage, relay password requests via D-Bus to UIs
 */
class UserInterface {
 public:
    virtual ~UserInterface() {}

    /**
     * A helper function which interactively asks the user for
     * a certain password. May throw errors.
     *
     * @param passwordName the name of the password in the config file, such as 'proxyPassword'
     * @param descr        A simple string explaining what the password is needed for,
     *                     e.g. "SyncML server". This string alone has to be enough
     *                     for the user to know what the password is for, i.e. the
     *                     string has to be unique.
     * @param key          the key used to retrieve password. Using this instead of ConfigNode is
     *                     to make user interface independent on Configuration Tree
     * @return entered password
     */
    virtual std::string askPassword(const std::string &passwordName,
                                    const std::string &descr,
                                    const ConfigPasswordKey &key) = 0;

    /**
     * A helper function which interactively asks the user for
     * a certain password. May throw errors. Final result will
     * be returned via callbacks. Use this function in code which
     * is currently inside glib event processing, because askPassword()
     * might only work when it is allowed to invoke glib.
     *
     * @param passwordName the name of the password in the config file, such as 'proxyPassword'
     * @param descr        A simple string explaining what the password is needed for,
     *                     e.g. "SyncML server". This string alone has to be enough
     *                     for the user to know what the password is for, i.e. the
     *                     string has to be unique.
     * @param key          the key used to retrieve password. Using this instead of ConfigNode is
     *                     to make user interface independent on Configuration Tree
     */
    virtual void askPasswordAsync(const std::string &passwordName,
                                  const std::string &descr,
                                  const ConfigPasswordKey &key,
                                  const boost::function<void (const std::string &)> &success,
                                  const boost::function<void ()> &failureException);

    /**
     * A helper function which is used for user interface to save
     * a certain password. Currently possibly syncml server. May
     * throw errors.
     * @param passwordName the name of the password in the config file, such as 'proxyPassword'
     * @param password     password to be saved
     * @param key          the key used to store password
     * @return true if ui saves the password and false if not
     */
    virtual bool savePassword(const std::string &passwordName,
                              const std::string &password,
                              const ConfigPasswordKey &key) = 0;

    /**
     * Read from stdin until end of stream. Must be connected to
     * stdin as seen by user (different in command line client
     * and D-Bus server).
     */
    virtual void readStdin(std::string &content) = 0;
};

/**
 * Some ConfigUserInterface implementations check in the system's
 * password manager before asking the user. Backends provide optional
 * access to GNOME keyring (maps to freedesktop.org Secrets D-Bus API)
 * and KWallet (custom protocol in KDE < 4.8, same Secrets API >=
 * 4.8).
 *
 * The following signals are to be invoked by ConfigUserInterface
 * implementations which want to use these extensions. They return
 * true if some backend implemented the request, false otherwise.
 */

/**
 * call one slot after the other, return as soon as the first one
 * returns true
 */
struct TrySlots
{
    typedef bool result_type;

    template<typename InputIterator>
    bool operator()(InputIterator first, InputIterator last) const
    {
        while (first != last) {
            if (*first) {
                return true;
            }
            ++first;
        }
        return false;
    }
};

/**
 * Same as ConfigUserInterface::askPassword(), except that the
 * password is returned as retval. If it was set, then the
 * password was found in the keyring. The return value indicates
 * whether any slot matched the configured keyring.
 *
 * Backends need to be sure that the user wants them to handle
 * the request before doing the work and returning true. They
 * should check the first parameter, the value of the "keyring"
 * configuration option, to determine that.
 *
 * GNOME keyring and KWallet add themselves here and in
 * SavePasswordSignal. KWallet adds itself with priority 0
 * and GNOME keyring with 1, which means that KWallet is called
 * first. It checks whether KDE really is the preferred
 * storage, otherwise defers to GNOME keyring (or any other
 * slot) by returning false.
 */
typedef boost::signals2::signal<bool (const InitStateTri &keyring,
                                      const std::string &passwordName,
                                      const std::string &descr,
                                      const ConfigPasswordKey &key,
                                      InitStateString &password),
                                TrySlots> LoadPasswordSignal;
LoadPasswordSignal &GetLoadPasswordSignal();

static const int INTERNAL_LOAD_PASSWORD_SLOTS = 1;

/**
 * Same as AskPasswordSignal for saving.
 */
typedef boost::signals2::signal<bool (const InitStateTri &keyring,
                                      const std::string &passwordName,
                                      const std::string &password,
                                      const ConfigPasswordKey &key),
                                TrySlots> SavePasswordSignal;
SavePasswordSignal &GetSavePasswordSignal();

static const int INTERNAL_SAVE_PASSWORD_SLOTS = 2;

class SimpleUserInterface : public UserInterface
{
    InitStateTri m_useKeyring;

 public:
    SimpleUserInterface(InitStateTri useKeyring) : m_useKeyring(useKeyring) {}
    virtual std::string askPassword(const std::string &passwordName, const std::string &descr, const ConfigPasswordKey &key) {
        InitStateString password;
        // Try to use keyring, if allowed.
        GetLoadPasswordSignal()(m_useKeyring, passwordName, descr, key,  password);
        return password;
    }
    virtual bool savePassword(const std::string &passwordName, const std::string &password, const ConfigPasswordKey &key) { return false; }
    virtual void readStdin(std::string &content) { content.clear(); }
};

SE_END_CXX

#endif // INCL_USERINTERFACE
