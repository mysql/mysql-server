/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef InitConfigFileParser_H
#define InitConfigFileParser_H

#include <ndb_global.h>
#include "portlib/ndb_compiler.h"

#include <Properties.hpp>
#include <ConfigValues.hpp>

class Config;
class ConfigInfo;

/**
 * @class InitConfigFileParser
 * @brief Reads initial config file and returns Config object
 * 
 * This class contains one public method InitConfigFileParser::parseConfig, 
 * which reads an initial configuration file and returns a Config 
 * object if the config file has correct syntax and semantic. 
 */
class InitConfigFileParser {
public:
  /**
   *   Constructor
   */
  InitConfigFileParser();
  ~InitConfigFileParser();

  /**
   *   Reads the initial configuration file, checks syntax and semantic
   *   and stores internally the values of all parameters.
   *
   *   @returns Config or NULL on failure
   *   @note must be freed by caller
   */
  Config * parseConfig(FILE * file);
  Config * parseConfig(const char * filename);
  Config * parse_mycnf(const char * cluster_config_suffix);

  /**
   * Parser context struct
   */
  enum ContextSectionType { Undefined, Section, DefaultSection };

  /**
   *   Context = Which section in init config file we are currently parsing
   */
  struct Context {
    Context();
    ~Context();

    ContextSectionType  type; ///< Section type (e.g. default section,section)
    char          fname[256]; ///< Section name occurring in init config file
    char          pname[256]; ///< Section name stored in properties object
    Uint32          m_lineno; ///< Current line no in config file
    Uint32   m_sectionLineno; ///< Where did current section start

    const ConfigInfo * m_info;           // The config info
    Properties * m_config;               // The config object
    Properties * m_defaults;             // The user defaults
    
    Properties       * m_currentSection; // The current section I'm in
    const Properties * m_userDefaults;   // The defaults of this section
    const Properties * m_systemDefaults; // The syst. defaults for this section
    const Properties * m_currentInfo;    // The "info" for this section
    
    Properties         m_userProperties; // User properties (temporary values)
    ConfigValuesFactory m_configValues;  //

  public:
    void reportError(const char * msg, ...)
      ATTRIBUTE_FORMAT(printf, 2, 3);
    void reportWarning(const char * msg, ...)
      ATTRIBUTE_FORMAT(printf, 2, 3);
  };

  static bool convertStringToUint64(const char* s, Uint64& val);
  static bool convertStringToBool(const char* s, bool& val);

private:
  /**
   *   Check if line only contains space/comments
   *   @param   line The line to check
   *   @return  true if spaces/comments only, false otherwise
   */
  bool isEmptyLine(const char* line) const;

  /**
   *   Checks if line contains a section header
   *   @param   line  String to search
   *   @return  section header if matching some section header, NULL otherwise
   */
  char* parseSectionHeader(const char* line) const;

  /**
   *   Checks if line contains a default header
   *   @param   line  String to search
   *   @return  section header if matching some section header, NULL otherwise
   */
  char* parseDefaultSectionHeader(const char* line) const;

  bool parseNameValuePair(Context&, const char* line);
  bool storeNameValuePair(Context&, const char* fname, const char* value);

  bool storeSection(Context&);

  const Properties* getSection(const char * name, const Properties* src);
 
  /**
   *   Information about parameters (min, max values etc)
   */
  ConfigInfo* m_info;

  bool handle_mycnf_defaults(Vector<struct my_option>& options,
			     InitConfigFileParser::Context& ctx, 
			     const char * name);
  
  bool load_mycnf_groups(Vector<struct my_option> & options,
			 InitConfigFileParser::Context& ctx,
			 const char * name,
			 const char *groups[]);

  bool store_in_properties(Vector<struct my_option>& options, 
			   InitConfigFileParser::Context& ctx,
			   const char * name);
  
  Config* run_config_rules(Context& ctx);
};

#endif // InitConfigFileParser_H
