#include <ndb_global.h>
#include "InitConfigFileParser.hpp"
#include "ConfigInfo.hpp"
#include "Config.hpp"

my_bool opt_core= 1;
my_bool opt_ndb_shm= 0;


#define CHECK(x) \
  if (!(x)) {\
    fprintf(stderr, "testConfig: '"#x"' failed on line %d\n", __LINE__); \
    exit(1); \
  }


static const ConfigInfo g_info;

/*
  Create a small config.ini with the given parameter and run
  it through InitConfigFileParser
 */
bool
check_param(const ConfigInfo::ParamInfo & param)
{

  FILE* config_file= tmpfile();
  CHECK(config_file);

  const char* section= g_info.nameToAlias(param._section);
  if (section == NULL)
    section= param._section;

  if (param._type == ConfigInfo::CI_SECTION)
  {
    fclose(config_file);
    return true;
  }

  if(param._default == MANDATORY)
  {
    // Mandatory parameter
    fclose(config_file);
    return true;
  }
  else
  {
    fprintf(config_file, "[%s]\n", section);
    fprintf(config_file, "%s=%s\n", param._fname, param._default);
  }

  // Fill in lines needed for a minimal config
  if (strcmp(section, "NDBD") != 0)
    fprintf(config_file, "[ndbd]\n");
  if (strcmp(param._fname, "NoOfReplicas") != 0)
    fprintf(config_file, "NoOfReplicas=1\n");

  if (strcmp(section, "NDB_MGMD") != 0)
    fprintf(config_file, "[ndb_mgmd]\n");
  if (strcmp(param._fname, "Hostname") != 0)
    fprintf(config_file, "HostName=localhost\n");

  if (strcmp(section, "MYSQLD") != 0)
    fprintf(config_file, "[mysqld]\n");

  rewind(config_file);

  // Run the config file through InitConfigFileParser
  // throw away the error messages for now.
  FILE* err_file= tmpfile();
  InitConfigFileParser parser(err_file);
  Config* conf = parser.parseConfig(config_file);
  fclose(config_file);
  fclose(err_file);

  if (conf == NULL)
    return false;
  delete conf;
  return true;
}


bool
check_params(void)
{
  bool ok= true;
  for (int j=0; j<g_info.m_NoOfParams; j++) {
    const ConfigInfo::ParamInfo & param= g_info.m_ParamInfo[j];
    if (!check_param(param))
    {
      ok= false;
    }
  }

  return true; // Ignore ok for now
}



Config*
create_config(const char* first, ...)
{
  va_list args;

  FILE* config_file= tmpfile();
  CHECK(config_file);

  va_start(args, first);
  const char* str= first;
  do
    fprintf(config_file, "%s\n", str);
  while((str= va_arg(args, const char*)) != NULL);
  va_end(args);

#if 0
  rewind(config_file);

  char buf[100];
  while(fgets(buf, sizeof(buf), config_file))
    ndbout_c(buf);
#endif

  rewind(config_file);

  InitConfigFileParser parser;
  Config* conf = parser.parseConfig(config_file);
  CHECK(conf);
  fclose(config_file);

  return conf;
}




void
diff_config(void)
{
  Config* c1=
    create_config("[ndbd]", "NoOfReplicas=1",
                  "[ndb_mgmd]", "HostName=localhost",
                  "[mysqld]", NULL);
  Config* c2=
    create_config("[ndbd]", "NoOfReplicas=1",
                  "[ndb_mgmd]", "HostName=localhost",
                  "[mysqld]", "[mysqld]", NULL);

  CHECK(c1->equal(c1));

  CHECK(!c1->equal(c2));
  CHECK(!c2->equal(c1));
  CHECK(!c2->illegal_change(c1));
  CHECK(!c1->illegal_change(c2));


  ndbout_c("==================");
  ndbout_c("c1->print_diff(c2)");
  c1->print_diff(c2);
  ndbout_c("==================");
  ndbout_c("c2->print_diff(c1)");
  c2->print_diff(c1);
  ndbout_c("==================");
  delete c1;
  delete c2;
}


int
main(void){
  ndbout_c("1..1");
  diff_config();
  CHECK(check_params());
  ndbout_c("ok");
  exit(0);
}
