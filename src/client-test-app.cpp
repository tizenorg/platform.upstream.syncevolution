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

#include <config.h>

#include <ClientTest.h>

#include <cppunit/extensions/HelperMacros.h>
#include <exception>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#ifdef HAVE_VALGRIND_VALGRIND_H
# include <valgrind/valgrind.h>
#endif
#ifdef HAVE_EXECINFO_H
# include <execinfo.h>
#endif

#include "EvolutionSyncClient.h"
#include "EvolutionSyncSource.h"
#include "SyncEvolutionUtil.h"

/*
 * always provide this test class, even if not used:
 * that way the test scripts can unconditionally
 * invoke "client-test SyncEvolution"
 */
CPPUNIT_REGISTRY_ADD_TO_DEFAULT("SyncEvolution");

/**
 * a wrapper class which automatically does an open() in the constructor and a close() in the destructor
 * and ensures that the sync mode is "none" = testing mode
 */
class TestEvolutionSyncSource : public EvolutionSyncSource {
public:
    TestEvolutionSyncSource(const string &type,
                            const EvolutionSyncSourceParams &params) :
        EvolutionSyncSource(params)
    {
        PersistentEvolutionSyncSourceConfig config(params.m_name, params.m_nodes);
        config.setSourceType(type);
        m_source.reset(EvolutionSyncSource::createSource(params));
        CPPUNIT_ASSERT(m_source.get());
    }

    virtual SyncMLStatus beginSync(SyncMode mode) throw () {
        CPPUNIT_ASSERT_NO_THROW(m_source->open());
        CPPUNIT_ASSERT(!m_source->hasFailed());
        return m_source->beginSync(mode);
    }

    virtual SyncMLStatus endSync() throw () {
        SyncMLStatus res = m_source->endSync();
        CPPUNIT_ASSERT_NO_THROW(m_source->close());
        CPPUNIT_ASSERT(!m_source->hasFailed());
        return res;
    }

    virtual SyncItem* getFirstItem() throw () { return m_source->getFirstItem(); }
    virtual SyncItem* getNextItem() throw () { return m_source->getNextItem(); }
    virtual SyncItem* getFirstNewItem() throw () { return m_source->getFirstNewItem(); }
    virtual SyncItem* getNextNewItem() throw () { return m_source->getNextNewItem(); }
    virtual SyncItem* getFirstUpdatedItem() throw () { return m_source->getFirstUpdatedItem(); }
    virtual SyncItem* getNextUpdatedItem() throw () { return m_source->getNextUpdatedItem(); }
    virtual SyncItem* getFirstDeletedItem() throw () { return m_source->getFirstDeletedItem(); }
    virtual SyncItem* getNextDeletedItem() throw () { return m_source->getNextDeletedItem(); }

    virtual SyncMLStatus addItem(SyncItem& item) throw () { return m_source->addItem(item); }
    virtual SyncMLStatus updateItem(SyncItem& item) throw () { return m_source->updateItem(item); }
    virtual SyncMLStatus deleteItem(SyncItem& item) throw () { return m_source->deleteItem(item); }
    virtual SyncMLStatus removeAllItems() throw () { return m_source->removeAllItems(); }
    const char *getName() throw () { return m_source->getName(); }

    virtual Databases getDatabases() { return m_source->getDatabases(); }
    virtual void open() { m_source->open(); }
    virtual SyncItem *createItem(const string &uid) { return m_source->createItem(uid); }
    virtual void close() { m_source->close(); }
    virtual void exportData(ostream &out) { m_source->exportData(out); }
    virtual string fileSuffix() const { return m_source->fileSuffix(); }
    virtual const char *getMimeType() const { return m_source->getMimeType(); }
    virtual const char *getMimeVersion() const { return m_source->getMimeVersion(); }
    virtual const char* getSupportedTypes() const { return m_source->getSupportedTypes(); }
    virtual void beginSyncThrow(bool needAll,
                                bool needPartial,
                                bool deleteLocal) { m_source->beginSyncThrow(needAll, needPartial, deleteLocal); }
    virtual void endSyncThrow() { m_source->endSyncThrow(); }
    virtual SyncMLStatus addItemThrow(SyncItem& item) { return m_source->addItemThrow(item); }
    virtual SyncMLStatus updateItemThrow(SyncItem& item) { return m_source->updateItemThrow(item); }
    virtual SyncMLStatus deleteItemThrow(SyncItem& item) { return m_source->deleteItemThrow(item); }
    virtual void logItem(const string &uid, const string &info, bool debug = false) { m_source->logItem(uid, info, debug); }
    virtual void logItem(const SyncItem &item, const string &info, bool debug = false) { m_source->logItem(item, info, debug); }

    auto_ptr<EvolutionSyncSource> m_source;
};

class EvolutionLocalTests : public LocalTests {
public:
    EvolutionLocalTests(const std::string &name, ClientTest &cl, int sourceParam, ClientTest::Config &co) :
        LocalTests(name, cl, sourceParam, co)
        {}

    virtual void addTests() {
        LocalTests::addTests();

#ifdef ENABLE_MAEMO
        if (config.createSourceA &&
            config.createSourceB &&
            config.templateItem &&
            strstr(config.templateItem, "BEGIN:VCARD") &&
            config.uniqueProperties) {
            ADD_TEST(EvolutionLocalTests, testOssoDelete);
        }
#endif
    }

private:

    // insert am item,
    // overwrite it with an additional X-OSSO-CONTACT-STATE:DELETED as Maemoe address book does,
    // iterate again and check that our own code deleted the item
    void testOssoDelete() {
        // get into clean state with one template item added
        deleteAll(createSourceA);
        insert(createSourceA, config.templateItem, config.itemType);

        // add X-OSSO-CONTACT-STATE:DELETED
        string item = config.templateItem;
        const char *comma = strchr(config.uniqueProperties, ':');
        size_t offset = item.find(config.uniqueProperties, 0,
                                  comma ? comma - config.uniqueProperties : strlen(config.uniqueProperties));
        CPPUNIT_ASSERT(offset != item.npos);
        item.insert(offset, "X-OSSO-CONTACT-STATE:DELETED\n");
        update(createSourceA, item.c_str(), false);

        // opening and preparing the source should delete the item
        std::auto_ptr<SyncSource> source;
        SOURCE_ASSERT_NO_FAILURE(source.get(), source.reset(createSourceA()));
        SOURCE_ASSERT(source.get(), source->beginSync(SYNC_NONE) == 0 );
        CPPUNIT_ASSERT_EQUAL(0, countItemsOfType(source.get(), TOTAL_ITEMS));
        CPPUNIT_ASSERT_EQUAL(0, countItemsOfType(source.get(), NEW_ITEMS));
        CPPUNIT_ASSERT_EQUAL(0, countItemsOfType(source.get(), UPDATED_ITEMS));
        CPPUNIT_ASSERT_EQUAL(1, countItemsOfType(source.get(), DELETED_ITEMS));
    }
};

/**
 * This code uses the ClientTest and and information provided by
 * the backends in their RegisterSyncSourceTest instances to test
 * real synchronization with a server.
 *
 * Configuration is done by environment variables which indicate which
 * part below the root node "client-test" of the the configuration tree to use;
 * beyond that everything needed for synchronization is read from the
 * configuration tree.
 *
 * - CLIENT_TEST_SERVER = maps to name of root node in configuration tree
 * - CLIENT_TEST_EVOLUTION_PREFIX = a common "evolutionsource" prefix for *all*
 *                                  sources; the source name followed by "_[12]"
 *                                  is appended to get unique names
 * - CLIENT_TEST_EVOLUTION_USER = sets the "evolutionuser" property of all sources
 * - CLIENT_TEST_EVOLUTION_PASSWORD = sets the "evolutionpassword" property of all sources
 * - CLIENT_TEST_SOURCES = comma separated list of active sources,
 *                         names as selected in their RegisterSyncSourceTest
 *                         instances
 * - CLIENT_TEST_DELAY = number of seconds to sleep between syncs, required
 *                       by some servers
 * - CLIENT_TEST_LOG = logfile name of a server, can be empty:
 *                     if given, then the content of that file will be
 *                     copied and stored together with the client log
 *                     (only works on Unix)
 * - CLIENT_TEST_NUM_ITEMS = numbers of contacts/events/... to use during
 *                           local and sync tests which create artificial
 *                           items
 *
 * The CLIENT_TEST_SERVER also has another meaning: it is used as hint
 * by the synccompare.pl script and causes it to automatically ignore
 * known, acceptable data modifications caused by sending an item to
 * a server and back again. Currently the script recognizes "funambol",
 * "scheduleworld", "synthesis" and "egroupware" as special server
 * names.
 */
class TestEvolution : public ClientTest {
public:
    /**
     * can be instantiated as client A with id == "1" and client B with id == "2"
     */
    TestEvolution(const string &id) :
        ClientTest(getenv("CLIENT_TEST_DELAY") ? atoi(getenv("CLIENT_TEST_DELAY")) : 0,
                   getenv("CLIENT_TEST_LOG") ? getenv("CLIENT_TEST_LOG") : ""),
        m_clientID(id),
        m_configs(EvolutionSyncSource::getTestRegistry())
    {
        const char *server = getenv("CLIENT_TEST_SERVER");

        if (id == "1") {
            m_clientB.reset(new TestEvolution("2"));
        }

        /* check server */
        if (!server) {
            server = "funambol";
            setenv("CLIENT_TEST_SERVER", "funambol", 1);
        }

        /* override Evolution database names? */
        const char *evoprefix = getenv("CLIENT_TEST_EVOLUTION_PREFIX");
        m_evoPrefix = evoprefix ? evoprefix :  "SyncEvolution_Test_";
        const char *evouser = getenv("CLIENT_TEST_EVOLUTION_USER");
        if (evouser) {
            m_evoUser = evouser;
        }
        const char *evopasswd = getenv("CLIENT_TEST_EVOLUTION_PASSWORD");
        if (evopasswd) {
            m_evoPassword = evopasswd;
        }

        /* check sources */
        const char *sourcelist = getenv("CLIENT_TEST_SOURCES");
        set<string> sources;
        if (sourcelist) {
            boost::split(sources, sourcelist, boost::is_any_of(","));
        } else {
            BOOST_FOREACH(const RegisterSyncSourceTest *test, m_configs) {
                sources.insert(test->m_configName);
            }
        }

        BOOST_FOREACH(const RegisterSyncSourceTest *test, m_configs) {
            if (sources.find(test->m_configName) != sources.end()) {
                m_source2Config.push_back(test->m_configName);
            }
        }

        // get configuration and set obligatory fields
        LoggerBase::instance().setLevel(Logger::DEBUG);
        std::string root = std::string("evolution/") + server + "_" + id;
        EvolutionSyncConfig config(string(server) + "_" + id);
        if (!config.exists()) {
            // no configuration yet
            config.setDefaults();
            config.setDevID(id == "1" ? "sc-api-nat" : "sc-pim-ppc");
        }
        BOOST_FOREACH(const RegisterSyncSourceTest *test, m_configs) {
            ClientTest::Config testconfig;
            getSourceConfig(test, testconfig);
            CPPUNIT_ASSERT(testconfig.type);

            boost::shared_ptr<EvolutionSyncSourceConfig> sc = config.getSyncSourceConfig(testconfig.sourceName);
            if (!sc || !sc->exists()) {
                // no configuration yet
                config.setSourceDefaults(testconfig.sourceName);
                sc = config.getSyncSourceConfig(testconfig.sourceName);
                CPPUNIT_ASSERT(sc);
                sc->setURI(testconfig.uri);
                sc->setSourceType(testconfig.type);
            }

            // always set these properties: they might have changed since the last run
            string database = getDatabaseName(test->m_configName);
            sc->setDatabaseID(database);
            sc->setUser(m_evoUser);
            sc->setPassword(m_evoPassword);
        }
        config.flush();
    }

    virtual LocalTests *createLocalTests(const std::string &name, int sourceParam, ClientTest::Config &co) {
        return new EvolutionLocalTests(name, *this, sourceParam, co);
    }

    virtual int getNumSources() {
        return m_source2Config.size();
    }

    virtual void getSourceConfig(int source, Config &config) {
        getSourceConfig(m_configs[m_source2Config[source]], config);
    }

    static void getSourceConfig(const RegisterSyncSourceTest *test, Config &config) {
        memset(&config, 0, sizeof(config));
        ClientTest::getTestData(test->m_testCaseName.c_str(), config);
        config.createSourceA = createSource;
        config.createSourceB = createSource;
        config.sourceName = test->m_configName.c_str();

        test->updateConfig(config);
    }

    virtual ClientTest *getClientB() {
        return m_clientB.get();
    }

    virtual bool isB64Enabled() {
        return false;
    }

    virtual SyncMLStatus doSync(const int *sources,
                                const std::string &logbase,
                                const SyncOptions &options)
    {
        set<string> activeSources;
        for(int i = 0; sources[i] >= 0; i++) {
            activeSources.insert(m_source2Config[sources[i]]);
        }

        string server = getenv("CLIENT_TEST_SERVER") ? getenv("CLIENT_TEST_SERVER") : "funambol";
        server += "_";
        server += m_clientID;
        
        class ClientTest : public EvolutionSyncClient {
        public:
            ClientTest(const string &server,
                       const set<string> &activeSources,
                       const string &logbase,
                       const SyncOptions &options) :
                EvolutionSyncClient(server, false, activeSources),
                m_logbase(logbase),
                m_options(options),
                m_started(false),
                m_aborted(false)
                {}

        protected:
            virtual void prepare() {
                setLogDir(m_logbase, true);
                setMaxLogDirs(0, true);
                setLoSupport(m_options.m_loSupport, true);
                setMaxObjSize(m_options.m_maxObjSize, true);
                setMaxMsgSize(m_options.m_maxMsgSize, true);
                setWBXML(m_options.m_isWBXML, true);
                EvolutionSyncClient::prepare();
            }
            virtual void prepare(const std::vector<EvolutionSyncSource *> &sources) {
                SyncModes modes(m_options.m_syncMode);
                setSyncModes(sources, modes);
                EvolutionSyncClient::prepare(sources);
            }

            virtual void displaySyncProgress(sysync::TProgressEventEnum type,
                                             int32_t extra1, int32_t extra2, int32_t extra3)
            {
                if (!m_started) {
                    m_started = true;
                    if (m_options.m_startCallback(*this, m_options)) {
                        m_aborted = true;
                    }
                }
            }

            virtual bool checkForAbort() { return m_aborted; }

        private:
            const string m_logbase;
            SyncOptions m_options;
            bool m_started;
            bool m_aborted;
        } client(server, activeSources, logbase, options);

        SyncReport report;
        SyncMLStatus status = client.sync(&report);
        options.m_checkReport.check(status, report);
        return status;
    }
  
private:
    string m_clientID;
    std::auto_ptr<TestEvolution> m_clientB;
    const TestRegistry &m_configs;

    /** prefix, username, password to be used for local databases */
    string m_evoPrefix, m_evoUser, m_evoPassword;

    /**
     * The ClientTest framework identifies active configs with an integer.
     * This is the mapping to the corresponding config name, created when
     * constructing this instance.
     */
    vector<string> m_source2Config;

    /** returns the name of the Evolution database */
    string getDatabaseName(const string &configName) {
        return m_evoPrefix + configName + "_" + m_clientID;
    }
    
    static SyncSource *createSource(ClientTest &client, int source, bool isSourceA) {
        TestEvolution &evClient((TestEvolution &)client);
        string changeID = "SyncEvolution Change ID #";
        string name = evClient.m_source2Config[source];
        changeID += isSourceA ? "1" : "2";
        string database = evClient.getDatabaseName(name);

        EvolutionSyncConfig config("client-test-changes");
        SyncSourceNodes nodes = config.getSyncSourceNodes(name,
                                                          string("_") + ((TestEvolution &)client).m_clientID +
                                                          "_" + (isSourceA ? "A" : "B"));

        // always set this property: the name might have changes since last test run
        nodes.m_configNode->setProperty("evolutionsource", database.c_str());
        nodes.m_configNode->setProperty("evolutionuser", evClient.m_evoUser.c_str());
        nodes.m_configNode->setProperty("evolutionpassword", evClient.m_evoPassword.c_str());

        EvolutionSyncSourceParams params(name,
                                         nodes,
                                         changeID);

        const RegisterSyncSourceTest *test = evClient.m_configs[name];
        ClientTestConfig testConfig;
        getSourceConfig(test, testConfig);

        SyncSource *ss = new TestEvolutionSyncSource(testConfig.type, params);
        return ss;
    }
};

static void handler(int sig)
{
    void *buffer[100];
    int size;
    
    fprintf(stderr, "\ncaught signal %d\n", sig);
    fflush(stderr);
#ifdef HAVE_EXECINFO_H
    size = backtrace(buffer, sizeof(buffer)/sizeof(buffer[0]));
    backtrace_symbols_fd(buffer, size, 2);
#endif
#ifdef HAVE_VALGRIND_VALGRIND_H
    VALGRIND_PRINTF_BACKTRACE("\ncaught signal %d\n", sig);
#endif
    /* system("objdump -l -C -d client-test >&2"); */
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_DFL;
    sigaction(SIGABRT, &act, NULL);
    abort();
}

static class RegisterTestEvolution {
public:
    RegisterTestEvolution() :
        testClient("1") {
        struct sigaction act;

        memset(&act, 0, sizeof(act));
        act.sa_handler = handler;
        sigaction(SIGABRT, &act, NULL);
        sigaction(SIGSEGV, &act, NULL);
        sigaction(SIGILL, &act, NULL);

#if defined(HAVE_GLIB)
        // this is required when using glib directly or indirectly
        g_type_init();
        g_thread_init(NULL);
#endif
        EDSAbiWrapperInit();
        testClient.registerTests();
    }

private:
    TestEvolution testClient;
    
} testEvolution;

int RegisterSyncSourceTest::dump(ClientTest &client, SyncSource &source, const char *file)
{
    std::ofstream out(file);
    
    ((EvolutionSyncSource &)source).exportData(out);

    out.close();
    return out.bad();
}
