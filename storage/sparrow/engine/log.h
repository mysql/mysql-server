#ifndef _engine_log_h_
#define _engine_log_h_

#include "mysqld_error.h"
#include "mysql/components/services/log_builtins.h"

namespace Sparrow {

void spw_print_msg(loglevel level, const char* str_format, ...) MY_ATTRIBUTE((format(printf, 2, 3)));

inline void spw_print_msg(loglevel level, const char* str_format, ...) 
{
	va_list args;
	va_start (args, str_format);
	char	buf[LOG_BUFF_MAX];
	std::vsnprintf(buf, LOG_BUFF_MAX, str_format, args);
	LogErr(level, ER_LOG_PRINTF_MSG, buf);
	va_end (args);
}

}

#define spw_print_system(str_format, ...)	\
do {	\
	spw_print_msg(SYSTEM_LEVEL, str_format, ##__VA_ARGS__); \
} while (0);

#define spw_print_information(str_format, ...)	\
do {	\
	spw_print_msg(INFORMATION_LEVEL, str_format, ##__VA_ARGS__); \
} while (0);

#define spw_print_warning(str_format, ...)	\
do {	\
	spw_print_msg(WARNING_LEVEL, str_format, ##__VA_ARGS__); \
} while (0);

#define spw_print_error(str_format, ...)	\
do {	\
	spw_print_msg(ERROR_LEVEL, str_format, ##__VA_ARGS__); \
} while (0);



#endif	// _engine_log_h_
