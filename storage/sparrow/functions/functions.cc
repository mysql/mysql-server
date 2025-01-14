/*
	InfoVista functions.
*/

#include "functions.h"
#include "ipaddress.h"
#include "../engine/misc.h"
#include "../engine/internalapi.h"

#include "../engine/log.h"
#include "sql/sql_class.h"
#include "sql/current_thd.h"
#include "sql/tztime.h"

using namespace IvFunctions;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Function IPTOSTR
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool Item_func_iptostr::resolve_type(THD *thd) {
  if (Item_str_func::resolve_type(thd)) return true;
  set_data_type_string(ulonglong(39));	// IPv6 max textual length.
  set_nullable(true);  // Can be NULL, e.g. in case of badly formed input string
  return false;
}

String* Item_func_iptostr::val_str(String* str) {
	assert(fixed == 1);
	Item& arg = *args[0];
	String* s = arg.val_str(str);
	if (arg.is_null() || s->charset() != &my_charset_bin || (s->length() != 4 && s->length() != 16)) {
		null_value = 1;
		return 0;
	}
	IpAddress address(reinterpret_cast<const uint8_t*>(s->ptr()), s->length());
	tmp_value.alloc(max_length);
	tmp_value.length(address.print(const_cast<char*>(tmp_value.ptr())));
	null_value = 0;

	return &tmp_value;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Function STRTOIP
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool Item_func_strtoip::resolve_type(THD *thd) {
  if (Item_str_func::resolve_type(thd)) return true;
  set_data_type_string(ulonglong(16));
  set_nullable(true);  // Can be NULL, e.g. in case of badly formed input string
  return false;
}

String* Item_func_strtoip::val_str(String* str) {
	assert(fixed == 1);
	Item& arg = *args[0];
	if (arg.is_null()) {
		null_value = 1;
		return 0;
	}
	String* s = arg.val_str(str);
	tmp_value.alloc(max_length);
	IpAddress address(reinterpret_cast<const uint8_t*>(tmp_value.ptr()), max_length);
	if (!address.parse(s->ptr(), s->length())) {
		null_value = 1;
		return 0;
	}
	tmp_value.length(address.isV4() ? 4 : 16);
	null_value = 0;
	return &tmp_value;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Function MASKIP
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool Item_func_maskip::resolve_type(THD *thd) {
  if (Item_str_func::resolve_type(thd)) return true;
  set_data_type_string(ulonglong(16));
  set_nullable(true);  // Can be NULL, e.g. in case of badly formed input string
  return false;
}

String* Item_func_maskip::val_str(String* str) {
	assert(fixed == 1);
	Item& arg0 = *args[0];
	String* s0 = arg0.val_str(str);
	if (arg0.is_null() || s0->charset() != &my_charset_bin || (s0->length() != 4 && s0->length() != 16)) {
		null_value = 1;
		return 0;
	}
	tmp_value.alloc(max_length);
	tmp_value.set(*s0, 0, s0->length());
	Item& arg1 = *args[1];
	String* s1 = arg1.val_str(str);
	if (arg1.is_null() || s1->charset() != &my_charset_bin || (s1->length() != 4 && s1->length() != 16)) {
		null_value = 1;
		return 0;
	}
	IpAddress address(reinterpret_cast<const uint8_t*>(tmp_value.ptr()), tmp_value.length());
	if (!address.applyMask(IpAddress(reinterpret_cast<const uint8_t*>(s1->ptr()), s1->length()))) {
		null_value = 1;
		return 0;
	}
	tmp_value.length(address.isV4() ? 4 : 16);
	null_value = 0;
	return &tmp_value;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Function GETIPMASK
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool Item_func_getipmask::resolve_type(THD *thd) {
  if (Item_str_func::resolve_type(thd)) return true;
  set_data_type_string(ulonglong(16));
  set_nullable(true);  // Can be NULL, e.g. in case of badly formed input string
  return false;
}

String* Item_func_getipmask::val_str(String* str) {
	Item& arg = *args[0];
	if (arg.is_null()) {
		null_value = 1;
		return 0;
	}
	longlong bits = arg.val_int();
	if (bits < 0 || bits > 128) {
		null_value = 1;
		return 0;
	}
	const uint32_t length = bits <= 32 ? 4 : 16;
	if (str->alloc(length)) {
		null_value = 1;
		return 0;
	}
	str->length(length);
	IpAddress address(reinterpret_cast<const uint8_t*>(str->ptr()), length);
	address.makeMask(static_cast<int>(bits));
	null_value = 0;
	return str;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Function ISIPV4
//////////////////////////////////////////////////////////////////////////////////////////////////////
// Replaced with MySQL built-in IP_ISV4()

/*bool Item_func_isipv4::val_bool() {
	Item& arg = *args[0];
	if (arg.is_null()) {
		null_value = 1;
		return 0;
	}
	String tmp;
	String* s = arg.val_str(&tmp);
	if (s == 0 || (s->length() != 4 && s->length() != 16) || s->charset() != &my_charset_bin) {
		null_value = 1;
		return 0;
	}
	IpAddress address(reinterpret_cast<const uint8_t*>(s->ptr()), s->length());
	null_value = 0;
	return address.isV4();
}*/

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Function ISIPPRIVATE
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool Item_func_isipprivate::resolve_type(THD *thd) {
  max_length = 1;
  set_nullable(true);  // Can be NULL, e.g. in case of badly formed input string
  return Item_int_func::resolve_type(thd);
}

bool Item_func_isipprivate::val_bool() {
	Item& arg = *args[0];
	if (arg.is_null()) {
		null_value = 1;
		return 0;
	}
	String tmp;
	String* s = arg.val_str(&tmp);
	if (s == 0 || (s->length() != 4 && s->length() != 16) || s->charset() != &my_charset_bin) {
		null_value = 1;
		return 0;
	}
	IpAddress address(reinterpret_cast<const uint8_t*>(s->ptr()), s->length());
	null_value = 0;
	return address.isPrivate();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions TADJUST and TADJUSTW
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool Item_func_tadjust::resolve_type(THD *thd) {
	if (Item_int_func::resolve_type(thd)) return true;
	set_nullable(true);
	max_length = MAX_BIGINT_WIDTH + 1;
	return false;
}

// Gets the number of periods.
int Item_func_tadjust::getN() {
	if (nArg_ == -1) {
		return 1;
	} else {
		Item& arg = *args[nArg_];
		if (arg.is_null()) {
			return -1;
		} else {
			return static_cast<int>(arg.val_int());
		}
	}
}

// Gets the first day of the week (0 is Monday ... 6 is Sunday).
int Item_func_tadjust::getFdow() {
	if (fdowArg_ == -1) {
		// Use MySQL setting: it is Sunday or Monday.
		// See http://dev.mysql.com/doc/refman/5.1/en/date-and-time-functions.html#function_week.
		return ((current_thd->variables.default_week_format % 2) == 0) ? 6 : 0;
	} else {
		Item& arg = *args[fdowArg_];
		if (arg.is_null()) {
			return -1;
		} else {
			return static_cast<int>(arg.val_int());
		}
	}
}

longlong Item_func_tadjust::getSeconds(MYSQL_TIME* t) {
	assert(fixed == 1);
	if (get_arg0_date(t, 0)) {
		null_value = args[0]->null_value;
		return 0;
	}
	if (args[0]->type() == FIELD_ITEM) {
		Field* field = static_cast<Item_field*>(args[0])->field;
		if (field->type() == MYSQL_TYPE_TIMESTAMP) {
			my_timeval	tm;
			int			warning = 0;
			if ( !static_cast<Field_timestamp*>(field)->get_timestamp(&tm, &warning) ) {
				return tm.m_tv_sec;
			}
		}
	}
	bool dummy;
	return static_cast<longlong>(current_thd->time_zone()->TIME_to_gmt_sec(t, &dummy));
}

longlong Item_func_tadjust::val_int() {
	assert(fixed == 1);
	const int period = getN();
	const int fdow = getFdow();
	if (period <= 0 || fdow < 0) {
		null_value = 1;
		return 0;
	}
	MYSQL_TIME t;
	longlong seconds = getSeconds(&t);
	if (seconds == 0) {
		null_value = 1;
		return 0;
	}

	// Do not handle microseconds.
	t.second_part = 0;

	// Adjust the timestamp to the given period.
	unsigned int p = static_cast<unsigned int>(period);
	unsigned int delta = 0;
	switch (intervalType_) {
		case INTERVAL_YEAR:
			t.year -= (t.year % p);
			t.month = 1;
			t.day = 1;
			t.hour = 0;
			t.minute = 0;
			t.second = 0;
			break;
		case INTERVAL_QUARTER:
			p *= 3;
			[[fallthrough]];
		case INTERVAL_MONTH:
			if (p > 12 || (12 % p) != 0) {
				null_value = 1;
				return 0;
			}
			t.month -= ((t.month - 1) % p);
			t.day = 1;
			t.hour = 0;
			t.minute = 0;
			t.second = 0;
			break;
		case INTERVAL_WEEK: {
			t.hour = 0;
			t.minute = 0;
			t.second = 0;
			long nday = calc_daynr(t.year, t.month, t.day);
			int weekDay = (nday - 2 - fdow) % 7;
			if (weekDay != 0) {
				get_date_from_daynr(nday - weekDay, &t.year, &t.month, &t.day);
			}
			break;
		}
		case INTERVAL_DAY: {
			t.hour = 0;
			t.minute = 0;
			t.second = 0;
			long nday = calc_daynr(t.year, t.month, t.day);
			nday = nday - nday%p;
			get_date_from_daynr(nday, &t.year, &t.month, &t.day);
			break;
	    }
		case INTERVAL_HOUR:
			if (p >= 24 || (24 % p) != 0) {
				null_value = 1;
				return 0;
			}
			t.hour -= (t.hour % p);
			t.minute = 0;
			t.second = 0;
			delta = p * 3600;
			break;
		case INTERVAL_MINUTE:
			if (p >= 60 || (60 % p) != 0) {
				null_value = 1;
				return 0;
			}
			t.minute -= (t.minute % p);
			t.second = 0;
			delta = p * 60;
			break;
		case INTERVAL_SECOND:
			if (p >= 60 || (60 % p) != 0) {
				null_value = 1;
				return 0;
			}
			t.second -= (t.second % p);
			delta = p;
			break;
		default:
			null_value = 1;
			return 0;
	}
	bool dummy;
	longlong adjusted = static_cast<longlong>(current_thd->time_zone()->TIME_to_gmt_sec(&t, &dummy));
	if (delta > 0) {
		longlong gap = seconds - adjusted;
		if (gap > 0 && gap > delta) {
			seconds = adjusted + delta;
		} else if (gap < 0 && -gap > delta) {
			seconds = adjusted - delta;
		} else {
			seconds = adjusted;
		}
	} else {
		seconds = adjusted;
	}
	if (seconds == 0) {
		null_value = 1;
		return 0;
	} else {
		null_value = 0;
		return seconds;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Function TNEXT
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool Item_func_tnext::resolve_type(THD *thd) {
	if (Item_int_func::resolve_type(thd)) return true;
	set_nullable(true);
	max_length = MAX_BIGINT_WIDTH + 1;
	return false;
}

longlong Item_func_tnext::val_int() {
	assert(fixed == 1);
	if (args[0]->is_null() || args[1]->is_null()) {
		null_value = 1;
		return 0;
	}
	longlong seconds = args[0]->val_int();
	if (seconds == 0) {
		null_value = 1;
		return 0;
	}
	longlong n = args[1]->val_int();
	if (n == 0) {
		null_value = 0;
		return seconds;
	}

	// Compute the added interval.
	Interval interval;
	memset(&interval, 0, sizeof(interval));
	if (n < 0) {
		interval.neg = true;
		n = -n;
	}
	switch (intervalType_) {
		case INTERVAL_YEAR:
			interval.year = static_cast<ulong>(n);
			break;
		case INTERVAL_QUARTER:
			n *= 3;
			 [[fallthrough]];
		case INTERVAL_MONTH:
			interval.month = static_cast<ulong>(n);
			break;
		case INTERVAL_WEEK:
			n *= 7;
			[[fallthrough]];
		case INTERVAL_DAY:
			interval.day = static_cast<ulong>(n);
			break;
		case INTERVAL_HOUR:
			interval.hour = static_cast<ulong>(n);
			break;
		case INTERVAL_MINUTE:
			interval.minute = static_cast<ulonglong>(n);
			break;
		case INTERVAL_SECOND:
			interval.second = static_cast<ulonglong>(n);
			break;
		default:
			null_value = 1;
			return 0;
	}
	MYSQL_TIME t;
	current_thd->time_zone()->gmt_sec_to_TIME(&t, static_cast<my_time_t>(seconds));
	if (date_add_interval_with_warn(current_thd, &t, intervalType_, interval)) {
		null_value = 1;
		return 0;
	}
	bool dummy;
	seconds = static_cast<longlong>(current_thd->time_zone()->TIME_to_gmt_sec(&t, &dummy));
	null_value = 0;
	return seconds;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Function GETNEWEST
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool Item_func_getnewest::resolve_type(THD *thd) {
	if (Item_int_func::resolve_type(thd)) return true;
	set_nullable(true);
	max_length = MAX_BIGINT_WIDTH + 1;
	return false;
}

longlong Item_func_getnewest::val_int() {
	Item& arg0 = *args[0];
	if (arg0.is_null()) {
		null_value = 1;
		return 0;
	}
	String tmp0;
	String* s0 = arg0.val_str(&tmp0);
	if (s0 == 0 ) {
		null_value = 1;
		return 0;
	}

	Item& arg1 = *args[1];
	if (arg1.is_null()) {
		null_value = 1;
		return 0;
	}
	String tmp1;
	String* s1 = arg1.val_str(&tmp1);
	if (s1 == 0 ) {
		null_value = 1;
		return 0;
	}

	using namespace Sparrow;

	Str		databaseName( s0->c_ptr() );
	Str		tableName( s1->c_ptr() );

	uint64_t newest = 0;
	try {
		MasterGuard		master = InternalApi::get(databaseName.c_str(), tableName.c_str(), false, false, 0);
		ReadGuard masterGuard(master->getLock());
		newest = master->getNewest() / 1000;

	} catch(const SparrowException& e) {
		null_value = 1;
		spw_print_error("Sparrow: Cannot get timestamp of newest data for %s.%s: %s", databaseName.c_str(), tableName.c_str(), e.getText());
		return HA_ERR_INTERNAL_ERROR;
	}

	return newest;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Function GETOLDEST
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool Item_func_getoldest::resolve_type(THD *thd) {
	if (Item_int_func::resolve_type(thd)) return true;
	set_nullable(true);
	max_length = MAX_BIGINT_WIDTH + 1;
	return false;
}


longlong Item_func_getoldest::val_int() {
	Item& arg0 = *args[0];
	if (arg0.is_null()) {
		null_value = 1;
		return 0;
	}
	String tmp0;
	String* s0 = arg0.val_str(&tmp0);
	if (s0 == 0 ) {
		null_value = 1;
		return 0;
	}

	Item& arg1 = *args[1];
	if (arg1.is_null()) {
		null_value = 1;
		return 0;
	}
	String tmp1;
	String* s1 = arg1.val_str(&tmp1);
	if (s1 == 0 ) {
		null_value = 1;
		return 0;
	}

	using namespace Sparrow;

	Str		databaseName( s0->c_ptr() );
	Str		tableName( s1->c_ptr() );

	uint64_t oldest = 0;
	try {
		MasterGuard		master = InternalApi::get(databaseName.c_str(), tableName.c_str(), false, false, 0);
		ReadGuard masterGuard(master->getLock());
		oldest = master->getOldest() / 1000;

	} catch(const SparrowException& e) {
		null_value = 1;
		spw_print_error("Sparrow: Cannot get timestamp of oldest data for %s.%s: %s", databaseName.c_str(), tableName.c_str(), e.getText());
		return HA_ERR_INTERNAL_ERROR;
	}

	return oldest;
}
