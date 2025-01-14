/*
	Table handler context.
*/

#define MYSQL_SERVER 1

#include "context.h"
#include "../handler/hasparrow.h"
#include "condition.h"
#include "persistent.h"
#include "transient.h"
#include "internalapi.h"

#include "sql/current_thd.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"

#include <algorithm>

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// QueryInfo
//////////////////////////////////////////////////////////////////////////////////////////////////////

void QueryInfo::clear(const bool resetContext) {
	index_ = DATA_FILE;
	dataMap_ = 0;
	keyBuffer_.clear();
	currentKey_ = KeyValue();
	minKey_ = KeyValue();
	maxKey_ = KeyValue();
	keyInfo_ = 0;
	indexHasTimestamps_ = false;
	if (resetContext) {
		snapshots_.clearAndDestroy();
	}
}

QueryInfo::~QueryInfo() {
	clear(true);
}

void QueryInfo::update(TableShare& share, TABLE& table, const uint32_t index) {
	clear(false);

	// In case of update, need to read all fields.
	const bool forUpdate = !bitmap_is_clear_all(table.write_set);
	mysqlIndex_ = index;
	const Master& master = share.getMaster();
	index_ = master.getIndexId(index);
	dataMap_ = 0;
	indexHasTimestamps_ = false;
	currentKey_ = KeyValue();
	minKey_ = KeyValue();
	maxKey_ = KeyValue();
	if (index_ != DATA_FILE) {
		keyInfo_ = table.key_info + index;

		// Check if requested fields are all present in index ("covering index").
		// In this case, no need to read the data file.
		MY_BITMAP* readSet = table.read_set;
		uint count = forUpdate ? table.s->fields : bitmap_bits_set(readSet);
		const ColumnIds& columnIds = master.getIndexes()[index_].getColumnIds();
		bool isCoveringIndex = false;
		if (columnIds.length() >= count) {
			const TableFields& fields = share.getFields();
			uint32_t f = 0;
			for (uint32_t i = 0; i < fields.length(); ++i) {
				const FieldBase* field = fields[i];
				if (field == 0 || !field->isMapped()) {
					continue;
				}
				if (bitmap_is_set(readSet, f)) {
					const uint32_t pos = columnIds.index(i);
					if (pos != SYS_NPOS) {
						dataMap_ |= (1 << pos);
						if (--count == 0) {
							isCoveringIndex = true;
							break;
						}
					}
				}
				++f;
			}
		}
		if (!isCoveringIndex) {
			dataMap_ = 0;
		}

		// Initialize flag for QueryInfo::stopOnFirstMatch().
		const Columns& columns = master.getColumns();
		for (uint32_t i = 0; i < columnIds.length(); ++i) {
			if (columns[columnIds[i]].getType() == COL_TIMESTAMP) {
				indexHasTimestamps_ = true;
			}
		}

		// Allocate enough space for temporary key values.
		const uint32_t keyLength = getKeyLength();
		keyBuffer_.resize(keyLength * 3);
		keyBuffer_.forceLength(keyLength * 3);
		uint8_t* keyBuffer = const_cast<uint8_t*>(keyBuffer_.data());
		currentKey_ = KeyValue(keyBuffer, HA_WHOLE_KEY);
		minKey_ = KeyValue(keyBuffer + keyLength, HA_WHOLE_KEY);
		maxKey_ = KeyValue(keyBuffer + keyLength * 2, HA_WHOLE_KEY);
	}
	updateIndirectors();
}

bool QueryInfo::snapshotTransientPartition(TransientPartition* partition) {
	PartitionSnapshot* snapshot = partition->snapshot();
	if (snapshot == 0) {
		return false;
	} else {
		assert(!snapshots_.contains(snapshot));
		snapshots_.insert(snapshot);
		return true;
	}
}

void QueryInfo::updateIndirectors() {
	SYSpHashIterator<PartitionSnapshot> iterator(snapshots_);
	while (++iterator) {
		iterator.key()->updateIndirector(index_);
	}
}

const PartitionSnapshot* QueryInfo::getSnapshot(const TransientPartition* partition) const {
	const PartitionSnapshot key(const_cast<TransientPartition*>(partition));
	const PartitionSnapshot* snapshot = snapshots_.find(&key);
	return snapshot;
}

// Compares keys passed as parameters.
int QueryInfo::compareKeys(const TableFields& fields, const KeyValue& leftKey, const KeyValue& rightKey) const {
	const KEY& keyInfo = getKeyInfo();
	const uint8_t* left = leftKey.getKey();
	const key_part_map leftMap = leftKey.getMap();
	const uint8_t* right = rightKey.getKey();
	const key_part_map rightMap = rightKey.getMap();
	for (uint32_t i = 0; i < keyInfo.user_defined_key_parts; ++i) {
		const uint32_t bit = 1 << i;
		if ((rightMap & bit) == 0 || (leftMap & bit) == 0) {
			continue;
		}
		const KEY_PART_INFO& keyPartInfo = keyInfo.key_part[i];
		const FieldBase& field = *fields[keyPartInfo.fieldnr - 1];
		int cmp = 0;
		if (keyPartInfo.null_bit != 0) {
			const bool leftIsNull = (*left++ != 0);
			const bool rightIsNull = (*right++ != 0);
			if (leftIsNull) {
				if (!rightIsNull) {
					cmp = -1;
				} else {
					cmp = 0;
				}
			} else {
				if (rightIsNull) {
					cmp = 1;
				} else {
					cmp = field.compare(left, right);
				}
			}
		} else {
			cmp = field.compare(left, right);
		}
		if (cmp != 0) {
			return cmp;
		}
		left += field.getLength(true);
		right += field.getLength(true);
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Context
//////////////////////////////////////////////////////////////////////////////////////////////////////

Context::Context() : table_(0), share_(0), loaded_(false), writing_(false), altered_(false),
	position_(UINT_MAX), indirectorIndex_(UINT_MAX), recordWrappers_(16), recordWrappersPerPartition_(16), insertRows_(0) {
	SPARROW_ENTER("Context::Context");
}

void Context::reset() {
	SPARROW_ENTER("Context::reset");
	partitions_.clear();
	mainPartitions_.clear();
	readers_.clear();
	positions_.clear();
	indirector_.clear();
	indirectorIndex_ = UINT_MAX;
	keyValues_.clear();
	queryInfo_.clear(true);
	loaded_ = false;
	unusableIndexes_.clear();
	updatableColumns_.clear();
	recordWrappers_.clearAndDestroy();
	recordWrappersPerPartition_.clearAndDestroy();
}

void Context::initialize(TABLE* table, TableShare* share) {
	SPARROW_ENTER("Context::initialize");
	table_ = table;
	share_ = share;
	reset();
	writing_ = false;
}

Context::~Context() {
	SPARROW_ENTER("Context::~Context");
	reset();
}

void Context::clone(const Context& context) {
	reset();
	if (context.loaded_) {
		partitions_ = context.partitions_;
		mainPartitions_ = context.mainPartitions_;
		readers_.initialize(context.partitions_.length());
		resetPosition();
		loaded_ = true;
		queryInfo_.updateIndirectors();
		unusableIndexes_ = context.unusableIndexes_;
		updatableColumns_ = context.updatableColumns_;
	}
}

void Context::resetPosition() {
	position_ = Position();
	const uint32_t n = partitions_.length();
	positions_.clear();
	positions_.resize(n);
	positions_.forceLength(n);
	indirector_.clear();
	indirector_.resize(n);
	indirector_.forceLength(n);
	indirectorIndex_ = UINT_MAX;
	const uint32_t length = queryInfo_.getKeyLength() * n;
	keyValues_.reshape(length);
}

PartitionReader* Context::getReader(const uint32_t partition, const uint32_t index, const bool isString, const BlockCacheHint& hint) _THROW_(SparrowException) {
	return readers_.get(partition, static_cast<PersistentPartition&>(*partitions_[partition].get()), index, isString, hint);
}

const RecordWrapper& Context::getRecordWrapper(const uint32_t alterSerial, const uint32_t index, const bool tree) {
	const TableShare& share = getShare();
	if (alterSerial == share.getColumnAlterSerial()) {
		return share.getRecordWrapper(index, tree);
	} else {
		const SerialRecordWrapper key(alterSerial, index, tree);
		SerialRecordWrapper* wrapper = recordWrappers_.find(&key);
		if (wrapper == 0) {
			wrapper = share.createSerialRecordWrapper(getTable(), alterSerial, index, tree);
			recordWrappers_.insert(wrapper);
		}
		return *wrapper;
	}
}

const RecordWrapper& Context::getRecordWrapper(const uint32_t alterSerial, const uint32_t partSerial, const ColumnIds& skippedColumnIds) {
	const TableShare& share = getShare();
	const PartSerialRecordWrapper key(partSerial);
	PartSerialRecordWrapper* wrapper = recordWrappersPerPartition_.find(&key);
	if (wrapper == 0) {
		wrapper = share.createPartSerialRecordWrapper(getTable(), alterSerial, partSerial, skippedColumnIds);
		recordWrappersPerPartition_.insert(wrapper);
	}
	return *wrapper;
}

// STATIC
Str Context::getQueryString() {
	THD* thd = current_thd;
	return Str(thd->query().str, static_cast<int>(thd->query().length));
}

void Context::loadTimePeriods( TimePeriods& periods ) {
	const Master& master = getShare().getMaster();
	uint64_t lower = 0;
	{
		// First get highest timestamp.
		ReadGuard guard(master.getLock());
		const uint64_t newest = master.getNewest();
		const uint64_t defaultWhere = master.getDefaultWhere();
		if (newest != 0 && defaultWhere != 0) {
			lower = newest - defaultWhere;
		}
	}

	THD* thd = current_thd;
	Item* cond = thd->lex->current_query_block()->where_cond();
	periods = Condition::getPeriods(table_, cond, lower);
}

/* Parses the current SQl statement to extract the time interval (if any) from the WHERE clause. 
	Deduce the covering list of partitions (put that list in partitions_): partition pruning mechanism.
	If the WHERE clause does not specify any time interval, use the defaultWhere as default time interval.
	That value is set at Master file level and set during table creation. If defaultWhere is 0, then look through all partitions.
	Set the boolean flag loaded_ to true so that this parsing won't be done again for this SQL statement.
	The SQL statement is found in the form of a pointer to a COND object in the MySQL THD structure 
	stored in the thread TLS, through thd->lex->current_select->prep_where
*/
void Context::load() {
	SPARROW_ENTER("Context::load");
	if (loaded_) {
		return;
	}
#ifndef NDEBUG
	const Str query(getQueryString());
	const char* s = query.c_str();
	const char* p = s;
	DBUG_LOCK;
	DBUG_PRINT("sparrow_context", ("Query:"));
	while (true) {
		if (*s == 0) {
			DBUG_PRINT("sparrow_context", ("%s", p));
			break;
		} else if (*s == '\n') {
			ptrdiff_t l = s - p;
			const Str tmp(p, static_cast<int>(l));
			DBUG_PRINT("sparrow_context", ("%s", tmp.c_str()));
			p = s + 1;
		}
		s++;
	}
#endif
	partitions_.clear();
	readers_.clear();
	TimePeriods periods;
	loadTimePeriods( periods );
	const Master& master = getShare().getMaster();
#ifndef NDEBUG
	Str speriods;
#endif
	{
		ReadGuard guard(master.getLock());

		// Transform time periods into a list of partitions.
		for (uint32_t i = 0; i < periods.length(); ++i) {
			const TimePeriod& period = periods[i];
			master.getPartitionsForTimePeriod(period, partitions_, queryInfo_);
#ifndef NDEBUG
			if (speriods.length() > 0) {
				speriods += Str(", ");
			}
			speriods += Str::fromTimePeriod(period);
#endif
		}

		// Get list of updatable columns: all except timestamp, strings and indexed columns.
		const Columns& columns = master.getColumns();
		for (uint32_t i = 0; i < columns.length(); ++i) {
			if (i != 0 && !columns[i].isString()) {
				updatableColumns_.append(i);
			}
		}
		const Indexes& indexes = master.getIndexes();
		for (uint32_t i = 0; i < indexes.length(); ++i) {
			const ColumnIds& ids = indexes[i].getColumnIds();
			for (uint32_t j = 0; j < ids.length(); ++j) {
				updatableColumns_.remove(ids[j]);
			}
		}

		// Get additional main partitions referenced by partitions.
		// Check alter status of partitions and list unusable (added) indexes.
		unusableIndexes_.clear();
		const Alterations& alterations = master.getIndexAlterations();
		const uint32_t nbAlterations = alterations.length();
		const uint32_t nbPartitions = partitions_.length();
		for (uint32_t i = 0; i < nbPartitions; ++i) {
			PartitionGuard& p = partitions_[i];
			if (!p->isTransient()) {
				PersistentPartition& partition = static_cast<PersistentPartition&>(*p);
				PersistentPartition* mainPartition = partition.getMainPartition();
				if (mainPartition != 0) {
					PartitionGuard mguard(mainPartition);
					if (!partitions_.contains(mguard)) {
						mainPartitions_.insertIfAbsent(mguard);
					}
				}
			}
			if (!p->isReady()) {
				for (uint32_t j = 0; j < nbAlterations; ++j) {
					const Alteration& alteration = alterations[j];
					if (alteration.getType() != ALT_ADD_INDEX
						|| alteration.getSerial() <= p->getIndexAlterSerial()) {
						continue;
					}
					const uint32_t index = alteration.getId();
					unusableIndexes_.insertIfAbsent(index);
				}
			}
		}
	}
	readers_.initialize(partitions_.length());
	resetPosition();
	loaded_ = true;
	queryInfo_.updateIndirectors();

	// Make sure MySQL will not use an index being created.
	TABLE& table = getTable();
	for (uint32_t i = 0; i < unusableIndexes_.length(); ++i) {
		const int bit = master.getMySqlIndexId(unusableIndexes_[i]);
		table.keys_in_use_for_query.clear_bit(bit);
		table.keys_in_use_for_group_by.clear_bit(bit);
		table.keys_in_use_for_order_by.clear_bit(bit);
		table.covering_keys.clear_bit(bit);
		table.quick_keys.clear_bit(bit);
		table.merge_keys.clear_bit(bit);
	}
#ifndef NDEBUG
	DBUG_PRINT("sparrow_context", ("Table %s.%s: loaded %u partitions into query context for %s", master.getDatabase().c_str(),
		master.getTable().c_str(), partitions_.length(), speriods.c_str()));
#endif
}

// Provide statistics to MySQL. 
void Context::getStats(ha_statistics& stats, const uint flag) {
	const Master& master = getShare().getMaster();
	TABLE& table = getTable();
	if (flag & HA_STATUS_VARIABLE) {
		uint64_t records = 0;
		uint64_t dataSize = 0;
		uint64_t indexSize = 0;
		TimePeriods	periods;
		loadTimePeriods( periods );
		if ( periods[0] == TimePeriod() ) {
			ReadGuard guard(master.getLock());
			records = master.getRecords() + master.getTransientRecords();
			dataSize = master.getDataSize();
			indexSize = master.getIndexSize();
		} else {
			load();
			SYSsortedVector<uint64_t> mainSerials;
			for (uint32_t i = 0; i < partitions_.length(); ++i) {
				const Partition& partition = *partitions_[i].get();
				if (partition.isTransient()) {
					records += partition.getRecords();
				} else {
					const PersistentPartition& persistentPartition = static_cast<const PersistentPartition&>(partition);
					const PersistentPartition& mainPartition = *persistentPartition.getMainPartition();
					if (mainSerials.insertIfAbsent(mainPartition.getSerial())) {
						records += mainPartition.getDataRecords();
						dataSize += mainPartition.getDataSize();
						indexSize += mainPartition.getIndexSize();
					}
				}
			}
		}
		stats.records = static_cast<ha_rows>(records);
		stats.deleted = 0;
		stats.data_file_length = dataSize;
		stats.index_file_length = indexSize;
		stats.delete_length = 0;
		if (records > 0) {
			stats.mean_rec_length = static_cast<ulong>(stats.data_file_length / records);
		} else {
			stats.mean_rec_length = 0;
		}
		stats.check_time = static_cast<time_t>(0);
	}
	// See mysql\include\my_base.h:766. 
	if (flag & HA_STATUS_CONST) {
		ReadGuard guard(master.getLock());
		stats.create_time = static_cast<ulong>(master.getTimeCreated());
		stats.max_data_file_length = 0;
		stats.max_index_file_length = 0;
		stats.block_size = 0;
		for (uint i = 0; i < table.s->keys; ++i) {
			for (uint j = 0; j < table.key_info[i].user_defined_key_parts; ++j) {
				table.key_info[i].rec_per_key[j]= 0;
			}
		}
	}
	if (flag & HA_STATUS_TIME) {
		ReadGuard guard(master.getLock());
		stats.update_time = static_cast<ulong>(master.getTimeUpdated());
	}
	if (flag & HA_STATUS_AUTO) {
		ReadGuard guard(master.getLock());
		stats.auto_increment_value = static_cast<ulonglong>(master.getAutoInc());
	}
}

// Moves to next row.
bool Context::moveNext(uint8_t* buffer) {
	load();
	while (true) {
		try {
			if (position_.isValid()) {
				const uint32_t partition = position_.getPartition();
				position_ = partitions_[partition]->moveNext(*this, position_);
				if (!position_.isValid()) {
					if (partition + 1 < partitions_.length()) {
						position_ = partitions_[partition + 1]->moveFirst(*this, partition + 1);
					}
				}
			} else if (!partitions_.isEmpty()) {
				position_ = partitions_[0]->moveFirst(*this, 0);
			}
			return position_.isValid() && partitions_[position_.getPartition()]->readData(*this, position_, buffer, BlockCacheHint::largeForward2_);
		} catch(const SparrowException&) {
			// Ignore error and retry.
		}
	}
}

// Moves to previous row.
bool Context::movePrevious(uint8_t* buffer) {
	load();
	while (true) {
		try {
			if (position_.isValid()) {
				const uint32_t partition = position_.getPartition();
				position_ = partitions_[partition]->movePrevious(*this, position_);
				if (!position_.isValid()) {
					if (partition > 0) {
						position_ = partitions_[partition - 1]->moveLast(*this, partition - 1);
					}
				}
			} else if (!partitions_.isEmpty()) {
				const uint32_t partition = partitions_.length() - 1;
				position_ = partitions_[partition]->moveLast(*this, partition);
			}
			return position_.isValid() && partitions_[position_.getPartition()]->readData(*this, position_, buffer, BlockCacheHint::largeBackward2_);
		} catch(const SparrowException&) {
			// Ignore error and retry.
		}
	}
}

// Moves to absolute position in data files and read record.
bool Context::moveAbsolute(const uint64_t position, uint8_t* buffer) {
	load();
	try {
		const Position restoredPos = restorePosition(position);
		if (restoredPos.isValid()) {
			position_ = restoredPos;
			return partitions_[position_.getPartition()]->readData(*this, position_, buffer, BlockCacheHint::smallAround2_);
		} else {
			return false;
		}
	} catch(const SparrowException& e) {
		e.toLog();
		return false;
	}
}

// Sets the active index. If MAX_KEY, there is no active index.
void Context::setActiveIndex(const uint index) {
	SPARROW_ENTER("Context::setActiveIndex");
	queryInfo_.update(getShare(), getTable(), index);
	resetPosition();
}

// Finds and reads the record using the current index and the given key information.
bool Context::findRecord(const KeyValue& key, const enum ha_rkey_function findFlag, uint8_t* buffer) {
	SPARROW_ENTER("Context::findRecord");
	load();
	resetPosition();
	const SearchFlag searchFlag(findFlag);
	try {
		const uint32_t nbPartitions = partitions_.length();
		for (uint32_t partition = 0; partition < nbPartitions; ++partition) {
			Position& pos = positions_[partition];
			const PartitionGuard    p = partitions_[partition];
			if (p.get() == NULL) {
				throw SparrowException::create(false, "NULL partition in context.");
			}
			pos = partitions_[partition]->indexFind(*this, partition, key, searchFlag);
			if (pos.isValid() && !partitions_[partition]->readKey(*this, pos, true, HA_WHOLE_KEY, keyValue(partition), true)) {
				return false;
			}
			indirector_[partition] = partition;
		}
		sortKeyValues();
		if (searchFlag == SearchFlag::EQ || searchFlag == SearchFlag::GE || searchFlag == SearchFlag::GT) {
			for (uint32_t i = 0; i < nbPartitions; ++i) {
				const Position& pos = positions_[indirector_[i]];
				if (pos.isValid()) {
					indirectorIndex_ = i;
					position_ = pos;
					break;
				}
			}
		} else {
			indirectorIndex_ = nbPartitions - 1;
			position_ = positions_[indirector_[indirectorIndex_]];
		}
		return position_.isValid() && partitions_[position_.getPartition()]->readData(*this, position_, buffer, BlockCacheHint::smallAround2_);
	} catch(const SparrowException& e) {
		e.toLog();
		return false;
	}
}

template class Sort<KeyIndirector, KeyComparator>;

// Sort key values for all partitions, in ascending order.
// Upon return, the context's indirector gives the partition number for each value.
// Invalid positions (no record found) come first in the indirector array.
void Context::sortKeyValues() {
	const KeyComparator comparator(*this);
	Sort<KeyIndirector, KeyComparator>::quickSort(indirector_, comparator, 0, partitions_.length());
}

// Finds first record for current index.
bool Context::findFirstRecord(uint8_t* buffer) {
	SPARROW_ENTER("Context::findFirstRecord");
	load();
	resetPosition();
	if (partitions_.isEmpty()) {
		return false;
	}
	try {
		// Save first position for all partitions.
		uint32_t partition = 0;
		while (partition < partitions_.length()) {
			Position& position = positions_[partition];
			position = partitions_[partition]->indexFirst(*this, partition);
			if (position.isValid()) {
				if (!partitions_[partition]->readKey(*this, position, true, HA_WHOLE_KEY, keyValue(partition), true)) {
					return false;
				}
				indirector_[partition] = partition;
				partition++;
			} else {
				// Discard invalid partition.
				partitions_.removeAt(partition);
				positions_.removeAt(partition);
				indirector_.removeAt(partition);
				if (partitions_.isEmpty()) {
					return false;
				}
			}
		}
		sortKeyValues();
		indirectorIndex_ = 0;
		position_ = positions_[indirector_[0]];
		return position_.isValid() && partitions_[position_.getPartition()]->readData(*this, position_, buffer, BlockCacheHint::smallAround2_);
	} catch(const SparrowException& e) {
		e.toLog();
		return false;
	}
}

// Finds last record for current index.
bool Context::findLastRecord(uint8_t* buffer) {
	SPARROW_ENTER("Context::findLastRecord");
	load();
	resetPosition();
	if (partitions_.isEmpty()) {
		return false;
	}
	try {
		// Save last position for all partitions.
		uint32_t partition = 0;
		while (partition < partitions_.length()) {
			Position& position = positions_[partition];
			position = partitions_[partition]->indexLast(*this, partition);
			if (position.isValid()) {
				if (!partitions_[partition]->readKey(*this, position, true, HA_WHOLE_KEY, keyValue(partition), true)) {
					return false;
				}
				indirector_[partition] = partition;
				partition++;
			} else {
				// Discard invalid partition.
				partitions_.removeAt(partition);
				positions_.removeAt(partition);
				indirector_.removeAt(partition);
				if (partitions_.isEmpty()) {
					return false;
				}
			}
		}
		sortKeyValues();
		indirectorIndex_ = partitions_.length() - 1;
		position_ = positions_[indirector_[indirectorIndex_]];
		return position_.isValid() && partitions_[position_.getPartition()]->readData(*this, position_, buffer, BlockCacheHint::smallAround2_);
	} catch(const SparrowException& e) {
		e.toLog();
		return false;
	}
}

// Finds next record for current index.
bool Context::findNextRecord(uint8_t* buffer) {
	SPARROW_ENTER("Context::findNextRecord");
	load();
	if (partitions_.isEmpty() || !position_.isValid()) {
		return false;
	}
	try {
		const uint32_t partition = position_.getPartition();
		Position& position = positions_[partition];
		if (position_.hasIntervalHint() && position_.getIndexHint() != position_.getEndHint()) {
			position = partitions_[partition]->indexNext(*this, position);
			position_ = position;
		} else {
			position = partitions_[partition]->indexNext(*this, position);
			if (position.isValid() && !partitions_[partition]->readKey(*this, position, true, HA_WHOLE_KEY, keyValue(partition), true)) {
				return false;
			}
			updateIndirector(position);
			position_ = Position();
			const uint32_t nbPartitions = partitions_.length();
			for (uint32_t i = 0; i < nbPartitions; ++i) {
				const Position& pos = positions_[indirector_[i]];
				if (pos.isValid()) {
					indirectorIndex_ = i;
					position_ = pos;
					break;
				}
			}
		}
		return position_.isValid() && partitions_[position_.getPartition()]->readData(*this, position_, buffer, BlockCacheHint::smallAround2_);
	} catch(const SparrowException& e) {
		e.toLog();
		return false;
	}
}

// Finds previous record for current index.
bool Context::findPreviousRecord(uint8_t* buffer) {
	SPARROW_ENTER("Context::findPreviousRecord");
	load();
	if (partitions_.isEmpty() || !position_.isValid()) {
		return false;
	}
	try {
		const uint32_t partition = position_.getPartition();
		Position& position = positions_[partition];
		if (position_.hasIntervalHint() && position_.getIndexHint() != position_.getStartHint()) {
			position = partitions_[partition]->indexPrevious(*this, position);
			position_ = position;
		} else {
			position = partitions_[partition]->indexPrevious(*this, position);
			if (position.isValid() && !partitions_[partition]->readKey(*this, position, false, HA_WHOLE_KEY, keyValue(partition), true)) {
				return false;
			}
			updateIndirector(position);
			indirectorIndex_ = partitions_.length() - 1;
			position_ = positions_[indirector_[indirectorIndex_]];
		}
		return position_.isValid() && partitions_[position_.getPartition()]->readData(*this, position_, buffer, BlockCacheHint::smallAround2_);
	} catch(const SparrowException& e) {
		e.toLog();
		return false;
	}
}

template class BinarySearch<SearchKeyComparator>;

// Update indirector for index scan.
void Context::updateIndirector(const Position& position) {
	const uint32_t save = indirector_[indirectorIndex_];
	uint32_t* data = const_cast<uint32_t*>(indirector_.data());
	const uint32_t nbPartitions = partitions_.length() - 1;
	if (position.isValid()) {
		SearchKeyComparator comparator(*this, position);
		const uint32_t insertionPoint = BinarySearch<SearchKeyComparator>::find(comparator, 0, nbPartitions, SearchFlag::GE);
		memmove(data + indirectorIndex_, data + indirectorIndex_ + 1, sizeof(data[0]) * (nbPartitions - indirectorIndex_));
		if (insertionPoint == UINT_MAX || insertionPoint == nbPartitions) {
			data[nbPartitions] = save;
		} else {
			memmove(data + insertionPoint + 1, data + insertionPoint, sizeof(data[0]) * (nbPartitions - insertionPoint));
			data[insertionPoint] = save;
		}
	} else {
		memmove(data + indirectorIndex_, data + indirectorIndex_ + 1, sizeof(data[0]) * (nbPartitions - indirectorIndex_));
		memmove(data + 1, data, sizeof(data[0]) * nbPartitions);
		data[0] = save;
	}
}

uint64_t Context::recordsTotal() {
	const Master& master = getShare().getMaster();
	ReadGuard guard(master.getLock());
	return  master.getRecords() + master.getTransientRecords();
}

// Returns an approximated number of records in the given range.
// CAUTION: if this method returns 0, MySQL will assume there is no row in the range!
// So make sure the returned result is either exact or over-estimated.
uint64_t Context::recordsInRange(const uint index, const key_range* minKey, const key_range* maxKey) {
	SPARROW_ENTER("Context::recordsInRange");
	load();
	const uint32_t nbPartitions = partitions_.length();
	if (nbPartitions == 0) {
		return 0;
	}
	try {
		ChangeIndexGuard guard(*this, index);

		// Is the index unusable?
		if (unusableIndexes_.contains(queryInfo_.getIndex())) {
			return HA_POS_ERROR - 1;
		}
		const uint32_t maxPartitions = 10;	// Do not scan all partitions.
		const double ratio = static_cast<double>(maxPartitions) / nbPartitions;
		const bool useRatio = nbPartitions > maxPartitions;
		uint64_t count = 0;
		uint32_t pos = UINT_MAX;
		uint32_t scannedPartitions = 0;
		for (uint32_t i = 0; i < nbPartitions; ++i) {
			const uint32_t newPos = useRatio ? static_cast<uint32_t>(i * ratio) : i;
			if (newPos == pos) {
				continue;
			}
			pos = newPos;
			scannedPartitions++;
			const PartitionGuard& partition = partitions_[pos];
			count += partition->recordsInRange(*this, pos, minKey, maxKey);
		}
		count = static_cast<uint64_t>(count * static_cast<double>(nbPartitions) / scannedPartitions);
		count = (count * sparrow_index_cost_percentage) / 100;		// MySQL seems to favor full table scans. See ha_innobase::info().
		return std::max(uint64_t{2}, count);
	} catch(const SparrowException& e) {
		e.toLog();
		return 0;
	}
}

void Context::writeLock() {
	writing_ = true;
	getShare().getMaster().startUpdate();
}

void Context::unlock() {
	if (writing_) {
		getShare().getMaster().endUpdate();
		writing_ = false;
	}
}

bool Context::updateRecord(const uint8_t* buffer) {
	SPARROW_ENTER("Context::updateRecord");
	try {
		return position_.isValid() && partitions_[position_.getPartition()]->updateData(*this, position_, buffer);
	} catch(const SparrowException& e) {
		e.toLog();
		return false;
	}
}

void Context::startInsert(const uint32_t rows) {
	SPARROW_ENTER("Context::startInsert");
	insertBuffer_.clear();
	insertRows_ = rows;
}

bool Context::insertRecord(const uint8_t* buffer) {
	SPARROW_ENTER("Context::insertRecord");
	try {
		const TableFields& fields = getShare().getMappedFields();
		const uint32_t n = fields.length();
		for (uint32_t i = 0; i < n; ++i) {
			fields[i]->insertTransform(buffer, insertBuffer_);
		}

		if(insertBuffer_.position() >= sparrow_direct_insertion_threshold) {
			// InsertBuffer_ is big enough. Send its content to sparrow
			const Master& master = getShare().getMaster();
			// We have to save the current allocated size of the insertBuffer_
			const uint64_t realLimit = insertBuffer_.limit();
			// Limit the buffer to what is already written
			insertBuffer_.limit(insertBuffer_.position());
			// And put the cursor at the beginning.
			insertBuffer_.position(0);
			InternalApi::write(master.getDatabase().c_str(), master.getTable().c_str(), insertBuffer_, insertRows_);
			// Once the data are given to sparrow, we could restore the previous buffer length
			insertBuffer_.limit(realLimit);
			// And rewind to the beginning for writing the new data over the old ones.
			insertBuffer_.position(0);
		}

		return true;
	} catch(const SparrowException& e) {
		e.toLog();
		return false;
	}
}

bool Context::endInsert() {
	SPARROW_ENTER("Context::endInsert");
	try {
		const Master& master = getShare().getMaster();
		insertBuffer_.limit(insertBuffer_.position());
		insertBuffer_.position(0);
		InternalApi::write(master.getDatabase().c_str(), master.getTable().c_str(), insertBuffer_, insertRows_);
		insertBuffer_.clear();
		return true;
	} catch(const SparrowException& e) {
		insertBuffer_.clear();
		e.toLog();
		return false;
	}		
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// KeyComparator
//////////////////////////////////////////////////////////////////////////////////////////////////////

KeyComparator::KeyComparator(Context& context) : context_(context), queryInfo_(context.getQueryInfo()), fields_(context.getShare().getMappedFields()) {
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SearchKeyComparator
//////////////////////////////////////////////////////////////////////////////////////////////////////

SearchKeyComparator::SearchKeyComparator(Context& context, const Position& position)
	: context_(context), queryInfo_(context.getQueryInfo()), fields_(context.getShare().getMappedFields()),
	keyValue_(context.keyValue(position.getPartition()), HA_WHOLE_KEY) {
	assert(position.isValid());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PartitionReaderGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

PartitionReaderGuard::PartitionReaderGuard(const PersistentPartition& partition, const uint32_t index, const bool isString, const BlockCacheHint& hint) _THROW_(SparrowException)
	: reader_(partition.createReader(index, isString, hint)), owned_(true) {
}

}
