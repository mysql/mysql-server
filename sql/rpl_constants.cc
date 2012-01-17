#include "rpl_constants.h"
#include "my_global.h"

/**
  Appends the protocol, specified by @code pt, to the @code flag.
  The set of the possible protocols is defined by an enumeration
  named @code Master_Slave_Proto.

  @param flag points to an @code uchar where the protocol is defined.
  @param pt   is the protocol to be appended.
*/
void add_master_slave_proto(ushort *flag, enum Master_Slave_Proto pt)
{
  DBUG_ASSERT(BINLOG_END < ((sizeof(ushort) * 8) + 1));
  DBUG_ASSERT(flag != NULL && pt != BINLOG_END);
  *flag|= 1 << pt;
}

/**
  Checks if a protocol @code pt is defined in the @code flag.

  @param flag where the protocol may be set.
  @param protocol that needs to be verified.

  @return true if the protocol is set, false otherwise.
*/
bool is_master_slave_proto(ushort flag, enum Master_Slave_Proto pt)
{
  DBUG_ASSERT(BINLOG_END < ((sizeof(ushort) * 8) + 1));
  DBUG_ASSERT(pt != BINLOG_END);
  ushort tmp_flag= 1 << pt;
  return (flag & tmp_flag);
}
