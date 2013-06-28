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

#include "edsf-view.h"
#include <syncevo/BoostHelper.h>

SE_GOBJECT_TYPE(EdsfPersona);

SE_BEGIN_CXX

EDSFView::EDSFView(const ESourceRegistryCXX &registry,
                   const std::string &uuid,
                   const std::string &query) :
    m_registry(registry),
    m_uuid(uuid),
    m_query(query)
{
}

void EDSFView::init(const boost::shared_ptr<EDSFView> &self)
{
    m_self = self;
}

boost::shared_ptr<EDSFView> EDSFView::create(const ESourceRegistryCXX &registry,
                                             const std::string &uuid,
                                             const std::string &query)
{
    boost::shared_ptr<EDSFView> view(new EDSFView(registry, uuid, query));
    view->init(view);
    return view;
}

void EDSFView::doStart()
{
    ESourceCXX source(e_source_registry_ref_source(m_registry, m_uuid.c_str()), false);
    if (!source) {
        SE_LOG_DEBUG(NULL, NULL, "edsf %s: address book not found", m_uuid.c_str());
        return;
    }
    m_store = EdsfPersonaStoreCXX::steal(edsf_persona_store_new_with_source_registry(m_registry, source));
    GErrorCXX gerror;
    m_ebook = EBookClientCXX::steal(
#ifdef HAVE_E_BOOK_CLIENT_NEW_DIRECT
                                    getenv("SYNCEVOLUTION_NO_PIM_EDS_DIRECT") ?
                                    e_book_client_new(source, gerror) :
                                    e_book_client_new_direct(m_registry, source, gerror)
#elif defined(HAVE_E_BOOK_CLIENT_CONNECT_DIRECT_SYNC)
                                    getenv("SYNCEVOLUTION_NO_PIM_EDS_DIRECT") ?
                                    e_book_client_new(source, gerror) :
                                    E_BOOK_CLIENT(e_book_client_connect_direct_sync(m_registry, source, NULL, gerror))
#else
                                    e_book_client_new(source, gerror)
#endif
                                    );
    if (!m_ebook) {
        SE_LOG_DEBUG(NULL, NULL, "edfs %s: no client for address book: %s", m_uuid.c_str(), gerror->message);
        return;
    }
    SYNCEVO_GLIB_CALL_ASYNC(e_client_open,
                            boost::bind(&EDSFView::opened,
                                        m_self,
                                        _1,
                                        _2),
                            E_CLIENT(m_ebook.get()),
                            false,
                            NULL);
}

void EDSFView::opened(gboolean success, const GError *gerror) throw()
{
    try {
        if (!success) {
            SE_LOG_DEBUG(NULL, NULL, "edfs %s: opening failed: %s", m_uuid.c_str(), gerror->message);
            return;
        }
        SYNCEVO_GLIB_CALL_ASYNC(e_book_client_get_contacts,
                                boost::bind(&EDSFView::read,
                                            m_self,
                                            _1,
                                            _2,
                                            _3),
                                m_ebook.get(),
                                m_query.c_str(),
                                NULL);
    } catch (...) {
        Exception::handle(HANDLE_EXCEPTION_NO_ERROR);
    }
}

void EDSFView::read(gboolean success, GSList *contactslist, const GError *gerror) throw()
{
    try {
        GListCXX<EContact, GSList, GObjectDestructor> contacts(contactslist);
        if (!success) {
            SE_LOG_DEBUG(NULL, NULL, "edfs %s: reading failed: %s", m_uuid.c_str(), gerror->message);
            return;
        }

        BOOST_FOREACH (EContact *contact, contacts) {
            EdsfPersonaCXX persona(edsf_persona_new(m_store, contact), false);
            GeeHashSetCXX personas(gee_hash_set_new(G_TYPE_OBJECT, g_object_ref, g_object_unref, NULL, NULL, NULL, NULL, NULL, NULL), false);
            gee_collection_add(GEE_COLLECTION(personas.get()), persona.get());
            FolksIndividualCXX individual(folks_individual_new(GEE_SET(personas.get())), false);
            m_addedSignal(individual);
        }
        m_isQuiescent = true;
        m_quiescenceSignal();
    } catch (...) {
        Exception::handle(HANDLE_EXCEPTION_NO_ERROR);
    }
}

SE_END_CXX
