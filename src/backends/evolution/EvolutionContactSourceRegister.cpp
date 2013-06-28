/*
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

#include "EvolutionContactSource.h"
#include "test.h"

static EvolutionSyncSource *createSource(const EvolutionSyncSourceParams &params)
{
    SourceType sourceType = EvolutionSyncSource::getSourceType(params.m_nodes);
    bool isMe = sourceType.m_backend == "Evolution Address Book";
    bool maybeMe = sourceType.m_backend == "addressbook";
    bool enabled;

    EDSAbiWrapperInit();
    enabled = EDSAbiHaveEbook && EDSAbiHaveEdataserver;
    
    if (isMe || maybeMe) {
        if (sourceType.m_format == "" || sourceType.m_format == "text/x-vcard") {
            return
#ifdef ENABLE_EBOOK
                enabled ? new EvolutionContactSource(params, EVC_FORMAT_VCARD_21) :
#endif
                isMe ? RegisterSyncSource::InactiveSource : NULL;
        } else if (sourceType.m_format == "text/vcard") {
            return
#ifdef ENABLE_EBOOK
                enabled ? new EvolutionContactSource(params, EVC_FORMAT_VCARD_30) :
#endif
                isMe ? RegisterSyncSource::InactiveSource : NULL;
        }
    }
    return NULL;
}

static RegisterSyncSource registerMe("Evolution Address Book",
#ifdef ENABLE_EBOOK
                                     true,
#else
                                     false,
#endif
                                     createSource,
                                     "Evolution Address Book = Evolution Contacts = addressbook = contacts = evolution-contacts\n"
                                     "   vCard 2.1 (default) = text/x-vcard\n"
                                     "   vCard 3.0 = text/vcard\n"
                                     "   The later is the internal format of Evolution and preferred with\n"
                                     "   servers that support it. One such server is ScheduleWorld\n"
                                     "   together with the \"card3\" uri.\n",
                                     Values() +
                                     (Aliases("Evolution Address Book") + "Evolution Contacts" + "evolution-contacts"));

#ifdef ENABLE_EBOOK
#ifdef ENABLE_UNIT_TESTS

class EvolutionContactTest : public CppUnit::TestFixture {
    CPPUNIT_TEST_SUITE(EvolutionContactTest);
    CPPUNIT_TEST(testInstantiate);
    CPPUNIT_TEST(testImport);
    CPPUNIT_TEST_SUITE_END();

protected:
    void testInstantiate() {
        boost::shared_ptr<EvolutionSyncSource> source;
        source.reset(EvolutionSyncSource::createTestingSource("addressbook", "addressbook", true));
        source.reset(EvolutionSyncSource::createTestingSource("addressbook", "contacts", true));
        source.reset(EvolutionSyncSource::createTestingSource("addressbook", "evolution-contacts", true));
        source.reset(EvolutionSyncSource::createTestingSource("addressbook", "Evolution Contacts", true));
        source.reset(EvolutionSyncSource::createTestingSource("addressbook", "Evolution Address Book:text/x-vcard", true));
        source.reset(EvolutionSyncSource::createTestingSource("addressbook", "Evolution Address Book:text/vcard", true));
    }

    /**
     * Tests parsing of contacts as they might be send by certain servers.
     * This complements the actual testing with real servers and might cover
     * cases not occurring with servers that are actively tested against.
     */
    void testImport() {
        // this only tests that we can instantiate something under the type "addressbook";
        // it might not be an EvolutionContactSource
        boost::shared_ptr<EvolutionContactSource> source21(dynamic_cast<EvolutionContactSource *>(EvolutionSyncSource::createTestingSource("evolutioncontactsource21", "evolution-contacts:text/x-vcard", true)));
        boost::shared_ptr<EvolutionContactSource> source30(dynamic_cast<EvolutionContactSource *>(EvolutionSyncSource::createTestingSource("evolutioncontactsource30", "Evolution Address Book:text/vcard", true)));
        string parsed;

#if 0
        // TODO: enable testing of incoming items again. Right now preparse() doesn't
        // do anything and needs to be replaced with Synthesis mechanisms.

        // SF bug 1796086: sync with EGW: lost or messed up telephones
        parsed = "BEGIN:VCARD\r\nVERSION:3.0\r\nTEL;CELL:cell\r\nEND:VCARD\r\n";
        CPPUNIT_ASSERT_EQUAL(parsed,
                             preparse(*source21,
                                      "BEGIN:VCARD\nVERSION:2.1\nTEL;CELL:cell\nEND:VCARD\n",
                                      "text/x-vcard"));

        parsed = "BEGIN:VCARD\r\nVERSION:3.0\r\nTEL;TYPE=CAR:car\r\nEND:VCARD\r\n";
        CPPUNIT_ASSERT_EQUAL(parsed,
                             preparse(*source21,
                                      "BEGIN:VCARD\nVERSION:2.1\nTEL;TYPE=CAR:car\nEND:VCARD\n",
                                      "text/x-vcard"));

        parsed = "BEGIN:VCARD\r\nVERSION:3.0\r\nTEL;TYPE=HOME:home\r\nEND:VCARD\r\n";
        CPPUNIT_ASSERT_EQUAL(parsed,
                             preparse(*source21,
                                      "BEGIN:VCARD\nVERSION:2.1\nTEL:home\nEND:VCARD\n",
                                      "text/x-vcard"));

        // TYPE=PARCEL not supported by Evolution, used to represent Evolutions TYPE=OTHER
        parsed = "BEGIN:VCARD\r\nVERSION:3.0\r\nTEL;TYPE=OTHER:other\r\nEND:VCARD\r\n";
        CPPUNIT_ASSERT_EQUAL(parsed,
                             preparse(*source21,
                                      "BEGIN:VCARD\nVERSION:2.1\nTEL;TYPE=PARCEL:other\nEND:VCARD\n",
                                      "text/x-vcard"));

        parsed = "BEGIN:VCARD\r\nVERSION:3.0\r\nTEL;TYPE=HOME;TYPE=VOICE:cell\r\nEND:VCARD\r\n";
        CPPUNIT_ASSERT_EQUAL(parsed,
                             preparse(*source21,
                                      "BEGIN:VCARD\nVERSION:2.1\nTEL;TYPE=HOME,VOICE:cell\nEND:VCARD\n",
                                      "text/x-vcard"));
#endif
    }

private:
    string preparse(EvolutionContactSource &source,
                    const char *data,
                    const char *type) {
        SyncItem item;
        item.setData(data, strlen(data));
        item.setDataType(type);
        return source.preparseVCard(item);
    }
};

SYNCEVOLUTION_TEST_SUITE_REGISTRATION(EvolutionContactTest);
#endif // ENABLE_UNIT_TESTS

#ifdef ENABLE_INTEGRATION_TESTS

namespace {
#if 0
}
#endif

/**
 * We are using the vcard30 tests because they are 
 * a bit more complete than the vcard21 ones.
 * This also tests the vCard 3.0 <-> vCard 2.1
 * conversion.
 *
 * The local tests become identical with the
 * vCard 3.0 ones because they import/export the
 * same data. 
 */
static class VCard21Test : public RegisterSyncSourceTest {
public:
    VCard21Test() : RegisterSyncSourceTest("vcard21", "vcard30") {}

    virtual void updateConfig(ClientTestConfig &config) const
    {
        config.uri = "card"; // Funambol
        config.type = "evolution-contacts:text/x-vcard";
        config.testcases = "testcases/vcard30.vcf";
    }
} vCard21Test;

static class VCard30Test : public RegisterSyncSourceTest {
public:
    VCard30Test() : RegisterSyncSourceTest("vcard30", "vcard30") {}

    virtual void updateConfig(ClientTestConfig &config) const
    {
        config.type = "evolution-contacts:text/vcard";
    }
} vCard30Test;

}
#endif // ENABLE_INTEGRATION_TESTS

#endif // ENABLE_EBOOK

