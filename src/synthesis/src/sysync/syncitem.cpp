/*
 *  File:         SyncItem.cpp
 *
 *  Author:			  Lukas Zeller (luz@synthesis.ch)
 *
 *  TSyncItem
 *    Abstract Base class of all data containing items.
 *    TSyncItems always correspond to a TSyncItemType
 *    which holds type information and conversion
 *    features.
 *
 *  Copyright (c) 2001-2009 by Synthesis AG (www.synthesis.ch)
 *
 *  2001-06-18 : luz : created
 *
 */

// includes
#include "prefix_file.h"

#include "sysync.h"
#include "syncitem.h"
#include "syncappbase.h"



using namespace sysync;


// equality mode names
const char * const sysync::compareRelevanceNames[numEQmodes] = {
  "never",    // irrelevant, only for fields with really unimportant data (such as REV) - note that this also prevents inclusion in CRC change detection
  "scripted", // relevant, but actual comparison is done in a script - standard compare loop must not check it
  "conflict", // for conflict, all fields that have user data should use at least this
  "slowsync", // for slow sync, all fields that must match for identifying records in slow sync
  "always",   // always relevant, fields that must always match (first-time sync match set)
  "n/a"
};

/*
 * Implementation of TSyncItem
 */

/* public TSyncItem members */


TSyncItem::TSyncItem(TSyncItemType *aItemType)
{
  fSyncOp=sop_none;
  fSyncItemTypeP=aItemType;
} // TSyncItem::TSyncItem


TSyncItem::~TSyncItem()
{
  // NOP
} // TSyncItem::~TSyncItem


// assignment (IDs and contents)
TSyncItem& TSyncItem::operator=(TSyncItem &aSyncItem)
{
  // - IDs
  setRemoteID(aSyncItem.getRemoteID());
  setLocalID(aSyncItem.getLocalID());
  // - syncop
  setSyncOp(aSyncItem.getSyncOp());
  // - Contents
  replaceDataFrom(aSyncItem);
  // done
  return *this;
} // TSyncItem::operator=


// get session zones pointer
GZones *TSyncItem::getSessionZones(void)
{
  return getSession() ? getSession()->getSessionZones() : NULL;
} // TSyncItem::getSessionZones



#ifdef SYDEBUG
TDebugLogger *TSyncItem::getDbgLogger(void)
{
  // commands log to session's logger
  return fSyncItemTypeP ? fSyncItemTypeP->getDbgLogger() : NULL;
} // TSyncItem::getDbgLogger

uInt32 TSyncItem::getDbgMask(void)
{
  if (!fSyncItemTypeP) return 0; // no session, no debug
  return fSyncItemTypeP->getDbgMask();
} // TSyncItem::getDbgMask
#endif


TSyncAppBase *TSyncItem::getSyncAppBase(void)
{
	return getSession() ? getSession()->getSyncAppBase() : NULL;	
} // TSyncItem::getSyncAppBase


/* end of TSyncItem implementation */

// eof
