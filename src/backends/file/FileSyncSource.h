/*
 * Copyright (C) 2007-2009 Patrick Ohly <patrick.ohly@gmx.de>
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

#ifndef INCL_FILESYNCSOURCE
#define INCL_FILESYNCSOURCE

#include "TrackingSyncSource.h"

#ifdef ENABLE_FILE

#include <memory>
#include <boost/noncopyable.hpp>

/**
 * Stores each SyncML item as a separate file in a directory.  The
 * directory has to be specified via the database name, using
 * [file://]<path> as format. The file:// prefix is optional, but the
 * directory is only created if it is used.
 * EvolutionSyncSource::getDatabaseID() gives us the database name.
 *
 * Change tracking is done via the file systems modification time
 * stamp: editing a file treats it as modified and then sends it to
 * the server in the next sync. Removing and adding files also works.
 *
 * The local unique identifier for each item is its name in the
 * directory. New files are created using a running count which 
 * initialized based on the initial content of the directory to
 * "highest existing number + 1" and incremented to avoid collisions.
 *
 * Although this sync source itself does not care about the content of
 * each item/file, the server needs to know what each item sent to it
 * contains and what items the source is able to receive. Therefore
 * the "type" property for this source must contain a data format
 * specified, including a version for it. Here are some examples:
 * - type=file:text/vcard:3.0
 * - type=file:text/plain:1.0
 */
class FileSyncSource : public TrackingSyncSource, private boost::noncopyable
{
  public:
    FileSyncSource(const EvolutionSyncSourceParams &params,
                   const string &dataformat);


 protected:
    /* implementation of EvolutionSyncSource interface */
    virtual void open();
    virtual void close();
    virtual Databases getDatabases();
    virtual SyncItem *createItem(const string &uid);
    virtual string fileSuffix() const;
    virtual const char *getMimeType() const;
    virtual const char *getMimeVersion() const;
    virtual const char *getSupportedTypes() const;
    virtual void logItem(const string &uid, const string &info, bool debug = false);
    virtual void logItem(const SyncItem &item, const string &info, bool debug = false);

    /* implementation of TrackingSyncSource interface */
    virtual void listAllItems(RevisionMap_t &revisions);
    virtual InsertItemResult insertItem(const string &uid, const SyncItem &item);
    virtual void deleteItem(const string &uid);

 private:
    /**
     * @name values obtained from the source's type property
     *
     * Other sync sources only support one hard-coded type and
     * don't need such variables.
     */
    /**@{*/
    string m_mimeType;
    string m_mimeVersion;
    string m_supportedTypes;
    /**@}*/

    /** directory selected via the database name in open(), reset in close() */
    string m_basedir;
    /** a counter which is used to name new files */
    long m_entryCounter;

    /**
     * get access time for file, formatted as revision string
     * @param filename    absolute path or path relative to current directory
     */
    string getATimeString(const string &filename);

    /**
     * create full filename from basedir and entry name
     */
    string createFilename(const string &entry);
};

#endif // ENABLE_FILE
#endif // INCL_FILESYNCSOURCE
