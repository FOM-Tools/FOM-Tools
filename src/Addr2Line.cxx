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

/* Translates function+offsets into sourcelines. First it parses the symbol 
   lookup table and maps file. Given the instruction pointer, the corresonding
   library is determined. Executes then eu-addr2line and writes the result into
   an output file. */

#include "FOMTools/Addr2Line.hpp" 
#include <algorithm>

std::string execute(std::string command) {
    const char * cmd = command.c_str();
    FILE* pipe = popen( cmd, "r");
    if (!pipe) return "Error";
    char buffer[128];
    std::string result = "";
    while(!feof(pipe)) {
    	if(fgets(buffer, 128, pipe) != NULL)
    		result += buffer;
    }
    pclose(pipe);
    return result;
}

bool parseMapsFile(char* maps, std::vector<unsigned long>& begin, std::vector<unsigned long>& end, std::vector<std::string>& library){
  std::ifstream mapsFile;
  mapsFile.open(maps);
  if(!mapsFile){
    std::cerr << "Could not open file " << maps << std::endl;
    return false;
    }

  std::string line;
  int i = 0;

  while (std::getline(mapsFile, line, '\n')){
    std::string s;
    std::stringstream ss(line);
    i = 0;
    bool found = false;
    while (getline(ss, s, ' ')){
      std::string s2;
      std::stringstream ss2(s);
      while ( getline(ss2, s2, '-') && i < 2) {
        if (i == 0) begin.push_back(strtoul(s2.c_str(), NULL, 16));
        if (i == 1) end.push_back(strtoul(s2.c_str(), NULL, 16));
        i = i + 1;
      }
    }
    library.push_back(s);
  }
  mapsFile.close();
  return true;
}
 
int findLibrary(unsigned long ip, std::vector<unsigned long>& begin, std::vector<unsigned long>& end, std::vector<std::string>& library){
 for (int i = 0; i <= begin.size(); i++)
    if(ip > begin[i] && ip < end[i]) 
       return i;
 return -1;
}  

   
bool translate(char* symbolLookupTable, char* maps, char* outputFilename){
  std::ifstream symbolLookupTableFile;
  symbolLookupTableFile.open(symbolLookupTable);
  if(!symbolLookupTableFile){
    std::cerr << "Could not open file " << symbolLookupTable << std::endl;
    return false;
    }
      
  std::vector<unsigned long> begin;
  std::vector<unsigned long> end;
  std::vector<std::string> library;
  if(!parseMapsFile(maps, begin, end, library))
    return false;

  unsigned long ip;
  std::string line, output;
  std::ofstream outputFile;
  outputFile.open(outputFilename);

  //ignore first line
  std::getline(symbolLookupTableFile, line, '\n');
  while (std::getline(symbolLookupTableFile, line, '\n')){
    std::vector<std::string> elements;
    std::string s;
    std::stringstream ss(line);
    int i = 0;
    while (getline(ss, s, ' ')) {
      std::string s2;
      std::stringstream ss2(s);
        while (getline(ss2, s2, '\t') && i < 2) {
           elements.push_back(s2);
           i = i + 1;
        }
      elements.push_back(s);}
    auto it = std::find(elements.begin(), elements.end(), "ip=");  
    if(it != elements.end()){
        auto it2 = std::next(it, 1);
        ip = strtoul((*it2).c_str(), NULL, 0);
        int index = findLibrary(ip, begin, end, library);
        if (index != -1){
            std::string lib = library[index];
            //output = execute("gdb --batch " + lib + " -ex list*'('" + elements[1] + elements[3] + elements[4] + "')'" );
            std::stringstream sstream;
            sstream << "0x" << std::hex << (ip-begin[index]);
            std::string tmp = sstream.str();  
            output = execute("eu-addr2line -C -i -e " + lib + " " + tmp);
	    std::replace(output.begin(),output.end(),'\n',',');
            outputFile << elements[0] << "\t" <<lib<<"\t "<< elements[1] << elements[3] << elements[4] << " in [ " << output.substr(0,output.find("\n")) << " ] \n";
         }
     }
  }
  outputFile.close();
  return true;
}

   
