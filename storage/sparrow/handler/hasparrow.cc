/*
	Sparrow handler.
*/

#include "hasparrow.h"

#include "../engine/internalapi.h"
#include "../engine/persistent.h"
#include "../engine/scheduler.h"
#include "../engine/listener.h"
#include "../engine/fileutil.h"
#include "../engine/cache.h"
#include "../engine/alter.h"
#include "../engine/purge.h"
#include "../engine/coalescing.h"
#include "../dns/dns.h"

//#include <sql_show.h>
#include "sql/current_thd.h"
#include "sql/sql_show.h"
#include "sql/sql_lex.h"
#include "sql/create_field.h"
#include "sql/mysqld.h"

#include "../engine/log.h"

// #ifdef __GNUC__
// #pragma GCC diagnostic ignored "-Wunused-parameter"
// #endif 


namespace Sparrow {

SparrowStatus SparrowStatus::status_;
bool	SparrowHandler::initialized_ = false;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SparrowHandler
//////////////////////////////////////////////////////////////////////////////////////////////////////

static const char* HA_SPARROW_EXTS[]= { 
	"",			// for directories
	".SPM",
	".SPI",
	".SPD",
	".SPS",
	NullS 
};


// STATIC
int SparrowHandler::initialize(void* p) {
	SPARROW_ENTER("SparrowHandler::initialize");

	Lock::initializeStatics();
	RWLock::initializeStatics();
	Cond::initializeStatics();

	// Initialize handlerton.   
	//  handlerton is a singleton structure - one instance per storage engine -
	//  to provide access to storage engine functionality that works on the
    //	"global" level (unlike handler class that works on a per-table basis).

	handlerton* hton = static_cast<handlerton*>(p);
	hton->state = SHOW_OPTION_YES;
	hton->db_type = static_cast<legacy_db_type>(DB_TYPE_SPARROW);
	hton->create = &SparrowHandler::create;
	hton->close_connection = SparrowHandler::closeConnection;
	hton->drop_database = SparrowHandler::dropDatabase;
	hton->panic = SparrowHandler::panic;
	hton->show_status = SparrowHandler::showStatus;
	hton->flags = HTON_TEMPORARY_NOT_SUPPORTED | HTON_NO_PARTITION;
	// TODO: Review this declaration when working on the schema alteration. 
	//hton->alter_table_flags = SparrowHandler::alterTableFlags;
	hton->file_extensions = HA_SPARROW_EXTS;

	try {
		Master::initialize();

		// Initialize socket util.
		SocketUtil::initialize();

		// Initialize file util.
		FileUtil::initialize();

		// Initialize caches.
		FileCache::initialize();
		BlockCache::initialize();

		// Initialize worker and writer threads.
		Worker::initialize();
		Flush::initialize();
		Writer::initialize();
		DnsWorker::initialize();
		ApiWorker::initialize();

		// Initialize coalescing threads.
		CoalescingWorker::initialize();

		// Start misc other background threads
		start_slave_threads();

		// Release thread local storage.
		IOContext::destroy();
	} catch(const SparrowException& e) {
		e.toLog();
		return ER_UNKNOWN_ERROR;
	}
	return 0;
}


int SparrowHandler::start_slave_threads() {	
	SPARROW_ENTER("SparrowHandler::start_slave_threads");
	try {
		// Initialize scheduler.
		Scheduler::initialize(); // THD thread

		// Initialize DNS service.
		Dns::initialize();		// THD thread

		// Initialize purge thread.
		Purge::initialize();	// THD thread

		// Find all master files.
		InternalApi::setup();

		// Initialize listener.
		Listener::initialize();	// THD thread

		initialized_ = true;

	} catch(const SparrowException& e) {
		e.toLog();
		return ER_UNKNOWN_ERROR;
	}
	return 0;
}


// STATIC
int SparrowHandler::stop_slave_threads() {
	if ( !initialized_ ) 
		return 0;

	SPARROW_ENTER("SparrowHandler::stop_slave_threads");
	spw_print_information("Shutting down Sparrow, phase 1...");

	uint64_t t = my_micro_time();

	InternalApi::StopCoalescingTasks();

	// Shutdown listener.
	Listener::shutdown();
	uint64_t now = my_micro_time();
	Str duration = Str::fromDuration((now - t) / 1000);
	spw_print_information("Shut down listener thread in %s", duration.c_str());
	t = now;

	// Force flush of transient partitions.
	if (!sparrow_quick_shutdown) {
		InternalApi::flushAll(true);
		now = my_micro_time();
		duration = Str::fromDuration((now - t) / 1000);
		spw_print_information("Flushed partitions in %s", duration.c_str());
		t = now;
	}
	Flush::shutdown();
	now = my_micro_time();
	duration = Str::fromDuration((now - t) / 1000);
	spw_print_information("Shut down flush threads in %s", duration.c_str());
	t = now;

	// Shutdown scheduler.
	Scheduler::shutdown();
	now = my_micro_time();
	duration = Str::fromDuration((now - t) / 1000);
	spw_print_information("Shut down scheduler thread in %s", duration.c_str());
	t = now;

	// Shutdown DNS service.
	Dns::shutdown();
	now = my_micro_time();
	duration = Str::fromDuration((now - t) / 1000);
	spw_print_information("Shut down DNS threads in %s", duration.c_str());
	t = now;

	DnsWorker::shutdown();
	now = my_micro_time();
	duration = Str::fromDuration((now - t) / 1000);
	spw_print_information("Shut down DNS worker threads in %s", duration.c_str());
	t = now;

	AlterWorker::shutdown();
	now = my_micro_time();
	duration = Str::fromDuration((now - t) / 1000);
	spw_print_information("Shut down alter worker threads in %s", duration.c_str());
	t = now;

	ApiWorker::shutdown();
	now = my_micro_time();
	duration = Str::fromDuration((now - t) / 1000);
	spw_print_information("Shut down API worker threads in %s", duration.c_str());
	t = now;

	// Shutdown coalescing threads.
	CoalescingWorker::shutdown();
	now = my_micro_time();
	duration = Str::fromDuration((now - t) / 1000);
	spw_print_information("Shut down coalescing threads in %s", duration.c_str());
	t = now;

	// Shutdown purge thread.
	Purge::shutdown();
	now = my_micro_time();
	duration = Str::fromDuration((now - t) / 1000);
	spw_print_information("Shut down purge thread in %s", duration.c_str());
	t = now;

	// Shutdown worker and writer threads.
	Worker::shutdown();
	now = my_micro_time();
	duration = Str::fromDuration((now - t) / 1000);
	spw_print_information("Shut down worker threads in %s", duration.c_str());
	t = now;

	Writer::shutdown();
	now = my_micro_time();
	duration = Str::fromDuration((now - t) / 1000);
	spw_print_information("Shut down writer threads in %s", duration.c_str());

	return 0;
}


// STATIC
int SparrowHandler::deinitialize(void* p) {
	SPARROW_ENTER("SparrowHandler::deinitialize");
	spw_print_information("Shutting down Sparrow, phase 2...");

#ifndef NDEBUG
	uint64_t t = my_micro_time();
#endif

	Str duration;

	// Close opened files.
	FileCache::get().clear();
#ifndef NDEBUG
	uint64_t now = my_micro_time();
	duration = Str::fromDuration((now - t) / 1000);
	DBUG_PRINT("sparrow_handler", ("Clear file cache in %s", duration.c_str()));
	t = now;
#endif

	Lock::deinitializeStatics();
	RWLock::deinitializeStatics();
	Cond::deinitializeStatics();

	spw_print_information("Sparrow shutdown complete.");
	return 0;
}

// STATIC
// Creates handler object for the table in the storage engine.
handler* SparrowHandler::create(handlerton* hton, TABLE_SHARE* table, bool partitioned, MEM_ROOT* mem_root) {
	SPARROW_ENTER("SparrowHandler::create");
	return new (mem_root) SparrowHandler(hton, table);
}

// STATIC
int SparrowHandler::closeConnection(handlerton* hton, THD* thd) {
	SPARROW_ENTER("SparrowHandler::closeConnection");
	IOContext::destroy();
	return 0;
}

// STATIC
int SparrowHandler::panic(handlerton* hton, enum ha_panic_function flag) {
	SPARROW_ENTER("SparrowHandler::panic");
	// Nothing to do.
	return 0;
}

// STATIC
bool SparrowHandler::showStatus(handlerton* hton, THD* thd, stat_print_fn* stat_print, enum ha_stat_type stat_type) {
	SPARROW_ENTER("SparrowHandler::showStatus");
	PrintBuffer buffer;
	buffer << Str::fromTimestamp(Scheduler::now()) << " - Sparrow Engine Status\n\n";
	buffer << "Uptime: " << Str::fromDuration(Scheduler::uptime()) << "\n";
	FileUtil::report(buffer);
	InternalApi::report(buffer);
	return stat_print(thd, SPARROW_ENGINE_NAME,
		static_cast<uint>(strlen(SPARROW_ENGINE_NAME)), "", 0,
		reinterpret_cast<const char*>(buffer.getData()), static_cast<uint>(buffer.position()));
}

// STATIC
void SparrowHandler::dropDatabase(handlerton* hton, char* path) {
	SPARROW_ENTER("SparrowHandler::dropDatabase");
}

SparrowHandler::SparrowHandler(handlerton* hton, TABLE_SHARE* table)
	: handler(hton, table) {
	SPARROW_ENTER("SparrowHandler::SparrowHandler");
	share_ = 0;
}

SparrowHandler::~SparrowHandler() {
	close();
}

const char* SparrowHandler::table_type() const {
	SPARROW_ENTER("SparrowHandler::table_type");
	return SPARROW_ENGINE_NAME;
}

//const char** SparrowHandler::bas_ext() const {
//	SPARROW_ENTER("SparrowHandler::bas_ext");
//	return HA_SPARROW_EXTS;
//}

/*int SparrowHandler::backup(THD* thd, HA_CHECK_OPT* checkOpt) {
	SPARROW_ENTER("SparrowHandler::backup");
	return HA_ADMIN_NOT_IMPLEMENTED;
}

int SparrowHandler::restore(THD* thd, HA_CHECK_OPT* checkOpt) {
	SPARROW_ENTER("SparrowHandler::restore");
	return HA_ADMIN_NOT_IMPLEMENTED;
}*/

handler::Table_flags SparrowHandler::table_flags() const {
	SPARROW_ENTER("SparrowHandler::table_flags");
	return HA_NO_TRANSACTIONS
		| HA_PARTIAL_COLUMN_READ
		| HA_STATS_RECORDS_IS_EXACT
		| HA_COUNT_ROWS_INSTANT
		| HA_NO_BLOBS
		| HA_FILE_BASED
		| HA_NULL_IN_KEY
		| HA_AUTO_PART_KEY
		| HA_CAN_SQL_HANDLER
		//| HA_REC_NOT_IN_SEQ		// Seems to have been removed. See sql/handler.h:222
		| HA_ANY_INDEX_MAY_BE_UNIQUE
		| HA_BINLOG_FLAGS
		| HA_NO_COPY_ON_ALTER
		| HA_CAN_REPAIR;
}

// STATIC
uint SparrowHandler::alterTableFlags(uint flags) {
	SPARROW_ENTER("SparrowHandler::alterTableFlags");
	return 0;
}

/*const char* SparrowHandler::index_type(uint inx) {
	SPARROW_ENTER("SparrowHandler::index_type");
	return "Default";
}*/

enum ha_key_alg SparrowHandler::get_default_index_algorithm() const
{
	SPARROW_ENTER("SparrowHandler::get_default_index_algorithm");
	return HA_KEY_ALG_SE_SPECIFIC;
}
bool SparrowHandler::is_index_algorithm_supported(enum ha_key_alg key_alg) const {
	return key_alg == HA_KEY_ALG_SE_SPECIFIC;
}




ulong SparrowHandler::index_flags(uint inx, uint part, bool all_parts) const {
	SPARROW_ENTER("SparrowHandler::index_flags");
	return HA_READ_NEXT
		| HA_READ_PREV
		| HA_KEYREAD_ONLY
		| HA_READ_ORDER
		| HA_READ_RANGE;
}

int SparrowHandler::info(uint flag) {
	SPARROW_ENTER("SparrowHandler::info");
	context_.getStats(getStats(), flag);
	return 0;
}

int SparrowHandler::analyze(THD* thd, HA_CHECK_OPT* checkOpt) {
	SPARROW_ENTER("SparrowHandler::analyze");
	return HA_ADMIN_NOT_IMPLEMENTED;
}

int SparrowHandler::check(THD* thd, HA_CHECK_OPT* checkOpt) {
	SPARROW_ENTER("SparrowHandler::check");
	return HA_ADMIN_NOT_IMPLEMENTED;
}

int SparrowHandler::optimize(THD* thd, HA_CHECK_OPT* checkOpt) {
	SPARROW_ENTER("SparrowHandler::optimize");
	return HA_ADMIN_NOT_IMPLEMENTED;
}

int SparrowHandler::repair(THD* thd, HA_CHECK_OPT* checkOpt) {
	SPARROW_ENTER("SparrowHandler::repair");
	Master& master = share_->getMaster();
	master.repair();
	return 0;
}


// Analyze the modifications to the columns. Try to create columns with the changes.
//	If there are incoherences or any issue during the creation, an exception will be thrown and 
//	caught by mysql alteration engine.
int SparrowHandler::check_alterations(Alter_info* info) {
	SPARROW_ENTER("SparrowHandler::check_alterations");
	const uint alterFlags = info->flags;
	ReadGuard guard(share_->getMaster().getLock());
	try {
		if (alterFlags & Alter_info::ALTER_ADD_COLUMN) {
			List_iterator<Create_field> iterator(info->create_list);
			Create_field* field;
			while ((field = iterator++) != 0) {
				if (field->after == first_keyword) {
					throw SparrowException::create(false, "cannot add column `%s` at first position because table timestamp must remain the first column",
						field->field_name);
				}
				createColumn(*field);	// Check column creation.
			}
		}
		if (alterFlags & Alter_info::ALTER_DROP_COLUMN) {
			for (const auto drop : info->drop_list) {
				const Str name(drop->name, false);
				const uint32_t pos = share_->getMaster().getColumn(name);
				if (pos == 0) {
					throw SparrowException::create(false, "cannot drop timestamp column `%s`", name.c_str());
				}
				const Indexes& indexes = share_->getMaster().getIndexes();
				for (uint32_t i = 0; i < indexes.length(); ++i) {
					const Index& index = indexes[i];
					if (!index.isDropped() && index.getColumnIds().contains(pos)) {
						throw SparrowException::create(false, "cannot drop column `%s` because it is used by index `%s`", name.c_str(), index.getName().c_str());
					}
				}
			}
		}
		if (alterFlags & Alter_info::ALTER_CHANGE_COLUMN) {
			List_iterator<Create_field> iterator(info->create_list);
			Create_field* field;
			while ((field = iterator++) != 0) {
				createColumn(*field);	// Check column creation.
			}
		}
		return 0;
	} catch(const SparrowException& e) {
		Str s("this operation: ");
		s += Str(e.getText());
		my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), s.c_str());
		return 1;
	}
}

// test_if_locked is a list of flags; see include/my_base.h:42
// mode indicates how to open the file. Example: O_RDONLY, O_WRONLY, O_RDWR, O_APPEND, ... See examples in other storage engines.
//int SparrowHandler::open(const char* name, int mode, uint options) {
int SparrowHandler::open(const char *name, int mode, uint test_if_locked, const dd::Table *table_def) {
	SPARROW_ENTER("SparrowHandler::open");
	const Str databaseName = FileUtil::getDatabaseName(name);
	const Str tableName = FileUtil::getTableName(name);
	DBUG_PRINT("sparrow_handler", ("Open table %s.%s with mode %d", databaseName.c_str(), tableName.c_str(), mode));
	try {
		share_ = TableShare::acquire(databaseName, tableName, table, &lockData_);
		context_.initialize(table, share_);
		return 0;
	} catch(const SparrowException& e) {
		spw_print_error("Sparrow: Cannot open table %s.%s: %s", databaseName.c_str(), tableName.c_str(), e.getText());
		return HA_ERR_INTERNAL_ERROR;
	}
}

handler* SparrowHandler::clone(const char* name, MEM_ROOT* mem_root) {
	// Do the same as the default implementation, but clone also the current context.
	SparrowHandler* handler = static_cast<SparrowHandler*>(handler::clone(name, mem_root));
	handler->context_.clone(context_);
	return handler;
}

int SparrowHandler::close() {
	SPARROW_ENTER("SparrowHandler::close");
	if (share_ != 0) {
		DBUG_PRINT("sparrow_handler", ("Close table %s.%s", share_->getDatabaseName().c_str(), share_->getTableName().c_str()));
		TableShare::release(share_);
		share_ = 0;
	}
	context_.reset();
	return 0;
}

int SparrowHandler::delete_table(const char *name, const dd::Table *table_def) {
	SPARROW_ENTER("SparrowHandler::delete_table");
	DBUG_PRINT("sparrow_handler", ("Delete table %s", name));

	const Str database = FileUtil::getDatabaseName(name);
	const Str table = FileUtil::getTableName(name);
	try {
		// Get the master file and remove it from the hash so it is no longer accessible from the API.
		MasterGuard master = InternalApi::get(database.c_str(), table.c_str(), false, true, 0);
		master->prepareForDeletion();
		Atomic::inc32(&SparrowStatus::get().ddlSerial_);
		return 0;
	} catch(const SparrowException& e) {
		spw_print_error("Sparrow: Cannot delete table %s.%s: %s", database.c_str(), table.c_str(), e.getText());
		return HA_ERR_INTERNAL_ERROR;
	}
}

int SparrowHandler::rename_table(const char *from, const char *to, const dd::Table *from_table_def, dd::Table *to_table_def) {
	SPARROW_ENTER("SparrowHandler::rename_table ");
	DBUG_PRINT("sparrow_handler", ("Rename table %s to %s", from, to));
	const Str fromDatabaseName = FileUtil::getDatabaseName(from);
	const Str fromTableName = FileUtil::getTableName(from);
	const Str toDatabaseName = FileUtil::getDatabaseName(to);
	const Str toTableName = FileUtil::getTableName(to);
	try {
		InternalApi::rename(fromDatabaseName.c_str(), fromTableName.c_str(),
			toDatabaseName.c_str(), toTableName.c_str());
		Atomic::inc32(&SparrowStatus::get().ddlSerial_);
		return 0;
	} catch(const SparrowException& e) {
		spw_print_error("Sparrow: Cannot rename table %s.%s to %s.%s: %s", fromDatabaseName.c_str(), fromTableName.c_str(),
			toDatabaseName.c_str(), toTableName.c_str(), e.getText());
		return HA_ERR_INTERNAL_ERROR;
	}
}

// Create new table.
int SparrowHandler::create(const char *name, TABLE *table, HA_CREATE_INFO *create_info, dd::Table *table_def) {
	SPARROW_ENTER("SparrowHandler::create");
	const Str databaseName = FileUtil::getDatabaseName(name);
	const Str tableName = FileUtil::getTableName(name);
	DBUG_PRINT("sparrow_handler", ("Create table %s.%s", databaseName.c_str(), tableName.c_str()));
	try {
		MasterGuard master = InternalApi::get(databaseName.c_str(), tableName.c_str(), true, false, table->s);
		WriteGuard guard(master->getLock());

		// Get columns.
		restore_record(table, s->default_values);
		BitmapGuard bitmapGuard(table);
		const uint nbColumns = table->s->fields;
		Columns columns(nbColumns);
		for (uint i = 0; i < nbColumns; ++i) {
			const Column column = createColumn(*table->field[i]);

			// First column must be a non-null timestamp.
			if (i == 0 && (column.getType() != COL_TIMESTAMP || column.isFlagSet(COL_NULLABLE))) {
				throw SparrowException::create(false, "first column `%s` must be a non-null timestamp", column.getName().c_str());
			}
			columns.append(column);
		}

		// Get indexes.
		const uint nbIndexes = table->s->keys;
		Indexes indexes(nbIndexes);
		indexes.forceLength(nbIndexes);
		IndexMappings indexMappings(nbIndexes);
		indexMappings.forceLength(nbIndexes);
		for (uint i = 0; i < nbIndexes; ++i) {
			const KEY& index = table->key_info[i];
			indexMappings[i] = i;
			const uint indexColumns = index.user_defined_key_parts;
			ColumnIds columnIds(indexColumns);
			for (uint j = 0; j < indexColumns; ++j) {
				const Field& field = *index.key_part[j].field;
				columnIds.append(static_cast<int>(field.field_index()));
			}
			const bool unique = (index.flags & HA_NOSAME) != 0;
			const Index newIndex(index.name, columnIds, unique);
			const uint32_t check = indexes.index(newIndex);
			if (check != SYS_NPOS) {
				throw SparrowException::create(false, "index %s is a duplicate of index %s", index.name, indexes[check].getName().c_str());
			}
			indexes[i] = newIndex;
		}

		// Get foreign keys.
		THD* thd = ha_thd();
		const String	query(thd->normalized_query());
		const Str sql(query.ptr(), static_cast<int>(query.length()));
		const ForeignKeys foreignKeys = getForeignKeys(sql.c_str(), master->getDatabase(), master->getTable(), table);

		// Setup table definition.
		master->setColumns(columns);
		master->setIndexes(indexes);
		master->setIndexMappings(indexMappings);
		master->setForeignKeys(foreignKeys);
		master->toDisk();
		Atomic::inc32(&SparrowStatus::get().ddlSerial_);
		return 0;
	} catch(const SparrowException& e) {
		spw_print_error("Sparrow: Cannot create table %s.%s: %s", databaseName.c_str(), tableName.c_str(), e.getText());
		return -1;
	}
	return 0;
}

// STATIC
Column SparrowHandler::createColumn(Field& field) _THROW_(SparrowException) {
	// Read default value.
	const enum_field_types fieldType = field.type();
	const uint32_t fieldFlags = field.all_flags();
	CHARSET_INFO* charset = const_cast<CHARSET_INFO*>(field.charset());
	const bool hasDefaultValue = !field.is_null()
		&& fieldType != FIELD_TYPE_BLOB
		&& !(fieldFlags & NO_DEFAULT_VALUE_FLAG)
		&& ((field.auto_flags & Field::NEXT_NUMBER) == 0);
	char tmp[MAX_FIELD_WIDTH];
	Str defaultValue;
	if (hasDefaultValue) {
		String s(tmp, sizeof(tmp), charset);
		field.val_str(&s);
		defaultValue = Str(s.c_ptr(), false);
	}
	uint	decimals = field.decimals();
	return createColumn(field.field_name, fieldType, decimals, fieldFlags, charset, defaultValue);
}

// STATIC
Column SparrowHandler::createColumn(Create_field& field) _THROW_(SparrowException) {
	// Read default value.
	Str defaultValue;
	String tmp;
	String* s = field.constant_default == nullptr ? nullptr : field.constant_default->val_str(&tmp);
	if (s != nullptr) {
		defaultValue = Str(s->c_ptr(), static_cast<int>(s->length()));
	}
	uint	decimals = field.decimals;
	return createColumn(field.field_name, field.sql_type, decimals, field.flags, field.charset == 0 ? &my_charset_utf8mb4_bin : field.charset, defaultValue);
}

// STATIC
Column SparrowHandler::createColumn(const char* name, const enum_field_types fieldType, const uint decimals, const uint32_t fieldFlags,
	const CHARSET_INFO* charset, const Str& defaultValue) _THROW_(SparrowException) {
	assert(charset != 0);
	ColumnType type = COL_UNKNOWN;
	switch (fieldType) {
		case MYSQL_TYPE_TINY: type = COL_BYTE; break;
		case MYSQL_TYPE_SHORT: type = COL_SHORT; break;
		case MYSQL_TYPE_LONG: type = COL_INT; break;
		case MYSQL_TYPE_DOUBLE: type = COL_DOUBLE; break;
		case MYSQL_TYPE_TIMESTAMP2:
		case MYSQL_TYPE_TIMESTAMP: type = COL_TIMESTAMP; break;
		case MYSQL_TYPE_LONGLONG: type = COL_LONG; break;
		case MYSQL_TYPE_VARCHAR: type = charset == &my_charset_bin ? COL_BLOB : COL_STRING; break;
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_FLOAT:
		case MYSQL_TYPE_NULL:
		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_YEAR:
		case MYSQL_TYPE_NEWDATE:
		case MYSQL_TYPE_BIT:
		case MYSQL_TYPE_NEWDECIMAL:
		case MYSQL_TYPE_ENUM:
		case MYSQL_TYPE_SET:
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_STRING:
		case MYSQL_TYPE_GEOMETRY:
		default : break;
	}
	if (type == COL_UNKNOWN) {
		throw SparrowException::create(false, "unknown type %d for column `%s`", static_cast<int>(fieldType), name);
	}

	// Strings must be UTF-8. All UTF-8 collations are allowed, so we check
	// only the character set.
	if (type == COL_STRING && strcmp(charset->csname, "utf8mb4") != 0) {
		throw SparrowException::create(false, "string column `%s` must be UTF-8", name);
	}

	// Blobs must be binary.
	if (type == COL_BLOB && charset != &my_charset_bin) {
		throw SparrowException::create(false, "blob column `%s` must have binary charset", name);
	}

	// Ignore unsigned flag on timestamps (set by MySQL).
	const uint32_t flags = (((fieldFlags & NOT_NULL_FLAG) == 0) ? COL_NULLABLE : 0) 
		| (((fieldFlags & AUTO_INCREMENT_FLAG) != 0) ? COL_AUTO_INC : 0)
		| ((type != COL_TIMESTAMP && (fieldFlags & UNSIGNED_FLAG) != 0) ? COL_UNSIGNED : 0);

	if ((flags & COL_AUTO_INC) != 0) {
		if (type != COL_LONG) {
			throw SparrowException::create(false, "column `%s` cannot be auto incremental; only longs can be", name);
		}
		if ((flags & COL_NULLABLE) != 0) {
			throw SparrowException::create(false, "column `%s` is auto incremental and cannot be nullable", name);
		}
	}
	const uint32_t	info = (type == COL_TIMESTAMP ? decimals : 0);
	return Column(name, type, flags, info, charset->csname, defaultValue);
}

// Utility method to remove comments from the given SQL statement.
// STATIC
Str SparrowHandler::stripComments(const char* sql)
{
	const char*	sptr;
	char* ptr;
	// Unclosed quote character (0 if none).
	char quote = 0;
	char* str = new char[strlen(sql)+1];
	strcpy(str, sql);
	sptr = sql;
	ptr = str;
	for ( ; ; ) {
		if (*sptr == '\0') {
			assert(ptr <= str + strlen(sql));
			*ptr = '\0';
			break;
		}
		bool moveNext = true;
		if (*sptr == quote) {
			// Closing quote character: do not look for starting quote or comments.
			quote = 0;
		} else if (quote) {
			// Within quotes: do not look for starting quotes or comments.
		} else if (*sptr == '"' || *sptr == '`') {
			// Starting quote: remember the quote character.
			quote = *sptr;
		} else if (*sptr == '#' || (sptr[0] == '-' && sptr[1] == '-' && sptr[2] == ' ')) {
			for ( ; ; ) {
				// In Unix a newline is 0x0A while in Windows it is 0x0D followed by 0x0A.
				if (*sptr == (char)0x0A || *sptr == (char)0x0D || *sptr == '\0') {
					moveNext = false;
				} else {
					sptr++;
				}
			}
		} else if (!quote && *sptr == '/' && *(sptr + 1) == '*') {
			for ( ; ; ) {
				if (*sptr == '*' && *(sptr + 1) == '/') {
					sptr += 2;
					moveNext = false;
				} else if (*sptr == '\0') {
					moveNext = false;
				} else {
					sptr++;
				}
			}
		}
		if (moveNext) {
			*ptr = *sptr;
			ptr++;
			sptr++;
		}
	}
	return Str(str);
}

// Gets the foreign keys of a table, given its create SQL statement and its column definitions.
// STATIC
ForeignKeys SparrowHandler::getForeignKeys(const char* sql, const Str& databaseName,
	const Str& tableName, TABLE* table) {
	// Find strings like "`fk col` <col definition> REFERENCES `db`.`tbl` (`col`)".
	ForeignKeys foreignKeys;
	const char* tokens[] = { "REFERENCES", 0, "(", 0, ")" };
	Str identifiers[4];
	const char* s = moveTo(sql, "(");
	if (s == 0) {
		return foreignKeys;
	}
	const char* start = s;
	int n = 0;
	for ( ; ; ) {
		int id = 0;
		for (int i = 0; i < static_cast<int>(sizeof(tokens) / sizeof(tokens[0])); ++i) {
			const char* token = tokens[i];
			bool hasDot = false;
			if (token == 0) {
				s = getIdentifier(s, identifiers[id++], hasDot); 
				if (hasDot) {
					s = getIdentifier(s, identifiers[id++], hasDot); 
				}
			} else {
				s = moveTo(s, token);
			}
			if (s == 0) {
				break;
			}
			if (i == 0) {
				// Move back to get column identifier.
				const char* save = s;
				while (s >= start && *s != ',') {
					s--;
				}
				s = moveTo(s, ",");
				if (s == 0) {
					break;
				}
				s = getIdentifier(s, identifiers[id++], hasDot);
				if (s == 0) {
					break;
				}
				s = save;
			}
		}
		if (id != 3 && id != 4) {
			break;
		}
		for (uint32_t columnId = 0; columnId < table->s->fields; ++columnId) {
			if (my_strcasecmp(&my_charset_latin1, table->field[columnId]->field_name, identifiers[0].c_str()) == 0) {
				const Str& dbName = (id == 3 ? databaseName : identifiers[1]);
				const Str& tblName = (id == 3 ? identifiers[1] : identifiers[2]);
				const Str& colName = (id == 3 ? identifiers[2] : identifiers[3]);
				char buffer[2048];
				snprintf(buffer, sizeof(buffer), "FK_%s_%s_%d", databaseName.c_str(),
					tableName.c_str(), n++);
				foreignKeys.append(ForeignKey(buffer, static_cast<int>(columnId), dbName, tblName, colName));
			}
		}
	}
	return foreignKeys;
}

// Utility method to parse strings: move s to the next occurence of keyword (case-insensitive),
// and skip whitespaces and newlines after. Returns 0 if not found.
// STATIC
const char* SparrowHandler::moveTo(const char* s, const char* keyword) {
	size_t l = strlen(keyword);
	while (*s != 0) {
		if (native_strncasecmp(s, keyword, l) == 0) {
			s += strlen(keyword);
			while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') {
				s++;
			}
			if (*s == 0) {
				return 0;
			}
			return s;
		}
		s++;
	}
	return 0;
}

// Utility method to parse an identifier: id, `id`, x.y or `x`.`y`.
// Returns 0 if we reach the end of string.
// STATIC
const char* SparrowHandler::getIdentifier(const char* s, Str& identifier, bool& hasDot) {
	bool hasQuote = false;
	if (*s == '`' || *s == '"') {
		hasQuote = true;
		s++;
	}
	const char* start = s;
	while (*s != 0 && (isalnum(*s) ||*s == '_')) {
		s++;
	}
	if (*s == 0) {
		return 0;
	}
	identifier = Str(start, static_cast<int>(s - start));
	if (hasQuote) {
		if (*s != '`' && *s != '"') {
			return 0;
		}
		s++;
	}
	if (*s == '.') {
		hasDot = true;
		s++;
		if (*s == 0) {
			return 0;
		}
	} else {
		hasDot = false;
	}
	return s;
}

// Gets create info for foreign keys. This method is called when e.g. calling
// DatabaseMetaData.getImportedKeys() on the JDBC side.
// The resulting string looks like this:
// "CONSTRAINT `<FK name>` FOREIGN KEY (`<col name>`) REFERENCES `<ref table name>` (`<ref col name>`) ON DELETE CASCADE".
// Multiple constraints are separated by commas.
/*char* SparrowHandler::get_foreign_key_create_info() {
	const Master& master = share_->getMaster();
	const ForeignKeys& foreignKeys = master.getForeignKeys();
	size_t length = 65536;
	char* result = 0;
	for ( ; ; ) {
		if (result != 0) {
			my_free(result);
			length *= 2;
		}
		result = static_cast<char*>(my_malloc(length, MYF(0)));
		result[0] = '\0';
		char* s = result;
		bool ok = true;
		for (uint32_t i = 0; i < foreignKeys.length(); ++i) {
			const ForeignKey& foreignKey = foreignKeys[i];
			const char* databaseName = foreignKey.getDatabaseName().length() == 0
				? master.getDatabase().c_str() : foreignKey.getDatabaseName().c_str();
			const char* sparrowColumnName = table->field[foreignKey.getColumnId()]->field_name;
			int l = snprintf(s, length, ",\n  CONSTRAINT `%s` FOREIGN KEY (`%s`) REFERENCES `%s`.`%s` (`%s`) ON DELETE RESTRICT ON UPDATE RESTRICT",
				foreignKey.getName().c_str(), sparrowColumnName, databaseName,
				foreignKey.getTableName().c_str(), foreignKey.getColumnName().c_str());
			if (l < 0) {
				ok = false;
				break;
			}
			s += l;
			length -= l;
		}
		if (ok) {
			break;
		}
	}
	return result;
}

void SparrowHandler::free_foreign_key_create_info(char* str) {
	my_free(str);
}

// Gets the list of foreign keys. This method is called when e.g. issuing a SQL statement
// like "SELECT * FROM information_schema.KEY_COLUMN_USAGE".
int SparrowHandler::get_foreign_key_list(THD* thd, List<FOREIGN_KEY_INFO>* f_key_list) {
	const Master& master = share_->getMaster();
	const ForeignKeys& foreignKeys = master.getForeignKeys();
	for (uint32_t i = 0; i < foreignKeys.length(); ++i) {
		const ForeignKey& foreignKey = foreignKeys[i];
		FOREIGN_KEY_INFO info;
		info.foreign_id = thd_make_lex_string(thd, 0, foreignKey.getName().c_str(),
			static_cast<uint>(foreignKey.getName().length()), 1);
		info.referenced_db = thd_make_lex_string(thd, 0, foreignKey.getDatabaseName().c_str(),
			static_cast<uint>(foreignKey.getDatabaseName().length()), 1);
		info.referenced_table = thd_make_lex_string(thd, 0, foreignKey.getTableName().c_str(),
			static_cast<uint>(foreignKey.getTableName().length()), 1);
		info.referenced_key_name = 0;
		info.referenced_fields.push_back(thd_make_lex_string(thd, 0, foreignKey.getColumnName().c_str(),
			static_cast<uint>(foreignKey.getColumnName().length()), 1));
		const char* columnName = table->field[foreignKey.getColumnId()]->field_name;
		info.foreign_fields.push_back(thd_make_lex_string(thd, 0, columnName,
			static_cast<uint>(strlen(columnName)), 1));
		info.delete_method = thd_make_lex_string(thd, 0, "RESTRICT", (uint)8, 1);
		info.update_method = thd_make_lex_string(thd, 0, "RESTRICT", (uint)8, 1);
		f_key_list->push_back(static_cast<FOREIGN_KEY_INFO*>(thd_memdup(thd, &info, sizeof(info))));
	}
	return 0;
}*/

THR_LOCK_DATA** SparrowHandler::store_lock(THD* thd, THR_LOCK_DATA** to, enum thr_lock_type lockType) {
	SPARROW_ENTER("SparrowHandler::store_lock");
#ifndef NDEBUG
	const char* slock = "unknown";
	switch (lockType) {
		case TL_IGNORE: slock = "TL_IGNORE"; break;
		case TL_UNLOCK: slock = "TL_UNLOCK"; break;
		case TL_READ_DEFAULT: slock = "TL_READ_DEFAULT"; break;
		case TL_READ: slock = "TL_READ"; break;
		case TL_READ_WITH_SHARED_LOCKS: slock = "TL_READ_WITH_SHARED_LOCKS"; break;
		case TL_READ_HIGH_PRIORITY: slock = "TL_READ_HIGH_PRIORITY"; break;
		case TL_READ_NO_INSERT: slock = "TL_READ_NO_INSERT"; break;
		case TL_WRITE_CONCURRENT_DEFAULT: slock = "TL_WRITE_CONCURRENT_DEFAULT"; break;
		case TL_WRITE_ALLOW_WRITE: slock = "TL_WRITE_ALLOW_WRITE"; break;
		case TL_WRITE_CONCURRENT_INSERT: slock = "TL_WRITE_CONCURRENT_INSERT"; break;
		//case TL_WRITE_DELAYED: slock = "TL_WRITE_DELAYED"; break;
		case TL_WRITE_DEFAULT: slock = "TL_WRITE_DEFAULT"; break;
		case TL_WRITE_LOW_PRIORITY: slock = "TL_WRITE_LOW_PRIORITY"; break;
		case TL_WRITE: slock = "TL_WRITE"; break;
		case TL_WRITE_ONLY: slock = "TL_WRITE_ONLY"; break;
	}
	DBUG_PRINT("sparrow_handler", ("Table %s.%s, lock type %s", share_->getDatabaseName().c_str(), share_->getTableName().c_str(), slock));
#endif
	if (lockType != TL_IGNORE && lockData_.type == TL_UNLOCK) {
		lockData_.type= lockType;
	}
	*to++ = &lockData_;
	return to;
}

int SparrowHandler::external_lock(THD* thd, int lockType) {
	SPARROW_ENTER("SparrowHandler::external_lock");
#ifndef NDEBUG
	if (share_ != 0) {
		const char* slock = "unknown";
		switch (lockType) {
			case F_UNLCK: slock = "F_UNLCK"; break;
			case F_RDLCK: slock = "F_RDLCK"; break;
			case F_WRLCK: slock = "F_WRLCK"; break;
		}
		DBUG_PRINT("sparrow_handler", ("Table %s.%s, lock type %s", share_->getDatabaseName().c_str(), share_->getTableName().c_str(), slock));
	}
#endif
	if (lockType == F_WRLCK) {
		context_.writeLock();
	} else if (lockType == F_UNLCK) {
		context_.unlock();
	}
	return 0;
}

void SparrowHandler::column_bitmaps_signal() {
	SPARROW_ENTER("SparrowHandler::column_bitmaps_signal");
	context_.setActiveIndex(getActiveIndex());
}

int SparrowHandler::rnd_init(bool scan) {
	SPARROW_ENTER("SparrowHandler::rnd_init");
	DBUG_PRINT("sparrow_handler", ("Table %s.%s", share_->getDatabaseName().c_str(), share_->getTableName().c_str()));
	context_.resetPosition();
	return 0;
}

int SparrowHandler::rnd_next(uchar* buf) {
	SPARROW_ENTER("SparrowHandler::rnd_next");
	ha_statistic_increment(&System_status_var::ha_read_rnd_next_count);
	const bool result = context_.moveNext(buf);
	table->set_row_status_from_handler(!result);
	//table->status = result ? 0 : STATUS_NOT_FOUND;
	return result ? 0 : HA_ERR_END_OF_FILE;
}

int SparrowHandler::rnd_pos(uchar* buf, uchar* pos) {
	SPARROW_ENTER("SparrowHandler::rnd_pos");
	ha_statistic_increment(&System_status_var::ha_read_rnd_count);
	const bool result = context_.moveAbsolute(my_get_ptr(pos, sizeof(uint64_t)), buf);
	table->set_row_status_from_handler(!result);
	//table->status = result ? 0 : STATUS_NOT_FOUND;
	return result ? 0 : HA_ERR_END_OF_FILE;
}

int SparrowHandler::rnd_end() {
	SPARROW_ENTER("SparrowHandler::rnd_end");
	DBUG_PRINT("sparrow_handler", ("Table %s.%s", share_->getDatabaseName().c_str(), share_->getTableName().c_str()));
	context_.resetPosition();
	return 0;
}

void SparrowHandler::position(const uchar* record) {
	SPARROW_ENTER("SparrowHandler::position");
	const uint64_t recordPosition = context_.savePosition();
	my_store_ptr(ref, sizeof(uint64_t), recordPosition);
}

int SparrowHandler::index_init(uint idx, bool sorted) {
	SPARROW_ENTER("SparrowHandler::index_init");
	setActiveIndex(idx);
	DBUG_PRINT("sparrow_handler", ("Table %s.%s, index %u",
		share_->getDatabaseName().c_str(), share_->getTableName().c_str(), getActiveIndex()));
	context_.setActiveIndex(idx);
	return 0;
}

int SparrowHandler::index_next(uchar* buf) {
	SPARROW_ENTER("SparrowHandler::index_next");
	ha_statistic_increment(&System_status_var::ha_read_next_count);
	bool result = context_.findNextRecord(buf);
	table->set_row_status_from_handler(!result);
	//table->status = result ? 0 : STATUS_NOT_FOUND;
	return result ? 0 : HA_ERR_END_OF_FILE;
}

int SparrowHandler::index_prev(uchar* buf) {
	SPARROW_ENTER("SparrowHandler::index_prev");
	ha_statistic_increment(&System_status_var::ha_read_prev_count);
	bool result = context_.findPreviousRecord(buf);
	table->set_row_status_from_handler(!result);
	//table->status = result ? 0 : STATUS_NOT_FOUND;
	return result ? 0 : HA_ERR_END_OF_FILE;
}

int SparrowHandler::index_first(uchar* buf) {
	SPARROW_ENTER("SparrowHandler::index_first");
#ifndef NDEBUG
	const uint64_t start = my_micro_time();
#endif
	ha_statistic_increment(&System_status_var::ha_read_first_count);
	bool result = context_.findFirstRecord(buf);
	table->set_row_status_from_handler(!result);
	//table->status = result ? 0 : STATUS_NOT_FOUND;
	DBUG_PRINT("sparrow_handler", ("Table %s.%s, index %u, duration %llums",
		share_->getDatabaseName().c_str(), share_->getTableName().c_str(), getActiveIndex(), static_cast<ulonglong>((my_micro_time() - start) / 1000)));
	return result ? 0 : HA_ERR_END_OF_FILE;
}

int SparrowHandler::index_last(uchar* buf) {
	SPARROW_ENTER("SparrowHandler::index_last");
#ifndef NDEBUG
	const uint64_t start = my_micro_time();
#endif
	ha_statistic_increment(&System_status_var::ha_read_last_count);
	bool result = context_.findLastRecord(buf);
	table->set_row_status_from_handler(!result);
	//table->status = result ? 0 : STATUS_NOT_FOUND;
	DBUG_PRINT("sparrow_handler", ("Table %s.%s, index %u, duration %llums",
		share_->getDatabaseName().c_str(), share_->getTableName().c_str(), getActiveIndex(), static_cast<ulonglong>((my_micro_time() - start) / 1000)));
	return result ? 0 : HA_ERR_END_OF_FILE;
}

int SparrowHandler::index_read_map(uchar* buf, const uchar* key,
	key_part_map keyPartMap, enum ha_rkey_function findFlag) {
	SPARROW_ENTER("SparrowHandler::index_read_map");
#ifndef NDEBUG
	const uint64_t start = my_micro_time();
#endif
	ha_statistic_increment(&System_status_var::ha_read_key_count);
	if (key == 0) {
		const bool result = context_.findFirstRecord(buf);
		table->set_row_status_from_handler(!result);
		//table->status = result ? 0 : STATUS_NOT_FOUND;
		DBUG_PRINT("sparrow_handler", ("Table %s.%s, index %u, duration %llums",
			share_->getDatabaseName().c_str(), share_->getTableName().c_str(), getActiveIndex(), static_cast<ulonglong>((my_micro_time() - start) / 1000)));
		return result ? 0 : HA_ERR_END_OF_FILE;
	} else {
		const bool result = context_.findRecord(KeyValue(const_cast<uint8_t*>(key), keyPartMap), findFlag, buf);
		table->set_row_status_from_handler(!result);
		//table->status = result ? 0 : STATUS_NOT_FOUND;
		DBUG_PRINT("sparrow_handler", ("Table %s.%s, index %u, duration %llums",
			share_->getDatabaseName().c_str(), share_->getTableName().c_str(), getActiveIndex(), static_cast<ulonglong>((my_micro_time() - start) / 1000)));
		return result ? 0 : HA_ERR_END_OF_FILE;
	}
}

int SparrowHandler::index_read_idx_map(uchar* buf, uint index, const uchar* key,
	key_part_map keyPartMap, enum ha_rkey_function findFlag) {
	SPARROW_ENTER("SparrowHandler::index_read_idx_map");
	index_init(index, true);
	return index_read_map(buf, key, keyPartMap, findFlag);
}

int SparrowHandler::index_read_last_map(uchar* buf, const uchar* key, key_part_map keyPartMap) {
	SPARROW_ENTER("SparrowHandler::index_read_last_map");
	return index_read_map(buf, key, keyPartMap, HA_READ_PREFIX_LAST);
}

int SparrowHandler::index_end() {
	SPARROW_ENTER("SparrowHandler::index_end");
	DBUG_PRINT("sparrow_handler", ("Table %s.%s, index %u",
		share_->getDatabaseName().c_str(), share_->getTableName().c_str(), getActiveIndex()));
	setActiveIndex(MAX_KEY);
	context_.resetPosition();
	return 0;
}

double SparrowHandler::scan_time() {
	SPARROW_ENTER("SparrowHandler::scan_time");
	return ulonglong2double(getStats().data_file_length) / IO_SIZE + 2;
}

double SparrowHandler::read_time(uint index, uint ranges, ha_rows rows) {
	SPARROW_ENTER("SparrowHandler::read_time");
	return rows2double(ranges + rows);
}

ha_rows SparrowHandler::records_in_range(uint inx, key_range* minKey, key_range* maxKey) {
	SPARROW_ENTER("SparrowHandler::records_in_range");
#ifndef NDEBUG
	const uint64_t start = my_micro_time();
#endif
	const uint64_t records = context_.recordsInRange(inx, minKey, maxKey);
#ifndef NDEBUG
	if (records == HA_POS_ERROR) {
		DBUG_PRINT("sparrow_handler", ("Table %s.%s: cannot use index %u for now",
			share_->getDatabaseName().c_str(), share_->getTableName().c_str(), inx));
	} else {
		DBUG_PRINT("sparrow_handler", ("Table %s.%s, index %u; found %llu records in range in %ums",
			share_->getDatabaseName().c_str(), share_->getTableName().c_str(), inx, static_cast<ulonglong>(records), static_cast<uint32_t>((my_micro_time() - start) / 1000)));
	}
#endif
	return static_cast<ha_rows>(records);
}

ha_rows SparrowHandler::estimate_rows_upper_bound() {
	SPARROW_ENTER("SparrowHandler::estimate_rows_upper_bound");
	const uint64_t records = context_.recordsTotal();
#ifndef NDEBUG
	DBUG_PRINT("sparrow_handler", ("Table %s.%s, total recordss %llu",
		share_->getDatabaseName().c_str(), share_->getTableName().c_str(), static_cast<ulonglong>(records)));
#endif
	return static_cast<ha_rows>(records);
}

void SparrowHandler::start_bulk_insert(ha_rows rows) {
	SPARROW_ENTER("SparrowHandler::start_bulk_insert");
	context_.startInsert(static_cast<uint32_t>(rows));
}

int SparrowHandler::write_row(uchar* buf) {
	SPARROW_ENTER("SparrowHandler::write_row");
	ha_statistic_increment(&System_status_var::ha_write_count);
	// Handled by MySQL core: http://dev.mysql.com/doc/refman/5.6/en/timestamp-initialization.html
	//if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT) {
	//	table->timestamp_field->set_time();
	//}
	if (table->next_number_field && buf == table->record[0]) {
		const int error = update_auto_increment();
		if (error) {
			return error;
		}
	}
	const bool result = context_.insertRecord(buf);
	return result ? 0 : HA_ERR_INTERNAL_ERROR;
}

int SparrowHandler::end_bulk_insert() {
	SPARROW_ENTER("SparrowHandler::end_bulk_insert");
	const bool result = context_.endInsert();
	return result ? 0 : HA_ERR_INTERNAL_ERROR;
}

int SparrowHandler::update_row(const uchar* old_data, uchar* new_data) {
	SPARROW_ENTER("SparrowHandler::update_row");
	ha_statistic_increment(&System_status_var::ha_update_count);
	//if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE) {
	//	table->timestamp_field->set_time();
	//}
	const bool result = context_.updateRecord(new_data);
	return result ? 0 : HA_ERR_INTERNAL_ERROR;
}

int SparrowHandler::delete_row(const uchar* buf) {
	SPARROW_ENTER("SparrowHandler::delete_row");
	ha_statistic_increment(&System_status_var::ha_delete_count);
	return HA_ERR_WRONG_COMMAND;
}

int SparrowHandler::delete_all_rows() {
	SPARROW_ENTER("SparrowHandler::delete_all_rows");
	return HA_ERR_WRONG_COMMAND;
}

void SparrowHandler::unlock_row() {
	SPARROW_ENTER("SparrowHandler::unlock_row");
}

bool SparrowHandler::get_error_message(int error, String* buf) {
	SPARROW_ENTER("SparrowHandler::get_error_message");
	// TODO retrieve last exception message
	return false;
}

/* Request storage engine to do an extra operation: enable,disable or run some functionality.
    See mysql\include\my_base.h:184 for details. 
 */
int SparrowHandler::extra(enum ha_extra_function operation) {
	SPARROW_ENTER("SparrowHandler::extra");
	return 0;
}

int SparrowHandler::reset() {
	SPARROW_ENTER("SparrowHandler::reset");
	DBUG_PRINT("sparrow_handler", ("Reset context for table %s.%s",
		share_->getDatabaseName().c_str(), share_->getTableName().c_str()));
	context_.reset();
	return 0;
}

int SparrowHandler::records(ha_rows *num_rows) {
	SPARROW_ENTER("SparrowHandler::records");
	*num_rows = getStats().records;
#ifndef NDEBUG
	DBUG_PRINT("sparrow_handler", ("Number of records in table %s.%s: %llu",
		share_->getDatabaseName().c_str(), share_->getTableName().c_str(), static_cast<ulonglong>(*num_rows)));
#endif
	return 0;
}

uint SparrowHandler::max_supported_record_length() const {
	SPARROW_ENTER("SparrowHandler::max_supported_record_length");
	return HA_MAX_REC_LENGTH;		// Actually, that's the deafult value.
}

uint SparrowHandler::max_supported_keys() const {
	SPARROW_ENTER("SparrowHandler::max_supported_keys");
	return MAX_KEY;		// Actually, that's the deafult value.
}

uint SparrowHandler::max_supported_key_parts() const {
	SPARROW_ENTER("SparrowHandler::max_supported_key_parts");
	return MAX_REF_PARTS;	// Actually, that's the deafult value.
}

uint SparrowHandler::max_supported_key_length() const {
	SPARROW_ENTER("SparrowHandler::max_supported_key_length");
	return MAX_KEY_LENGTH;	// Actually, that's the deafult value.
}

uint SparrowHandler::max_supported_key_part_length(HA_CREATE_INFO *create_info) const {
	SPARROW_ENTER("SparrowHandler::max_supported_key_part_length");
	return MAX_KEY_LENGTH;	// Actually, that's the deafult value.
}

bool SparrowHandler::is_crashed() const {
	SPARROW_ENTER("SparrowHandler::is_crashed");
	return false;
}

bool SparrowHandler::auto_repair() const {
	SPARROW_ENTER("SparrowHandler::auto_repair");
	return false;
}

void SparrowHandler::get_auto_increment(ulonglong offset, ulonglong increment,
	ulonglong nb_desired_values, ulonglong* first_value,
	ulonglong* nb_reserved_values) {
	SPARROW_ENTER("SparrowHandler::get_auto_increment");
	SparrowHandler::info(HA_STATUS_AUTO);
	*first_value = stats.auto_increment_value;
	*nb_reserved_values = ULLONG_MAX;
}

/*
int SparrowHandler::reset_auto_increment(ulonglong value) {
	SPARROW_ENTER("SparrowHandler::reset_auto_increment");
	DBUG_PRINT("sparrow_handler", ("Reset auto increment on table %s.%s to %llu",
		share_->getDatabaseName().c_str(), share_->getTableName().c_str(), static_cast<ulonglong>(value)));
	share_->getMaster().setAutoInc(static_cast<int64_t>(value));
	return 0;
}*/

bool SparrowHandler::check_if_incompatible_data(HA_CREATE_INFO* info, uint table_changes) {
	SPARROW_ENTER("SparrowHandler::check_if_incompatible_data");
	return table_changes == IS_EQUAL_YES ? COMPATIBLE_DATA_YES : COMPATIBLE_DATA_NO;
}

enum_alter_inplace_result SparrowHandler::check_if_supported_inplace_alter(TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
	SPARROW_ENTER("SparrowHandler::check_if_supported_inplace_alter");

	// Check the alterations are compatible with Sparrow
	if ( check_alterations( ha_alter_info->alter_info ) != 0 ) {
		return HA_ALTER_ERROR;
	}

	// Operations for altering a table that Sparrow does not care about
	Alter_inplace_info::HA_ALTER_FLAGS inplace_ignore_operations=
		Alter_inplace_info::ALTER_COLUMN_COLUMN_FORMAT |
		Alter_inplace_info::ALTER_COLUMN_STORAGE_TYPE;

	// Column alterations in Sparrow can be performed without table copy.
	Alter_inplace_info::HA_ALTER_FLAGS inplace_offline_operations=
		Alter_inplace_info::ADD_INDEX |
		Alter_inplace_info::DROP_INDEX |
		Alter_inplace_info::ADD_UNIQUE_INDEX |
		Alter_inplace_info::DROP_UNIQUE_INDEX |
		Alter_inplace_info::ADD_PK_INDEX |
		Alter_inplace_info::DROP_PK_INDEX |
		Alter_inplace_info::ADD_COLUMN |
		Alter_inplace_info::DROP_COLUMN |
		Alter_inplace_info::ALTER_VIRTUAL_COLUMN_ORDER |
		Alter_inplace_info::ALTER_STORED_COLUMN_ORDER |
		Alter_inplace_info::ALTER_COLUMN_DEFAULT |
		Alter_inplace_info::ADD_FOREIGN_KEY |
		Alter_inplace_info::DROP_FOREIGN_KEY |
		Alter_inplace_info::ALTER_VIRTUAL_COLUMN_TYPE |
		Alter_inplace_info::ALTER_STORED_COLUMN_TYPE |
		Alter_inplace_info::ALTER_COLUMN_EQUAL_PACK_LENGTH |
		Alter_inplace_info::ALTER_COLUMN_NAME |
		Alter_inplace_info::ALTER_COLUMN_DEFAULT |
		Alter_inplace_info::CHANGE_CREATE_OPTION;

	/* Is there at least one operation that requires copy algorithm? */
	if (ha_alter_info->handler_flags & ~(inplace_offline_operations | inplace_ignore_operations))
		return HA_ALTER_INPLACE_NOT_SUPPORTED;

	/*if (ha_alter_info->handler_flags & Alter_inplace_info::CHANGE_CREATE_OPTION) {
		HA_CREATE_INFO *create_info= ha_alter_info->create_info;

		// TODO: Sparrow - allow changing character set/collation from utf8 to utf8_bin without table copy.
		bool	inplace = false;
		if ( create_info->used_fields & (HA_CREATE_USED_CHARSET | HA_CREATE_USED_DEFAULT_CHARSET)
			&& table->s->table_charset == &my_charset_utf8mb4_general_ci
			&& (create_info->table_charset == &my_charset_utf8mb4_bin || create_info->default_table_charset == &my_charset_utf8mb4_bin) ) {
			inplace = true;
		}

		if ( !inplace) 
			return HA_ALTER_INPLACE_NOT_SUPPORTED;
	}*/

	if (ha_alter_info->handler_flags & Alter_inplace_info::ALTER_STORED_COLUMN_TYPE) {
		// TODO: Sparrow: changing the length of a VARCHAR or VARBINARY field is allowed.
		/*if (tmp != IS_EQUAL_YES
			&& tmp_new_field->sql_type == field->real_type()
			&& field->real_type() == MYSQL_TYPE_VARCHAR
			&& tmp_new_field->length != field->max_display_length()) {
				tmp = IS_EQUAL_YES;
		}*/
	}

	return HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE;
}

//bool SparrowHandler::prepare_inplace_alter_table( TABLE *altered_table, Alter_inplace_info *ha_alter_info )
bool SparrowHandler::prepare_inplace_alter_table(TABLE *altered_table, Alter_inplace_info *ha_alter_info, const dd::Table *old_table_def, dd::Table *new_table_def)
{
	SPARROW_ENTER("SparrowHandler::prepare_inplace_alter_table");
	return false;
}

//bool SparrowHandler::inplace_alter_table( TABLE *altered_table, Alter_inplace_info *ha_alter_info )
bool SparrowHandler::inplace_alter_table(TABLE *altered_table, Alter_inplace_info *ha_alter_info, const dd::Table *old_table_def, dd::Table *new_table_def)
{
	SPARROW_ENTER("SparrowHandler::prepare_inplace_alter_table");
	DBUG_PRINT("sparrow_handler", ("Start altering table %s.%s", share_->getDatabaseName().c_str(), share_->getTableName().c_str()));

	if ( ha_alter_info->handler_flags & Alter_inplace_info::CHANGE_CREATE_OPTION ) {
		update_create_info( ha_alter_info->create_info );
	}

	if ( ha_alter_info->handler_flags & (Alter_inplace_info::DROP_INDEX | Alter_inplace_info::DROP_UNIQUE_INDEX | Alter_inplace_info::DROP_PK_INDEX) ) {
		drop_index( table, ha_alter_info->index_drop_buffer, ha_alter_info->index_drop_count );
	}

	if ( ha_alter_info->handler_flags & (Alter_inplace_info::ADD_INDEX | Alter_inplace_info::ADD_UNIQUE_INDEX | Alter_inplace_info::ADD_PK_INDEX) ) {
		add_index( altered_table, ha_alter_info->key_info_buffer, ha_alter_info->index_add_buffer, ha_alter_info->index_add_count );
	}

	return false;
}

//bool SparrowHandler::commit_inplace_alter_table( TABLE *altered_table, Alter_inplace_info *ha_alter_info, bool commit )
bool SparrowHandler::commit_inplace_alter_table(TABLE *altered_table, Alter_inplace_info *ha_alter_info, bool commit, const dd::Table *old_table_def, dd::Table *new_table_def)
{
	SPARROW_ENTER("SparrowHandler::commit_inplace_alter_table");
	if ( commit == false && context_.getAltered() == true ) { 
		spw_print_error("Sparrow: Cannot rollback alterations already made on table %s.%s", 
			share_->getDatabaseName().c_str(), share_->getTableName().c_str());
	}
	return false;
}

//void SparrowHandler::notify_table_changed()
void SparrowHandler::notify_table_changed(Alter_inplace_info *ha_alter_info)
{
	SPARROW_ENTER("SparrowHandler::notify_table_changed");
}

int SparrowHandler::add_index(TABLE* altered_table, KEY* key_info_buffer, uint* index_add_buffer, uint nb) {
	SPARROW_ENTER("SparrowHandler::add_index");
	Master& master = share_->getMaster();
	KEY* key_info = NULL;
	try {
		WriteGuard guard(master.getLock());
		Indexes indexes = master.getIndexes();
		IndexMappings mappings = master.getIndexMappings();
		Alterations alterations = master.getIndexAlterations();
		uint32_t serial = master.getIndexAlterSerial();

		for ( uint i=0; i<nb; ++i ) {
			// Build new index.
			key_info = &key_info_buffer[index_add_buffer[i]];
			DBUG_PRINT("sparrow_handler", ("Add index %s on table %s.%s",
				print_KEY(*key_info).c_str(), share_->getDatabaseName().c_str(), share_->getTableName().c_str()));
			const uint indexColumns = key_info->user_defined_key_parts;
			ColumnIds columnIds(indexColumns);
			for (uint j = 0; j < indexColumns; ++j) {
				const Field* field = key_info->key_part[j].field;
				uint32_t	colId = UINT_MAX;
				if ( field != NULL ) {
					colId = static_cast<int>(master.getColumn(Str(field->field_name)));
				} else {
					int		colPos = static_cast<int>(key_info->key_part[j].fieldnr);
					colId = master.getColumn(colPos);
				}
				columnIds.append(colId);
			}
			const bool unique = (key_info->flags & HA_NOSAME) != 0;
			const Index newIndex(key_info->name, columnIds, unique);

			// Append new index.
			const uint32_t offset = indexes.length();
			indexes.append(newIndex);
			const uint32_t mysqlIndexId = index_add_buffer[i];
			while (mappings.length() <= mysqlIndexId) {
				mappings.append(-1);
			}
			mappings[mysqlIndexId] = static_cast<int>(offset);
			alterations.append(Alteration(ALT_ADD_INDEX, ++serial, offset));
		}
		assert(!alterations.isEmpty());
		master.setIndexes(indexes);
		master.setIndexMappings(mappings);
		master.setIndexAlterSerial(serial);
		master.setIndexAlterations(alterations);
		master.toDisk();
		context_.setAltered( true );
	} catch(const SparrowException& e) {
		if ( key_info != NULL ) { 
			spw_print_error("Sparrow: Cannot add index %s on table %s.%s: %s", print_KEY(*key_info).c_str(),
				share_->getDatabaseName().c_str(), share_->getTableName().c_str(), e.getText());
		} else {
			spw_print_error("Sparrow: Cannot add index(es) on table %s.%s: %s",
				share_->getDatabaseName().c_str(), share_->getTableName().c_str(), e.getText());
		}
		return -1;
	}
	master.startIndexAlter(false);
	return 0;
}


int SparrowHandler::drop_index(TABLE* table, KEY** key_info_buffer, uint nb) {
	SPARROW_ENTER("SparrowHandler::drop_index");
	assert(nb != 0);
	Master& master = share_->getMaster();
	KEY* key_info = NULL;
	try {
		WriteGuard guard(master.getLock());
		Indexes indexes = master.getIndexes();
		IndexMappings mappings = master.getIndexMappings();
		Alterations alterations = master.getIndexAlterations();
		uint32_t serial = master.getIndexAlterSerial();

		for ( uint i=0; i<nb; ++i ) {
			key_info = key_info_buffer[i];
			DBUG_PRINT("sparrow_handler", ("Drop index %s on table %s.%s", print_KEY(*key_info).c_str(),
				share_->getDatabaseName().c_str(), share_->getTableName().c_str()));
			// Search for this index's position in mysql internal data array
			const uint nbIndexes = table->s->keys;
			uint j = 0;
			for (; j < nbIndexes; ++j) {
				const KEY& index = table->key_info[j];
				if ( strcmp( key_info->name, index.name ) == 0 ) {
					const uint32_t mysqlIndexId = j;
					const int offset = mappings[mysqlIndexId];
					mappings[mysqlIndexId] = -1;
					indexes[offset].drop();
					alterations.append(Alteration(ALT_DROP_INDEX, ++serial, offset));
					break;
				}
			}
			assert( j < nbIndexes );
		}
		assert(!alterations.isEmpty());
		master.setIndexes(indexes);
		master.setIndexMappings(mappings);
		master.setIndexAlterSerial(serial);
		master.setIndexAlterations(alterations);
		master.toDisk();
		context_.setAltered( true );
	} catch(const SparrowException& e) {
		if ( key_info!= NULL ) { 
		spw_print_error("Sparrow: Cannot drop index %s from table %s.%s: %s", print_KEY(*key_info).c_str(),
			share_->getDatabaseName().c_str(), share_->getTableName().c_str(), e.getText());
		} else {
			spw_print_error("Sparrow: Cannot drop index(es) from table %s.%s: %s",
				share_->getDatabaseName().c_str(), share_->getTableName().c_str(), e.getText());
		}
		return -1;
	}
	master.startIndexAlter(false);
	return 0;
}

void SparrowHandler::update_create_info(HA_CREATE_INFO* create_info) {
	SPARROW_ENTER("SparrowHandler::update_create_info");
	table->file->info(HA_STATUS_AUTO);
	if (!(create_info->used_fields & HA_CREATE_USED_AUTO)) {
		create_info->auto_increment_value = stats.auto_increment_value;
	}
}

// Methods for information schema tables.

// Timestamps fields are displayed in seconds. So the decimals are set to 0. 
static ST_FIELD_INFO sparrow_tables_field_info[] = {
	// Name, length, type, value, maybe_null, old_name, open_method.
	{"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{"DATA_SIZE", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"INDEX_SIZE", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"PERIOD", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"PARTITIONS", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"FILES", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"PERSISTENT_RECORDS", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"TRANSIENT_RECORDS", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"OLDEST", 0, MYSQL_TYPE_TIMESTAMP, 0, true, 0, 0 },
	{"NEWEST", 0, MYSQL_TYPE_TIMESTAMP, 0, true, 0, 0 },
	{"MAX_LIFETIME", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"DEFAULT_WHERE", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"STRING_OPTIMIZATION", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"COALESCING_PERIOD", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"COALESCING_PERCENTAGE", 5, MYSQL_TYPE_DOUBLE, 0, true, 0, 0 },
	{"AGE", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{0, 0, MYSQL_TYPE_NULL, 0, 0, 0, 0}
};

// STATIC
int SparrowHandler::initializeISTables(void* p) {
	SPARROW_ENTER("SparrowHandler::initializeISTables");
	ST_SCHEMA_TABLE* schema = static_cast<ST_SCHEMA_TABLE*>(p);
	schema->fields_info = sparrow_tables_field_info;
	schema->fill_table = SparrowHandler::fillISTables;
	return 0;
}

// STATIC
int SparrowHandler::fillISTables(THD* thd, Table_ref* tables, [[maybe_unused]] Item* cond) {
	SPARROW_ENTER("SparrowHandler::fillISTables");
	CHARSET_INFO* scs = system_charset_info;
	TABLE* table = static_cast<TABLE*>(tables->table);
	SortedMasters masters = InternalApi::getAll();
	char tmp[1024];
	for (uint32_t i = 0; i < masters.length(); ++i) {
		const Master& master = *masters[i];
		ReadGuard masterGuard(master.getLock());
		int f = 0;
		const Str& sdatabase = master.getDatabase();
		table->field[f++]->store(sdatabase.c_str(), static_cast<uint>(sdatabase.length()), scs);
		const Str& stable = master.getTable();
		table->field[f++]->store(stable.c_str(), static_cast<uint>(stable.length()), scs);
		table->field[f++]->store(static_cast<int64_t>(master.getDataSize()), false);
		table->field[f++]->store(static_cast<int64_t>(master.getIndexSize()), false);
		table->field[f++]->store(static_cast<int64_t>(master.getAggregationPeriod()), false);
		const uint32_t partitions = master.getPartitions().length();
		table->field[f++]->store(static_cast<int64_t>(partitions), false);
		table->field[f++]->store(static_cast<int64_t>(1 + master.getIndexMappings().length()), false);
		table->field[f++]->store(static_cast<int64_t>(master.getRecords()), false);
		table->field[f++]->store(static_cast<int64_t>(master.getTransientRecords()), false);
		const uint64_t oldest = master.getOldest() / 1000;
		if (oldest == 0) {
			table->field[f++]->set_null();
		} else {
			table->field[f]->set_notnull();
			my_timeval		tm;
			tm.m_tv_sec = oldest;
			tm.m_tv_usec = 0;
			static_cast<Field_timestamp*>(table->field[f++])->store_timestamp(&tm);
		}
		const uint64_t newest = master.getNewest() / 1000;
		if (newest == 0) {
			table->field[f++]->set_null();
		} else {
			table->field[f]->set_notnull();
			my_timeval		tm;
			tm.m_tv_sec = newest;
			tm.m_tv_usec = 0;
			static_cast<Field_timestamp*>(table->field[f++])->store_timestamp(&tm);
		}
		table->field[f++]->store(static_cast<int64_t>(master.getMaxLifetime() / 1000), false);
		table->field[f++]->store(static_cast<int64_t>(master.getDefaultWhere() / 1000), false);
		table->field[f++]->store(static_cast<int64_t>(master.getStringOptimization()), false);
		table->field[f++]->store(static_cast<int64_t>(master.getCoalescingPeriod() / 1000), false);
		const double coalescingPercentage = master.getCoalescingPercentage();
		if (coalescingPercentage < 0) {
			table->field[f++]->set_null();
		} else {
			table->field[f]->set_notnull();
			snprintf(tmp, sizeof(tmp), "%.1f", coalescingPercentage);
			table->field[f++]->store(tmp, static_cast<uint>(strlen(tmp)), scs);
		}
		table->field[f++]->store(static_cast<int64_t>(master.getAge() / 1000), false);
		schema_table_store_record(thd, table);
	}
	return 0;
}

// STATIC
int SparrowHandler::deinitializeISTables(void* p) {
	SPARROW_ENTER("SparrowHandler::deinitializeISTables");
	return 0;
}

static ST_FIELD_INFO sparrow_columns_field_info[] = {
	// Name, length, type, value, maybe_null, old_name, open_method.
	{"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{"COLUMN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{"INTERNAL_ID", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"IS_DROPPED", 3, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{"IS_IP", 3, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{"IP_LOOKUP", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, true, 0, 0 },
	{"IS_DNS_IDENTIFIER", 3, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{"SERIAL", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"DROP_SERIAL", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{0, 0, MYSQL_TYPE_NULL, 0, 0, 0, 0}
};

// STATIC
int SparrowHandler::initializeISColumns(void* p) {
	SPARROW_ENTER("SparrowHandler::initializeISColumns");
	ST_SCHEMA_TABLE* schema = static_cast<ST_SCHEMA_TABLE*>(p);
	schema->fields_info = sparrow_columns_field_info;
	schema->fill_table = SparrowHandler::fillISColumns;
	return 0;
}

// STATIC
int SparrowHandler::fillISColumns(THD* thd, Table_ref* tables, [[maybe_unused]] Item* cond) {
	SPARROW_ENTER("SparrowHandler::fillISColumns");
	CHARSET_INFO* scs = system_charset_info;
	TABLE* table = static_cast<TABLE*>(tables->table);
	SortedMasters masters = InternalApi::getAll();
	for (uint32_t i = 0; i < masters.length(); ++i) {
		const Master& master = *masters[i];
		ReadGuard masterGuard(master.getLock());
		const Columns& columns = master.getColumns();
		for (uint32_t j = 0; j < columns.length(); ++j) {
			int f = 0;
			const Str& sdatabase = master.getDatabase();
			table->field[f++]->store(sdatabase.c_str(), static_cast<uint>(sdatabase.length()), scs);
			const Str& stable = master.getTable();
			table->field[f++]->store(stable.c_str(), static_cast<uint>(stable.length()), scs);
			const Column& column = columns[j];
			const Str& scolumn = column.getName();
			table->field[f++]->store(scolumn.c_str(), static_cast<uint>(scolumn.length()), scs);
			table->field[f++]->store(static_cast<int64_t>(j), false);
			const char* s = column.isDropped() ? "YES" : "NO";
			table->field[f++]->store(s, static_cast<uint>(strlen(s)), scs);
			s = column.isFlagSet(COL_IP_ADDRESS) ? "YES" : "NO";
			table->field[f++]->store(s, static_cast<uint>(strlen(s)), scs);
			if (column.isFlagSet(COL_IP_LOOKUP)) {
				table->field[f]->set_notnull();
				s = columns[column.getInfo()].getName().c_str();
				table->field[f++]->store(s, static_cast<uint>(strlen(s)), scs);
			} else {
				table->field[f++]->set_null();
			}
			s = column.isFlagSet(COL_DNS_IDENTIFIER) ? "YES" : "NO";
			table->field[f++]->store(s, static_cast<uint>(strlen(s)), scs);
			table->field[f++]->store(static_cast<int64_t>(column.getSerial()), false);
			table->field[f++]->store(static_cast<int64_t>(column.getDropSerial()), false);
			schema_table_store_record(thd, table);
		}
	}
	return 0;
}

// STATIC
int SparrowHandler::deinitializeISColumns(void* p) {
	SPARROW_ENTER("SparrowHandler::deinitializeISColumns");
	return 0;
}

static ST_FIELD_INFO sparrow_indexes_field_info[] = {
	// Name, length, type, value, maybe_null, old_name, open_method.
	{"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{"INDEX_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{"UNIQUE", 3, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{"COLUMNS", 1024, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{"FILE_SUFFIX", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{0, 0, MYSQL_TYPE_NULL, 0, 0, 0, 0}
};

// STATIC
int SparrowHandler::initializeISIndexes(void* p) {
	SPARROW_ENTER("SparrowHandler::initializeISIndexes");
	ST_SCHEMA_TABLE* schema = static_cast<ST_SCHEMA_TABLE*>(p);
	schema->fields_info = sparrow_indexes_field_info;
	schema->fill_table = SparrowHandler::fillISIndexes;
	return 0;
}

// STATIC
int SparrowHandler::fillISIndexes(THD* thd, Table_ref* tables, [[maybe_unused]] Item* cond) {
	SPARROW_ENTER("SparrowHandler::fillISIndexes");
	CHARSET_INFO* scs = system_charset_info;
	TABLE* table = static_cast<TABLE*>(tables->table);
	SortedMasters masters = InternalApi::getAll();
	for (uint32_t i = 0; i < masters.length(); ++i) {
		const Master& master = *masters[i];
		ReadGuard masterGuard(master.getLock());
		const Indexes& indexes = master.getIndexes();
		const Columns& columns = master.getColumns();
		for (uint32_t j = 0; j < indexes.length(); ++j) {
			const Index& index = indexes[j];
			if (index.isDropped()) {
				continue;
			}
			int f = 0;
			const Str& sdatabase = master.getDatabase();
			table->field[f++]->store(sdatabase.c_str(), static_cast<uint>(sdatabase.length()), scs);
			const Str& stable = master.getTable();
			table->field[f++]->store(stable.c_str(), static_cast<uint>(stable.length()), scs);
			const Str& sindex = index.getName();
			table->field[f++]->store(sindex.c_str(), static_cast<uint>(sindex.length()), scs);
			const char* s = index.isUnique() ? "YES" : "NO";
			table->field[f++]->store(s, static_cast<uint>(strlen(s)), scs);
			Str scolumns;
			const ColumnIds& columnIds = index.getColumnIds();
			for (uint32_t k = 0; k < columnIds.length(); ++k) {
				if (k > 0) {
					scolumns += Str(", ");
				}
				scolumns += columns[columnIds[k]].getName();
			}
			table->field[f++]->store(scolumns.c_str(), static_cast<uint>(scolumns.length()), scs);
			char buffer[128];
			snprintf(buffer, sizeof(buffer), "_%02u.spi", j);
			table->field[f++]->store(buffer, static_cast<uint>(strlen(buffer)), scs);
			schema_table_store_record(thd, table);
		}
	}
	return 0;
}

// STATIC
int SparrowHandler::deinitializeISIndexes(void* p) {
	SPARROW_ENTER("SparrowHandler::deinitializeISIndexes");
	return 0;
}

static ST_FIELD_INFO sparrow_alterations_field_info[] = {
	// Name, length, type, value, maybe_null, old_name, open_method.
	{"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{"INFO", 1024, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{"PERCENTAGE", 5, MYSQL_TYPE_DOUBLE, 0, false, 0, 0 },
	{0, 0, MYSQL_TYPE_NULL, 0, 0, 0, 0}
};

// STATIC
int SparrowHandler::initializeISAlterations(void* p) {
	SPARROW_ENTER("SparrowHandler::initializeISAlterations");
	ST_SCHEMA_TABLE* schema = static_cast<ST_SCHEMA_TABLE*>(p);
	schema->fields_info = sparrow_alterations_field_info;
	schema->fill_table = SparrowHandler::fillISAlterations;
	return 0;
}

// STATIC
int SparrowHandler::fillISAlterations(THD* thd, Table_ref* tables, [[maybe_unused]] Item* cond) {
	SPARROW_ENTER("SparrowHandler::fillISAlterations");
	CHARSET_INFO* scs = system_charset_info;
	TABLE* table = static_cast<TABLE*>(tables->table);
	SortedMasters masters = InternalApi::getAll();
	char tmp[1024];
	for (uint32_t i = 0; i < masters.length(); ++i) {
		uint64_t elapsed;
		uint64_t left;
		double percentage;
		const Master& master = *masters[i];
		ReadGuard guard(master.getLock());
		if (master.getIndexAlterStatus(elapsed, left, percentage)) {
			const Alterations& alterations = master.getIndexAlterations();
			for (uint32_t j = 0; j < alterations.length(); ++j) {
				int f = 0;
				const Str& sdatabase = master.getDatabase();
				table->field[f++]->store(sdatabase.c_str(), static_cast<uint>(sdatabase.length()), scs);
				const Str& stable = master.getTable();
				table->field[f++]->store(stable.c_str(), static_cast<uint>(stable.length()), scs);
				const Str info = alterations[j].getDescription(master);
				table->field[f++]->store(info.c_str(), static_cast<uint>(info.length()), scs);
				snprintf(tmp, sizeof(tmp), "%.1f", percentage);
				table->field[f++]->store(tmp, static_cast<uint>(strlen(tmp)), scs);
				schema_table_store_record(thd, table);
			}
		}
	}
	return 0;
}

// STATIC
int SparrowHandler::deinitializeISAlterations(void* p) {
	SPARROW_ENTER("SparrowHandler::deinitializeISAlterations");
	return 0;
}

// Timestamps fields are displayed in seconds. So the decimals are set to 0. 
static ST_FIELD_INFO sparrow_partitions_field_info[] = {
	// Name, length, type, value, maybe_null, old_name, open_method.
	{"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{"VERSION", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"SERIAL", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"DATA_SERIAL", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"DATA_RECORDS", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"RECORD_OFFSET", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"INDEX_ALTER_SERIAL", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"COLUMN_ALTER_SERIAL", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"FILESYSTEM", 1024, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	//{"START", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_TIMESTAMP, 0, false, 0, 0 },
	{"START", 0, MYSQL_TYPE_TIMESTAMP, 0, false, 0, 0 },
	//{"END", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_TIMESTAMP, 0, false, 0, 0 },
	{"END", 0, MYSQL_TYPE_TIMESTAMP, 0, false, 0, 0 },
	{"DURATION", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"RECORDS", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"DATA_SIZE", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"INDEX_SIZE", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, false, 0, 0 },
	{"IS_READY", 3, MYSQL_TYPE_STRING, 0, false, 0, 0 },
	{0, 0, MYSQL_TYPE_NULL, 0, 0, 0, 0}
};

// STATIC
int SparrowHandler::initializeISPartitions(void* p) {
	SPARROW_ENTER("SparrowHandler::initializeISPartitions");
	ST_SCHEMA_TABLE* schema = static_cast<ST_SCHEMA_TABLE*>(p);
	schema->fields_info = sparrow_partitions_field_info;
	schema->fill_table = SparrowHandler::fillISPartitions;
	return 0;
}

// STATIC
int SparrowHandler::fillISPartitions(THD* thd, Table_ref* tables, [[maybe_unused]] Item* cond) {
	SPARROW_ENTER("SparrowHandler::fillISPartitions");
	CHARSET_INFO* scs = system_charset_info;
	TABLE* table = static_cast<TABLE*>(tables->table);
	SortedMasters masters = InternalApi::getAll();
	for (uint32_t i = 0; i < masters.length(); ++i) {
		const Master& master = *masters[i];
		ReadGuard masterGuard(master.getLock());
		const Partitions& partitions = master.getPartitions();
		for (uint32_t j = 0; j < partitions.length(); ++j) {
			const Partition& p = *partitions[j];
			if (p.isTransient() || p.isTemporary()) {
				continue;
			}
			const PersistentPartition& partition = static_cast<const PersistentPartition&>(p);
			int f = 0;
			const Str& sdatabase = master.getDatabase();
			table->field[f++]->store(sdatabase.c_str(), static_cast<uint>(sdatabase.length()), scs);
			const Str& stable = master.getTable();
			table->field[f++]->store(stable.c_str(), static_cast<uint>(stable.length()), scs);
			table->field[f++]->store(static_cast<int64_t>(partition.getVersion()), false);
			table->field[f++]->store(static_cast<int64_t>(partition.getSerial()), false);
			table->field[f++]->store(static_cast<int64_t>(partition.getDataSerial()), false);
			table->field[f++]->store(static_cast<int64_t>(partition.getDataRecords()), false);
			table->field[f++]->store(static_cast<int64_t>(partition.getRecordOffset()), false);
			table->field[f++]->store(static_cast<int64_t>(partition.getIndexAlterSerial()), false);
			table->field[f++]->store(static_cast<int64_t>(partition.getColumnAlterSerial()), false);
			const char* s = FileUtil::getFilesystemPath(partition.getFilesystem());
			table->field[f++]->store(s, static_cast<uint>(strlen(s)), scs);
			// For tables in the information_schema, timestamps must be rounded to the second, therefore, we round the min timestamp to the lower second,
			//	and the max timestamp to the higher second. 
			const uint64_t start = partition.getMin();
			my_timeval		tm;
			tm.m_tv_sec = start/1000;
			tm.m_tv_usec = 0;
			static_cast<Field_timestamp*>(table->field[f++])->store_timestamp(&tm);
			const uint64_t end = partition.getMax();
			tm.m_tv_sec = end/1000;
			if ((end%1000) != 0) tm.m_tv_sec += 1;
			static_cast<Field_timestamp*>(table->field[f++])->store_timestamp(&tm);
			table->field[f++]->store(static_cast<int64_t>(end - start), false);
			table->field[f++]->store(static_cast<int64_t>(partition.getRecords()), false);
			table->field[f++]->store(static_cast<int64_t>(partition.getDataSize()), false);
			table->field[f++]->store(static_cast<int64_t>(partition.getIndexSize()), false);
			s = partition.isReady() ? "YES" : "NO";
			table->field[f++]->store(s, static_cast<uint>(strlen(s)), scs);
			schema_table_store_record(thd, table);
		}
	}
	return 0;
}

// STATIC
int SparrowHandler::deinitializeISPartitions(void* p) {
	SPARROW_ENTER("SparrowHandler::deinitializeISPartitions");
	return 0;
}

// STATIC
Str SparrowHandler::print_KEY(const KEY& index) {
	char	buffer[256];
	sprintf( buffer, "%s {", index.name );
	for ( uint i=0; i<index.user_defined_key_parts; ++i ) {
		if (i != 0 ) strcat(buffer, ", ");
		const Field* field = index.key_part[i].field;
		if ( field != NULL ) {
			strcat(buffer, field->field_name);
		} else {
			char	buffer2[64];
			sprintf(buffer2, "%u", index.key_part[i].fieldnr);
			strcat(buffer, buffer2);
		}
	}
	strcat(buffer, "}");
	return Str(buffer, static_cast<int>(strlen(buffer)));
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// TableShare
//////////////////////////////////////////////////////////////////////////////////////////////////////

// This object is locked by MySQL and represents the table data. It contains a reference to the
// master file, as well as pre-built fields and record readers which can be shared between
// multiple handler threads.

SYSpHash<TableShare> TableShare::hash_(1024);
Lock TableShare::lock_(true, "TableShare::lock_");

TableShare::TableShare(const Str& databaseName, const Str& tableName, TABLE* table) _THROW_(SparrowException)
	: databaseName_(databaseName), tableName_(tableName), key_(table == 0) {
	SPARROW_ENTER("TableShare::TableShare");
	if (!key_) {
		thr_lock_init(&tableLock_);
		master_ = InternalApi::get(databaseName.c_str(), tableName.c_str(), false, false, table->s);
		bool changed = false;
		PartitionIds	flushedPartitions;
		{
			WriteGuard guard(master_->getLock());

			// Check if there are column alterations.
			if (current_thd->lex->alter_info != nullptr)
			{
				Alter_info& alterInfo = *current_thd->lex->alter_info;
				const uint alterFlags = alterInfo.flags;
				if (alterFlags & Alter_info::ALTER_ADD_COLUMN) {
					List_iterator<Create_field> iterator(alterInfo.create_list);
					Create_field* field;
					while ((field = iterator++) != 0) {
						Column newColumn = SparrowHandler::createColumn(*field);
						master_->addColumn(field->after, newColumn);
						changed = true;
					}
				}
				if (alterFlags & Alter_info::ALTER_DROP_COLUMN) {
					for (const Alter_drop *drop : alterInfo.drop_list) {
						if (drop->type == Alter_drop::COLUMN) {
							master_->dropColumn(drop->name);
							changed = true;
						}
					}
				}
				if (alterFlags & Alter_info::ALTER_CHANGE_COLUMN) {
					List_iterator<Create_field> iterator(alterInfo.create_list);
					Create_field* field;
					while ((field = iterator++) != 0) {
						master_->renameColumn(field->change, field->field_name);
						changed = true;
					}
				}
				if (alterFlags & Alter_info::ALTER_ADD_INDEX || alterFlags & Alter_info::ALTER_DROP_INDEX) {
					changed = true;
				}
				if (changed) {
					master_->toDisk();

					// Force flush to make sure transient partitions are up to date.
					//master_->forceFlushNoLock();
					TransientPartitions	transientPartitions;
					master_->getTransientPartitions( transientPartitions );
					master_->forceFlushNoLock( transientPartitions, true );

					uint	nb = transientPartitions.length();
					if (nb != 0) {
						flushedPartitions.resize(nb);
						for (uint i=0; i<nb; ++i) {
							flushedPartitions.append(transientPartitions[i]->getSerial());
						}
					}

					alterInfo.flags = 0;
				}
			}
			columnAlterSerial_ = master_->getColumnAlterSerial();

			// If table fields do not match master's columns, this is a temporary table opened for alteration:
			// do not try to build fields and record readers.
			const Columns& columns = master_->getColumns();
			uint32_t j = 0;
			for (uint32_t i = 0; i < columns.length(); ++i) {
				const Column& column = columns[i];
				if (column.isDropped()) {
					continue;
				}
				if (j == table->s->fields) {
					return;
				}
				const Str name(table->field[j++]->field_name, false);
				if (name.compareTo(column.getName(), true) != 0) {
					return;
				}
			}
			if (j < table->s->fields) {
				return;
			}

			// Create fields.
			const uint32_t serial = master_->getColumnAlterSerial();
			FieldBase::createFields(serial, false, table->field, columns, fields_);
			for (uint32_t i = 0; i < fields_.length(); ++i) {
				FieldBase* field = fields_[i];
				if (field != 0 && field->isMapped()) {
					mappedFields_.append(field);
				}
			}

			// Create record readers. The first record reader is for the SPD files, the following ones are for the 
			//	index files (the nodes in the index files contain the values of the indexed columns). 
			const Indexes& indexes = master_->getIndexes();
			const uint32_t nbIndexes = indexes.length();
			recordWrappers_.resize(1 + nbIndexes * 2);
			recordWrappers_.append(RecordWrapper(fields_, 0, false));
			for (uint32_t i = 0; i < nbIndexes; ++i) {
				const ColumnIds* columnIds = &indexes[i].getColumnIds();
				recordWrappers_.append(RecordWrapper(fields_, columnIds, false));
				recordWrappers_.append(RecordWrapper(fields_, columnIds, true));
			}
		}
		if (changed) {
			// Wait until flush is done.
			master_->waitForFlush(flushedPartitions);
		}
	}
}

SerialRecordWrapper* TableShare::createSerialRecordWrapper(TABLE& table, const uint32_t serial, const uint32_t index, const bool tree) const {
	TableFieldsGuard fieldsGuard;
	TableFields& fields = fieldsGuard.get();
	ReadGuard guard(master_->getLock());
	FieldBase::createFields(serial, false, table.field, master_->getColumns(), fields);
	return new SerialRecordWrapper(serial, index, tree, fields, index == DATA_FILE ? 0 : &master_->getIndexes()[index].getColumnIds());
}

PartSerialRecordWrapper* TableShare::createPartSerialRecordWrapper(TABLE& table, const uint32_t alterSerial, const uint32_t partSerial, const ColumnIds& skippedColumnIds) const {
	TableFields fields;
	ReadGuard guard(master_->getLock());
	FieldBase::createFields(alterSerial, false, table.field, master_->getColumns(), fields, &skippedColumnIds);
	return new PartSerialRecordWrapper(partSerial, fields);
}

TableShare::~TableShare() {
	if (!key_) {
		mappedFields_.clear();
		fields_.clearAndDestroy();
		thr_lock_delete(&tableLock_);
	}
}

// STATIC
TableShare* TableShare::acquire(const Str& databaseName, const Str& tableName, TABLE* table, THR_LOCK_DATA* lockData) _THROW_(SparrowException) {
	const TableShare key(databaseName, tableName, 0);
	TableShare* share = 0;
	{
		Guard guard(lock_);
		share = hash_.find(&key);
		if (share == 0) {
			share = new TableShare(databaseName, tableName, table);
			hash_.insert(share);
		}
	}
	thr_lock_data_init(&share->tableLock_, lockData, 0);
	share->acquireRef();
	return share;
}

// STATIC
void TableShare::release(TableShare* share) {
	if (share->releaseRef()) {
		Guard guard(lock_);
		hash_.remove(share);
		delete share;
	}
}

}
