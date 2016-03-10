/*
 *  Copyright (c) CERN 2015
 *
 *  Authors:
 *      Nathalie Rauschmayr <nathalie.rauschmayr_ at _ cern _dot_ ch>
 *      Sami Kama <sami.kama_ at _ cern _dot_ ch>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */


#ifndef __MALLOC_HOOKS_STREAMER_HPP
#define __MALLOC_HOOKS_STREAMER_HPP

#ifndef __NO_FORK_SUPPORT__
#include <pthread.h>
#endif
#include <string>
#include <vector>
#include <cstdint>
#include <iostream>
#include <ctime>
#include "config-FOMTools.h"
#ifdef ZLIB_FOUND
#include "zlib.h"
#endif
#ifdef BZip2_FOUND
#include "bzlib.h"
#endif

namespace FOM_mallocHook{
  typedef unsigned int index_t;
  struct header{
    uint64_t tstart;//time when malloc is called
    uint64_t treturn;// time when real malloc returns
    uint64_t tend;// time when record is ready for serialization
    char allocType;// type of the allocation 0-free 1-malloc 2-realloc 3-calloc
    uintptr_t addr;//returned address
    size_t size; //size of allocation
    int count; // # of stacks in record
  }__attribute__((packed));
  //
  // Just keeps header information in local variables, indices are located in pre-allocated memory locations.
  //
  class MemRecord{
  public:
    enum OVERLAP_TYPE{Undefined=0,//Overlap is meaningless (i.e when reading from file)
		      Underflow=1,// malloc begins before the region start and ends before region ends (ms<rs<me<re)
		      Overflow=2, // malloc begins after region start and ends after region ends (rs<ms<re<me)
		      Subset=3, // malloc starts after region start and ends before region ends (rs<ms<me<re)
		      Superset=4,// malloc starts before region start and end after region ends (ms<rs<re=<me)
		      Exact=5 //malloc range matches the scanned region (rs=ms<me=re)
    };
    MemRecord(void*);
    ~MemRecord();
    uintptr_t getFirstPage() const;
    uintptr_t getLastPage() const ;
    uint64_t getTStart()const;
    uint64_t getTReturn() const;
    uint64_t getTEnd()const;
    uintptr_t getAddr() const;
    size_t getSize() const;
    char getAllocType() const;
    const index_t* const getStacks(int *count) const;
    std::vector<index_t> getStacks() const;
    const FOM_mallocHook::header* const getHeader() const;
    OVERLAP_TYPE getOverlap() const;
    void setOverlap(OVERLAP_TYPE);
  private:
    FOM_mallocHook::header m_h;
    FOM_mallocHook::index_t* m_stacks;
    OVERLAP_TYPE m_overlap;
  };

  //
  //RecordIndex. Keeps a reference to already existing memory location (such as a mmapped file)
  //
  class RecordIndex{
  public:
    RecordIndex(const FOM_mallocHook::header*);
    ~RecordIndex();
    uintptr_t getFirstPage() const;
    uintptr_t getLastPage() const ;
    uintptr_t getAddr() const;
    uint64_t getTStart()const;
    uint64_t getTReturn() const;
    uint64_t getTEnd()const;
    size_t getSize() const;
    char getAllocType() const;
    const index_t* const getStacks(int *count) const;
    std::vector<index_t> getStacks() const;
    const FOM_mallocHook::header* const getHeader() const;
    MemRecord::OVERLAP_TYPE getOverlap() const;
    void setOverlap(MemRecord::OVERLAP_TYPE);
  private:
    const FOM_mallocHook::header *m_h;
    MemRecord::OVERLAP_TYPE m_overlap;
  };

  //
  //FullRecord. Keeps a copy of full record in private memory
  //
  class FullRecord{
  public:
    FullRecord(const FOM_mallocHook::header*);
    FullRecord(const FullRecord& );
    ~FullRecord();
    uintptr_t getFirstPage() const;
    uintptr_t getLastPage() const ;
    uint64_t getTStart()const;
    uint64_t getTReturn() const;
    uint64_t getTEnd()const;
    uintptr_t getAddr() const;
    size_t getSize() const;
    char getAllocType() const;
    const index_t* const getStacks(int *count) const;
    std::vector<index_t> getStacks() const;
    const FOM_mallocHook::header* const getHeader() const;
    MemRecord::OVERLAP_TYPE getOverlap() const;
    void setOverlap(MemRecord::OVERLAP_TYPE);
  private:
    FOM_mallocHook::header *m_h;
    MemRecord::OVERLAP_TYPE m_overlap;
  };

  class FileStats;
  class Reader{
  public:
    Reader(std::string fileName);
    Reader()=delete;
    ~Reader();
    //const MemRecord&  readNext();
    const MemRecord&  at(size_t);
    size_t size();
    const FOM_mallocHook::FileStats* getFileStats() const;

  private:
    int m_fileHandle;
    size_t m_fileLength;
    std::string m_fileName;
    void *m_fileBegin;
    //MemRecord m_curr;
    std::vector<MemRecord> m_records;
    FOM_mallocHook::FileStats* m_fileStats;
    bool m_fileOpened;

  };

  class IndexingReader{
  public:
    IndexingReader(std::string fileName,unsigned int indexPeriod=100);
    IndexingReader()=delete;
    ~IndexingReader();
    //const MemRecord&  readNext();
    const RecordIndex at(size_t);
    size_t size();
    size_t indexedSize();
    const FOM_mallocHook::FileStats* getFileStats() const;
  private:
    RecordIndex& seek(const RecordIndex& start, uint offset);
    int m_fileHandle;
    size_t m_fileLength;
    std::string m_fileName;
    void *m_fileBegin;
    std::vector<RecordIndex> m_records;
    FOM_mallocHook::FileStats* m_fileStats;
    bool m_fileOpened;
    uint m_period;
    uint m_remainder;
    size_t m_lastIndex;
    size_t m_numRecords;
    const FOM_mallocHook::header* m_lastHdr;    
  };

  struct BucketStats{
    size_t itemsInBucket;
    size_t uncompressedSize;
    size_t compressedSize;
    uint64_t compressionTime;
  };
  //  Writers;
  
    class WriterBase{
    public:
      WriterBase(std::string fileName,int compress,size_t bucketSize);
      WriterBase() = delete;
      virtual ~WriterBase();
      virtual void writeRecord(const MemRecord& r)=0;
      virtual void writeRecord(const RecordIndex& r)=0;
      virtual void writeRecord(const void* hdr)=0;
      virtual bool closeFile(bool flush=false)=0;
      virtual bool reopenFile(bool seekEnd=true)=0;
    protected:
      std::string m_fileName;
      size_t m_nRecords;
      size_t m_maxDepth;
      int m_fileHandle;
      bool m_fileOpened;
      int m_compress;
      size_t m_bucketSize;
      FileStats* m_stats;
      time_t getProcessStartTime();
      bool parseCmdline(char* buff,size_t *len);
    };

  class PlainWriter:public WriterBase{
  public:
    PlainWriter(std::string fileName,int compress,size_t bucketSize);
    PlainWriter() = delete;
    ~PlainWriter();
    void writeRecord(const MemRecord& r);
    void writeRecord(const RecordIndex& r);
    void writeRecord(const void* hdr);
    bool closeFile(bool flush=false);
    bool reopenFile(bool seekEnd=true);
  };

#ifdef ZLIB_FOUND
  class ZlibWriter:public WriterBase{
  public:
    ZlibWriter(std::string fileName,int compress,size_t bucketSize);
    ZlibWriter() = delete;
    ~ZlibWriter();
    void writeRecord(const MemRecord& r);
    void writeRecord(const RecordIndex& r);
    void writeRecord(const void* hdr);
    bool closeFile(bool flush=false);
    bool reopenFile(bool seekEnd=true);
  private:
    void compressBuffer();
    size_t m_nRecordsInBuffer;
    size_t m_bucketOffset;
    size_t m_compBuffLen;
    int m_compLevel;
    BucketStats m_bs;
    uint8_t *m_buff;
    uint8_t *m_cBuff;    
  };
#endif

#ifdef BZip2_FOUND
  class BZip2Writer:public WriterBase{
  public:
    BZip2Writer(std::string fileName,int compress,size_t bucketSize){};
    BZip2Writer() = delete;
    ~BZip2Writer(){};
    void writeRecord(const MemRecord& r){};
    void writeRecord(const RecordIndex& r){};
    void writeRecord(const void* hdr){};
    bool closeFile(bool flush=false){};
    bool reopenFile(bool seekEnd=true){};
  private:
    void compressBuffer(size_t lenB){};
    size_t m_nRecordsInBuffer;
    size_t m_bucketOffset;
    BucketStats m_bs;
    char *m_buff;
    char *m_cBuff;    
  };
#endif
  
#ifdef LibLZMA_FOUND
  class LZMAWriter:public WriterBase{
public:
  LZMAWriter(std::string fileName,int compress,size_t bucketSize){};
  LZMAWriter() = delete;
  ~LZMAWriter(){};
  void writeRecord(const MemRecord& r){};
  void writeRecord(const RecordIndex& r){};
  void writeRecord(const void* hdr){};
  bool closeFile(bool flush=false){};
  bool reopenFile(bool seekEnd=true){};
private:
  void compressBuffer(size_t lenB){};
  size_t m_nRecordsInBuffer;
  size_t m_bucketOffset;
  BucketStats m_bs;
  char *m_buff;
  char *m_cBuff;    
};
#endif
    
  class FileStats{
public:
  FileStats();
  ~FileStats();
  //getters
  int      getVersion()const;
  size_t   getNumRecords()const;
  size_t   getMaxStackLen() const;
  std::vector<std::string> getCmdLine()const;
  uint64_t getStartTime() const;
  uint64_t getStartUTC() const;//in ns from utc 0
  uint32_t getPid()const;
  int      getCompression()const;
  size_t   getBucketSize()const;
  size_t   getNumBuckets()const;

  //setters
  void setVersion(int);
  void setNumRecords(size_t);
  void setStackDepthLimit(size_t);
  void setCmdLine(char*,size_t);
  void setStartTime(uint64_t t);
  void setStartTimeUTC(uint64_t t);
  void setPid(uint32_t);
  void setCompression(int Comp);
  void setBucketSize(size_t bsize);
  void setNumBuckets(size_t bsize);

  int read(int fd,bool keepOffset=true);
  int write(int fd,bool keepOffset=true)const;
  int read(std::istream &in);
  int write(std::ostream &out)const;
  std::ostream& print(std::ostream &out=std::cout)const;
private:
  friend class FOM_mallocHook::WriterBase;
  struct fileHdr{
  char key[4]; //HEADER MARKER
  int ToolVersion; //Version used to generate
  int Compression;// 0 -no compression 10000x zlib
  size_t NumRecords; //Number of records in file
  size_t MaxStacks; //Max number of stacks 
  size_t BucketSize;// Size of compressed buffer, 0 otherwise
  size_t NumBuckets;// Number of buckets in file
  uint32_t Pid;//PID of process
  uint64_t StartTime;// Start time of process in machine time
  uint64_t StartTimeUtc;// Start time of process in UTC
  uint64_t CompressionHeaderSize;
  size_t CmdLength; //length of command-line
  char* CmdLine;// commandline string
} *m_hdr;
};
}

#endif
