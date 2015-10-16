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

#ifndef __REGION_FINDER_H
#define __REGION_FINDER_H
#include <string>
#include <vector>
#include "RegionInfo.hpp"
#include "Streamers.hpp"
namespace FOM_mallocHook{
  class Reader;
  class RegionFinder{
  public:
    enum ALLOCTIME{BEFORE=-1,ANYTIME=0,AFTER=1};
    RegionFinder(const std::string & mallocFile);
    ~RegionFinder();
    std::vector<FOM_mallocHook::MemRecord> getAllocations(const RegionInfo&,ALLOCTIME t=ANYTIME)const;
    std::vector<std::vector<FOM_mallocHook::MemRecord> > getAllocationSets(const std::vector<RegionInfo> &,ALLOCTIME t=ANYTIME)const;
  private:
    FOM_mallocHook::Reader *m_rdr;
  };
  
}//end namespace
#endif
