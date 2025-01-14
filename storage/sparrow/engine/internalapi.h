/*
	Internal API.
*/

#ifndef _engine_internalapi_h_
#define _engine_internalapi_h_

#include "types.h"
#include "master.h"

struct MYSQL;
struct MYSQL_RES;

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MySQLGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

class MySQLGuard {
private:

	MYSQL* mysql_;
	MYSQL_RES* result_;

public:

	MySQLGuard(const char* username, const char* password) _THROW_(SparrowException);

	void execute(const char* stmt) _THROW_(SparrowException);

	MYSQL_RES* get() {
		return result_;
	}

	~MySQLGuard();

	void clear();

	static void check(MYSQL* mysql, const char* stmt) _THROW_(SparrowException);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Engines
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Engines {
private:

	uint32_t bits_;

public:

	Engines(uint32_t bits) : bits_(bits) {
	}

	const char* getName(int i) const {
		const char* engines[] = { "sparrow", "myisam", "innodb" };
		return engines[i];
	}

	// Table suffix.
	const char* getSuffix(int i) const {
		const char* suffix[] = { "", "_myisam", "_innodb" };
		return suffix[i];
	}

	int getCount() const {
		return 3;
	}

	bool isEnabled(int i) const {
		return (bits_ & (1 << i)) != 0;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// InternalApi
//////////////////////////////////////////////////////////////////////////////////////////////////////

class InternalApi {
private:

	// Static hash table of directories.
	static SYSpHash<Master> hash_;
	static Lock hashLock_;

	// Is a flush on going?
	static volatile uint32_t flushOnGoing_;

private:

	static bool update(const char* username, const char* password, const char* database, const char* table,
		const ColumnExs& columns, const Indexes& indexes, const ForeignKeys& foreignKeys, const DnsConfiguration& dnsConfiguration,
		const uint32_t aggregationPeriod, const uint64_t defaultWhere, const uint64_t stringOptimization, 
		const uint64_t maxLifetime, const uint64_t coalescingPeriod) _THROW_(SparrowException);
	static void check(const ColumnExs& columns, const DnsConfiguration& dnsConfiguration) _THROW_(SparrowException);

	static void reportAlterStatus(PrintBuffer& buffer, const SortedMasters& masters);
	static void reportCoalescingStatus(PrintBuffer& buffer, const SortedMasters& masters);

public:

	// Setup.
	static void setup() _THROW_(SparrowException);

	// Initializes table.
	static void init(const char* username, const char* password, const char* database, const char* table,
		const ColumnExs& columns, const Indexes& indexes, const ForeignKeys& foreignKeys, const DnsConfiguration& dnsConfiguration,
		const uint32_t aggregationPeriod, const uint64_t defaultWhere, const uint64_t stringOptimization,
		const uint64_t maxLifetime, const uint64_t coalescingPeriod) _THROW_(SparrowException);

	// Writes data.
	static void write(const char* database, const char* table, ByteBuffer& buffer, const uint32_t rows) _THROW_(SparrowException);

	// Writes data on a selection of columns.
	static void write(const char* database, const char* table, const Names& columns, ByteBuffer& buffer, const uint32_t rows) _THROW_(SparrowException);

	// Removes some partitions.
	static void removePartitions(const char* database, const char* table, const TimePeriod& period) _THROW_(SparrowException);

	// Renames a table
	static void rename(const char* database, const char* table,
		const char* newDatabase, const char* newTable) _THROW_(SparrowException);

	// Gets a master file.
	static MasterKeepAlive get(const char* database, const char* table,
		const bool create, const bool remove, TABLE_SHARE* s) _THROW_(SparrowException);

	// Gets all master files.
	static Masters getAll();

	// Flushes the transient partition of all master files.
	static void flushAll(const bool shutdown);

	// Stops the coalescing 
	static void StopCoalescingTasks(const char* schema=NULL);

	// Reporting.
	static void printGrid(PrintBuffer& buffer, const SYSvector<Str>& headers, SYSslist<Str>& values, const uint32_t totalValues);
	static void report(PrintBuffer& buffer) _THROW_(SparrowException);

	// Debugging.
#ifndef NDEBUG
	static bool hashContains(Master* master) {
		return hash_.contains(master);
	}
#endif
};

}

#endif /* #ifndef _engine_internalapi_h_ */
