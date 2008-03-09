/*
 * Copyright (C) 2008 Patrick Ohly
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY, TITLE, NONINFRINGEMENT or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307  USA
 */
#ifndef INCL_SYNC_EVOLUTION_CONFIG
# define INCL_SYNC_EVOLUTION_CONFIG

#include "FilterConfigNode.h"

#include "spds/AbstractSyncConfig.h"
#include "spds/AbstractSyncSourceConfig.h"
#include <boost/shared_ptr.hpp>
#include <list>
#include <string>
#include <sstream>
using namespace std;

class EvolutionSyncSourceConfig;
class PersistentEvolutionSyncSourceConfig;
class ConfigTree;
struct SyncSourceNodes;

/**
 * A property has a name and a comment.  Derived classes might have
 * additional code to read and write the property from/to a
 * ConfigNode.
 *
 * @TODO add default value if the property is not set
 */
class ConfigProperty {
 public:
 ConfigProperty(const string &name, const string &comment) :
    m_name(name),
        m_comment(comment)
        {}
    
    const string m_name, m_comment;

    void setProperty(ConfigNode &node, const string &value) { node.setProperty(m_name, value, m_comment); }
    void setProperty(FilterConfigNode &node, const string &value, bool temporarily = false) {
        if (temporarily) {
            node.addFilter(m_name, value);
        } else {
            node.setProperty(m_name, value, m_comment);
        }
    }
    string getProperty(const ConfigNode &node) const { return node.readProperty(m_name); }
};

/**
 * Instead of reading and writing strings, this class interprets the content
 * as a specific type.
 */
template<class T, T def> class TypedConfigProperty : public ConfigProperty {
 public:
    TypedConfigProperty(const string &name, const string &comment, const T defValue = def) :
    ConfigProperty(name, comment),
    m_defValue(def)
        {}

    void setProperty(ConfigNode &node, const T &value) {
        ostringstream out;

        out << value;
        node.setProperty(m_name, out.str(), m_comment);
    }
    void setProperty(FilterConfigNode &node, const T &value, bool temporarily = false) {
        ostringstream out;

        out << value;
        if (temporarily) {
            node.addFilter(m_name, out.str());
        } else {
            node.setProperty(m_name, out.str(), m_comment);
        }
    }

    int getProperty(ConfigNode &node) {
        istringstream in(node.readProperty(m_name));
        T res;
        if (in >> res) {
            return res;
        } else {
            return m_defValue;
        }
    }

 private:
    T m_defValue;
};

typedef TypedConfigProperty<int, 0> IntConfigProperty;
typedef TypedConfigProperty<unsigned int, 0> UIntConfigProperty;
typedef TypedConfigProperty<long, 0> LongConfigProperty;
typedef TypedConfigProperty<unsigned long, 0> ULongConfigProperty;


/**
 * Instead of reading and writing strings, this class interprets the content
 * as boolean with T/F or 1/0 (default format).
 */
class BoolConfigProperty : public ConfigProperty {
 public:
 BoolConfigProperty(const string &name, const string &comment) :
    ConfigProperty(name, comment)
        {}

    void setProperty(ConfigNode &node, bool value) {
        node.setProperty(m_name, value ? "1" : "0", m_comment);
    }
    void setProperty(FilterConfigNode &node, bool value, bool temporarily) {
        string v(value ? "1" : "0");
        if (temporarily) {
            node.addFilter(m_name, v);
        } else {
            node.setProperty(m_name, v, m_comment);
        }
    }
    int getProperty(ConfigNode &node) {
        string res = node.readProperty(m_name);

        return !strcasecmp(res.c_str(), "T") ||
            !strcasecmp(res.c_str(), "TRUE") ||
            atoi(res.c_str()) != 0;
    }
};

/**
 * A registry for all properties which might be saved in the same ConfigNode.
 * Currently the same as a simple list. Someone else owns the instances.
 */
typedef list<const ConfigProperty *> ConfigPropertyRegistry;

/**
 * Store the current string value of a property in a cache
 * and return the "const char *" pointer that is expected by
 * the client library.
 */
class ConfigStringCache {
 public:
    const char *getProperty(const ConfigNode &node, const ConfigProperty &prop) {
        string value = prop.getProperty(node);
        pair< map<string, string>::iterator, bool > res = m_cache.insert(pair<string,string>(prop.m_name, value));
        if (!res.second) {
            res.first->second = value;
        }
        return res.first->second.c_str();
    }

 private:
    map<string, string> m_cache;
};

/**
 * This class implements the client library configuration interface
 * by mapping values to properties to entries in a ConfigTree. The
 * mapping is either the traditional one used by SyncEvolution <= 0.7
 * and client library <= 6.5 or the new layout introduced with
 * SyncEvolution >= 0.8. If for a given server name the old config
 * exists, then it is used. Otherwise the new layout is used.
 *
 * This class can be instantiated on its own and then provides access
 * to properties actually stored in files. EvolutionSyncClient
 * inherits from this class so that a derived client has the chance to
 * override every single property (although it doesn't have to).
 * Likewise EvolutionSyncSource is derived from
 * EvolutionSyncSourceConfig.
 *
 * Properties can be set permanently (this changes the underlying
 * ConfigNode) and temporarily (this modifies the FilterConfigNode
 * which wraps the ConfigNode).
 *
 * The old layout is:
 * - $HOME/.sync4j/evolution/<server>/spds/syncml/config.txt
 * -- spds/sources/<source>/config.txt
 * ---                      changes_<changeid>/config.txt
 *
 * The new layout is:
 * - ${XDG_CONFIG:-${HOME}/.config}/syncevolution/foo - base directory for server foo
 * -- config.ini - constant per-server settings
 * -- .internal.ini - read/write server properties - hidden from users because of the leading dot
 * -- sources/bar - base directory for source bar
 * --- config.ini - constant per-source settings
 * --- .internal.ini - read/write source properties
 * --- .changes_<changeid>.ini - change tracking node (content under control of sync source)
 *
 * Because this class needs to handle different file layouts it always
 * uses a FileConfigTree instance. Other implementations would be
 * possible.
 */
class EvolutionSyncConfig : public AbstractSyncConfig {
 public:
    /**
     * Opens the configuration for a specific server.
     * Will succeed even if config does not yet exist.
     */
    EvolutionSyncConfig(const string &server);

    /** true if the main configuration file already exists */
    bool exists();

    /** write changes */
    void flush();

    /**
     * A list of all properties. Can be extended by derived clients.
     */
    static ConfigPropertyRegistry &getRegistry();

    /**
     * Returns a wrapper around all properties of the given source
     * which are saved in the config tree. Note that this is different
     * from the set of sync source configs used by the SyncManager:
     * the SyncManger uses the AbstractSyncSourceConfig. In
     * SyncEvolution those are implemented by the
     * EvolutionSyncSource's actually instantiated by
     * EvolutionSyncClient. Those are complete whereas
     * PersistentEvolutionSyncSourceConfig only provides access to a
     * subset of the properties.
     */
    virtual boost::shared_ptr<PersistentEvolutionSyncSourceConfig> getSyncSourceConfig(const string &name);

    /**
     * Returns list of all configured (not active!) sync sources.
     */
    virtual list<string> getSyncSources();

    /**
     * Creates config nodes for a certain node. The nodes are not
     * yet created in the backend if they do not yet exist.
     *
     * @param name       the name of the sync source
     * @param trackName  additional part of the tracking node name (used for unit testing)
     */
    SyncSourceNodes getSyncSourceNodes(const string &name,
                                       const string &trackName = "");

    /**
     * create a new sync configuration for the current server name
     *
     * @name serverTemplate     use default settings for this server (e.g. "funambol", "scheduleworld")
     */
    void setDefaults(const string &serverTemplate);

    /**
     * create a new sync source configuration
     */
    void setSourceDefaults(const string &name);

    /**
     * @defgroup SyncEvolutionSettings
     *
     * See etc/syncml-config.txt and the property definitions in
     * SyncEvolutionConfig.cpp for the user-visible explanations of
     * these settings.
     *
     * @{
     */

    virtual const char *getLogDir() const;
    virtual void setLogDir(const string &value, bool temporarily = false);

    virtual int getMaxLogDirs() const;
    virtual void setMaxLogDirs(int value, bool temporarily = false);

    virtual int getLogLevel() const;
    virtual void setLogLevel(int value, bool temporarily = false);

    /**@}*/

    /**
     * @defgroup AbstractSyncConfig
     *
     * These settings are required by the Funambol C++ client library.
     * Some of them are hard-coded in this class. A derived class could
     * make them configurable again, should that be desired.
     *
     * @{
     */

    /**
     * @defgroup ActiveSyncSources
     *
     * This group of calls grants access to all active sources. In
     * SyncEvolution the EvolutionSyncClient class decides which
     * sources are active and thus fully configured and reimplements
     * these calls.
     *
     * @{
     */
    virtual AbstractSyncSourceConfig* getAbstractSyncSourceConfig(const char* name) const { return NULL; }
    virtual AbstractSyncSourceConfig* getAbstractSyncSourceConfig(unsigned int i) const { return NULL; }
    virtual unsigned int getAbstractSyncSourceConfigsCount() const { return 0; }
    /**@}*/

    virtual const char*  getUsername() const;
    virtual void setUsername(const string &value, bool temporarily = false);
    virtual const char*  getPassword() const;
    virtual void setPassword(const string &value, bool temporarily = false);
    virtual bool getUseProxy() const;
    virtual void setUseProxy(bool value, bool temporarily = false);
    virtual const char*  getProxyHost() const;
    virtual void setProxyHost(const string &value, bool temporarily = false);
    virtual int getProxyPort() const { return 0; }
    virtual const char* getProxyUsername() const;
    virtual void setProxyUsername(const string &value, bool temporarily = false);
    virtual const char* getProxyPassword() const;
    virtual void setProxyPassword(const string &value, bool temporarily = false);
    virtual const char*  getSyncURL() const;
    virtual void setSyncURL(const string &value, bool temporarily = false);
    virtual const char*  getClientAuthType() const;
    virtual void setClientAuthType(const string &value, bool temporarily = false);
    virtual bool getLoSupport() const;
    virtual void setLoSupport(bool value, bool temporarily = false);
    virtual unsigned long getMaxMsgSize() const;
    virtual void setMaxMsgSize(unsigned long value, bool temporarily = false);
    virtual unsigned int getMaxObjSize() const;
    virtual void setMaxObjSize(unsigned int value, bool temporarily = false);
    virtual unsigned long getReadBufferSize() const;
    virtual void setReadBufferSize(unsigned long value, bool temporarily = false);
    virtual bool  getCompression() const;
    virtual void setCompression(bool value, bool temporarily = false);
    virtual unsigned int getResponseTimeout() const;
    virtual void setResponseTimeout(unsigned int value, bool temporarily = false);
    virtual const char*  getDevID() const;
    virtual void setDevID(const string &value, bool temporarily = false);

    virtual bool getServerAuthRequired() const { return false; }
    virtual const char*  getServerAuthType() const { return ""; }
    virtual const char*  getServerPWD() const { return ""; }
    virtual const char*  getServerID() const { return ""; }

    virtual const char*  getUserAgent() const { return "SyncEvolution"; }
    virtual const char*  getVerDTD() const { return "1.1"; }
    virtual const char*  getMan() const { return "Patrick Ohly"; }
    virtual const char*  getMod() const { return "SyncEvolution"; }
    virtual const char*  getOem() const { return "Open Source"; }
    virtual const char*  getFwv() const { return ""; }
    virtual const char*  getHwv() const { return ""; }
    virtual const char*  getDsV() const { return ""; }
    virtual const char*  getSwv() const;
    virtual const char*  getDevType() const;

    virtual bool getUtc() const { return true; }
    virtual bool getNocSupport() const { return false; }

    virtual const char*  getServerNonce() const;
    virtual void setServerNonce(const char *value);
    virtual const char*  getClientNonce() const;
    virtual void setClientNonce(const char *value);
    virtual const char*  getDevInfHash() const;
    virtual void setDevInfHash(const char *value);

    /**@}*/

private:
    string m_server;
    bool m_oldLayout;

    /** holds all config nodes relative to the root that we found */
    boost::shared_ptr<ConfigTree> m_tree;

    /** access to global sync properties */
    boost::shared_ptr<FilterConfigNode> m_configNode;
    boost::shared_ptr<ConfigNode> m_hiddenNode;

    /** temporary overrides for source settings */
    FilterConfigNode::ConfigFilter_t m_sourceFilter;

    mutable ConfigStringCache m_stringCache;
};

/**
 * This set of config nodes is to be used by EvolutionSyncSourceConfig
 * to accesss properties.
 */
struct SyncSourceNodes {
    /**
     * @param configNode    node for user-visible properties
     * @param hiddenNode    node for internal properties (may be the same as
     *                      configNode in old config layouts!)
     * @param trackingNode  node for tracking changes (always different than the
     *                      other two nodes)
     */
    SyncSourceNodes(const boost::shared_ptr<FilterConfigNode> &configNode,
                    const boost::shared_ptr<ConfigNode> &hiddenNode,
                    const boost::shared_ptr<ConfigNode> &trackingNode) : 
    m_configNode(configNode),
        m_hiddenNode(hiddenNode),
        m_trackingNode(trackingNode)
    {}

    const boost::shared_ptr<FilterConfigNode> m_configNode;
    const boost::shared_ptr<ConfigNode> m_hiddenNode;
    const boost::shared_ptr<ConfigNode> m_trackingNode;
};
    


/**
 * This class maps per-source properties to ConfigNode properties.
 * Some properties are not configurable and have to be provided
 * by derived classes.
 */
class EvolutionSyncSourceConfig : public AbstractSyncSourceConfig {
 public:
    EvolutionSyncSourceConfig(const string &name, const SyncSourceNodes &nodes);

    static ConfigPropertyRegistry &getRegistry();

    /**
     * @defgroup EvolutionSyncSourceConfigExtensions
     *
     * @{
     */
    virtual const char *getUser() const;
    virtual void setUser(const string &value, bool temporarily = false);

    const char *getPassword() const;
    virtual void setPassword(const string &value, bool temporarily = false);

    virtual const char *getDatabaseID() const;
    virtual void setDatabaseID(const string &value, bool temporarily = false);
    /**@}*/

    /**
     * Returns the data source type configured as part of the given
     * configuration; different EvolutionSyncSources then check whether
     * they support that type. This call has to work before instantiating
     * a source and thus gets nodes explicitly.
     */
    static string getSourceType(const SyncSourceNodes &nodes);
    virtual const char *getSourceType() const;
    virtual void setSourceType(const string &value, bool temporarily = false);


    /**@}*/

    /**
     * @defgroup AbstractSyncSourceConfigAPI_not_yet_implemented
     *
     * These calls have to be implemented by EvolutionSyncSource
     * instances. Some sources support more than one type. The
     * configuration then selects the preferred format in
     * the getSourceType() string.
     * 
     * @{
     */

    /**
     * Returns the preferred mime type of the items handled by the sync source.
     * Example: "text/x-vcard"
     */
    virtual const char *getMimeType() const = 0;

    /**
     * Returns the version of the mime type used by client.
     * Example: "2.1"
     */
    virtual const char *getMimeVersion() const = 0;

    /**
     * A string representing the source types (with versions) supported by the SyncSource.
     * The string must be formatted as a sequence of "type:version" separated by commas ','.
     * For example: "text/x-vcard:2.1,text/vcard:3.0".
     * The version can be left empty, for example: "text/x-s4j-sifc:".
     * Supported types will be sent as part of the DevInf.
     */
    virtual const char* getSupportedTypes() const = 0;

    /**@}*/

    /**
     * @defgroup AbstractSyncSourceConfigAPI_implemented
     * @{
     */
    virtual const char *getType() const { return getMimeType(); }
    virtual const char *getVersion() const { return getMimeVersion(); }
    virtual const char*  getName() const { return m_name.c_str(); }
    /**@}*/

    /**
     * Returns the SyncSource URI: used in SyncML to address the data
     * on the server.
     *
     * Each URI has to be unique during a sync session, i.e.
     * two different sync sources cannot access the same data at
     * the same time.
     */
    virtual const char*  getURI() const;
    virtual void setURI(const string &value, bool temporarily = false);

    /**
     * Returns a comma separated list of the possible syncModes for the
     * SyncSource. Sync modes can be one of
     * - slow
     * - two-way
     * - one-way-from-server
     * - one-way-from-client
     * - refresh-from-server
     * - refresh-from-client
     * - one-way-from-server
     * - one-way-from-client
     * - addrchange (Funambol extension)
     *
     * This is hard-coded in SyncEvolution because changing it
     * wouldn't have any effect (IMHO).
     */
    virtual const char*  getSyncModes() const { return "slow,two-way,one-way-from-server,one-way-from-client,refresh-from-server,refresh-from-client"; }

    /**
     * Gets the default syncMode as one of the strings listed in setSyncModes.
     */
    virtual const char*  getSync() const;
    virtual void setSync(const string &value, bool temporarily = false);
    
    /**
     * Specifies how the content of an outgoing item should be
     * encoded by the client library if the sync source does not
     * set an encoding on the item that it created. Valid values
     * are listed in SyncItem::encodings.
     */
    virtual const char*  getEncoding() const;
    virtual void setEncoding(const string &value, bool temporarily = false);

    /**
     * Sets the last sync timestamp. Called by the sync engine at
     * the end of a sync. The client must save that modified
     * value; it is needed to decide during the next sync whether
     * an incremental sync is possible.
     *
     * SyncEvolution will reset this value when a SyncSource fails
     * and thus force a slow sync during the next sync.
     *
     * @param timestamp the last sync timestamp
     */
    virtual unsigned long getLast() const;
    virtual void setLast(unsigned long timestamp);

    /**
     * "des" enables an encryption mode which only the Funambol server
     * understands. Not configurable in SyncEvolution unless a derived
     * SyncSource decides otherwise.
     */
    virtual const char* getEncryption() const { return ""; }

    /**
     * Returns an array of CtCap with all the capabilities for this
     * source.  The capabilities specify which parts of e.g. a vCard
     * the sync source supports. Not specifying this in detail by
     * returning an empty array implies that it supports all aspects.
     * This is the default implementation of this call.
     *
     * @return an ArrayList of CTCap
     */
    virtual const ArrayList& getCtCaps() const { static const ArrayList dummy; return dummy; }

    /**@}*/

 private:
    string m_name;
    SyncSourceNodes m_nodes;
    mutable ConfigStringCache m_stringCache;
};

/**
 * Adds dummy implementations of the missing calls to
 * EvolutionSyncSourceConfig so that the other properties can be read.
 */
class PersistentEvolutionSyncSourceConfig : public EvolutionSyncSourceConfig {
 public:
    PersistentEvolutionSyncSourceConfig(const string &name, const SyncSourceNodes &nodes) :
    EvolutionSyncSourceConfig(name, nodes) {}

    virtual const char* getMimeType() const { return ""; }
    virtual const char* getMimeVersion() const { return ""; }
    virtual const char* getSupportedTypes() const { return ""; }
};

#endif
