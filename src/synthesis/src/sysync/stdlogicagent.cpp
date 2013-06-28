/**
 *  @File     stdlogicagent.cpp
 *
 *  @Author   Lukas Zeller (luz@synthesis.ch)
 *
 *  @brief TStdLogicAgent
 *    Agent (=server or client session) for standard database logic implementations, see @ref TStdLogicDS
 *
 *    Copyright (c) 2001-2009 by Synthesis AG (www.synthesis.ch)
 *
 *  @Date 2005-09-23 : luz : created from custdbagent
 */
/*
 */

// includes
#include "prefix_file.h"
#include "stdlogicagent.h"


/*
 * Implementation of TStdLogicAgent
 */

/* public TStdLogicAgent members */

#ifdef SYSYNC_CLIENT

TStdLogicAgent::TStdLogicAgent(TSyncClientBase *aClientBaseP, const char *aSessionID) :
  TSyncClient(aClientBaseP,aSessionID)
{
  // nop
} // TStdLogicAgent::TStdLogicAgent

#else

TStdLogicAgent::TStdLogicAgent(TSyncAppBase *aAppBaseP, TSyncSessionHandle *aSessionHandleP, const char *aSessionID) :
  TSyncServer(aAppBaseP, aSessionHandleP, aSessionID)
{
  // nop
  InternalResetSession();
} // TStdLogicAgent::TStdLogicAgent

#endif


// destructor
TStdLogicAgent::~TStdLogicAgent()
{
  // make sure everything is terminated BEFORE destruction of hierarchy begins
  TerminateSession();
} // TStdLogicAgent::~TStdLogicAgent


// Terminate session
void TStdLogicAgent::TerminateSession()
{
  if (!fTerminated) {
    InternalResetSession();
  }
  inherited::TerminateSession();
} // TStdLogicAgent::TerminateSession



// Reset session
void TStdLogicAgent::InternalResetSession(void)
{
} // TStdLogicAgent::InternalResetSession


// Virtual version
void TStdLogicAgent::ResetSession(void)
{
  // do my own stuff
  InternalResetSession();
  // let ancestor do its stuff
  inherited::ResetSession();
} // TStdLogicAgent::ResetSession


/* end of TStdLogicAgent implementation */

// eof
