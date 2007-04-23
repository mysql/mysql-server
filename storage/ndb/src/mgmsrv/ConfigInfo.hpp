/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef ConfigInfo_H
#define ConfigInfo_H

#ifndef NDB_MGMAPI
#include <kernel_types.h>
#include <Properties.hpp>
#include <ndb_limits.h>
#include <NdbOut.hpp>
#include "InitConfigFileParser.hpp"
#endif /* NDB_MGMAPI */

/**
 * A MANDATORY parameters must be specified in the config file
 * An UNDEFINED parameter may or may not be specified in the config file
 */

// Default value for mandatory params.
#define MANDATORY ((char*)~(UintPtr)0)
// Default value for undefined params.
#define UNDEFINED ((char*) 0)

/**
 * @class  ConfigInfo
 * @brief  Metainformation about ALL cluster configuration parameters 
 *
 * Use the getters to find out metainformation about parameters.
 */
class ConfigInfo {
public:
  enum Type        { CI_BOOL, CI_INT, CI_INT64, CI_STRING, CI_SECTION };
  enum Status      { CI_USED,            ///< Active
		     CI_DEPRICATED,      ///< Can be, but shouldn't
		     CI_NOTIMPLEMENTED,  ///< Is ignored.
		     CI_INTERNAL         ///< Not configurable by the user
  };

  /**
   *   Entry for one configuration parameter
   */
  struct ParamInfo {
    Uint32         _paramId;
    const char*    _fname;   
    const char*    _section;
    const char*    _description;
    Status         _status;
    bool           _updateable;    
    Type           _type;          
    const char*    _default;
    const char*    _min;
    const char*    _max;
  };

#ifndef NDB_MGMAPI
  struct AliasPair{
    const char * name;
    const char * alias;
  };

  /**
   * Entry for one section rule
   */
  struct SectionRule {
    const char * m_section;
    bool (* m_sectionRule)(struct InitConfigFileParser::Context &, 
			   const char * m_ruleData);
    const char * m_ruleData;
  };
  
  /**
   * Entry for config rule
   */
  struct ConfigRuleSection {
    BaseString m_sectionType;
    Properties * m_sectionData;
  };

  struct ConfigRule {
    bool (* m_configRule)(Vector<ConfigRuleSection>&, 
			  struct InitConfigFileParser::Context &, 
			  const char * m_ruleData);
    const char * m_ruleData;
  };
  
  ConfigInfo();

  /**
   *   Checks if the suggested value is valid for the suggested parameter
   *   (i.e. if it is >= than min and <= than max).
   *
   *   @param  section  Init Config file section name
   *   @param  fname    Name of parameter
   *   @param  value    Value to check
   *   @return true if parameter value is valid.
   * 
   *   @note Result is not defined if section/name are wrong!
   */
  bool verify(const Properties* secti, const char* fname, Uint64 value) const;
  static const char* nameToAlias(const char*);
  static const char* getAlias(const char*);
  bool isSection(const char*) const;

  const char*  getDescription(const Properties * sec, const char* fname) const;
  Type         getType(const Properties * section, const char* fname) const;
  Status       getStatus(const Properties* section, const char* fname) const;
  Uint64       getMin(const Properties * section, const char* fname) const;
  Uint64       getMax(const Properties * section, const char* fname) const;
  Uint64       getDefault(const Properties * section, const char* fname) const;
  
  const Properties * getInfo(const char * section) const;
  const Properties * getDefaults(const char * section) const;
  
  void print() const;
  void print(const char* section) const;
  void print(const Properties * section, const char* parameter) const;

private:
  Properties               m_info;
  Properties               m_systemDefaults;

  static const AliasPair   m_sectionNameAliases[];
  static const char*       m_sectionNames[];
  static const int         m_noOfSectionNames;
#endif /* NDB_MGMAPI */

public:
  static const ParamInfo   m_ParamInfo[];
  static const int         m_NoOfParams;
  
#ifndef NDB_MGMAPI
  static const SectionRule m_SectionRules[];
  static const ConfigRule  m_ConfigRules[];
  static const int         m_NoOfRules;
#endif /* NDB_MGMAPI */
};

#endif // ConfigInfo_H
