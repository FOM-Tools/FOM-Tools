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

#include <string>
#include <functional>
#include "RegionInfo.hpp"

namespace PageMerger{
  auto sLambda([](const RegionInfo& a,const RegionInfo& b) -> bool {return (a.size<b.size);});
  std::function<bool(const RegionInfo&,const RegionInfo&)> bySize(std::cref(sLambda));
}
bool MergePages(const std::string in,const std::string out,
		const std::function<bool(const RegionInfo&,const RegionInfo&)> *comp=&(PageMerger::bySize));
