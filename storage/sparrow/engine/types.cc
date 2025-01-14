/*
	Engine types.
*/

#include "types.h"
#include "fileutil.h"
#include "persistent.h"
#include "../handler/hasparrow.h"

#include "../engine/log.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// BlockCacheHint
//////////////////////////////////////////////////////////////////////////////////////////////////////

const BlockCacheHint BlockCacheHint::smallForward0_(BlockCacheHint::SMALL, BlockCacheHint::FORWARD, 0);
const BlockCacheHint BlockCacheHint::largeForward0_(BlockCacheHint::LARGE, BlockCacheHint::FORWARD, 0);
const BlockCacheHint BlockCacheHint::largeAround0_(BlockCacheHint::LARGE, BlockCacheHint::AROUND, 0);
const BlockCacheHint BlockCacheHint::largeForward1_(BlockCacheHint::LARGE, BlockCacheHint::FORWARD, 1);
const BlockCacheHint BlockCacheHint::largeBackward1_(BlockCacheHint::LARGE, BlockCacheHint::BACKWARD, 1);
const BlockCacheHint BlockCacheHint::largeAround1_(BlockCacheHint::LARGE, BlockCacheHint::AROUND, 1);
const BlockCacheHint BlockCacheHint::smallAround2_(BlockCacheHint::SMALL, BlockCacheHint::AROUND, 2);
const BlockCacheHint BlockCacheHint::mediumAround2_(BlockCacheHint::MEDIUM, BlockCacheHint::AROUND, 2);
const BlockCacheHint BlockCacheHint::largeForward2_(BlockCacheHint::LARGE, BlockCacheHint::FORWARD, 2);
const BlockCacheHint BlockCacheHint::largeBackward2_(BlockCacheHint::LARGE, BlockCacheHint::BACKWARD, 2);
const BlockCacheHint BlockCacheHint::smallForward3_(BlockCacheHint::SMALL, BlockCacheHint::FORWARD, 3);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Str
//////////////////////////////////////////////////////////////////////////////////////////////////////

const char* Str::empty_ = "";

// Timestamp is in milliseconds.
// STATIC
Str Str::fromTimestamp(const uint64_t timestamp) {
	const uint32_t milliseconds = timestamp % 1000;
	time_t tt = static_cast<time_t>(timestamp / 1000);
	struct tm t;
	localtime_r(&tt, &t);
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%04d/%02d/%02d %2d:%02d:%02d.%03u", 1900 + t.tm_year, t.tm_mon + 1, t.tm_mday,
		t.tm_hour, t.tm_min, t.tm_sec, milliseconds);
	return Str(buffer);
}

// STATIC
Str Str::fromTimePeriod(const TimePeriod& period) {
	if (period.isVoid()) {
		return Str("]void[");
	}
	const uint64_t* low = period.getLow();
	Str sLow = low == 0 ? Str("-inf") : Str::fromTimestamp(*low);
	const uint64_t* up = period.getUp();
	Str sUp = up == 0 ? Str("+inf") : Str::fromTimestamp(*up);
	char buffer[128];
	snprintf(buffer, sizeof(buffer), "%s%s, %s%s", period.isLowerIncluded() ? "[" : "]", sLow.c_str(),
		sUp.c_str(), period.isUpperIncluded() ? "]" : "[");
	return Str(buffer);
}

// Duration is in milliseconds.
// STATIC
Str Str::fromDuration(const uint64_t duration) {
	char buffer[128];
	const uint32_t milliseconds = static_cast<uint32_t>(duration % 1000);
	if (duration < 1000) {
		snprintf(buffer, sizeof(buffer), "%ums", milliseconds);
	} else if (duration < 60000) {
		if (milliseconds == 0) {
			snprintf(buffer, sizeof(buffer), "%us", static_cast<uint>(duration / 1000));
		} else {
			snprintf(buffer, sizeof(buffer), "%us%03ums", static_cast<uint>(duration / 1000), milliseconds);
		}
	} else if (duration < 3600000) {
		snprintf(buffer, sizeof(buffer), "%um", static_cast<uint>(duration / 60000));
	} else if (duration < 86400000) {
		const uint minutes =  static_cast<uint>((duration % 3600000) / 60000);
		if (minutes == 0) {
			snprintf(buffer, sizeof(buffer), "%uh", static_cast<uint>(duration / 3600000));
		} else {
			snprintf(buffer, sizeof(buffer), "%uh%um", static_cast<uint>(duration / 3600000), minutes);
		}
	} else {
		const uint hours = static_cast<uint>((duration % 86400000) / 3600000);
		if (hours == 0) {
			snprintf(buffer, sizeof(buffer), "%ud", static_cast<uint>(duration / 86400000));
		} else {
			snprintf(buffer, sizeof(buffer), "%ud%uh", static_cast<uint>(duration / 86400000), hours);
		}
	}
	return Str(buffer);
}

// Size is in bytes.
// STATIC
Str Str::fromSize(const uint64_t size) {
	char buffer[128];
	if (size < static_cast<uint64_t>(1024)) {
		snprintf(buffer, sizeof(buffer), "%llu", static_cast<ulonglong>(size));
	} else if (size < static_cast<uint64_t>(1024) * 1024) {
		snprintf(buffer, sizeof(buffer), "%llu KB", static_cast<ulonglong>(size / 1024));
	} else if (size < static_cast<uint64_t>(1024) * 1024 * 1024) {
		snprintf(buffer, sizeof(buffer), "%llu MB", static_cast<ulonglong>(size / 1024 / 1024));
	} else if (size < static_cast<uint64_t>(1024) * 1024 * 1024 * 1024) {
		snprintf(buffer, sizeof(buffer), "%.1f GB", static_cast<double>(size) / 1024 / 1024 / 1024);
	} else {
		snprintf(buffer, sizeof(buffer), "%.1f TB", static_cast<double>(size) / 1024 / 1024 / 1024 / 1024);
	}
	return Str(buffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SparrowException
//////////////////////////////////////////////////////////////////////////////////////////////////////

SparrowException::SparrowException(const char* text, const bool logged /* = true */, unsigned int err_code /*=0xFFFFFFFF*/ ) : logged_(logged), err_code_(err_code) {
	strncpy(buffer_, text, sizeof(buffer_)-1);
	buffer_[sizeof(buffer_)-1] = '\0';
}

// STATIC
SparrowException SparrowException::create(const bool addError, const char* format, ...)
{
	char buffer[1024];
	va_list varargs;
	va_start(varargs, format);
	vsnprintf(buffer, sizeof(buffer), format, varargs);
	va_end(varargs);
	if (addError) {
		char error[1024];
#ifdef _WIN32
		LPSTR serror = error;
		if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, GetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), serror, sizeof(error), 0) == 0) {
			snprintf(error, sizeof(error), "error %d", GetLastError());
		} else {	// Windows adds a nasty new line char...
			size_t l = strlen(error) - 1;
			while (error[l] == '\n' || error[l] == '\r') {
				error[l--] = 0;
			}
		}
#else
		snprintf(error, sizeof(error), "%s", strerror(errno));
#endif
		char result[2050];
		snprintf(result, sizeof(result), "%s (%s)", buffer, error);
		return SparrowException(result);
	} else {
		return SparrowException(buffer);
	}
}

void SparrowException::toLog() const {
	if (logged_) {
		spw_print_error("Sparrow: %s", getText());
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CodeStateGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef NDEBUG
Lock CodeStateGuard::lock_(true, "CodeStateGuard::lock_");
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ColumnEx
//////////////////////////////////////////////////////////////////////////////////////////////////////

// STATIC
const char* ColumnEx::getSqlType(const ColumnType type) {
	switch(type) {
		case COL_BLOB: return "VARBINARY";
		case COL_BYTE: return "TINYINT";
		case COL_DOUBLE: return "DOUBLE";
		case COL_INT: return "INT";
		case COL_LONG: return "BIGINT";
		case COL_STRING: return "VARCHAR";
		case COL_TIMESTAMP: return "TIMESTAMP";
		case COL_SHORT: return "SMALLINT";
		default:
			return "";
	}
}

Str ColumnEx::getDefinition() const {
	char tmp[1024];
	char* buffer = tmp;
	const ColumnType type = getType();
	buffer += sprintf(buffer, "`%s` %s", getName().c_str(), getSqlType(type));
	if ( type == COL_TIMESTAMP ) {
		// For timestamps, the decimal precision (0..6) is stored in the info_ field. But older client applications may still use the string size.
		//	Therefore, check both.
		uint	decimals = getStringSize() != 0 ? getStringSize() : getInfo();
		buffer += sprintf(buffer, "(%u)", decimals);	
	}
	if (type == COL_BLOB || type == COL_STRING) {
		buffer += sprintf(buffer, "(%u)", getStringSize());
	}
	if (isFlagSet(COL_UNSIGNED)) {
		buffer += sprintf(buffer, " UNSIGNED");
	}
	if (isFlagSet(COL_NULLABLE)) {
		buffer += sprintf(buffer, " NULL");
	} else {
		buffer += sprintf(buffer, " NOT NULL");
	}
	const Str& defaultValue = getDefaultValue();
	if (defaultValue.length() != 0) {
		buffer += sprintf(buffer, " DEFAULT '%s'", defaultValue.c_str());
	}
	if (isFlagSet(COL_AUTO_INC)) {
		buffer += sprintf(buffer, " AUTO_INCREMENT");
	}
	return Str(tmp);
}

}
