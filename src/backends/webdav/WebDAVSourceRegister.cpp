/*
 * Copyright (C) 2010 Intel Corporation
 */

#include "CalDAVSource.h"
#include "test.h"

#include <syncevo/declarations.h>
SE_BEGIN_CXX

static SyncSource *createSource(const SyncSourceParams &params)
{
    SourceType sourceType = SyncSource::getSourceType(params.m_nodes);
    bool isMe;

    isMe = sourceType.m_backend == "CalDAV";
    if (isMe) {
        if (sourceType.m_format == "" ||
            sourceType.m_format == "text/calendar" ||
            sourceType.m_format == "text/x-calendar" ||
            sourceType.m_format == "text/x-vcalendar") {
#ifdef ENABLE_DAV
            return new CalDAVSource(params);
#else
            return RegisterSyncSource::InactiveSource;
#endif
        }
    }

#if 0
    isMe = sourceType.m_backend == "CardDAV";
    if (isMe) {
        if (sourceType.m_format == "" ||
            sourceType.m_format == "text/x-vcard" ||
            sourceType.m_format == "text/vcard") {
#ifdef ENABLE_DAV
            return new CardDAVSource(params);
#else
            return RegisterSyncSource::InactiveSource;
#endif
        }
    }
#endif

    return NULL;
}

static RegisterSyncSource registerMe("DAV",
#ifdef ENABLE_DAV
                                     true,
#else
                                     false,
#endif
                                     createSource,
                                     "CalDAV\n"
                                     "   iCalendar 2.0 (default) = text/calendar\n"
                                     "   vCalendar 1.0 = text/x-vcalendar\n"
#if 0
                                     "CardDAV\n"
                                     "   vCard 2.1 (default) = text/x-vcard\n"
                                     "   vCard 3.0 = text/vcard\n"
#endif
                                     ,
                                     Values() +
                                     Aliases("CalDAV")
#if 0
                                     + Aliases("CardDAV")
#endif
                                     );

#ifdef ENABLE_DAV
#ifdef ENABLE_UNIT_TESTS

class WebDAVTest : public CppUnit::TestFixture {
    CPPUNIT_TEST_SUITE(WebDAVTest);
    CPPUNIT_TEST(testInstantiate);
    CPPUNIT_TEST_SUITE_END();

protected:
    void testInstantiate() {
        boost::shared_ptr<TestingSyncSource> source;
        source.reset((TestingSyncSource *)SyncSource::createTestingSource("CalDAV", "CalDAV", true));
        source.reset((TestingSyncSource *)SyncSource::createTestingSource("CalDAV", "CalDAV:text/calendar", true));
        source.reset((TestingSyncSource *)SyncSource::createTestingSource("CalDAV", "CalDAV:text/x-vcalendar", true));
    }
};

SYNCEVOLUTION_TEST_SUITE_REGISTRATION(WebDAVTest);

#endif // ENABLE_UNIT_TESTS

namespace {
#if 0
}
#endif

static class CalDAVTest : public RegisterSyncSourceTest {
public:
    CalDAVTest() : RegisterSyncSourceTest("ical20", "ical20") {}

    virtual void updateConfig(ClientTestConfig &config) const
    {
        config.type = "CalDAV";
    }
} CalDAVTest;

}

#endif // ENABLE_DAV

SE_END_CXX