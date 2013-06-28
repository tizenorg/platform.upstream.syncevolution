/**
 *  @File     customimpl.cpp
 *
 *  @Author   Lukas Zeller (luz@synthesis.ch)
 *
 *  @brief TCustomImplDS
 *    Base class for customizable datastores (mainly extended DB mapping features
 *    common to all derived classes like ODBC, DBAPI etc.).
 *
 *    Copyright (c) 2001-2009 by Synthesis AG (www.synthesis.ch)
 *
 *  @Date 2005-12-05 : luz : separated from odbcapids
 */


// includes
#include "sysync.h"
#include "multifielditem.h"
#include "mimediritemtype.h"
#include "customimplds.h"
#include "customimplagent.h"


namespace sysync {

#ifndef BASED_ON_BINFILE_CLIENT
#ifdef SYDEBUG
const char * const MapEntryTypeNames[numMapEntryTypes] = {
  "invalid",
  "normal",
  "tempidmap",
  "pendingmap"
};
#endif
#endif // not BASED_ON_BINFILE_CLIENTs


#ifdef SCRIPT_SUPPORT

class TCustomDSfuncs {
public:

  // Custom Impl datastore specific script functions
  // ===============================================

  // string FOLDERKEY()
  // returns folder key
  static void func_FolderKey(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsString(
      static_cast<TCustomImplDS *>(aFuncContextP->getCallerContext())->fFolderKey.c_str()
    );
  }; // func_FolderKey


  // string TARGETKEY()
  // returns target key
  static void func_TargetKey(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsString(
      static_cast<TCustomImplDS *>(aFuncContextP->getCallerContext())->fTargetKey.c_str()
    );
  }; // func_TargetKey


  // integer ARRAYINDEX()
  // returns current array index when reading or writing an array
  // in the finish function it denotes the number of array items totally
  static void func_ArrayIndex(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsInteger(
      static_cast<TCustomImplDS *>(aFuncContextP->getCallerContext())->fArrIdx
    );
  }; // func_ArrayIndex


  // string PARENTKEY()
  // returns key of (array) parent object (like %k)
  static void func_ParentKey(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsString(
      static_cast<TCustomImplDS *>(aFuncContextP->getCallerContext())->fParentKey.c_str()
    );
  }; // func_ParentKey


  // integer WRITING()
  // returns true if script is called while writing to DB
  static void func_Writing(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsBoolean(
      static_cast<TCustomImplDS *>(aFuncContextP->getCallerContext())->fWriting
    );
  }; // func_Writing


  // integer DELETING()
  // returns true if script is called while deleting in DB
  static void func_Deleting(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsBoolean(
      static_cast<TCustomImplDS *>(aFuncContextP->getCallerContext())->fDeleting
    );
  }; // func_Deleting


  // integer INSERTING()
  // returns true if script is called while inserting new data to DB
  static void func_Inserting(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    aTermP->setAsBoolean(
      static_cast<TCustomImplDS *>(aFuncContextP->getCallerContext())->fInserting
    );
  }; // func_Inserting


  // string LOGSUBST(string logtext)
  // returns log placeholders substituted in logtext
  static void func_LogSubst(TItemField *&aTermP, TScriptContext *aFuncContextP)
  {
    string logtext;
    aFuncContextP->getLocalVar(0)->getAsString(logtext); // log text
    // perform substitutions
    static_cast<TCustomImplDS *>(aFuncContextP->getCallerContext())->DoLogSubstitutions(logtext,false);
    // return it
    aTermP->setAsString(logtext);
  }; // func_LogSubst

}; // TCustomDSfuncs

const uInt8 param_LogSubst[] = { VAL(fty_string) };

// builtin function table for datastore level
const TBuiltInFuncDef CustomDSFuncDefs[] = {
  { "FOLDERKEY",  TCustomDSfuncs::func_FolderKey, fty_string, 0, NULL },
  { "TARGETKEY",  TCustomDSfuncs::func_TargetKey, fty_string, 0, NULL },
  { "ARRAYINDEX", TCustomDSfuncs::func_ArrayIndex, fty_integer, 0, NULL },
  { "PARENTKEY", TCustomDSfuncs::func_ParentKey, fty_string, 0, NULL },
  { "WRITING", TCustomDSfuncs::func_Writing, fty_integer, 0, NULL },
  { "INSERTING", TCustomDSfuncs::func_Inserting, fty_integer, 0, NULL },
  { "DELETING", TCustomDSfuncs::func_Deleting, fty_integer, 0, NULL },
  { "LOGSUBST", TCustomDSfuncs::func_LogSubst, fty_string, 1, param_LogSubst }
};


// chain to generic local engine datastore funcs
static void *CustomDSChainFunc2(void *&aCtx)
{
  // context pointer for datastore-level funcs is the datastore
  // -> no change needed
  // next table is localEngineDS's
  return (void *)&DBFuncTable;
} // CustomDSChainFunc2

const TFuncTable CustomDSFuncTable2 = {
  sizeof(CustomDSFuncDefs) / sizeof(TBuiltInFuncDef), // size of table
  CustomDSFuncDefs, // table pointer
  CustomDSChainFunc2 // chain generic DB functions
};



#endif



// Config
// ======

TCustomDSConfig::TCustomDSConfig(const char* aName, TConfigElement *aParentElement) :
  inherited(aName,aParentElement),
  fFieldMappings("mappings",this)
  #ifdef SCRIPT_SUPPORT
  ,fResolveContextP(NULL)
  ,fDSScriptsResolved(false)
  #endif
{
  // nop so far
  clear();
} // TCustomDSConfig::TCustomDSConfig


TCustomDSConfig::~TCustomDSConfig()
{
  // clear
  clear();
} // TCustomDSConfig::~TCustomDSConfig


// init defaults
void TCustomDSConfig::clear(void)
{
  // init defaults
  // - multi-folder support
  fMultiFolderDB=false;
  // Data table options
  // - charset to be used in the data table
  fDataCharSet=chs_ansi; // suitable default for ODBC
  // - line end mode to be used in the data table for multiline data
  fDataLineEndMode=lem_dos; // default to CRLF, as this seems to be safest assumption
  // - if set, causes that data is read from DB first and then merged
  //   with updated fields. Not needed in normal SQL DBs, as they can
  //   update a subset of all columns. However, might still be needed
  //   for special cases like Achil that needs record size calculation
  //   or sortfeldXXX generation.
  fUpdateAllFields=false;
  // - Date/Time info
  fDataIsUTC=false; // compatibility flag only, will set fDataTimeZone to TCTX_UTC at Resolve if set
  fDataTimeZone=TCTX_SYSTEM; // default to local system time
  fUserZoneOutput=true; // by default, non-floating timestamps are moved to user zone after reading from DB. Only if zone context for timestamp fields is really retrieved from the DB on a per record level, this can be switched off
  #ifndef BASED_ON_BINFILE_CLIENT
  // - flag indicating that admin tables have DS 1.2 support (map entrytype, map flags, fResumeAlertCode, fLastSuspend, fLastSuspendIdentifier
  fResumeSupport=false;
  fResumeItemSupport=false; // no item resume as well
  // - admin capability info
  fSyncTimeStampAtEnd=false; // if set, time point of sync is taken AFTER last write to DB (for single-user DBs like FMPro). Note that target table layout is different in this case!
  fOneWayFromRemoteSupported=false; // compatible with old layout of target tables, no support
  #endif // not BASED_ON_BINFILE_CLIENT
  fStoreSyncIdentifiers=false; // compatible with old layout of target tables, no support
  // clear embedded
  fFieldMappings.clear();
  #ifdef SCRIPT_SUPPORT
  // - script called after admin data is loaded (before any data access takes place)
  fAdminReadyScript.erase();
  // - script called at end of sync session
  fSyncEndScript.erase();
  // - clear script resolve context
  if (fResolveContextP) {
    delete fResolveContextP;
    fResolveContextP=NULL;
  }
  fDSScriptsResolved=false;
  #endif
  // clear inherited
  inherited::clear();
} // TCustomDSConfig::clear


// config element parsing
bool TCustomDSConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // multi-folder-support
  if (strucmp(aElementName,"multifolder")==0)
    expectBool(fMultiFolderDB);
  // user data related properties
  else if (strucmp(aElementName,"datacharset")==0)
    expectEnum(sizeof(fDataCharSet),&fDataCharSet,DBCharSetNames,numCharSets);
  else if (strucmp(aElementName,"datalineends")==0)
    expectEnum(sizeof(fDataLineEndMode),&fDataLineEndMode,lineEndModeNames,numLineEndModes);
  else if (strucmp(aElementName,"updateallfields")==0)
    expectBool(fUpdateAllFields);
  // - Date/Time info
  else if (strucmp(aElementName,"timeutc")==0) {
    // - warn for usage of old timeutc
    ReportError(false,"Warning: <timeutc> is deprecated - please use <datatimezone> instead",aElementName);
    expectBool(fDataIsUTC);
  }
  else if (strucmp(aElementName,"datatimezone")==0)
    expectTimezone(fDataTimeZone);
  else if (strucmp(aElementName,"userzoneoutput")==0)
    expectBool(fUserZoneOutput);
  #ifndef BASED_ON_BINFILE_CLIENT
  // - admin capability info
  else if (strucmp(aElementName,"synctimestampatend")==0)
    expectBool(fSyncTimeStampAtEnd);
  else if (strucmp(aElementName,"fromremoteonlysupport")==0)
    expectBool(fOneWayFromRemoteSupported);
  else if (strucmp(aElementName,"resumesupport")==0)
    expectBool(fResumeSupport);
  else if (strucmp(aElementName,"resumeitemsupport")==0)
    expectBool(fResumeItemSupport);
  #endif
  else if (
    strucmp(aElementName,"storelastsyncidentifier")==0 ||
    strucmp(aElementName,"storesyncidentifiers")==0
  )
    expectBool(fStoreSyncIdentifiers);
  #ifdef SCRIPT_SUPPORT
  else if (strucmp(aElementName,"adminreadyscript")==0)
    expectScript(fAdminReadyScript, aLine, getDSFuncTableP());
  else if (strucmp(aElementName,"syncendscript")==0)
    expectScript(fSyncEndScript, aLine, getDSFuncTableP());
  #endif
  // - field mappings
  else if (strucmp(aElementName,"fieldmap")==0) {
    // check reference argument
    const char* ref = getAttr(aAttributes,"fieldlist");
    if (!ref) {
      ReportError(true,"fieldmap missing 'fieldlist' attribute");
    }
    else {
      // look for field list
      TMultiFieldDatatypesConfig *mfcfgP =
        dynamic_cast<TMultiFieldDatatypesConfig *>(getSyncAppBase()->getRootConfig()->fDatatypesConfigP);
      if (!mfcfgP) SYSYNC_THROW(TConfigParseException("no multifield config"));
      TFieldListConfig *cfgP = mfcfgP->getFieldList(ref);
      if (!cfgP)
        return fail("fieldlist '%s' not defined for fieldmap",ref);
      // - store field list reference in map
      fFieldMappings.fFieldListP=cfgP;
      // - let element handle parsing
      expectChildParsing(fFieldMappings);
    }
  }
  // - none known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TCustomDSConfig::localStartElement


// resolve
void TCustomDSConfig::localResolve(bool aLastPass)
{
  // convert legacy UTC flag to timezone setting
  if (fDataIsUTC)
    fDataTimeZone = TCTX_UTC;
  // scripts
  #ifdef SCRIPT_SUPPORT
  if (aLastPass) {
    // resolve map scripts now in case they haven't been resolved already
    ResolveDSScripts();
  }
  #endif
  // resolve children
  fFieldMappings.Resolve(aLastPass);
  #ifdef SCRIPT_SUPPORT
  // Now everything is resolved, we can forget the resolve context
  if (aLastPass && fResolveContextP) {
    delete fResolveContextP;
    fResolveContextP = NULL;
  }
  #endif
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TCustomDSConfig::localResolve


#ifdef SCRIPT_SUPPORT

// resolve DS related scripts, but make sure we do that only once
// Note: MAKE SURE that order of resolving is same as rebuilding order!
void TCustomDSConfig::ResolveDSScripts(void)
{
  // resolve
  if (!fDSScriptsResolved) {
    // resolve eventual API level scripts first
    apiResolveScripts();
    // resolve start and end scripts first
    TScriptContext::resolveScript(getSyncAppBase(),fAdminReadyScript,fResolveContextP,NULL);
    TScriptContext::resolveScript(getSyncAppBase(),fSyncEndScript,fResolveContextP,NULL);
    // - option filter generator script
    TScriptContext::resolveScript(getSyncAppBase(),fFieldMappings.fOptionFilterScript,fResolveContextP,fFieldMappings.fFieldListP);
    // - map scripts
    TScriptContext::resolveScript(getSyncAppBase(),fFieldMappings.fInitScript,fResolveContextP,fFieldMappings.fFieldListP);
    TScriptContext::resolveScript(getSyncAppBase(),fFieldMappings.fAfterReadScript,fResolveContextP,fFieldMappings.fFieldListP);
    TScriptContext::resolveScript(getSyncAppBase(),fFieldMappings.fBeforeWriteScript,fResolveContextP,fFieldMappings.fFieldListP);
    TScriptContext::resolveScript(getSyncAppBase(),fFieldMappings.fAfterWriteScript,fResolveContextP,fFieldMappings.fFieldListP);
    TScriptContext::resolveScript(getSyncAppBase(),fFieldMappings.fFinishScript,fResolveContextP,fFieldMappings.fFieldListP);
    TScriptContext::resolveScript(getSyncAppBase(),fFieldMappings.fFinalisationScript,fResolveContextP,fFieldMappings.fFieldListP);
    fDSScriptsResolved=true;
  }
} // TCustomDSConfig::ResolveDSScripts

#endif


// transfer size limits from map to type
static void transferMapOptionsToType(TFieldMapList &aFml, TMultiFieldItemType *aItemTypeP, sInt16 aMaxRepeat, sInt16 aRepInc)
{
  TFieldMapList::iterator pos;
  TFieldMapItem *fmiP;

  for (pos=aFml.begin(); pos!=aFml.end(); pos++) {
    fmiP = *pos;
    #ifdef ARRAYDBTABLES_SUPPORT
    if (fmiP->isArray()) {
      // is array, recurse into fields of array
      TFieldMapArrayItem *fmaiP = static_cast<TFieldMapArrayItem *>(fmiP);
      transferMapOptionsToType(
        fmaiP->fArrayFieldMapList,
        aItemTypeP,
        fmaiP->fMaxRepeat,
        fmaiP->fRepeatInc
      );
    }
    else
    #endif
    if (fmiP->fid>=0) {
      // normal map, apply to all related fields
      if (aMaxRepeat==0) aMaxRepeat=1; // unlimited repeat is only applied to first field
      #ifdef ARRAYFIELD_SUPPORT
      TFieldDefinition *fdP=aItemTypeP->getFieldDefinition(fmiP->fid);
      if (fdP && fdP->array) {
        // this is an array field, disable fid offsetting
        aMaxRepeat=1;
      }
      #endif
      for (sInt16 k=0; k<aMaxRepeat; k++) {
        TFieldOptions *optP = aItemTypeP->getFieldOptions(fmiP->fid+k*aRepInc);
        if (!optP) break;
        if (
          optP->maxsize==FIELD_OPT_MAXSIZE_NONE || // no size defined yet
          optP->maxsize==FIELD_OPT_MAXSIZE_UNKNOWN || // or defined as unknown
          (optP->maxsize>sInt32(fmiP->maxsize) && // or defined size is larger than that one set in the mapping...
          fmiP->maxsize!=0) // ..but map size is not unlimited
        ) {
          // set new max size
          optP->maxsize = fmiP->maxsize;
        }
        if (fmiP->notruncate)
          optP->notruncate=true;
      }
    }
  }
} // transferMapOptionsToType


// Add (probably datastore-specific) limits such as MaxSize and NoTruncate to types
void TCustomDSConfig::addTypeLimits(TLocalEngineDS *aLocalDatastoreP, TSyncSession *aSessionP)
{
  // add field size limitations from map to all types
  TSyncItemTypePContainer::iterator pos;
  TSyncItemTypePContainer *typesP = &(aLocalDatastoreP->fRxItemTypes);
  for (uInt8 i=0; i<2; i++) {
    for (pos=typesP->begin(); pos!=typesP->end(); pos++) {
      // apply maps to type
      transferMapOptionsToType(
        fFieldMappings.fFieldMapList,
        static_cast<TMultiFieldItemType *>(*pos),
        1,1 // single instance only
      );
    }
    typesP = &(aLocalDatastoreP->fTxItemTypes);
  }
} // TCustomDSConfig::addTypeLimits


// proptotype to make compiler happy
bool parseMap(TCustomDSConfig *aCustomDSConfig, TConfigElement *cfgP, bool aIsArray, TFieldListConfig *aFieldListP, TFieldMapList &aFieldMapList, const char **aAttributes);

// parse map items
bool parseMap(TCustomDSConfig *aCustomDSConfig, TConfigElement *cfgP, bool aIsArray, TFieldListConfig *aFieldListP, TFieldMapList &aFieldMapList, const char **aAttributes)
{
  TFieldMapItem *mapitemP;
  sInt16 fid = VARIDX_UNDEFINED;

  // base field reference is possible for arrays too, to specify the relevant array size
  const char* ref = cfgP->getAttr(aAttributes,aIsArray ? "sizefrom" : "references");
  if (ref) {
    // get fid for referenced field
    if (!aCustomDSConfig->fFieldMappings.fFieldListP) SYSYNC_THROW(TConfigParseException("map with no field list defined"));
    #ifdef SCRIPT_SUPPORT
    fid = TConfigElement::getFieldIndex(ref,aCustomDSConfig->fFieldMappings.fFieldListP,aCustomDSConfig->fResolveContextP);
    #else
    fid = TConfigElement::getFieldIndex(ref,aCustomDSConfig->fFieldMappings.fFieldListP);
    #endif
  }
  #ifdef ARRAYDBTABLES_SUPPORT
  if (aIsArray) {
    // array container
    mapitemP = aCustomDSConfig->newFieldMapArrayItem(aCustomDSConfig,cfgP);
    // save size field reference if any
    mapitemP->fid=fid;
    // extra attributes for derived classes
    mapitemP->checkAttrs(aAttributes);
    // let array container parse details
    cfgP->expectChildParsing(*mapitemP);
  }
  else
  #endif
  {
    // simple map
    cfgP->expectEmpty(); // plain maps may not have content
    // process creation of map item
    const char* nam = cfgP->getAttr(aAttributes,"name");
    const char* type = cfgP->getAttr(aAttributes,"type");
    const char* mode = cfgP->getAttr(aAttributes,"mode");
    bool truncate = true;
    cfgP->getAttrBool(aAttributes,"truncate",truncate,true);
    if (!nam || !ref || !type)
      return cfgP->fail("map must have 'name', 'references', 'type' and 'mode' attributes at least");
    if (fid==VARIDX_UNDEFINED)
      return cfgP->fail("map references unknown field '%s'",ref);
    // convert type
    sInt16 ty;
    if (!StrToEnum(DBFieldTypeNames,numDBfieldTypes,ty,type))
      return cfgP->fail("unknown type '%s'",type);
    // convert mode
    // - needed flags
    bool rd,wr,fins,fupd;
    // - optional flags
    bool asparam=false,floatingts=false,needsfinalisation=false;
    if (mode) {
      rd=false,wr=false; fins=false; fupd=false;
      while (*mode) {
        if (tolower(*mode)=='r') rd=true;
        else if (tolower(*mode)=='w') { wr=true; fins=true; fupd=true; } // for both insert and update
        else if (tolower(*mode)=='i') { wr=true; fins=true; } // insert only
        else if (tolower(*mode)=='u') { wr=true; fupd=true; } // update only
        else if (tolower(*mode)=='p') { asparam=true; } // map as parameter (e.g. ODBC parameter mechanism for INSERT/UPDATE statements)
        else if (tolower(*mode)=='f') { floatingts=true; } // map as floating time field (will be written as-is, no conversion from/to DB time zone takes place)
        else if (tolower(*mode)=='x') { needsfinalisation=true; } // needs to be kept for finalisation at end of session (for relational link updates etc.)
        else
          return cfgP->fail("invalid mode '%c'",*mode);
        // next char
        mode++;
      }
    }
    else {
      // default mode for needed flags
      rd=true; wr=true; fins=true; fupd=true;
    }
    // Optionals
    // - size
    sInt32 sz=0; // default to unlimited
    if (!cfgP->getAttrLong(aAttributes,"size",sz,true))
      cfgP->fail("invalid size specification");
    // - statement index
    sInt16 setno=0; // default to 0
    if (!cfgP->getAttrShort(aAttributes,"set_no",setno,true))
      cfgP->fail("invalid set_no specification");
    // create mapitem, name is SQL field name
    mapitemP = aCustomDSConfig->newFieldMapItem(nam,cfgP);
    mapitemP->fid=fid;
    mapitemP->dbfieldtype=(TDBFieldType)ty;
    mapitemP->readable=rd;
    mapitemP->writable=wr;
    mapitemP->for_insert=fins;
    mapitemP->for_update=fupd;
    mapitemP->as_param=asparam;
    mapitemP->floating_ts=floatingts;
    mapitemP->needs_finalisation=needsfinalisation;
    mapitemP->maxsize=(uInt32)sz;
    mapitemP->notruncate=!truncate;
    mapitemP->setNo=(uInt16)setno;
    // extra attributes for derived classes
    mapitemP->checkAttrs(aAttributes);
  } // if normal map
  // - and add it to the list
  aFieldMapList.push_back(mapitemP);
  return true;
} // parseMap


// Field Map item
// ==============

TFieldMapItem::TFieldMapItem(const char *aElementName, TConfigElement *aParentElement) :
  TConfigElement(aElementName,aParentElement)
{
  // default suitable for array container
  readable=false;
  writable=false;
  for_insert=false;
  for_update=false;
  as_param=false;
  floating_ts=false;
  needs_finalisation=false;
  setNo=0; // default set is 0
  maxsize=0; // no max size
  notruncate=false; // allow truncation by default
  fid=VARIDX_UNDEFINED; // no field
  dbfieldtype=dbft_string; // default to string
} // TFieldMapItem::TFieldMapItem



#ifdef ARRAYDBTABLES_SUPPORT

// array container
// ===============

TFieldMapArrayItem::TFieldMapArrayItem(TCustomDSConfig *aCustomDSConfigP, TConfigElement *aParentElement) :
  TFieldMapItem("array",aParentElement),
  fCustomDSConfigP(aCustomDSConfigP)
{
  clear();
} // TFieldMapArrayItem::TFieldMapArrayItem


TFieldMapArrayItem::~TFieldMapArrayItem()
{
  // nop so far
  clear();
} // TFieldMapArrayItem::~TFieldMapArrayItem


// init defaults
void TFieldMapArrayItem::clear(void)
{
  // init defaults
  // - clear values
  #ifdef OBJECT_FILTERING
  fNoItemsFilter.erase();
  #endif
  fMaxRepeat=1;
  fRepeatInc=1;
  fStoreEmpty=false;
  #ifdef SCRIPT_SUPPORT
  // - clear scripts
  fInitScript.erase();
  fAfterReadScript.erase();
  fBeforeWriteScript.erase();
  fAfterWriteScript.erase();
  fFinishScript.erase();
  fScriptsResolved=false;
  #endif
  // - clear map items
  TFieldMapList::iterator pos;
  for (pos=fArrayFieldMapList.begin(); pos!=fArrayFieldMapList.end(); pos++)
    delete (*pos);
  fArrayFieldMapList.clear();
  // clear inherited
  inherited::clear();
} // TFieldMapArrayItem::clear



#ifdef SCRIPT_SUPPORT

void TFieldMapArrayItem::expectScriptUnresolved(string &aTScript,sInt32 aLine, const TFuncTable *aContextFuncs)
{
	if (fScriptsResolved) {
    fail("array scripts must be defined before first <map> within array");
  }
  else {
		expectScript(aTScript,aLine,aContextFuncs);
  }
} // TFieldMapArrayItem::expectScriptUnresolved

#endif



// config element parsing
bool TFieldMapArrayItem::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  if (strucmp(aElementName,"map")==0) {
    #ifdef SCRIPT_SUPPORT
    // early resolve basic map scripts so map entries can refer to local vars
    if (!fScriptsResolved) ResolveArrayScripts();
    #endif
    // now parse map
    return parseMap(fCustomDSConfigP,this,false,fCustomDSConfigP->fFieldMappings.fFieldListP,fArrayFieldMapList,aAttributes);
  }
  /* nested arrays not yet supported
  // %%%% Note: if we do support them, we need to update
  //      the script resolution stuff above and make ProcessArrayScripts recursive
  else if (strucmp(aElementName,"array")==0)
    #ifdef SCRIPT_SUPPORT
    // early resolve basic map scripts so map entries can refer to local vars
    if (!fScriptsResolved) ResolveArrayScripts();
    #endif
    // now parse nested array map
    return parseMap(fBaseFieldMappings,this,true,fFieldListP,fArrayFieldMapList,aAttributes);
  */
  else if (strucmp(aElementName,"maxrepeat")==0)
    expectInt16(fMaxRepeat);
  else if (strucmp(aElementName,"repeatinc")==0)
    expectInt16(fRepeatInc);
  else if (strucmp(aElementName,"storeempty")==0)
    expectBool(fStoreEmpty);
  #ifdef OBJECT_FILTERING
  else if (strucmp(aElementName,"noitemsfilter")==0)
    expectString(fNoItemsFilter);
  #endif
  #ifdef SCRIPT_SUPPORT
  else if (strucmp(aElementName,"initscript")==0)
    expectScriptUnresolved(fInitScript, aLine, fCustomDSConfigP->fFieldMappings.getDSFuncTableP());
  else if (strucmp(aElementName,"afterreadscript")==0)
    expectScriptUnresolved(fAfterReadScript, aLine, fCustomDSConfigP->fFieldMappings.getDSFuncTableP());
  else if (strucmp(aElementName,"beforewritescript")==0)
    expectScriptUnresolved(fBeforeWriteScript, aLine, fCustomDSConfigP->fFieldMappings.getDSFuncTableP());
  else if (strucmp(aElementName,"afterwritescript")==0)
    expectScriptUnresolved(fAfterWriteScript, aLine, fCustomDSConfigP->fFieldMappings.getDSFuncTableP());
  else if (strucmp(aElementName,"finishscript")==0)
    expectScriptUnresolved(fFinishScript, aLine, fCustomDSConfigP->fFieldMappings.getDSFuncTableP());
  #endif
  // - none known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TFieldMapArrayItem::localStartElement



#ifdef SCRIPT_SUPPORT

// process single array's scripts (resolve or rebuild them)
void TFieldMapArrayItem::ResolveArrayScripts(void)
{
  // resolve
  TScriptContext::resolveScript(getSyncAppBase(),fInitScript,fCustomDSConfigP->fResolveContextP,fCustomDSConfigP->fFieldMappings.fFieldListP);
  TScriptContext::resolveScript(getSyncAppBase(),fAfterReadScript,fCustomDSConfigP->fResolveContextP,fCustomDSConfigP->fFieldMappings.fFieldListP);
  TScriptContext::resolveScript(getSyncAppBase(),fBeforeWriteScript,fCustomDSConfigP->fResolveContextP,fCustomDSConfigP->fFieldMappings.fFieldListP);
  TScriptContext::resolveScript(getSyncAppBase(),fAfterWriteScript,fCustomDSConfigP->fResolveContextP,fCustomDSConfigP->fFieldMappings.fFieldListP);
  TScriptContext::resolveScript(getSyncAppBase(),fFinishScript,fCustomDSConfigP->fResolveContextP,fCustomDSConfigP->fFieldMappings.fFieldListP);
  fScriptsResolved=true;
} // TFieldMapArrayItem::ResolveArrayScripts

#endif


// resolve
void TFieldMapArrayItem::localResolve(bool aLastPass)
{
  if (aLastPass) {
    #ifdef SCRIPT_SUPPORT
    // resolve map scripts now in case they haven't been resolved already
    if (!fScriptsResolved) ResolveArrayScripts();
    #endif
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TFieldMapArrayItem::localResolve

#endif


// field mappings
// ==============


TFieldMappings::TFieldMappings(const char* aName, TConfigElement *aParentElement) :
  TConfigElement(aName,aParentElement),
  fFieldListP(NULL)
{
  clear();
} // TFieldMappings::TFieldMappings


TFieldMappings::~TFieldMappings()
{
  // nop so far
  clear();
} // TFieldMappings::~TFieldMappings


// init defaults
void TFieldMappings::clear(void)
{
  // init defaults
  // - clear map items
  TFieldMapList::iterator pos;
  for (pos=fFieldMapList.begin(); pos!=fFieldMapList.end(); pos++)
    delete (*pos);
  fFieldMapList.clear();
  #ifdef SCRIPT_SUPPORT
  // - clear scripts
  fOptionFilterScript.erase();
  fInitScript.erase();
  fAfterReadScript.erase();
  fBeforeWriteScript.erase();
  fAfterWriteScript.erase();
  fFinishScript.erase(); // fFinishScript is now in use for exit call of datastore handling
  fFinalisationScript.erase(); // called for each item with fields having the needs_finalisation set (BEFORE fFinishScript)
  #endif
  // - clear reference
  fFieldListP=NULL;
  // clear inherited
  inherited::clear();
} // TFieldMappings::clear


#ifdef SCRIPT_SUPPORT

void TFieldMappings::expectScriptUnresolved(string &aTScript,sInt32 aLine, const TFuncTable *aContextFuncs)
{
	TCustomDSConfig *dscfgP = static_cast<TCustomDSConfig *>(getParentElement());
	if (dscfgP->fDSScriptsResolved) {
    fail("database scripts must be defined before first <map>");
  }
  else {
		expectScript(aTScript,aLine,aContextFuncs);
  }
} // TFieldMappings::expectScriptUnresolved

#endif


// config element parsing
bool TFieldMappings::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  TCustomDSConfig *dscfgP = static_cast<TCustomDSConfig *>(getParentElement());

  // checking the elements
  if (strucmp(aElementName,"map")==0) {
    #ifdef SCRIPT_SUPPORT
    // early resolve basic datastore scripts so map entries can refer to local vars already
    dscfgP->ResolveDSScripts();
    #endif
    // now parse map
    return parseMap(dscfgP,this,false,fFieldListP,fFieldMapList,aAttributes);
  }
  else if (strucmp(aElementName,"automap")==0) {
    // auto-create map entries for all fields in the field list
    // (this is a convenience function to allow nearly identical usage of textdb and dbapi with text module)
    if (!fFieldListP) SYSYNC_THROW(TConfigParseException("automap with no field list defined"));
    // check mode option
    bool indexAsName=false;
    if (!getAttrBool(aAttributes,"indexasname",indexAsName,true))
      fail("invalid indexasname value");
    // iterate through field definitions and create a string mapping
    TFieldDefinitionList::iterator pos;
    sInt16 fid=0;
    string fieldname;
    for (pos=fFieldListP->fFields.begin(); pos!=fFieldListP->fFields.end(); ++pos, ++fid) {
      // create DB field name
      if (indexAsName)
        StringObjPrintf(fieldname,"%hd",fid); // use fid as field name
      else
        fieldname=TCFG_CSTR(pos->fieldname); // use internal field's name
      // create mapitem using field index/itemfield name as DB field name
      TFieldMapItem *mapitemP = static_cast<TCustomDSConfig *>(getParentElement())->newFieldMapItem(fieldname.c_str(),this);
      mapitemP->fid=fid; // fid corresponds with position in field definitions list
      mapitemP->dbfieldtype = pos->type==fty_timestamp ? dbft_timestamp : dbft_string; // map timestamps as such, otherwise all are strings
      mapitemP->readable=true;
      mapitemP->writable=true;
      mapitemP->for_insert=true;
      mapitemP->for_update=true;
      // - and add it to the list
      fFieldMapList.push_back(mapitemP);
    }
    // that's it
    expectEmpty();
  }
  #ifdef ARRAYDBTABLES_SUPPORT
  else if (strucmp(aElementName,"array")==0) {
    #ifdef SCRIPT_SUPPORT
    // early resolve basic datastore scripts so array map entries can refer to local vars already!
    dscfgP->ResolveDSScripts();
    #endif
    // now parse array map
    return parseMap(dscfgP,this,true,fFieldListP,fFieldMapList,aAttributes);
  }
  #endif
  #ifdef SCRIPT_SUPPORT
  else if (strucmp(aElementName,"optionfilterscript")==0)
    expectScriptUnresolved(fOptionFilterScript, aLine, getDSFuncTableP());
  else if (strucmp(aElementName,"initscript")==0)
    expectScriptUnresolved(fInitScript, aLine, getDSFuncTableP());
  else if (strucmp(aElementName,"afterreadscript")==0)
    expectScriptUnresolved(fAfterReadScript, aLine, getDSFuncTableP());
  else if (strucmp(aElementName,"beforewritescript")==0)
    expectScriptUnresolved(fBeforeWriteScript, aLine, getDSFuncTableP());
  else if (strucmp(aElementName,"afterwritescript")==0)
    expectScriptUnresolved(fAfterWriteScript, aLine, getDSFuncTableP());
  else if (strucmp(aElementName,"finishscript")==0)
    expectScriptUnresolved(fFinishScript, aLine, getDSFuncTableP());
  else if (strucmp(aElementName,"finalisationscript")==0)
    expectScriptUnresolved(fFinalisationScript, aLine, getDSFuncTableP());
  #endif
  // - none known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TFieldMappings::localStartElement



// resolve
void TFieldMappings::localResolve(bool aLastPass)
{
  // resolve each map
  TFieldMapList::iterator pos;
  for (pos=fFieldMapList.begin(); pos!=fFieldMapList.end(); pos++) {
    (*pos)->Resolve(aLastPass);
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TFieldMappings::localResolve


#ifdef SCRIPT_SUPPORT

// access to DS func table pointer
const TFuncTable *TFieldMappings::getDSFuncTableP(void)
{
  return static_cast<TCustomDSConfig *>(getParentElement())->getDSFuncTableP();
} // TFieldMappings::getDSFuncTableP

#endif

/*
 * Implementation of TCustomImplDS
 */


// constructor
TCustomImplDS::TCustomImplDS(
  TCustomDSConfig *aConfigP,
  sysync::TSyncSession *aSessionP,
  const char *aName,
  uInt32 aCommonSyncCapMask
) :
  inherited(aConfigP,aSessionP, aName, aCommonSyncCapMask)
{
  fNeedFinalisation=false;
  // save pointer to config record
  fConfigP=aConfigP;
  fMultiFolderDB = fConfigP->fMultiFolderDB;
  // make a local copy of the typed agent pointer
  fAgentP=static_cast<TCustomImplAgent *>(fSessionP);
  // make a local copy of the typed agent config pointer
  fAgentConfigP = dynamic_cast<TCustomAgentConfig *>(
    aSessionP->getRootConfig()->fAgentConfigP
  );
  if (!fAgentConfigP) SYSYNC_THROW(TSyncException(DEBUGTEXT("TCustomImplDS finds no AgentConfig","odds7")));
  #ifdef SCRIPT_SUPPORT
  fScriptContextP=NULL; // no context yet
  #endif
  // init these keys - these might or might not be used by descendants
  fFolderKey.erase(); // the folder key is undefined
  fTargetKey.erase(); // the target key is undefined
  // clear rest
  InternalResetDataStore();
} // TCustomImplDS::TCustomImplDS


TCustomImplDS::~TCustomImplDS()
{
  InternalResetDataStore();
} // TCustomImplDS::~TCustomImplDS


/// @brief called while agent is still fully ok, so we must clean up such that later call of destructor does NOT access agent any more
void TCustomImplDS::announceAgentDestruction(void)
{
  // reset myself
  InternalResetDataStore();
  // make sure we don't access the agent any more
  // Note: as CustomImplDS always needs to be derived, we don't call
  // engTerminateDatastore() here, but rely on descendants having done that already
  fAgentP = NULL;
  // call inherited
  inherited::announceAgentDestruction();
} // TCustomImplDS::announceAgentDestruction


/// @brief called to reset datastore
/// @note must be safe to be called multiple times and even after announceAgentDestruction()
void TCustomImplDS::InternalResetDataStore(void)
{
  #ifdef SCRIPT_SUPPORT
  fOptionFilterTested=false; // not tested yet
  fOptionFilterWorksOnDBLevel=true; // assume true
  #endif
  // delete sync set
  DeleteSyncSet();
  // delete finalisation queue
  TMultiFieldItemList::iterator pos;
  for (pos=fFinalisationQueue.begin();pos!=fFinalisationQueue.end();pos++)
  	delete (*pos); // delete the item
  fFinalisationQueue.clear();
  #ifndef BASED_ON_BINFILE_CLIENT
  fGetPhase=gph_done; // must be initialized first by startDataRead
  fGetPhasePrepared=false;
  // Clear map table and sync set lists
  fMapTable.clear();
  #else // not BASED_ON_BINFILE_CLIENT
  fSyncSetLoaded=false;
  #endif // BASED_ON_BINFILE_CLIENT
  fNoSingleItemRead=false; // assume we can read single items
  if (fAgentP) {
    // forget script context
    #ifdef SCRIPT_SUPPORT
    if (fScriptContextP) {
      delete fScriptContextP; // forget context
      fScriptContextP=NULL;
    }
    #endif
  }
} // TCustomImplDS::InternalResetDataStore



// helper for getting a field pointer (local script var or item's field)
TItemField *TCustomImplDS::getMappedBaseFieldOrVar(TMultiFieldItem &aItem, sInt16 aFid)
{
  // get base field (array itself for array fields, not an element)
  #ifdef SCRIPT_SUPPORT
  if (fScriptContextP)
    return fScriptContextP->getFieldOrVar(&aItem,aFid);
  else
    return aItem.getField(aFid);
  #else
  return aItem.getField(aFid);
  #endif
} // TCustomImplDS::getMappedBaseFieldOrVar



// helper for getting a field pointer (local script var or item's field)
TItemField *TCustomImplDS::getMappedFieldOrVar(TMultiFieldItem &aItem, sInt16 aFid, sInt16 aRepOffset, bool aExistingOnly)
{
  TItemField *fieldP=NULL;
  // get field (or base field)
  #ifdef ARRAYFIELD_SUPPORT
  fieldP = getMappedBaseFieldOrVar(aItem,aFid);
  if (!fieldP) return NULL; // no field
  if (fieldP->isArray()) {
    // use aRepOffset as array index
    fieldP = fieldP->getArrayField(aRepOffset,aExistingOnly);
  }
  else
  #endif
  {
    // use aRepOffset as fid offset
    #ifdef SCRIPT_SUPPORT
    if (aFid<0) aRepOffset=0; // locals are never offset
    #endif
    fieldP = getMappedBaseFieldOrVar(aItem,aFid+aRepOffset);
  }
  return fieldP;
} // TCustomImplDS::getMappedFieldOrVar



// inform logic of coming state change
localstatus TCustomImplDS::dsBeforeStateChange(TLocalEngineDSState aOldState,TLocalEngineDSState aNewState)
{
  if (aNewState>=dssta_dataaccessdone && aOldState<dssta_dataaccessdone) {
    // ending data access
    #ifdef SCRIPT_SUPPORT
    // Call the finalisation script for added or updated items
    if (fNeedFinalisation) {
    	PDEBUGBLOCKFMT(("Finalisation","Finalizing written items","datastore=%s",getName()));
      PDEBUGPRINTFX(DBG_DATA,(
        "Finalizing %ld written items",
        (long)fFinalisationQueue.size()
      ));
      fAgentP->fScriptContextDatastore=this;
    	while (fFinalisationQueue.size()>0) {
        // process finalisation script
        TMultiFieldItem *itemP = *(fFinalisationQueue.begin());
        TScriptContext::execute(
        	fScriptContextP,fConfigP->fFieldMappings.fFinalisationScript,fConfigP->getDSFuncTableP(),fAgentP,
          itemP,true // pass the item from the queue, is writable (mainly to allow fields to be passed as by-ref params)
        );
        // no longer needed
        delete itemP;
        // remove from queue
        fFinalisationQueue.erase(fFinalisationQueue.begin());
      }
      PDEBUGENDBLOCK("Finalisation");
    }
    // Finally, call the finish script of the field mappings
    fWriting=false;
    fInserting=false;
    fDeleting=false;
    fAgentP->fScriptContextDatastore=this;
    if (!TScriptContext::execute(fScriptContextP,fConfigP->fFieldMappings.fFinishScript,fConfigP->getDSFuncTableP(),fAgentP))
      return 510; // script error -> DB error
    #endif
  }
  // let inherited do its stuff as well
  return inherited::dsBeforeStateChange(aOldState,aNewState);
} // TCustomImplDS::dsBeforeStateChange



// inform logic of happened state change
localstatus TCustomImplDS::dsAfterStateChange(TLocalEngineDSState aOldState,TLocalEngineDSState aNewState)
{
  if (aNewState==dssta_completed) {
    // completed now, call finish script
    #ifdef SCRIPT_SUPPORT
    // process sync end script
    fAgentP->fScriptContextDatastore=this;
    TScriptContext::execute(fScriptContextP,fConfigP->fSyncEndScript,fConfigP->getDSFuncTableP(),fAgentP);
    #endif
  }
  // let inherited do its stuff as well
  return inherited::dsAfterStateChange(aOldState,aNewState);
} // TCustomImplDS::dsAfterStateChange


#ifndef BASED_ON_BINFILE_CLIENT

// mark all map entries as deleted
bool TCustomImplDS::deleteAllMaps(void)
{
  string sql,s;
  bool allok=true;

  TMapContainer::iterator pos;
  for (pos=fMapTable.begin();pos!=fMapTable.end();pos++) {
    (*pos).deleted=true; // deleted
  }
  PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("deleteAllMaps: all existing map entries (%ld) now marked deleted=1",fMapTable.size()));
  return allok;
} // TCustomImplDS::deleteAllMaps


// find non-deleted map entry by local ID/maptype
TMapContainer::iterator TCustomImplDS::findMapByLocalID(const char *aLocalID,TMapEntryType aEntryType, bool aDeletedAsWell)
{
  TMapContainer::iterator pos;
  if (aLocalID) {
    for (pos=fMapTable.begin();pos!=fMapTable.end();pos++) {
      if (
        (*pos).localid==aLocalID && (*pos).entrytype==aEntryType
        // && !(*pos).remoteid.empty() // Note: was ok in old versions, but now we can have map entries from resume with empty localID
        && (aDeletedAsWell || !(*pos).deleted) // if selected, don't show deleted entries
      ) {
        // found
        return pos;
      }
    }
  }
  return fMapTable.end();
} // TCustomImplDS::findMapByLocalID


// find map entry by remote ID
TMapContainer::iterator TCustomImplDS::findMapByRemoteID(const char *aRemoteID)
{
  TMapContainer::iterator pos;
  if (aRemoteID) {
    for (pos=fMapTable.begin();pos!=fMapTable.end();pos++) {
      if (
        (*pos).remoteid==aRemoteID && (*pos).entrytype == mapentry_normal && !(*pos).deleted // only plain normal non-deleted maps (no tempid or mapforresume)
      ) {
        // found
        return pos;
      }
    }
  }
  return fMapTable.end();
} // TCustomImplDS::findMapByRemoteID


#ifndef SYSYNC_CLIENT

// - called when a item in the sync set changes its localID (due to local DB internals)
//   Datastore must make sure that eventually cached items get updated
void TCustomImplDS::dsLocalIdHasChanged(const char *aOldID, const char *aNewID)
{
  // find item in map
  TMapContainer::iterator pos=findMapByLocalID(aOldID,mapentry_normal); // only plain maps, no deleted ones
  if (pos!=fMapTable.end()) {
    // found, modify now
    // NOTE: we may not modify a localid, but must delete the entry and add a new one
    // - get remote ID
    string remoteid = (*pos).remoteid;
    uInt32 mapflags = (*pos).mapflags;
    // - mark old map entry as deleted
    (*pos).deleted=true;
    // - create new one
    modifyMap(mapentry_normal,aNewID,remoteid.c_str(),mapflags,false);
  }
  // let base class do what is needed to update the item itself
  inherited::dsLocalIdHasChanged(aOldID, aNewID);
} // TCustomImplDS::dsLocalIdHasChanged

#endif

/// @brief modify internal map table
/// @note
/// - if aDelete is set, map entry will be deleted
/// - aClearFlags (default=all) will be cleared when updating only
/// - aMapFlags will be set when updating
/// - if aRemoteID is NULL when updating an existing (not marked deleted) item, existing remoteID will NOT be modified
/// - otherwise, map item will be added or updated.
/// - routine makes sure that there is no more than one map for each localID/entrytype and
///   each remoteID.
/// - remoteID can also be a temporary localID used by server to send to clients with too small MaxGUIDSize (mapflag_tempidmap set)
/// - routine will re-activate deleted entries to avoid unnecessary delete/insert
void TCustomImplDS::modifyMap(TMapEntryType aEntryType, const char *aLocalID, const char *aRemoteID, uInt32 aMapFlags, bool aDelete, uInt32 aClearFlags)
{
  TMapContainer::iterator pos=fMapTable.end();

  DEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
    "ModifyMap called: aEntryType=%s, aLocalID='%s, aRemoteid='%s', aMapflags=0x%lX, aDelete=%d",
    MapEntryTypeNames[aEntryType],
    aLocalID && *aLocalID ? aLocalID : "<none>",
    aRemoteID ? (*aRemoteID ? aRemoteID : "<set none>") : "<do not change>",
    aMapFlags,
    (int)aDelete
  ));
  // - if there is a localID, search map entry (even if it is deleted)
  if (aLocalID && *aLocalID!=0) {
    for (pos=fMapTable.begin();pos!=fMapTable.end();pos++) {
      if (
        // localID and entrytype matches
        (*pos).localid==aLocalID && (*pos).entrytype==aEntryType
      ) {
        PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
          "- found entry by entrytype/localID='%s' - remoteid='%s', mapflags=0x%lX, changed=%d, deleted=%d, added=%d, markforresume=%d, savedmark=%d",
          aLocalID,
          (*pos).remoteid.c_str(),
          (*pos).mapflags,
          (int)(*pos).changed,
          (int)(*pos).deleted,
          (int)(*pos).added,
          (int)(*pos).markforresume,
          (int)(*pos).savedmark
        ));
        break;
      }
    }
  }
  else aLocalID=NULL;
  // decide what to do
  if (aDelete) {
    // delete
    if (!aLocalID) {
      if (!aRemoteID) return; // nop
      // we need to search by remoteID first
      pos=findMapByRemoteID(aRemoteID);
      #ifdef SYDEBUG
      if (pos!=fMapTable.end()) {
        DEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
          "- found entry by remoteID='%s' - localid='%s', mapflags=0x%lX, changed=%d, deleted=%d, added=%d, markforresume=%d, savedmark=%d",
          aRemoteID,
          (*pos).localid.c_str(),
          (*pos).mapflags,
          (int)(*pos).changed,
          (int)(*pos).deleted,
          (int)(*pos).added,
          (int)(*pos).markforresume,
          (int)(*pos).savedmark
        ));
      }
      #endif
    }
    if (pos==fMapTable.end()) return; // not found, nop
    // mark deleted
    if ((*pos).added) {
      // has been added in this session and not yet saved
      // so it does not yet exist in the DB at all
      // - simply forget entry
      fMapTable.erase(pos);
      // - done, ok
      return;
    }
    // entry has already been saved to DB before - only mark deleted
    (*pos).deleted=true;
  } // delete
  else {
    // update or add
    if (pos==fMapTable.end()) {
      PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
        "- found no matching entry for localID '%s' - creating new one, added=true",
        aLocalID
      ));
      // add, because there is not yet an item with that localid/entrytype
      TMapEntry entry;
      entry.entrytype=aEntryType;
      entry.added=true;
      entry.changed=true;
      entry.localid=aLocalID;
      AssignString(entry.remoteid,aRemoteID); // if NULL, remoteID will be empty
      entry.savedmark=false;
      entry.markforresume=false;
      entry.mapflags=0; // none set by default
      fMapTable.push_front(entry);
      pos=fMapTable.begin(); // first entry is new entry
    }
    else {
      PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
        "- matching entry found - re-activating deleted and/or updating contents if needed"
      ));
      /* %%% NO!!!! plain wrong - because this causes added and later changed entries never be added to the DB!
      // %%% No room for paranoia here - makes things worse!
      // just to make sure - added should not be set here
      // (if we delete an added one before, it will be completely deleted from the list again)
      (*pos).added=false;
      */
      // check if contents change, update if so
      if (
        (((*pos).mapflags & ~mapflag_useforresume) != aMapFlags) || // flags different (useForResume not tested!)
        (aRemoteID && (*pos).remoteid!=aRemoteID) // remoteID different
      ) {
        // new RemoteID (but not NULL = keep existing) or different mapflags were passed -> this is a real change
        if (aRemoteID)
          (*pos).remoteid=aRemoteID;
        (*pos).changed=true; // really changed compared to what is already in DB
      }
    }
    // now item exists, set details
    (*pos).deleted=false; // in case we had it deleted before, but not yet saved
    // clear those flags shown in aClearFlags (by default: all) and set those in aMapFlags
    (*pos).mapflags = (*pos).mapflags & ~aClearFlags | aMapFlags;
    // now remove all other items with same remoteID (except if we have no or empty remoteID)
    if (aEntryType==mapentry_normal && aRemoteID && *aRemoteID) {
      // %%% note: this is strictly necessary only for add, but cleans up for update
      TMapContainer::iterator pos2;
      for (pos2=fMapTable.begin();pos2!=fMapTable.end();pos2++) {
        if (pos2!=pos && (*pos2).remoteid==aRemoteID && (*pos2).entrytype==aEntryType) {
          // found another one with same remoteID/entrytype
          PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
            "- cleanup: removing same remoteID from other entry with localid='%s', mapflags=0x%lX, changed=%d, deleted=%d, added=%d, markforresume=%d, savedmark=%d",
            (*pos2).localid.c_str(),
            (*pos2).mapflags,
            (int)(*pos2).changed,
            (int)(*pos2).deleted,
            (int)(*pos2).added,
            (int)(*pos2).markforresume,
            (int)(*pos2).savedmark
          ));
          // this remoteID is invalid for sure as we just have assigned it to another item - remove it
          (*pos2).remoteid.erase();
          (*pos2).changed=true; // make sure it gets saved
        }
      }
    }
  } // modify or add
} // TCustomImplDS::modifyMap


#endif // not BASED_ON_BINFILE_CLIENT


// delete syncset
// - if aContentsOnly, only item data will be deleted, but localID/containerid will
//   be retained
void TCustomImplDS::DeleteSyncSet(bool aContentsOnly)
{
  TSyncSetList::iterator pos;
  for (pos=fSyncSetList.begin();pos!=fSyncSetList.end();pos++) {
    // delete contained item, if any
    if ((*pos)->itemP) {
      delete ((*pos)->itemP);
      (*pos)->itemP=NULL;
    }
    if (!aContentsOnly)
      delete (*pos); // delete syncsetitem itself
  }
  if (!aContentsOnly) fSyncSetList.clear();
} // TCustomImplDS::DeleteSyncSet


// - get container ID for specified localid
bool TCustomImplDS::getContainerID(const char *aLocalID, string &aContainerID)
{
  TSyncSetList::iterator pos = findInSyncSet(aLocalID);
  if (pos!=fSyncSetList.end()) {
    aContainerID = (*pos)->containerid;
    return true; // found
  }
  return false; // not found
} // TCustomImplDS::getContainerID


// find entry in sync set by localid
TSyncSetList::iterator TCustomImplDS::findInSyncSet(const char *aLocalID)
{
  TSyncSetList::iterator pos;
  for (pos=fSyncSetList.begin();pos!=fSyncSetList.end();pos++) {
    if ((*pos)->localid==aLocalID) {
      // found
      return pos;
    }
  }
  return fSyncSetList.end();
} // TCustomImplDS::findInSyncSet


// called when message processing
void TCustomImplDS::dsEndOfMessage(void)
{
  // let ancestor do things
  inherited::dsEndOfMessage();
} // TCustomImplDS::dsEndOfMessage



// Simple DB access interface methods


// - returns true if database implementation can only update all fields of a record at once
bool TCustomImplDS::dsReplaceWritesAllDBFields(void)
{
  // return true if we should read record from DB before replacing.
  return fConfigP->fUpdateAllFields;
} // TCustomImplDS::dsReplaceWritesAllDBFields


#ifdef OBJECT_FILTERING

// - returns true if DB implementation can also apply special filters like CGI-options
//   /dr(x,y) etc. during fetching
bool TCustomImplDS::dsOptionFilterFetchesFromDB(void)
{
  #ifndef SYSYNC_TARGET_OPTIONS
  // there are no ranges to filter at all
  return true; // we can "filter" this (nothing)
  #else
  // no filter range set: yes, we can filter
  if (fDateRangeStart==0 && fDateRangeEnd==0) return true; // we can "filter" this
  // see if a script provides a solution
  #ifdef SCRIPT_SUPPORT
  if (!fOptionFilterTested) {
    fOptionFilterTested=true;
    // call script to take measures such that database implementation can
    // filter, returns true if filtering is entirely possible
    // (e.g. for ODBC, script should generate appropriate WHERE clause and set it with SETSQLFILTER())
    fAgentP->fScriptContextDatastore=this;
    fOptionFilterWorksOnDBLevel = TScriptContext::executeTest(
      false, // assume we cannot filter if no script or script returns nothing
      fScriptContextP, // context
      fConfigP->fFieldMappings.fOptionFilterScript, // the script
      fConfigP->getDSFuncTableP(),fAgentP // funcdefs/context
    );
  }
  if (fOptionFilterWorksOnDBLevel) return true;
  #endif
  // we can't filter, let anchestor try
  return inherited::dsOptionFilterFetchesFromDB();
  #endif
} // TCustomImplDS::dsOptionFilterFetchesFromDB


#endif // OBJECT_FILTERING



/// sync login (into this database)
/// @note might be called several times (auth retries at beginning of session)
/// @note must update the following saved AND current state variables
/// - in TLocalEngineDS: fLastRemoteAnchor, fLastLocalAnchor, fResumeAlertCode, fFirstTimeSync
///   - for client: fPendingAddMaps
///   - for server: fTempGUIDMap
/// - in TStdLogicDS: fPreviousSyncTime, fCurrentSyncTime
/// - in TCustomImplDS: fCurrentSyncCmpRef, fCurrentSyncIdentifier, fPreviousToRemoteSyncCmpRef,
///        fPreviousToRemoteSyncIdentifier, fPreviousSuspendCmpRef, fPreviousSuspendIdentifier
/// - in derived classes: whatever else belongs to dsSavedAdmin and dsCurrentAdmin state
localstatus TCustomImplDS::implMakeAdminReady(
  const char *aDeviceID,    // remote device URI (device ID)
  const char *aDatabaseID,  // database ID
  const char *aRemoteDBID  // database ID of remote device
)
{
  localstatus sta=LOCERR_OK; // assume ok
  string sql;


  // init state variables
  // dsSavedAdmin
  // - of TLocalEngineDS:
  fFirstTimeSync=true; // assume first time sync
  fLastRemoteAnchor.erase(); // remote anchor not known yet
  fResumeAlertCode=0; // no session to resume
  // - of TStdLogicDS:
  fPreviousSyncTime=0;
  // - of TCustomImplDS:
  fPreviousToRemoteSyncCmpRef=0; // no previous session
  fPreviousToRemoteSyncIdentifier.erase();
  fPreviousSuspendCmpRef=0; // no previous suspend
  fPreviousSuspendIdentifier.erase();

  // dsCurrentAdmin
  // - of TLocalEngineDS:
  // - of TStdLogicDS:
  fCurrentSyncTime=0;
  // - of TCustomImplDS:
  fCurrentSyncCmpRef=0;
  fCurrentSyncIdentifier.erase();

  #ifndef BASED_ON_BINFILE_CLIENT
  fMapTable.clear(); // map is empty to begin with
  #endif
  // now get admin data
  SYSYNC_TRY {
    #ifdef SCRIPT_SUPPORT
    // rebuild context for all scripts (if not already resolved)
    // Note: unlike while reading config, here all maps and scripts are already available
    //       so this will build the entire context at once.
    if (!fScriptContextP) {
      // Rebuild order MUST be same as resolving order (see ResolveDSScripts())
      // - scripts in eventual derivates
      apiRebuildScriptContexts();
      // - adminready and end scripts outside the fieldmappings
      TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fConfigP->fAdminReadyScript,fScriptContextP,fSessionP);
      TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fConfigP->fSyncEndScript,fScriptContextP,fSessionP);
      // - scripts within fieldmappings
      TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fConfigP->fFieldMappings.fOptionFilterScript,fScriptContextP,fSessionP);
      TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fConfigP->fFieldMappings.fInitScript,fScriptContextP,fSessionP);
      TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fConfigP->fFieldMappings.fAfterReadScript,fScriptContextP,fSessionP);
      TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fConfigP->fFieldMappings.fBeforeWriteScript,fScriptContextP,fSessionP);
      TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fConfigP->fFieldMappings.fAfterWriteScript,fScriptContextP,fSessionP);
      TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fConfigP->fFieldMappings.fFinishScript,fScriptContextP,fSessionP);
      TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fConfigP->fFieldMappings.fFinalisationScript,fScriptContextP,fSessionP);
      #ifdef ARRAYDBTABLES_SUPPORT
      // - rebuild array script vars
      TFieldMapList::iterator pos;
      for (pos=fConfigP->fFieldMappings.fFieldMapList.begin(); pos!=fConfigP->fFieldMappings.fFieldMapList.end(); pos++) {
        if ((*pos)->isArray()) {
          TFieldMapArrayItem *fmaiP = dynamic_cast<TFieldMapArrayItem *>(*pos);
          if (fmaiP) {
            // rebuild
            // %%% note, this is not capable of nested arrays yet
            TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fmaiP->fInitScript,fScriptContextP,fSessionP);
            TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fmaiP->fAfterReadScript,fScriptContextP,fSessionP);
            TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fmaiP->fBeforeWriteScript,fScriptContextP,fSessionP);
            TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fmaiP->fAfterWriteScript,fScriptContextP,fSessionP);
            TScriptContext::rebuildContext(fSessionP->getSyncAppBase(),fmaiP->fFinishScript,fScriptContextP,fSessionP);
          }
        }
      }
      #endif
      // now instantiate variables
      TScriptContext::buildVars(fScriptContextP);
    }
    #endif
    #ifdef BASED_ON_BINFILE_CLIENT
    // binfile's implLoadAdminData will do the job
    sta = inherited::implMakeAdminReady(aDeviceID, aDatabaseID, aRemoteDBID);

    #else
    // Load admin data from TXXXApiDS (ODBC, text or derived class' special implementation)
    sta = apiLoadAdminData(
      aDeviceID,    // remote device URI (device ID)
      aDatabaseID,  // database ID
      aRemoteDBID   // database ID of remote device
    );
    #endif
    // set error if one occurred during load
    if (sta==LOCERR_OK) {
      // extra check: if we get empty remote anchor, this is a first-time sync even if DB claims the opposite
      if (fLastRemoteAnchor.empty())
        fFirstTimeSync=true;
      // create identifier if we don't have it stored in the DB
      if (!fConfigP->fStoreSyncIdentifiers) {
        // we generate the identifiers as ISO8601 UTC string from the timestamp
        TimestampToISO8601Str(fPreviousToRemoteSyncIdentifier,fPreviousToRemoteSyncCmpRef,TCTX_UTC,false,false);
        TimestampToISO8601Str(fPreviousSuspendIdentifier,fPreviousSuspendCmpRef,TCTX_UTC,false,false);
      }
      // determine time of this sync
      fCurrentSyncTime=
        fAgentP->getDatabaseNowAs(TCTX_UTC);
      // by default, these two are equal
      // (but if DB cannot write items with timestamp exactly==fCurrentSyncTime, fCurrentSyncCmpRef might be a later time, like end-of-session)
      fCurrentSyncCmpRef = fCurrentSyncTime;
      // admin ready now, call script that may access DB to fetch some extra options
      #ifdef SCRIPT_SUPPORT
      fAgentP->fScriptContextDatastore=this;
      if (!TScriptContext::executeTest(true,fScriptContextP,fConfigP->fAdminReadyScript,fConfigP->getDSFuncTableP(),fAgentP))
        sta=510; // script returns false or fails -> DB error
      #endif
    } // if apiLoadAdminData successful
  }
  SYSYNC_CATCH(exception &e)
    PDEBUGPRINTFX(DBG_ERROR,("implMakeAdminReady exception: %s",e.what()));
    sta=510;
  SYSYNC_ENDCATCH
  // done
  return sta;
} // TCustomImplDS::implMakeAdminReady



// start data read
localstatus TCustomImplDS::implStartDataRead()
{
  localstatus sta = LOCERR_OK;

  // get field map list
  TFieldMapList &fml = fConfigP->fFieldMappings.fFieldMapList;

  // check if we have fileds that must be finalized or array fields at all (to avoid unneeded operations if not)
  TFieldMapList::iterator pos;
	#ifdef ARRAYDBTABLES_SUPPORT
  fHasArrayFields=false; // until we KNOW otherwise
  #endif
  fNeedFinalisation=false; // until we KNOW otherwise
  for (pos=fml.begin(); pos!=fml.end(); pos++) {
    // - check finalisation
    if ((*pos)->needs_finalisation)
      fNeedFinalisation=true;
    // - check array mappings
	  #ifdef ARRAYDBTABLES_SUPPORT
    if ((*pos)->isArray())
      fHasArrayFields=true;
    #endif
  }
  #ifdef BASED_ON_BINFILE_CLIENT
  // further preparation is in binfileds
  sta = inherited::implStartDataRead();
  if (sta==LOCERR_OK) {
    // now make sure the syncset is loaded
    if (!makeSyncSetLoaded(
      fSlowSync
      #ifdef OBJECT_FILTERING
      || fFilteringNeededForAll
      #endif
    ))
      sta = 510; // error
  }
  #else
  // kill all map entries if slow sync (but not if resuming!!)
  if (fSlowSync && !isResuming()) {
    // mark all map entries as deleted
    deleteAllMaps();
  }
  // - count entire read as database read
  TP_DEFIDX(li);
  TP_SWITCH(li,fSessionP->fTPInfo,TP_database);
  PDEBUGBLOCKFMTCOLL(("ReadSyncSet","Reading Sync Set from Database","datastore=%s",getName()));
  SYSYNC_TRY {
    // read sync set (maybe from derived non-odbc data source)
    // - in slow sync, we need all items (so allow ReadSyncSet to read them all here)
    // - if all items must be filtered, we also need all data
    // Note: ReadSyncSet will decide if it actually needs to load the syncset or not (depends on refresh, slowsync and needs of apiZapSyncSet())
    sta = apiReadSyncSet(
      fSlowSync
      #ifdef OBJECT_FILTERING
      || fFilteringNeededForAll
      #endif
    );
    // determine how GetItem will start
    fGetPhase = fSlowSync ? gph_added_changed : gph_deleted; // just report added (not-in-map, map is cleared already) for slowsync
    // phase not yet prepared
    fGetPhasePrepared = false;
    // end of DB read
    PDEBUGENDBLOCK("ReadSyncSet");
    TP_START(fSessionP->fTPInfo,li);
  }
  SYSYNC_CATCH(exception &e)
    PDEBUGPRINTFX(DBG_ERROR,("StartDataRead exception: %s",e.what()));
    sta=510;
    // end of DB read
    PDEBUGENDBLOCK("ReadSyncSet");
    TP_START(fSessionP->fTPInfo,li);
  SYSYNC_ENDCATCH
  #endif // not BASED_ON_BINFILE_CLIENT
  return sta;
} // TCustomImplDS::implStartDataRead



// Queue the data needed for finalisation (usually - relational link updates)
// as a item copy with only finalisation-required fields
void TCustomImplDS::queueForFinalisation(TMultiFieldItem *aItemP)
{
	sInt16 fid;
	// create a same-typed copy of the original item (initially empty)
  TMultiFieldItem *itemP = new TMultiFieldItem(aItemP->getItemType(),aItemP->getTargetItemType());
  // copy localID and syncop
  itemP->setLocalID(aItemP->getLocalID());
  itemP->setSyncOp(aItemP->getSyncOp());
  // copy fields that are marked for finalisation
  TFieldMapList &fml = fConfigP->fFieldMappings.fFieldMapList;
  TFieldMapList::iterator pos;
  for (pos=fml.begin(); pos!=fml.end(); pos++) {
	  TFieldMapItem *fmiP = *pos;
		#ifdef ARRAYDBTABLES_SUPPORT
  	if (fmiP->isArray()) {
		  TFieldMapList::iterator pos2;
		  TFieldMapList &afml = static_cast<TFieldMapArrayItem *>(fmiP)->fArrayFieldMapList;
  		for (pos2=afml.begin(); pos2!=afml.end(); pos2++) {
			  TFieldMapItem *fmi2P = *pos2;
        fid = fmi2P->fid;
        if (fmi2P->needs_finalisation && fid>=0) {
          // this mapping indicates need for finalisation and references a fieldlist field, copy referenced field
          *(itemP->getField(fid))=*(aItemP->getField(fid));
        }
      }
    }
  	else
    #endif // ARRAYDBTABLES_SUPPORT
    {
      fid = fmiP->fid;
      if (fmiP->needs_finalisation && fid>=0) {
        // this mapping indicates need for finalisation and references a fieldlist field, copy referenced field
        *(itemP->getField(fid))=*(aItemP->getField(fid));
      }
    }
  }
  // put the finalisation item into the queue
  fFinalisationQueue.push_back(itemP);
} // TCustomImplDS::queueForFinalisation



#ifndef BASED_ON_BINFILE_CLIENT

/// @brief called to have all non-yet-generated sync commands as "to-be-resumed"
void TCustomImplDS::implMarkOnlyUngeneratedForResume(void)
{
  // Note: all "markforresume" flags (but NOT the actual mapflag_useforresume!) are cleared
  //       after loading or saving admin, so we can start adding resume marks BEFORE
  //       implMarkOnlyUngeneratedForResume is called (needed to re-add items that got
  //       an unsuccessful status from remote that suggests re-trying in next resume, such as 514)
  // add all not-yet-got items
  TMapContainer::iterator pos;
  TGetPhases getPhase = fGetPhase; // start at current get phase
  bool getPrepared = fGetPhasePrepared;
  // now flag all deletes that need resuming
  if (getPhase==gph_deleted) {
    // if we are still in "deleted" phase, add these first
    if (!getPrepared)
      pos=fMapTable.begin();
    else
      pos=fDeleteMapPos;
    // now mark pending deletes
    while (pos!=fMapTable.end()) {
      // check only undeleted map entries
      // Note: non-normal maps are always in deleted state in fMapTable, so these will be skipped as well
      if ((*pos).deleted) continue;
      // check if deleted
      if (findInSyncSet((*pos).localid.c_str())==fSyncSetList.end()) {
        // mark this as pending for resume
        (*pos).markforresume=true;
      }
      // next
      ++pos;
    }
    getPhase=gph_added_changed;
    getPrepared=false;
  }
  // now flag all changes and adds that need resuming
  // if we are already gph_done, no items need to be flagged here
  if (getPhase==gph_added_changed) {
    // if we are in the add/change phase, add not-yet handled adds/changes
    TSyncSetList::iterator syncsetpos;
    if (!getPrepared)
      syncsetpos=fSyncSetList.begin();
    else
      syncsetpos=fSyncSetPos;
    // now mark pending changes and adds
    while (syncsetpos!=fSyncSetList.end()) {
      // check if we need to mark it
      bool needMark=false;
      pos=findMapByLocalID((*syncsetpos)->localid.c_str(),mapentry_normal,true); // find deleted ones as well
      if (fSlowSync) {
        #ifdef SYSYNC_CLIENT
        // for client, there are no reference-only: mark all leftovers in a slow sync
        needMark=true;
        #else
        // for server, make sure not to mark reference-only.
        if (!isResuming() || pos==fMapTable.end()) {
          // if not resuming, or we have no map for this one at all - we'll need it again for resume
          needMark=true;
        }
        else {
          // for slowsync resume which have already a map:
          // - items that are not marked for resume, but already have a remoteID mapped
          //   are reference-only and must NOT be marked
          if (((*pos).mapflags & mapflag_useforresume) || (*pos).remoteid.empty())
            needMark=true;
        }
        #endif
      }
      else if (!isRefreshOnly()) {
        // not slow sync, and not refresh from remote only - mark those that are actually are involved
        if (pos!=fMapTable.end()) {
          // known item, needs a mark only if record is modified (and updates reported at all)
          needMark=((*syncsetpos)->isModified) && fReportUpdates;
        }
        else {
          // adds need marking, anyway
          needMark=true;
        }
      }
      // now apply mark if needed
      if (needMark) {
        if (pos==fMapTable.end()) {
          // no map entry for this item yet (this means that this is a new detected add
          // - add pendingAddConfirm item now, and mark it for resume
          TMapEntry entry;
          entry.entrytype=mapentry_normal;
          entry.localid=(*syncsetpos)->localid.c_str();
          entry.remoteid.erase();
          entry.mapflags=mapflag_pendingAddConfirm;
          entry.added=true;
          entry.changed=true;
          entry.deleted=false;
          entry.markforresume=true;
          entry.savedmark=false;
          fMapTable.push_back(entry);
        }
        else {
          // add flag to existing map item
          if ((*pos).deleted) {
            // undelete (re-use existing, but currently invalid entry)
            (*pos).remoteid.erase();
            (*pos).changed=true;
            (*pos).deleted=false;
            (*pos).mapflags=0;
          }
          (*pos).markforresume=true;
        }
      }
      // next
      ++syncsetpos;
    }
  }
} // TCustomImplDS::implMarkOnlyUngeneratedForResume


// called to confirm a sync operation's completion (status from remote received)
// @note aSyncOp passed not necessarily reflects what was sent to remote, but what actually happened
void TCustomImplDS::dsConfirmItemOp(TSyncOperation aSyncOp, cAppCharP aLocalID, cAppCharP aRemoteID, bool aSuccess, localstatus aErrorStatus)
{
  if (aSyncOp==sop_delete || aSyncOp==sop_archive_delete) {
    // a confirmed delete causes the entire map entry to be removed (item no longer exists (or is visible) locally or remotely)
    if (aSuccess) {
      PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("successful status for delete received -> delete map entry now"));
      modifyMap(mapentry_normal,aLocalID,aRemoteID,0,true);
    }
  }
  else {
    TMapContainer::iterator pos;
    #ifdef SYSYNC_CLIENT
    // for client, always find by localid
    pos=findMapByLocalID(aLocalID,mapentry_normal);
    #else
    // for server, only add can be found by localid
    if (aSyncOp==sop_add)
      pos=findMapByLocalID(aLocalID,mapentry_normal);
    else
      pos=findMapByRemoteID(aRemoteID);
    #endif
    if (pos!=fMapTable.end()) {
      // Anyway, clear the status pending flag
      // Note: we do not set the "changed" bit here because we don't really need to make this persistent between sessions
      (*pos).mapflags &= ~mapflag_pendingStatus;
      #ifdef SYSYNC_CLIENT
      if (aSuccess) {
        // Note: we do not check for sop here - any successfully statused sop will clear the pending add status
        //       (e.g. in slow sync, items reported as add to engine are actually sent as replaces, but still
        //       seeing a ok status means that they are not any longer pending as adds)
        // Note: the same functionality formerly was in TStdLogicDS::startDataWrite() - making sure that a
        //       add sent to the server is not repeated. As every item reported by implGetItem now already
        //       has a map entry (adds get one with mapflag_pendingAddConfirm set), we just need to clear
        //       the flag here now that we know the add has reached the server.
        // Note: For the server, we can clear the mapflag_pendingAddConfirm not before we have received a <Map> item for it!
        PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("successful status for non-delete received -> clear mapflag_pendingAddConfirm"));
        (*pos).mapflags &= ~mapflag_pendingAddConfirm;
        (*pos).changed = true; // this MUST be made persistent!
      }
      #endif
    }
    else {
      PDEBUGPRINTFX(DBG_ERROR+DBG_EXOTIC,("dsConfirmItemOp - INTERNAL ERROR: no map entry exists for item"));
    }
  }
  // let inherited know as well
  inherited::dsConfirmItemOp(aSyncOp, aLocalID, aRemoteID, aSuccess, aErrorStatus);
} // TCustomImplDS::confirmItemOp


// called to mark an already sent item as "to-be-resent", e.g. due to temporary
// error status conditions, by localID or remoteID (latter only in server case).
void TCustomImplDS::implMarkItemForResend(cAppCharP aLocalID, cAppCharP aRemoteID)
{
  // Note: this is only relevant for replaces and some adds:
  //       - some adds will not have a map entry yet
  //       - deletes will not have their map entry deleted until they are confirmed
  //       But replaces would not get re-sent as they would not get detected
  //       modified any more once a session has completed.
  TMapContainer::iterator pos;
  if (aLocalID && *aLocalID)
    pos=findMapByLocalID(aLocalID,mapentry_normal,false); // only undeleted ones
  else if (aRemoteID && *aRemoteID)
    pos=findMapByRemoteID(aRemoteID);
  else
    return; // neither local nor remote ID specified -> nop
  if (pos==fMapTable.end()) {
    PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("implMarkItemForResend: no map entry found -> item does not exist or is an add which will be sent anyway"));
    return; // no item can be searched or created
  }
  // set resend flag now (if not already set)
  if (!((*pos).mapflags & mapflag_resend)) {
    (*pos).mapflags |= mapflag_resend;
    (*pos).changed=true;
  }
  // and make sure it gets resent in case of a suspend as well (mark for resume)
  // This is needed for suspends where unprocessed items gets statused with 514 BEFORE
  // the server knows that this is going to be a suspend.
  (*pos).markforresume=true;
  PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC+DBG_HOT,(
    "localID='%s' marked for resending by setting mapflag_resend (AND mark for eventual resume!), flags now=0x%lX",
    (*pos).localid.c_str(),
    (*pos).mapflags
  ));
} // TCustomImplDS::implMarkItemForResend


// called to mark an already generated (but unsent or sent but not yet statused) item
// as "to-be-resumed", by localID or remoteID (latter only in server case).
void TCustomImplDS::implMarkItemForResume(cAppCharP aLocalID, cAppCharP aRemoteID, bool aUnSent)
{
  TMapContainer::iterator pos;
  if (aLocalID && *aLocalID)
    pos=findMapByLocalID(aLocalID,mapentry_normal,true); // also find deleted ones
  else if (aRemoteID && *aRemoteID)
    pos=findMapByRemoteID(aRemoteID);
  else
    return; // no item can be searched or created
  if (pos!=fMapTable.end()) {
    // we have an entry for this item, mark it for resume
    if ((*pos).deleted) {
      // undelete (re-use existing, but currently invalid entry)
      (*pos).remoteid.erase();
      (*pos).changed=true;
      (*pos).deleted=false;
      (*pos).mapflags=0;
    }
    // for unsent ones, the status is not pending any more
    else if (aUnSent && ((*pos).mapflags & (mapflag_pendingStatus+mapflag_pendingDeleteStatus))) {
      // reset the status pending flags
      (*pos).changed=true;
      (*pos).mapflags &= ~(mapflag_pendingStatus+mapflag_pendingDeleteStatus);
    }
    // For Server: those that have mapflag_pendingAddConfirm (adds) may be marked for
    //   resume ONLY if we can rely on early maps or if they are completely unsent.
    //   Sent adds will just keep their mapflag_pendingAddConfirm until they receive their map
    // For Client: all items will be marked for resume
    #ifndef SYSYNC_CLIENT
    if (
      ((*pos).mapflags & mapflag_pendingAddConfirm) && // is an add...
      !aUnSent && // ...and already sent out
      !fSessionP->getSessionConfig()->fRelyOnEarlyMaps // and we can't rely on the client sending the maps before
    ) {
      // for server: already sent adds may not be repeated unless we can rely on early maps
      PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
        "implMarkItemForResume: localID='%s', has mapFlags=0x%lX and was probably executed at remote -> NOT marked for resume",
        (*pos).localid.c_str(),
        (*pos).mapflags
      ));
      (*pos).markforresume=false;
    }
    else
    #endif
    {
      // for client: everything may be repeated and therefore marked for resume
      // for server: unsent adds will also be marked, or all if we can rely on early maps (which is the default)
      PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
        "implMarkItemForResume: localID='%s', has mapFlags=0x%lX and was %s executed at remote%s -> mark for resume",
        (*pos).localid.c_str(),
        (*pos).mapflags,
        aUnSent ? "NOT" : "probably",
        fSessionP->getSessionConfig()->fRelyOnEarlyMaps ? " (relying on early maps)" : ""
      ));
      (*pos).markforresume=true;
    }
  }
  else if (aLocalID && *aLocalID) {
    PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
      "implMarkItemForResume: localID='%s', was not yet in map and was %sexecuted at remote -> created map and marked for resume",
      aLocalID,
      aUnSent ? "NOT " : "probably"
    ));
    // we have no entry for this item, make one (only if this is not a remoteID-only item,
    // but this should not occur here anyway - items without localID can only be replaces
    // from server to client, in which case we have a map entry anyway)
    TMapEntry entry;
    entry.entrytype=mapentry_normal;
    entry.localid=aLocalID;
    entry.remoteid.erase();
    entry.mapflags=0;
    entry.added=true;
    entry.changed=true;
    entry.deleted=false;
    entry.markforresume=true;
    entry.savedmark=false;
    fMapTable.push_back(entry);
  }
} // TCustomImplDS::implMarkItemForResume



// Get next item from database
localstatus TCustomImplDS::implGetItem(
  bool &aEof,
  bool &aChanged, // if set on entry, only changed ones will be reported, otherwise all will be returned and aChanged contains flag if entry has changed or not
  TSyncItem* &aSyncItemP
)
{
  localstatus sta = LOCERR_OK;

  bool reportChangedOnly = aChanged; // save initial state, as we might repeat...
  bool rep=true; // to start-up lower part
  TSyncOperation sop=sop_none;
  aEof=true; // assume we have nothing to report
  string remid;
  TMultiFieldItem *myitemP=NULL;

  // short-cut if refreshing only and not slowsync resuming (then we need the items for comparison)
  if (isRefreshOnly() && !(isResuming() && isSlowSync()))
    return sta; // aEof is set, return nothing

  TP_DEFIDX(li);
  TP_SWITCH(li,fSessionP->fTPInfo,TP_database);
  SYSYNC_TRY {
    // check mode
    if (fGetPhase==gph_deleted) {
      // report deleted items, that is those in the map that are not
      // any more in the sync set of local IDs
      // Note: we need no extra database access any more for this
      if (!fGetPhasePrepared) {
        // start at beginning of map table
        fDeleteMapPos=fMapTable.begin();
        fGetPhasePrepared=true;
      }
      do {
        rep=false;
        // report all those map entries that have no data entry (any more)
        // as deleted records
        // - check if there is more data in map table to process
        if (fDeleteMapPos==fMapTable.end()) {
          fGetPhase=gph_added_changed; // end of this phase, now report additions and modifications
          rep=true; // continue in second part (get updated records)
          fGetPhasePrepared=false; // next phase must be prepared
          break;
        }
        else {
          // search if there is a local ID still existing for that ID
          TMapEntry entry = (*fDeleteMapPos);
          // check only undeleted map entries
          if (entry.deleted || entry.entrytype!=mapentry_normal) {
            // deleted or non-normal map entry - simply skip
            rep=true;
          }
          else if (isResuming() && !(entry.mapflags & mapflag_useforresume)) {
            // this delete has already been reported deleted in previous suspended session
            // (or it was deleted from the datastore WHILE or after start of a suspended session,
            // in this case the item will be reported deleted in the NEXT complete sync session)
            PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("Resuming and found item not marked for resume -> ignore for delete checking"));
            rep=true;
          }
          else if (findInSyncSet(entry.localid.c_str())==fSyncSetList.end()) {
            // this item has been deleted (and not yet been reported if this is a resume) -> report it
            PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("Normal sync and found item in map which is not in syncset -> delete and mark with mapflag_pendingDeleteStatus"));
            // - create new empty TMultiFieldItem
            myitemP =
              (TMultiFieldItem *) newItemForRemote(ity_multifield);
            // - add IDs
            myitemP->setRemoteID(entry.remoteid.c_str());
            myitemP->setLocalID(entry.localid.c_str());
            // - set operation
            myitemP->setSyncOp(sop_delete);
            // - set item
            aSyncItemP = myitemP;
            aEof=false; // report something
            // mark entry as waiting for delete status
            // NOTES: - we cannot delete the map entry until we get a confirmItemOp() for it
            //        - the pendingStatus flag does not need to be persistent between sessions, so we don't set the changed flag here!
            entry.mapflags |= mapflag_pendingDeleteStatus;
            (*fDeleteMapPos)=entry; // save updated entry in list
            // found one to report
          }
          else {
            rep=true; // must repeat again
          }
        }
        // go to next
        fDeleteMapPos++;
      } while(rep);
    } // report deleted phase
    if (rep && fGetPhase==gph_added_changed) {
      // report changed and added items. Those that are in the sync set but
      // not yet in the map are added items, those that are already in the
      // map are changed items if the mod date is newer than last sync
      // and deleted if they don't pass extra filters
      if (!fGetPhasePrepared) {
        // start at beginning of map table
        fSyncSetPos=fSyncSetList.begin();
        fGetPhasePrepared=true;
      }
      do {
        rep=false;
        remid.erase();
        // - check if there is more data in the syncset to process
        if (fSyncSetPos==fSyncSetList.end()) {
          fGetPhase=gph_done; // end of this phase, now report additions and modifications
          rep=true; // continue in next phase (if any)
          fGetPhasePrepared=false; // next phase must be prepared
          break;
        }
        else {
          // get syncset entry
          TSyncSetItem *syncsetitemP = (*fSyncSetPos);
          sop=sop_none;
          TMapContainer::iterator pos;
          // search if there is a map entry for this item already
          pos=findMapByLocalID(syncsetitemP->localid.c_str(),mapentry_normal); // do not find deleted ones, only valid entries!
          #ifdef SYDEBUG
          if (pos!=fMapTable.end()) {
            // Debug
            PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,(
              "Item localID='%s' already has map entry: remoteid='%s', mapflags=0x%lX, changed=%d, deleted=%d, added=%d, markforresume=%d, savedmark=%d",
              syncsetitemP->localid.c_str(),
              (*pos).remoteid.c_str(),
              (*pos).mapflags,
              (int)(*pos).changed,
              (int)(*pos).deleted,
              (int)(*pos).added,
              (int)(*pos).markforresume,
              (int)(*pos).savedmark
            ));
          }
          #endif
          // now find what syncop results
          if (fSlowSync && !isResuming()) {
            // all items in local sync set are to be reported
            PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("Slow sync and not resuming -> all items are first reported sop_wants_replace (will become add later)"));
            sop=sop_wants_replace;
            aChanged=true;
            // clear the resend flags if any
            if (pos!=fMapTable.end()) {
              if ((*pos).mapflags & mapflag_resend) {
                (*pos).mapflags &= ~mapflag_resend;
                (*pos).changed = true;
              }
            }
          }
          else {
            if (pos!=fMapTable.end()) {
              #ifndef SYSYNC_CLIENT
              // for slowsync resume - items that are not marked for resume, but already have a remoteID mapped
              // must be presented for re-match with sop_reference_only
              if (fSlowSync && isResuming() && !((*pos).mapflags & mapflag_useforresume) && !(*pos).remoteid.empty()) {
                // this item apparently was already slow-sync-matched before the suspend - still show it for reference to avoid re-adding it
                sop=sop_reference_only;
              }
              else
              #endif
              if (!isRefreshOnly()) {
                // item is already in map: check if this is an already detected, but unfinished add
                if (!((*pos).mapflags & mapflag_pendingAddConfirm)) {
                  // is a replace (not an add): changed if mod date newer or resend flagged (AND updates enabled)
                  // Note: For reporting modifications, the date of last sending-data-to-remote date is relevant
                  bool hasChanged=
                    (((*fSyncSetPos)->isModified) || ((*pos).mapflags & mapflag_resend))
                    && fReportUpdates;
                  // reset resend flag here if it acutually causes a resend here (otherwise, keep it for later)
                  if (hasChanged && (*pos).mapflags & mapflag_resend) {
                    PDEBUGPRINTFX(DBG_ADMIN+DBG_HOT,("Item '%s' treated as changed because resend-flag was set",syncsetitemP->localid.c_str()));
                    (*pos).mapflags &= ~mapflag_resend;
                    (*pos).changed = true;
                  }
                  if (isResuming()) {
                    // Basically, on resume just report those that have the mapflag_useforresume flag set.
                    // However, there is one difficult problem here:
                    // - Those that were successfully modified in the suspended part of a session, but get modified
                    //   AGAIN between suspend and this resume will NOT be detected as changes any more. Therefore, for
                    //   items we see here that don't have the mapflag_useforresume, we need to check additionally if
                    //   they eventually have changed AFTER THE LAST SUSPEND SAVE
                    if ((*pos).mapflags & mapflag_useforresume) {
                      PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("Resuming and found marked-for-resume -> send replace"));
                      sop=sop_wants_replace;
                      hasChanged=true; // mark this as change (again, because marked for resume)
                    }
                    else if (!reportChangedOnly || hasChanged) {
                      // this one does not have the flag set, but it would be reported as a change if this was not a resume
                      // so this could be a change happened between suspend and resume
                      if (syncsetitemP->isModifiedAfterSuspend) {
                        // yes, this one was modified AFTER the suspended session, so we'll send it, too
                        PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("Resuming and found NOT marked-for-resume, but changed after last suspend -> send replace again"));
                        sop=sop_wants_replace;
                        hasChanged=true; // mark this as change (since last suspend, that is)
                      }
                    }
                  }
                  else if (!reportChangedOnly || hasChanged) {
                    // report only if aChanged was false on entry (=reportChangedOnly is false) or if modified anyway
                    PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("Normal sync, item changed -> send replace"));
                    sop=sop_wants_replace;
                  }
                  remid=(*pos).remoteid;
                  aChanged=hasChanged; // return changed status anyway
                } // if not add
                else {
                  // already detected, but unfinished add
                  // For server: Might be resent ONLY if resuming and marked for resume (otherwise, we MUST wait for map or we'll get duplicates)
                  // For client: resend always except if resuming and not marked for it
                  if (isResuming()) {
                    if ((*pos).mapflags & mapflag_useforresume) {
                      PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("Resuming and found marked-for-resume with mapflag_pendingAddConfirm -> unsent add, send it again"));
                      sop=sop_wants_add;
                    }
                    else {
                      PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("Resuming and NOT marked-for-resume with mapflag_pendingAddConfirm -> ignore"));
                    }
                  }
                  else {
                    #ifdef SYSYNC_CLIENT
                    // for client - repeating an add does not harm (but helps if it did not reach the server in the previous attempt
                    PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("Non-resume sync found item with mapflag_pendingAddConfirm -> send it again"));
                    sop=sop_wants_add;
                    #else
                    // for server - repeating an add potentially DOES harm (duplicate if client already got the add, but didn't send a map yet)
                    // but it's ok if it's flagged as an explicit resend (this happens only if we have got error status from remote)
                    if ((*pos).mapflags & mapflag_resend) {
                      PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("Item with mapflag_pendingAddConfirm (add) also has mapflag_resend -> we can safely resend"));
                      sop=sop_wants_add;
                      // - reset resend flag here
                      (*pos).mapflags &= ~mapflag_resend;
                      (*pos).changed = true;
                    }
                    else {
                      PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("Non-resume sync found item with mapflag_pendingAddConfirm (add) -> ignore until map is found"));
                    }
                    #endif
                  }
                }


              } // if not refreshonly
            } // a map entry already exists
            else {
              // item is not yet in map: this is a new one. Report it if we are not resuming a previous session
              // (in this case, items added after start of the original session will be detected at NEXT full
              // session, so just leave it out for now)
              // Note: this is first-time add detection. If we get this reported as sop_wants_add below, a map with
              //       mapflag_pendingAddConfirm will be created for it.
              if (isRefreshOnly()) {
                PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("New item (no map yet) detected during Refresh only -> ignore for now, will be added in next two-way sync"));
                sop=sop_none;
              }
              else if (isResuming()) {
                PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("Resuming and found new item which was not present in original session -> ignore for now, will be added in next regular sync"));
                sop=sop_none;
              }
              else {
                PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("Normal sync, item not yet in map -> add to remote"));
                sop=sop_wants_add;
              }
            }
          }
          if (sop!=sop_none) {
            // we need the item from the database
            SYSYNC_TRY {
              bool fetched=false;
              // - check if we've already read it (at apiReadSyncSet())
              if (syncsetitemP->itemP) {
                // yes, take it out of the syncset
                if (fNoSingleItemRead) {
                  // we may need it later, so we can only pass a copy to caller
                  myitemP =
                    (TMultiFieldItem *) newItemForRemote(ity_multifield);
                  if (!myitemP)
                    SYSYNC_THROW(TSyncException("newItemForRemote could not create new Item"));
                  // copy item (id, op, contents)
                  (*myitemP) = (*(syncsetitemP->itemP));
                }
                else {
                  // we don't need it later, pass original to caller and remove link in syncsetitem
                  myitemP = syncsetitemP->itemP;
                  syncsetitemP->itemP = NULL; // syncsetitem does not own it any longer
                }
                fetched=true; // we have the item
                // Make sure item is fully equipped
                // - assign local id, as it is required by DoDataSubstitutions
                myitemP->setLocalID(syncsetitemP->localid.c_str());
                // - assign remote id if we know one
                myitemP->setRemoteID(remid.c_str());
                // - set operation
                myitemP->setSyncOp(sop);
              }
              else {
                // We need to read the item here
                // - create new empty TMultiFieldItem
                myitemP =
                  (TMultiFieldItem *) newItemForRemote(ity_multifield);
                if (!myitemP)
                  SYSYNC_THROW(TSyncException("newItemForRemote could not create new Item"));
                // - assign local id, as it is required by DoDataSubstitutions
                myitemP->setLocalID(syncsetitemP->localid.c_str());
                // - assign remote id if we know one
                myitemP->setRemoteID(remid.c_str());
                // - set operation
                myitemP->setSyncOp(sop);
                // Now fetch item (read phase)
                sta=apiFetchItem(*myitemP,true,syncsetitemP);
                if (sta==LOCERR_OK) {
                  // successfully fetched
                  fetched=true;
                }
                else if (sta==404) {
                  // this record has been deleted since we have read the
                  // localid list. If this is was an add, we can simply
                  // ignore it
                  // - decide what this means now
                  if (sop==sop_reference_only || sop==sop_wants_add) {
                    // was deleted before we could fetch it for adding (or reference), just ignore
                    PDEBUGPRINTFX(DBG_DATA+DBG_DETAILS,("to-be-added record localID=%s was deleted during this sync session -> ignore",myitemP->getLocalID()));
                    rep=true;
                    delete myitemP;
                    // could still be that we have a mapflag_useforresume map entry, make sure we get rid of it
                    if (pos!=fMapTable.end()) {
                      // mark deleted
                      (*pos).deleted=true;
                    }
                    goto nextchanged;
                  }
                  else {
                    // was changed, but now doesn't exist any more -> treat as delete
                    // - adjust operation
                    PDEBUGPRINTFX(DBG_DATA+DBG_DETAILS,("to-be-changed record localID=%s was deleted during this sync session -> delete",myitemP->getLocalID()));
                    myitemP->setSyncOp(sop_delete);
                    myitemP->cleardata(); // make sure it is empty
                    // - set item to return to caller
                    aSyncItemP = myitemP;
                    aEof=false; // report something
                    if (!fSlowSync) {
                      // Note: we can safely assume that the map entry exists - otherwise sop would be sop_wants_add
                      // map entry must be marked to show that this now is a pending delete
                      (*pos).changed=true;
                      (*pos).mapflags &= ~(mapflag_pendingStatus);
                      (*pos).mapflags |= mapflag_pendingDeleteStatus;
                    }
                    fetched=false; // prevent map manipulations below
                  }
                }
                else {
                  // other error
                  SYSYNC_THROW(TSyncException("Error fetching data from DB",sta));
                }
              } // else: fetch from DB needed
              if (fetched) {
                /* %%% moved map adjustments to implReviewReadItem, as maps must not be created
                 * for items that get filtered out of the syncset (which happens later after
                 * calling this routine)!
                 *
                // if we don't have a map entry here, this MUST be a potential add
                // NOTE: Don't touch map if this is a for-reference-only (meaning that the map is
                //   already ok, and it is included here ONLY to find eventual slowsync matches)!
                if (sop!=sop_reference_only) {
                  if (pos==fMapTable.end()) {
                    // this MUST be an add - create new map entry
                    modifyMap(mapentry_normal,myitemP->getLocalID(),NULL,mapflag_pendingAddConfirm+mapflag_pendingStatus,false);
                  }
                  else {
                    // only adjust map flags
                    if (fSlowSync || sop==sop_add || sop==sop_wants_add) {
                      // for slowsync, all items are kind of "adds", that is, not yet mapped (server case)
                      //   or not yet statused (client case)
                      // for normal sync, make sure adds get mapflag_pendingAddConfirm set
                      (*pos).mapflags &= ~mapflag_pendingDeleteStatus;
                      (*pos).mapflags |= mapflag_pendingAddConfirm+mapflag_pendingStatus;
                      (*pos).changed=true;
                    }
                    else {
                      // not add (and never a delete here) -> is replace. Set status pending flag (which doesn't need to be saved to DB)
                      (*pos).mapflags &= ~(mapflag_pendingAddConfirm+mapflag_pendingDeleteStatus);
                      (*pos).mapflags |= mapflag_pendingStatus;
                    }
                  }
                }
                */
                // set item to return to caller
                aSyncItemP = myitemP;
                aEof=false; // report something
                // info
                PDEBUGPRINTFX(DBG_DATA+DBG_DETAILS,("Fetched record data from DB with localID=%s",myitemP->getLocalID()));
              }
            }
            SYSYNC_CATCH(...)
              if (myitemP) delete myitemP;
              SYSYNC_RETHROW;
            SYSYNC_ENDCATCH
          } // item to report
          else {
            // item read must not be reported, try to get next
            rep=true;
          }
        } // not end of syncset
      nextchanged:
        // go to next
        fSyncSetPos++;
      } while(rep);
    } // report added and changed phase
    if (rep) {
      // end of items
      aEof=true;
      // syncset can be deleted only if we can retrieve individual items later from DB
      // if not, we must keep the syncset in memory
      if (!fNoSingleItemRead) {
        // we don't need the SyncSet list any more
        // (and especially the items that did NOT get reported to the caller of GetItem
        // can be deleted now to free memory. Reported items are now owned by the caller)
        DeleteSyncSet(fMultiFolderDB);
      }
    }
    // done
    TP_START(fSessionP->fTPInfo,li);
    // show item fetched
    #ifdef SYDEBUG
    if (PDEBUGTEST(DBG_DATA+DBG_SCRIPTS) && aSyncItemP)
      aSyncItemP->debugShowItem(DBG_DATA); // show item fetched
    #endif
  }
  SYSYNC_CATCH(exception &e)
    PDEBUGPRINTFX(DBG_ERROR,("GetItem exception: %s",e.what()));
    TP_START(fSessionP->fTPInfo,li);
    sta=510;
  SYSYNC_ENDCATCH
  // done
  return sta;
} // TCustomImplDS::implGetItem

#endif


// end of read
localstatus TCustomImplDS::implEndDataRead(void)
{
  #ifdef BASED_ON_BINFILE_CLIENT
  // let binfile handle it
  return inherited::implEndDataRead();
  #else
  return apiEndDataRead();
  #endif
} // TCustomImplDS::implEndDataRead


// start of write
localstatus TCustomImplDS::implStartDataWrite()
{
  localstatus sta = LOCERR_OK;

  #ifdef BASED_ON_BINFILE_CLIENT
  // let binfile handle it
  sta = inherited::implStartDataWrite();
  #else
  SYSYNC_TRY {
    // let actual data implementation prepare
    sta = apiStartDataWrite();
    if (sta==LOCERR_OK) {
      // Notes:
      // - transaction starts implicitly when first INSERT / UPDATE / DELETE occurs
      // - resumed slow refreshes must NOT zap the sync set again!
      // - prevent zapping when datastore is in readonly mode!
      if (fRefreshOnly && fSlowSync && !isResuming() && !fReadOnly) {
        // now, we need to zap the DB first
        PDEBUGBLOCKFMTCOLL(("ZapSyncSet","Zapping sync set in database","datastore=%s",getName()));
        SYSYNC_TRY {
          sta=apiZapSyncSet();
          PDEBUGENDBLOCK("ZapSyncSet");
        }
        SYSYNC_CATCH(exception &e)
          PDEBUGPRINTFX(DBG_ERROR,("ZapSyncSet exception: %s",e.what()));
          sta=510;
          // end of DB read
          PDEBUGENDBLOCK("ZapSyncSet");
        SYSYNC_ENDCATCH
        if (sta!=LOCERR_OK) {
          PDEBUGPRINTFX(DBG_ERROR,("implStartDataWrite: cannot zap data for refresh, status=%hd",sta));
        }
        // ok, now that the old data is zapped, we MUST forget the former sync set, it is now for sure invalid
        DeleteSyncSet(false);
      }
    }
  }
  SYSYNC_CATCH(exception &e)
    PDEBUGPRINTFX(DBG_ERROR,("implStartDataWrite exception: %s",e.what()));
    sta=510;
  SYSYNC_ENDCATCH
  #endif
  // done
  return sta;
} // TCustomImplDS::implStartDataWrite


#ifndef BASED_ON_BINFILE_CLIENT

// review reported entry (allows post-processing such as map deleting)
// MUST be called after StartDataWrite, before any actual writing,
// for each item obtained in GetItem
localstatus TCustomImplDS::implReviewReadItem(
  TSyncItem &aItem         // the item
)
{
  // get the operation
  TSyncOperation sop = aItem.getSyncOp();
  // NOTE: Don't touch map if this is a for-reference-only (meaning that the map is
  //   already ok, and it is included here ONLY to find eventual slowsync matches)!
  if (sop!=sop_reference_only) {
    // Adjust map flags or create map if needed
    if (fSlowSync || sop==sop_add || sop==sop_wants_add) {
      // for slowsync, all items are kind of "adds", that is, not yet mapped (server case)
      //   or not yet statused (client case)
      // for normal sync, make sure adds get mapflag_pendingAddConfirm set and remoteID gets cleared (is not valid in any case)
      modifyMap(mapentry_normal,aItem.getLocalID(),"",mapflag_pendingAddConfirm+mapflag_pendingStatus,false,mapflag_pendingDeleteStatus);
    }
    else if (sop==sop_delete) {
      // In case when postFetch filtering changed an item from replace to delete,
      // we must make sure that the map entry gets the deleted status set
      // - simply make sure deleted item's map entry will get deleted once the delete is confirmed
      modifyMap(mapentry_normal,aItem.getLocalID(),NULL,mapflag_pendingDeleteStatus,false,0);
    }
    else {
      // not add (and never a delete here) -> is replace. Set status pending flag (which doesn't need to be saved to DB)
      modifyMap(mapentry_normal,aItem.getLocalID(),NULL,mapflag_pendingStatus,false,mapflag_pendingAddConfirm+mapflag_pendingDeleteStatus);
    }
  }
  return LOCERR_OK;
} // TCustomImplDS::implReviewReadItem


// - retrieve specified item from database
bool TCustomImplDS::implRetrieveItemByID(
  TSyncItem &aItem,         // the item
  TStatusCommand &aStatusCommand
)
{
  bool ok=true;

  // determine item's local ID
  if (!aItem.hasLocalID()) {
    #ifdef SYSYNC_CLIENT
    // client case: MUST have local ID
    aStatusCommand.setStatusCode(400); // bad request (no address)
    return false;
    #else
    // no local ID specified directly, address by remote ID
    if (!aItem.hasRemoteID()) {
      aStatusCommand.setStatusCode(400); // bad request (no address)
      return false;
    }
    // lookup remote ID in map
    TMapContainer::iterator mappos = findMapByRemoteID(aItem.getRemoteID());
    if (mappos==fMapTable.end()) {
      aStatusCommand.setStatusCode(404); // not found
      return false;
    }
    // set local ID
    aItem.setLocalID(mappos->localid.c_str());
    // check if we have a local ID now
    if (!aItem.hasLocalID()) {
      aStatusCommand.setStatusCode(400); // bad request (no address)
      return false;
    }
    #endif
  }
  TP_DEFIDX(li);
  TP_SWITCH(li,fSessionP->fTPInfo,TP_database);
  // if we can't fetch single items from DB, we should have all
  // in the syncset and can get them from there
  if (fNoSingleItemRead) {
    // search sync set by localID
    TSyncSetList::iterator pos=findInSyncSet(aItem.getLocalID());
    if (pos!=fSyncSetList.end() && (*pos)->itemP) {
      // found, copy data to item passed
      aItem.replaceDataFrom(*((*pos)->itemP));
    }
    else {
      // not found
      aStatusCommand.setStatusCode(404); // not found
      ok=false;
    }
  }
  else {
    // fetch from DB
    SYSYNC_TRY {
      // fetch data from DB
      localstatus sta=apiFetchItem(*((TMultiFieldItem *)&aItem),false,NULL); // no syncsetitem known
      if (sta!=LOCERR_OK) {
        aStatusCommand.setStatusCode(sta);
        ok=false;
      }
    }
    SYSYNC_CATCH(exception &e)
      PDEBUGPRINTFX(DBG_ERROR,("implRetrieveItemByID exception: %s",e.what()));
      aStatusCommand.setStatusCode(510);
      ok=false;
    SYSYNC_ENDCATCH
  }
  // return status
  TP_START(fSessionP->fTPInfo,li);
  return ok;
} // TCustomImplDS::implRetrieveItemByID



/// called to set maps.
/// @note aRemoteID or aLocalID can be NULL - which signifies deletion of a map entry
/// @note that this might be needed for clients accessing a server-style database as well
localstatus TCustomImplDS::implProcessMap(cAppCharP aRemoteID, cAppCharP aLocalID)
{
  localstatus sta = 510; // error
  // Note: Map must be ready to have either empty local or remote ID to delete an entry
  if (!aLocalID) {
    // delete by remote ID
    modifyMap(mapentry_normal,NULL,aRemoteID,0,true);
    PDEBUGPRINTFX(DBG_ADMIN,("Map entry (or entries) for RemoteID='%s' removed",aRemoteID));
    sta=LOCERR_OK;
  }
  else {
    // - if RemoteID is empty, this means that Map should be deleted
    if (!aRemoteID) {
      // Map delete request
      modifyMap(mapentry_normal,aLocalID,NULL,0,true);
      PDEBUGPRINTFX(DBG_ADMIN,("Map entry (or entries) for LocalID='%s' removed",aLocalID));
      sta=LOCERR_OK;
    }
    else {
      // Map modify or add request, automatically clears mapflag_pendingAddConfirm (and all other)
      // flag(s), INLCUDING resume mark. So even if all sent adds (and not only the unsent ones)
      // get marked for resume in a suspend (fRelyOnEarlyMaps set), those that actually got added
      // in the previous session will not be re-sent for sure.
      modifyMap(mapentry_normal,aLocalID,aRemoteID,0,false);
      PDEBUGPRINTFX(DBG_ADMIN,("Map entry updated: LocalID='%s', RemoteID='%s'",aLocalID,aRemoteID));
      sta=LOCERR_OK;
    } // if not map delete
  }
  // return status
  return sta;
} // TCustomImplDS::implProcessMap



/// process item (according to operation: add/delete/replace - and for future: copy/move)
/// @note data items will be sent only after StartWrite()
bool TCustomImplDS::implProcessItem(
  TSyncItem *aItemP,         // the item
  TStatusCommand &aStatusCommand
) {
  bool ok=true;
  localstatus sta=LOCERR_OK;
  string localID;
  const char *remoteID;
  // %%% bool RemoteIDKnown=false;
  TMapContainer::iterator mappos;
  TSyncOperation sop=sop_none;

  TP_DEFIDX(li);
  TP_SWITCH(li,fSessionP->fTPInfo,TP_database);
  // get field map list
  TFieldMapList &fml = fConfigP->fFieldMappings.fFieldMapList;
  SYSYNC_TRY {
    // get casted item pointer
    TMultiFieldItem *myitemP = (TMultiFieldItem *)aItemP;
    // - get op
    sop = myitemP->getSyncOp();
    // - check IDs
    #ifdef SYSYNC_CLIENT
    // Client case: we always get the local ID, except for add
    localID=myitemP->getLocalID();
    remoteID=myitemP->getRemoteID();
    if (!localID.empty() && sop!=sop_add && sop!=sop_wants_add)
      mappos=findMapByLocalID(localID.c_str(),mapentry_normal); // for all but sop == sop_add
    else
      mappos=fMapTable.end(); // if there is no localid or it is an add, we have no map entry yet
    #else
    // Server case: we only know the remote ID
    // - get remoteID
    remoteID=myitemP->getRemoteID();
    // first see if we have a map entry for this remote ID
    localID.erase(); // none yet
    // Note: even items detected for deletion still have a map item until deletion is confirmed by the remote party,
    //       so we'll be able to update already "deleted" items (in case they are not really gone, but only invisible in the sync set)
    mappos=findMapByRemoteID(remoteID); // search for it
    if (mappos!=fMapTable.end()) {
      localID = (*mappos).localid; // assign it if we have it
    }
    #endif
    // - now perform op
    aStatusCommand.setStatusCode(510); // default DB error
    switch (sop) {
      /// @todo sop_copy is now implemented by read/add sequence
      ///       in localEngineDS, but will be moved here later eventually
      case sop_add :
        // add item and retrieve new localID for it
        sta = apiAddItem(*myitemP,localID);
        #ifndef SYSYNC_CLIENT
        if (sta==DB_DataMerged) {
        	// while adding, data was merged with pre-existing data (external from the sync set)
          // so we should retrieve the full data and send an update back to the client
          // - this is like forcing a conflict, i.e. this loads the item by local/remoteid and adds it to
          //   the to-be-sent list of the server.
					PDEBUGPRINTFX(DBG_DATA,("Database adapter indicates that added item was merged with pre-existing data (status 207), so update client with merged item"));
          forceConflict(myitemP);
          sta = LOCERR_OK; // otherwise, treat as ok
        }
        #endif
        if (sta!=LOCERR_OK) {
          aStatusCommand.setStatusCode(sta);
          ok=false;
        }
        else {
          // added ok
          // - save what is needed for finalisation
          if (fNeedFinalisation) {
          	myitemP->setLocalID(localID.c_str()); // finalisation needs to know the local ID
          	queueForFinalisation(myitemP);
          }
          // - status ok
          aStatusCommand.setStatusCode(201); // item added
          // - add or update map entry (in client case, remoteID is irrelevant and eventually is not saved)
          modifyMap(mapentry_normal,localID.c_str(),remoteID,0,false);
          ok=true;
        }
        break;
      case sop_replace :
        if (mappos==fMapTable.end()) {
          // not found in map table
          aStatusCommand.setStatusCode(404);
          ok=false;
        }
        else {
          // - make sure item has local ID set
          myitemP->setLocalID(localID.c_str());
          // update item
          sta = apiUpdateItem(*myitemP);
          if (sta!=LOCERR_OK) {
            aStatusCommand.setStatusCode(sta);
            ok=false;
          }
          else {
            // updated ok
            // - save what is needed for finalisation
            if (fNeedFinalisation)
              queueForFinalisation(myitemP);
            // - status ok
            aStatusCommand.setStatusCode(200); // item replaced ok
            ok=true;
          }
        }
        break;
      case sop_delete :
      case sop_archive_delete :
      case sop_soft_delete :
        if (mappos==fMapTable.end()) {
          // not found in map table means that remote is trying to
          // delete an item that wasn't mapped before. This is different from
          // the case below when the actual item is not there any more, but
          // the map still existed (-> 211)
          aStatusCommand.setStatusCode(404);
          ok=false;
        }
        else {
          // - make sure item has local ID set
          myitemP->setLocalID(localID.c_str());
          // delete item
          sta = apiDeleteItem(*myitemP);
          if (sta!=LOCERR_OK) {
            // not found is reported as successful 211 status, because result is ok (item deleted, whatever reason)
            if (sta==404)
              sta=211; // ok, but item was not there any more
            else
              ok=false; // others are real errors
            aStatusCommand.setStatusCode(sta);
          }
          else {
            // - ok
            aStatusCommand.setStatusCode(200); // item deleted ok
            ok=true;
          }
          if (ok) {
            // delete map entry anyway
            // - mark the map entry for deletion
            modifyMap(mapentry_normal,localID.c_str(),NULL,0,true);
          }
        }
        break;
      default :
        SYSYNC_THROW(TSyncException("Unknown sync op in TCustomImplDS::implProcessItem"));
    } // switch
    if (ok) {
      // successful, save new localID in item
      myitemP->setLocalID(localID.c_str());
      // make sure transaction is complete after processing the item
      TP_START(fSessionP->fTPInfo,li);
      return true;
    }
    else {
      // make sure transaction is rolled back for this item
      TP_START(fSessionP->fTPInfo,li);
      return false;
    }
  }
  SYSYNC_CATCH (exception &e)
    PDEBUGPRINTFX(DBG_ERROR,("******** TCustomImplDS::implProcessItem exception: %s",e.what()));
    aStatusCommand.setStatusCode(510);
    goto error;
  SYSYNC_ENDCATCH
  SYSYNC_CATCH (...)
    PDEBUGPRINTFX(DBG_ERROR,("******** TCustomImplDS::implProcessItem unknown exception"));
    goto error;
  SYSYNC_ENDCATCH
error:
  // Switch back to previous timer
  TP_START(fSessionP->fTPInfo,li);
  return false;
} // TCustomImplDS::implProcessItem



// private helper to prepare for apiSaveAdminData()
localstatus TCustomImplDS::SaveAdminData(bool aSessionFinished, bool aSuccessful)
{
  TMapContainer::iterator pos;
  localstatus sta=LOCERR_OK;
  // calculate difference between current and previous state of tempGUID maps or pending maps
  // - mark all non-main map entries as deleted (those that still exist will be re-added later)
  // - also do some clean-up in case of successful end-of-session
  pos=fMapTable.begin();
  while (pos!=fMapTable.end()) {
    if ((*pos).entrytype!=mapentry_normal) {
      // Note: this is not strictly needed any more, as non-normal maps are
      // now already entered into fMapTable with deleted flag set
      // (to make sure they don't get used by accident)
      (*pos).deleted=true;
    }
    else if (!(*pos).deleted) {
      // in case of map table without flags, we must get rid of all non-real maps
      if (!dsResumeSupportedInDB() && aSessionFinished) {
        #ifndef SYSYNC_CLIENT
        // For client, remoteid is irrelevant and can well be empty
        //   Map entries exist for those items that are not newly added on the client
        // For server, maps w/o remoteid are not really mapped and must not be saved when
        //   we have no flags to mark this special conditon (mapflag_pendingAddConfirm)
        if ((*pos).remoteid.empty()) {
          // no remoteid -> this is not a real map, we cannot represent it w/o resume support (=flags) in map table
          DEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("LocalID='%s' has no remoteID - cannot be stored in non-DS-1.2 Map DB -> removed map",(*pos).localid.c_str()));
          if ((*pos).added) {
            // was never added to DB, so no need to delete it in DB either - just forget it
            TMapContainer::iterator delpos=pos++;
            fMapTable.erase(delpos);
            continue;
          }
          else {
            // is already in DB - mark deleted
            (*pos).deleted=true;
          }
        }
        else
        #endif
        {
          // clear all specials
          (*pos).mapflags=0;
          (*pos).savedmark=false;
          (*pos).markforresume=false;
        }
      }
      // normal, undeleted map entry, check for cleanup
      else if (
        aSessionFinished && aSuccessful &&
        ((*pos).mapflags & mapflag_pendingAddConfirm)
      ) {
        // successful end of session - we can forget pending add confirmations (as the add commands apparently never reached the remote at all)
        #ifndef SYSYNC_CLIENT
        // Note: for clients, maps can well have an empty remoteid (because it does not need to be saved)
        if ((*pos).remoteid.empty()) {
          PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("Successful end of session but localID='%s' has no remoteID and pendingAddConfirm still set -> removed map",(*pos).localid.c_str()));
          // if not mapped, this will be a re-add in the next session, so forget it for now
          if ((*pos).added) {
            // was never added to DB, so no need to delete it in DB either - just forget it
            TMapContainer::iterator delpos=pos++;
            fMapTable.erase(delpos);
            continue;
          }
          else {
            // is already in DB - mark deleted
            (*pos).deleted=true;
          }
        }
        else
        #endif
        {
          // For server: is mapped, which means that it now exists in the client - just clean mapflag_pendingAddConfirm
          // For client: just clean the pendingAddConfirm
          // Note: maps like this should not exist at this time - as at end of a successful session all items should
          //       have got confirmation.
          PDEBUGPRINTFX(DBG_ERROR,("Apparently successful end of session - but localID='%s' has pendingAddConfirm still set: sync may not be fully complete",(*pos).localid.c_str()));
          (*pos).mapflags &= ~mapflag_pendingAddConfirm; // keep mapflag_pendingStatus for documentary/debug purposes
          (*pos).changed = true; // make sure it gets written to DB
        }
      }
    }
    // increment here only so we can do "continue" w/o pos increment after delete
    pos++;
  } // for all map entries
  if (dsResumeSupportedInDB()) {
    // If we have different map entry types - re-add the special entries from the separate lists
    // Note: these entries are already in the global map table, but with the deleted flag set.
    //       Here those that still exist now will be re-activated (without saving them again if not needed)
    TStringToStringMap::iterator spos;
    #ifdef SYSYNC_CLIENT
    // - now pending maps (unsent ones)
    PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("SaveAdminData: adding %ld entries from fPendingAddMap as mapentry_pendingmap",fPendingAddMaps.size()));
    for (spos=fPendingAddMaps.begin();spos!=fPendingAddMaps.end();spos++) {
    	string locID = (*spos).first;
      dsFinalizeLocalID(locID); // make sure we have the permanent version in case datastore implementation did deliver temp IDs
      modifyMap(mapentry_pendingmap, locID.c_str(), (*spos).second.c_str(), 0, false);
    }
    // - now pending maps (sent, but not seen status yet)
    PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("SaveAdminData: adding %ld entries from fUnconfirmedMaps as mapentry_pendingmap/mapflag_pendingMapStatus",fUnconfirmedMaps.size()));
    for (spos=fUnconfirmedMaps.begin();spos!=fUnconfirmedMaps.end();spos++) {
      modifyMap(mapentry_pendingmap, (*spos).first.c_str(), (*spos).second.c_str(), mapflag_pendingMapStatus, false);
    }
    #else
    // - the tempguid maps
    PDEBUGPRINTFX(DBG_ADMIN+DBG_EXOTIC,("SaveAdminData: adding %ld entries from fTempGUIDMap as mapentry_tempidmap",fTempGUIDMap.size()));
    for (spos=fTempGUIDMap.begin();spos!=fTempGUIDMap.end();spos++) {
      modifyMap(mapentry_tempidmap, (*spos).second.c_str(), (*spos).first.c_str(), 0, false);
    }
    #endif
  }
  sta=apiSaveAdminData(aSessionFinished,aSuccessful);
  if (sta!=LOCERR_OK) {
    PDEBUGPRINTFX(DBG_ERROR,("SaveAdminData failed, err=%hd",sta));
  }
  return sta;
} // TCustomImplDS::SaveAdminData

#endif // not BASED_ON_BINFILE_CLIENT


// save end of session state
localstatus TCustomImplDS::implSaveEndOfSession(bool aUpdateAnchors)
{
  localstatus sta=LOCERR_OK;
  PDEBUGBLOCKCOLL("SaveEndOfSession");
  // update TCustomImplDS dsSavedAdmin variables (other levels have already updated their variables
  if (aUpdateAnchors) {
    if (!fRefreshOnly || fSlowSync) {
      // This was really a two-way sync or we implicitly know that
      // we are now in sync with remote (like after one-way-from-remote refresh = reload local)
      #ifndef BASED_ON_BINFILE_CLIENT
      // Note: in case of BASED_ON_BINFILE_CLIENT, these updates will be done by binfileds
      //       (also note that fPreviousToRemoteSyncCmpRef has different semantics in BASED_ON_BINFILE_CLIENT,
      //       as it serves as a last-changelog-update reference then)
      // But here, fPreviousToRemoteSyncCmpRef is what it seems - the timestamp corresponding to last sync to remote
      if (fConfigP->fSyncTimeStampAtEnd) {
        // if datastore cannot explicitly set modification timestamps, best time to save is current time
        fPreviousToRemoteSyncCmpRef = fAgentP->getDatabaseNowAs(TCTX_UTC);
      }
      else {
        // if datastore can set modification timestamps, best time to save is start of sync
        fPreviousToRemoteSyncCmpRef = fCurrentSyncTime;
      }
      #endif
      // also update opaque reference string eventually needed in DS API implementations
      fPreviousToRemoteSyncIdentifier = fCurrentSyncIdentifier;
    }
    // updating anchor means invalidating last Suspend
    fPreviousSuspendCmpRef = fPreviousToRemoteSyncCmpRef; // setting to current reference can do less harm than setting it to zero
    fPreviousSuspendIdentifier.erase();
  }
  #ifdef BASED_ON_BINFILE_CLIENT
  // if we sit on top of binfile, let binfile do the actual end-if-session work
  // (updates of cmprefs etc. are done at binfile level again).
  sta = inherited::implSaveEndOfSession(aUpdateAnchors);
  #else
  // save admin data now
  sta=SaveAdminData(true,aUpdateAnchors); // end of session
  // we can foget the maps now
  fMapTable.clear();
  #endif
  PDEBUGENDBLOCK("SaveEndOfSession");
  return sta;
} // TCustomImplDS::implSaveEndOfSession


// - end write with commit
bool TCustomImplDS::implEndDataWrite(void)
{
  localstatus sta=LOCERR_OK;
  TP_DEFIDX(li);
  TP_SWITCH(li,fSessionP->fTPInfo,TP_database);
  SYSYNC_TRY {
    // first make sure data writing ends (and obtain current sync identifier)
    sta = apiEndDataWrite(fCurrentSyncIdentifier);
  }
  SYSYNC_CATCH (exception &e)
    PDEBUGPRINTFX(DBG_ERROR,("******** implEndDataWrite exception: %s",e.what()));
    TP_START(fSessionP->fTPInfo,li);
    return false;
  SYSYNC_ENDCATCH
  TP_START(fSessionP->fTPInfo,li);
  #ifdef BASED_ON_BINFILE_CLIENT
  // binfile level must be called as well
  sta = inherited::implEndDataWrite();
  #endif
  return sta;
} // TCustomImplDS::implEndDataWrite


// delete sync set one by one
localstatus TCustomImplDS::zapSyncSet(void)
{
  TSyncSetList::iterator pos;
  localstatus sta;
  // - create dummy item
  TStatusCommand dummy(getSession());
  TMultiFieldItem *delitemP =
    static_cast<TMultiFieldItem *>(newItemForRemote(ity_multifield));
  delitemP->setSyncOp(sop_delete);
  PDEBUGPRINTFX(DBG_DATA,(
    "Zapping datastore: deleting %ld items from database",
    (long)fSyncSetList.size()
  ));
  for (pos=fSyncSetList.begin(); pos!=fSyncSetList.end(); ++pos) {
    // delete
    delitemP->setLocalID((*pos)->localid.c_str());
    sta = apiDeleteItem(*delitemP);
    // success or "211 - not deleted" is ok.
    if (sta!=LOCERR_OK && sta!=211) return sta;
  }
  delete delitemP;
  return LOCERR_OK; // zapped ok
} // TCustomImplDS::zapSyncSet


#ifndef BASED_ON_BINFILE_CLIENT

// - save status information required to eventually perform a resume (as passed to datastore with
//   implMarkOnlyUngeneratedForResume() and implMarkItemForResume())
//   (or, in case the session is really complete, make sure that no resume state is left)
localstatus TCustomImplDS::implSaveResumeMarks(void)
{
  PDEBUGBLOCKCOLL("SaveResumeMarks");
  // update anchoring info for resume
  if (fConfigP->fSyncTimeStampAtEnd) {
    // if datastore cannot explicitly set modification timestamps, best time to save is current time
    fPreviousSuspendCmpRef = fAgentP->getDatabaseNowAs(TCTX_UTC);
  }
  else {
    // if datastore can set modification timestamps, best time to save is start of sync
    fPreviousSuspendCmpRef = fCurrentSyncTime;
  }
  // also update opaque reference string eventually needed in DS API implementations
  fPreviousSuspendIdentifier = fCurrentSyncIdentifier;
  // save admin data now
  localstatus sta=SaveAdminData(false,false); // not end of session, not successful end either
  PDEBUGENDBLOCK("SaveResumeMarks");
  return sta;
} // TCustomImplDS::implSaveResumeMarks


#else // not BASED_ON_BINFILE_CLIENT


// Connecting methods when CustomImplDS is used on top of BinFileImplDS


bool TCustomImplDS::makeSyncSetLoaded(bool aNeedAll)
{
  localstatus sta = LOCERR_OK;

  if (!fSyncSetLoaded) {
    PDEBUGBLOCKFMTCOLL(("ReadSyncSet","Reading Sync Set from Database","datastore=%s",getName()));
    SYSYNC_TRY {
      sta = apiReadSyncSet(aNeedAll);
      PDEBUGENDBLOCK("ReadSyncSet");
    }
    SYSYNC_CATCH(exception &e)
      PDEBUGPRINTFX(DBG_ERROR,("makeSyncSetLoaded exception: %s",e.what()));
      sta=510;
      // end of DB read
      PDEBUGENDBLOCK("ReadSyncSet");
    SYSYNC_ENDCATCH
    if (sta==LOCERR_OK)
      fSyncSetLoaded=true; // is now loaded
  }
  return fSyncSetLoaded; // ok only if now loaded
} // TCustomImplDS::makeSyncSetLoaded


/// get first item's ID and modification status from the sync set
/// @return false if no item found
bool TCustomImplDS::getFirstItemInfo(localid_out_t &aLocalID, bool &aItemHasChanged)
{
  // reset the iterator
  fSyncSetPos = fSyncSetList.begin();
  // now get first item's info
  return getNextItemInfo(aLocalID, aItemHasChanged);
} // TCustomImplDS::getFirstItemInfo



/// get next item's ID and modification status from the sync set.
/// @return false if no item found
bool TCustomImplDS::getNextItemInfo(localid_out_t &aLocalID, bool &aItemHasChanged)
{
  if (!fSyncSetLoaded)
    return false; // no syncset, nothing to report
  if (fSyncSetPos!=fSyncSetList.end()) {
    // get the info
    TSyncSetItem *syncsetitemP = (*fSyncSetPos);
    // - ID
    STR_TO_LOCALID(syncsetitemP->localid.c_str(),aLocalID);
    // - changeflag
    aItemHasChanged = syncsetitemP->isModified;
    // advance to next item in sync set
    fSyncSetPos++;
    // ok
    return true;
  }
  // no more items
  return false;
} // TCustomImplDS::getNextItemInfo



/// get item by local ID from the sync set. Caller obtains ownership if aItemP is not NULL after return
/// @return != LOCERR_OK  if item with specified ID is not found.
localstatus TCustomImplDS::getItemByID(localid_t aLocalID, TSyncItem *&aItemP)
{
  if (!fSyncSetLoaded)
    return 510; // syncset should be loaded here!
  // search in syncset
  string localid;
  LOCALID_TO_STRING(aLocalID,localid);
  TSyncSetList::iterator syncsetpos = findInSyncSet(localid.c_str());
  if (syncsetpos==fSyncSetList.end())
    return 404; // not found
  // return item already fetched or fetch it if not fetched before
  TSyncSetItem *syncsetitemP = (*syncsetpos);
  if (syncsetitemP->itemP) {
    // already fetched - pass it to caller and remove link in syncsetitem
    aItemP = syncsetitemP->itemP;
    syncsetitemP->itemP = NULL; // syncsetitem does not own it any longer
  }
  else {
    // item not yet fetched (or already retrieved once), fetch it now
    // - create new empty TMultiFieldItem
    aItemP =
      (TMultiFieldItem *) newItemForRemote(ity_multifield);
    if (!aItemP)
      return 510;
    // - assign local id, as it is required by DoDataSubstitutions
    aItemP->setLocalID(syncsetitemP->localid.c_str());
    // - set default operation
    aItemP->setSyncOp(sop_replace);
    // Now fetch item (read phase)
    localstatus sta=apiFetchItem(*((TMultiFieldItem *)aItemP),true,syncsetitemP);
    if (sta!=LOCERR_OK)
      return sta; // error
  }
  // ok
  return LOCERR_OK;
} // TCustomImplDS::getItemByID


/* no need to implement this here, calling API level directly from binfile is enough
/// signal start of data write phase
localstatus TCustomImplDS::apiStartDataWrite(void);
*/

/* must not be implemented here, as TCustomImplDS also derives
   implEndDataWrite() which makes sure apiEndDataWrite(cmpRef) is called
   at the right time
/// signal end of data write phase
localstatus TCustomImplDS::apiEndDataWrite(void)
{
  // call customImplDS branch's version of apiEndDataWrite
  string thisSyncIdentifier;
  return apiEndDataWrite(thisSyncIdentifier);
} // TCustomImplDS::apiEndDataWrite
*/


/// update item by local ID in the sync set. Caller retains ownership of aItemP
/// @return != LOCERR_OK  if item with specified ID is not found.
localstatus TCustomImplDS::updateItemByID(localid_t aLocalID, TSyncItem *aItemP)
{
  if (!aItemP) return 510; // error
  if (!aItemP->isBasedOn(ity_multifield)) return 415; // must be multifield item
  TMultiFieldItem *myItemP = static_cast<TMultiFieldItem *>(aItemP);
  // - assign localid
  string localid;
  LOCALID_TO_STRING(aLocalID,localid);
  myItemP->setLocalID(localid.c_str());
  // have API handle it
  localstatus sta = apiUpdateItem(*myItemP);
  if (sta==LOCERR_OK) {
    // updated ok
    // - save what is needed for finalisation
    if (fNeedFinalisation) {
      queueForFinalisation(myItemP);
    }		
  }
  return sta;
} // TCustomImplDS::updateItemByID


/// delete item by local ID in the sync set.
/// @return != LOCERR_OK if item with specified ID is not found.
localstatus TCustomImplDS::deleteItemByID(localid_t aLocalID)
{
  // create new dummy TMultiFieldItem
  TMultiFieldItem *myItemP =
    (TMultiFieldItem *) newItemForRemote(ity_multifield);
  // assign localid
  string localid;
  LOCALID_TO_STRING(aLocalID,localid);
  myItemP->setLocalID(localid.c_str());
  // have API delete it
  localstatus sta = apiDeleteItem(*myItemP);
  delete myItemP; // delete dummy item
  // return status
  return sta;
} // TCustomImplDS::deleteItemByID


/// create new item in the sync set. Caller retains ownership of aItemP.
/// @return LOCERR_OK or error code.
/// @param[out] aNewLocalID local ID assigned to new item
/// @param[out] aReceiveOnly is set to true if local changes/deletion of this item should not be
///   reported to the server in normal syncs.
localstatus TCustomImplDS::createItem(TSyncItem *aItemP,localid_out_t &aNewLocalID, bool &aReceiveOnly)
{
  if (!aItemP) return 510; // error
  if (!aItemP->isBasedOn(ity_multifield)) return 415; // must be multifield item
  TMultiFieldItem *myItemP = static_cast<TMultiFieldItem *>(aItemP);
  // add it to the database
  string newLocalID;
  localstatus sta = apiAddItem(*myItemP,newLocalID);
  // return assigned ID
  STR_TO_LOCALID(newLocalID.c_str(),aNewLocalID);
  if (sta==LOCERR_OK) {
    // added ok
    // - save what is needed for finalisation
    if (fNeedFinalisation) {
      myItemP->setLocalID(newLocalID.c_str()); // finalisation needs to know the local ID
      queueForFinalisation(myItemP);
    }		
  }
  // so far, we don't have receive-only items
  aReceiveOnly = false;
  // return status
  return sta;
} // TCustomImplDS::createItem


/// zaps the entire datastore, returns LOCERR_OK if ok
/// @return LOCERR_OK or error code.
localstatus TCustomImplDS::zapDatastore(void)
{
  // make sure we have the sync set if we need it to zap it
  if (apiNeedSyncSetToZap()) {
    // make sure we have the sync set
    if (!makeSyncSetLoaded(false)) return 510; // error
  }
  // Zap the sync set in this datastore (will eventually call zapSyncSet)
  return apiZapSyncSet();
} // TCustomImplDS::zapDatastore


/// get error code for last routine call that returned !=LOCERR_OK
/// @return platform specific DB error code
uInt32 TCustomImplDS::lastDBError(void)
{
  return 0; // none available
} // TCustomImplDS::lastDBError


#endif // BASED_ON_BINFILE_CLIENT

/* end of TCustomImplDS implementation */

} // namespace

// eof