/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef InitConfigFileParser_H
#define InitConfigFileParser_H

#include <stdio.h>
#include <Properties.hpp>

class Config;
class ConfigInfo;

/**
 * @class InitConfigFileParser
 * @brief Reads initial config file and returns Config object
 * 
 * This class contains one public method InitConfigFileParser::getConfig, 
 * which reads an initial configuration file and returns a Config 
 * object if the config file has correct syntax and semantic. 
 */
class InitConfigFileParser {
public:
  enum ContextSectionType { Undefined, Section, DefaultSection };

  /**
   *   Context = Which section in init config file we are currently parsing
   */
  struct Context {
    ContextSectionType  type; ///< Section type (e.g. default section,section)
    char          fname[256]; ///< Section name occuring in init config file
    char          pname[256]; ///< Section name stored in properties object
    Uint32          m_lineno; ///< Current line no in config file
    Uint32   m_sectionLineno; ///< Where did current section start

    const ConfigInfo * m_info;           // The config info
    const Properties * m_config;         // The config object
    const Properties * m_defaults;       // The user defaults
    
    Properties       * m_currentSection; // The current section I'm in
    const Properties * m_userDefaults;   // The defaults of this section
    const Properties * m_systemDefaults; // The syst. defaults for this section
    const Properties * m_currentInfo;    // The "info" for this section
    
    Properties         m_userProperties; // User properties (temporary values)

  public:
    void reportError(const char * msg, ...);
    void reportWarning(const char * msg, ...);
  };


  /**
   *   Constructor
   *   @param initialConfigFileName: Name of the initial configuration file
   */
  InitConfigFileParser(const char* initialConfigFileName);
  ~InitConfigFileParser();

  /**
   *   Reads the initial configuration file, checks syntax and semantic
   *   and stores internally the values of all parameters.
   *
   *   @returns  true if succeeded, o/w false (e.g. incorrect config file)
   */
  bool readConfigFile();

  /**
   *   Get config.  Must execute InitConfigFileParser::readConfigFile first.
   *
   *   @returns  Config if succeeded, o/w NULL 
   */
  const Config* getConfig();


private:
  /**
   *   Check if line only contains space/comments
   *   @param   line: The line to check
   *   @return  true if spaces/comments only, false otherwise
   */
  bool isEmptyLine(const char* line) const;

  /**
   *   Checks if line contains a section header
   *   @param   line:  String to search
   *   @return  section header if matching some section header, NULL otherwise
   */
  char* parseSectionHeader(const char* line) const;

  /**
   *   Checks if line contains a default header
   *   @param   line:  String to search
   *   @return  section header if matching some section header, NULL otherwise
   */
  char* parseDefaultSectionHeader(const char* line) const;
  
  bool parseNameValuePair(Context&, const char* line);
  bool storeNameValuePair(Context&, const char* fname, const char* value);
  
  bool convertStringToUint32(const char* s, Uint32& val, Uint32 log10base = 0);
  bool convertStringToBool(const char* s, bool& val);

  const Properties* getSection(const char * name, const Properties* src); 
  
  /***************************************************************************
   *  VARIABLES
   ***************************************************************************/
  FILE*        m_initConfigStream;

  /**
   *   Information about parameters (min, max values etc)
   */
  const ConfigInfo* m_info;

  /**
   *   Configuration from initial configuration file 
   *   (returned by InitConfigFileParser::readConfigFile)
   */
  Config* m_config;

  /**
   *   Default values specified in default sections
   */
  Properties* m_defaults;

  bool storeSection(Context&);
};

#endif // InitConfigFileParser_H
