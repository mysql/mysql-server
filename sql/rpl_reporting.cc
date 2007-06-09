
#include "mysql_priv.h"
#include "rpl_reporting.h"

void
Slave_reporting_capability::report(loglevel level, int err_code, const char *msg, ...)
{
  void (*report_function)(const char *, ...);
  char buff[MAX_SLAVE_ERRMSG];
  char *pbuff= buff;
  uint pbuffsize= sizeof(buff);
  va_list args;
  va_start(args, msg);

  switch (level)
  {
  case ERROR_LEVEL:
    /*
      It's an error, it must be reported in Last_error and Last_errno in SHOW
      SLAVE STATUS.
    */
    pbuff= last_error.message;
    pbuffsize= sizeof(last_error.message);
    last_error.number = err_code;
    report_function= sql_print_error;
    break;
  case WARNING_LEVEL:
    report_function= sql_print_warning;
    break;
  case INFORMATION_LEVEL:
    report_function= sql_print_information;
    break;
  default:
    DBUG_ASSERT(0);                            // should not come here
    return;          // don't crash production builds, just do nothing
  }

  my_vsnprintf(pbuff, pbuffsize, msg, args);

  va_end(args);

  /* If the msg string ends with '.', do not add a ',' it would be ugly */
  report_function("Slave %s: %s%s Error_code: %d",
                  m_thread_name, pbuff,
                  (pbuff[0] && *(strend(pbuff)-1) == '.') ? "" : ",",
                  err_code);
}
