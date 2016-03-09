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
    //from http://stackoverflow.com/questions/7758580/writing-your-own-stl-container/7759622#7759622
    // class iterator{
    //   iterator();
    //   iterator(const iterator&);
    //   ~iterator();
    //   typedef difference_type std::ptrdiff_t;
    //   typedef size_type std::size_t;
    //   typedef reference MemRecord&;
    //   typedef pointer MemRecord*;
    //   typedef const_pointer const MemRecord*;
    //   typedef const_reference const MemRecord&;
    //   typedef std::random_access_iterator_tag iterator_category; //or another tag      
    //   iterator& operator=(const iterator&);
    //   bool operator==(const iterator&) const;
    //   bool operator!=(const iterator&) const;
    //   bool operator<(const iterator&) const; //optional
    //   bool operator>(const iterator&) const; //optional
    //   bool operator<=(const iterator&) const; //optional
    //   bool operator>=(const iterator&) const; //optional

    //   iterator& operator++();
    //   iterator operator++(int); //optional
    //   iterator& operator--(); //optional
    //   iterator operator--(int); //optional
    //   iterator& operator+=(size_type); //optional
    //   iterator operator+(size_type) const; //optional
    //   friend iterator operator+(size_type, const iterator&); //optional
    //   iterator& operator-=(size_type); //optional            
    //   iterator operator-(size_type) const; //optional
    // };

    // class const_iterator {
    // public:
    //   typedef difference_type std::ptrdiff_t;
    //   typedef size_type std::size_t;
    //   typedef reference MemRecord&;
    //   typedef pointer MemRecord*;
    //   typedef const_pointer const MemRecord*;
    //   typedef const_reference const MemRecord&;
    //   typedef std::random_access_iterator_tag iterator_category; //or another tag

    //   const_iterator ();
    //   const_iterator (const const_iterator&);
    //   const_iterator (const iterator&);
    //   ~const_iterator();

    //   const_iterator& operator=(const const_iterator&);
    //   bool operator==(const const_iterator&) const;
    //   bool operator!=(const const_iterator&) const;
    //   bool operator<(const const_iterator&) const; //optional
    //   bool operator>(const const_iterator&) const; //optional
    //   bool operator<=(const const_iterator&) const; //optional
    //   bool operator>=(const const_iterator&) const; //optional

    //   const_iterator& operator++();
    //   const_iterator operator++(int); //optional
    //   const_iterator& operator--(); //optional
    //   const_iterator operator--(int); //optional
    //   const_iterator& operator+=(size_type); //optional
    //   const_iterator operator+(size_type) const; //optional
    //   friend const_iterator operator+(size_type, const const_iterator&); //optional
    //   const_iterator& operator-=(size_type); //optional            
    //   const_iterator operator-(size_type) const; //optional
    //   difference_type operator-(const_iterator) const; //optional

    //   const_reference operator*() const;
    //   const_pointer operator->() const;
    //   const_reference operator[](size_type) const; //optional
    // };
    // typedef std::reverse_iterator<iterator> reverse_iterator; //optional
    // typedef std::reverse_iterator<const_iterator> const_reverse_iterator; //optional
    // iterator begin();
    // const_iterator begin() const;
    // const_iterator cbegin() const;
    // iterator end();
    // const_iterator end() const;
    // const_iterator cend() const;
    // reverse_iterator rbegin(); //optional
    // const_reverse_iterator rbegin() const; //optional
    // const_reverse_iterator crbegin() const; //optional
    // reverse_iterator rend(); //optional
    // const_reverse_iterator rend() const; //optional
    // const_reverse_iterator crend() const; //optional

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

  //  class FileStats;
  
  class Writer{
  public:
    Writer(std::string fileName,int compress,size_t bucketSize);
    Writer() = delete;
    ~Writer();
    bool writeRecord(const MemRecord& r);
    bool writeRecord(const RecordIndex& r);
    bool writeRecord(const void* hdr);
    bool closeFile(bool flush=false);
    bool reopenFile(bool seekEnd=true);
  private:
    time_t getProcessStartTime();
    bool parseCmdline(char* buff,size_t *len);
    std::string m_fileName;
    size_t m_nRecords;
    size_t m_maxDepth;
    int m_fileHandle;
    bool m_fileOpened;
    char* m_bucket;
    char* m_cBucket;
    int m_compress;
    size_t m_bucketSize;
    FileStats* m_stats;
  };

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
    friend class FOM_mallocHook::Writer;
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
      size_t CmdLength; //length of command-line
      char* CmdLine;// commandline string
    } *m_hdr;
  };

}

#endif
