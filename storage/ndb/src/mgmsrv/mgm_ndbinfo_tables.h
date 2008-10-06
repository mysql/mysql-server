#include <ndbinfo.h>

DECLARE_NDBINFO_TABLE(ndbinfo_TABLES,1) =
{{"TABLES",1,0},
 {
   {"TABLE_NAME",NDBINFO_TYPE_STRING}
 }
};

DECLARE_NDBINFO_TABLE(ndbinfo_LOGDESTINATION,5) =
{{"LOGDESTINATION",5,0},
 {
   {"NODE_ID",NDBINFO_TYPE_NUMBER},
   {"TYPE",NDBINFO_TYPE_STRING},
   {"PARAMS",NDBINFO_TYPE_STRING},
   {"CURRENT_SIZE",NDBINFO_TYPE_NUMBER},
   {"MAX_SIZE",NDBINFO_TYPE_NUMBER},
 }
};

int number_mgm_ndbinfo_tables= 2;

struct ndbinfo_table *mgm_ndbinfo_tables[] = {
  &ndbinfo_TABLES.t,
  &ndbinfo_LOGDESTINATION.t,
};

