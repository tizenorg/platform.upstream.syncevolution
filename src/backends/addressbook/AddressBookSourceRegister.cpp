/*
 * Copyright (C) 2008 Patrick Ohly
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "AddressBookSource.h"
#include "SyncEvolutionUtil.h"

static EvolutionSyncSource *createSource(const EvolutionSyncSourceParams &params)
{
    pair <string, string> sourceType = EvolutionSyncSource::getSourceType(params.m_nodes);
    bool isMe = sourceType.first == "apple-contacts";

#ifndef ENABLE_ADDRESSBOOK
    return isMe ? RegisterSyncSource::InactiveSource : NULL;
#else
    bool maybeMe = sourceType.first == "addressbook";
    
    if (isMe || maybeMe) {
        // Hack: choose default based on server URI.  "card3"
        // indicates ScheduleWorld, which works better with (requires?)
        // sending vCard 3.0.
        bool vCard3 = false;
        PersistentEvolutionSyncSourceConfig config(params.m_name, params.m_nodes);
        if (config.getURI() && !strcmp(config.getURI(), "card3")) {
            vCard3 = true;
        }

        if (sourceType.second == "text/x-vcard") {
            vCard3 = false;
        } else if (sourceType.second == "text/vcard") {
            vCard3 = true;
        }

        return new AddressBookSource(params, vCard3);
    }
    return NULL;
#endif
}

static RegisterSyncSource registerMe("iPhone/Mac OS X Address Book",
#ifdef ENABLE_ADDRESSBOOK
                                     true,
#else
                                     false,
#endif
                                     createSource,
                                     "Mac OS X or iPhone Address Book = addressbook = contacts = apple-contacts\n"
                                     "   vCard 2.1 (default) = text/x-vcard\n"
                                     "   vCard 3.0 = text/vcard\n",
                                     Values() +
                                     (Aliases("apple-contacts") + "Mac OS X Address Book" + "iPhone Address Book"));

#ifdef ENABLE_ADDRESSBOOK
#ifdef ENABLE_UNIT_TESTS

class EvolutionAddressbookTest : public CppUnit::TestFixture {
    CPPUNIT_TEST_SUITE(EvolutionAddressbookTest);
    CPPUNIT_TEST(testInstantiate);
    CPPUNIT_TEST_SUITE_END();

protected:
    void testInstantiate() {
        boost::shared_ptr<EvolutionSyncSource> source;
        source.reset(EvolutionSyncSource::createTestingSource("contacts", "contacts", true));
        source.reset(EvolutionSyncSource::createTestingSource("contacts", "addressbook", true));
        source.reset(EvolutionSyncSource::createTestingSource("contacts", "apple-contacts", true));
        source.reset(EvolutionSyncSource::createTestingSource("contacts", "Mac OS X Address Book:text/vcard", true));
        source.reset(EvolutionSyncSource::createTestingSource("contacts", "iPhone Address Book:text/x-vcard", true));
    }
};

SYNCEVOLUTION_TEST_SUITE_REGISTRATION(EvolutionAddressbookTest);

#endif // ENABLE_UNIT_TESTS

#ifdef ENABLE_INTEGRATION_TESTS
namespace {
#if 0
}
#endif

static class VCard21Test : public RegisterSyncSourceTest {
public:
    VCard21Test() : RegisterSyncSourceTest("addressbook_vcard21", "vcard21") {}

    virtual void updateConfig(ClientTestConfig &config) const
    {
        config.type = "apple-contacts:text/x-vcard";
    }
} vCard21Test;

static class VCard30Test : public RegisterSyncSourceTest {
public:
    VCard30Test() : RegisterSyncSourceTest("addressbook_vcard30", "vcard30") {}

    virtual void updateConfig(ClientTestConfig &config) const
    {
        config.type = "apple-contacts:text/vcard";
    }
} vCard30Test;

}
#endif // ENABLE_INTEGRATION_TESTS

#endif // ENABLE_ADDRESSBOOK
