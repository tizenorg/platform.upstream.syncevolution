/* 
 *  File:         SyncServer.cpp
 *
 *  Author:			  Lukas Zeller (luz@synthesis.ch)
 *
 *  TSyncServer
 *    <describe here>
 *
 *  Copyright (c) 2001-2009 by Synthesis AG (www.synthesis.ch)
 *
 *  2001-05-07 : luz : Created
 *
 */

// includes
#include <errno.h>
#include "prefix_file.h"

#include "sysync.h"
#include "syncserver.h"

#ifdef SYSYNC_CLIENT
  #error "SYSYNC_CLIENT must NOT be defined when compiling syncserver.cpp"
#endif


// %%% define to prevent GET command sent to Client when
//     package #1 did not contain DEVINF
//#define NO_DEVINF_GET 1

// %%% define to combine SYNC and MAP as one response package
// %%%%%%% probably useless and totally wrong, but trying to
// %%%%%%% get 9210 to work.
//#define COMBINE_SYNCANDMAP

// define to include a 222 alert in every response to a message
// that does not include the <Final/> flag (otherwise, 222 Alert
// will only be generated when no other command is to be sent back)
//#define ALWAYS_CONTINUE222 1

using namespace sysync; 


namespace sysync {

// Support for SySync Diagnostic Tool
#ifdef SYSYNC_TOOL


// test login into database
int testLogin(int argc, const char *argv[])
{
  if (argc<0) {
    // help requested
    CONSOLEPRINTF(("  login [<username> <password>] [<deviceid>]"));
    CONSOLEPRINTF(("    test login to database with syncml user/password and optional deviceid"));
    return EXIT_SUCCESS;
  }

  TSyncSession *sessionP = NULL;
  const char *username = NULL;
  const char *password = NULL;
  const char *deviceid = "sysytool_test";

  // check for argument
  if (argc<2) {
    // no user/password, test anonymous login
  	if (argc>0) deviceid = argv[0];
  }
  else {
    // login with user/password
    username = argv[0];
    password = argv[1];
    if (argc>2) {
      // explicit device ID
      deviceid = argv[2];
    }
  }
    
  // get session to work with
  sessionP =
    static_cast<TSyncSessionDispatch *>(getSyncAppBase())->getSySyToolSession();

  bool authok = false;
  
  // try login
  if (username) {
    // real login with user and password
    authok = sessionP->SessionLogin(username, password, sectyp_clearpass, deviceid);
  }
  else {
    // anonymous - do a "login" with empty credentials
    authok = sessionP->SessionLogin("anonymous", NULL, sectyp_anonymous, deviceid);
  }
  
  if (authok) {
    CONSOLEPRINTF(("+++++ Successfully authorized"));
  }
  else {
    CONSOLEPRINTF(("----- Authorisation failed"));
  }
  
  return authok;
} // testLogin


// convert user data into internal format and back
int convertData(int argc, const char *argv[])
{
  if (argc<0) {
    // help requested
    CONSOLEPRINTF(("  convert <datastore name> <data file, vcard or vcalendar etc.> [<explicit input type>] [<output type>]"));
    CONSOLEPRINTF(("    Convert data to internal format of specified datastore and back"));
    return EXIT_SUCCESS;
  }

  TSyncSession *sessionP = NULL;
  const char *datastore = NULL;
  const char *rawfilename = NULL;
  const char *inputtype = NULL;
  const char *outputtype = NULL;

  // check for argument
  if (argc<2) {
  	CONSOLEPRINTF(("required datatype name and raw file name arguments"));
  	return EXIT_FAILURE;
  }
  datastore = argv[0];
  rawfilename = argv[1];
  if (argc>=3) {
    // third arg is explicit input type
    inputtype=argv[2];
  }
  outputtype=inputtype; // default to input type
  if (argc>=4) {
    // fourth arg is explicit output type
    outputtype=argv[3];
  }
  
  // get session to work with
  sessionP =
    static_cast<TSyncSessionDispatch *>(getSyncAppBase())->getSySyToolSession();
  // configure session
  sessionP->fRemoteCanHandleUTC = true; // run generator and parser in UTC enabled mode
  
  
  // switch mimimal debugging on
  sessionP->getDbgLogger()->setMask(sessionP->getDbgLogger()->getMask() | (DBG_PARSE+DBG_GEN));
    
  // find datastore
  TLocalEngineDS *datastoreP = sessionP->findLocalDataStore(datastore);
  TSyncItemType *inputtypeP = NULL;
  TSyncItemType *outputtypeP = NULL;
  if (!datastoreP) {
  	CONSOLEPRINTF(("datastore type '%s' not found",datastore));
  	return EXIT_FAILURE;
  }

  // find input type
  if (inputtype) {
    // search in datastore
    inputtypeP=datastoreP->getReceiveType(inputtype,NULL);
  }
  else {
    // use preferred rx type
    inputtypeP=datastoreP->getPreferredRxItemType();
  }
  if (!inputtypeP) {
  	CONSOLEPRINTF(("input type not found"));
  	return EXIT_FAILURE;
  }
  // find output type
  if (outputtype) {
    // search in datastore
    outputtypeP=datastoreP->getSendType(outputtype,NULL);
  }
  else {
    // use preferred rx type
    outputtypeP=datastoreP->getPreferredTxItemType();
  }
  if (!outputtypeP) {
  	CONSOLEPRINTF(("output type not found"));
  	return EXIT_FAILURE;
  }
  // prepare type usage
  if (inputtypeP==outputtypeP)
    inputtypeP->initDataTypeUse(datastoreP, true, true);
  else {
    inputtypeP->initDataTypeUse(datastoreP, false, true);
    outputtypeP->initDataTypeUse(datastoreP, true, false);
  }    

  // now open file and read data item
  FILE *infile;
  size_t insize=0;
  uInt8 *databuffer;
  
  infile = fopen(rawfilename,"rb");
  if (!infile) {
  	CONSOLEPRINTF(("Cannot open input file '%s' (%d)",rawfilename,errno));
  	return EXIT_FAILURE;    
  }
  // - get size of file
  fseek(infile,0,SEEK_END);
  insize=ftell(infile);
  fseek(infile,0,SEEK_SET);
  // - create buffer of appropriate size
  databuffer = new uInt8[insize];
  if (!databuffer) {
  	CONSOLEPRINTF(("Not enough memory to read input file '%s' (%d)",rawfilename,errno));
  	return EXIT_FAILURE;    
  }  
  // - read data
  if (fread(databuffer,1,insize,infile)<insize) {
  	CONSOLEPRINTF(("Error reading input file '%s' (%d)",rawfilename,errno));
  	return EXIT_FAILURE;    
  }
  CONSOLEPRINTF(("\nNow converting into internal field representation\n"));
  // create a sml item
  TStatusCommand statusCmd(sessionP);
  SmlItemPtr_t smlitemP = newItem();
  smlitemP->data=newPCDataStringX(databuffer,true,insize);
  delete[] databuffer;
  // create and fill a Sync item
  TSyncItem *syncitemP = inputtypeP->newSyncItem(
    smlitemP, // SyncML toolkit item Data to be converted into SyncItem
    sop_replace, // the operation to be performed with this item
    fmt_chr,    // assume default (char) format
    inputtypeP, // target myself
    datastoreP, // local datastore
    statusCmd // status command that might be modified in case of error
  );
  // forget SyncML version
  smlFreeItemPtr(smlitemP);
  if (!syncitemP) {
  	CONSOLEPRINTF(("Error converting input file to internal format (SyncML status code=%hd)",statusCmd.getStatusCode()));
  	return EXIT_FAILURE;
  }

  CONSOLEPRINTF(("\nNow copying item and convert back to transport format\n"));

  // make new for output type
  TSyncItem *outsyncitemP = outputtypeP->newSyncItem(
    outputtypeP, // target myself
    datastoreP  // local datastore
  );
  // copy data
  outsyncitemP->replaceDataFrom(*syncitemP);
  delete syncitemP;
  // convert back
  smlitemP=outputtypeP->newSmlItem(
    outsyncitemP,   // the syncitem to be represented as SyncML
    datastoreP // local datastore
  );
  if (!syncitemP) {
  	CONSOLEPRINTF(("Could not convert back item data"));
  	return EXIT_FAILURE;
  }
  
  // forget converted back item
  smlFreeItemPtr(smlitemP);
  
  return EXIT_SUCCESS;
} // convertData

#endif // SYSYNC_TOOL



/*
 * Implementation of TServerConfig
 */
 

// config constructor
TServerConfig::TServerConfig(const char *aElementName, TConfigElement *aParentElementP) :
  TSessionConfig(aElementName,aParentElementP)
{
  clear();
} // TServerConfig::TServerConfig


// config destructor
TServerConfig::~TServerConfig()
{
} // TServerConfig::~TServerConfig


// init defaults
void TServerConfig::clear(void)
{
  // init defaults
  fRequestedAuth=auth_md5;
  fRequiredAuth=auth_md5;
  fAutoNonce=true;
  fConstantNonce.erase();
  fExternalURL.erase();
  fMaxGUIDSizeSent=32; // reasonable size, but prevent braindamaged Exchange-size IDs to be sent
  // clear inherited  
  inherited::clear();
  // modify timeout after inherited sets it
  fSessionTimeout=DEFAULT_SERVERSESSIONTIMEOUT;
} // TServerConfig::clear


// server config element parsing
bool TServerConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  if (strucmp(aElementName,"requestedauth")==0)
		expectEnum(sizeof(fRequestedAuth),&fRequestedAuth,authTypeNames,numAuthTypes);
  else if (strucmp(aElementName,"requiredauth")==0)
		expectEnum(sizeof(fRequiredAuth),&fRequiredAuth,authTypeNames,numAuthTypes);
  // here to maintain compatibility with old pre 1.0.5.3 config files
  else if (strucmp(aElementName,"reqiredauth")==0)
		expectEnum(sizeof(fRequiredAuth),&fRequiredAuth,authTypeNames,numAuthTypes);
  else if (strucmp(aElementName,"autononce")==0)
		expectBool(fAutoNonce);
  else if (strucmp(aElementName,"constantnonce")==0)
		expectString(fConstantNonce);
  else if (strucmp(aElementName,"externalurl")==0)
		expectString(fExternalURL);
  else if (strucmp(aElementName,"maxguidsizesent")==0)
    expectUInt16(fMaxGUIDSizeSent);
  // - none known here
  else
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TServerConfig::localStartElement


// resolve
void TServerConfig::localResolve(bool aLastPass)
{
  // check
  if (aLastPass) {
    if (!fAutoNonce && fConstantNonce.empty())
      ReportError(false,"Warning: 'constantnonce' should be defined when 'autononce' is not set");
  }
  // resolve inherited  
  inherited::localResolve(aLastPass);
} // TServerConfig::localResolve


/*
 * Implementation of TSyncServer
 */


TSyncServer::TSyncServer(
  TSyncAppBase *aAppBaseP,
  TSyncSessionHandle *aSessionHandleP,
  const char *aSessionID // a session ID
) :
  TSyncSession(aAppBaseP,aSessionID)  
  #ifdef ENGINEINTERFACE_SUPPORT
  ,fEngineState(ses_needdata)
  #endif
{
  // init answer buffer
  fBufferedAnswer=NULL;
  fBufferedAnswerSize=0;  
  // reset data counts
  fIncomingBytes=0;
  fOutgoingBytes=0;
  // init own stuff
  InternalResetSession();
  // save session handle
  fSessionHandleP = aSessionHandleP; // link to handle
  // create all locally available datastores from config
  TServerConfig *configP = static_cast<TServerConfig *>(aAppBaseP->getRootConfig()->fAgentConfigP);
  TLocalDSList::iterator pos;
  for (pos=configP->fDatastores.begin(); pos!=configP->fDatastores.end(); pos++) {
    // create the datastore
    addLocalDataStore(*pos);
  }   
} // TSyncServer::TSyncServer


TSyncServer::~TSyncServer()
{
  // forget any buffered answers
  bufferAnswer(NULL,0);
  // reset session
  InternalResetSession();
  // show session data transfer
  PDEBUGPRINTFX(DBG_HOT,(
    "Session data transfer statistics: incoming bytes=%ld, outgoing bytes=%ld",
    fIncomingBytes,
    fOutgoingBytes
  ));
  // DO NOT remove session from dispatcher here,
  //   this is the task of the dispatcher itself!
  CONSOLEPRINTF(("Terminated SyncML session (server id=%s)\n",getLocalSessionID()));
  // show end of session in global level
  POBJDEBUGPRINTFX(getSyncAppBase(),DBG_HOT,(
    "TSyncServer::~TSyncServer: Deleted SyncML session (local session id=%s)",
    getLocalSessionID()
  ));
} // TSyncServer::~TSyncServer


// Reset session
void TSyncServer::InternalResetSession(void)
{
  // %%% remove this as soon as Server is 1.1 compliant
  //fSyncMLVersion=syncml_vers_1_0; // only accepts 1.0 for now %%%%
} // TSyncServer::InternalResetSession


// Virtual version
void TSyncServer::ResetSession(void)
{
  // let ancestor do its stuff
  TSyncSession::ResetSession();
  // do my own stuff
  InternalResetSession();
} // TSyncServer::ResetSession


// called when incoming SyncHdr fails to execute
bool TSyncServer::syncHdrFailure(bool aTryAgain)
{
  if (!aTryAgain) {
    // not already retried executing
    // special case: header failed to execute, this means that session must be reset
    // - Reset session (aborts all DB transactions etc.)
    ResetSession();
    PDEBUGPRINTFX(DBG_ERROR,("Trying to recover SyncHdr failure: =========== Session restarted ====================="));
    // - now all session infos are gone except this command which is owned by
    //   this function alone. Execute it again.
    aTryAgain=true;
  }
  else {
    // special special case: header failed to execute the second time
    DEBUGPRINTFX(DBG_ERROR,("Fatal internal problem, SyncHdr execution failed twice"));
    aTryAgain=false; // just to make sure
    SYSYNC_THROW((TSyncException("SyncHdr fatal execution problem")));
  }
  return aTryAgain;
} // TSyncServer::syncHdrFailure


// undefine these only for tests. Introduced to find problem with T68i
#define USE_RESPURI
#define RESPURI_ONLY_WHEN_NEEDED

// create a RespURI string. If none needed, return NULL
SmlPcdataPtr_t TSyncServer::newResponseURIForRemote(void)
{
  // do it in a transport-independent way, therefore let dispatcher do it
  string respURI; // empty string
  #ifdef USE_RESPURI
  getSyncAppBase()->generateRespURI(
    respURI,  // remains unaffected if no RespURI could be calculated
    fInitialLocalURI.c_str(), // initial URI used by remote to send first message
    fLocalSessionID.c_str()  // server generated unique session ID
  );
  // Omit RespURI if local URI as seen by client is identical
  #ifdef RESPURI_ONLY_WHEN_NEEDED
  // %%% attempt to make T68i work
  if (respURI==fLocalURI) {
    respURI.erase();
    DEBUGPRINTFX(DBG_SESSION,(
      "Generated RespURI and sourceLocURI are equal (%s)-> RespURI omitted",
      fLocalURI.c_str()
    ));
  }
  #endif
  #endif
  // Note: returns NULL if respURI is empty string
  return newPCDataOptString(respURI.c_str());
} // newResponseURIForRemote


// called after successful decoding of an incoming message
bool TSyncServer::MessageStarted(SmlSyncHdrPtr_t aContentP, TStatusCommand &aStatusCommand, bool aBad)
{
  Ret_t err=SML_ERR_OK;

  // message not authorized by default
  fMessageAuthorized=false;
  
  // Get information from SyncHdr which is needed for answers
  // - session ID to be used for responses
  fSynchdrSessionID=smlPCDataToCharP(aContentP->sessionID);
  // - local URI (as seen by remote client)
  fLocalURI=smlSrcTargLocURIToCharP(aContentP->target);
  fLocalName=smlSrcTargLocNameToCharP(aContentP->target);
  // - also remember URI to which first message was sent
  // %%% note: incoming ID is not a criteria, because it might be >1 due to
  //     client retrying something which it thinks is for the same session
  //if (fIncomingMsgID==1) {
  if (fOutgoingMsgID==0) {
    // this is the first message, remember first URI used to contact server
    // (or set preconfigured string from <externalurl>)
    if (getServerConfig()->fExternalURL.empty())
      fInitialLocalURI=fLocalURI; // use what client sends to us
    else
      fInitialLocalURI=getServerConfig()->fExternalURL; // use preconfigured URL
    // Many clients, including SCTS send the second login attempt with a MsgID>1,
    // and depending on how they handle RespURI, they might get a new session for that
    // -> so, just handle the case that a new session does not start with MsgID=1
    if (fIncomingMsgID>1) {
      PDEBUGPRINTFX(DBG_ERROR,(
        "New session gets first message with MsgID=%ld (should be 1). Might be due to retries, adjusting OutgoingID as well",
        fIncomingMsgID
      ));
      fOutgoingMsgID=fIncomingMsgID-1; // to make it match what client expects
    }
  }
  // - remote URI
  fRemoteURI=smlSrcTargLocURIToCharP(aContentP->source);
  fRemoteName=smlSrcTargLocNameToCharP(aContentP->source);
  // - RespURI (remote URI to respond to, if different from source)
  fRespondURI.erase();
  if (aContentP->respURI) {
    fRespondURI=smlPCDataToCharP(aContentP->respURI);
    DEBUGPRINTFX(DBG_PROTO,("RespURI specified = '%s'",fRespondURI.c_str()));
  }
  if (fRespondURI==fRemoteURI) fRespondURI.erase(); // if specified but equal to remote: act as if not specified
  // More checking if header was ok
  if (aBad) {
    // bad header, only do what is needed to get a status back to client
    fSessionAuthorized=false;
    fIncomingState=psta_init;
    fOutgoingState=psta_init;
    fNewOutgoingPackage=true;
    // issue header to make sure status can be sent back to client
    if (!fMsgNoResp)
      issueHeader(false); // issue header, do not prevent responses
  }
  else {  
    // check busy (or expired) case
    if (serverBusy()) {
      #ifdef APP_CAN_EXPIRE
    	if (getSyncAppBase()->fAppExpiryStatus!=LOCERR_OK) {
    		aStatusCommand.setStatusCode(511); // server failure (expired)
    		aStatusCommand.addItemString("License expired or invalid");
        PDEBUGPRINTFX(DBG_ERROR,("License expired or invalid - Please contact Synthesis AG to obtain license"));
      }
    	else
    	#endif
    	{
      	aStatusCommand.setStatusCode(101); // busy
      }
      issueHeader(false); // issue header, do not prevent responses
      AbortSession(0,true); // silently discard rest of commands
      return false; // header not ok
    }  
    // now check what state we are in
    if (fIncomingState==psta_idle) {  
      // Initialize
      // - session-wide authorization not yet there
      fSessionAuthorized=false;
      fMapSeen=false;
      // - session has started, we are processing first incoming
      //   package and generating first outgoing package
      //   (init, eventually changed to combined init/sync by <sync> in this package)
      fIncomingState=psta_init;
      fOutgoingState=psta_init;
      fNewOutgoingPackage=true;
    }
    // authorization check
    if (fIncomingState>=psta_init) {
      // now check authorization
      if (!fSessionAuthorized) {    
        // started, but not yet permanently authorized
        fMessageAuthorized=checkCredentials(
          smlSrcTargLocNameToCharP(aContentP->source), // user name in clear text according to SyncML 1.0.1
          aContentP->cred, // actual credentials
          aStatusCommand
        );
        // NOTE: aStatusCommand has now the appropriate status and chal (set by checkCredentials())
        // if credentials do not match, stop processing commands (but stay with the session)
        if (!fMessageAuthorized) {
          AbortCommandProcessing(aStatusCommand.getStatusCode());  
          PDEBUGPRINTFX(DBG_PROTO,("Authorization failed with status %hd, stop command processing",aStatusCommand.getStatusCode()));
        }
        // now determine if authorization is permanent or not
        if (fMessageAuthorized) {
          fAuthFailures=0; // reset count
          if (messageAuthRequired()) {
            // each message needs autorisation again (or no auth at all)
            // - 200 ok, next message needs authorization again (or again: none)
            fSessionAuthorized=false; // no permanent authorization
            aStatusCommand.setStatusCode(200);
            // - add challenge for next auth (different nonce)
            aStatusCommand.setChallenge(newSessionChallenge());
            PDEBUGPRINTFX(DBG_PROTO,("Authorization ok, but required again for subsequent messages: 200 + chal"));
          }
          else {
            // entire session is authorized
            fSessionAuthorized=true; // permanent authorization
            // - 212 authentication accepted (or 200 if none is reqired at all)          
            aStatusCommand.setStatusCode(requestedAuthType()==auth_none ? 200 : 212);
            // - add challenge for next auth (in next session, but as we support carry
            //   forward via using sessionID, we need to send one here as well)
            aStatusCommand.setChallenge(newSessionChallenge());
            PDEBUGPRINTFX(DBG_PROTO,("Authorization accepted: 212"));
          }
        }
      } // authorisation check
      else {
        // already authorized from previous message
        PDEBUGPRINTFX(DBG_PROTO,("Authorization ok from previous request: 200"));
        fMessageAuthorized=true;
      }
      // Start response message AFTER auth check, to allow issueHeader
      // to check auth state and customize the header accordingly (no
      // RespURI for failed auth for example)
      if (!fMsgNoResp) {
        issueHeader(false); // issue header, do not prevent responses
      }
    } // if started at least
  } // if not aBad
  // return startmessage status
  // debug info
  #ifdef SYDEBUG
  if (PDEBUGMASK & DBG_SESSION) {
    PDEBUGPRINTFX(DBG_SESSION,(
      "---> MessageStarted, Message %sauthorized, incoming state='%s', outgoing state='%s'",
      fMessageAuthorized ? "" : "NOT ",
      PackageStateNames[fIncomingState],
      PackageStateNames[fOutgoingState]
    ));
  	TLocalDataStorePContainer::iterator pos;
    for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
      // Show state of local datastores
      PDEBUGPRINTFX(DBG_SESSION,(
        "Local Datastore '%s': State=%s, %s%s sync, %s%s",
        (*pos)->getName(),
        (*pos)->getDSStateName(),
        (*pos)->isResuming() ? "RESUMED " : "",
        (*pos)->fSlowSync ? "SLOW" : "normal",
        SyncModeDescriptions[(*pos)->fSyncMode],
        (*pos)->fServerAlerted ? ", Server-Alerted" : ""
      ));
    }
  }
  #endif
  // final check for too many auth failures
  if (!fMessageAuthorized) {
    #ifdef NO_NONCE_OLD_BEAHVIOUR
    AbortSession(aStatusCommand.getStatusCode(),true); // local error
    // avoid special treatment of non-authorized message, we have aborted, this is enough
    fMessageAuthorized=true;
    #else
    // Unsuccessful auth, count this
    fAuthFailures++;
    PDEBUGPRINTFX(DBG_ERROR,(
      "Authorization failed %hd. time, (any reason), sending status %hd",
      fAuthFailures,
      aStatusCommand.getStatusCode()
    ));
    // - abort session after too many auth failures
    if (fAuthFailures>=MAX_AUTH_ATTEMPTS) {
      PDEBUGPRINTFX(DBG_ERROR,("Too many (>=%hd) failures, aborting session",MAX_AUTH_ATTEMPTS));
      AbortSession(400,true);
    }
    #endif        
  }  
  // returns false on BAD header (but true on wrong/bad/missing cred)
  return true;
} // TSyncServer::MessageStarted


void TSyncServer::MessageEnded(bool aIncomingFinal)
{
  bool alldone;
  TPackageStates newoutgoingstate,newincomingstate;
  TLocalDataStorePContainer::iterator pos;
  bool allFromClientOnly=false;
  
  // Incoming message ends here - what is following are commands initiated by the server
  // not directly related to a incoming command.
  PDEBUGENDBLOCK("SyncML_Incoming");
  // assume that outgoing package is NOT finished, so outgoing state does not change
  newoutgoingstate=fOutgoingState;
  // new incoming state depends on whether this message is final or not
  if ((aIncomingFinal || (fIncomingState==psta_supplement)) && fMessageAuthorized) {
    // Note: in supplement state, incoming final is not relevant (may or may not be present, there is
    //       no next phase anyway 
    // find out if this is a shortened session (no map phase) due to
    // from-client-only in all datastores
    if (!fCompleteFromClientOnly) {
      allFromClientOnly=true;
      for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
        // check sync modes
        if ((*pos)->isActive() && (*pos)->getSyncMode()!=smo_fromclient) {
          allFromClientOnly=false;
          break;
        }
      }
    }
    // determine what package comes next
    switch (fIncomingState) {
      case psta_init :
        newincomingstate=psta_sync;
        break;
      case psta_sync :
      case psta_initsync :
        // end of sync phase means end of session if all datastores are in from-client-only mode
        if (allFromClientOnly) {
          PDEBUGPRINTFX(DBG_PROTO+DBG_HOT,("All datastores in from-client-only mode: don't expect map phase from client"));
          newincomingstate=psta_supplement;
        }
        else {
          newincomingstate=psta_map;
        }
        break;
      case psta_map :
      case psta_supplement : // supplement state does not exit automatically
        // after map, eventually some supplement status/alert 222 messages are needed from client
        newincomingstate=psta_supplement;
        break;
      default:
        // by default, back to idle
        newincomingstate=psta_idle;
        break;
    } // switch
  }
  else {
    // not final or not authorized: no change in state
    newincomingstate=fIncomingState;
  }
  // show status before processing
  PDEBUGPRINTFX(DBG_SESSION,(
    "---> MessageEnded starts   : old incoming state='%s', old outgoing state='%s', %sNeedToAnswer",
    PackageStateNames[fIncomingState],
    PackageStateNames[fOutgoingState],
    fNeedToAnswer ? "" : "NO "
  ));
  // process
  if (isAborted()) {
    // actual aborting has already taken place
    PDEBUGPRINTFX(DBG_ERROR,("***** Session is flagged 'aborted' -> MessageEnded ends package and session"));
    newoutgoingstate=psta_idle;
    newincomingstate=psta_idle;
    fInProgress=false;
  } // if aborted
  else if (isSuspending()) {
    // only flagged for suspend - but datastores are not yet aborted, do it now
    AbortSession(514,true,getAbortReasonStatus());
    PDEBUGPRINTFX(DBG_ERROR,("***** Session is flagged 'suspended' -> MessageEnded ends package and session"));
    newoutgoingstate=psta_idle;
    newincomingstate=psta_idle;
    fInProgress=false;
  }
  else if (!fMessageAuthorized) {
    // not authorized messages will just be ignored, no matter if final or not,
    // so outgoing will NEVER be final on non-authorized messages
    // %%% before 1.0.4.9, this was fInProgress=true
    //  DEBUGPRINTFX(DBG_ERROR,("***** Message not authorized, ignore and DONT end package, session continues"));
    //  fInProgress=true; 
    PDEBUGPRINTFX(DBG_ERROR,("***** Message not authorized, ignore msg and terminate session"));
    fInProgress=false; 
  }
  else {
    // determine if session continues living or not
    // - if in other than idle state, session will continue
    fInProgress =
      (newincomingstate!=psta_idle) || // if not idle, we'll continue
      !fMessageAuthorized; // if not authorized, we'll continue as well (retrying auth)   
    // Check if we need to send an Alert 222 to get more messages of this package
    if (!aIncomingFinal) {
      // not end of incoming package
      #ifndef ALWAYS_CONTINUE222
      if (!fNeedToAnswer)
      #endif
      {
        #ifdef COMBINE_SYNCANDMAP
        // %%% make sure session gets to an end in case combined sync/map was used
        if (fMapSeen && fIncomingState==psta_map && fOutgoingState==psta_map) {
          DEBUGPRINTFX(DBG_HOT,("********** Incoming, non-final message in (combined)map state needs no answer -> force end of outgoing package"));          
          newoutgoingstate=psta_idle;
        }
        else
        #endif
        {
          // detected 222-loop on init here: when we have nothing to answer in init
          // and nothing is alerted -> break session
          // %%% not sure if this is always ok
          if (fIncomingState<=psta_init) {
            PDEBUGPRINTFX(DBG_ERROR,("############## Looks like if we were looping in an init-repeat loop -> force final"));
            fInProgress=false;
            fOutgoingState=psta_idle;
          }
          else {
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
      }
    }
    else {
      // end of package, finish processing package
      if (fIncomingState==psta_init) {
        // - try to load devinf from cache (only if we don't have both datastores and type info already)
        if (!fRemoteDataStoresKnown || !fRemoteDataTypesKnown) {
          SmlDevInfDevInfPtr_t devinfP;
          TStatusCommand dummystatus(this);  
          if (loadRemoteDevInf(getRemoteURI(),devinfP)) {
            // we have cached devinf, analyze it now
            localstatus sta = analyzeRemoteDevInf(devinfP);
            PDEBUGPRINTFX(DBG_ERROR,("devInf from Cache could not be analyzed: error=%hd",sta));
          }
        }
        // - if no DevInf for remote datastores cached or received yet,
        //   issue GET for it now
        if (!fRemoteDataStoresKnown) {
          // if we know datastores here, but not types, this means that remote does not have
          // CTCap, so it makes no sense to issue a GET again.
          #ifndef NO_DEVINF_GET
          // end of initialisation package, but datastores not known yet
          // (=no DevInf Put received) --> ask for devinf now
          PDEBUGPRINTFX(DBG_REMOTEINFO,("No DevInf received or cached, request DevInf using GET command"));
          TGetCommand *getcommandP = new TGetCommand(this);
          getcommandP->addTargetLocItem(SyncMLDevInfNames[fSyncMLVersion]);
          string devinftype=SYNCML_DEVINF_META_TYPE;
          addEncoding(devinftype);
          getcommandP->setMeta(newMetaType(devinftype.c_str()));
          ISSUE_COMMAND_ROOT(this,getcommandP);
          #endif
        }
      }
    }
    // make sure syncing local datastores get informed of end-of-<Sync>-message
    if (fIncomingState==psta_sync || fIncomingState==psta_initsync) {
      // end of an incoming message of the Sync Package
      // - let all local datastores know, this is now the time to generate
      //   <sync> commands, if needed
      // Note: if there are SyncEnd commands delayed, this means that this is
      //       not yet the time to start <sync> commands. Instead, when all
      //       queued SyncEnd commands are executed later, engEndOfSyncFromRemote()
      //       will be called with the endOfAllSyncCommands flag true instead
      //       of now. 
      for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
        (*pos)->engEndOfSyncFromRemote(aIncomingFinal && !delayedSyncEndsPending());
      }
    }
    // Detect outgoing package state transitions
    // - init to sync
    if (fOutgoingState==psta_init && newincomingstate>psta_init) {
      // new outgoing state is sync.
      // Note: In combined init&sync mode, sync command received in init state
      //       will set outgoing state from init to init-sync while processing message,
      //       so no transition needs to be detected here
      newoutgoingstate=psta_sync;
    }
    // - sync to map
    else if (
      (fOutgoingState==psta_sync || fOutgoingState==psta_initsync) && // outgoing is sync..
      (newincomingstate>=psta_initsync) && // ..and incoming has finished sync
      !allFromClientOnly // ..and this is not a session with all datastores doing from-client-only
    ) {
      // outgoing message belongs to Sync package
      // - ask all local datastores if they are finished with sync command generation
      alldone=true;
      for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
        alldone = alldone && (*pos)->isSyncDone();
      }
      if (alldone) {
        // outgoing state changes to map (or supplement if all datastores are from-client-only
        PDEBUGPRINTFX(DBG_HOT,("All datastores are done with generating <Sync>"));
        newoutgoingstate=psta_map;
        #ifdef COMBINE_SYNCANDMAP
        // %%% it seems as if 9210 needs combined Sync/Map package and 
        if (fMapSeen) {
          // prevent FINAL to be sent at end of message
          DEBUGPRINTFX(DBG_HOT,("********** Combining outgoing sync and map-response packages into one"));
          fOutgoingState=psta_map;
        }
        #endif
      }
    }
    // - map (or from-client-only sync) to idle
    else if (
      (fOutgoingState==psta_map && newincomingstate==psta_supplement) ||
      (allFromClientOnly && (fOutgoingState==psta_sync || fOutgoingState==psta_initsync))
    ) {
      // we are going back to idle now
      newoutgoingstate=psta_idle;
      // session ends if it doesn't need to continue for session-level reasons
      if (!sessionMustContinue()) {
        PDEBUGPRINTFX(DBG_HOT,("Session completed, now let datastores terminate all sync operations"));
        for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
          // finished with Map: end of sync
          (*pos)->engFinishDataStoreSync(); // successful
        }
        // session ends now
        fInProgress=false;
        // let custom PUTs which may transmit some session statistics etc. happen now
        issueCustomEndPut();
      }
    }
    // - if no need to answer (e.g. nothing to send back except OK status for SyncHdr),
    //   session is over now (as well)
    if (!fNeedToAnswer) fInProgress=false;
  } // else 
  // Now finish outgoing message
  #ifdef DONT_FINAL_BAD_AUTH_ATTEMPTS
  // - PREVENT final flag after failed auth attempts
  if(FinishMessage(
    fOutgoingState!=newoutgoingstate || fOutgoingState==psta_idle, // final when state changed or idle
    !fMessageAuthorized || serverBusy() // busy or unauthorized prevent final flag at any rate
  ))
  #else
  // - DO set final flag after failed auth attempts
  if(FinishMessage(
    !fMessageAuthorized || fOutgoingState!=newoutgoingstate || fOutgoingState==psta_idle, // final when state changed or idle
    serverBusy() // busy prevents final flag at any rate
  ))  
  #endif
  {
    // outgoing state HAS changed
    fOutgoingState=newoutgoingstate;
  }
  // Now update incoming state
  fIncomingState=newincomingstate;
  // show states
  PDEBUGPRINTFX(DBG_HOT,(
    "---> MessageEnded finishes : new incoming state='%s', new outgoing state='%s', %sNeedToAnswer",
    PackageStateNames[fIncomingState],
    PackageStateNames[fOutgoingState],
    fNeedToAnswer ? "" : "NO "
  ));
  // let all local datastores know that message has ended
  for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
    // let them know
    (*pos)->engEndOfMessage();
    // Show state of local datastores
    PDEBUGPRINTFX(DBG_HOT,(
      "Local Datastore '%s': State=%s, %s%s sync, %s%s",
      (*pos)->getName(),
      (*pos)->getDSStateName(),
      (*pos)->isResuming() ? "RESUMED " : "",
      (*pos)->fSlowSync ? "SLOW" : "normal",
      SyncModeDescriptions[(*pos)->fSyncMode],
      (*pos)->fServerAlerted ? ", Server-Alerted" : ""
    ));
  }
  // End of outgoing message
  PDEBUGPRINTFX(DBG_HOT,(
    "=================> Finished generating outgoing message #%ld, request=%ld",
    fOutgoingMsgID,
    getSyncAppBase()->requestCount()
  ));
  PDEBUGENDBLOCK("SyncML_Outgoing");
} // TSyncServer::MessageEnded


void TSyncServer::RequestEnded(bool &aHasData)
{
  // to make sure, finish any unfinished message
  FinishMessage(true); // final allowed, as this is an out-of-normal-order case anyway  
  // if we need to answer, we have data
  // - SyncML specs 1.0.1 says that server must always respond, even if message
  //   contains of a Status for the SyncHdr only 
  aHasData=true;
  // %%% first drafts of 1.0.1 said that SyncHdr Status only messages must not be sent...
  // aHasData=fNeedToAnswer; // %%%
  
  // now let all datastores know that request processing ends here (so they might
  // prepare for a thread switch)
  // terminate sync with all datastores
  TLocalDataStorePContainer::iterator pos;
  for (pos=fLocalDataStores.begin(); pos!=fLocalDataStores.end(); ++pos) {
    // now let them know that request has ended
    (*pos)->engRequestEnded();
  }
} // TSyncServer::RequestEnded


// Called at end of Request, returns true if session must be deleted
// returns flag if data to be returned. If response URI was specified
// different, it is returned in aRespURI, otherwise aRespURI is empty.
bool TSyncServer::EndRequest(bool &aHasData, string &aRespURI, uInt32 aReqBytes)
{
  // count incoming data
  fIncomingBytes+=aReqBytes;
  // let client or server do what is needed
  if (fMessageRetried) {
    // Message processing cancelled
    CancelMessageProcessing();
    // Nothing happened 
	  // - but count bytes 
    fOutgoingBytes+=fBufferedAnswerSize;
    PDEBUGPRINTFX(DBG_HOT,(
	    "========= Finished retried request with re-sending buffered answer (session %sin progress), incoming bytes=%ld, outgoing bytes=%ld",
	    fInProgress ? "" : "NOT ",
	    aReqBytes,
	    fBufferedAnswerSize
	  ));
	  aHasData=false; // we do not have data in the sml instance (but we have/had some in the retry re-send buffer)
  }
  else {
    // end request
	  RequestEnded(aHasData);
	  // count bytes
    fOutgoingBytes+=getOutgoingMessageSize();
	  PDEBUGPRINTFX(DBG_HOT,(
	    "========= Finished request (session %sin progress), processing time=%ld msec, incoming bytes=%ld, outgoing bytes=%ld",
	    fInProgress ? "" : "NOT ",
	    (sInt32)((getSystemNowAs(TCTX_UTC)-getLastRequestStarted()) * nanosecondsPerLinearTime / 1000000),
	    aReqBytes,
      getOutgoingMessageSize()
	  ));	  
	  // return RespURI (is empty if none specified or equal to message source URI)
	  aRespURI = fRespondURI;
  }
  if (!fInProgress) {
    // terminate datastores here already in case we are not in progress any more
    // here. If any of the datastores are in progress at this point, this is a
    // protocol violation, and therefore we return a 400.
    // Note: resetting the session later will also call TerminateDatastores, but then
    //       with a 408 (which is misleading when the session ends here due to protocol
    //       problem.
    TerminateDatastores(400);
  }
  //%%% moved to happen before end of SyncML_Outgoing 
  //PDEBUGENDBLOCK("SyncML_Incoming");
  if (fRequestMinTime>0) {
    // make sure we spent enough time with this request, if not, artificially extend time
    // - get number of seconds already spent
    sInt32 t =
      (sInt32)((getSystemNowAs(TCTX_UTC)-getLastRequestStarted()) / (lineartime_t)secondToLinearTimeFactor);
    // - delay if needed
    if (t<fRequestMinTime) {
  	  PDEBUGPRINTFX(DBG_HOT,(
  	    "requestmintime is set to %ld seconds, we have spent only %ld seconds so far -> sleeping %ld seconds",
  	    fRequestMinTime,
  	    t,
  	    fRequestMinTime-t
  	  ));
      CONSOLEPRINTF(("  ...delaying response by %ld seconds because requestmintime is set to %ld",fRequestMinTime,fRequestMinTime-t));
  	  sleepLineartime((lineartime_t)(fRequestMinTime-t)*secondToLinearTimeFactor);
    }
  }
  // thread might end here, so stop profiling
  TP_STOP(fTPInfo);
  #ifdef SYDEBUG
  // we are not the main thread any longer
  getDbgLogger()->DebugThreadOutputDone();
  #endif
  // return true if session is not in progress any more
  return(!fInProgress);
} // TSyncServer::EndRequest


// buffer answer in the session's buffer if transport allows it
Ret_t TSyncServer::bufferAnswer(MemPtr_t aAnswer, MemSize_t aAnswerSize)
{
  // get rid of previous buffered answer
  if (fBufferedAnswer)
    delete[] fBufferedAnswer;
  fBufferedAnswer=NULL;
  fBufferedAnswerSize=0;
  // save new answer (if not empty)
  if (aAnswer && aAnswerSize) {
    // allocate buffer
    fBufferedAnswer = new unsigned char[aAnswerSize];
    // copy data
    if (!fBufferedAnswer) return SML_ERR_NOT_ENOUGH_SPACE;
    memcpy(fBufferedAnswer,aAnswer,aAnswerSize);
    // save size
    fBufferedAnswerSize=aAnswerSize;
  }
  return SML_ERR_OK;
} // TSyncServer::bufferAnswer


// get buffered answer from the session's buffer if there is any
void TSyncServer::getBufferedAnswer(MemPtr_t &aAnswer, MemSize_t &aAnswerSize)
{
  aAnswer=fBufferedAnswer;
  aAnswerSize=fBufferedAnswerSize;  
  PDEBUGPRINTFX(DBG_HOT,(
    "Buffered answer read from session: %ld bytes",
    fBufferedAnswerSize
  ));
} // TSyncServer::getBufferedAnswer


// returns remaining time for request processing [seconds]
sInt32 TSyncServer::RemainingRequestTime(void)
{
  // if no request timeout specified, use session timeout
  sInt32 t = fRequestMaxTime ? fRequestMaxTime : getSessionConfig()->fSessionTimeout;
  // calculate number of remaining seconds
  return
    t==0 ?
      0x7FFFFFFF : // "infinite"
      t - (sInt32)((getSystemNowAs(TCTX_UTC)-getLastRequestStarted()) / (lineartime_t)secondToLinearTimeFactor);
} // TSyncServer::RemainingRequestTime





// process a Map command in context of server session
bool TSyncServer::processMapCommand(
  SmlMapPtr_t aMapCommandP,      // the map command contents
  TStatusCommand &aStatusCommand, // pre-set 200 status, can be modified in case of errors
  bool &aQueueForLater
)
{
  bool allok=false; // assume not ok
  localstatus sta;

  // remember that this session has seen a map command already
  fMapSeen=true;
  // Detecting a map command in supplement incomin state indicates a
  // client like funambol that send to many <final/> in pre-map phases
  // (such as in 222-Alert messages). So we reset the session state back
  // to incoming/outgoing map to correct this client bug
  if (fIncomingState==psta_supplement) {
    // back to map phase, as client apparently IS still in map phase, despite too many
    // <final/> sent
    PDEBUGPRINTFX(DBG_ERROR,(
      "Warning: detected <Map> command after end of Map phase - buggy client sent too many <final/>. Re-entering map phase to compensate"
    ));
    fIncomingState=psta_map;
    fOutgoingState=psta_map;
  }
  // find database(s)
  // - get relative URI of requested database
  const char *targetdburi = smlSrcTargLocURIToCharP(aMapCommandP->target);
  TLocalEngineDS *datastoreP = findLocalDataStoreByURI(targetdburi);
  if (!datastoreP) {
    // no such local datastore
    aStatusCommand.setStatusCode(404); // not found
  }
  else {
    // local datastore found
    // - maps can be processed when we are at least ready for early (chached by client from previous session) maps
    if (datastoreP->testState(dssta_syncmodestable)) {
      // datastore is ready 
      PDEBUGBLOCKFMT(("ProcessMap", "Processing items from Map command", "datastore=%s", targetdburi));
      allok=true; // assume all ok
      SmlMapItemListPtr_t nextnode = aMapCommandP->mapItemList;
      while (nextnode) {
        POINTERTEST(nextnode->mapItem,("MapItemList node w/o MapItem"));
        PDEBUGPRINTFX(DBG_HOT,(
          "Mapping remoteID='%s' to localID='%s'",
          smlSrcTargLocURIToCharP(nextnode->mapItem->source),
          smlSrcTargLocURIToCharP(nextnode->mapItem->target)
        ));
        sta = datastoreP->engProcessMap(
          #ifdef DONT_STRIP_PATHPREFIX_FROM_REMOTEIDS
          smlSrcTargLocURIToCharP(nextnode->mapItem->source),
          #else 
          relativeURI(smlSrcTargLocURIToCharP(nextnode->mapItem->source)),
          #endif
          relativeURI(smlSrcTargLocURIToCharP(nextnode->mapItem->target))
        );
        if (sta!=LOCERR_OK) {
          PDEBUGPRINTFX(DBG_ERROR,("    Mapping FAILED!"));
          aStatusCommand.setStatusCode(sta);
          allok=false;
          break;
        }
        // next mapitem
        nextnode=nextnode->next;
      } // while more mapitems
      // terminate Map command
      allok=datastoreP->MapFinishAsServer(allok,aStatusCommand);
      PDEBUGENDBLOCK("ProcessMap");
    }
    else {
      // we must queue the command for later execution
      aQueueForLater=true;
      allok=true; // ok for now, we'll re-execute this later
    }
  } // database found
  return allok;
} // TSyncServer::processMapCommand
    

// - start sync group
bool TSyncServer::processSyncStart(
  SmlSyncPtr_t aSyncP,            // the Sync element
  TStatusCommand &aStatusCommand, // pre-set 200 status, can be modified in case of errors
  bool &aQueueForLater // will be set if command must be queued for later (re-)execution
)
{
  // Init datastores for sync
  localstatus sta = initSync(
    smlSrcTargLocURIToCharP(aSyncP->target),  // local datastore
    smlSrcTargLocURIToCharP(aSyncP->source)  // remote datastore
  );
  if (sta!=LOCERR_OK) {
    aStatusCommand.setStatusCode(sta);
    return false;
  }
  // let local datastore prepare for sync as server
  // - let local process sync command
  bool ok=fLocalSyncDatastoreP->engProcessSyncCmd(aSyncP,aStatusCommand,aQueueForLater);
  // Note: ok means that the sync command is addressing existing datastores. However,
  //       it does not mean that the actual processing is already executed; aQueueForLater
  //       could be set!
  // if ok and not queued: update package states
  if (ok) {
    if (fIncomingState==psta_init || fIncomingState==psta_initsync) {
      // detected sync command in init package -> this is combined init/sync
      #ifdef SYDEBUG
      if (fIncomingState==psta_init)
        DEBUGPRINTFX(DBG_HOT,("<Sync> started init package -> switching to combined init/sync"));
      #endif
      // - set new incoming state
      fIncomingState=psta_initsync;
      // - also update outgoing state, if it is in init package
      if (fOutgoingState==psta_init)
        fOutgoingState=psta_initsync;
    }
    else if (fCmdIncomingState!=psta_sync) {
      DEBUGPRINTFX(DBG_ERROR,(
        "<Sync> found in wrong incoming package state '%s' -> aborting session",
        PackageStateNames[fCmdIncomingState]
      ));
      aStatusCommand.setStatusCode(403); // forbidden
      fLocalSyncDatastoreP->engAbortDataStoreSync(403,true); // abort, local problem
      ok=false;
    }
    else {
      // - show sync start    
      DEBUGPRINTFX(DBG_HOT,(
        "<Sync> started, cmd-incoming state='%s', incoming state='%s', outgoing state='%s'",
        PackageStateNames[fCmdIncomingState],
        PackageStateNames[fIncomingState],
        PackageStateNames[fOutgoingState]
      ));
    }
  }
  return ok;
} // TSyncServer::processSyncStart



// get next nonce string top be sent to remote party for subsequent MD5 auth
void TSyncServer::getNextNonce(const char *aDeviceID, string &aNextNonce)
{
  fLastNonce.erase();
  if (getServerConfig()->fAutoNonce) {
    // generate nonce out of source ref and session ID
    // This scheme can provide nonce carrying forward between 
    // sessions by initializing lastNonce with the srcRef/sessionid-1
    // assuming client to use nonce from last session.
    sInt32 sid;
    // use current day as nonce varying number
    sid = time(NULL) / 3600 / 24; 
    generateNonce(fLastNonce,aDeviceID,sid);
  }
  else {
    // get constant nonce (if empty, this is NO nonce)
    fLastNonce=getServerConfig()->fConstantNonce;
  }
  // return new nonce
  DEBUGPRINTFX(DBG_PROTO,("getNextNonce: created nonce='%s'",fLastNonce.c_str()));
  aNextNonce=fLastNonce;
} // TSyncServer::getNextNonce


// - get nonce string for specified deviceID
void TSyncServer::getAuthNonce(const char *aDeviceID, string &aAuthNonce)
{
  // if no device ID, use session default nonce
  if (!aDeviceID) {
    TSyncSession::getAuthNonce(aDeviceID,fLastNonce);
  }
  else {
    // Basic nonce mechanism needing no per-device storage:
    // - we have no stored last nonce, but we can re-create nonce used
    //   for last session with this device by the used algorithm
    if (getServerConfig()->fAutoNonce) {
      if (fLastNonce.empty()) {
        // none available, produce new one
        sInt32 sid;
        // use current day as nonce varying number
        sid = time(NULL) / 3600 / 24; 
        generateNonce(fLastNonce,aDeviceID,sid);
      }
    }
    else {
      // return constant nonce
      fLastNonce=getServerConfig()->fConstantNonce;
    }
  }
  DEBUGPRINTFX(DBG_PROTO,("getAuthNonce: current auth nonce='%s'",fLastNonce.c_str()));
  aAuthNonce=fLastNonce;
} // TSyncServer::getAuthNonce



// info about server status
bool TSyncServer::serverBusy(void)
{
	// return flag (which might have been set by some connection
	// limit code in sessiondispatch).
	// When app is expired, all server sessions are busy anyway
	#ifdef APP_CAN_EXPIRE
	return fSessionIsBusy || (getSyncAppBase()->fAppExpiryStatus!=LOCERR_OK);
	#else
	return fSessionIsBusy;
	#endif
} // TSyncServer::serverBusy


// access to config
TServerConfig *TSyncServer::getServerConfig(void)
{
  TServerConfig *scP;
  GET_CASTED_PTR(scP,TServerConfig,getSyncAppBase()->getRootConfig()->fAgentConfigP,DEBUGTEXT("no TServerConfig","ss1"));
  return scP;
} // TSyncServer::getServerConfig


// info about requested auth type
TAuthTypes TSyncServer::requestedAuthType(void)
{
  return getServerConfig()->fRequestedAuth;
} // TSyncServer::requestedAuthType


// check if auth type is allowed
bool TSyncServer::isAuthTypeAllowed(TAuthTypes aAuthType)
{
  return aAuthType>=getServerConfig()->fRequiredAuth;
} // TSyncServer::isAuthTypeAllowed


#ifdef ENGINEINTERFACE_SUPPORT

// Support for EngineModule common interface
// -----------------------------------------


#ifndef ENGINE_LIBRARY

// dummy server engine support to allow AsKey from plugins

#warning "using ENGINEINTERFACE_SUPPORT in old-style appbase-rooted environment. Should be converted to real engine usage later"

// Engine factory function for non-Library case
ENGINE_IF_CLASS *newEngine(void)
{
  // For real engine based targets, newEngine must create a target-specific derivate
  // of the engine, which then has a suitable newSyncAppBase() method to create the
  // appBase. For old-style environment, a generic TServerEngineInterface is ok, as this
  // in turn calls the global newSyncAppBase() which then returns the appropriate
  // target specific appBase. Here we just return a dummy server engine base. 
  return new TDummyServerEngineInterface;
} // newEngine

/// @brief returns a new application base.
TSyncAppBase *TDummyServerEngineInterface::newSyncAppBase(void)
{
  // For not really engine based targets, the appbase factory function is
  // a global routine (for real engine targets, it is a true virtual of
  // the engineInterface, implemented in the target's leaf engineInterface derivate.
  // - for now, use the global appBase creator routine
  return sysync::newSyncAppBase(); // use global factory function 
} // TDummyServerEngineInterface::newSyncAppBase


#else

// Real server engine support

/// @brief Executes next step of the session
/// @param aStepCmd[in/out] step command (STEPCMD_xxx):
///        - tells caller to send or receive data or end the session etc.
///        - instructs engine to abort or time out the session etc.
/// @param aInfoP[in] pointer to a TEngineProgressInfo structure, NULL if no progress info needed
/// @return LOCERR_OK on success, SyncML or LOCERR_xxx error code on failure
TSyError TSyncServer::SessionStep(uInt16 &aStepCmd, TEngineProgressInfo *aInfoP)
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
  	fEngineState = ses_done; // we are done
  }

  // handle pre-processed step command according to current engine state
  switch (fEngineState) {

    // Done state
    case ses_done :
      // session done, nothing happens any more
      aStepCmd = STEPCMD_DONE;
      sta = LOCERR_OK;
      break;

    // Waiting for SyncML request data
    case ses_needdata:
      switch (stepCmdIn) {
        case STEPCMD_GOTDATA :
          // got data, now start processing it
          fEngineState = ses_processing;
          aStepCmd = STEPCMD_OK;
          sta = LOCERR_OK;
          break;
      } // switch stepCmdIn for ces_processing
      break;

    // Waiting until SyncML answer data is sent
    case ses_dataready:
      switch (stepCmdIn) {
        case STEPCMD_SENTDATA :
          // sent data, now wait for next request
          fEngineState = ses_needdata;
          aStepCmd = STEPCMD_NEEDDATA;
          sta = LOCERR_OK;
          break;
      } // switch stepCmdIn for ces_processing
      break;


    // Ready for generation steps
    case ses_generating:
      switch (stepCmdIn) {
        case STEPCMD_STEP :
          sta = generatingStep(aStepCmd,aInfoP);
          break;
      } // switch stepCmdIn for ces_generating
      break;

    // Ready for processing steps
    case ses_processing:
      switch (stepCmdIn) {
        case STEPCMD_STEP :
          sta = processingStep(aStepCmd,aInfoP);
          break;
      } // switch stepCmdIn for ces_processing
      break;

  case numServerEngineStates:
      // invalid
      break;

  } // switch fEngineState

  // done
  return sta;
} // TSyncServer::SessionStep




// Step that processes SyncML request data
TSyError TSyncServer::processingStep(uInt16 &aStepCmd, TEngineProgressInfo *aInfoP)
{
  localstatus sta = LOCERR_WRONGUSAGE;
  InstanceID_t myInstance = getSmlWorkspaceID();
  Ret_t rc;
  
  // %%% non-functional at this time
  sta = LOCERR_NOTIMP;
  /*
  #error "%%% figure out encoding if we are starting a new session here"

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
    // - switch engine state to generating answer message (if any)
    aStepCmd = STEPCMD_OK;
    fEngineState = ses_generating;
    sta = LOCERR_OK;
  }
  else {
    // processing failed
    PDEBUGPRINTFX(DBG_ERROR,("===> smlProcessData failed, returned 0x%hX",(sInt16)rc));
    // dump the message that failed to process
    #ifdef SYDEBUG
    if (data) DumpSyncMLBuffer(data,datasize,false,rc);
    #endif    
    // abort the session (causing proper error events to be generated and reported back)
    AbortSession(LOCERR_PROCESSMSG, true);
    // session is now done
    fEngineState = ses_done;
    // step by itself is ok - let app continue stepping (to restart session or complete abort)
    aStepCmd = STEPCMD_OK;
    sta = LOCERR_OK;
  }
  */
  // done
  return sta;
} // TSyncServer::processingStep



// Step that generates SyncML answer data
TSyError TSyncServer::generatingStep(uInt16 &aStepCmd, TEngineProgressInfo *aInfoP)
{
  localstatus sta = LOCERR_WRONGUSAGE;
  bool done;

  // %%% non-functional at this time
  sta = LOCERR_NOTIMP;
  /*
  #error "%%% figure out encoding if we are starting a new session here"

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
  */
  // return status
  return sta;
} // TSyncServer::generatingStep


#endif // ENGINE_LIBRARY



/// @brief Get new session key to access details of this session
appPointer TSyncServer::newSessionKey(TEngineInterface *aEngineInterfaceP)
{
  return new TServerParamsKey(aEngineInterfaceP,this);
} // TSyncServer::newSessionKey




// Server runtime settings key
// ---------------------------

// Constructor
TServerParamsKey::TServerParamsKey(TEngineInterface *aEngineInterfaceP, TSyncServer *aServerSessionP) :
  inherited(aEngineInterfaceP,aServerSessionP),
  fServerSessionP(aServerSessionP)
{
} // TServerParamsKey::TServerParamsKey


// - read local session ID
static TSyError readLocalSessionID(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TServerParamsKey *mykeyP = static_cast<TServerParamsKey *>(aStructFieldsKeyP);
  return TStructFieldsKey::returnString(
    mykeyP->fServerSessionP->getLocalSessionID(),
    aBuffer,aBufSize,aValSize
  );
} // readLocalSessionID


// - read initial local URI
static TSyError readInitialLocalURI(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TServerParamsKey *mykeyP = static_cast<TServerParamsKey *>(aStructFieldsKeyP);
  return TStructFieldsKey::returnString(
    mykeyP->fServerSessionP->getInitialLocalURI(),
    aBuffer,aBufSize,aValSize
  );
} // readInitialLocalURI


// - read abort status
static TSyError readAbortStatus(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  appPointer aBuffer, memSize aBufSize, memSize &aValSize
)
{
  TServerParamsKey *mykeyP = static_cast<TServerParamsKey *>(aStructFieldsKeyP);
  return TStructFieldsKey::returnInt(
  	mykeyP->fServerSessionP->getAbortReasonStatus(),
    sizeof(TSyError),
    aBuffer,aBufSize,aValSize
  );
} // readAbortStatus



// - write abort status, which means aborting a session
TSyError writeAbortStatus(
  TStructFieldsKey *aStructFieldsKeyP, const TStructFieldInfo *aFldInfoP,
  cAppPointer aBuffer, memSize aValSize
)
{
  TServerParamsKey *mykeyP = static_cast<TServerParamsKey *>(aStructFieldsKeyP);
  // abort the session
  TSyError sta = *((TSyError *)aBuffer);
	mykeyP->fServerSessionP->AbortSession(sta, true);
  return LOCERR_OK;
} // writeAbortStatus



// accessor table for server session key
static const TStructFieldInfo ServerParamFieldInfos[] =
{  
  // valName, valType, writable, fieldOffs, valSiz
  { "localSessionID", VALTYPE_TEXT, false, 0, 0, &readLocalSessionID, NULL },
  { "initialLocalURI", VALTYPE_TEXT, false, 0, 0, &readInitialLocalURI, NULL },
  { "abortStatus", VALTYPE_INT16, true, 0, 0, &readAbortStatus, &writeAbortStatus },
};

// get table describing the fields in the struct
const TStructFieldInfo *TServerParamsKey::getFieldsTable(void)
{
  return ServerParamFieldInfos;
} // TServerParamsKey::getFieldsTable

sInt32 TServerParamsKey::numFields(void)
{
  return sizeof(ServerParamFieldInfos)/sizeof(TStructFieldInfo);
} // TServerParamsKey::numFields

// get actual struct base address
uInt8P TServerParamsKey::getStructAddr(void)
{
  // prepared for accessing fields in server session object
  return (uInt8P)fServerSessionP;
} // TServerParamsKey::getStructAddr


#endif // ENGINEINTERFACE_SUPPORT


} // namespace sysync

/* end of TSyncServer implementation */

// eof
