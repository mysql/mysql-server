/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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


/* This file defines all time functions */

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <m_ctype.h>
#include <time.h>

/*
** Todo: Move month and days to language files
*/

static String month_names[] = { "January", "February", "March", "April",
			       "May", "June", "July", "August",
			       "September", "October", "November", "December" };
static String day_names[] = { "Monday", "Tuesday", "Wednesday",
			     "Thursday", "Friday", "Saturday" ,"Sunday" };

/*
** Get a array of positive numbers from a string object.
** Each number is separated by 1 non digit character
** Return error if there is too many numbers.
** If there is too few numbers, assume that the numbers are left out
** from the high end. This allows one to give:
** DAY_TO_SECOND as "D MM:HH:SS", "MM:HH:SS" "HH:SS" or as seconds.
*/

bool get_interval_info(const char *str,uint length,uint count,
		       long *values)
{
  const char *end=str+length;
  uint i;
  while (str != end && !isdigit(*str))
    str++;

  for (i=0 ; i < count ; i++)
  {
    long value;
    for (value=0; str != end && isdigit(*str) ; str++)
      value=value*10L + (long) (*str - '0');
    values[i]= value;
    while (str != end && !isdigit(*str))
      str++;
    if (str == end && i != count-1)
    {
      i++;
      /* Change values[0...i-1] -> values[0...count-1] */
      bmove_upp((char*) (values+count), (char*) (values+i),
		sizeof(long)*i);
      bzero((char*) values, sizeof(long)*(count-i));
      break;
    }
  }
  return (str != end);
}

longlong Item_func_period_add::val_int()
{
  ulong period=(ulong) args[0]->val_int();
  int months=(int) args[1]->val_int();

  if ((null_value=args[0]->null_value || args[1]->null_value) ||
      period == 0L)
    return 0; /* purecov: inspected */
  return (longlong)
    convert_month_to_period((uint) ((int) convert_period_to_month(period)+
				    months));
}


longlong Item_func_period_diff::val_int()
{
  ulong period1=(ulong) args[0]->val_int();
  ulong period2=(ulong) args[1]->val_int();

  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0; /* purecov: inspected */
  return (longlong) ((long) convert_period_to_month(period1)-
		     (long) convert_period_to_month(period2));
}



longlong Item_func_to_days::val_int()
{
  TIME ltime;
  if (get_arg0_date(&ltime,0))
    return 0;
  return (longlong) calc_daynr(ltime.year,ltime.month,ltime.day);
}

longlong Item_func_dayofyear::val_int()
{
  TIME ltime;
  if (get_arg0_date(&ltime,0))
    return 0;
  return (longlong) calc_daynr(ltime.year,ltime.month,ltime.day) -
    calc_daynr(ltime.year,1,1) + 1;
}

longlong Item_func_dayofmonth::val_int()
{
  TIME ltime;
  (void) get_arg0_date(&ltime,1);
  return (longlong) ltime.day;
}

longlong Item_func_month::val_int()
{
  TIME ltime;
  (void) get_arg0_date(&ltime,1);
  return (longlong) ltime.month;
}

String* Item_func_monthname::val_str(String* str)
{
  uint month=(uint) Item_func_month::val_int();
  if (!month)					// This is also true for NULL
    return (String*) 0;
  return &month_names[month-1];
}

// Returns the quarter of the year

longlong Item_func_quarter::val_int()
{
  TIME ltime;
  (void) get_arg0_date(&ltime,1);
  return (longlong) ((ltime.month+2)/3);
}

longlong Item_func_hour::val_int()
{
  TIME ltime;
  (void) get_arg0_time(&ltime);
  return ltime.hour;
}

longlong Item_func_minute::val_int()
{
  TIME ltime;
  (void) get_arg0_time(&ltime);
  return ltime.minute;
}
// Returns the second in time_exp in the range of 0 - 59

longlong Item_func_second::val_int()
{
  TIME ltime;
  (void) get_arg0_time(&ltime);
  return ltime.second;
}


// Returns the week of year in the range of 0 - 53

longlong Item_func_week::val_int()
{
  uint year;
  TIME ltime;
  if (get_arg0_date(&ltime,0))
    return 0;
  return (longlong) calc_week(&ltime, 0, args[1]->val_int() == 0, &year);
}


longlong Item_func_yearweek::val_int()
{
  uint year,week;
  TIME ltime;
  if (get_arg0_date(&ltime,0))
    return 0;
  week=calc_week(&ltime, 1, args[1]->val_int() == 0, &year);
  return week+year*100;
}


/* weekday() has a automatic to_days() on item */

longlong Item_func_weekday::val_int()
{
  ulong tmp_value=(ulong) args[0]->val_int();
  if ((null_value=(args[0]->null_value || !tmp_value)))
    return 0; /* purecov: inspected */

  return (longlong) calc_weekday(tmp_value,odbc_type)+test(odbc_type);
}

String* Item_func_dayname::val_str(String* str)
{
  uint weekday=(uint) val_int();		// Always Item_func_daynr()
  if (null_value)
    return (String*) 0;
  return &day_names[weekday];
}


longlong Item_func_year::val_int()
{
  TIME ltime;
  (void) get_arg0_date(&ltime,1);
  return (longlong) ltime.year;
}


longlong Item_func_unix_timestamp::val_int()
{
  if (arg_count == 0)
    return (longlong) current_thd->query_start();
  if (args[0]->type() == FIELD_ITEM)
  {						// Optimize timestamp field
    Field *field=((Item_field*) args[0])->field;
    if (field->type() == FIELD_TYPE_TIMESTAMP)
      return ((Field_timestamp*) field)->get_timestamp();
  }
  String *str=args[0]->val_str(&value);
  if ((null_value=args[0]->null_value))
  {
    return 0; /* purecov: inspected */
  }
  return (longlong) str_to_timestamp(str->ptr(),str->length());
}


longlong Item_func_time_to_sec::val_int()
{
  TIME ltime;
  (void) get_arg0_time(&ltime);
  return ltime.hour*3600L+ltime.minute*60+ltime.second;
}


/*
** Convert a string to a interval value
** To make code easy, allow interval objects without separators.
*/

static bool get_interval_value(Item *args,interval_type int_type,
			       String *str_value, INTERVAL *t)
{
  long array[4],value;
  const char *str;
  uint32 length;
  LINT_INIT(value);  LINT_INIT(str); LINT_INIT(length);

  bzero((char*) t,sizeof(*t));
  if ((int) int_type <= INTERVAL_SECOND)
  {
    value=(long) args->val_int();
    if (args->null_value)
      return 1;
    if (value < 0)
    {
      t->neg=1;
      value= -value;
    }
  }
  else
  {
    String *res;
    if (!(res=args->val_str(str_value)))
      return (1);

    /* record negative intervalls in t->neg */
    str=res->ptr();
    const char *end=str+res->length();
    while (str != end && isspace(*str))
      str++;
    if (str != end && *str == '-')
    {
      t->neg=1;
      str++;
    }
    length=(uint32) (end-str);		// Set up pointers to new str
  }

  switch (int_type) {
  case INTERVAL_YEAR:
    t->year=value;
    break;
  case INTERVAL_MONTH:
    t->month=value;
    break;
  case INTERVAL_DAY:
    t->day=value;
    break;
  case INTERVAL_HOUR:
    t->hour=value;
    break;
  case INTERVAL_MINUTE:
    t->minute=value;
    break;
  case INTERVAL_SECOND:
    t->second=value;
    break;
  case INTERVAL_YEAR_MONTH:			// Allow YEAR-MONTH YYYYYMM
    if (get_interval_info(str,length,2,array))
      return (1);
    t->year=array[0];
    t->month=array[1];
    break;
  case INTERVAL_DAY_HOUR:
    if (get_interval_info(str,length,2,array))
      return (1);
    t->day=array[0];
    t->hour=array[1];
    break;
  case INTERVAL_DAY_MINUTE:
    if (get_interval_info(str,length,3,array))
      return (1);
    t->day=array[0];
    t->hour=array[1];
    t->minute=array[2];
    break;
  case INTERVAL_DAY_SECOND:
    if (get_interval_info(str,length,4,array))
      return (1);
    t->day=array[0];
    t->hour=array[1];
    t->minute=array[2];
    t->second=array[3];
    break;
  case INTERVAL_HOUR_MINUTE:
    if (get_interval_info(str,length,2,array))
      return (1);
    t->hour=array[0];
    t->minute=array[1];
    break;
  case INTERVAL_HOUR_SECOND:
    if (get_interval_info(str,length,3,array))
      return (1);
    t->hour=array[0];
    t->minute=array[1];
    t->second=array[2];
    break;
  case INTERVAL_MINUTE_SECOND:
    if (get_interval_info(str,length,2,array))
      return (1);
    t->minute=array[0];
    t->second=array[1];
    break;
  }
  return 0;
}


String *Item_date::val_str(String *str)
{
  ulong value=(ulong) val_int();
  if (null_value)
    return (String*) 0;
  if (!value)					// zero daynr
  {
    str->copy("0000-00-00");
    return str;
  }
  if (str->alloc(11))
    return &empty_string;			/* purecov: inspected */
  sprintf((char*) str->ptr(),"%04d-%02d-%02d",
	  (int) (value/10000L) % 10000,
	  (int) (value/100)%100,
	  (int) (value%100));
  str->length(10);
  return str;
}


bool Item_date::save_in_field(Field *field)
{
  TIME ltime;
  timestamp_type t_type=TIMESTAMP_FULL;
  if (get_date(&ltime,1))
  {
    if (null_value)
      return set_field_to_null(field);
    t_type=TIMESTAMP_NONE;			// Error
  }
  field->set_notnull();
  field->store_time(&ltime,t_type);
  return 0;
}


longlong Item_func_from_days::val_int()
{
  longlong value=args[0]->val_int();
  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */

  uint year,month,day;
  get_date_from_daynr((long) value,&year,&month,&day);
  return (longlong) (year*10000L+month*100+day);
}


void Item_func_curdate::fix_length_and_dec()
{
  struct tm tm_tmp,*start;
  time_t query_start=current_thd->query_start();
  decimals=0; max_length=10;
  localtime_r(&query_start,&tm_tmp);
  start=&tm_tmp;
  value=(longlong) ((ulong) ((uint) start->tm_year+1900)*10000L+
		    ((uint) start->tm_mon+1)*100+
		    (uint) start->tm_mday);
  /* For getdate */
  ltime.year=	start->tm_year+1900;
  ltime.month=	start->tm_mon+1;
  ltime.day=	start->tm_mday;
  ltime.hour=	0;
  ltime.minute=	0;
  ltime.second=	0;
  ltime.second_part=0;
  ltime.neg=0;
  ltime.time_type=TIMESTAMP_DATE;
}

bool Item_func_curdate::get_date(TIME *res,
				 bool fuzzy_date __attribute__((unused)))
{
  *res=ltime;
  return 0;
}

void Item_func_curtime::fix_length_and_dec()
{
  struct tm tm_tmp,*start;
  time_t query_start=current_thd->query_start();
  decimals=0; max_length=8;
  localtime_r(&query_start,&tm_tmp);
  start=&tm_tmp;
  value=(longlong) ((ulong) ((uint) start->tm_hour)*10000L+
		    (ulong) (((uint) start->tm_min)*100L+
			     (uint) start->tm_sec));
  sprintf(buff,"%02d:%02d:%02d",
	  (int) start->tm_hour,
	  (int) start->tm_min,
	  (int) start->tm_sec);
  buff_length=(uint) strlen(buff);
}

void Item_func_now::fix_length_and_dec()
{
  struct tm tm_tmp,*start;
  time_t query_start=current_thd->query_start();
  decimals=0; max_length=19;
  localtime_r(&query_start,&tm_tmp);
  start=&tm_tmp;
  value=((longlong) ((ulong) ((uint) start->tm_year+1900)*10000L+
		     (((uint) start->tm_mon+1)*100+
		      (uint) start->tm_mday))*(longlong) 1000000L+
	 (longlong) ((ulong) ((uint) start->tm_hour)*10000L+
		     (ulong) (((uint) start->tm_min)*100L+
			    (uint) start->tm_sec)));
  sprintf(buff,"%04d-%02d-%02d %02d:%02d:%02d",
	  ((int) (start->tm_year+1900)) % 10000,
	  (int) start->tm_mon+1,
	  (int) start->tm_mday,
	  (int) start->tm_hour,
	  (int) start->tm_min,
	  (int) start->tm_sec);
  buff_length=(uint) strlen(buff);
  /* For getdate */
  ltime.year=	start->tm_year+1900;
  ltime.month=	start->tm_mon+1;
  ltime.day=	start->tm_mday;
  ltime.hour=	start->tm_hour;
  ltime.minute=	start->tm_min;
  ltime.second=	start->tm_sec;
  ltime.second_part=0;
  ltime.neg=0;
  ltime.time_type=TIMESTAMP_FULL;
}

bool Item_func_now::get_date(TIME *res,
			     bool fuzzy_date __attribute__((unused)))
{
  *res=ltime;
  return 0;
}


bool Item_func_now::save_in_field(Field *to)
{
  to->set_notnull();
  to->store_time(&ltime,TIMESTAMP_FULL);
  return 0;
}


String *Item_func_sec_to_time::val_str(String *str)
{
  char buff[23];
  const char *sign="";
  longlong seconds=(longlong) args[0]->val_int();
  if ((null_value=args[0]->null_value))
    return (String*) 0;
  if (seconds < 0)
  {
    seconds= -seconds;
    sign= "-";
  }
  uint sec= (uint) (seconds % 3600);
  sprintf(buff,"%s%02lu:%02u:%02u",sign,(long) (seconds/3600),
	  sec/60, sec % 60);
  str->copy(buff,(uint) strlen(buff));
  return str;
}


longlong Item_func_sec_to_time::val_int()
{
  longlong seconds=args[0]->val_int();
  longlong sign=1;
  if ((null_value=args[0]->null_value))
    return 0;
  if (seconds < 0)
  {
    seconds= -seconds;
    sign= -1;
  }
  return sign*((seconds / 3600)*10000+((seconds/60) % 60)*100+ (seconds % 60));
}


void Item_func_date_format::fix_length_and_dec()
{
  decimals=0;
  if (args[1]->type() == STRING_ITEM)
  {						// Optimize the normal case
    fixed_length=1;
    max_length=format_length(((Item_string*) args[1])->const_string());
  }
  else
  {
    fixed_length=0;
    max_length=args[1]->max_length*10;
    set_if_smaller(max_length,MAX_BLOB_WIDTH);
  }
  maybe_null=1;					// If wrong date
}


uint Item_func_date_format::format_length(const String *format)
{
  uint size=0;
  const char *ptr=format->ptr();
  const char *end=ptr+format->length();

  for (; ptr != end ; ptr++)
  {
    if (*ptr != '%' || ptr == end-1)
      size++;
    else
    {
      switch(*++ptr) {
      case 'M': /* month, textual */
      case 'W': /* day (of the week), textual */
	size += 9;
	break;
      case 'D': /* day (of the month), numeric plus english suffix */
      case 'Y': /* year, numeric, 4 digits */
      case 'x': /* Year, used with 'v' */
      case 'X': /* Year, used with 'v, where week starts with Monday' */
	size += 4;
	break;
      case 'a': /* locale's abbreviated weekday name (Sun..Sat) */
      case 'b': /* locale's abbreviated month name (Jan.Dec) */
      case 'j': /* day of year (001..366) */
	size += 3;
	break;
      case 'U': /* week (00..52) */
      case 'u': /* week (00..52), where week starts with Monday */
      case 'V': /* week 1..53 used with 'x' */
      case 'v': /* week 1..53 used with 'x', where week starts with Monday */
      case 'H': /* hour (00..23) */
      case 'y': /* year, numeric, 2 digits */
      case 'm': /* month, numeric */
      case 'd': /* day (of the month), numeric */
      case 'h': /* hour (01..12) */
      case 'I': /* --||-- */
      case 'i': /* minutes, numeric */
      case 'k': /* hour ( 0..23) */
      case 'l': /* hour ( 1..12) */
      case 'p': /* locale's AM or PM */
      case 'S': /* second (00..61) */
      case 's': /* seconds, numeric */
      case 'c': /* month (0..12) */
      case 'e': /* day (0..31) */
	size += 2;
	break;
      case 'r': /* time, 12-hour (hh:mm:ss [AP]M) */
	size += 11;
	break;
      case 'T': /* time, 24-hour (hh:mm:ss) */
	size += 8;
	break;
      case 'w': /* day (of the week), numeric */
      case '%':
      default:
	size++;
	break;
      }
    }
  }
  return size;
}


String *Item_func_date_format::val_str(String *str)
{
  String *format;
  TIME l_time;
  char intbuff[15];
  uint size,weekday;

  if (!date_or_time)
  {
    if (get_arg0_date(&l_time,1))
      return 0;
  }
  else
  {
    String *res;
    if (!(res=args[0]->val_str(str)))
    {
      null_value=1;
      return 0;
    }
    if (str_to_time(res->ptr(),res->length(),&l_time))
    {
      null_value=1;
      return 0;
    }
    l_time.year=l_time.month=l_time.day=0;
    null_value=0;
  }

  if (!(format = args[1]->val_str(str)) || !format->length())
  {
    null_value=1;
    return 0;
  }

  if (fixed_length)
    size=max_length;
  else
    size=format_length(format);
  if (format == str)
    str=&str_value;				// Save result here
  if (str->alloc(size))
  {
    null_value=1;
    return 0;
  }
  str->length(0);

  /* Create the result string */
  const char *ptr=format->ptr();
  const char *end=ptr+format->length();
  for ( ; ptr != end ; ptr++)
  {
    if (*ptr != '%' || ptr+1 == end)
      str->append(*ptr);
    else
    {
      switch (*++ptr) {
      case 'M':
	if(!l_time.month)
	{
	  null_value=1;
	  return 0;
	}
	str->append(month_names[l_time.month-1]);
	break;
      case 'b':
	if(!l_time.month)
	{
	  null_value=1;
	  return 0;
	}
	str->append(month_names[l_time.month-1].ptr(),3);
	break;
      case 'W':
	if(date_or_time)
	{
	  null_value=1;
	  return 0;
	}
	weekday=calc_weekday(calc_daynr(l_time.year,l_time.month,l_time.day),0);
	str->append(day_names[weekday]);
	break;
      case 'a':
	if(date_or_time)
	{
	  null_value=1;
	  return 0;
	}
	weekday=calc_weekday(calc_daynr(l_time.year,l_time.month,l_time.day),0);
	str->append(day_names[weekday].ptr(),3);
	break;
      case 'D':
	if(date_or_time)
	{
	  null_value=1;
	  return 0;
	}
	sprintf(intbuff,"%d",l_time.day);
	str->append(intbuff);
	if (l_time.day >= 10 &&  l_time.day <= 19)
	  str->append("th");
	else
	{
	  switch (l_time.day %10)
	  {
	  case 1:
	    str->append("st");
	    break;
	  case 2:
	    str->append("nd");
	    break;
	  case 3:
	    str->append("rd");
	    break;
	  default:
	    str->append("th");
	    break;
	  }
	}
	break;
      case 'Y':
	sprintf(intbuff,"%04d",l_time.year);
	str->append(intbuff);
	break;
      case 'y':
	sprintf(intbuff,"%02d",l_time.year%100);
	str->append(intbuff);
	break;
      case 'm':
	sprintf(intbuff,"%02d",l_time.month);
	str->append(intbuff);
	break;
      case 'c':
	sprintf(intbuff,"%d",l_time.month);
	str->append(intbuff);
	break;
      case 'd':
	sprintf(intbuff,"%02d",l_time.day);
	str->append(intbuff);
	break;
      case 'e':
	sprintf(intbuff,"%d",l_time.day);
	str->append(intbuff);
	break;
      case 'H':
	sprintf(intbuff,"%02d",l_time.hour);
	str->append(intbuff);
	break;
      case 'h':
      case 'I':
	sprintf(intbuff,"%02d", (l_time.hour+11)%12+1);
	str->append(intbuff);
	break;
      case 'i':					/* minutes */
	sprintf(intbuff,"%02d",l_time.minute);
	str->append(intbuff);
	break;
      case 'j':
	if(date_or_time)
	{
	  null_value=1;
	  return 0;
	}
	sprintf(intbuff,"%03d",
		(int) (calc_daynr(l_time.year,l_time.month,l_time.day) -
		       calc_daynr(l_time.year,1,1)) + 1);
	str->append(intbuff);
	break;
      case 'k':
	sprintf(intbuff,"%d",l_time.hour);
	str->append(intbuff);
	break;
      case 'l':
	sprintf(intbuff,"%d", (l_time.hour+11)%12+1);
	str->append(intbuff);
	break;
      case 'p':
	str->append(l_time.hour < 12 ? "AM" : "PM");
	break;
      case 'r':
	sprintf(intbuff,(l_time.hour < 12) ? "%02d:%02d:%02d AM" :
		"%02d:%02d:%02d PM",(l_time.hour+11)%12+1,l_time.minute,
		l_time.second);
	str->append(intbuff);
	break;
      case 'S':
      case 's':
	sprintf(intbuff,"%02d",l_time.second);
	str->append(intbuff);
	break;
      case 'T':
	sprintf(intbuff,"%02d:%02d:%02d",l_time.hour,l_time.minute,l_time.second);
	str->append(intbuff);
	break;
      case 'U':
      case 'u':
      {
	uint year;
	sprintf(intbuff,"%02d",calc_week(&l_time, 0, (*ptr) == 'U', &year));
	str->append(intbuff);
      }
      break;
      case 'v':
      case 'V':
      {
	uint year;
	sprintf(intbuff,"%02d",calc_week(&l_time, 1, (*ptr) == 'V', &year));
	str->append(intbuff);
      }
      break;
      case 'x':
      case 'X':
      {
	uint year;
	(void) calc_week(&l_time, 1, (*ptr) == 'X', &year);
	sprintf(intbuff,"%04d",year);
	str->append(intbuff);
      }
      break;
      case 'w':
	weekday=calc_weekday(calc_daynr(l_time.year,l_time.month,l_time.day),1);
	sprintf(intbuff,"%01d",weekday);
	str->append(intbuff);
	break;
      default:
	str->append(*ptr);
	break;
      }
    }
  }
  return str;
}


String *Item_func_from_unixtime::val_str(String *str)
{
  struct tm tm_tmp,*start;
  time_t tmp=(time_t) args[0]->val_int();
  if ((null_value=args[0]->null_value))
    return 0;
  localtime_r(&tmp,&tm_tmp);
  start=&tm_tmp;
  if (str->alloc(20))
    return str;					/* purecov: inspected */
  sprintf((char*) str->ptr(),"%04d-%02d-%02d %02d:%02d:%02d",
	  (int) start->tm_year+1900,
	  (int) start->tm_mon+1,
	  (int) start->tm_mday,
	  (int) start->tm_hour,
	  (int) start->tm_min,
	  (int) start->tm_sec);
  str->length(19);
  return str;
}


longlong Item_func_from_unixtime::val_int()
{
  time_t tmp=(time_t) (ulong) args[0]->val_int();
  if ((null_value=args[0]->null_value))
    return 0;
  struct tm tm_tmp,*start;
  localtime_r(&tmp,&tm_tmp);
  start= &tm_tmp;
  return ((longlong) ((ulong) ((uint) start->tm_year+1900)*10000L+
		      (((uint) start->tm_mon+1)*100+
		       (uint) start->tm_mday))*LL(1000000)+
	  (longlong) ((ulong) ((uint) start->tm_hour)*10000L+
		      (ulong) (((uint) start->tm_min)*100L+
			       (uint) start->tm_sec)));
}

bool Item_func_from_unixtime::get_date(TIME *ltime,
				       bool fuzzy_date __attribute__((unused)))
{
  time_t tmp=(time_t) (ulong) args[0]->val_int();
  if ((null_value=args[0]->null_value))
    return 1;
  struct tm tm_tmp,*start;
  localtime_r(&tmp,&tm_tmp);
  start= &tm_tmp;
  ltime->year=	start->tm_year+1900;
  ltime->month=	start->tm_mon+1;
  ltime->day=	start->tm_mday;
  ltime->hour=	start->tm_hour;
  ltime->minute=start->tm_min;
  ltime->second=start->tm_sec;
  ltime->second_part=0;
  ltime->neg=0;
  return 0;
}

  /* Here arg[1] is a Item_interval object */

bool Item_date_add_interval::get_date(TIME *ltime, bool fuzzy_date)
{
  long period,sign;
  INTERVAL interval;

  if (args[0]->get_date(ltime,0) ||
      get_interval_value(args[1],int_type,&value,&interval))
    goto null_date;
  sign= (interval.neg ? -1 : 1);
  if (date_sub_interval)
    sign = -sign;

  null_value=0;
  switch (int_type) {
  case INTERVAL_SECOND:
  case INTERVAL_MINUTE:
  case INTERVAL_HOUR:
  case INTERVAL_MINUTE_SECOND:
  case INTERVAL_HOUR_SECOND:
  case INTERVAL_HOUR_MINUTE:
  case INTERVAL_DAY_SECOND:
  case INTERVAL_DAY_MINUTE:
  case INTERVAL_DAY_HOUR:
    long sec,days,daynr;
    ltime->time_type=TIMESTAMP_FULL;		// Return full date

    sec=((ltime->day-1)*3600*24L+ltime->hour*3600+ltime->minute*60+
	 ltime->second +
	 sign*(interval.day*3600*24L +
	       interval.hour*3600+interval.minute*60+interval.second));
    days=sec/(3600*24L); sec=sec-days*3600*24L;
    if (sec < 0)
    {
      days--;
      sec+=3600*24L;
    }
    ltime->second=sec % 60;
    ltime->minute=sec/60 % 60;
    ltime->hour=sec/3600;
    daynr= calc_daynr(ltime->year,ltime->month,1) + days;
    get_date_from_daynr(daynr,&ltime->year,&ltime->month,&ltime->day);
    if (daynr < 0 || daynr >= 3652424) // Day number from year 0 to 9999-12-31
      goto null_date;
    break;
  case INTERVAL_DAY:
    period= calc_daynr(ltime->year,ltime->month,ltime->day) +
      sign*interval.day;
    if (period < 0 || period >= 3652424) // Daynumber from year 0 to 9999-12-31
      goto null_date;
    get_date_from_daynr((long) period,&ltime->year,&ltime->month,&ltime->day);
    break;
  case INTERVAL_YEAR:
    ltime->year += sign*interval.year;
    if ((int) ltime->year < 0 || ltime->year >= 10000L)
      goto null_date;
    if (ltime->month == 2 && ltime->day == 29 &&
	calc_days_in_year(ltime->year) != 366)
      ltime->day=28;				// Was leap-year
    break;
  case INTERVAL_YEAR_MONTH:
  case INTERVAL_MONTH:
    period= (ltime->year*12 + sign*interval.year*12 +
	     ltime->month-1 + sign*interval.month);
    if (period < 0 || period >= 120000L)
      goto null_date;
    ltime->year= (uint) (period / 12);
    ltime->month= (uint) (period % 12L)+1;
    /* Adjust day if the new month doesn't have enough days */
    if (ltime->day > days_in_month[ltime->month-1])
    {
      ltime->day = days_in_month[ltime->month-1];
      if (ltime->month == 2 && calc_days_in_year(ltime->year) == 366)
	ltime->day++;				// Leap-year
    }
    break;
  default:
    goto null_date;
  }
  return 0;					// Ok

 null_date:
  return (null_value=1);
}


String *Item_date_add_interval::val_str(String *str)
{
  TIME ltime;

  if (Item_date_add_interval::get_date(&ltime,0))
    return 0;
  if (ltime.time_type == TIMESTAMP_DATE)
  {
    if (str->alloc(11))
      goto null_date;
    sprintf((char*) str->ptr(),"%04d-%02d-%02d",
	    ltime.year,ltime.month,ltime.day);
    str->length(10);
  }
  else
  {
    if (str->alloc(20))
      goto null_date;
    sprintf((char*) str->ptr(),"%04d-%02d-%02d %02d:%02d:%02d",
	    ltime.year,ltime.month,ltime.day,
	    ltime.hour,ltime.minute,ltime.second);
    str->length(19);
  }
  return str;

 null_date:
  null_value=1;
  return 0;
}

longlong Item_date_add_interval::val_int()
{
  TIME ltime;
  if (Item_date_add_interval::get_date(&ltime,0))
    return (longlong) 0;
  return ((longlong) (((ulong) ltime.year)*10000L+
		      (((uint) ltime.month)*100+
		       (uint) ltime.day))*(longlong) 1000000L+
	  (longlong) ((ulong) ((uint) ltime.hour)*10000L+
		      (ulong) (((uint)ltime.minute)*100L+
			       (uint) ltime.second)));
}

void Item_extract::fix_length_and_dec()
{
  value.alloc(32);				// alloc buffer

  maybe_null=1;					// If wrong date
  switch (int_type) {
  case INTERVAL_YEAR:		max_length=4; date_value=1; break;
  case INTERVAL_YEAR_MONTH:	max_length=6; date_value=1; break;
  case INTERVAL_MONTH:		max_length=2; date_value=1; break;
  case INTERVAL_DAY:		max_length=2; date_value=1; break;
  case INTERVAL_DAY_HOUR:	max_length=9; date_value=0; break;
  case INTERVAL_DAY_MINUTE:	max_length=11; date_value=0; break;
  case INTERVAL_DAY_SECOND:	max_length=13; date_value=0; break;
  case INTERVAL_HOUR:		max_length=2; date_value=0; break;
  case INTERVAL_HOUR_MINUTE:	max_length=4; date_value=0; break;
  case INTERVAL_HOUR_SECOND:	max_length=6; date_value=0; break;
  case INTERVAL_MINUTE:		max_length=2; date_value=0; break;
  case INTERVAL_MINUTE_SECOND:	max_length=4; date_value=0; break;
  case INTERVAL_SECOND:		max_length=2; date_value=0; break;
  }
}


longlong Item_extract::val_int()
{
  TIME ltime;
  long neg;
  if (date_value)
  {
    if (get_arg0_date(&ltime,1))
      return 0;
    neg=1;
  }
  else
  {
    String *res= args[0]->val_str(&value);
    if (!res || str_to_time(res->ptr(),res->length(),&ltime))
    {
      null_value=1;
      return 0;
    }
    neg= ltime.neg ? -1 : 1;
    null_value=0;
  }

  switch (int_type) {
  case INTERVAL_YEAR:		return ltime.year;
  case INTERVAL_YEAR_MONTH:	return ltime.year*100L+ltime.month;
  case INTERVAL_MONTH:		return ltime.month;
  case INTERVAL_DAY:		return ltime.day;
  case INTERVAL_DAY_HOUR:	return (long) (ltime.day*100L+ltime.hour)*neg;
  case INTERVAL_DAY_MINUTE:	return (long) (ltime.day*10000L+
					       ltime.hour*100L+
					       ltime.minute)*neg;
  case INTERVAL_DAY_SECOND:	 return ((longlong) ltime.day*1000000L+
					 (longlong) (ltime.hour*10000L+
						     ltime.minute*100+
						     ltime.second))*neg;
  case INTERVAL_HOUR:		return (long) ltime.hour*neg;
  case INTERVAL_HOUR_MINUTE:	return (long) (ltime.hour*100+ltime.minute)*neg;
  case INTERVAL_HOUR_SECOND:	return (long) (ltime.hour*10000+ltime.minute*100+
					       ltime.second)*neg;
  case INTERVAL_MINUTE:		return (long) ltime.minute*neg;
  case INTERVAL_MINUTE_SECOND:	return (long) (ltime.minute*100+ltime.second)*neg;
  case INTERVAL_SECOND:		return (long) ltime.second*neg;
  }
  return 0;					// Impossible
}
