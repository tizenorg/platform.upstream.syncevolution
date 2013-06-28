/**
 *  @File     binfile.cpp
 *
 *  @Author   Lukas Zeller (luz@synthesis.ch)
 *
 *  @brief TBinFileBase
 *    Simple record based binary file storage class
 *
 *    Copyright (c) 2003-2009 by Synthesis AG (www.synthesis.ch)
 *
 *  @Date 2006-03-28 : luz : extracted into separate file from TBinfileImplDS
 */

#include "prefix_file.h"

#include "binfilebase.h"

namespace sysync {

// TBinFileBase
// ============

// constructor
TBinFileBase::TBinFileBase() :
  fDestructed(false)
{
  fBinFileHeader.idword=0;
  fBinFileHeader.version=0;
  fBinFileHeader.headersize=sizeof(TBinFileHeader);
  fBinFileHeader.recordsize=0;
  fBinFileHeader.numrecords=0;
  fBinFileHeader.allocatedrecords=0;
  fBinFileHeader.uniquerecordid=0;
  fHeaderDirty=false;
  fExtraHeaderDirty=false;
  fExtraHeaderP=NULL;
  fExtraHeaderSize=0;
} // TBinFileBase::TBinFileBase


// destructor
TBinFileBase::~TBinFileBase()
{
  destruct();
} // TBinFileBase::~TBinFileBase


void TBinFileBase::destruct(void)
{
  if (!fDestructed) doDestruct();
  fDestructed=true;
} // TBinFileBase::destruct


void TBinFileBase::doDestruct(void)
{
  // make sure files are closed
  close();
} // TBinFileBase::doDestruct





// - set path to binary file containing the database
void TBinFileBase::setFileInfo(const char *aFilename,uInt32 aVersion,uInt32 aIdWord)
{
  close();
  fFilename=aFilename;
  fIdWord=aIdWord;
  fVersion=aVersion;
} // TBinFileBase::setFileInfo


// - try to open existing DB file according to params set with setFileInfo
bferr TBinFileBase::open(uInt32 aExtraHeadersize, void *aExtraHeaderP, TUpdateFunc aUpdateFunc)
{
  // make sure it is closed first
  close();
  // save extra header info
  fExtraHeaderSize=aExtraHeadersize;
  fExtraHeaderP=aExtraHeaderP;
  // try to open file for (binary) update
  if (!platformOpenFile(fFilename.c_str(),fopm_update))
    return BFE_NOTFOUND;
  // read header
  fHeaderDirty=false;
  platformSeekFile(0);
  if (!platformReadFile(&fBinFileHeader,sizeof(fBinFileHeader))) {
    close();
    return BFE_BADSTRUCT;
  }
  // check type and Version
  if (fBinFileHeader.idword!=fIdWord) {
    close();
    return BFE_BADTYPE;
  }
  if (fBinFileHeader.version!=fVersion) {
    // try to update file if update-func is provided
    if (aUpdateFunc) {
      // check if we can update (no data provided for update)
      uInt32 oldversion=fBinFileHeader.version;
      uInt32 newrecordsize=aUpdateFunc(oldversion,fVersion,NULL,NULL,0);
      if (newrecordsize) {
        // we can update from current to requested version
        // - allocate buffer for all records
        uInt32 numrecords = fBinFileHeader.numrecords;
        uInt32 oldrecordsize = fBinFileHeader.recordsize;
        void *oldrecords = malloc(numrecords * oldrecordsize);
        if (!oldrecords) return BFE_MEMORY;
        // - read all current records into memory
        readRecord(0,oldrecords,numrecords);
        // - truncate the file
        truncate();
        // - modify header fields
        fBinFileHeader.version=fVersion; // update version
        fBinFileHeader.recordsize=newrecordsize; // update record size
        fHeaderDirty=true; // header must be updated
        // - write new header
        flushHeader();
        // - now convert buffered records
        void *newrecord = malloc(newrecordsize);
        for (uInt32 i=0; i<numrecords; i++) {
          // call updatefunc to convert record
          if (aUpdateFunc(oldversion,fVersion,(void *)((uInt8 *)oldrecords+i*oldrecordsize),newrecord,oldrecordsize)) {
            // save new record
            uInt32 newi;
            newRecord(newi,newrecord);
          }
        }
        // - forget buffers
        free(newrecord);
        free(oldrecords);
      }
      else {
        // cannot update
        close();
        return BFE_BADVERSION;
      }
    }
    else {
      // cannot update
      close();
      return BFE_BADVERSION;
    }
  }
  // check extra header compatibility
  if (fBinFileHeader.headersize<sizeof(TBinFileHeader)+fExtraHeaderSize) {
    close();
    return BFE_BADSTRUCT;
  }
  // read extra header
  if (fExtraHeaderP) {
    platformReadFile(fExtraHeaderP,fExtraHeaderSize);
    fExtraHeaderDirty=false;
  }
  return BFE_OK;
} // TBinFileBase::open


// - create new DB file according to params set with setFileInfo
bferr TBinFileBase::create(uInt32 aRecordsize, uInt32 aExtraHeadersize, void *aExtraHeaderP, bool aOverwrite)
{
  bferr e;

  close();
  // try to open
  e=open(aExtraHeadersize,NULL); // do not pass our new header data in case there is an old file already
  if (e==BFE_NOTFOUND || aOverwrite) {
    close();
    // create new file
    if (!platformOpenFile(fFilename.c_str(),fopm_create))
      return BFE_IOERR; // could not create for some reason
    // prepare header
    fBinFileHeader.idword=fIdWord;
    fBinFileHeader.version=fVersion;
    fBinFileHeader.headersize=sizeof(TBinFileHeader)+aExtraHeadersize;
    fBinFileHeader.recordsize=aRecordsize;
    fBinFileHeader.numrecords=0;
    fBinFileHeader.allocatedrecords=0;
    fBinFileHeader.uniquerecordid=0;
    fHeaderDirty=true;
    // - link in the new extra header buffer
    fExtraHeaderP=aExtraHeaderP;
    fExtraHeaderDirty=true; // make sure it gets written
    // write entire header
    e=flushHeader();
  }
  else if (e==BFE_OK) {
    // already exists
    close();
    e=BFE_EXISTS;
  }
  return e;
} // TBinFileBase::create


// - close the file
bferr TBinFileBase::close(void)
{
  if (platformFileIsOpen()) {
    // remove empty space from end of file
    truncate(fBinFileHeader.numrecords);
    // write new header
    flushHeader();
    // close file
    platformCloseFile();
  }
  return BFE_OK;
} // TBinFileBase::close


// - close and delete file (full cleanup)
bferr TBinFileBase::closeAndDelete(void)
{
	close();
  // now delete
	return platformDeleteFile(fFilename.c_str()) ? BFE_OK : BFE_IOERR;
} // TBinFileBase::closeAndDelete



// - flush the header to the file
bferr TBinFileBase::flushHeader(void)
{
  // save header if dirty
  if (fHeaderDirty) {
    platformSeekFile(0);
    platformWriteFile(&fBinFileHeader,sizeof(fBinFileHeader));
    fHeaderDirty=false;
  }
  // save extra header if existing and requested
  if (fExtraHeaderP && fExtraHeaderDirty) {
    platformSeekFile(sizeof(fBinFileHeader));
    platformWriteFile(fExtraHeaderP,fExtraHeaderSize);
    fExtraHeaderDirty=false;
  }
  // make sure data is really flushed in case we get improperly
  // destructed
  platformFlushFile();
  return BFE_OK;
} // TBinFileBase::flushHeader


// - truncate to specified number of records
bferr TBinFileBase::truncate(uInt32 aNumRecords)
{
  if (!platformFileIsOpen()) return BFE_NOTOPEN;
  platformTruncateFile(fBinFileHeader.headersize+aNumRecords*fBinFileHeader.recordsize);
  fBinFileHeader.numrecords=aNumRecords;
  fBinFileHeader.allocatedrecords=aNumRecords;
  fHeaderDirty=true;
  return BFE_OK;
} // TBinFileBase::truncate


// - read by index
bferr TBinFileBase::readRecord(uInt32 aIndex, void *aRecordData, uInt32 aNumRecords)
{
  if (!platformFileIsOpen()) return BFE_NOTOPEN;
  if (aNumRecords==0) return BFE_OK;
  // find record position in file
  if (aIndex+aNumRecords>fBinFileHeader.numrecords)
    return BFE_BADINDEX; // not enough records to read
  if (!platformSeekFile(fBinFileHeader.headersize+aIndex*fBinFileHeader.recordsize))
    return BFE_BADINDEX;
  // read data now
  if (!platformReadFile(aRecordData,fBinFileHeader.recordsize*aNumRecords))
    return BFE_IOERR; // could not read as expected
  return BFE_OK;
} // TBinFileBase::readRecord


// - update by index
bferr TBinFileBase::updateRecord(uInt32 aIndex, const void *aRecordData, uInt32 aNumRecords)
{
  if (!platformFileIsOpen()) return BFE_NOTOPEN;
  if (aNumRecords==0) return BFE_OK; // nothing to do
  // find record position in file
  if (aIndex+aNumRecords>fBinFileHeader.numrecords)
    return BFE_BADINDEX; // trying to update more records than actually here
  if (!platformSeekFile(fBinFileHeader.headersize+aIndex*fBinFileHeader.recordsize))
    return BFE_BADINDEX;
  // write data now
  if (!platformWriteFile(aRecordData,fBinFileHeader.recordsize*aNumRecords))
    return BFE_IOERR; // could not read as expected
  return BFE_OK;
} // TBinFileBase::updateRecord


// - new record, returns new index
bferr TBinFileBase::newRecord(uInt32 &aIndex, const void *aRecordData)
{
  if (!platformFileIsOpen()) return BFE_NOTOPEN;
  // go to end of file
  if (!platformSeekFile(fBinFileHeader.headersize+fBinFileHeader.numrecords*fBinFileHeader.recordsize))
    return BFE_IOERR;
  // write a new record
  if (!platformWriteFile(aRecordData,fBinFileHeader.recordsize))
    return BFE_IOERR; // could not read as expected
  // update header
  aIndex=fBinFileHeader.numrecords++; // return index of new record
  fBinFileHeader.allocatedrecords++;
  fHeaderDirty=true;
  return BFE_OK;
} // TBinFileBase::newRecord


// - delete record
bferr TBinFileBase::deleteRecord(uInt32 aIndex)
{
  if (!platformFileIsOpen()) return BFE_NOTOPEN;
  if (aIndex>=fBinFileHeader.numrecords) return BFE_BADINDEX;
  if (aIndex<fBinFileHeader.numrecords-1) {
    // we need to move last record
    void *recP = malloc(fBinFileHeader.recordsize);
    if (!recP) return BFE_IOERR; // no memory
    // read last record
    readRecord(fBinFileHeader.numrecords-1,recP);
    // write it to new position
    updateRecord(aIndex,recP);
    free(recP);
  }
  fBinFileHeader.numrecords--;
  fHeaderDirty=true;
  return BFE_OK;
} // TBinFileBase::deleteRecord

} // namespace sysync

/* end of TBinFileBase implementation */

// eof