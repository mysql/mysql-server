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

#include <ndb_global.h>

#include "InitConfigFileParser.hpp"
#include "Config.hpp"
#include "MgmtErrorReporter.hpp"
#include <NdbOut.hpp>
#include "ConfigInfo.hpp"
#include <m_string.h>

const int MAX_LINE_LENGTH = 1024;  // Max length of line of text in config file
static void trim(char *);

static void require(bool v) { if(!v) abort();}

//****************************************************************************
//  Ctor / Dtor
//****************************************************************************
InitConfigFileParser::InitConfigFileParser(FILE * out)
{
  m_info = new ConfigInfo();
  m_errstream = out ? out : stdout;
}

InitConfigFileParser::~InitConfigFileParser() {
  delete m_info;
}

//****************************************************************************
//  Read Config File
//****************************************************************************
InitConfigFileParser::Context::Context(const ConfigInfo * info, FILE * out)
  :  m_userProperties(true), m_configValues(1000, 20) {

  m_config = new Properties(true);
  m_defaults = new Properties(true);
  m_errstream = out;
}

InitConfigFileParser::Context::~Context(){
  if(m_config != 0)
    delete m_config;

  if(m_defaults != 0)
    delete m_defaults;
}

Config *
InitConfigFileParser::parseConfig(const char * filename) {
  FILE * file = fopen(filename, "r");
  if(file == 0){
    fprintf(m_errstream, "Error opening file: %s\n", filename);
    return 0;
  }
  
  Config * ret = parseConfig(file);
  fclose(file);
  return ret;
}

Config *
InitConfigFileParser::parseConfig(FILE * file) {

  char line[MAX_LINE_LENGTH];

  Context ctx(m_info, m_errstream); 
  ctx.m_lineno = 0;
  ctx.m_currentSection = 0;

  /*************
   * Open file *
   *************/
  if (file == NULL) {
    return 0;
  }

  /***********************
   * While lines to read *
   ***********************/
  while (fgets(line, MAX_LINE_LENGTH, file)) {
    ctx.m_lineno++;

    trim(line);

    if (isEmptyLine(line)) // Skip if line is empty or comment
      continue;   

    // End with NULL instead of newline
    if (line[strlen(line)-1] == '\n')
      line[strlen(line)-1] = '\0';
    
    /********************************
     * 1. Parse new default section *
     ********************************/
    if (char* section = parseDefaultSectionHeader(line)) {
      if(!storeSection(ctx)){
	free(section);
	ctx.reportError("Could not store previous default section "
			"of configuration file.");
	return 0;
      }
      BaseString::snprintf(ctx.fname, sizeof(ctx.fname), section); free(section);
      ctx.type             = InitConfigFileParser::DefaultSection;
      ctx.m_sectionLineno  = ctx.m_lineno;
      ctx.m_currentSection = new Properties(true);
      ctx.m_userDefaults   = NULL;
      require((ctx.m_currentInfo = m_info->getInfo(ctx.fname)) != 0);
      require((ctx.m_systemDefaults = m_info->getDefaults(ctx.fname)) != 0);
      continue;
    }
    
    /************************
     * 2. Parse new section *
     ************************/
    if (char* section = parseSectionHeader(line)) {
      if(!storeSection(ctx)){
	free(section);
	ctx.reportError("Could not store previous section "
			"of configuration file.");
	return 0;
      }
      BaseString::snprintf(ctx.fname, sizeof(ctx.fname), section);
      free(section);
      ctx.type             = InitConfigFileParser::Section;
      ctx.m_sectionLineno  = ctx.m_lineno;      
      ctx.m_currentSection = new Properties(true);
      ctx.m_userDefaults   = getSection(ctx.fname, ctx.m_defaults);
      require((ctx.m_currentInfo    = m_info->getInfo(ctx.fname)) != 0);
      require((ctx.m_systemDefaults = m_info->getDefaults(ctx.fname)) != 0);
      continue;
    }
    
    /****************************
     * 3. Parse name-value pair *
     ****************************/
    if (!parseNameValuePair(ctx, line)) {
      ctx.reportError("Could not parse name-value pair in config file.");
      return 0;
    }
  }
  
  if (ferror(file)){
    ctx.reportError("Failure in reading");
    return 0;
  } 

  if(!storeSection(ctx)) {
    ctx.reportError("Could not store section of configuration file.");
    return 0;
  }
  for(size_t i = 0; ConfigInfo::m_ConfigRules[i].m_configRule != 0; i++){
    ctx.type             = InitConfigFileParser::Undefined;
    ctx.m_currentSection = 0;
    ctx.m_userDefaults   = 0;
    ctx.m_currentInfo    = 0;
    ctx.m_systemDefaults = 0;
    
    Vector<ConfigInfo::ConfigRuleSection> tmp;
    if(!(* ConfigInfo::m_ConfigRules[i].m_configRule)(tmp, ctx,
						      ConfigInfo::m_ConfigRules[i].m_ruleData))
      return 0;

    for(size_t j = 0; j<tmp.size(); j++){
      BaseString::snprintf(ctx.fname, sizeof(ctx.fname), tmp[j].m_sectionType.c_str());
      ctx.type             = InitConfigFileParser::Section;
      ctx.m_currentSection = tmp[j].m_sectionData;
      ctx.m_userDefaults   = getSection(ctx.fname, ctx.m_defaults);
      require((ctx.m_currentInfo    = m_info->getInfo(ctx.fname)) != 0);
      require((ctx.m_systemDefaults = m_info->getDefaults(ctx.fname)) != 0);
      if(!storeSection(ctx))
	return 0;
    }
  }

  Uint32 nConnections = 0;
  Uint32 nComputers = 0;
  Uint32 nNodes = 0;
  Uint32 nExtConnections = 0;
  const char * system = "?";
  ctx.m_userProperties.get("NoOfConnections", &nConnections);
  ctx.m_userProperties.get("NoOfComputers", &nComputers);
  ctx.m_userProperties.get("NoOfNodes", &nNodes);
  ctx.m_userProperties.get("ExtNoOfConnections", &nExtConnections);
  ctx.m_userProperties.get("ExtSystem", &system);
  ctx.m_config->put("NoOfConnections", nConnections);
  ctx.m_config->put("NoOfComputers", nComputers);
  ctx.m_config->put("NoOfNodes", nNodes);

  char tmpLine[MAX_LINE_LENGTH];
  BaseString::snprintf(tmpLine, MAX_LINE_LENGTH, "EXTERNAL SYSTEM_");
  strncat(tmpLine, system, MAX_LINE_LENGTH);
  strncat(tmpLine, ":NoOfConnections", MAX_LINE_LENGTH);
  ctx.m_config->put(tmpLine, nExtConnections);
  
  Config * ret = new Config();
  ret->m_configValues = (struct ndb_mgm_configuration*)ctx.m_configValues.getConfigValues();
  ret->m_oldConfig = ctx.m_config; ctx.m_config = 0;
  return ret;
}

//****************************************************************************
//  Parse Name-Value Pair
//****************************************************************************

bool InitConfigFileParser::parseNameValuePair(Context& ctx, const char* line)
{
  if (ctx.m_currentSection == NULL){
    ctx.reportError("Value specified outside section");
    return false;
  }

  // *************************************
  //  Split string at first occurrence of 
  //  '=' or ':'
  // *************************************

  Vector<BaseString> tmp_string_split;
  if (BaseString(line).split(tmp_string_split,
			     "=:", 2) != 2)
  {
    ctx.reportError("Parse error");
    return false;
  }

  // *************************************
  //  Remove all after #
  // *************************************

  Vector<BaseString> tmp_string_split2;
  tmp_string_split[1].split(tmp_string_split2,
			    "#", 2);
  tmp_string_split[1]=tmp_string_split2[0];

  // *************************************
  // Remove leading and trailing chars
  // *************************************
  {
    for (int i = 0; i < 2; i++)
      tmp_string_split[i].trim("\r\n \t"); 
  }

  // *************************************
  // First in split is fname
  // *************************************

  const char *fname= tmp_string_split[0].c_str();

  if (!ctx.m_currentInfo->contains(fname)) {
    ctx.reportError("[%s] Unknown parameter: %s", ctx.fname, fname);
    return false;
  }
  ConfigInfo::Status status = m_info->getStatus(ctx.m_currentInfo, fname);
  if (status == ConfigInfo::CI_NOTIMPLEMENTED) {
    ctx.reportWarning("[%s] %s not yet implemented", ctx.fname, fname);
  }
  if (status == ConfigInfo::CI_DEPRICATED) {
    const char * desc = m_info->getDescription(ctx.m_currentInfo, fname);
    if(desc){
      ctx.reportWarning("[%s] %s is depricated, use %s instead", 
			ctx.fname, fname, desc);
    } else {
      ctx.reportWarning("[%s] %s is depricated", ctx.fname, fname);
    } 
  }

  // ***********************
  //  Store name-value pair
  // ***********************

  return storeNameValuePair(ctx, fname, tmp_string_split[1].c_str());
}


//****************************************************************************
//  STORE NAME-VALUE pair in properties section 
//****************************************************************************

bool 
InitConfigFileParser::storeNameValuePair(Context& ctx,
					 const char* fname, 
					 const char* value) {
  
  const char * pname = fname;

  if (ctx.m_currentSection->contains(pname)) {
    ctx.reportError("[%s] Parameter %s specified twice", ctx.fname, fname);
    return false;
  }
  
  // ***********************
  //  Store name-value pair
  // ***********************

  const ConfigInfo::Type type = m_info->getType(ctx.m_currentInfo, fname);
  switch(type){
  case ConfigInfo::CI_BOOL: {
    bool value_bool;
    if (!convertStringToBool(value, value_bool)) {
      ctx.reportError("Illegal boolean value for parameter %s", fname);
      return false;
    }
    MGM_REQUIRE(ctx.m_currentSection->put(pname, value_bool));
    break;
  }
  case ConfigInfo::CI_INT:
  case ConfigInfo::CI_INT64:{
    Uint64 value_int;
    if (!convertStringToUint64(value, value_int)) {
      ctx.reportError("Illegal integer value for parameter %s", fname);
      return false;
    }
    if (!m_info->verify(ctx.m_currentInfo, fname, value_int)) {
      ctx.reportError("Illegal value %s for parameter %s.\n"
		      "Legal values are between %Lu and %Lu", value, fname,
		      m_info->getMin(ctx.m_currentInfo, fname), 
		      m_info->getMax(ctx.m_currentInfo, fname));
      return false;
    }
    if(type == ConfigInfo::CI_INT){
      MGM_REQUIRE(ctx.m_currentSection->put(pname, (Uint32)value_int));
    } else {
      MGM_REQUIRE(ctx.m_currentSection->put64(pname, value_int));
    }
    break;
  }
  case ConfigInfo::CI_STRING:
    MGM_REQUIRE(ctx.m_currentSection->put(pname, value));
    break;
  case ConfigInfo::CI_SECTION:
    abort();
  }
  return true;
}

//****************************************************************************
//  Is Empty Line
//****************************************************************************

bool InitConfigFileParser::isEmptyLine(const char* line) const {
  int i;
  
  // Check if it is a comment line
  if (line[0] == '#') return true;               

  // Check if it is a line with only spaces
  for (i = 0; i < MAX_LINE_LENGTH && line[i] != '\n' && line[i] != '\0'; i++) {
    if (line[i] != ' ' && line[i] != '\t') return false;
  }
  return true;
}

//****************************************************************************
//  Convert String to Int
//****************************************************************************
bool InitConfigFileParser::convertStringToUint64(const char* s, 
						 Uint64& val,
						 Uint32 log10base) {
  if (s == NULL)
    return false;
  if (strlen(s) == 0) 
    return false;

  errno = 0;
  char* p;
  Int64 v = strtoll(s, &p, log10base);
  if (errno != 0)
    return false;
  
  long mul = 0;
  if (p != &s[strlen(s)]){
    char * tmp = strdup(p);
    trim(tmp);
    switch(tmp[0]){
    case 'k':
    case 'K':
      mul = 10;
      break;
    case 'M':
      mul = 20;
      break;
    case 'G':
      mul = 30;
      break;
    default:
      free(tmp);
      return false;
    }
    free(tmp);
  }
  
  val = (v << mul);
  return true;
}

bool InitConfigFileParser::convertStringToBool(const char* s, bool& val) {
  if (s == NULL) return false;
  if (strlen(s) == 0) return false;

  if (!strcmp(s, "Y") || !strcmp(s, "y") || 
      !strcmp(s, "Yes") || !strcmp(s, "YES") || !strcmp(s, "yes") || 
      !strcmp(s, "True") || !strcmp(s, "TRUE") || !strcmp(s, "true") ||
      !strcmp(s, "1")) {
    val = true;
    return true;
  }

  if (!strcmp(s, "N") || !strcmp(s, "n") || 
      !strcmp(s, "No") || !strcmp(s, "NO") || !strcmp(s, "no") || 
      !strcmp(s, "False") || !strcmp(s, "FALSE") || !strcmp(s, "false") ||
      !strcmp(s, "0")) {
    val = false;
    return true;
  }
  
  return false;  // Failure to convert
}

//****************************************************************************
//  Parse Section Header
//****************************************************************************
static void
trim(char * str){
  int len = strlen(str);
  for(len--;
      (str[len] == '\r' || str[len] == '\n' || 
       str[len] == ' ' || str[len] == '\t') && 
	len > 0; 
      len--)
    str[len] = 0;
  
  int pos = 0;
  while(str[pos] == ' ' || str[pos] == '\t')
    pos++;
  
  if(str[pos] == '\"' && str[len] == '\"') {
    pos++;
    str[len] = 0;
    len--;
  }
  
  memmove(str, &str[pos], len - pos + 2);
}

char* 
InitConfigFileParser::parseSectionHeader(const char* line) const {
  char * tmp = strdup(line);

  if(tmp[0] != '['){
    free(tmp);
    return NULL;
  }

  if(tmp[strlen(tmp)-1] != ']'){
    free(tmp);
    return NULL;
  }
  tmp[strlen(tmp)-1] = 0;

  tmp[0] = ' ';
  trim(tmp);

  // Get the correct header name if an alias
  {
    const char *tmp_alias= m_info->getAlias(tmp);
    if (tmp_alias) {
      free(tmp);
      tmp= strdup(tmp_alias);
    }
  }

  // Lookup token among sections
  if(!m_info->isSection(tmp)) {
    free(tmp);
    return NULL;
  }
  if(m_info->getInfo(tmp)) return tmp;

  free(tmp);
  return NULL;
}

//****************************************************************************
//  Parse Default Section Header
//****************************************************************************

char* 
InitConfigFileParser::parseDefaultSectionHeader(const char* line) const {
  static char token1[MAX_LINE_LENGTH], token2[MAX_LINE_LENGTH];

  int no = sscanf(line, "[%120[A-Z_a-z] %120[A-Z_a-z]]", token1, token2);

  // Not correct no of tokens 
  if (no != 2) return NULL;

  // Not correct keyword at end
  if (!strcasecmp(token2, "DEFAULT") == 0) return NULL;

  const char *token1_alias= m_info->getAlias(token1);
  if (token1_alias == 0)
    token1_alias= token1;

  if(m_info->getInfo(token1_alias)){
    return strdup(token1_alias);
  }
  
  // Did not find section
  return NULL;
}

const Properties *
InitConfigFileParser::getSection(const char * name, const Properties * src){
  const Properties * p;
  if(src && src->get(name, &p))
    return p;

  return 0;
}

//****************************************************************************
//  STORE section
//****************************************************************************
bool
InitConfigFileParser::storeSection(Context& ctx){
  if(ctx.m_currentSection == NULL)
    return true;
  for(int i = strlen(ctx.fname) - 1; i>=0; i--){
    ctx.fname[i] = toupper(ctx.fname[i]);
  }
  BaseString::snprintf(ctx.pname, sizeof(ctx.pname), ctx.fname);
  char buf[255];
  if(ctx.type == InitConfigFileParser::Section)
    BaseString::snprintf(buf, sizeof(buf), "%s", ctx.fname);
  if(ctx.type == InitConfigFileParser::DefaultSection)
    BaseString::snprintf(buf, sizeof(buf), "%s DEFAULT", ctx.fname);
  BaseString::snprintf(ctx.fname, sizeof(ctx.fname), buf);
  if(ctx.type == InitConfigFileParser::Section){
    for(int i = 0; i<m_info->m_NoOfRules; i++){
      const ConfigInfo::SectionRule & rule = m_info->m_SectionRules[i];
      if(!strcmp(rule.m_section, "*") || !strcmp(rule.m_section, ctx.fname)){
	if(!(* rule.m_sectionRule)(ctx, rule.m_ruleData)){
	  return false;
	}
      }
    }
  }
  if(ctx.type == InitConfigFileParser::DefaultSection)
    require(ctx.m_defaults->put(ctx.pname, ctx.m_currentSection));
  if(ctx.type == InitConfigFileParser::Section)
    require(ctx.m_config->put(ctx.pname, ctx.m_currentSection));
  delete ctx.m_currentSection; ctx.m_currentSection = NULL;
  return true;
}

void
InitConfigFileParser::Context::reportError(const char * fmt, ...){
  va_list ap;
  char buf[1000];
  
  va_start(ap, fmt);
  if (fmt != 0)
    BaseString::vsnprintf(buf, sizeof(buf)-1, fmt, ap);
  va_end(ap);
  fprintf(m_errstream, "Error line %d: %s\n",
	  m_lineno, buf);

  //m_currentSection->print();
}

void
InitConfigFileParser::Context::reportWarning(const char * fmt, ...){
  va_list ap;
  char buf[1000];
  
  va_start(ap, fmt);
  if (fmt != 0)
    BaseString::vsnprintf(buf, sizeof(buf)-1, fmt, ap);
  va_end(ap);
  fprintf(m_errstream, "Warning line %d: %s\n",
	  m_lineno, buf);
}
