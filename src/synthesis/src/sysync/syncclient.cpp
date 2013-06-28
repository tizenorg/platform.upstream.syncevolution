/*
 *  File:         SyncClient.cpp
 *
 *  Author:       Lukas Zeller (luz@synthesis.ch)
 *
 *  TSyncClient
 *    Provides functionality required for a Synchronisation
 *    client.
 *
 *  Copyright (c) 2002-2009 by Synthesis AG (www.synthesis.ch)
 *
 *  2002-05-06 : luz : created from old SyncClient.h (which was a
 *                     intermediate class between server and session,
 *                     as initially we considered a client-server
 *                     combined class. Now both SyncClient and
 *                     SyncServer are directly derived from SyncSession
 *
 */


// includes
#include "prefix_file.h"
#include "sysync.h"
#include "syncclient.h"
#include "syncappbase.h"

#ifdef HARD_CODED_SERVER_URI
  #include "syserial.h"
#endif

#ifndef SYSYNC_CLIENT
  #error "SYSYNC_CLIENT must be defined when compiling syncclient.cpp"
#endif



namespace sysync {


#ifdef PRECONFIGURED_SYNCREQUESTS

// Implementation of TSyncReqConfig
// ================================

// config for databases to sync with
TSyncReqConfig::TSyncReqConfig(TLocalDSConfig *aLocalDSCfg, TConfigElement *aParentElement) :
  TConfigElement("syncrequest",aParentElement),
  fLocalDSConfig(aLocalDSCfg)
{
  clear();
} // TSyncReqConfig::TSyncReqConfig


TSyncReqConfig::~TSyncReqConfig()
{
  // nop so far
} // TSyncReqConfig::~TSyncReqConfig


// init defaults
void TSyncReqConfig::clear(void)
{
  // init defaults
  // - local client datatstore subselection path or CGI (such as "test" in "contact/test")
  fLocalPathExtension.erase();
  // - remote server DB layer auth
  fDBUser.erase();
  fDBPassword.erase();
  // - remote server datastore path
  fServerDBPath.erase();
  // - sync mode
  fSyncMode=smo_twoway;
  fSlowSync=false; // default to non-slow
  // - DS 1.2 filtering parameters
  fRecordFilterQuery.erase();
  fFilterInclusive=false;
  // clear inherited
  inherited::clear();
} // TSyncReqConfig::clear


// config element parsing
bool TSyncReqConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  if (strucmp(aElementName,"localpathextension")==0)
    expectString(fLocalPathExtension);
  else if (strucmp(aElementName,"dbuser")==0)
    expectString(fDBUser);
  else if (strucmp(aElementName,"dbpassword")==0)
    expectString(fDBPassword);
  else if (strucmp(aElementName,"dbpath")==0)
    expectString(fServerDBPath);
  else if (strucmp(aElementName,"syncmode")==0)
    expectEnum(sizeof(fSyncMode),&fSyncMode,SyncModeNames,numSyncModes);
  else if (strucmp(aElementName,"slowsync")==0)
    expectBool(fSlowSync);
  else if (strucmp(aElementName,"recordfilter")==0)
    expectString(fRecordFilterQuery);
  else if (strucmp(aElementName,"filterinclusive")==0)
    expectBool(fFilterInclusive);

  // - none known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TSyncReqConfig::localStartElement


// resolve
void TSyncReqConfig::localResolve(bool aLastPass)
{
  if (aLastPass) {
    // check for required settings
    // %%% tbd
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TSyncReqConfig::localResolve


// create appropriate type of local datastore from config and init sync parameters
TLocalEngineDS *TSyncReqConfig::initNewLocalDataStore(TSyncSession *aSessionP)
{
  // - create appropriate type of localdsP
  TLocalEngineDS *localdsP = fLocalDSConfig->newLocalDataStore(aSessionP);
  // - set parameters
  localdsP->dsSetClientSyncParams(
    fSyncMode,
    fSlowSync,
    fServerDBPath.c_str(),
    fDBUser.c_str(),
    fDBPassword.c_str(),
    fLocalPathExtension.c_str(),
    fRecordFilterQuery.c_str(),
    fFilterInclusive
  );
  return localdsP;
} // TSyncReqConfig::initNewLocalDataStore

#endif // PRECONFIGURED_SYNCREQUESTS


// Implementation of TClientConfig
// ===============================


TClientConfig::TClientConfig(const char* aName, TConfigElement *aParentElement) :
  TSessionConfig(aName,aParentElement)
{
  clear();
} // TClientConfig::TClientConfig


TClientConfig::~TClientConfig()
{
  clear();
} // TClientConfig::~TClientConfig


// init defaults
void TClientConfig::clear(void)
{
  // init auth defaults (note that these MUST correspond with the defaults set by loadRemoteParams() !!!
  fAssumedServerAuth=auth_none; // start with no auth
  fAssumedServerAuthEnc=fmt_chr; // start with char encoding
  fAssumedNonce.erase(); // start with no nonce
  // auth retry options (mainly for stupid servers like SCTS)
  #ifdef SCTS_COMPATIBILITY_HACKS
  fNewSessionForAuthRetry=false;
  fNoRespURIForAuthRetry=false;
  #else
  fNewSessionForAuthRetry=true; // all production Synthesis clients had it hardcoded (ifdeffed) this way until 2.9.8.7
  fNoRespURIForAuthRetry=true; // all production Synthesis clients had it hardcoded (ifdeffed) this way until 2.9.8.7
  #endif
  fSmartAuthRetry=true; // try to be smart and try different auth retry (different from fNewSessionForAuthRetry/fNoRespURIForAuthRetry) if first attempts fail
  // other defaults
  fPutDevInfAtSlowSync=true; // smartner server needs it, and it does not harm so we have it on by default
  #ifndef NO_LOCAL_DBLOGIN
  fLocalDBUser.erase();
  fLocalDBPassword.erase();
  fNoLocalDBLogin=false;
  #endif
  #ifdef PRECONFIGURED_SYNCREQUESTS
  fEncoding=SML_XML; // default to more readable XML
  fServerUser.erase();
  fServerPassword.erase();
  fServerURI.erase();
  fTransportUser.erase();
  fTransportPassword.erase();
  fSocksHost.erase();
  fProxyHost.erase();
  fProxyUser.erase();
  fProxyPassword.erase();
  // remove sync db specifications
  TSyncReqList::iterator pos;
  for(pos=fSyncRequests.begin();pos!=fSyncRequests.end();pos++)
    delete *pos;
  fSyncRequests.clear();
  #endif
  // clear inherited
  TSessionConfig::clear();
  // modify timeout after inherited sets it
  fSessionTimeout=DEFAULT_CLIENTSESSIONTIMEOUT;
  // SyncML version support
  fAssumedServerVersion=MAX_SYNCML_VERSION; // try with highest version we support
  fMaxSyncMLVersionSupported=MAX_SYNCML_VERSION; // support what we request (overrides session default)
} // TClientConfig::clear


#ifndef HARDCODED_CONFIG

// config element parsing
bool TClientConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  // - defaults for starting a session
  if (strucmp(aElementName,"defaultsyncmlversion")==0)
    expectEnum(sizeof(fAssumedServerVersion),&fAssumedServerVersion,SyncMLVersionNames,numSyncMLVersions);
  else if (strucmp(aElementName,"defaultauth")==0)
    expectEnum(sizeof(fAssumedServerAuth),&fAssumedServerAuth,authTypeNames,numAuthTypes);
  else if (strucmp(aElementName,"defaultauthencoding")==0)
    expectEnum(sizeof(fAssumedServerAuthEnc),&fAssumedServerAuthEnc,encodingFmtSyncMLNames,numFmtTypes);
  else if (strucmp(aElementName,"defaultauthnonce")==0)
    expectString(fAssumedNonce);
  else if (strucmp(aElementName,"newsessionforretry")==0)
    expectBool(fNewSessionForAuthRetry);
  else if (strucmp(aElementName,"originaluriforretry")==0)
    expectBool(fNoRespURIForAuthRetry);
  else if (strucmp(aElementName,"smartauthretry")==0)
    expectBool(fSmartAuthRetry);
  // - other options
  else if (strucmp(aElementName,"putdevinfatslowsync")==0)
    expectBool(fPutDevInfAtSlowSync);
  else if (strucmp(aElementName,"fakedeviceid")==0)
    expectString(fFakeDeviceID);
  else
  #ifndef NO_LOCAL_DBLOGIN
  if (strucmp(aElementName,"localdbuser")==0)
    expectString(fLocalDBUser);
  else if (strucmp(aElementName,"localdbpassword")==0)
    expectString(fLocalDBPassword);
  else if (strucmp(aElementName,"nolocaldblogin")==0)
    expectBool(fNoLocalDBLogin);
  else
  #endif
  // serverURL is always available to allow define fixed URL in config that can't be overridden in profiles
  if (strucmp(aElementName,"serverurl")==0)
    expectString(fServerURI);
  else
  #ifdef PRECONFIGURED_SYNCREQUESTS
  if (strucmp(aElementName,"syncmlencoding")==0)
    expectEnum(sizeof(fEncoding),&fEncoding,SyncMLEncodingNames,numSyncMLEncodings);
  else if (strucmp(aElementName,"serveruser")==0)
    expectString(fServerUser);
  else if (strucmp(aElementName,"serverpassword")==0)
    expectString(fServerPassword);
  else if (strucmp(aElementName,"sockshost")==0)
    expectString(fSocksHost);
  else if (strucmp(aElementName,"proxyhost")==0)
    expectString(fProxyHost);
  else if (strucmp(aElementName,"proxyuser")==0)
    expectString(fProxyUser);
  else if (strucmp(aElementName,"proxypassword")==0)
    expectString(fProxyPassword);
  else if (strucmp(aElementName,"transportuser")==0)
    expectString(fTransportUser);
  else if (strucmp(aElementName,"transportpassword")==0)
    expectString(fTransportPassword);
  // - Sync DB specification
  else if (strucmp(aElementName,"syncrequest")==0) {
    // definition of a new datastore
    const char* nam = getAttr(aAttributes,"datastore");
    if (!nam) {
      ReportError(true,"syncrequest missing 'datastore' attribute");
    }
    else {
      // search datastore
      TLocalDSConfig *localDSCfgP = getLocalDS(nam);
      if (!localDSCfgP)
        return fail("unknown local datastore '%s' specified",nam);
      // create new syncDB config linked to that datastore
      TSyncReqConfig *syncreqcfgP = new TSyncReqConfig(localDSCfgP,this);
      // - save in list
      fSyncRequests.push_back(syncreqcfgP);
      // - let element handle parsing
      expectChildParsing(*syncreqcfgP);
    }
  }
  else
  #endif
  // - none known here
    return TSessionConfig::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TClientConfig::localStartElement

#endif

// resolve
void TClientConfig::localResolve(bool aLastPass)
{
  if (aLastPass) {
    #ifdef PRECONFIGURED_SYNCREQUESTS
    // - resolve requests
    TSyncReqList::iterator pos;
    for(pos=fSyncRequests.begin();pos!=fSyncRequests.end();pos++)
      (*pos)->Resolve(aLastPass);
    #endif
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TClientConfig::localResolve



// Implementation of TSyncClient
// =============================


// constructor
TSyncClient::TSyncClient(
  TSyncClientBase *aSyncClientBaseP,
  cAppCharP aSessionID // a session ID
) :
  TSyncSession(aSyncClientBaseP,aSessionID)
  #ifdef ENGINEINTERFACE_SUPPORT
  ,fEngineState(ces_idle)
  #endif
{
  #ifdef HARD_CODED_SERVER_URI
  fNoCRCPrefixLen=0;
  #endif
  // reset session now to get correct initial state
  InternalResetSession();
  // restart with session numbering at 1 (incremented before use)
  fClientSessionNo=0;
} // TSyncClient::TSyncClient


// destructor
TSyncClient::~TSyncClient()
{
  // make sure everything is terminated BEFORE destruction of hierarchy begins
  TerminateSession();
} // TSyncClient::~TSyncClient


// Terminate session
void TSyncClient::TerminateSession()
{
  if (!fTerminated) {
    InternalResetSession();
    #ifdef ENGINEINTERFACE_SUPPORT
    // switch state to done to prevent any further activity via SessionStep()
    fEngineState = ces_done;
    #endif
  }
  inherited::TerminateSession();
} // TSyncClient::TerminateSession



#ifdef ENGINEINTERFACE_SUPPORT

// Support for EngineModule common interface
// -----------------------------------------


/// @brief Executes next step of the session
/// @param aStepCmd[in/out] step command (STEPCMD_xxx):
///        - tells caller to send or receive data or end the session etc.
///        - instructs engine to suspend or abort the session etc.
/// @param aInfoP[in] pointer to a TEngineProgressInfo structure, NULL if no progress info needed
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TSyncClient::SessionStep(uInt16 &aStepCmd, TEngineProgressInfo *aInfoP)
{
  uInt16 stepCmdIn = aStepCmd;
  localstatus sta = LOCERR_WRONGUSAGE;

  // init default response
  aStepCmd = STEPCMD_ERROR; // error
  if (aInfoP) {
    aInfoP->eventtype=PEV_NOP;
    aInfoP->targetID=0;
    aInfoP->extra1=0;
    aInfoP->extra2=0;
    aInfoP->extra3=0;
  }

  // if session is already aborted, no more steps are required
  if (isAborted()) {
  	fEngineState = ces_done; // we are done
  }

  // handle pre-processed step command according to current engine state
  switch (fEngineState) {

    // Idle state
    case ces_done :
      // session done, nothing happens any more
      aStepCmd = STEPCMD_DONE;
      sta = LOCERR_OK;
      break;

    case ces_idle :
      // in idle, we can only start a session
      switch (stepCmdIn) {
        case STEPCMD_CLIENTSTART:
        case STEPCMD_CLIENTAUTOSTART:
          // initialize a new session
          sta = InitializeSession(fProfileSelectorInternal,stepCmdIn==STEPCMD_CLIENTAUTOSTART);
          if (sta!=LOCERR_OK) break;
          // engine is now ready, start generating first request
          fEngineState = ces_generating;
          // ok with no status
          aStepCmd = STEPCMD_OK;
          break;
      } // switch stepCmdIn for ces_idle
      break;

    // Ready for generation steps
    case ces_generating:
      switch (stepCmdIn) {
        case STEPCMD_STEP :
          sta = generatingStep(aStepCmd,aInfoP);
          break;
      } // switch stepCmdIn for ces_generating
      break;

    // Ready for processing steps
    case ces_processing:
      switch (stepCmdIn) {
        case STEPCMD_STEP :
          sta = processingStep(aStepCmd,aInfoP);
          break;
      } // switch stepCmdIn for ces_processing
      break;

    // Waiting for SyncML data
    case ces_needdata:
      switch (stepCmdIn) {
        case STEPCMD_GOTDATA :
          // got data, now start processing it
          OBJ_PROGRESS_EVENT(getSyncAppBase(),pev_recvend,NULL,0,0,0);
          fIgnoreMsgErrs=false; // do not ignore errors by default
          fEngineState = ces_processing;
          aStepCmd = STEPCMD_OK;
          sta = LOCERR_OK;
          break;
      } // switch stepCmdIn for ces_processing
      break;

    // Waiting until SyncML data is sent
    case ces_dataready:
      switch (stepCmdIn) {
        case STEPCMD_SENTDATA :
          // sent data, now request answer data
          OBJ_PROGRESS_EVENT(getSyncAppBase(),pev_sendend,NULL,0,0,0);
          fEngineState = ces_needdata;
          aStepCmd = STEPCMD_NEEDDATA;
          sta = LOCERR_OK;
          break;
      } // switch stepCmdIn for ces_processing
      break;

  case numClientEngineStates:
      // invalid
      break;

  } // switch fEngineState

  // done
  return sta;
} // TSyncClient::SessionStep



// Step that generates SyncML data
TSyError TSyncClient::generatingStep(uInt16 &aStepCmd, TEngineProgressInfo *aInfoP)
{
  localstatus sta = LOCERR_WRONGUSAGE;
  bool done;

  //%%% at this time, generate next message in one step
  sta = NextMessage(done);
  if (done) {
    // done with session, with or without error
    fEngineState = ces_done; // blocks any further activity with the session
    aStepCmd = STEPCMD_DONE;
    // terminate session to provoke all end-of-session progress events
    TerminateSession();
  }
  else if (sta==LOCERR_OK) {
    // next is sending request to server
    fEngineState = ces_dataready;
    aStepCmd = STEPCMD_SENDDATA;
    OBJ_PROGRESS_EVENT(getSyncAppBase(),pev_sendstart,NULL,0,0,0);
  }
  // return status
  return sta;
} // TSyncClient::generatingStep



// Step that processes SyncML data
TSyError TSyncClient::processingStep(uInt16 &aStepCmd, TEngineProgressInfo *aInfoP)
{
  InstanceID_t myInstance = getSmlWorkspaceID();
  Ret_t rc;
  localstatus sta = LOCERR_WRONGUSAGE;

  // now process next command
  PDEBUGPRINTFX(DBG_EXOTIC,("Calling smlProcessData(NEXT_COMMAND)"));
  #ifdef SYDEBUG
  MemPtr_t data = NULL;
  MemSize_t datasize;
  smlPeekMessageBuffer(getSmlWorkspaceID(), false, &data, &datasize);
  #endif  
  rc=smlProcessData(
    myInstance,
    SML_NEXT_COMMAND
  );
  if (rc==SML_ERR_CONTINUE) {
    // processed ok, but message not completely processed yet
    // - engine state remains as is
    aStepCmd = STEPCMD_OK; // ok w/o progress %%% for now, progress is delivered via queue in next step
    sta = LOCERR_OK;
  }
  else if (rc==SML_ERR_OK) {
    // message completely processed
    // - switch engine state to generating next message (if any)
    aStepCmd = STEPCMD_OK;
    fEngineState = ces_generating;
    sta = LOCERR_OK;
  }
  else {
    // processing failed
    PDEBUGPRINTFX(DBG_ERROR,("===> smlProcessData failed, returned 0x%hX",(sInt16)rc));
    // dump the message that failed to process
    #ifdef SYDEBUG
    if (data) DumpSyncMLBuffer(data,datasize,false,rc);
    #endif    
    if (!fIgnoreMsgErrs) {
	    // abort the session (causing proper error events to be generated and reported back)
     	AbortSession(LOCERR_PROCESSMSG, true);
      // session is now done
      fEngineState = ces_done;
    }
    else {
    	// we must ignore errors e.g. because of session restart and go back to generate next message
      fEngineState = ces_generating;
    }
    // anyway, step by itself is ok - let app continue stepping (to restart session or complete abort)
    aStepCmd = STEPCMD_OK;
    sta = LOCERR_OK;
  }
  // now check if this is a session restart
  if (sta==LOCERR_OK && isStarting()) {
    // this is still the beginning of a session, which means
    // that we are restarting the session and caller should close
    // eventually open communication with the server before sending the next message
    aStepCmd = STEPCMD_RESTART;
  }
  // done
  return sta;
} // TSyncClient::processingStep

#endif // ENGINEINTERFACE_SUPPORT




void TSyncClient::InternalResetSession(void)
{
  // use remote URI as specified to start a session
  fRespondURI = fRemoteURI;
  #ifdef HARD_CODED_SERVER_URI
  #if defined(CUSTOM_URI_SUFFIX) && !defined(HARD_CODED_SERVER_URI_LEN)
  #error "HARD_CODED_SERVER_URI_LEN must be defined when using CUSTOM_URI_SUFFIX"
  #endif
  #ifdef HARD_CODED_SERVER_URI_LEN
  // only part of URL must match (max HARD_CODED_SERVER_URI_LEN chars will be added, less if URI is shorter)
  fServerURICRC = addNameToCRC(SYSER_CRC32_SEED, fRemoteURI.c_str()+fNoCRCPrefixLen, false, HARD_CODED_SERVER_URI_LEN);
  #else
  // entire URL (except prefix) must match
  fServerURICRC = addNameToCRC(SYSER_CRC32_SEED, fRemoteURI.c_str()+fNoCRCPrefixLen, false);
  #endif
  #endif
  // set SyncML version
  // Note: will be overridden with call to loadRemoteParams()
  fSyncMLVersion = syncml_vers_unknown; // unknown
  // will be cleared to suppress automatic use of DS 1.2 SINCE/BEFORE filters
  // (e.g. for date range in func_SetDaysRange())
  fServerHasSINCEBEFORE = true;
} // TSyncClient::InternalResetSession


// Virtual version
void TSyncClient::ResetSession(void)
{
  // let ancestor do its stuff
  TSyncSession::ResetSession();
  // do my own stuff (and probably modify settings of ancestor!)
  InternalResetSession();
} // TSyncClient::ResetSession



// initialize the client session and link it with the SML toolkit
localstatus TSyncClient::InitializeSession(uInt32 aProfileSelector, bool aAutoSyncSession)
{
  localstatus sta;

  // Select profile now (before creating instance, as encoding is dependent on profile)
  sta=SelectProfile(aProfileSelector, aAutoSyncSession);
  if (sta) return sta;
  // Start a SyncML toolkit instance now and set the encoding from config
  InstanceID_t myInstance;
  if (!getSyncAppBase()->newSmlInstance(
    getEncoding(),
    getRootConfig()->fLocalMaxMsgSize * 2, // twice the message size
    myInstance
  )) {
    return LOCERR_SMLFATAL;
  }
  // let toolkit know the session pointer
  if (getSyncAppBase()->setSmlInstanceUserData(myInstance,this)!=SML_ERR_OK) // toolkit must know session (as userData)
    return LOCERR_SMLFATAL;
  // remember the instance myself
  setSmlWorkspaceID(myInstance); // session must know toolkit workspace
  // done
  return LOCERR_OK;
} // TSyncClient::InitializeSession



// select a profile (returns false if profile not found)
// Note: base class just tries to retrieve information from
//       config
localstatus TSyncClient::SelectProfile(uInt32 aProfileSelector, bool aAutoSyncSession)
{
  #ifndef PRECONFIGURED_SYNCREQUESTS
  // no profile settings in config -> error
  return LOCERR_NOCFG;
  #else
  // get profile settings from config
  TClientConfig *configP = static_cast<TClientConfig *>(getRootConfig()->fAgentConfigP);
  // - get encoding
  fEncoding=configP->fEncoding; // SyncML encoding
  // - set server access details
  fRemoteURI=configP->fServerURI; // Remote URI = Server URI
  fTransportUser=configP->fTransportUser; // transport layer user (e.g. HTTP auth)
  fTransportPassword=configP->fTransportPassword; // transport layer password (e.g. HTTP auth)
  fServerUser=configP->fServerUser; // Server layer authentification user name
  fServerPassword=configP->fServerPassword; // Server layer authentification password
  #ifndef NO_LOCAL_DBLOGIN
  fLocalDBUser=configP->fLocalDBUser; // Local DB authentification user name (empty if local DB is single user)
  fNoLocalDBLogin=configP->fNoLocalDBLogin; // if set, no local DB auth takes place, but fLocalDBUser is used as userkey (depending on DB implementation)
  fLocalDBPassword=configP->fLocalDBPassword; // Local DB authentification password
  #endif
  fProxyHost=configP->fProxyHost; // Proxy host
  fSocksHost=configP->fSocksHost; // Socks host
  fProxyUser=configP->fProxyUser;
  fProxyPassword=configP->fProxyPassword;
  // Reset session after profile change
  // and also remove any datastores we might have
  ResetAndRemoveDatastores();
  // - create and init datastores needed for this session from config
  //   Note: probably config has no sync requests, but they are created later
  //         programmatically
  TSyncReqList::iterator pos;
  for (pos=configP->fSyncRequests.begin(); pos!=configP->fSyncRequests.end(); pos++) {
    // create and init the datastore
    fLocalDataStores.push_back(
      (*pos)->initNewLocalDataStore(this)
    );
  }
  // create "new" session ID (derivates will do this better)
  fClientSessionNo++;
  return LOCERR_OK;
  #endif
} // TSyncClient::SelectProfile


// make sure we are logged in to local datastore
localstatus TSyncClient::LocalLogin(void)
{
  #ifndef NO_LOCAL_DBLOGIN
  if (!fNoLocalDBLogin && !fLocalDBUser.empty()) {
    // check authorisation (login to correct user) in local DB
    if (!SessionLogin(fLocalDBUser.c_str(),fLocalDBPassword.c_str(),sectyp_clearpass,fRemoteURI.c_str())) {
      return localError(401); // done & error
    }
  }
  #endif
  return LOCERR_OK;
} // TSyncClient::LocalLogin


// starting with engine version 2.0.8.7 a client's device ID (in devinf) is no longer
// a constant string, but the device's unique ID
string TSyncClient::getDeviceID(void)
{
  if (fLocalURI.empty())
    return SYSYNC_CLIENT_DEVID; // return default ID
  else
    return fLocalURI;
} // TSyncClient::getDeviceID

string TSyncClient::getDeviceType(void)
{
  // taken from configuration or SYSYNC_CLIENT_DEVTYP
  return getSyncAppBase()->getDevTyp();
}



// process message in the instance buffer
localstatus TSyncClient::processAnswer(void)
{
  InstanceID_t myInstance = getSmlWorkspaceID();
  Ret_t err;

  // now process data
  DEBUGPRINTF(("===> now calling smlProcessData"));
  #ifdef SYDEBUG
  MemPtr_t data = NULL;
  MemSize_t datasize;
  smlPeekMessageBuffer(getSmlWorkspaceID(), false, &data, &datasize);
  #endif  
  fIgnoreMsgErrs=false;
  err=smlProcessData(
    myInstance,
    SML_ALL_COMMANDS
  );
  if (err) {
    // dump the message that failed to process
    #ifdef SYDEBUG
    if (data) DumpSyncMLBuffer(data,datasize,false,err);
    #endif    
  	if (!fIgnoreMsgErrs) {
      PDEBUGPRINTFX(DBG_ERROR,("===> smlProcessData failed, returned 0x%hX",(sInt16)err));
      // other problem or already using SyncML 1.0 --> error
      return LOCERR_PROCESSMSG;
    }
  }
  // now check if this is a session restart
  if (isStarting()) {
    // this is still the beginning of a session
    return LOCERR_SESSIONRST;
  }
  return LOCERR_OK;
} // TSyncClientBase::processAnswer



// let session produce (or finish producing) next message into
// SML workspace
// - returns aDone if no answer needs to be sent (=end of session)
// - returns 0 if successful
// - returns SyncML status code if unsucessfully aborted session
localstatus TSyncClient::NextMessage(bool &aDone)
{
  TLocalDataStorePContainer::iterator pos;
  TSyError status;

  TP_START(fTPInfo,TP_general); // could be new thread
  // default to not continuing
  aDone=true;
  #ifdef PROGRESS_EVENTS
  // check for user suspend
  if (!getSyncAppBase()->NotifyProgressEvent(pev_suspendcheck)) {
    SuspendSession(LOCERR_USERSUSPEND);
  }
  #endif
  // done if session was aborted by last received commands
  if (isAborted()) return getAbortReasonStatus(); // done & error
  // check package state
  if (fOutgoingState==psta_idle) {
    // if suspended here, we'll just stop - nothing has happened yet
    if (isSuspending()) {
      AbortSession(fAbortReasonStatus,true);
      return getAbortReasonStatus();
    }
    // start of an entirely new client session
    #ifdef HARD_CODED_SERVER_URI
    // extra check to limit hacking
    if (fServerURICRC != SERVER_URI_CRC) {
      // someone has tried to change the URI
      DEBUGPRINTFX(DBG_ERROR,("hardcoded Server URI CRC mismatch"));
      return LOCERR_LIMITED; // user will not know what this means, but we will
    }
    #endif
    // - check if we have client requests
    if (fLocalDataStores.size()<1) {
      PDEBUGPRINTFX(DBG_ERROR,("No datastores defined to sync with"));
      return LOCERR_NOCFG;
    }
    // %%% later, we could probably load cached info about
    //     server requested auth, devinf etc. here
    // use remote URI as specified to start a session
    fRespondURI=fRemoteURI;
    // get default params for sending first message to remote
    // Note: may include remote flag settings that influence creation of my own ID below, that's why we do it now here
    loadRemoteParams();
    // get info about my own URI, whatever that is
    #ifndef HARDCODED_CONFIG
    if (!static_cast<TClientConfig *>(getRootConfig()->fAgentConfigP)->fFakeDeviceID.empty()) {
      // return fake Device ID if we have one defined in the config file (useful for testing)
      fLocalURI = static_cast<TClientConfig *>(getRootConfig()->fAgentConfigP)->fFakeDeviceID;
    }
    else
    #endif
    {
      if (!getLocalDeviceID(fLocalURI) || devidWithUserHash()) {
        // Device ID is not really unique, make a hash including user name to make it pseudo-unique
        // create MD5 hash from non-unique ID and user name
        // Note: when compiled with GUARANTEED_UNIQUE_DEVICID, devidWithUserHash() is always false.
        md5::SYSYNC_MD5_CTX context;
        uInt8 digest[16]; // for MD5 digest
        md5::Init (&context);
        // - add what we got for ID
        md5::Update (&context, (uInt8 *)fLocalURI.c_str(), fLocalURI.size());
        // - add user name, if any
        if (fLocalURI.size()>0) md5::Update (&context, (uInt8 *)fServerUser.c_str(), fServerUser.size());
        // - done
        md5::Final (digest, &context);
        // now make hex string of that
        fLocalURI = devidWithUserHash() ? 'x' : 'X'; // start with X to document this special case (lowercase = forced by remoteFlag)
        for (int n=0; n<16; n++) {
          AppendHexByte(fLocalURI,digest[n]);
        }
      }
    }
    // get my own name (if any)
    getPlatformString(pfs_device_name,fLocalName);
    // override some of these if not set by loadRemoteParams()
    if (fSyncMLVersion==syncml_vers_unknown)
      fSyncMLVersion=static_cast<TClientConfig *>(getRootConfig()->fAgentConfigP)->fAssumedServerVersion;
    if (fRemoteRequestedAuth==auth_none)
      fRemoteRequestedAuth=static_cast<TClientConfig *>(getRootConfig()->fAgentConfigP)->fAssumedServerAuth;
    if (fRemoteRequestedAuthEnc==fmt_chr)
      fRemoteRequestedAuthEnc=static_cast<TClientConfig *>(getRootConfig()->fAgentConfigP)->fAssumedServerAuthEnc;
    if (fRemoteNonce.empty())
      fRemoteNonce=static_cast<TClientConfig *>(getRootConfig()->fAgentConfigP)->fAssumedNonce;

    // we are not yet authenticated for the entire session
    fNeedAuth=true;
    // now ready for init
    fOutgoingState=psta_init; // %%%% could also set psta_initsync for combined init/sync
    fIncomingState=psta_idle; // remains idle until first answer SyncHdr with OK status is received
    fInProgress=true; // assume in progress
    // set session ID string
    StringObjPrintf(fSynchdrSessionID,"%hd",(sInt16)fClientSessionNo);
    // now we have a session id, can now display debug stuff
    #ifdef SYDEBUG
    string t;
    StringObjTimestamp(t,getSystemNowAs(TCTX_SYSTEM));
    PDEBUGPRINTFX(DBG_HOT,("\n[%s] =================> Starting new client session",t.c_str()));
    #endif
    // - make sure we are logged into the local database (if needed)
    status=LocalLogin();
    if (status!=LOCERR_OK) return status;
    // create header for first message no noResp
    issueHeader(false);
  }
  else {
    // check for proper end of session (caused by MessageEnded analysis)
    if (!fInProgress) {
      // end sync in all datastores (save anchors etc.)
      PDEBUGPRINTFX(DBG_PROTO,("Successful end of session -> calling engFinishDataStoreSync() for datastores now"));
      for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos)
        (*pos)->engFinishDataStoreSync(); // successful end
      PDEBUGPRINTFX(DBG_PROTO,("Session not any more in progress: NextMessage() returns OK status=0"));
      return LOCERR_OK; // done & ok
    }
  }
  // check expired case
  #ifdef APP_CAN_EXPIRE
  if (getClientBase()->fAppExpiryStatus!=LOCERR_OK) {
    PDEBUGPRINTFX(DBG_ERROR,("Evaluation Version expired - Please contact Synthesis AG for release version"));
    return getClientBase()->fAppExpiryStatus; // payment required, done & error
  }
  #endif
  if (fOutgoingState==psta_init || fOutgoingState==psta_initsync) {
    // - if suspended in init, nothing substantial has happened already, so just exit
    if (isSuspending() && fOutgoingState==psta_init) {
      AbortSession(fAbortReasonStatus,true);
      return getAbortReasonStatus();
    }
    // - prepare Alert(s) for databases to sync
    bool anyfirstsyncs=false;
    bool anyslowsyncs=false;
    TLocalEngineDS *localDS;
    for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
      // prepare alert
      localDS = *pos;
      status=localDS->engPrepareClientSyncAlert(NULL); // not as superdatastore
      if (status!=LOCERR_OK) {
        // local database error
        return localError(status); // not found
      }
      if (localDS->fFirstTimeSync) anyfirstsyncs=true;
      if (localDS->fSlowSync) anyslowsyncs=true;
    }
    // send devinf in Put command right away with init message if either...
    // - mustSendDevInf() returns true signalling an external condition that suggests sending devInf (like changed config)
    // - any datastore is doing first time sync
    // - fPutDevInfAtSlowSync is true and any datastore is doing slow sync
    if (
    	mustSendDevInf() ||
      anyfirstsyncs ||
      (anyslowsyncs && static_cast<TClientConfig *>(getRootConfig()->fAgentConfigP)->fPutDevInfAtSlowSync)
    ) {
      TDevInfPutCommand *putcmdP = new TDevInfPutCommand(this);
      issueRootPtr(putcmdP);
    }
    // try to load devinf from cache (only if we don't know it already)
    if (!fRemoteDataStoresKnown || !fRemoteDataTypesKnown) {
      SmlDevInfDevInfPtr_t devinfP;
      if (loadRemoteDevInf(getRemoteURI(),devinfP)) {
        // we have cached devinf, analyze it now
        analyzeRemoteDevInf(devinfP);
      }
    }
    // GET the server's info if server didn't send it and we haven't cached at least the datastores
    if (!fRemoteDataStoresKnown) {
      // if we know datastores here, but not types, this means that remote does not have
      // CTCap, so it makes no sense to issue a GET again.
      #ifndef NO_DEVINF_GET
      PDEBUGPRINTFX(DBG_REMOTEINFO,("Nothing known about server, request DevInf using GET command"));
      TGetCommand *getcommandP = new TGetCommand(this);
      getcommandP->addTargetLocItem(SyncMLDevInfNames[fSyncMLVersion]);
      string devinftype=SYNCML_DEVINF_META_TYPE;
      addEncoding(devinftype);
      getcommandP->setMeta(newMetaType(devinftype.c_str()));
      ISSUE_COMMAND_ROOT(this,getcommandP);
      #endif
    }
    // - create Alert(s) for databases to sync
    for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
      // create alert for non-subdatastores
      localDS = *pos;
      if (!localDS->isSubDatastore()) {
        TAlertCommand *alertcmdP;
        status=localDS->engGenerateClientSyncAlert(alertcmdP);
        if (status!=0) {
          // local database error
          return status; // not found
        }
        ///%%%% unneeded (probably got here by copy&paste accidentally): if (localDS->fFirstTimeSync) anyfirstsyncs=true;
        // issue alert
        issueRootPtr(alertcmdP);
      }
    }
    // append sync phase if we have combined init/sync
    if (fOutgoingState==psta_initsync) fOutgoingState=psta_sync;
  }
  // process sync/syncop/map generating phases after init
  if (!isSuspending()) {
    // normal, session continues
    if (fOutgoingState==psta_sync) {
      // hold back sync until server has finished first package (init or initsync)
      if (fIncomingState==psta_sync || fIncomingState==psta_initsync) {
        // start sync for alerted datastores
        for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
          // Note: some datastores might be aborted due to unsuccessful alert.
          if ((*pos)->isActive()) {
          	// prepare engine for sync (%%% new routine in 3.2.0.3, summarizing engInitForSyncOps() and
            // switching to dssta_dataaccessstarted, i.e. loading sync set), but do in only once
            if (!((*pos)->testState(dssta_syncsetready))) {
            	// not yet started
	          	status = (*pos)->engInitForClientSync();
	            if (status!=LOCERR_OK) {
                // failed
                AbortSession(status,true);
                return getAbortReasonStatus();
              }
            }
          	/* %%% old code: engInitForSyncOps was here, but switching to dssta_dataaccessstarted
                   was done earlier when handling alert from server. This caused that types
                   were not ready when loading the sync set, making client-side filtering impossible
            // initialize datastore for sync
            TStatusCommand dummystatus(this);
            status = (*pos)->engInitForSyncOps((*pos)->getRemoteDBPath());
            if (status!=LOCERR_OK) {
              // failed
              AbortSession(status,true);
              return status;
            }
            // init prepared, we can now call datastore to generate sync command
            */
            // start or continue (which is largely nop, as continuing works via unfinished sync command)
            // generating sync items
            (*pos)->engClientStartOfSyncMessage();
          }
        }
      }
    }
    else if (fOutgoingState==psta_map) {
      // hold back map until server has started sync at least (incominstate >=psta_sync)
      // NOTE: This is according to the specs, which says that client can begin
      //   with Map/update status package BEFORE sync package from server is
      //   completely received.
      // NOTE: Starfish server expects this and generates a few 222 alerts
      //   if we wait here, but then goes to map as well
      //   (so (fIncomingState==psta_map)-version works as well here!
      // %%%% other version: wait until server has started map phase as well
      // %%%% if (fIncomingState==psta_map) {
      if (fIncomingState>=psta_sync) {
        // start map for synced datastores
        for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
          // Note: some datastores might be aborted due to unsuccessful alert.
          if ((*pos)->isActive()) {
            // now call datastore to generate map command if not already done
            (*pos)->engClientStartOfMapMessage(fIncomingState<psta_map);
          }
        }
      }
    }
    else if (fOutgoingState==psta_supplement) {
    	// we are waiting for the server to complete a pending phase altough we are already done
      // with everything we want to send.
      // -> just generate a Alert 222 and wait for server to complete
      PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,("Client finished so far, but needs to wait in supplement outgoing state until server finishes phase"));
    }
    else if (fOutgoingState!=psta_init) {
      // NOTE: can be psta_init because "if" begins again after psta_init checking
      //       to allow psta_init appending psta_sync for combined init/sync
      // no known state
      return 9999; // %%%%%
    }
  } // if not suspended

  // security only: exit here if session got aborted in between
  if (isAborted())
    return getAbortReasonStatus(); // done & error
  if (!fInProgress)
    return 9999; // that's fine with us
  // now, we know that we will (most probably) send a message, so default for aDone is false from now on
  aDone=false;
  bool outgoingfinal;

  // check for suspend
  if (isSuspending()) {
    // make sure we send a Suspend Alert
    TAlertCommand *alertCmdP = new TAlertCommand(this,NULL,(uInt16)224);
    // - we just put local and remote URIs here
    SmlItemPtr_t itemP = newItem();
    itemP->target = newLocation(fRemoteURI.c_str());
    itemP->source = newLocation(fLocalURI.c_str());
    alertCmdP->addItem(itemP);
    ISSUE_COMMAND_ROOT(this,alertCmdP);
    // outgoing message is final, regardless of any session state
    outgoingfinal=true;
    MarkSuspendAlertSent(true);
  }
  else {
    // Determine if package can be final and if we need an 222 Alert
    // NOTE: if any commands were interruped or not sent due to outgoing message
    //       size limits, FinishMessage() will prevent final anyway, so no
    //       separate checking for enOfSync or endOfMap is needed.
    // - can finalize message when server has at least started answering current package
    //   OR if this is the first message (probably repeatedly) sent
    outgoingfinal = fIncomingState >= fOutgoingState || fIncomingState==psta_idle;
    if (outgoingfinal) {
      // allow early success here in case of nothing to respond, and nothing pending
      // StarFish server does need this...
      if (!fNeedToAnswer) {
        if (hasPendingCommands()) {
          // we have pending commands, cannot be final message
          outgoingfinal=false;
        }
        else {
          // no pending commands -> we're done now
          PDEBUGPRINTFX(DBG_PROTO,("Early end of session (nothing to send to server any more) -> calling engFinishDataStoreSync() for datastores now"));
          // - end sync in all datastores (save anchors etc.)
          for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos)
            (*pos)->engFinishDataStoreSync(); // successful end
          PDEBUGPRINTFX(DBG_PROTO,("Session not any more in progress: NextMessage() returns OK status=0"));
          // done & ok
          aDone=true;
          return LOCERR_OK;
        }
      }
    }
  }
  if (!outgoingfinal) {
    // - send Alert 222 if we need to continue package but have nothing to send
    //   (or ALWAYS_CONTINUE222 defined)
    #ifndef ALWAYS_CONTINUE222
    if (!fNeedToAnswer)
    #endif
    {
      // not final, and nothing to answer otherwise: create alert-Command to request more info
      TAlertCommand *alertCmdP = new TAlertCommand(this,NULL,(uInt16)222);
      // %%% not clear from spec what has to be in item for 222 alert code
      //     but there MUST be an Item for the Alert command according to SyncML TK
      // - we just put local and remote URIs here
      SmlItemPtr_t itemP = newItem();
      itemP->target = newLocation(fRemoteURI.c_str());
      itemP->source = newLocation(fLocalURI.c_str());
      alertCmdP->addItem(itemP);
      ISSUE_COMMAND_ROOT(this,alertCmdP);
    }
  }
  // send custom end-of session puts
  if (!isSuspending() && outgoingfinal && fOutgoingState==psta_map) {
    // End of outgoing map package; let custom PUTs which may transmit some session statistics etc. happen now
    issueCustomEndPut();
  }
  // message complete, now finish it
  FinishMessage(
    outgoingfinal, // allowed if possible
    false // final not prevented
  );
  // Note, now fNewOutgoingPackage is set (by FinishMessage())
  // if next message will be responded to with a new package

  // debug info
  #ifdef SYDEBUG
  if (PDEBUGMASK & DBG_SESSION) {
    PDEBUGPRINTFX(DBG_SESSION,(
      "---> NextMessage, outgoing state='%s', incoming state='%s'",
      PackageStateNames[fOutgoingState],
      PackageStateNames[fIncomingState]
    ));
    TLocalDataStorePContainer::iterator pos;
    for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
      // Show state of local datastores
      PDEBUGPRINTFX(DBG_SESSION,(
        "Local Datastore '%s': %sState=%s, %s%s sync, %s%s",
        (*pos)->getName(),
        (*pos)->isAborted() ? "ABORTED - " : "",
        (*pos)->getDSStateName(),
        (*pos)->isResuming() ? "RESUMED " : "",
        (*pos)->fSlowSync ? "SLOW" : "normal",
        SyncModeDescriptions[(*pos)->fSyncMode],
        (*pos)->fServerAlerted ? ", Server-Alerted" : ""
      ));
    }
  }
  PDEBUGENDBLOCK("SyncML_Outgoing");
  if (getLastIncomingMsgID()>0) {
    // we have already received an incoming message, so we have started an "SyncML_Incoming" blocks sometime
    PDEBUGENDBLOCK("SyncML_Incoming"); // terminate debug block of previous incoming message as well
  }
  #endif
  // ok
  return LOCERR_OK; // ok
} // TSyncClient::NextMessage



// called after successful decoding of an incoming message
bool TSyncClient::MessageStarted(SmlSyncHdrPtr_t aContentP, TStatusCommand &aStatusCommand, bool aBad)
{
  // message not authorized by default
  fMessageAuthorized=false;

  // Check information from SyncHdr
  if (
    aBad ||
    (!(fSynchdrSessionID==smlPCDataToCharP(aContentP->sessionID))) ||
    (!(fLocalURI==smlSrcTargLocURIToCharP(aContentP->target)))
  ) {
    // bad response
    PDEBUGPRINTFX(DBG_ERROR,(
      "Bad SyncHeader from Server. Syntax %s, SessionID (rcvd/correct) = '%s' / '%s', LocalURI (rcvd/correct) = '%s' / '%s'",
      aBad ? "ok" : "BAD",
      smlPCDataToCharP(aContentP->sessionID),
      fSynchdrSessionID.c_str(),
      smlSrcTargLocURIToCharP(aContentP->target),
      fLocalURI.c_str()
    ));
    aStatusCommand.setStatusCode(400); // bad response/request
    AbortSession(400,true);
    return false;
  }
  // check for suspend: if we are suspended at this point, this means that we have sent the Suspend Alert already
  // in the previous message (due to user suspend request), so we can now terminate the session
  if (isSuspending() && isSuspendAlertSent()) {
    AbortSession(514,true,LOCERR_USERSUSPEND);
    return false;
  }
  // - RespURI (remote URI to respond to)
  if (aContentP->respURI) {
    fRespondURI=smlPCDataToCharP(aContentP->respURI);
    DEBUGPRINTFX(DBG_PROTO,("RespURI set to = '%s'",fRespondURI.c_str()));
  }
  // authorization check
  // Note: next message will be started not before status for last one
  //       has been processed. Commands issued before will automatically
  //       be queued by issuePtr()
  // %%% none for now
  fSessionAuthorized=true;
  fMessageAuthorized=true;
  // returns false on BAD header (but true on wrong/bad/missing cred)
  return true;
} // TSyncClient::MessageStarted


// determines new package states and sets fInProgress
void TSyncClient::MessageEnded(bool aIncomingFinal)
{
  TLocalDataStorePContainer::iterator pos;

  // show status before processing
  PDEBUGPRINTFX(DBG_SESSION,(
    "MessageEnded starts   : old outgoing state='%s', old incoming state='%s', %sNeedToAnswer",
    PackageStateNames[fOutgoingState],
    PackageStateNames[fIncomingState],
    fNeedToAnswer ? "" : "NO "
  ));
  bool allFromClientOnly=false;
  // process exceptions
  if (fAborted) {
    PDEBUGPRINTFX(DBG_ERROR,("***** Session is flagged 'aborted' -> MessageEnded ends package and session"));
    fOutgoingState=psta_idle;
    fIncomingState=psta_idle;
    fInProgress=false;
  } // if aborted
  else if (!fMessageAuthorized) {
    // not authorized messages will just be ignored, so
    // nothing changes in states
    // %%% this will probably not really work, as we would need to repeat the last
    //     message in this (unlikely) case that fMessageAuthorized is not set for
    //     a non-first message (first message case is handled in handleHeaderStatus)
    DEBUGPRINTFX(DBG_ERROR,("***** received Message not authorized, ignore and DONT end package"));
    fInProgress=true;
  }
  else {
    fInProgress=true; // assume we need to continue
    // Note: the map phase will not take place, if all datastores are in
    //       send-to-server-only mode and we are not in non-conformant old
    //       synthesis-compatible fCompleteFromClientOnly mode.
    if (!fCompleteFromClientOnly) {
      // let all local datastores know that message has ended
      allFromClientOnly=true;
      for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
        // check sync modes
        if ((*pos)->isActive() && (*pos)->getSyncMode()!=smo_fromclient) {
          allFromClientOnly=false;
          break;
        }
      }
    }
    // new outgoing state is determined by the incomingState of this message
    // (which is the answer to the <final/> message of the previous outgoing package)
    if (fNewOutgoingPackage && fIncomingState!=psta_idle) {
      // last message sent was an end-of-package, so next will be a new package
      if (fIncomingState==psta_init) {
        // server has responded (or is still responding) to our finished init,
        // so client enters sync state now (but holds back sync until server
        // has finished init)
        fOutgoingState=psta_sync;
      }
      else if (fIncomingState==psta_sync || fIncomingState==psta_initsync) {
        // server has started (or already finished) sending statuses for our
        // <sync> or its own <sync>
        // client can enter map state (but holds back maps until server
        // has finished sync/initsync). In case of allFromClientOnly, we skip the map phase
        // but only if there is no need to answer.
        // Otherwise, this is most probably an old (pre 2.9.8.2) Synthesis server that has
        // sent an empty <sync> (and the status for it has set fNeedToAnswer), so we still
        // go to map phase.
        if (allFromClientOnly && !fNeedToAnswer) {
          fOutgoingState=psta_supplement; // all datastores are from-client-only, skip map phase
          PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,("All datastores in from-client-only mode, and no need to answer: skip map phase"));
        }
        else {
          fOutgoingState=psta_map; // Some datastores do from-server-only or twoway, so we need a map phase
          allFromClientOnly=false; // do not skip map phase
        }
      }
      else {
        // map is finished as well, we might need extra packages just to
        // finish getting results for map commands
        fOutgoingState=psta_supplement;
      }
    }
    // New incoming state is simply derived from the incoming state of
    // this message
    if (aIncomingFinal && fIncomingState!=psta_idle) {
      if (fIncomingState==psta_init) {
        // switch to sync
        fIncomingState=psta_sync;
      }
      else if (fIncomingState==psta_sync || fIncomingState==psta_initsync) {
        // check what to do
        if (allFromClientOnly) {
          // no need to answer and allFromClientOnly -> this is the end of the session
          fIncomingState=psta_supplement;
          fInProgress=false; // normally, at end of map answer, we are done
        }
        else {
          fIncomingState=psta_map;
        }
      }
      else {
        // end of a map phase - end of session (if no fNeedToAnswer)
        fIncomingState=psta_supplement;
        // this only ALLOWS ending the session, but it will continue as long
        // as more than OK for SyncHdr (fNeedToAnswer) must be sent
        fInProgress=false; // normally, at end of map answer, we are done
      }
    }
    // continue anyway as long as we need to answer
    if (fNeedToAnswer) fInProgress=true;
  }
  // show states
  PDEBUGPRINTFX(DBG_HOT,(
    "MessageEnded finishes : new outgoing state='%s', new incoming state='%s', %sNeedToAnswer",
    PackageStateNames[fOutgoingState],
    PackageStateNames[fIncomingState],
    fNeedToAnswer ? "" : "NO "
  ));
  // let all local datastores know that message has ended
  for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
    // let them know
    (*pos)->engEndOfMessage();
    // Show state of local datastores
    PDEBUGPRINTFX(DBG_HOT,(
      "Local Datastore '%s': %sState=%s, %s%s sync, %s%s",
      (*pos)->getName(),
      (*pos)->isAborted() ? "ABORTED - " : "",
      (*pos)->getDSStateName(),
      (*pos)->isResuming() ? "RESUMED " : "",
      (*pos)->isSlowSync() ? "SLOW" : "normal",
      SyncModeDescriptions[(*pos)->getSyncMode()],
      (*pos)->fServerAlerted ? ", Server-Alerted" : ""
    ));
  }
  // thread might end here, so stop profiling
  TP_STOP(fTPInfo);
} // TSyncClient::MessageEnded


// get credentials/username to authenticate with remote party, NULL if none
SmlCredPtr_t TSyncClient::newCredentialsForRemote(void)
{
  if (fNeedAuth) {
    // generate cretentials from username/password
    PDEBUGPRINTFX(DBG_PROTO+DBG_USERDATA,("Authenticating with server as user '%s'",fServerUser.c_str()));
    // NOTE: can be NULL when fServerRequestedAuth is auth_none
    return newCredentials(
      fServerUser.c_str(),
      fServerPassword.c_str()
    );
  }
  else {
    // already authorized, no auth needed
    return NULL;
  }
} // TSyncClient::newCredentialsForRemote


// get client base
TSyncClientBase *TSyncClient::getClientBase(void)
{
  return static_cast<TSyncClientBase *>(getSyncAppBase());
} // TSyncClient::getClientBase


// called when incoming SyncHdr fails to execute
bool TSyncClient::syncHdrFailure(bool aTryAgain)
{
  // do not try to re-execute the header, just let message processing fail;
  // this will cause the client's main loop to try using an older protocol
  return false;
} // TSyncClient::syncHdrFailure


// retry older protocol, returns false if no older protocol to try
bool TSyncClient::retryOlderProtocol(bool aSameVersionRetry, bool aOldMessageInBuffer)
{
  if (fIncomingState==psta_idle) {
    // if we have not started a session yet and not using oldest protocol already,
    // we want to retry with next older SyncML version
    if (aSameVersionRetry) {
      // just retry same version
      PDEBUGPRINTFX(DBG_PROTO,("Retrying session start with %s",SyncMLVerProtoNames[fSyncMLVersion]));
    }
    else if (fSyncMLVersion>getSessionConfig()->fMinSyncMLVersionSupported) {
      // next lower
      fSyncMLVersion=(TSyncMLVersions)(((uInt16)fSyncMLVersion)-1);
      PDEBUGPRINTFX(DBG_PROTO,("Server does not support our SyncML version, trying with %s",SyncMLVerProtoNames[fSyncMLVersion]));
    }
    else {
      // cannot retry
      return false;
    }
    // retry
    retryClientSessionStart(aOldMessageInBuffer);
    return true;
  }
  // session already started or no older protocol to try
  return false;
} // TSyncClient::retryOlderProtocol


// prepares client session such that it will do a retry to start a session
// (but keeping already received auth/nonce/syncML-Version state)
void TSyncClient::retryClientSessionStart(bool aOldMessageInBuffer)
{
  TClientConfig *configP = static_cast<TClientConfig *>(getRootConfig()->fAgentConfigP);

  // now restarting
  PDEBUGPRINTFX(DBG_HOT,("=================> Retrying Client Session Start"));
  bool newSessionForAuthRetry = configP->fNewSessionForAuthRetry;
  bool noRespURIForAuthRetry = configP->fNoRespURIForAuthRetry;
	// check if we should use modified behaviour (smart retries)
  if (configP->fSmartAuthRetry && fAuthRetries>MAX_NORMAL_AUTH_RETRIES) {
  	if (newSessionForAuthRetry) {
    	// if we had new session for retry, switch to in-session retry now
      newSessionForAuthRetry = false;
      noRespURIForAuthRetry = false;
    }
    else {
    	// if we had in-session retry, try new session retry now
      newSessionForAuthRetry = true;
      noRespURIForAuthRetry = true;    
    }
    PDEBUGPRINTFX(DBG_PROTO,("Smart retry with modified behaviour: newSessionForAuthRetry=%d, noRespURIForAuthRetry=%d",newSessionForAuthRetry,noRespURIForAuthRetry));
  }
  // now retry
  if (newSessionForAuthRetry) {
    // Notes:
    // - must apparently be disabled for SCTS 3.1.2 and eventually Mightyphone
    // - must be enabled e.g for for Magically Server
    // Create new session ID
    StringObjPrintf(fSynchdrSessionID,"%hd",(sInt16)++fClientSessionNo);
    // restart message counting at 1
    fIncomingMsgID=0;
    fOutgoingMsgID=0;
    // we must terminate the block here when we reset fIncomingMsgID, as NextMessage
    // only closes the incoming block when fIncomingMsgID>0
    PDEBUGENDBLOCK("SyncML_Incoming");
  }
  if (noRespURIForAuthRetry) {
    // Notes:
    // - must apparently be switched on for Starfish.
    // - must apparently be switched off for SCTS 3.1.2.
    // make sure we send next msg to the original URL
    fRespondURI=fRemoteURI;
  }
  // - make sure status for SyncHdr will not be generated!
  forgetHeaderWaitCommands();
  // check if we have already started next outgoing message
  if (!fOutgoingStarted) {
    if (aOldMessageInBuffer) {
      // make sure we start with a fresh output buffer
      // Note: This usually only occur when we are not currently parsing
      //       part of the buffer. If we are parsing, the remaining incoming
      //       message gets cleared as well.
      getClientBase()->clrUnreadSmlBufferdata();
    }
    // start a new message
    issueHeader(false);
  }
  else {
    if (aOldMessageInBuffer) {
      PDEBUGPRINTFX(DBG_ERROR,("Warning - restarting session with old message in output buffer"));
    }
  }
  // - make sure subsequent commands (most probably statuses for Alerts)
  //   don't get processed
  AbortCommandProcessing(0); // silently discard all further commands
  // - make sure eventual processing errors do not abort the session
  fIgnoreMsgErrs = true;
} // TSyncClient::retryClientSessionStart


// handle status received for SyncHdr, returns false if not handled
bool TSyncClient::handleHeaderStatus(TStatusCommand *aStatusCmdP)
{
  TClientConfig *configP = static_cast<TClientConfig *>(getRootConfig()->fAgentConfigP);
  bool handled=true;
  const char *txt;
  SmlMetInfMetInfPtr_t chalmetaP=NULL;
  SmlChalPtr_t chalP;

  // first evaluate eventual challenge in header status
  chalP = aStatusCmdP->getStatusElement()->chal;
  if (chalP) {
    chalmetaP = smlPCDataToMetInfP(chalP->meta);
    if (chalmetaP) {
      sInt16 ty;
      // - get auth type
      if (!chalmetaP->type) AbortSession(401,true); // missing auth, but no type
      txt = smlPCDataToCharP(chalmetaP->type);
      PDEBUGPRINTFX(DBG_PROTO,("Remote requests auth type='%s'",txt));
      if (StrToEnum(authTypeSyncMLNames,numAuthTypes,ty,txt))
        fRemoteRequestedAuth=(TAuthTypes)ty;
      else {
        AbortSession(406,true); // unknown auth type, not supported
        goto donewithstatus;
      }
      // - get auth format
      if (!smlPCDataToFormat(chalmetaP->format, fRemoteRequestedAuthEnc)) {
        AbortSession(406,true); // unknown auth format, not supported
        goto donewithstatus;
      }
      // - get next nonce
      if (chalmetaP->nextnonce) {
        // decode B64
        uInt32 l;
        uInt8 *nonce = b64::decode(smlPCDataToCharP(chalmetaP->nextnonce), 0, &l);
        fRemoteNonce.assign((char *)nonce,l);
        sysync_free(nonce);
      }
      // - show
      PDEBUGPRINTFX(DBG_PROTO,(
        "Next Cred will have type='%s' and format='%s' and use nonce='%s'",
        authTypeNames[fRemoteRequestedAuth],
        encodingFmtNames[fRemoteRequestedAuthEnc],
        fRemoteNonce.c_str()
      ));
    }
    /* %%% do not save here already, we don't know if SyncML version is ok
           moved to those status code cases below that signal
    // let descendant eventually save auth params
    saveRemoteParams();
    */
  }
  // now evaluate status code
  switch (aStatusCmdP->getStatusCode()) {
    case 101: // Busy
      // Abort
      AbortSession(101,false);
      break;
    case 212: // authentication accepted for entire session
      fNeedAuth=false; // no need for further auth
      PDEBUGPRINTFX(DBG_PROTO,("Remote accepted authentication for entire session"));
    case 200: // authentication accepted for this message
      // if this is the first authorized message we get an OK for the synchdr, this is
      // also the first incoming message that is really processed as init message
      if (fIncomingState==psta_idle && fMessageAuthorized) {
        // first incoming is expected to be same as first outgoing (init or initsync)
        fIncomingState=fOutgoingState;
        PDEBUGPRINTFX(DBG_PROTO,("Authenticated successfully with remote server"));
      }
      else {
        PDEBUGPRINTFX(DBG_PROTO,("Authentication with server ok for this message"));
      }
      // let descendant eventually save auth params
      saveRemoteParams();
      break;
    case 501: // handle a "command not implemented" for the SyncHdr like 513 (indication that server does not like our header)
    case 400: // ..and 400 as well (sync4j case, as it seems)
    case 513: // bad protocol version
    case 505: // bad DTD version (NextHaus/DeskNow case)
      // try with next lower protocol
      PDEBUGENDBLOCK("processStatus"); // done processing status
      if (!retryOlderProtocol()) {
        // no older SyncML protocol we can try --> abort
        AbortSession(513,false); // server does not know any of our SyncML versions
      }
      break;
    case 401: // bad authentication
      // Bad authorisation
      if (fAuthRetries==0)
        // if first attempt is rejected with "bad", we conclude that the
        // last attempt was carrying auth data and was not a attempt to get challenge
        // from server. Therefore we count this as two tries (one get chal, one really failing)
        fAuthRetries=2;
      else
        fAuthRetries++; // just count attempt to auth
      /* %%% no longer required, is tested below at authfail:
      if (fAuthRetries>MAX_AUTH_RETRIES) {
        AbortSession(401,false); // abort session, too many retries
        break;
      }
      */
      // Treat no nonce like empty nonce to make sure that a server (like SySync old versions...)
      // that does not send a nonce at all does not get auth with some old, invalid nonce string included.
      if (chalmetaP && chalmetaP->nextnonce==NULL) fRemoteNonce.erase();
      // otherwise treat like 407
      goto authfail;
    case 407: // authentication required
      // new since 2.0.4.6: count this as well (normally this happens once when sending
      // no auth to the server to force it to send us auth chal first).
      fAuthRetries++;
    authfail:
      PDEBUGPRINTFX(DBG_ERROR,("Authentication failed (status=%hd) with remote server",aStatusCmdP->getStatusCode()));
      // Auth fail after we have received a valid response for the init message indicates protocol messed up
      if (fIncomingState!=psta_idle) {
        AbortSession(400,true); // error in protocol handling from remote
        break;
      }
      // Check if smart retries (with modified in-session vs out-of-session behaviour) are enabled
      if (!configP->fSmartAuthRetry && fAuthRetries>MAX_NORMAL_AUTH_RETRIES) {
      	fAuthRetries = MAX_SMART_AUTH_RETRIES+1; // skip additional smart retries
      }
      // Missing or bad authorisation, evaluate chal
      if (!chalmetaP || fAuthRetries>MAX_SMART_AUTH_RETRIES) {
      	#ifdef SYDEBUG
      	if (!chalmetaP) {
		      PDEBUGPRINTFX(DBG_ERROR,("Bad auth but no challenge in response status -> can't work - no retry"));
        }
        #endif
        AbortSession(aStatusCmdP->getStatusCode(),false); // retries exhausted or no retry possible (no chal) -> stop session
        break;
      }
      // let descendant eventually save auth params
      saveRemoteParams();
      // modify session for re-start
      PDEBUGENDBLOCK("processStatus"); // done processing status
      retryClientSessionStart(false); // no previously sent message in the buffer
      break;
    default:
      handled=false; // could not handle status
  } // switch
donewithstatus:
  // Anyway, reception of status for header enables generation of next message header
  // (plus already generated commands such as status for response header)
  if (!fMsgNoResp && !isAborted()) {
    // issue header now if not already issued above
    if (!fOutgoingStarted) {
      // interrupt status processing block here as issueHeader will do a start-of-message PDEBUGBLOCK
      PDEBUGENDBLOCK("processStatus");
      issueHeader(false);
      PDEBUGBLOCKDESC("processStatus","finishing processing incoming SyncHdr Status");
    }
  }
  // return handled status
  return handled;
} // TSyncClient::handleHeaderStatus


// - start sync group (called in client or server roles)
bool TSyncClient::processSyncStart(
  SmlSyncPtr_t aSyncP,           // the Sync element
  TStatusCommand &aStatusCommand, // pre-set 200 status, can be modified in case of errors
  bool &aQueueForLater // will be set if command must be queued for later (re-)execution
)
{
  if (fIncomingState!=psta_sync && fIncomingState!=psta_initsync) {
    aStatusCommand.setStatusCode(403); // forbidden in this context
    PDEBUGPRINTFX(DBG_ERROR,("Sync command not allowed outside of sync phase (-> 403)"));
    AbortSession(400,true);
    return false;
  }
  // just find appropriate database, must be already initialized for sync!
  // determine local database to sync with (target)
  TLocalEngineDS *datastoreP = findLocalDataStoreByURI(smlSrcTargLocURIToCharP(aSyncP->target));
  if (!datastoreP) {
    // no such local datastore
    PDEBUGPRINTFX(DBG_ERROR,("Sync command for unknown DS locURI '%s' (-> 404)",smlSrcTargLocURIToCharP(aSyncP->target)));
    aStatusCommand.setStatusCode(404); // not found
    return false;
  }
  else {
    // save the pointer, will e.g. be used to route subsequent server commands
    fLocalSyncDatastoreP=datastoreP;
    // let local datastore know (in server case, this is done in TSyncServer)
    return fLocalSyncDatastoreP->engProcessSyncCmd(aSyncP,aStatusCommand,aQueueForLater);
  }
  return true;
} // TSyncClient::processSyncStart



#ifdef ENGINEINTERFACE_SUPPORT


// Support for EngineModule common interface
// =========================================


/// @brief Get new session key to access details of this session
appPointer TSyncClient::newSessionKey(TEngineInterface *aEngineInterfaceP)
{
  return new TClientParamsKey(aEngineInterfaceP,this);
} // TSyncClient::newSessionKey


// Client runtime settings key
// ---------------------------

// Constructor
TClientParamsKey::TClientParamsKey(TEngineInterface *aEngineInterfaceP, TSyncClient *aClientSessionP) :
  inherited(aEngineInterfaceP,aClientSessionP),
  fClientSessionP(aClientSessionP)
{
} // TClientParamsKey::TClientParamsKey


// - read connection URL
static TSyError readConnectURI(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TClientParamsKey *mykeyP = static_cast<TClientParamsKey *>(aStructFieldsKeyP);
  return TStructFieldsKey::returnString(
    mykeyP->fClientSessionP->getSendURI(),
    aBuffer,aBufSize,aValSize
  );
} // readConnectURI


// - read host part of connection URL
static TSyError readConnectHost(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TClientParamsKey *mykeyP = static_cast<TClientParamsKey *>(aStructFieldsKeyP);
  string host;
  splitURL(mykeyP->fClientSessionP->getSendURI(),NULL,&host,NULL,NULL,NULL);
  return TStructFieldsKey::returnString(
    host.c_str(),
    aBuffer,aBufSize,aValSize
  );
} // readConnectHost


// - read document part of connection URL
static TSyError readConnectDoc(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TClientParamsKey *mykeyP = static_cast<TClientParamsKey *>(aStructFieldsKeyP);
  string doc;
  splitURL(mykeyP->fClientSessionP->getSendURI(),NULL,NULL,&doc,NULL,NULL);
  return TStructFieldsKey::returnString(
    doc.c_str(),
    aBuffer,aBufSize,aValSize
  );
} // readConnectDoc


// - read content type string
static TSyError readContentType(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TClientParamsKey *mykeyP = static_cast<TClientParamsKey *>(aStructFieldsKeyP);
  string contentType = SYNCML_MIME_TYPE;
  mykeyP->fClientSessionP->addEncoding(contentType);
  return TStructFieldsKey::returnString(
    contentType.c_str(),
    aBuffer,aBufSize,aValSize
  );
} // readContentType


// - read local session ID
static TSyError readLocalSessionID(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TClientParamsKey *mykeyP = static_cast<TClientParamsKey *>(aStructFieldsKeyP);
  return TStructFieldsKey::returnString(
    mykeyP->fClientSessionP->getLocalSessionID(),
    aBuffer,aBufSize,aValSize
  );
} // readLocalSessionID


// - write (volatile, write-only) password for running this session
// (for cases where we don't want to rely on binfile storage for sensitive password data)
TSyError writeSessionPassword(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  TClientParamsKey *mykeyP = static_cast<TClientParamsKey *>(aStructFieldsKeyP);
	mykeyP->fClientSessionP->setServerPassword((cAppCharP)aBuffer, aValSize);
  return LOCERR_OK;
} // writeSessionPassword


#ifdef ENGINE_LIBRARY
// - read display alert
static TSyError readDisplayAlert(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TClientEngineInterface *clientEngineP =
    static_cast<TClientEngineInterface *>(aStructFieldsKeyP->getEngineInterface());
  return TStructFieldsKey::returnString(
    clientEngineP->fAlertMessage.c_str(),
    aBuffer,aBufSize,aValSize
  );
} // readDisplayAlert
#endif


// accessor table for client params
static const TStructFieldInfo ClientParamFieldInfos[] =
{
  // valName, valType, writable, fieldOffs, valSiz
  { "connectURI", VALTYPE_TEXT, false, 0, 0, &readConnectURI, NULL },
  { "connectHost", VALTYPE_TEXT, false, 0, 0, &readConnectHost, NULL },
  { "connectDoc", VALTYPE_TEXT, false, 0, 0, &readConnectDoc, NULL },
  { "contenttype", VALTYPE_TEXT, false, 0, 0, &readContentType, NULL },
  { "localSessionID", VALTYPE_TEXT, false, 0, 0, &readLocalSessionID, NULL },
  { "sessionPassword", VALTYPE_TEXT, true, 0, 0, NULL, &writeSessionPassword },
  #ifdef ENGINE_LIBRARY
  { "displayalert", VALTYPE_TEXT, false, 0, 0, &readDisplayAlert, NULL },
  #endif
};

// get table describing the fields in the struct
const TStructFieldInfo *TClientParamsKey::getFieldsTable(void)
{
  return ClientParamFieldInfos;
} // TClientParamsKey::getFieldsTable

sInt32 TClientParamsKey::numFields(void)
{
  return sizeof(ClientParamFieldInfos)/sizeof(TStructFieldInfo);
} // TClientParamsKey::numFields

// get actual struct base address
uInt8P TClientParamsKey::getStructAddr(void)
{
  // prepared for accessing fields in client session object
  return (uInt8P)fClientSessionP;
} // TClientParamsKey::getStructAddr

#endif // ENGINEINTERFACE_SUPPORT

} // namespace sysync

// eof
