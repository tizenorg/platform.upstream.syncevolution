/*
 * Copyright (C) 2013 Intel Corporation
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

#include "signon.h"

#include <syncevo/IdentityProvider.h>

#include <syncevo/declarations.h>
SE_BEGIN_CXX

static class gSSOProvider : public IdentityProvider
{
public:
    gSSOProvider() :
        // This uses "gsso" at the moment. The advantage of that is that if
        // gSSO and UOA were installed in parallel, the user could choose which
        // one to use. If it turns out that the two will never be installed at the
        // same time, then this perhaps should be "signon" instead, which then would
        // pick either a gSSO or UAO backend depending on which is available.
        IdentityProvider("gsso",
                         "gsso:<numeric account ID>[,<service name>]\n"
                         "   Authentication using libgsignond + libaccounts,\n"
                         "   using an account created and managed with libaccounts.\n"
                         "   The service name is optional. If not given, the\n"
                         "   settings from the account will be used.")
    {}

    virtual boost::shared_ptr<AuthProvider> create(const InitStateString &username,
                                                   const InitStateString &password)
    {
        // Returning NULL if not enabled...
        boost::shared_ptr<AuthProvider> provider;
#ifdef USE_GSSO
        provider = createGSSOAuthProvider(username, password);
#endif
        return provider;
    }
} gsso;

SE_END_CXX
