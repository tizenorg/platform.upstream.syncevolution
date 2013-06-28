/*
 * Copyright (C) 2008 Funambol, Inc.
 * Copyright (C) 2008-2009 Patrick Ohly <patrick.ohly@gmx.de>
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

#ifndef INCL_TESTSYNCCLIENT
#define INCL_TESTSYNCCLIENT

#include <string>
#include <vector>
#include <list>

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include <SyncML.h>
#include <TransportAgent.h>
#include <SyncSource.h>

#include "test.h"

#ifdef ENABLE_INTEGRATION_TESTS

#include <cppunit/TestSuite.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>

#include <syncevo/declarations.h>
SE_BEGIN_CXX

class SyncContext;
class EvolutionSyncSource;
class TransportWrapper;
class TestingSyncSource;

/**
 * This class encapsulates logging and checking of a SyncReport.
 * When constructed with default parameters, no checking will be done.
 * Otherwise the sync report has to contain exactly the expected result.
 * When multiple sync sources are active, @b all of them have to behave
 * alike (which is how the tests are constructed).
 *
 * No item is ever supposed to fail.
 */
class CheckSyncReport {
  public:
    CheckSyncReport(int clAdded = -1, int clUpdated = -1, int clDeleted = -1,
                    int srAdded = -1, int srUpdated = -1, int srDeleted = -1,
                    bool mstSucceed = true, SyncMode mode = SYNC_NONE) :
        clientAdded(clAdded),
        clientUpdated(clUpdated),
        clientDeleted(clDeleted),
        serverAdded(srAdded),
        serverUpdated(srUpdated),
        serverDeleted(srDeleted),
        mustSucceed(mstSucceed),
        syncMode(mode)
        {}

    virtual ~CheckSyncReport() {}

    int clientAdded, clientUpdated, clientDeleted,
        serverAdded, serverUpdated, serverDeleted;
    bool mustSucceed;
    SyncMode syncMode;

    /**
     * checks that the sync completed as expected and throws
     * CPPUnit exceptions if something is wrong
     *
     * @param res     return code from SyncClient::sync()
     * @param report  the sync report stored in the SyncClient
     */
    virtual void check(SyncMLStatus status, SyncReport &report) const;
};

/**
 * parameters for running a sync
 */
struct SyncOptions {
    /** default maximum message size */
    static const long DEFAULT_MAX_MSG_SIZE = 128 * 1024;
    /** default maximum object size */
    static const long DEFAULT_MAX_OBJ_SIZE = 1024 * 1024 * 1024;
    /** sync mode chosen by client */
    SyncMode m_syncMode;
    /**
     * has to be called after a successful or unsuccessful sync,
     * will dump the report and (optionally) check the result;
     * beware, the later may throw exceptions inside CPPUNIT macros
     */
    CheckSyncReport m_checkReport;

    /** maximum message size supported by client */
    long m_maxMsgSize;
    /** maximum object size supported by client */
    long m_maxObjSize;
    /** enabled large object support */
    bool m_loSupport;
    /** enabled WBXML (default) */
    bool m_isWBXML;

    bool m_isSuspended; 
    
    bool m_isAborted;

    typedef boost::function<bool (SyncContext &,
                                  SyncOptions &)> Callback_t;
    /**
     * Callback to be invoked after setting up local sources, but
     * before running the engine. May throw exception to indicate
     * error and return true to stop sync without error.
     */
    Callback_t m_startCallback;

    boost::shared_ptr<TransportAgent> m_transport;

    SyncOptions(SyncMode syncMode = SYNC_NONE,
                const CheckSyncReport &checkReport = CheckSyncReport(),
                long maxMsgSize = DEFAULT_MAX_MSG_SIZE, // 128KB = large enough that normal tests should run with a minimal number of messages
                long maxObjSize = DEFAULT_MAX_OBJ_SIZE, // 1GB = basically unlimited...
                bool loSupport = false,
                bool isWBXML = defaultWBXML(),
                Callback_t startCallback = EmptyCallback,
                boost::shared_ptr<TransportAgent> transport =
                boost::shared_ptr<TransportAgent>()) :
        m_syncMode(syncMode),
        m_checkReport(checkReport),
        m_maxMsgSize(maxMsgSize),
        m_maxObjSize(maxObjSize),
        m_loSupport(loSupport),
        m_isWBXML(isWBXML),
        m_isSuspended(false),
        m_isAborted(false),
        m_startCallback(startCallback),
        m_transport (transport)
    {}

    SyncOptions &setSyncMode(SyncMode syncMode) { m_syncMode = syncMode; return *this; }
    SyncOptions &setCheckReport(const CheckSyncReport &checkReport) { m_checkReport = checkReport; return *this; }
    SyncOptions &setMaxMsgSize(long maxMsgSize) { m_maxMsgSize = maxMsgSize; return *this; }
    SyncOptions &setMaxObjSize(long maxObjSize) { m_maxObjSize = maxObjSize; return *this; }
    SyncOptions &setLOSupport(bool loSupport) { m_loSupport = loSupport; return *this; }
    SyncOptions &setWBXML(bool isWBXML) { m_isWBXML = isWBXML; return *this; }
    SyncOptions &setStartCallback(const Callback_t &callback) { m_startCallback = callback; return *this; }
    SyncOptions &setTransportAgent(const boost::shared_ptr<TransportAgent> transport)
                                  {m_transport = transport; return *this;}

    static bool EmptyCallback(SyncContext &,
                              SyncOptions &) { return false; }

    /** if CLIENT_TEST_XML=1, then XML, otherwise WBXML */
    static bool defaultWBXML();
};

class LocalTests;
class SyncTests;

/**
 * This is the interface expected by the testing framework for sync
 * clients.  It defines several methods that a derived class must
 * implement if it wants to use that framework. Note that this class
 * itself is not derived from SyncClient. This gives a user of this
 * framework the freedom to implement it in two different ways:
 * - implement a class derived from both SyncClient and ClientTest
 * - add testing of an existing subclass of SyncClient by implementing
 *   a ClientTest which uses that subclass
 *
 * The client is expected to support change tracking for multiple
 * servers. Although the framework always always tests against the
 * same server, for most tests it is necessary to access the database
 * without affecting the next synchronization with the server. This is
 * done by asking the client for two different sync sources via
 * Config::createSourceA and Config::createSourceB which have to
 * create them in a suitable way - pretty much as if the client was
 * synchronized against different server. A third, different change
 * tracking is needed for real synchronizations of the data.
 *
 * Furthermore the client is expected to support multiple data sources
 * of the same kind, f.i. two different address books. This is used to
 * test full client A <-> server <-> client B synchronizations in some
 * tests or to check server modifications done by client A with a
 * synchronization against client B. In those tests client A is mapped
 * to the first data source and client B to the second one.
 *
 * Handling configuration and creating classes is entirely done by the
 * subclass of ClientTest, the frameworks makes no assumptions
 * about how this is done. Instead it queries the ClientTest for
 * properties (like available sync sources) and then creates several
 * tests.
 */
class ClientTest {
  public:
    ClientTest(int serverSleepSec = 0, const std::string &serverLog= "");
    virtual ~ClientTest();

    /**
     * This function registers tests using this instance of ClientTest for
     * later use during a test run.
     *
     * The instance must remain valid until after the tests were
     * run. To run them use a separate test runner, like the one from
     * client-test-main.cpp.
     */
    virtual void registerTests();

    typedef ClientTestConfig Config;

    /**
     * Creates an instance of LocalTests (default implementation) or a
     * class derived from it.  LocalTests provides tests which cover
     * the SyncSource interface and can be executed without a SyncML
     * server. It also contains utility functions for working with
     * SyncSources.
     *
     * A ClientTest implementation can, but doesn't have to extend
     * these tests by instantiating a derived class here.
     */
    virtual LocalTests *createLocalTests(const std::string &name, int sourceParam, ClientTest::Config &co);

    /**
     * Creates an instance of SyncTests (default) or a class derived
     * from it.  SyncTests provides tests which cover the actual
     * interaction with a SyncML server.
     *
     * A ClientTest implementation can, but doesn't have to extend
     * these tests by instantiating a derived class here.
     */
    virtual SyncTests *createSyncTests(const std::string &name, std::vector<int> sourceIndices, bool isClientA = true);

    /**
     * utility function for dumping items which are C strings with blank lines as separator
     */
    static int dump(ClientTest &client, TestingSyncSource &source, const char *file);

    /**
     * utility function for splitting file into items with blank lines as separator
     *
     * @retval realfile       If <file>.<server>.tem exists, then it is used instead
     *                        of the generic version. The caller gets the name of the
     *                        file that was opened here.
     */
    static void getItems(const char *file, std::list<std::string> &items, std::string &realfile);

    /**
     * utility function for importing items with blank lines as separator
     */
    static int import(ClientTest &client, TestingSyncSource &source, const char *file, std::string &realfile);

    /**
     * utility function for comparing vCard and iCal files with the external
     * synccompare.pl Perl script
     */
    static bool compare(ClientTest &client, const char *fileA, const char *fileB);

    struct ClientTestConfig config;

    /**
     * A derived class can use this call to get default test
     * cases, but still has to add callbacks which create sources
     * and execute a sync session.
     *
     * Some of the test cases are compiled into the library, other
     * depend on the auxiliary files from the "test" directory.
     * Currently supported types:
     * - vcard30 = vCard 3.0 contacts
     * - vcard21 = vCard 2.1 contacts
     * - ical20 = iCal 2.0 events
     * - vcal10 = vCal 1.0 events
     * - itodo20 = iCal 2.0 tasks
     */
    static void getTestData(const char *type, Config &config);

    /**
     * Data sources are enumbered from 0 to n-1 for the purpose of
     * testing. This call returns n.
     */
    virtual int getNumLocalSources() = 0;
    virtual int getNumSyncSources() = 0;

    /**
     * Called to fill the given test source config with information
     * about a sync source identified by its index. It's okay to only
     * fill in the available pieces of information and set everything
     * else to zero.
     * Two kinds of source config indexs are maintained, used for localSources
     * and SyncSources, this is because virtual datasoures should be visible as
     * a whole to the synccontext while should be viewed as a list of sub
     * datasoures for Localtests.
     */
    virtual void getLocalSourceConfig(int source, Config &config) = 0;
    virtual void getSyncSourceConfig(int source, Config &config) = 0;

    /**
     * Find the correspoding test source config via config name.
     */
    virtual void getSourceConfig(const string &configName, Config &config) =0;
    /*
     * Give me a test source config name, return the index in localSyncSources.
     * */
    virtual int getLocalSourcePosition (const string &configName) =0;

    /**
     * The instance to use as second client. Returning NULL disables
     * all checks which require a second client. The returned pointer
     * must remain valid throughout the life time of the tests.
     *
     * The second client must be configured to access the same server
     * and have data sources which match the ones from the primary
     * client.
     */
    virtual ClientTest *getClientB() = 0;

    /**
     * Execute a synchronization with the selected sync sources
     * and the selected synchronization options. The log file
     * in LOG has been set up already for the synchronization run
     * and should not be changed by the client.
     *
     * @param activeSources a -1 terminated array of sync source indices
     * @param logbase      basename for logging: can be used for directory or as file (by adding .log suffix)
     * @param options      sync options to be used
     * @return return code of SyncClient::sync()
     */
    virtual SyncMLStatus doSync(
        const int *activeSources,
        const std::string &logbase,
        const SyncOptions &options) = 0;


    /**
     * This is called after successful sync() calls (res == 0) as well
     * as after unsuccessful ones (res != 1). The default implementation
     * sleeps for the number of seconds specified when constructing this
     * instance and copies the server log if one was named.
     *
     * @param res       result of sync()
     * @param logname   base name of the current sync log (without ".client.[AB].log" suffix)
     */
    virtual void postSync(int res, const std::string &logname);

  protected:
    /**
     * time to sleep in postSync()
     */
    int serverSleepSeconds;

    /**
     * server log file which is copied by postSync() and then
     * truncated (Unix only, Windows does not allow such access
     * to an open file)
     */
    std::string serverLogFileName;

  private:
    /**
     * really a CppUnit::TestFactory, but declared as void * to avoid
     * dependencies on the CPPUnit header files: created by
     * registerTests() and remains valid until the client is deleted
     */
    void *factory;
};

/**
 * helper class to encapsulate ClientTest::Config::createsource_t
 * pointer and the corresponding parameters
 */
class CreateSource {
public:
    CreateSource(ClientTest::Config::createsource_t createSourceParam, ClientTest &clientParam, int sourceParam, bool isSourceAParam) :
        createSource(createSourceParam),
        client(clientParam),
        source(sourceParam),
        isSourceA(isSourceAParam) {}

    TestingSyncSource *operator() () {
        CPPUNIT_ASSERT(createSource);
        return createSource(client, source, isSourceA);
    }

    const ClientTest::Config::createsource_t createSource;
    ClientTest &client;
    const int source;
    const bool isSourceA;
};


/**
 * local test of one sync source and utility functions also used by
 * sync tests
 */
class LocalTests : public CppUnit::TestSuite, public CppUnit::TestFixture {
public:
    /** the client we are testing */
    ClientTest &client;

    /** number of the source we are testing in that client */
    const int source;

    /** configuration that corresponds to source */
    const ClientTest::Config config;

    /** helper funclets to create sources */
    CreateSource createSourceA, createSourceB;

    LocalTests(const std::string &name, ClientTest &cl, int sourceParam, ClientTest::Config &co) :
        CppUnit::TestSuite(name),
        client(cl),
        source(sourceParam),
        config(co),
        createSourceA(co.createSourceA, cl, sourceParam, true),
        createSourceB(co.createSourceB, cl, sourceParam, false)
        {}

    /**
     * adds the supported tests to the instance itself;
     * this is the function that a derived class can override
     * to add additional tests
     */
    virtual void addTests();

    /**
     * opens source and inserts the given item; can be called
     * regardless whether the data source already contains items or not
     *
     * @param relaxed   if true, then disable some of the additional checks after adding the item
     * @return the LUID of the inserted item
     */
    virtual std::string insert(CreateSource createSource, const char *data, bool relaxed = false);

    /**
     * assumes that exactly one element is currently inserted and updates it with the given item
     *
     * @param check     if true, then reopen the source and verify that the reported items are as expected
     */
    virtual void update(CreateSource createSource, const char *data, bool check = true);

    /**
     * updates one item identified by its LUID with the given item
     *
     * The type of the item is cleared, as in insert() above.
     */
    virtual void update(CreateSource createSource, const char *data, const std::string &luid);

    /** deletes all items locally via sync source */
    virtual void deleteAll(CreateSource createSource);

    /**
     * takes two databases, exports them,
     * then compares them using synccompare
     *
     * @param refFile      existing file with source reference items, NULL uses a dump of sync source A instead
     * @param copy         a sync source which contains the copied items, begin/endSync will be called
     * @param raiseAssert  raise assertion if comparison yields differences (defaults to true)
     * @return true if the two databases are equal
     */
    virtual bool compareDatabases(const char *refFile, TestingSyncSource &copy, bool raiseAssert = true);

    /**
     * insert artificial items, number of them determined by TEST_EVOLUTION_NUM_ITEMS
     * unless passed explicitly
     *
     * @param createSource    a factory for the sync source that is to be used
     * @param startIndex      IDs are generated starting with this value
     * @param numItems        number of items to be inserted if non-null, otherwise TEST_EVOLUTION_NUM_ITEMS is used
     * @param size            minimum size for new items
     * @return number of items inserted
     */
    virtual std::list<std::string> insertManyItems(CreateSource createSource, int startIndex = 1, int numItems = 0, int size = -1);

    /**
     * create an artificial item for the current database
     *
     * @param item      item number: items with different number should be
     *                  recognized as different by SyncML servers
     * @param revision  differentiates items with the same item number (= updates of an older item)
     * @param size      if > 0, then create items at least that large (in bytes)
     * @return created item
     */
    std::string createItem(int item, const std::string &revision, int size);
    std::string createItem(int item, int revision, int size) {
        char buffer[32];
        sprintf(buffer, "%d", revision);
        return createItem(item, std::string(buffer), size);
    }

    /* for more information on the different tests see their implementation */

    virtual void testOpen();
    virtual void testIterateTwice();
    virtual void testSimpleInsert();
    virtual void testLocalDeleteAll();
    virtual void testComplexInsert();
    virtual void testLocalUpdate();
    virtual void testChanges();
    virtual void testImport();
    virtual void testImportDelete();
    virtual void testManyChanges();
    virtual void testLinkedItemsParent();
    virtual void testLinkedItemsChild();
    virtual void testLinkedItemsParentChild();
    virtual void testLinkedItemsChildParent();
    virtual void testLinkedItemsChildChangesParent();
    virtual void testLinkedItemsRemoveParentFirst();
    virtual void testLinkedItemsRemoveNormal();
    virtual void testLinkedItemsInsertParentTwice();
    virtual void testLinkedItemsInsertChildTwice();
    virtual void testLinkedItemsParentUpdate();
    virtual void testLinkedItemsUpdateChild();
    virtual void testLinkedItemsInsertBothUpdateChild();
    virtual void testLinkedItemsInsertBothUpdateParent();

};

int countItemsOfType(TestingSyncSource *source, int state);
std::list<std::string> listItemsOfType(TestingSyncSource *source, int state);

/**
 * Tests synchronization with one or more sync sources enabled.
 * When testing multiple sources at once only the first config
 * is checked to see which tests can be executed.
 */
class SyncTests : public CppUnit::TestSuite, public CppUnit::TestFixture {
public:
    /** the client we are testing */
    ClientTest &client;

    SyncTests(const std::string &name, ClientTest &cl, std::vector<int> sourceIndices, bool isClientA = true);
    ~SyncTests();

    /** adds the supported tests to the instance itself */
    virtual void addTests();

protected:
    /** list with all local test classes for manipulating the sources and their index in the client */
    typedef std::vector< std::pair<int, LocalTests *> > source_array_t;
    source_array_t sources;
    typedef source_array_t::iterator source_it;

    /**
     * Stack of log file prefixes which are to be appended to the base name,
     * which already contains the current test name. Add a new prefix by
     * instantiating SyncPrefix. Its destructor takes care of popping
     * the prefix.
     */
    std::list<std::string> logPrefixes;

    class SyncPrefix {
        SyncTests &m_tests;
    public:
        SyncPrefix(const std::string &prefix, SyncTests &tests) :
        m_tests(tests) {
            tests.logPrefixes.push_back(prefix);
        }
        ~SyncPrefix() {
            m_tests.logPrefixes.pop_back();
        }
    };
    friend class SyncPrefix;

    /** the indices from sources, terminated by -1 (for sync()) */
    int *sourceArray;

    /** utility functions for second client */
    SyncTests *accessClientB;

    enum DeleteAllMode {
        DELETE_ALL_SYNC,   /**< make sure client and server are in sync,
                              delete locally,
                              sync again */
        DELETE_ALL_REFRESH /**< delete locally, refresh server */
    };

    /**
     * Compare databases second client with either reference file(s)
     * or first client.  The reference file(s) must follow the naming
     * scheme <reFileBase><source name>.dat
     */
    virtual bool compareDatabases(const char *refFileBase = NULL,
                                  bool raiseAssert = true);

    /** deletes all items locally and on server */
    virtual void deleteAll(DeleteAllMode mode = DELETE_ALL_SYNC);

    /** get both clients in sync with empty server, then copy one item from client A to B */
    virtual void doCopy();

    /**
     * replicate server database locally: same as SYNC_REFRESH_FROM_SERVER,
     * but done with explicit local delete and then a SYNC_SLOW because some
     * servers do no support SYNC_REFRESH_FROM_SERVER
     */
    virtual void refreshClient(SyncOptions options = SyncOptions());

    /* for more information on the different tests see their implementation */

    // do a two-way sync without additional checks,
    // may or may not actually be done in two-way mode
    virtual void testTwoWaySync() {
        doSync(SyncOptions(SYNC_TWO_WAY));
    }

    // do a slow sync without additional checks
    virtual void testSlowSync() {
        doSync(SyncOptions(SYNC_SLOW,
                           CheckSyncReport(-1,-1,-1, -1,-1,-1, true, SYNC_SLOW)));
    }
    // do a refresh from server sync without additional checks
    virtual void testRefreshFromServerSync() {
        doSync(SyncOptions(SYNC_REFRESH_FROM_SERVER,
                           CheckSyncReport(-1,-1,-1, -1,-1,-1, true, SYNC_REFRESH_FROM_SERVER)));
    }

    // do a refresh from client sync without additional checks
    virtual void testRefreshFromClientSync() {
        doSync(SyncOptions(SYNC_REFRESH_FROM_CLIENT,
                           CheckSyncReport(-1,-1,-1, -1,-1,-1, true, SYNC_REFRESH_FROM_CLIENT)));
    }

    // delete all items, locally and on server using two-way sync
    virtual void testDeleteAllSync() {
        deleteAll(DELETE_ALL_SYNC);
    }

    virtual void testDeleteAllRefresh();
    virtual void testRefreshFromClientSemantic();
    virtual void testRefreshFromServerSemantic();
    virtual void testRefreshStatus();

    // test that a two-way sync copies an item from one address book into the other
    void testCopy() {
        doCopy();
        compareDatabases();
    }

    virtual void testUpdate();
    virtual void testComplexUpdate();
    virtual void testDelete();
    virtual void testMerge();
    virtual void testTwinning();
    virtual void testOneWayFromServer();
    virtual void testOneWayFromClient();
    bool doConversionCallback(bool *success,
                              SyncContext &client,
                              SyncOptions &options);
    virtual void testConversion();
    virtual void testItems();
    virtual void testItemsXML();
    virtual void testAddUpdate();

    // test copying with maxMsg and no large object support
    void testMaxMsg() {
        doVarSizes(true, false);
    }
    // test copying with maxMsg and large object support
    void testLargeObject() {
        doVarSizes(true, true);
    }

    virtual void testManyItems();
    virtual void testManyDeletes();
    virtual void testSlowSyncSemantic();
    virtual void testComplexRefreshFromServerSemantic();

    virtual void doInterruptResume(int changes,
                  boost::shared_ptr<TransportWrapper> wrapper); 
    enum {
        CLIENT_ADD = (1<<0),
        CLIENT_REMOVE = (1<<1),
        CLIENT_UPDATE = (1<<2),
        SERVER_ADD = (1<<3),
        SERVER_REMOVE = (1<<4),
        SERVER_UPDATE = (1<<5)
    };
    virtual void testInterruptResumeClientAdd();
    virtual void testInterruptResumeClientRemove();
    virtual void testInterruptResumeClientUpdate();
    virtual void testInterruptResumeServerAdd();
    virtual void testInterruptResumeServerRemove();
    virtual void testInterruptResumeServerUpdate();
    virtual void testInterruptResumeFull();

    virtual void testUserSuspendClientAdd();
    virtual void testUserSuspendClientRemove();
    virtual void testUserSuspendClientUpdate();
    virtual void testUserSuspendServerAdd();
    virtual void testUserSuspendServerRemove();
    virtual void testUserSuspendServerUpdate();
    virtual void testUserSuspendFull();

    virtual void testResendClientAdd();
    virtual void testResendClientRemove();
    virtual void testResendClientUpdate();
    virtual void testResendServerAdd();
    virtual void testResendServerRemove();
    virtual void testResendServerUpdate();
    virtual void testResendFull();

    /**
     * implements testMaxMsg(), testLargeObject(), testLargeObjectEncoded()
     * using a sequence of items with varying sizes
     */
    virtual void doVarSizes(bool withMaxMsgSize,
                            bool withLargeObject);

    /**
     * executes a sync with the given options,
     * checks the result and (optionally) the sync report
     */
    virtual void doSync(const SyncOptions &options);
    virtual void doSync(const char *logPrefix,
                        const SyncOptions &options) {
        SyncPrefix prefix(logPrefix, *this);
        doSync(options);
    }
};

/*
 * A transport wraper wraps a real transport impl and gives user 
 * possibility to do additional work before/after transport operation.
 * We use TransportFaultInjector to emulate a network failure;
 * We use UserSuspendInjector to emulate a user suspend after receving
 * a response.
 */
class TransportWrapper : public TransportAgent {
protected:
    int m_interruptAtMessage, m_messageCount;
    boost::shared_ptr<TransportAgent> m_wrappedAgent;
    Status m_status;
    SyncOptions *m_options;
public:
    TransportWrapper() {
        m_messageCount = 0;
        m_interruptAtMessage = -1;
        m_wrappedAgent = boost::shared_ptr<TransportAgent>();
        m_status = INACTIVE;
        m_options = NULL;
    }
    ~TransportWrapper() {
    }

    virtual int getMessageCount() { return m_messageCount; }

    virtual void setURL(const std::string &url) { m_wrappedAgent->setURL(url); }
    virtual void setContentType(const std::string &type) { m_wrappedAgent->setContentType(type); }
    virtual void setAgent(boost::shared_ptr<TransportAgent> agent) {m_wrappedAgent = agent;}
    virtual void setSyncOptions(SyncOptions *options) {m_options = options;}
    virtual void setInterruptAtMessage (int interrupt) {m_interruptAtMessage = interrupt;}
    virtual void cancel() { m_wrappedAgent->cancel(); }
    virtual void shutdown() { m_wrappedAgent->shutdown(); }

    virtual void rewind() {
        m_messageCount = 0;
        m_interruptAtMessage = -1;
        m_status = INACTIVE;
        m_options = NULL;
        m_wrappedAgent.reset();
    }
    virtual Status wait(bool noReply = false) { return m_status; }
    virtual void setCallback (TransportCallback cb, void *udata, int interval) 
    { return m_wrappedAgent->setCallback(cb, udata, interval);}
};

/** assert equality, include string in message if unequal */
#define CLIENT_TEST_EQUAL( _prefix, \
                           _expected, \
                           _actual ) \
    CPPUNIT_ASSERT_EQUAL_MESSAGE( std::string(_prefix) + ": " + #_expected + " == " + #_actual, \
                                  _expected, \
                                  _actual )

/** execute _x and then check the status of the _source pointer */
#define SOURCE_ASSERT_NO_FAILURE(_source, _x) \
{ \
    CPPUNIT_ASSERT_NO_THROW(_x); \
    CPPUNIT_ASSERT((_source)); \
}

/** check _x for true and then the status of the _source pointer */
#define SOURCE_ASSERT(_source, _x) \
{ \
    CPPUNIT_ASSERT(_x); \
    CPPUNIT_ASSERT((_source)); \
}

/** check that _x evaluates to a specific value and then the status of the _source pointer */
#define SOURCE_ASSERT_EQUAL(_source, _value, _x) \
{ \
    CPPUNIT_ASSERT_EQUAL(_value, _x); \
    CPPUNIT_ASSERT((_source)); \
}

/** same as SOURCE_ASSERT() with a specific failure message */
#define SOURCE_ASSERT_MESSAGE(_message, _source, _x)     \
{ \
    CPPUNIT_ASSERT_MESSAGE((_message), (_x)); \
    CPPUNIT_ASSERT((_source)); \
}

/**
 * convenience macro for adding a test name like a function,
 * to be used inside addTests() of an instance of that class
 *
 * @param _class      class which contains the function
 * @param _function   a function without parameters in that class
 */
#define ADD_TEST(_class, _function) \
    ADD_TEST_TO_SUITE(this, _class, _function)

#define ADD_TEST_TO_SUITE(_suite, _class, _function) \
    _suite->addTest(FilterTest(new CppUnit::TestCaller<_class>(_suite->getName() + "::" #_function, &_class::_function, *this)))

SE_END_CXX

#endif // ENABLE_INTEGRATION_TESTS
#endif // INCL_TESTSYNCCLIENT
