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

#ifndef INCL_EVOLUTIONSYNCSOURCE
#define INCL_EVOLUTIONSYNCSOURCE

#include <syncevo/TrackingSyncSource.h>
#include <syncevo/eds_abi_wrapper.h>

#include <syncevo/declarations.h>
SE_BEGIN_CXX


/**
 * The base class for all Evolution backends.
 * Same as TrackingSyncSource plus some Evolution
 * specific helper methods.
 */
class EvolutionSyncSource : public TrackingSyncSource
{
 public:
    /**
     * Creates a new Evolution sync source.
     */
    EvolutionSyncSource(const SyncSourceParams &params,
                        int granularitySeconds = 1) :
        TrackingSyncSource(params,
                           granularitySeconds)
        {
        }

  protected:
#ifdef HAVE_EDS
    /**
     * searches the list for a source with the given uri or name
     *
     * @param list      a list previously obtained from Gnome
     * @param id        a string identifying the data source: either its name or uri
     * @return   pointer to source or NULL if not found
     */
    ESource *findSource( ESourceList *list, const string &id );
#endif

 public:
#ifdef HAVE_EDS
    using SyncSourceBase::throwError;

    /**
     * throw an exception after an operation failed and
     * remember that this instance has failed
     *
     * output format: <source name>: <action>: <error string>
     *
     * @param action     a string describing the operation or object involved
     * @param gerror     if not NULL: a more detailed description of the failure,
     *                                will be freed
     */
    void throwError(const string &action,
                    GError *gerror);
#endif
};


SE_END_CXX
#endif // INCL_EVOLUTIONSYNCSOURCE
