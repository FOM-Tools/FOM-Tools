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

#ifndef __REGION_INFO_H
#define __REGION_INFO_H
#include <unistd.h>
#include <cstdint>

class RegionInfo{
 public:
  RegionInfo():addr(0),size(0),t_sec(0),t_nsec(0),pBegin(0),pEnd(0),flags(0){};
  uint64_t addr;//address of allocation
  size_t size; //size of allocation or count of pages
  long t_sec; //seconds of allocation
  long t_nsec;// nanoseconds of allocation
  uintptr_t pBegin; //start of page block
  uintptr_t pEnd; // end of page block
  int flags; //flags (bits for page stats, overlap for address ranges)
};

#endif
