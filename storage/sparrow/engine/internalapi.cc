/*
	Internal API.
*/

#define MYSQL_SERVER 1
#include "../handler/plugin.h"		// For configuration parameters.
#include "internalapi.h"
#include "master.h"
#include "fileutil.h"
#include "transient.h"
#include "coalescing.h"
#include "purge.h"

#include "mysql.h"
#include "sql/mysqld.h"
#include "sql/current_thd.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// InternalApi
//////////////////////////////////////////////////////////////////////////////////////////////////////

SYSpHash<Master> InternalApi::hash_(64);
Lock InternalApi::hashLock_(true, "InternalApi::hashLock_");
volatile uint32_t InternalApi::flushOnGoing_ = 0;

// Setup: lists all master files by scanning data directory for spm files.
// STATIC
void InternalApi::setup() _THROW_(SparrowException) {
	SPARROW_ENTER("InternalApi::setup");
	Files files;
	FileUtil::scanDirectory(mysql_real_data_home, ".spm", 2, files, true);
	SYSslistIterator<Str> iterator(files);
	while (++iterator) {
		const Str& file = iterator.key();
		const Str database = FileUtil::getDatabaseName(file.c_str());
		const Str table = FileUtil::getTableName(file.c_str());
		InternalApi::get(database.c_str(), table.c_str(), true, false, 0);
	}

	// Make room if necessary.
	Purge::wakeUp();
	Scheduler::addTask(new PurgeTask());
}

typedef Entry<Str, ColumnEx> NamedColumn;
typedef SYSvector<NamedColumn> NamedColumns;
typedef Pair<Str, Str> AddedColumn;
typedef SYSvector<AddedColumn, 16> AddedColumns;

// Initializes table.
// STATIC
void InternalApi::init(const char* username, const char* password, const char* database, const char* table,
	const ColumnExs& columns, const Indexes& indexes, const ForeignKeys& foreignKeys, const DnsConfiguration& dnsConfiguration,
	const uint32_t aggregationPeriod, const uint64_t defaultWhere, const uint64_t stringOptimization, 
	const uint64_t maxLifetime, const uint64_t coalescingPeriod) _THROW_(SparrowException) {
	SPARROW_ENTER("InternalApi::init");
	char tmp[16 * 1024];
	bool create = false;
	Str s;
	{
		MasterGuard master;
		{
			Guard guard(hashLock_);
			const Master key(database, table, true);
			master = hash_.find(&key);
		}
		if (master == 0) {
			create = true;
		} else {
			// The table already exists. Check if the definition has changed. Get the table definition from MySQL.
			Str ldatabase(database);
			ldatabase.toLower();
			Str ltable(table);
			ltable.toLower();
			MySQLGuard mysql(username, password);
			snprintf(tmp, sizeof(tmp), "select column_name as name,"
				" data_type as type, character_maximum_length as length, datetime_precision"
				" from information_schema.columns"
				" where table_schema='%s' and table_name='%s'"
				" order by ordinal_position", ldatabase.c_str(), ltable.c_str());
			mysql.execute(tmp);

			ReadGuard guard(master->getLock());
			const Columns& masterColumns = master->getColumns();
			const Indexes& masterIndexes = master->getIndexes();
#ifndef NDEBUG
			DBUG_PRINT("sparrow_api", ("Table %s.%s already exists", database, table));
			DBUG_PRINT("sparrow_api", ("Current columns:"));
			for (uint32_t i = 0; i < masterColumns.length(); ++i) {
				const Column& column = masterColumns[i];
				if (column.isDropped()) {
					continue;
				}
				DBUG_PRINT("sparrow_api", ("Column %u: %s - %s", i, column.getName().c_str(), ColumnEx::getSqlType(column.getType())));
			}
			DBUG_PRINT("sparrow_api", ("Current indexes:"));
			for (uint32_t i = 0; i < masterIndexes.length(); ++i) {
				const Index& index = masterIndexes[i];
				if (index.isDropped()) {
					continue;
				}
				Str scolumns;
				const ColumnIds& ids = index.getColumnIds();
				for (uint32_t j = 0; j < ids.length(); ++j) {
					if (j > 0) {
						scolumns += Str(", ");
					}
					scolumns += masterColumns[ids[j]].getName();
				}
				DBUG_PRINT("sparrow_api", ("Index %u: %s - %s", i, index.getName().c_str(), scolumns.c_str()));
			}
			DBUG_PRINT("sparrow_api", ("New columns:"));
			for (uint32_t i = 0; i < columns.length(); ++i) {
				const Column& column = columns[i];
				DBUG_PRINT("sparrow_api", ("Column %u: %s - %s", i, column.getName().c_str(), ColumnEx::getSqlType(column.getType())));
			}
			DBUG_PRINT("sparrow_api", ("New indexes:"));
			for (uint32_t i = 0; i < indexes.length(); ++i) {
				const Index& index = indexes[i];
				Str scolumns;
				const ColumnIds& ids = index.getColumnIds();
				for (uint32_t j = 0; j < ids.length(); ++j) {
					if (j > 0) {
						scolumns += Str(", ");
					}
					scolumns += columns[ids[j]].getName();
				}
				DBUG_PRINT("sparrow_api", ("Index %u: %s - %s", i, index.getName().c_str(), scolumns.c_str()));
			}
#endif
			NamedColumns newColumnsByName(columns.length());
			for (uint32_t i = 0; i < columns.length(); ++i) {
				const ColumnEx& column = columns[i];
				newColumnsByName.append(NamedColumn(column.getName(), column));
			}
			Names existingColumns;
			Names droppedColumns;
			Names indexesToDrop;

			// Process result from query executed above
			MYSQL_RES* result = mysql.get();
			MYSQL_ROW row;
			bool first = true;
			while ((row = mysql_fetch_row(result)) != 0) {
				int i = 0;
				const Str name(row[i++]);
				const Str stype(row[i++]);
				const char* l = row[i++];
				const uint32_t length = l == 0 ? 0 : static_cast<uint32_t>(atoi(l));
				const char* m = row[i++];
				const uint64_t datetime_precision = m == 0 ? 0 : static_cast<uint64_t>(atol(m));
				existingColumns.append(name);
				const uint32_t index = newColumnsByName.index(NamedColumn(name));
				if (index == SYS_NPOS) {
					if (!droppedColumns.contains(name)) {
						droppedColumns.append(name);
					}
				} else {
					const ColumnEx& column = newColumnsByName[index].getValue();
					const ColumnType type = column.getType();

					// Take advantage of having the column definition from MySQL to check information consistency with column definition from Sparrow
					if (type == COL_TIMESTAMP) {
						const uint32_t	mast_col_indx = master->getColumn(name);
						if (mast_col_indx != SYS_NPOS) {		// Actually, it should never be SYS_NPOS. Otherwise, it would mean there's a big pb. 
							Column&	column = master->getColumns()[mast_col_indx];
							if (column.getInfo() != datetime_precision) {
								spw_print_information("Decimal precision of timestamp column for table %s.%s has been adjusted to 3.", database, table);
								column.setInfo(datetime_precision);
							}
						}
					}

					bool alter_col = false;
					if ((type == COL_BLOB || type == COL_STRING)
						&& stype.compareTo(Str(ColumnEx::getSqlType(type)), false) == 0
						&& length != column.getStringSize()) {
						alter_col = true;
					} if (type == COL_TIMESTAMP && column.getInfo() != UINT_MAX) {
						// Timestamp precision can be passed either through the column's info parameter or through its string size parameter. So check both.
						uint32_t	decimals = column.getInfo() != 0 ? column.getInfo() : column.getStringSize();
						if (datetime_precision != decimals) {
							if ( decimals > 6 ) {
								spw_print_warning("Timestamp precision is out of range (%u, %u) in declaration of column %s for table %s.%s. "
									"Timestamp decimal precision must be between 0 and 6.", column.getInfo(), column.getStringSize(), name.c_str(), database, table);
							} else {
								spw_print_information("Timestamp precision has changed (%u, %u) vs %u in declaration of column %s for table %s.%s.", 
									column.getInfo(), column.getStringSize(), (uint)datetime_precision, name.c_str(), database, table);
								alter_col = true;
							}
						//} else {
						//	spw_print_warning("Timestamp precision is the same (%u, %u) vs %u in declaration of column %s for table %s.%s.", 
						//		column.getInfo(), column.getStringSize(), (uint)datetime_precision, name.c_str(), database, table);
						}
					}
					if ( alter_col ) {
						if (first) {
							snprintf(tmp, sizeof(tmp), "alter table `%s`.`%s` ", database, table);
							s += Str(tmp);
						} else {
							s += Str(", ");
						}
						first = false;
						const Str definition(column.getDefinition());
						snprintf(tmp, sizeof(tmp), "modify column %s", definition.c_str());
						s += Str(tmp);
					}
				}
			}

			// Detect indexes referencing dropped columns.
			for (uint32_t i = 0; i < masterIndexes.length(); ++i) {
				const Index& index = masterIndexes[i];
				if (index.isDropped()) {
					continue;
				}
				const ColumnIds& ids = index.getColumnIds();
				for (uint32_t j = 0; j < ids.length(); ++j) {
					const Str& columnName = masterColumns[ids[j]].getName();
					if (droppedColumns.contains(columnName)) {
						indexesToDrop.append(index.getName());
						break;
					}
				}
			}

			// Detect added columns.
			AddedColumns addedColumns;
			Str iafter;
			for (uint32_t i = 0; i < newColumnsByName.length(); ++i) {
				const NamedColumn& namedColumn = newColumnsByName[i];
				const Str& name = namedColumn.getKey();
				if (!existingColumns.contains(name)) {
					addedColumns.append(AddedColumn(iafter, name));
				}
				iafter = name;
			}
			// Check our detected dropped and added columns are correct: simulate dropping the columns, and adding the new ones,
			//	then compare the result with the list of columns passed as argument.
			if (!droppedColumns.isEmpty() || !addedColumns.isEmpty()) {
				Names simulation(existingColumns);
				for (uint32_t i = 0; i < droppedColumns.length(); ++i) {
					simulation.remove(droppedColumns[i]);
				}
				for (uint32_t i = 0; i < addedColumns.length(); ++i) {
					const AddedColumn& addedColumn = addedColumns[i];
					const Str& after = addedColumn.getFirst();
					const Str& name = addedColumn.getSecond();
					const uint32_t index = after.isEmpty() ? 0 : (simulation.index(after) + 1);
					simulation.insertAt(index, name);
				}
				bool equals = true;
				if (simulation.length() == columns.length()) {
					for (uint32_t i = 0; i < simulation.length(); ++i) {
						if (simulation[i] != columns[i].getName()) {
							equals = false;
							break;
						}
					}
				} else {
					equals = false;
				}
				// If the result of our simulation was successful, generate the SQL queries corresponding to these changes.
				//	Start by the dropped indexes, then the dropped columns. Finally create the new columns.
				if (equals) {
					bool first = true;
					for (uint32_t i = 0; i < indexesToDrop.length(); ++i) {
						const Str& name = indexesToDrop[i];
						if (first) {
							snprintf(tmp, sizeof(tmp), "; alter table `%s`.`%s` ", database, table);
							s += Str(tmp);
						} else {
							s += Str(", ");
						}
						first = false;
						snprintf(tmp, sizeof(tmp), "drop index `%s`", name.c_str());
						s += Str(tmp);
					}
					first = true;
					for (uint32_t i = 0; i < droppedColumns.length(); ++i) {
						const Str& name = droppedColumns[i];
						if (first) {
							snprintf(tmp, sizeof(tmp), "; alter table `%s`.`%s` ", database, table);
							s += Str(tmp);
						} else {
							s += Str(", ");
						}
						first = false;
						snprintf(tmp, sizeof(tmp), "drop column `%s`", name.c_str());
						s += Str(tmp);
					}
					first = true;
					for (uint32_t i = 0; i < addedColumns.length(); ++i) {
						const AddedColumn& addedColumn = addedColumns[i];
						if (first) {
							snprintf(tmp, sizeof(tmp), "; alter table `%s`.`%s` ", database, table);
							s += Str(tmp);
						} else {
							s += Str(", ");
						}
						first = false;
						const Str& after = addedColumn.getFirst();
						const NamedColumn key(addedColumn.getSecond());
						const uint32_t index = newColumnsByName.index(key);
						const Str definition = newColumnsByName[index].getValue().getDefinition();
						Str position;
						if (after.isEmpty()) {
							position = Str("first");
						} else {
							snprintf(tmp, sizeof(tmp), "after `%s`", after.c_str());
							position = Str(tmp);
						}
						snprintf(tmp, sizeof(tmp), "add column %s %s", definition.c_str(), position.c_str());
						s += Str(tmp);
					}
				}
			}
		}
	}
	while (true) {
		// If the table does not exists, start by creating it officially through MySQL, using DDL (Data Definition Language)
		if (create) {
			MySQLGuard mysql(username, password);
			snprintf(tmp, sizeof(tmp), "create database if not exists `%s`"
				" default character set utf8mb4 collate utf8mb4_0900_ai_ci", database);
			mysql.execute(tmp);
			snprintf(tmp, sizeof(tmp), "create table if not exists `%s`.`%s` (", database, table);
			s = Str(tmp);
			bool first = true;
			for (uint32_t i = 0; i < columns.length(); ++i) {
				const ColumnEx& column = columns[i];
				if (first) {
					first = false;
				} else {
					s += Str(", ");
				}
				const ForeignKey* fk = 0;
				for (uint32_t j = 0; j < foreignKeys.length(); ++j) {
					const ForeignKey& foreignKey = foreignKeys[j];
					if (static_cast<uint32_t>(foreignKey.getColumnId()) == i) {
						fk = &foreignKey;
						break;
					}
				}
				s += column.getDefinition();
				if (fk != 0) {
					snprintf(tmp, sizeof(tmp), " references `%s`.`%s`(`%s`)", fk->getDatabaseName().isEmpty() ? database : fk->getDatabaseName().c_str(),
						fk->getTableName().c_str(), fk->getColumnName().c_str());
					s += Str(tmp);
				}
			}
			for (uint32_t i = 0; i < indexes.length(); ++i) {
				const Index& index = indexes[i];
				snprintf(tmp, sizeof(tmp), ", %skey `index_%u` (", index.isUnique() ? "unique " : "", i);
				s += Str(tmp);
				first = true;
				const ColumnIds& ids = index.getColumnIds();
				for (uint32_t j = 0; j < ids.length(); ++j) {
					const ColumnEx& column = columns[ids[j]];
					snprintf(tmp, sizeof(tmp), "%s`%s`", first ? "" : ", ", column.getName().c_str());
					first = false;
					s += Str(tmp);
				}
				s += Str(")");
			}
			s += Str(") engine=sparrow");
		}
		if (!s.isEmpty()) {
			if (create) {
				spw_print_information("Creating table %s.%s : %s", database, table, s.c_str());
			} else {
				spw_print_information("Definition of table %s.%s has changed: %s", database, table, s.c_str());
			}
			char* dummy;
			char* token = my_strtok_r(const_cast<char*>(s.c_str()), ";", &dummy);
			// Execute each SQL statement separately. SQL statements are separated by ';'
			while (token != 0) {
				MySQLGuard mysql(username, password);
				mysql.execute(token);
				token = my_strtok_r(0, ";", &dummy);
			}
		}

		// Update what cannot be changed through DDL: columns, foreign keys and DNS.
		if (update(username, password, database, table, columns, indexes, foreignKeys, dnsConfiguration, aggregationPeriod,
			defaultWhere, stringOptimization, maxLifetime, coalescingPeriod)) {
			break;
		} else {
			// Retry creation in case the table has been dropped or renamed because it is incompatible.
			create = true;
		}
	}
}

// Update table if necessary.
// STATIC
bool InternalApi::update(const char* username, const char* password, const char* database, const char* table,
	const ColumnExs& columns, const Indexes& indexes, const ForeignKeys& foreignKeys, const DnsConfiguration& dnsConfiguration,
	const uint32_t aggregationPeriod, const uint64_t defaultWhere, const uint64_t stringOptimization, 
	const uint64_t maxLifetime, const uint64_t coalescingPeriod) _THROW_(SparrowException) {
	SPARROW_ENTER("InternalApi::update");
	bool incompatible = false;
	{
		MasterKeepAlive master = InternalApi::get(database, table, false, false, 0);
		Indexes addedIndexes(indexes.length());
		Indexes droppedIndexes;
		Columns masterColumns;
		SYSvector<uint32_t> ids;	// Ids used by index names, if index names are formatted as "index_xxx".
		{
			WriteGuard guard(master->getLock());

			// Update indexes so they reference the actual columns (take care of dropped columns).
			for (uint32_t i = 0; i < indexes.length(); ++i) {
				const Index& index = indexes[i];
				const Index newIndex(index.getName().c_str(), master->updateColumnIds(index.getColumnIds(), columns), index.isUnique());
				addedIndexes.append(newIndex);
			}

			// Check column definitions are compatible.
			if (master->compareColumns(columns)) {
				check(columns, dnsConfiguration);

				// Update column definitions.
				master->updateColumns(columns);
				masterColumns = master->getColumns();

				// Check index changes.
				const Indexes& masterIndexes = master->getIndexes();
				for (uint32_t i = 0; i < masterIndexes.length(); ++i) {
					const Index& masterIndex = masterIndexes[i];
					if (!masterIndex.isDropped() && !addedIndexes.remove(masterIndex)) {
						droppedIndexes.append(masterIndex);
					} else {
						uint32_t id = 0;
						if (sscanf(masterIndex.getName().c_str(), "index_%u", &id) == 1) {
							ids.append(id);
						}
					}
				}
				// Make sure indexes have distinct names.
				for (uint32_t i = 0; i < addedIndexes.length(); ++i) {
					uint32_t id = 0;
					Index& addedIndex = addedIndexes[i];
					if (sscanf(addedIndex.getName().c_str(), "index_%u", &id) == 1) {
						if (ids.contains(id)) {
							id = 0;
							while (ids.contains(id)) {
								id++;
							}
							char buffer[128];
							snprintf(buffer, sizeof(buffer), "index_%u", id);
							addedIndex.setName(Str(buffer));
						}
						ids.append(id);
					}
				}
				master->setForeignKeys(foreignKeys);
				master->setAggregationPeriod(aggregationPeriod);
				master->setDefaultWhere(defaultWhere);
				master->setStringOptimization(stringOptimization);
				master->setMaxLifetime(maxLifetime);
				master->setCoalescingPeriod(coalescingPeriod);
				master->toDisk();
			} else {
				incompatible = true;
			}
		}

		if (!incompatible) {
			// Start coalescing in case coalescing period changed.
			master->coalesce();

			// Alter indexes if necessary.
			if (!addedIndexes.isEmpty() || !droppedIndexes.isEmpty()) {
				MySQLGuard mysql(username, password);
				char buffer[2048];
				snprintf(buffer, sizeof(buffer), "alter table `%s`.`%s` ", database, table);
				Str sql(buffer);
				bool first = true;
				for (uint32_t i = 0; i < droppedIndexes.length(); ++i) {
					snprintf(buffer, sizeof(buffer), "%sdrop index `%s`", (first ? "" : ", "), droppedIndexes[i].getName().c_str());
					sql += Str(buffer);
					first = false;
				}
				for (uint32_t i = 0; i < addedIndexes.length(); ++i) {
					const Index& addedIndex = addedIndexes[i];
					snprintf(buffer, sizeof(buffer), "%sadd index `%s`", (first ? "" : ", "), addedIndex.getName().c_str());
					sql += Str(buffer);
					const ColumnIds& columnIds = addedIndex.getColumnIds();
					for (uint32_t j = 0; j < columnIds.length(); ++j) {
						const Column& column = masterColumns[columnIds[j]];
						snprintf(buffer, sizeof(buffer), "%s`%s`%s", (j == 0 ? "(" : ", "),
							column.getName().c_str(), (j + 1 == columnIds.length() ? ")" : ""));
						sql += Str(buffer);
					}
					first = false;
				}
				mysql.execute(sql.c_str());
			}

			// Setup DNS configuration.
			TransientPartitions partitions;
			DnsConfigurationGuard dnsGuard;
			{
				WriteGuard guard(master->getLock());
				partitions = master->setDnsConfiguration(dnsConfiguration);
				dnsGuard = master->getDnsConfiguration();
				master->toDisk();
			}
			for (uint32_t i = 0; i < partitions.length(); ++i) {
				partitions[i]->updateDnsConfiguration(dnsGuard.get());
			}
		}
	}

	// If the table already exists but does not fit existing definition: drop it or rename it.
	// Take care to do it with the MasterKeepAlive released, otherwise table deletion will block.
	if (incompatible) {
		MySQLGuard mysql(username, password);
		char buffer[2048];
		if (sparrow_incompatible_table == 0) {
			spw_print_warning("Table %s.%s is incompatible with new definition: drop table", database, table);
			snprintf(buffer, sizeof(buffer), "drop table if exists `%s`.`%s`", database, table);
		} else {
			time_t ts = std::time(nullptr);
			struct tm t;
			gmtime_r(&ts, &t);
			char newTable[1024];
			int l = snprintf(newTable, sizeof(newTable), "%s", table);
			if (l == 0 || strftime(newTable + l, sizeof(newTable) - l, "%Y%m%d%H%M%S", &t) == 0) {
				throw SparrowException::create(false, "Cannot rename table %s.%s; internal error.",	database, table);
			}
			spw_print_warning("Table %s.%sis incompatible with new definition: rename table to %s.%s", database, table, database, newTable);
			snprintf(buffer, sizeof(buffer), "rename table `%s`.`%s` to `%s`.`%s`", database, table, database, newTable);
		}
		mysql.execute(buffer);
		snprintf(buffer, sizeof(buffer), "Table %s.%s has been %s", database, table, (sparrow_incompatible_table == 0 ? "dropped" : "renamed"));
		return false;
	} else {
		return true;
	}	
}

// STATIC
void InternalApi::check(const ColumnExs& columns, const DnsConfiguration& dnsConfiguration) _THROW_(SparrowException) {
	SPARROW_ENTER("InternalApi::check");
	bool hasDnsIdentifier = false;
	bool hasReverseDns = false;
	for (uint32_t i = 0; i < columns.length(); ++i) {
		const Column& column = columns[i];
		if (column.isFlagSet(COL_IP_LOOKUP)) {
			if (column.getType() != COL_STRING) {
				throw SparrowException::create(false, "Column \"%s\" must be a string since it contains ip address lookups",
					column.getName().c_str());
			}
			const uint32_t index = column.getInfo();
			if (index >= columns.length()) {
				throw SparrowException::create(false, "Cannot find ip address column referenced by lookup column \"%s\"",
					column.getName().c_str());
			}
			const Column& ip = columns[index];
			if (ip.getType() != COL_BLOB || !ip.isFlagSet(COL_IP_ADDRESS)) {
				throw SparrowException::create(false, "Column \"%s\" contains reverse ip lookups of values from column \"%s\", but this column does not contain ip addresses",
					column.getName().c_str(), ip.getName().c_str());
			}
			if (column.isFlagSet(COL_NULLABLE) && !ip.isFlagSet(COL_NULLABLE)) {
				throw SparrowException::create(false, "Nullable column \"%s\" contains ip lookups of values from column \"%s\", but this column is not nullable",
					column.getName().c_str(), ip.getName().c_str());
			}
			hasReverseDns = true;
		}
		if (column.isFlagSet(COL_DNS_IDENTIFIER)) {
			if (hasDnsIdentifier) {
				throw SparrowException::create(false, "Only one column can be the DNS identifier");
			}
			hasDnsIdentifier = true;
		}
	}
	if (hasReverseDns && !dnsConfiguration.isEmpty()) {
		if (!hasDnsIdentifier && (dnsConfiguration.entries() > 1 || !dnsConfiguration.contains(DnsConfigId(-1)))) {
			throw SparrowException::create(false, "At least one column contains ip lookups and there is no DNS identifier column - in this case, the DNS configuration can contain only the wildcard identifier");
		}
	}
}

// Gets a master file.
// STATIC
MasterKeepAlive InternalApi::get(const char* database, const char* table,
	const bool create, const bool remove, TABLE_SHARE* s) _THROW_(SparrowException) {
	SPARROW_ENTER("InternalApi::get");
	Guard guard(hashLock_);
	const Master key(database, table, true);
	Master* master = hash_.find(&key);
	const bool inHash = master != 0;
	if (master == 0 && create) {
		// Not found in memory: maybe on disk?
		master = Master::fromDisk(database, table, s);
	}
	if (master == 0) {
		if (create) {
			// Not found in memory nor on disk: create it in memory.
			DBUG_PRINT("sparrow_api", ("Create new master %s.%s", database, table));
			master = new Master(database, table, false);
		} else {
			throw SparrowException::create(false, "Cannot find master file for table %s.%s", database, table);
		}
	}
	if (remove) {
		if (inHash) {
			DBUG_PRINT("sparrow_api", ("Remove master %s.%s from hash", database, table));
			hash_.remove(master);
		}
	} else if (!inHash && master != 0) {
		DBUG_PRINT("sparrow_api", ("Insert master %s.%s into hash", database, table));
		hash_.insert(master);

		// Start alterations if necessary.
		master->startIndexAlter(false);

		// Start coalescing if necessary.
		master->coalesce();
	}
	return MasterKeepAlive(master);
}

// Gets all master files currently in the hash table.
// STATIC
Masters InternalApi::getAll() {
	SPARROW_ENTER("InternalApi::getAll");
	Guard guard(hashLock_);
	Masters masters(hash_.entries());
	SYSpHashIterator<Master> iterator(hash_);
	while (++iterator) {
		masters.insert(MasterKeepAlive(iterator.key()));
	}
	return masters;
}

// Flushes the transient partition of all master files.
// STATIC
void InternalApi::flushAll(const bool shutdown) {
	SPARROW_ENTER("InternalApi::getAll");

	// If a InternalApi::flushAll() is already being processed somewhere in another thread
	//	drop this one
	if (!Atomic::cas32(&InternalApi::flushOnGoing_, 0, 1)) {
		return;
	}
	// If transient partitions are already being flushed, no need to force another "flush all" job
	if ( TransientPartition::getNbFlushs() > 0 ) {
		Atomic::dec32(&InternalApi::flushOnGoing_);
		return;
	}

	Masters masters = InternalApi::getAll();
	bool flush = false;
	for (uint32_t i = 0; i < masters.length(); ++i) {
		if (masters[i]->forceFlush()) {
			flush = true;
		}
	}

	Atomic::dec32(&InternalApi::flushOnGoing_);

	if (shutdown) {
		if (flush) {
			spw_print_information("Sparrow is flushing transient data to disk...");
		}
		TransientPartition::waitForFlushs();
	}
}

// STATIC
// Stops the pending coalescing tasks
void InternalApi::StopCoalescingTasks(const char* schema) {
	bool		all = (schema == NULL || strlen(schema) == 0);
	Masters		masters = InternalApi::getAll();
	const uint32_t nbMasters = masters.length();
	for (uint32_t i=0; i<nbMasters; ++i) {
		Master&		master = *masters[i];
		if (all || strcmp(master.getDatabase().c_str(), schema) == 0) {
			master.stopCoalescingTasks();
		}
	}
}

// Renames a table.
// STATIC
void InternalApi::rename(const char* database, const char* table,
	const char* newDatabase, const char* newTable) _THROW_(SparrowException) {
	SPARROW_ENTER("InternalApi::rename");
	MasterKeepAlive master = InternalApi::get(database, table, false, true, 0);
	{
		WriteGuard guard(master->getLock());
		master->rename(newDatabase, newTable);
	}

	// Put the master file back in the hash (hash code changed).
	Guard guard(hashLock_);
	hash_.insert(master.get());
}

// Writes data to a Sparrow table.
// STATIC
void InternalApi::write(const char* database, const char* table, ByteBuffer& buffer, const uint32_t rows) _THROW_(SparrowException) {
	SPARROW_ENTER("InternalApi::write");
	DBUG_PRINT("sparrow_api", ("Inserting %u rows into table %s.%s", rows, database, table));

	// Find master file for given table.
	MasterKeepAlive master = InternalApi::get(database, table, false, false, 0);
	uint64_t	timestamp = 0;
	while (true) {
		if (TransientPartition::waitForRoom(master.isStopping())) {
			// Get the current transient partition, or create a new one if necessary.
			TransientPartitionGuard partition = master->getTransientPartition( timestamp );

			// Insert data.
			if (partition->insert(buffer, rows, timestamp)) {
				// Insertion succeeded.
				break;
			}
		} else {
			throw SparrowException::create(false, "Insertion aborted because table %s.%s is being deleted", database, table);
		}
		// The partition was full or a timestamp was out of the coalescing period: retry.
	}
}

// Writes data to a Sparrow table using a subset of columns. 
// STATIC
void InternalApi::write(const char* database, const char* table, const Names& colNames, ByteBuffer& buffer, const uint32_t rows) _THROW_(SparrowException) {
	SPARROW_ENTER("InternalApi::write");
	DBUG_PRINT("sparrow_api", ("Inserting %u rows into table %s.%s on a selection of %u columns", rows, database, table, colNames.entries()));

	// Find master file for given table.
	MasterKeepAlive master = InternalApi::get(database, table, false, false, 0);
	ColumnIds		colIds;
	master->getColumnIds(colNames, colIds);
	uint64_t	timestamp = 0;
	while (true) {
		if (TransientPartition::waitForRoom(master.isStopping())) {
			// Get the current transient partition, or create a new one if necessary.
			TransientPartitionGuard partition = master->getTransientPartition( timestamp );

			// Insert data.
			if (partition->insert(buffer, rows, colNames, colIds, timestamp)) {
				// Insertion succeeded.
				break;
			}
		} else {
			throw SparrowException::create(false, "Insertion aborted because table %s.%s is being deleted", database, table);
		}
		// The partition was full or a timestamp was out of the coalescing period: retry.
	}
}

// STATIC
void InternalApi::removePartitions(const char* database, const char* table, const TimePeriod& period) _THROW_(SparrowException) {
	SPARROW_ENTER("InternalApi::removePartitions");
#ifndef NDEBUG
	const Str sPeriod = Str::fromTimePeriod(period);
	DBUG_PRINT("sparrow_api", ("Removing partitions in interval %s from table %s.%s", sPeriod.c_str(), database, table));
#endif

	// Find master file for given table.
	MasterKeepAlive master = InternalApi::get(database, table, false, false, 0);
	master->removePartitions(period);
}

// Helper to print a grid. The parameter totalValues, if not null, gives the number of trailing values
// to be printed as a total on the last line.
// STATIC
void InternalApi::printGrid(PrintBuffer& buffer, const SYSvector<Str>& headers, SYSslist<Str>& values, const uint32_t totalValues) {
	SYSslist<Str> strings;
	SYSarray<int> lengths(headers.length(), 0);
	for (uint32_t i = 0; i < lengths.length(); ++i) {
		strings.append(headers[i]);
	}
	strings.appendAll(values);
	SYSslistIterator<Str> iterator(strings);
	uint32_t i = 0;
	while (++iterator) {
		uint32_t index = i++ % lengths.length();
		lengths[index] = std::max(lengths[index], iterator.key().length());
	}

	char separator[2048];
	Str totalSeparator;
	char* t = separator;
	for (i = 0; i < lengths.length(); ++i) {
		*t++ = '+';
		if (totalValues != 0 && i == totalValues) {
			totalSeparator = Str(separator, static_cast<int>(t - separator));
		}
		for (int j = 0; j < lengths[i] + 2; ++j) {
			*t++ = '-';
		}
	}
	*t++ = '+';
	if (totalValues != 0 && i == totalValues) {
		totalSeparator = Str(separator, static_cast<int>(t - separator));
	}
	*t++ = 0;

	// Header
	iterator.reset();
	buffer << separator << "\n";
	int pos = 0;
	for (i = 0; i < lengths.length(); ++i) {
		++iterator;
		++pos;
		const Str& s = iterator.key();
		buffer << "| " << s;
		for (int j = 0; j < lengths[i] - s.length(); ++j) {
			buffer << " ";
		}
		buffer << " ";
	}
	buffer << "|\n" << separator << "\n";
	i = 0;
	while (++iterator) {
		const Str& s = iterator.key();
		buffer << "| " << s;
		for (int j = 0; j < lengths[i] - s.length(); ++j) {
			buffer << " ";
		}
		buffer << " ";
		i++;
		++pos;
		if (i == lengths.length()) {
			buffer << "|\n";
			if (totalValues > 0 && pos == static_cast<int>(strings.entries() - totalValues)) {
				buffer << separator << "\n";
			}
			i = 0;
		}
	}
	if (totalValues == 0) {
		buffer << separator << "\n";
	} else {
		if (totalValues < headers.length()) {
			buffer << "|\n";
		}
		buffer << totalSeparator << "\n";
	}
}

// STATIC
void InternalApi::reportAlterStatus(PrintBuffer& buffer, const SortedMasters& masters) {
	SPARROW_ENTER("InternalApi::reportAlterStatus");
	SYSslist<Str> strings;
	bool isAltering = false;
	for (uint32_t i = 0; i < masters.length(); ++i) {
		const Master& master = *masters[i];
		ReadGuard guard(master.getLock());
		if (master.getIndexAlterStatus(strings)) {
			isAltering = true;
		}
	}
	if (isAltering) {
		const char* h[] = { "Table", "Alteration", "Elapsed", "Left", "Progress" };
		SYSvector<Str> headers(sizeof(h) / sizeof(h[0]));
		for (uint32_t i = 0; i < headers.capacity(); ++i) {
			headers.append(Str(h[i]));
		}
		buffer << "\nOn going table alterations:\n\n";
		InternalApi::printGrid(buffer, headers, strings, 0);
	}
}

// STATIC
void InternalApi::reportCoalescingStatus(PrintBuffer& buffer, const SortedMasters& masters) {
	SPARROW_ENTER("InternalApi::reportCoalescingStatus");
	SYSslist<Str> strings;
	const char* h[] = { "Table", "Coalescing Period", "Progress" };
	SYSvector<Str> headers(sizeof(h) / sizeof(h[0]));
	for (uint32_t i = 0; i < headers.capacity(); ++i) {
		headers.append(Str(h[i]));
	}
	char tmp[1024];
	for (uint32_t i = 0; i < masters.length(); ++i) {
		const Master& master = *masters[i];
		ReadGuard masterGuard(master.getLock());
		if (master.getNewest(true) == 0) {
			continue;
		}
		snprintf(tmp, sizeof(tmp), "%s.%s", master.getDatabase().c_str(), master.getTable().c_str());
		strings.append(Str(tmp));
		strings.append(Str::fromDuration(master.getCoalescingPeriod()));
		const double coalescingPercentage = master.getCoalescingPercentage();
		if (coalescingPercentage < 0) {
			strings.append(Str("N/A"));
		} else {
			snprintf(tmp, sizeof(tmp), "%.1f%%", coalescingPercentage);
			strings.append(Str(tmp));
		}
	}
	buffer << "\nCoalescing status:\n\n";
	InternalApi::printGrid(buffer, headers, strings, 0);
}

// Report status of Sparrow tables.
// STATIC
void InternalApi::report(PrintBuffer& buffer) _THROW_(SparrowException) {
	SPARROW_ENTER("InternalApi::report");
	SortedMasters masters = InternalApi::getAll();
	if (masters.isEmpty()) {
		buffer << "\nThere are no Sparrow tables.\n";
		return;
	}
	buffer << "\nSparrow tables:\n\n";
	const char* h[] = { "Name", "Data", "Index", "Total", "Period", "Partitions", "Files", "Records", "Average", "Oldest", "Newest", "Lifetime (max)", "Avg row count" };
	SYSvector<Str> headers(sizeof(h) / sizeof(h[0]));
	for (uint32_t i = 0; i < headers.capacity(); ++i) {
		headers.append(Str(h[i]));
	}
	char tmp[1024];
	uint64_t totalData = 0;
	uint64_t totalIndex = 0;
	uint64_t projected = 0;	// Projected total size.
	uint64_t throughput = 0;	// Bytes per day.
	SYSslist<Str> strings;
	for (uint32_t i = 0; i < masters.length(); ++i) {
		const Master& master = *masters[i];
		ReadGuard masterGuard(master.getLock());
		const uint32_t partitions = master.getPartitions().length();
		if (partitions > 0) {
			snprintf(tmp, sizeof(tmp), "%s.%s", master.getDatabase().c_str(), master.getTable().c_str());
			strings.append(Str(tmp));
			strings.append(Str::fromSize(master.getDataSize()));
			totalData += master.getDataSize();
			strings.append(Str::fromSize(master.getIndexSize()));
			totalIndex += master.getIndexSize();
			const uint64_t size = master.getDataSize() + master.getIndexSize();
			strings.append(Str::fromSize(size));
			snprintf(tmp, sizeof(tmp), "%u", master.getAggregationPeriod());
			strings.append(Str(tmp));
			snprintf(tmp, sizeof(tmp), "%u", partitions);
			strings.append(Str(tmp));
			snprintf(tmp, sizeof(tmp), "%u", 1 + master.getIndexMappings().length());
			strings.append(Str(tmp));
			const uint64_t persistentRecords = master.getRecords();
			const uint64_t transientRecords = master.getTransientRecords();
			const uint64_t records = persistentRecords + transientRecords;
			snprintf(tmp, sizeof(tmp), "%llu", static_cast<ulonglong>(records));
			strings.append(Str(tmp));
			if (records == 0 || persistentRecords == 0) {
				strings.append(Str("N/A"));
			} else {
				snprintf(tmp, sizeof(tmp), "%llu", static_cast<ulonglong>(size / persistentRecords));
				strings.append(Str(tmp));
			}
			const uint64_t oldest = master.getOldest();
			strings.append(oldest == 0 ? Str("N/A") : Str::fromTimestamp(oldest));
			const uint64_t newest = master.getNewest();
			strings.append(newest == 0 ? Str("N/A") : Str::fromTimestamp(newest));
			const uint64_t lifetime = master.getAge();
			const Str slifetime(Str::fromDuration(lifetime));
			const uint64_t maxLifetime = master.getMaxLifetime();
			const Str smaxLifetime(Str::fromDuration(maxLifetime));
			snprintf(tmp, sizeof(tmp), "%s (%s)", slifetime.c_str(), smaxLifetime.c_str());
			strings.append(Str(tmp));
			uint64_t	aggLifetime;
			if (master.getAggregationPeriod() == 0) {
				aggLifetime = lifetime;
			} else {
				uint64_t	aggregationPeriod = static_cast<uint64_t>(master.getAggregationPeriod())*1000;
				uint64_t	oldest_rounded = (oldest/aggregationPeriod)*aggregationPeriod;
				uint64_t	newest_rounded = (newest/aggregationPeriod)*aggregationPeriod;
				uint64_t	lifetime_rounded = newest_rounded - oldest_rounded;
				aggLifetime = ((lifetime_rounded/(master.getAggregationPeriod()*1000))+1)*master.getAggregationPeriod()*1000;
			}
			
			if (aggLifetime == 0) {
				strings.append(Str("N/A"));
			} else {
				uint64_t	avgRowCount = records*master.getAggregationPeriod()*1000/aggLifetime;
				snprintf(tmp, sizeof(tmp), "%llu", static_cast<ulonglong>(avgRowCount));
				strings.append(Str(tmp));
			}
			if (aggLifetime != 0) {
				const uint64_t tableSize = master.getDataSize() + master.getIndexSize();
				double ratio = static_cast<double>(maxLifetime) / aggLifetime;
				projected += static_cast<uint64_t>(ratio * tableSize);
				ratio = 86400000.0 / aggLifetime;
				throughput += static_cast<uint64_t>(ratio * tableSize);
			}
		}
	}
	strings.append(Str("TOTAL"));
	strings.append(Str::fromSize(totalData));
	strings.append(Str::fromSize(totalIndex));
	if (sparrow_max_disk_size == 0) {
		strings.append(Str::fromSize(totalData + totalIndex));
	} else {
		snprintf(tmp, sizeof(tmp), "%s (%s)", Str::fromSize(totalData + totalIndex).c_str(),
			Str::fromSize(sparrow_max_disk_size).c_str());
		strings.append(Str(tmp));
	}
	InternalApi::printGrid(buffer, headers, strings, 4);

	if (projected != 0) {
		buffer << "\nProjected total size is " << Str::fromSize(projected) << ". Write throughput is " << Str::fromSize(throughput) << " per day.\n";
	}

	// Alteration status of all tables.
	InternalApi::reportAlterStatus(buffer, masters);

	// Coalescing status of all tables.
	InternalApi::reportCoalescingStatus(buffer, masters);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MySQLGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

MySQLGuard::MySQLGuard(const char* username, const char* password) _THROW_(SparrowException)
	: result_(0) {
	mysql_ = mysql_init(0);
	if (mysql_ == NULL) {
		throw SparrowException::create(false, "Failed to initialize connection handler.");
	}

	uint protocol = MYSQL_PROTOCOL_TCP;
	mysql_options(mysql_, MYSQL_OPT_PROTOCOL, &protocol);

	// Use big timeouts because some operations may be long (e.g. drop Sparrow table
	// may require deleting a lot of files.)
	uint big = 86400;
	mysql_options(mysql_, MYSQL_OPT_READ_TIMEOUT, &big);
	mysql_options(mysql_, MYSQL_OPT_WRITE_TIMEOUT, &big);
	if (mysql_real_connect(mysql_, 0, username, password, 0, mysqld_port, 0, 0) == 0) {
		MySQLGuard::check(mysql_, 0);
	}
}

void MySQLGuard::execute(const char* stmt) _THROW_(SparrowException) {
	SPARROW_ENTER("MySQLGuard::execute");
	DBUG_PRINT("sparrow_api", ("Statement: %s", stmt));
	if (mysql_real_query(mysql_, stmt, (uint)strlen(stmt)) != 0) {
		MySQLGuard::check(mysql_, stmt);
	}
	clear();
	result_ = mysql_store_result(mysql_);
}

MySQLGuard::~MySQLGuard() {
	clear();
	mysql_close(mysql_);
}

void MySQLGuard::clear() {
	if (result_ != 0) {
		mysql_free_result(result_);
		result_ = 0;
	}
}


// STATIC
void MySQLGuard::check(MYSQL* mysql, const char* stmt) _THROW_(SparrowException) {
	const char* sqlError = mysql_error(mysql);
	unsigned int err_code = mysql_errno(mysql);
	const char* msg = "unknown error";
	if (sqlError == 0 || strlen(sqlError) == 0) {
		uint e = mysql->net.last_errno;
		if (e != 0) {
			msg = ER_THD(current_thd, e);
		}
	} else {
		msg = sqlError;
	}
	
	if (stmt == 0) {
		SparrowException	e = SparrowException::create(false, "Cannot connect: %u, %s", err_code, msg);
		e.set_err_code( err_code );
		throw e;
	} else {
		// Truncate statement to 255 chars if necessary.
		char tstmt[256];
		strncpy(tstmt, stmt, sizeof(tstmt));
		tstmt[sizeof(tstmt) - 1] = 0;
		SparrowException	e = SparrowException::create(false, "Cannot execute \"%s\": %u, %s", tstmt, err_code, msg);
		e.set_err_code( err_code );
		throw e;
	}
}

}
