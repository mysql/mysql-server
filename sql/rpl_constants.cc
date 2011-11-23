#include "rpl_constants.h"
#include "my_global.h"

void set_master_slave_proto(ushort *flag, enum Master_Slave_Proto pt)
{
  DBUG_ASSERT(flag != NULL && pt != BINLOG_END);
  *flag= 1 << pt;
}

bool is_master_slave_proto(ushort flag, enum Master_Slave_Proto pt)
{
  DBUG_ASSERT(pt != BINLOG_END);
  ushort tmp_flag= 1 << pt;
  return (flag == tmp_flag);
}
