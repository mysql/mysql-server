#include "mysql_priv.h"

extern handlerton heap_hton;
extern handlerton myisam_hton;
extern handlerton myisammrg_hton;
extern handlerton binlog_hton;
#ifdef WITH_INNOBASE_STORAGE_ENGINE
extern handlerton innobase_hton;
#endif
#ifdef WITH_BERKELEY_STORAGE_ENGINE
extern handlerton berkeley_hton;
#endif
#ifdef WITH_EXAMPLE_STORAGE_ENGINE
extern handlerton example_hton;
#endif
#ifdef WITH_ARCHIVE_STORAGE_ENGINE
extern handlerton archive_hton;
#endif
#ifdef WITH_CSV_STORAGE_ENGINE
extern handlerton tina_hton;
#endif
#ifdef WITH_BLACKHOLE_STORAGE_ENGINE
extern handlerton blackhole_hton;
#endif
#ifdef WITH_FEDERATED_STORAGE_ENGINE
extern handlerton federated_hton;
#endif
#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
extern handlerton ndbcluster_hton;
#endif
#ifdef WITH_PARTITION_STORAGE_ENGINE
extern handlerton partition_hton;
#endif

/*
  This array is used for processing compiled in engines.
*/
handlerton *sys_table_types[]=
{
  &heap_hton,
  &myisam_hton,
#ifdef WITH_INNOBASE_STORAGE_ENGINE
  &innobase_hton,
#endif
#ifdef WITH_BERKELEY_STORAGE_ENGINE
  &berkeley_hton,
#endif
#ifdef WITH_EXAMPLE_STORAGE_ENGINE
  &example_hton,
#endif
#ifdef WITH_ARCHIVE_STORAGE_ENGINE
  &archive_hton,
#endif
#ifdef WITH_CSV_STORAGE_ENGINE
  &tina_hton,
#endif
#ifdef WITH_BLACKHOLE_STORAGE_ENGINE
  &blackhole_hton,
#endif
#ifdef WITH_FEDERATED_STORAGE_ENGINE
  &federated_hton,
#endif
#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
  &ndbcluster_hton,
#endif
#ifdef WITH_PARTITION_STORAGE_ENGINE
  &partition_hton,
#endif
  &myisammrg_hton,
  &binlog_hton,
  NULL
};
