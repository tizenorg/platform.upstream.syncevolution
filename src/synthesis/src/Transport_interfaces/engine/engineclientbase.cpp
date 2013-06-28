/*
 *  TEngineClientBase
 *    Engine library specific descendant of TSyncClientBase
 *    Global object, manages starting of client sessions.
 *
 *  Copyright (c) 2002-2009 by Synthesis AG (www.synthesis.ch)
 *
 *  2007-09-04 : luz : Created
 *
 */


#include "prefix_file.h"
#include "engine_client.h"
#include "engineclientbase.h"


namespace sysync {


// write to platform's "console", whatever that is
void AppConsolePuts(const char *aText)
{
  // Just print to platform's console
  PlatformConsolePuts(aText);
} // AppConsolePuts


// TEngineCommConfig
// =================
// (dummy at this time)


// config constructor
TEngineCommConfig::TEngineCommConfig(TConfigElement *aParentElementP) :
  TCommConfig("engineclient",aParentElementP)
{
  // do not call clear(), because this is virtual!
} // TEngineCommConfig::TXPTCommConfig


// config destructor
TEngineCommConfig::~TEngineCommConfig()
{
  // nop by now
} // TEngineCommConfig::~TXPTCommConfig


// init defaults
void TEngineCommConfig::clear(void)
{
  // init defaults
  // %%% none for now
  // clear inherited
  inherited::clear();
} // TEngineCommConfig::clear


#ifndef HARDCODED_CONFIG

// XPT transport config element parsing
bool TEngineCommConfig::localStartElement(const char *aElementName, const char **aAttributes, sInt32 aLine)
{
  // checking the elements
  /*
  if (strucmp(aElementName,"xxx")==0)
    expectBool(fXXX);
  else
  */
    return inherited::localStartElement(aElementName,aAttributes,aLine);
  // ok
  return true;
} // TEngineCommConfig::localStartElement

#endif


// resolve
void TEngineCommConfig::localResolve(bool aLastPass)
{
  if (aLastPass) {
    // check for required settings
    // %%% tbd
  }
  // resolve inherited
  inherited::localResolve(aLastPass);
} // TEngineCommConfig::localResolve



// TEngineClientBase
// =================


// constructor
TEngineClientBase::TEngineClientBase() :
  TSyncClientBase()
{
  // init
} // TEngineClientBase::TEngineClientBase


// destructor
TEngineClientBase::~TEngineClientBase()
{
  fDeleting=true; // flag deletion to block calling critical (virtual) methods
  // clean up
  // %%%
} // TEngineClientBase::~TEngineClientBase



// factory methods of Rootconfig
// =============================


// create default transport config
void TEngineClientRootConfig::installCommConfig(void)
{
  // engine API needs no config at this time, commconfig is a NOP dummy for now
  fCommConfigP=new TEngineCommConfig(this);
} // TEngineClientRootConfig::installCommConfig


#ifndef HARDCODED_CONFIG

bool TEngineClientRootConfig::parseCommConfig(const char **aAttributes, sInt32 aLine)
{
  // engine API needs no config at this time
  return false;
} // TEngineClientRootConfig::parseCommConfig

#endif


} // namespace sysync

// eof
