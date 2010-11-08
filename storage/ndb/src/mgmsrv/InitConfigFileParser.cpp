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

  return run_config_rules(ctx);
}

Config*
InitConfigFileParser::run_config_rules(Context& ctx)
{
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
  BaseString::snprintf(tmpLine, MAX_LINE_LENGTH,
                       "EXTERNAL SYSTEM_%s:NoOfConnections", system);
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
    if(desc && desc[0]){
      ctx.reportWarning("[%s] %s is depricated, use %s instead", 
			ctx.fname, fname, desc);
    } else if (desc == 0){
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
  if(ctx.type == InitConfigFileParser::DefaultSection &&
     !ctx.m_defaults->put(ctx.pname, ctx.m_currentSection))
  {
    ctx.reportError("Duplicate default section not allowed");
    return false;
  }
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

#include <my_sys.h>
#include <my_getopt.h>

static int order = 1;
static 
my_bool 
parse_mycnf_opt(int, const struct my_option * opt, char * value)
{
  long *app_type= (long*) &opt->app_type;
  if(opt->comment)
    (*app_type)++;
  else
    *app_type = order++;
  return 0;
}

bool
InitConfigFileParser::store_in_properties(Vector<struct my_option>& options, 
					  InitConfigFileParser::Context& ctx,
					  const char * name)
{
  for(unsigned i = 0; i<options.size(); i++)
  {
    if(options[i].comment && 
       options[i].app_type && 
       strcmp(options[i].comment, name) == 0)
    {
      Uint64 value_int;
      switch(options[i].var_type){
      case GET_INT:
	value_int = *(Uint32*)options[i].value;
	break;
      case GET_LL:
	value_int = *(Uint64*)options[i].value;
	break;
      case GET_STR:
	ctx.m_currentSection->put(options[i].name, *(char**)options[i].value);
	continue;
      default:
	abort();
      }

      const char * fname = options[i].name;
      if (!m_info->verify(ctx.m_currentInfo, fname, value_int)) {
	ctx.reportError("Illegal value %lld for parameter %s.\n"
			"Legal values are between %Lu and %Lu", 
			value_int, fname,
			m_info->getMin(ctx.m_currentInfo, fname), 
			m_info->getMax(ctx.m_currentInfo, fname));
	return false;
      }

      ConfigInfo::Status status = m_info->getStatus(ctx.m_currentInfo, fname);
      if (status == ConfigInfo::CI_DEPRICATED) {
	const char * desc = m_info->getDescription(ctx.m_currentInfo, fname);
	if(desc && desc[0]){
	  ctx.reportWarning("[%s] %s is depricated, use %s instead", 
			    ctx.fname, fname, desc);
	} else if (desc == 0){
	  ctx.reportWarning("[%s] %s is depricated", ctx.fname, fname);
	} 
      }
      
      if (options[i].var_type == GET_INT)
	ctx.m_currentSection->put(options[i].name, (Uint32)value_int);
      else
	ctx.m_currentSection->put64(options[i].name, value_int);	
    }
  }
  return true;
}

bool
InitConfigFileParser::handle_mycnf_defaults(Vector<struct my_option>& options,
					    InitConfigFileParser::Context& ctx, 
					    const char * name)
{
  strcpy(ctx.fname, name);
  ctx.type = InitConfigFileParser::DefaultSection;
  ctx.m_currentSection = new Properties(true);
  ctx.m_userDefaults   = NULL;
  require((ctx.m_currentInfo = m_info->getInfo(ctx.fname)) != 0);
  require((ctx.m_systemDefaults = m_info->getDefaults(ctx.fname)) != 0);
  if(store_in_properties(options, ctx, name))
    return storeSection(ctx);
  return false;
}

static
int
load_defaults(Vector<struct my_option>& options, const char* groups[])
{
  int argc = 1;
  const char * argv[] = { "ndb_mgmd", 0, 0, 0, 0 };
  BaseString file;
  BaseString extra_file;
  BaseString group_suffix;

  const char *save_file = my_defaults_file;
  const char *save_extra_file = my_defaults_extra_file;
  const char *save_group_suffix = my_defaults_group_suffix;

  if (my_defaults_file)
  {
    file.assfmt("--defaults-file=%s", my_defaults_file);
    argv[argc++] = file.c_str();
  }

  if (my_defaults_extra_file)
  {
    extra_file.assfmt("--defaults-extra-file=%s", my_defaults_extra_file);
    argv[argc++] = extra_file.c_str();
  }

  if (my_defaults_group_suffix)
  {
    group_suffix.assfmt("--defaults-group-suffix=%s",
                        my_defaults_group_suffix);
    argv[argc++] = group_suffix.c_str();
  }

  char ** tmp = (char**)argv;
  int ret = load_defaults("my", groups, &argc, &tmp);
  
  my_defaults_file = save_file;
  my_defaults_extra_file = save_extra_file;
  my_defaults_group_suffix = save_group_suffix;
  
  if (ret == 0)
  {
    return handle_options(&argc, &tmp, options.getBase(), parse_mycnf_opt);
  }
  
  return ret;
}

bool
InitConfigFileParser::load_mycnf_groups(Vector<struct my_option> & options,
					InitConfigFileParser::Context& ctx,
					const char * name,
					const char *groups[])
{
  unsigned i;
  Vector<struct my_option> copy;
  for(i = 0; i<options.size(); i++)
  {
    if(options[i].comment && strcmp(options[i].comment, name) == 0)
    {
      options[i].app_type = 0;
      copy.push_back(options[i]);
    }
  }

  struct my_option end;
  bzero(&end, sizeof(end));
  copy.push_back(end);

  if (load_defaults(copy, groups))
    return false;
  
  return store_in_properties(copy, ctx, name);
}

Config *
InitConfigFileParser::parse_mycnf() 
{
  int i;
  Config * res = 0;
  Vector<struct my_option> options;
  for(i = 0; i<ConfigInfo::m_NoOfParams; i++)
  {
    {
      struct my_option opt;
      bzero(&opt, sizeof(opt));
      const ConfigInfo::ParamInfo& param = ConfigInfo::m_ParamInfo[i];
      switch(param._type){
      case ConfigInfo::CI_BOOL:
	opt.value = (uchar **)malloc(sizeof(int));
	opt.var_type = GET_INT;
	break;
      case ConfigInfo::CI_INT: 
	opt.value = (uchar**)malloc(sizeof(int));
	opt.var_type = GET_INT;
	break;
      case ConfigInfo::CI_INT64:
	opt.value = (uchar**)malloc(sizeof(Int64));
	opt.var_type = GET_LL;
	break;
      case ConfigInfo::CI_STRING: 
	opt.value = (uchar**)malloc(sizeof(char *));
	opt.var_type = GET_STR;
	break;
      default:
	continue;
      }
      opt.name = param._fname;
      opt.id = 256;
      opt.app_type = 0;
      opt.arg_type = REQUIRED_ARG;
      opt.comment = param._section;
      options.push_back(opt);
    }
  }
  
  struct my_option *ndbd, *ndb_mgmd, *mysqld, *api;

  /**
   * Add ndbd, ndb_mgmd, api/mysqld
   */
  Uint32 idx = options.size();
  {
    struct my_option opt;
    bzero(&opt, sizeof(opt));
    opt.name = "ndbd";
    opt.id = 256;
    opt.value = (uchar**)malloc(sizeof(char*));
    opt.var_type = GET_STR;
    opt.arg_type = REQUIRED_ARG;
    options.push_back(opt);

    opt.name = "ndb_mgmd";
    opt.id = 256;
    opt.value = (uchar**)malloc(sizeof(char*));
    opt.var_type = GET_STR;
    opt.arg_type = REQUIRED_ARG;
    options.push_back(opt);

    opt.name = "mysqld";
    opt.id = 256;
    opt.value = (uchar**)malloc(sizeof(char*));
    opt.var_type = GET_STR;
    opt.arg_type = REQUIRED_ARG;
    options.push_back(opt);

    opt.name = "ndbapi";
    opt.id = 256;
    opt.value = (uchar**)malloc(sizeof(char*));
    opt.var_type = GET_STR;
    opt.arg_type = REQUIRED_ARG;
    options.push_back(opt);

    bzero(&opt, sizeof(opt));
    options.push_back(opt);

    ndbd = &options[idx];
    ndb_mgmd = &options[idx+1];
    mysqld = &options[idx+2];
    api = &options[idx+3];
  }
  
  Context ctx(m_info, m_errstream); 
  const char *groups[]= { "cluster_config", 0 };
  if (load_defaults(options, groups))
    goto end;

  ctx.m_lineno = 0;
  if(!handle_mycnf_defaults(options, ctx, "DB"))
    goto end;
  if(!handle_mycnf_defaults(options, ctx, "API"))
    goto end;
  if(!handle_mycnf_defaults(options, ctx, "MGM"))
    goto end;
  if(!handle_mycnf_defaults(options, ctx, "TCP"))
    goto end;
  if(!handle_mycnf_defaults(options, ctx, "SHM"))
    goto end;
  if(!handle_mycnf_defaults(options, ctx, "SCI"))
    goto end;

  {
    struct sect { struct my_option* src; const char * name; } sections[] = 
      {
	{ ndb_mgmd, "MGM" }
	,{ ndbd, "DB" }
	,{ mysqld, "API" }
	,{ api, "API" }
	,{ 0, 0 }, { 0, 0 }
      };
    
    for(i = 0; sections[i].src; i++)
    {
      for(int j = i + 1; sections[j].src; j++)
      {
	if (sections[j].src->app_type < sections[i].src->app_type)
	{
	  sect swap = sections[i];
	  sections[i] = sections[j];
	  sections[j] = swap;
	}
      }
    }
    
    ctx.type = InitConfigFileParser::Section;
    ctx.m_sectionLineno  = ctx.m_lineno;      
    for(i = 0; sections[i].src; i++)
    {
      if (sections[i].src->app_type)
      {
	strcpy(ctx.fname, sections[i].name);
	BaseString str(*(char**)sections[i].src->value);
	Vector<BaseString> list;
	str.split(list, ",");
	
	const char * defaults_groups[] = { 0,  0, 0 };
	for(unsigned j = 0; j<list.size(); j++)
	{
	  BaseString group_idx;
	  BaseString group_host;
	  group_idx.assfmt("%s.%s.%d", groups[0], 
			   sections[i].src->name, j + 1);
	  group_host.assfmt("%s.%s.%s", groups[0], 
			    sections[i].src->name, list[j].c_str());
	  defaults_groups[0] = group_idx.c_str();
	  if(list[j].length())
	    defaults_groups[1] = group_host.c_str();
	  else
	    defaults_groups[1] = 0;
	  
	  ctx.m_currentSection = new Properties(true);
	  ctx.m_userDefaults = getSection(ctx.fname, ctx.m_defaults);
	  require((ctx.m_currentInfo = m_info->getInfo(ctx.fname)) != 0);
	  require((ctx.m_systemDefaults = m_info->getDefaults(ctx.fname))!= 0);
	  ctx.m_currentSection->put("HostName", list[j].c_str());
	  if(!load_mycnf_groups(options, ctx, sections[i].name, 
				defaults_groups))
	    goto end;
	  
	  if(!storeSection(ctx))
	    goto end;
	}
      }
    }
  }

  res = run_config_rules(ctx);

end:
  for(i = 0; options[i].name; i++)
    free(options[i].value);

  return res;
}

template class Vector<struct my_option>;

/*
  See include/my_getopt.h for the declaration of struct my_option
*/
