/*  
 *  File:    myadapter.h
 *
 *  Author:  Beat Forster (bfo@synthesis.ch)
 *
 *  This bridge allows a OceanBlue module
 *  which needn't to be changed for different
 *  DBApi database adapters
 *
 *  Copyright (c) 2008-2009 by Synthesis AG (www.synthesis.ch)
 *
 */

#ifndef MYADAPTER_H
#define MYADAPTER_H


//*** defs used for unified OceanBlue ***
#define MyAdapter_Name   "SnowWhite"
#define MyAdapter_Module  SnowWhite_Module
#define MyAdapter_Session SnowWhite_Session
#define MyAdapter         SnowWhite

#include "snowwhite.h"


#endif // MYADAPTER_H
/* eof */
