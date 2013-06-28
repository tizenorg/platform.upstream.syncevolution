/*
 * Copyright (C) 2008-2009 Patrick Ohly <patrick.ohly@gmx.de>
 * Copyright (C) 2010 Intel Corporation
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

#include "KCalExtendedSource.h"
#include "test.h"

#include <syncevo/declarations.h>
SE_BEGIN_CXX

static SyncSource *createSource(const SyncSourceParams &params)
{
    SourceType sourceType = SyncSource::getSourceType(params.m_nodes);
    bool isMe = sourceType.m_backend == "mkcal";
    bool maybeMe = sourceType.m_backend == "calendar";

    if (isMe || maybeMe) {
        if (sourceType.m_format == "" ||
            sourceType.m_format == "text/x-vcalendar" ||
            sourceType.m_format == "text/x-calendar" ||
            sourceType.m_format == "text/calendar") {
            return
#ifdef ENABLE_KCALEXTENDED
                true ? new KCalExtendedSource(params) :
#endif
                isMe ? RegisterSyncSource::InactiveSource : NULL;
        }
    }
    return NULL;
}

static RegisterSyncSource registerMe("KCalExtended",
#ifdef ENABLE_KCALEXTENDED
                                     true,
#else
                                     false,
#endif
                                     createSource,
                                     "mkcal = KCalExtended = calendar\n"
                                     "   iCalendar 2.0 = text/calendar\n",
                                     Values() +
                                     (Aliases("mkcal") + "KCalExtended" + "MeeGo Calendar"));

#ifdef ENABLE_KCALEXTENDED
#ifdef ENABLE_UNIT_TESTS

class KCalExtendedSourceUnitTest : public CppUnit::TestFixture {
    CPPUNIT_TEST_SUITE(KCalExtendedSourceUnitTest);
    CPPUNIT_TEST(testInstantiate);
    CPPUNIT_TEST_SUITE_END();

protected:
    void testInstantiate() {
        boost::shared_ptr<SyncSource> source;
        source.reset(SyncSource::createTestingSource("KCalExtended", "KCalExtended:text/calendar:2.0", true));
    }
};

SYNCEVOLUTION_TEST_SUITE_REGISTRATION(KCalExtendedSourceUnitTest);

#endif // ENABLE_UNIT_TESTS

namespace {
#if 0
}
#endif

static class ICal20Test : public RegisterSyncSourceTest {
public:
    ICal20Test() : RegisterSyncSourceTest("kcal_ical20", "ical20") {}

    virtual void updateConfig(ClientTestConfig &config) const
    {
        config.type = "KCalExtended:text/calendar";
    }
} iCal20Test;

}

#endif // ENABLE_KCALEXTENDED

SE_END_CXX
