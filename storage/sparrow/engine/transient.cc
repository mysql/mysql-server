/*
	Transient partition.
*/

#include "../handler/hasparrow.h"
#include "transient.h"
#include "master.h"
#include "purge.h"
#include "fileutil.h"
#include "internalapi.h"
#include "persistent.h"
#include "flush.h"
#include "coalescing.h"
#include "../functions/ipaddress.h"
#include "../dns/dnscache.h"

#include "../engine/log.h"

namespace Sparrow {

using namespace IvFunctions;

// Instantiate template classes.
template class ColumnAccessorSimple<int8_t>;
template class ColumnAccessorSimple<uint8_t>;
template class ColumnAccessorSimple<int16_t>;
template class ColumnAccessorSimple<uint16_t>;
template class ColumnAccessorSimple<int32_t>;
template class ColumnAccessorSimple<uint32_t>;
template class ColumnAccessorSimple<int64_t>;
template class ColumnAccessorSimple<uint64_t>;
template class ColumnAccessorSimple<double>;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TransientPartition
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Condition to know if insertion is possible.
Lock TransientPartition::condLock_(true, "TransientPartition::condLock_");
Cond TransientPartition::insertCond_(true, TransientPartition::condLock_, "TransientPartition::insertCond_");

// To wait until flushs are completed.
volatile uint32_t TransientPartition::flushs_ = 0;
Lock TransientPartition::PartLock_(true, "TransientPartition::PartLock_");
uint64_t TransientPartition::sizeFlushing_ = 0;		// Total size of partitions that have been forced to be flushed
SYSvector<TransientPartition*, 256> TransientPartition::AllTransPartitions_;		// List of all transient partitions, first one is oldest, last is newest.
SYSvector<TransientPartition*, 256> TransientPartition::FlushingPartitions_;		// List of partitions that have been flushed before the normal timeout.

Cond TransientPartition::flushCond_(true, TransientPartition::condLock_, "TransientPartition::flushCond_");

// Empty time period (returned when this transient partition is empty).
TimePeriod TransientPartition::voidPeriod_ = TimePeriod(static_cast<uint64_t>(0)).makeIntersection(static_cast<uint64_t>(1));

TransientPartition::TransientPartition(Master* master, const uint64_t serial)
	: Partition(serial, 0, 0, master->getIndexAlterSerial(), master->getColumnAlterSerial()),
	master_(master), dnsConfiguration_(master->getDnsConfiguration()), accessors_(0), timestampAccessor_(0),
	minTimestamp_(ULLONG_MAX), maxTimestamp_(0), dnsIdAccessor_(0),
	hasString_(false), lock_(false, TransientPartition::getName(master, serial, "lock_").c_str()), timestamp_(Scheduler::now()),
	records_(0), jobCounter_(0), errors_(0), flush_tries_(0), done_(false), dnsPending_(0), flush_(false), dataSize_(0), indexSize_(0),
	stringOffset_(0), stringSize_(0), flushTimestamp_(0), size_(0) {
	SPARROW_ENTER("TransientPartition::TransientPartition");
	DBUG_PRINT("sparrow_transient", ("Creating transient partition %s.%s.%llu",
		master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(serial)));

	// Initialize column ids.
	const Columns& columns = master_->getColumns();
	uint32_t i;
	uint32_t pos = 0;
	for (i = 0; i < columns.length(); ++i) {
		const Column& column = columns[i];
		if (!column.isDropped()) {
			if (column.isString()) {
				hasString_ = true;
			}
			++pos;
		}
	}
	const uint32_t validColumns = pos;
	const Indexes& indexes = master_->getIndexes();
	uint32_t nbIndexes = indexes.length();
	indexStringFlags_ = IndexStringFlags(nbIndexes);
	for (i = 0; i < nbIndexes; ++i) {
		const Index& index = indexes[i];
		if (!index.isDropped()) {
			indexIds_.append(i);
			ColumnIds columnIds(index.getColumnIds());
			bool hasString = false;
			for (uint32_t j = 0; j < columnIds.length(); ++j) {
				const uint32_t id = columnIds[j];
				if (columns[id].isString()) {
					hasString = true;
					break;
				}
			}
			indexStringFlags_.append(hasString);
			columnIds_.append(columnIds);
		}
	}

	// Create columns.
	accessors_.resize(validColumns);
	dnsIpAccessors_.resize(validColumns);
	for (i = 0; i < validColumns; ++i) {
		dnsIpAccessors_.append(0);
	}
	pos = 0;
	for (i = 0; i < columns.length(); ++i) {
		const Column& column = columns[i];
		if (column.isDropped()) {
			continue;
		}
		ColumnAccessor* accessor = 0;
		switch (column.getType()) {
			case COL_STRING: {
				ColumnAccessorBin* binAccessor = new ColumnAccessorBin(i, column, binBuffer_);
				if (column.isFlagSet(COL_IP_LOOKUP)) {
					dnsLookupAccessors_.append(binAccessor);
				}
				accessor = binAccessor;
				break;
			}
			case COL_BLOB: {
				ColumnAccessorBin* binAccessor = new ColumnAccessorBin(i, column, binBuffer_);
				if (column.isFlagSet(COL_IP_ADDRESS)) {
					dnsIpAccessors_[pos] = binAccessor;
				}
				accessor = binAccessor;
				break;
			}
			case COL_BYTE: {
				if (column.isFlagSet(COL_UNSIGNED)) {
					accessor = new ColumnAccessorSimple<uint8_t>(i, column);
				} else {
					accessor = new ColumnAccessorSimple<int8_t>(i, column);
				}
				break;
			}
			case COL_SHORT: {
				if (column.isFlagSet(COL_UNSIGNED)) {
					accessor = new ColumnAccessorSimple<uint16_t>(i, column);
				} else {
					accessor = new ColumnAccessorSimple<int16_t>(i, column);
				}
				break;
			}
			case COL_DOUBLE: accessor = new ColumnAccessorSimple<double>(i, column); break;
			case COL_INT: {
				if (column.isFlagSet(COL_UNSIGNED)) {
					accessor = new ColumnAccessorSimple<uint32_t>(i, column);
				} else {
					accessor = new ColumnAccessorSimple<int32_t>(i, column);
				}
				if (column.isFlagSet(COL_DNS_IDENTIFIER)) {
					dnsIdAccessor_ = accessor;
				}
				break;
			}
			case COL_LONG: {
				if (column.isFlagSet(COL_UNSIGNED)) {
					accessor = new ColumnAccessorSimple<uint64_t>(i, column);
				} else {
					accessor = new ColumnAccessorSimple<int64_t>(i, column);
				}
				break;
			}
			case COL_TIMESTAMP: {
				ColumnAccessorSimple<uint64_t>* simpleAccessor = new ColumnAccessorSimple<uint64_t>(i, column);
				if (pos == 0) {
					// First column is the timestamp.
					timestampAccessor_ = simpleAccessor;
				}
				accessor = simpleAccessor;
				break;
			}
			default: {
				break;
			}
		}
		accessors_.append(accessor);
		++pos;
	}

	// Reference this partition in the list of transient partitions
	addPartition(this);
}

// STATIC
Str TransientPartition::getName(Master* master, const uint64_t serial, const char* name) {
	char tmp[1024];
	snprintf(tmp, sizeof(tmp), "TransientPartition(%s.%s.%llu)::%s",
		master->getDatabase().c_str(), master->getTable().c_str(), static_cast<ulonglong>(serial), name);
	return Str(tmp);
}

TransientPartition::~TransientPartition() {
	SPARROW_ENTER("TransientPartition::~TransientPartition");
	DBUG_PRINT("sparrow_transient", ("Destroying transient partition %s.%s.%llu",
		master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial())));
	flushedPartition(this);
	clear();
}

void TransientPartition::clear() {
	SPARROW_ENTER("TransientPartition::clear");
	SizeGuard sizeGuard(*this);
	for (uint32_t i = 0; i < accessors_.length(); ++i) {
		accessors_[i]->clear();
	}
	binBuffer_.clear();
	accessors_.clearAndDestroy();
}

PartitionSnapshot* TransientPartition::snapshot() {
	SPARROW_ENTER("TransientPartition::snapshot");
	ReadGuard guard(lock_);
	uint32_t records = getRecords();
	DBUG_PRINT("sparrow_transient", ("Snapshoting transient partition %s.%s.%llu: %u records",
		master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), records));
	if (records == 0) {
		return 0;
	} else {
		return new PartitionSnapshot(this, records);
	}
}

// If necessary, wait until there is some room in the tuple buffer.
// STATIC
bool TransientPartition::waitForRoom(volatile bool& aborting) {
	SPARROW_ENTER("TransientPartition::waitForRoom");
	Guard guard(TransientPartition::condLock_);
	uint64_t	threshold = sparrow_max_tuple_buffer_size;
	uint64_t	thresholdLower = (sparrow_max_tuple_buffer_size * sparrow_tuple_buffer_threshold) / 100;

	uint64_t	waitStartTime = my_micro_time();
	bool	hasWaited = false;
	bool	errMsg = false;

	for(;;) {
		uint64_t tupleBufferSize = SparrowStatus::get().tupleBufferSize_;
		if (tupleBufferSize >= threshold) {
			if (hasWaited) {
				uint64_t waitEndTime = my_micro_time();
				uint64_t waitTime = (waitEndTime - waitStartTime)/1000;
				if (waitTime > 60000) {			// 1 minute threshold
					// Make checks and force insertion
					Guard	guard(PartLock_);
					if (!errMsg) {
						spw_print_information("[waitForRoom] Waited for room too long: size %llu, threshold %llu, size flushing %llu, nb flushing %u, nb transient %u",
							static_cast<ulonglong>(tupleBufferSize), static_cast<ulonglong>(threshold), static_cast<ulonglong>(sizeFlushing_), FlushingPartitions_.entries(), AllTransPartitions_.entries());
						errMsg = true;
					}
					if (FlushingPartitions_.isEmpty() && AllTransPartitions_.isEmpty()) {
						spw_print_information("[waitForRoom] Nothing to flush! Tuple buffer size is wrong. Resetting");
						sizeFlushing_ = 0;
						SparrowStatus::get().tupleBufferSize_ = 0;
					}
					return true;
				}
			}

			TransientPartition::insertCond_.wait(100, true);
			if (aborting) {
				return false;
			}
			threshold = thresholdLower;
			hasWaited = true;
		} else {
			if (hasWaited) {
				DBUG_PRINT("sparrow_transient", ("[waitForRoom] There's enough room: %llu < %llu.", static_cast<ulonglong>(tupleBufferSize), static_cast<ulonglong>(threshold)));
			}
			break;
		}
	}
	if (hasWaited) {
		uint64_t waitEndTime = my_micro_time();
		uint64_t waitTime = (waitEndTime - waitStartTime)/1000;
		Atomic::add64(&SparrowStatus::get().flushWait_, waitTime);
	}
	return true;
}

// Unmarshalls incoming buffer using columns and fill accessors.
bool TransientPartition::insert(ByteBuffer& buffer, const uint32_t rows, uint64_t& last_timestamp) _THROW_(SparrowException) {
	SPARROW_ENTER("TransientPartition::insert");
#ifndef NDEBUG
	uint64_t tstart = my_micro_time();
#endif
	DBUG_PRINT("sparrow_transient", ("Inserting %u rows into transient partition %s.%s.%llu ? Trying to take lock",
		rows, master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial())));

	WriteGuard guard(lock_);

	// Check if partition can accept more data.
	if (done_) {
		DBUG_PRINT("sparrow_transient", ("Transient partition %llu done ==> we need another one", static_cast<ulonglong>(getSerial())));
		return false;
	}
	DBUG_PRINT("sparrow_transient", ("Inserting %u rows into transient partition %s.%s.%llu",
		rows, master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial())));
	uint32_t saved = timestampAccessor_->length();
	uint64_t savedPosition = buffer.position();
	int64_t autoInc = master_->getAutoInc();
	int64_t savedAutoInc = autoInc;
	const uint32_t initial = records_;
	uint32_t	nbRows = 0;
	DBUG_PRINT("sparrow_transient", ("Initially: timestampAccessor_ %u, buffer.position %llu, master_->getAutoInc %lld, records_ %u",
		saved, static_cast<ulonglong>(savedPosition), static_cast<longlong>(autoInc), records_));
	bool resolve = false;
	uint64_t dnsTimestamp = 0;
	const uint64_t coalescingPeriod = master_->getCoalescingPeriod();
	uint64_t low = coalescingPeriod == 0 ? 0 : (minTimestamp_ == ULLONG_MAX ? 0 : (minTimestamp_ - (minTimestamp_ % coalescingPeriod)));
	uint64_t up = low == 0 ? 0 : (low + coalescingPeriod);
#ifndef NDEBUG
	const Str coalesc = Str::fromDuration(coalescingPeriod);
	const Str low_ts = Str::fromTimestamp(low);
	const Str up_ts = Str::fromTimestamp(up);
	DBUG_PRINT("sparrow_transient", ("coalesc p %s, low %s, up %s", coalesc.c_str(), low_ts.c_str(), up_ts.c_str()));
#endif
	try {
		SizeGuard sizeGuard(*this);
		DataReader reader(buffer, binBuffer_);
		while (!reader.end()) {
			saved = timestampAccessor_->length();
			savedPosition = buffer.position();
			savedAutoInc = autoInc;
			for (uint32_t i = 0; i < accessors_.length(); ++i) {
				ColumnAccessor* accessor = accessors_[i];
				const Column& column = accessor->getColumn();

				// Reverse DNS column: insert NULL for now (see dnsLookup()).
				if (column.isFlagSet(COL_IP_LOOKUP)) {
					accessor->insertNull();
					resolve = true;
				} else if (column.isFlagSet(COL_AUTO_INC)) {
					autoInc = accessor->insertAutoInc(autoInc);
				} else {
					uint8_t isNull = 0;
					if (accessor->isNullable()) {
						reader >> isNull;
					} 
					if (isNull) {
						accessor->insertNull();
					} else {
						accessor->insertValue(reader);
					}
				}
			}
			nbRows++;
			const uint64_t timestamp = timestampAccessor_->last();
			if (timestamp == 0) {
				throw SparrowException::create(false, "Cannot insert row with zero timestamp");
			}
			// Make checks in case of data corruption
			{
				uint64_t	t = timestamp/1000;	// in seconds
				t /= (3600ULL*24*364);	// Number of years since 1970
				if (t < 30 || t > 60) {
					throw SparrowException::create(false, "Cannot insert data: wrong timestamp.");
				}
			}
			if (coalescingPeriod != 0) {
				if (low == 0) {
					low = timestamp - (timestamp % coalescingPeriod);
					up = low + coalescingPeriod;
				} else if (timestamp < low || timestamp >= up) {
#ifndef NDEBUG
					const Str ts = Str::fromTimestamp(timestamp);
					const Str low_ts = Str::fromTimestamp(low);
					const Str up_ts = Str::fromTimestamp(up);
					DBUG_PRINT("sparrow_transient", ("ts %s (at rows %u), low %s, up %s", ts.c_str(), nbRows, low_ts.c_str(), up_ts.c_str()));
#endif

					for (uint32_t i = 0; i < accessors_.length(); ++i) {
						accessors_[i]->shrink(saved);
					}
					if (saved < records_ || !resolve) {
						records_ = saved;
					}
					buffer.position(savedPosition);
					master_->setAutoInc(savedAutoInc);
					last_timestamp = timestamp;
#ifndef NDEBUG
					const Str min_ts = Str::fromTimestamp(minTimestamp_);
					const Str max_ts = Str::fromTimestamp(maxTimestamp_);
					DBUG_PRINT("sparrow_transient", ("Finished inserting in transient partition %llu: %s, %s",
						static_cast<ulonglong>(getSerial()), min_ts.c_str(), max_ts.c_str()));
#endif
					scheduleFlush(dnsTimestamp);
					return false;
				}
			}
			minTimestamp_ = std::min(minTimestamp_, timestamp);
			maxTimestamp_ = std::max(maxTimestamp_, timestamp);
		}

		// Lookup IP addresses if necessary.
		if (resolve) {
			dnsTimestamp = dnsLookup(initial, false);
			if (dnsTimestamp != 0) {
#ifndef NDEBUG
				const Str sTimestamp = Str::fromTimestamp(dnsTimestamp);
				DBUG_PRINT("sparrow_transient", ("Scheduling DNS update of transient partition %s.%s.%llu at %s",
					master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), sTimestamp.c_str()));
#endif
				Scheduler::addTask(new DnsTask(master_.get(), getSerial()), dnsTimestamp);
			}
		} else {
			records_ = timestampAccessor_->length();
		}
#ifndef NDEBUG
		const Str duration(Str::fromDuration((my_micro_time() - tstart) / 1000));
		const Str sTimestamp = Str::fromTimestamp(dnsTimestamp);
		DBUG_PRINT("sparrow_transient", ("Inserted %u rows into transient partition %s.%s.%llu in %s",
			rows, master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), duration.c_str()));
		{
			DBUG_PRINT("sparrow_transient", ("NOW: timestampAccessor_ %u, buffer.position %llu, master_->getAutoInc %lld, records_ %u",
				saved, static_cast<ulonglong>(savedPosition), static_cast<longlong>(autoInc), records_));

			const Str min_ts = Str::fromTimestamp(minTimestamp_);
			const Str max_ts = Str::fromTimestamp(maxTimestamp_);
			DBUG_PRINT("sparrow_transient", ("Transient partition %llu has been added %u rows, min, max ts %s, %s",
				static_cast<ulonglong>(getSerial()), nbRows, min_ts.c_str(), max_ts.c_str()));
		}

#endif
	} catch(const SparrowException& e) {
		// Rollback to last good position.
#ifndef NDEBUG
		const Str duration(Str::fromDuration((my_micro_time() - tstart) / 1000));
		DBUG_PRINT("sparrow_transient", ("Rollback transient partition %s.%s.%llu to %u rows: %s (insertion done in %s)",
			master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), saved, e.getText(), duration.c_str()));
#endif
		SizeGuard sizeGuard(*this);
		for (uint32_t i = 0; i < accessors_.length(); ++i) {
			accessors_[i]->shrink(saved);
		}
		if (saved < records_ || !resolve) {
			records_ = saved;
		}
		buffer.position(savedPosition);
		master_->setAutoInc(savedAutoInc);
		scheduleFlush(dnsTimestamp);
		throw e;
	}
	scheduleFlush(dnsTimestamp);
	master_->setAutoInc(autoInc);
	return true;
}


// Unmarshalls incoming buffer using only a selection of columns and fill accessors.
bool TransientPartition::insert(ByteBuffer& buffer, const uint32_t rows, const Names& columns, const ColumnIds& colIds, uint64_t& last_timestamp) _THROW_(SparrowException) {
	SPARROW_ENTER("TransientPartition::insert");
#ifndef NDEBUG
	uint64_t tstart = my_micro_time();
#endif
	DBUG_PRINT("sparrow_transient", ("Inserting %u rows on %u columns into transient partition %s.%s.%llu ? Trying to take lock",
		rows, columns.entries(), master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial())));

	WriteGuard guard(lock_);

	// Check if partition can accept more data.
	if (done_) {
		DBUG_PRINT("sparrow_transient", ("Transient partition %llu done ==> we need another one", static_cast<ulonglong>(getSerial())));
		return false;
	}
	DBUG_PRINT("sparrow_transient", ("Inserting %u rows into transient partition %s.%s.%llu",
		rows, master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial())));

	// Building the indirection table between each referenced column and the corresponding column accessor.
	Indirector		accessorPos(colIds.length());
	for (uint32_t i=0; i<colIds.length(); ++i) {
		const uint32_t	id = colIds[i];
		uint32_t j = 0;
		for (; j<accessors_.length(); ++j) {
			if (accessors_[j]->getColumnId() == id) {
				if (accessorPos.contains(j)) {
					throw SparrowException::create(false, "Failed to insert %u rows into transient partition %s.%s.%llu because the column %s is referenced twice.",
						rows, master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), columns[i].c_str());
				}
				accessorPos[i] = j;
				break;
			}
		}
		if (j == accessors_.length()) {
			throw SparrowException::create(false, "Failed to insert %u rows into transient partition %s.%s.%llu because no accessor was found for column %s",
				rows, master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), columns[i].c_str());
		}
	}
	//assert( accessorPos.length() == colIds.length() );

	Indirector		accessorMissing(accessors_.length() - accessorPos.length());
	uint	j = 0;
	for (uint32_t i=0; i<accessors_.length(); ++i) {
		if (!accessorPos.contains(i)) {
			accessorMissing[j++] = i;
		}
	}
	assert( j == accessorMissing.length() );

	uint32_t saved = timestampAccessor_->length();
	uint64_t savedPosition = buffer.position();
	int64_t autoInc = master_->getAutoInc();
	int64_t savedAutoInc = autoInc;
	const uint32_t initial = records_;
	uint32_t	nbRows = 0;
	DBUG_PRINT("sparrow_transient", ("Initially: timestampAccessor_ %u, buffer.position %llu, master_->getAutoInc %lld, records_ %u",
		saved, static_cast<ulonglong>(savedPosition), static_cast<longlong>(autoInc), records_));
	bool resolve = false;
	uint64_t dnsTimestamp = 0;
	const uint64_t coalescingPeriod = master_->getCoalescingPeriod();
	uint64_t low = coalescingPeriod == 0 ? 0 : (minTimestamp_ == ULLONG_MAX ? 0 : (minTimestamp_ - (minTimestamp_ % coalescingPeriod)));
	uint64_t up = low == 0 ? 0 : (low + coalescingPeriod);
#ifndef NDEBUG
	const Str coalesc = Str::fromDuration(coalescingPeriod);
	const Str low_ts = Str::fromTimestamp(low);
	const Str up_ts = Str::fromTimestamp(up);
	DBUG_PRINT("sparrow_transient", ("coalesc p %s, low %s, up %s", coalesc.c_str(), low_ts.c_str(), up_ts.c_str()));
#endif
	try {
		SizeGuard sizeGuard(*this);
		DataReader reader(buffer, binBuffer_);
		while (!reader.end()) {
			saved = timestampAccessor_->length();
			savedPosition = buffer.position();
			savedAutoInc = autoInc;
			for (uint32_t i=0; i<accessorPos.length(); ++i) {
				ColumnAccessor* accessor = accessors_[accessorPos[i]];
				[[maybe_unused]] const Column& column = accessor->getColumn();
				assert(column.isFlagSet(COL_IP_LOOKUP) == false);
				assert(column.isFlagSet(COL_AUTO_INC) == false);

				uint8_t isNull = 0;
				if (accessor->isNullable()) {
					reader >> isNull;
				} 
				if (isNull) {
					accessor->insertNull();
				} else {
					accessor->insertValue(reader);
				}
			}
			for (uint32_t i=0; i<accessorMissing.length(); ++i) {
				ColumnAccessor* accessor = accessors_[accessorMissing[i]];
				const Column& column = accessor->getColumn();

				// Reverse DNS column: insert NULL for now (see dnsLookup()).
				if (column.isFlagSet(COL_IP_LOOKUP)) {
					accessor->insertNull();
					resolve = true;
				} else if (column.isFlagSet(COL_AUTO_INC)) {
					autoInc = accessor->insertAutoInc(autoInc);
				} else if (accessor->isNullable()) {
					accessor->insertNull();
				} else {
					accessor->insertDummy(true);
				}
			}
			nbRows++;
			const uint64_t timestamp = timestampAccessor_->last();
			if (timestamp == 0) {
				throw SparrowException::create(false, "Cannot insert row with zero timestamp");
			}
			// Make checks in case of data corruption
			{
				uint64_t	t = timestamp/1000;	// in seconds
				t /= (3600ULL*24*364);	// Number of years since 1970
				if (t < 30 || t > 60) {
					throw SparrowException::create(false, "Cannot insert data: wrong timestamp.");
				}
			}
			if (coalescingPeriod != 0) {
				if (low == 0) {
					low = timestamp - (timestamp % coalescingPeriod);
					up = low + coalescingPeriod;
				} else if (timestamp < low || timestamp >= up) {
#ifndef NDEBUG
					const Str ts = Str::fromTimestamp(timestamp);
					const Str low_ts = Str::fromTimestamp(low);
					const Str up_ts = Str::fromTimestamp(up);
					DBUG_PRINT("sparrow_transient", ("ts %s (at rows %u), low %s, up %s", ts.c_str(), nbRows, low_ts.c_str(), up_ts.c_str()));
#endif

					for (uint32_t i = 0; i < accessors_.length(); ++i) {
						accessors_[i]->shrink(saved);
					}
					if (saved < records_ || !resolve) {
						records_ = saved;
					}
					buffer.position(savedPosition);
					master_->setAutoInc(savedAutoInc);
					last_timestamp = timestamp;
#ifndef NDEBUG
					const Str min_ts = Str::fromTimestamp(minTimestamp_);
					const Str max_ts = Str::fromTimestamp(maxTimestamp_);
					DBUG_PRINT("sparrow_transient", ("Finished inserting in transient partition %llu: %s, %s",
						static_cast<ulonglong>(getSerial()), min_ts.c_str(), max_ts.c_str()));
#endif
					scheduleFlush(dnsTimestamp);
					return false;
				}
			}
			minTimestamp_ = std::min(minTimestamp_, timestamp);
			maxTimestamp_ = std::max(maxTimestamp_, timestamp);
		}

		// Lookup IP addresses if necessary.
		if (resolve) {
			dnsTimestamp = dnsLookup(initial, false);
			if (dnsTimestamp != 0) {
#ifndef NDEBUG
				const Str sTimestamp = Str::fromTimestamp(dnsTimestamp);
				DBUG_PRINT("sparrow_transient", ("Scheduling DNS update of transient partition %s.%s.%llu at %s",
					master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), sTimestamp.c_str()));
#endif
				Scheduler::addTask(new DnsTask(master_.get(), getSerial()), dnsTimestamp);
			}
		} else {
			records_ = timestampAccessor_->length();
		}
#ifndef NDEBUG
		const Str duration(Str::fromDuration((my_micro_time() - tstart) / 1000));
		const Str sTimestamp = Str::fromTimestamp(dnsTimestamp);
		DBUG_PRINT("sparrow_transient", ("Inserted %u rows into transient partition %s.%s.%llu in %s",
			rows, master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), duration.c_str()));
		{
			DBUG_PRINT("sparrow_transient", ("NOW: timestampAccessor_ %u, buffer.position %llu, master_->getAutoInc %lld, records_ %u",
				saved, static_cast<ulonglong>(savedPosition), static_cast<longlong>(autoInc), records_));

			const Str min_ts = Str::fromTimestamp(minTimestamp_);
			const Str max_ts = Str::fromTimestamp(maxTimestamp_);
			DBUG_PRINT("sparrow_transient", ("Transient partition %llu has been added %u rows, min, max ts %s, %s",
				static_cast<ulonglong>(getSerial()), nbRows, min_ts.c_str(), max_ts.c_str()));
		}

#endif
	} catch(const SparrowException& e) {
		// Rollback to last good position.
#ifndef NDEBUG
		const Str duration(Str::fromDuration((my_micro_time() - tstart) / 1000));
		DBUG_PRINT("sparrow_transient", ("Rollback transient partition %s.%s.%llu to %u rows: %s (insertion done in %s)",
			master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), saved, e.getText(), duration.c_str()));
#endif
		SizeGuard sizeGuard(*this);
		for (uint32_t i = 0; i < accessors_.length(); ++i) {
			accessors_[i]->shrink(saved);
		}
		if (saved < records_ || !resolve) {
			records_ = saved;
		}
		buffer.position(savedPosition);
		master_->setAutoInc(savedAutoInc);
		scheduleFlush(dnsTimestamp);
		throw e;
	}
	scheduleFlush(dnsTimestamp);
	master_->setAutoInc(autoInc);
	return true;
}


void TransientPartition::scheduleFlush(const uint64_t dnsTimestamp) {
	if (timestampAccessor_->length() == 0) {
		return;
	}
	if (dnsTimestamp > flushTimestamp_) {
		// If we need more time to complete DNS resolution, change flush timestamp
		// but forbid adding more data.
		done_ = true;
#ifndef NDEBUG
		const Str sTimestamp = Str::fromTimestamp(dnsTimestamp);
		DBUG_PRINT("sparrow_transient", ("Delaying flush of transient partition %s.%s.%llu at %s for DNS",
			master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), sTimestamp.c_str()));
#endif
		flushTimestamp_ = dnsTimestamp;
		Scheduler::addTask(new FlushTask(master_.get(), getSerial()), flushTimestamp_);
	} else if (flushTimestamp_ == 0) {
		// Schedule timeout flush.
		flushTimestamp_ = timestamp_ + sparrow_flush_interval * 1000;
#ifndef NDEBUG
		const Str sTimestamp = Str::fromTimestamp(flushTimestamp_);
		DBUG_PRINT("sparrow_transient", ("Scheduling flush of transient partition %s.%s.%llu at %s",
			master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), sTimestamp.c_str()));
#endif
		Scheduler::addTask(new FlushTask(master_.get(), getSerial()), flushTimestamp_);
	} else if (done_) {
		// Partition is full; try to flush now.
		flushTimestamp_ = dnsTimestamp == 0 ? Scheduler::now() : dnsTimestamp;
#ifndef NDEBUG
		const Str sTimestamp = Str::fromTimestamp(flushTimestamp_);
		DBUG_PRINT("sparrow_transient", ("Scheduling flush of full transient partition %s.%s.%llu at %s",
			master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), sTimestamp.c_str()));
#endif
		Scheduler::addTask(new FlushTask(master_.get(), getSerial()), flushTimestamp_);
	}
}

void TransientPartition::updateDnsConfiguration(DnsConfiguration* dnsConfiguration) {
	SPARROW_ENTER("TransientPartition::updateDnsConfiguration");
	WriteGuard guard(lock_);
	dnsConfiguration_ = dnsConfiguration;
}

// DNS lookup is performed through several passes:
// - One pass each time new data are inserted. In this case, start gives
// the starting row for filling lookup columns. This pass gets lookups already in the cache.
// - One last pass after the DNS timeout has expired, and before this partition
// is flushed to disk and mutated. In this case, start is 0. This last pass is necessary
// to fill all cache-miss lookups.
// Returns 0 if DNS resolution is complete, or the timestamp of expected completion
// if there are pending resolutions.
uint64_t TransientPartition::dnsLookup(const uint32_t start, const bool lastPass) {
	SPARROW_ENTER("TransientPartition::dnsLookup");

#ifndef NDEBUG
	uint64_t tstart = my_micro_time();
#endif
	uint64_t now = std::time(nullptr);
	uint64_t mnow = my_micro_time();
	bool pending = false;
	const uint32_t end = timestampAccessor_->length();
	DnsConfiguration* configuration = dnsConfiguration_.get();
	uint32_t records = UINT_MAX;
	for (uint32_t i = 0; i < dnsLookupAccessors_.length(); ++i) {
		ColumnAccessorBin* dnsLookupAccessor = dnsLookupAccessors_[i];
		ColumnAccessorBin* dnsIpAccessor = dnsIpAccessors_[dnsLookupAccessor->getColumn().getInfo()];
		uint32_t limit = records_;
		for (uint32_t row = start; row < end; ++row) {
			// If the IP address is valid and the corresponding lookup is empty, try to resolve.
			if (dnsLookupAccessor->isNull(row) && !dnsIpAccessor->isNull(row)) {
				const BinString& string = *(*dnsIpAccessor)[row];		// The IP address to resolve into a host name.
				bool setIpAsString = false;
				if (configuration == 0) {
					// No DNS server: set the IP address as a string.
					setIpAsString = true;
				} else {
					// If the DNS identifier is not set or NULL, use wildcard value (-1).
					const int id = (dnsIdAccessor_ == 0 || dnsIdAccessor_->isNull(row)) ? -1 : static_cast<int>(dnsIdAccessor_->getValue(row));
					DnsCacheEntry* entry;
					{
						Guard guard(configuration->getLock());
						entry = configuration->doResolve(now, mnow, id, string.getData(), string.getLength());
					}
					if (entry == 0) {
						if (lastPass) {
							// The DNS worker is late; we should have an entry now.
							// Set the IP address as a string.
							setIpAsString = true;
						} else {
							pending = true;
						}
					} else {
						const Str& name = entry->getName();
						dnsLookupAccessor->insertValue(row, name.c_str(), name.length());
					}
				}
				if (setIpAsString) {
					char buffer[128];
					IpAddress address(string.getData(), string.getLength());
					dnsLookupAccessor->insertValue(row, buffer, address.print(buffer));
				}
			}
			if (!pending && row >= limit) {
				limit = row + 1;
			}
		}
		records = std::min(records, limit);
	}
	if (records != UINT_MAX) {
		records_ = records;
	}
#ifndef NDEBUG
	if (end > start) {
		const Str duration(Str::fromDuration((my_micro_time() - tstart) / 1000));
		DBUG_PRINT("sparrow_transient", ("Resolved ip lookups in transient partition %s.%s.%llu for rows %u-%u%s in %s%s, records=%u",
			master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), start, end,
			lastPass ? " (last pass)" : "", duration.c_str(), pending ? " (still pending)" : "", records_));
	}
#endif
	if (pending) {
		return Scheduler::now() + sparrow_dns_timeout * (1 + sparrow_dns_retries);
	} else {
		return 0;
	}
}

void TransientPartition::dnsUpdate() {
	SPARROW_ENTER("TransientPartition::dnsUpdate");
	WriteGuard	guard(lock_, false, true);
	if (!flush_) {
		SizeGuard sizeGuard(*this);
		dnsLookup(records_, false);
	}
}

// Sends jobs to compute indexes and, in turn, write generated data.
bool TransientPartition::flush(const uint64_t timestamp, bool master_lock_taken, bool force) {
	SPARROW_ENTER("TransientPartition::flush");

	TimePeriod	period;
	uint64_t		serial;
	uint32_t		columnAlterSerial;
	uint32_t		indexAlterSerial;
	ColumnIds	emptyColumnsIds;
	{
		WriteGuard guard(lock_, false, true);

		if ( flush_ || (timestamp != flushTimestamp_ && !force) ) {
			return false;
		}

		done_ = true;

		{
			SizeGuard sizeGuard(*this);
			dnsLookup(records_, true);
		}
		flushingPartition(this);

		if ( getRecords() == 0 ) {
			return false;
		}

		period = getPeriodNoLock();
		serial = getSerial();
		columnAlterSerial = getColumnAlterSerial();
		indexAlterSerial = getIndexAlterSerial();
		if (sparrow_column_optimisation) {
			refreshEmptyColumns();
			emptyColumnsIds = getEmptyColumns();
		}

#ifndef NDEBUG
		const Str trs_period = Str::fromTimePeriod(period);
		const Str min_ts = Str::fromTimestamp(minTimestamp_);
		const Str max_ts = Str::fromTimestamp(maxTimestamp_);
		DBUG_PRINT("sparrow_transient", ("Flush transient partition %llu, nb recs %u, %s [%s, %s]", 
			static_cast<ulonglong>(getSerial()), records_, trs_period.c_str(), min_ts.c_str(), max_ts.c_str() ));
		// UGLY !
		char	msg[1024] = "";
		for (uint i=0; i<emptyColumnsIds.length(); ++i) {
			if (i != 0) {
				strcat(msg, ",");
			}
			char	value[16];
			sprintf(value, "%u", emptyColumnsIds[i]);
			strcat(msg, value);
		}
		DBUG_PRINT("sparrow_transient", ("%u Skipped columns: %s", emptyColumnsIds.length(), msg));
#endif
	}

	PersistentPartitionGuard	mainPartition;
	{
		// Find a persistent partition we can extend, or create a new one.
		ReadGuard masterGuard(master_->getLock(), false, !master_lock_taken);
		mainPartition = master_->findMainPartition( serial, period, columnAlterSerial, indexAlterSerial, emptyColumnsIds );
		DBUG_PRINT("sparrow_transient", ("Main partition for transient part %llu would be %llu", static_cast<ulonglong>(getSerial()), 
			static_cast<ulonglong>(mainPartition->getSerial())));
	}

	WriteGuard guard(lock_);
	if ( !flush_ ) {
		// If column or index alteration has taken place while we had released the lock, get the new main partition
		if ( columnAlterSerial != getColumnAlterSerial() || indexAlterSerial != getIndexAlterSerial() )
		{
			period = getPeriodNoLock();
			serial = getSerial();
			columnAlterSerial = getColumnAlterSerial();
			indexAlterSerial = getIndexAlterSerial();

#ifndef NDEBUG
			const Str trs_period = Str::fromTimePeriod(period);
			const Str min_ts = Str::fromTimestamp(minTimestamp_);
			const Str max_ts = Str::fromTimestamp(maxTimestamp_);
			DBUG_PRINT("sparrow_transient", ("Column or Index alteration requires new main partition for transient %llu", static_cast<ulonglong>(getSerial()) ));
#endif

			guard.release();
			{
				ReadGuard masterGuard(master_->getLock(), false, !master_lock_taken);
				mainPartition = master_->findMainPartition( serial, period, columnAlterSerial, indexAlterSerial, emptyColumnsIds );
				DBUG_PRINT("sparrow_transient", ("New Main partition for transient part %llu would be %llu", static_cast<ulonglong>(getSerial()), static_cast<ulonglong>(mainPartition->getSerial())));
			}
			guard.acquire();
		}
		setEmptyColumns( mainPartition->getSkippedColumns() );

		doFlush( mainPartition );
	}

	return true;
}

void TransientPartition::refreshEmptyColumns() {
	emptyColumnsIds_.clear();
	for (uint32_t i=0; i<accessors_.length(); ++i) {
		if (accessors_[i]->getColumn().isFlagSet(COL_IP_LOOKUP))
			continue;
		if (accessors_[i]->areAllNulls()) {
			emptyColumnsIds_.append(accessors_[i]->getColumnId());
		}
	}
}

// Returns true if the partition is being flushed
bool TransientPartition::forceFlush( bool master_lock_taken ) {
	SPARROW_ENTER("TransientPartition::forceFlush");
	return flush(flushTimestamp_, master_lock_taken, true);
}

void TransientPartition::doFlush( PersistentPartitionGuard mainPartition ) {
	SPARROW_ENTER("TransientPartition::doFlush");
	{
		Guard flushGuard(TransientPartition::condLock_);
		flushs_++;
	}

	// DEBUG BPL - is already set in method flush()
	assert(done_ == true);
	if (!done_) {
		done_ = true;
		spw_print_information("[DEBUG] Forcing done_ to true on flushed partition %s.%s.%llu",
			master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()));
	}

	dataSerial_ = mainPartition->getSerial();

	// Set file system for index files.
	const bool coalescing = sparrow_coalescing && master_->getCoalescingPeriod() != 0;
	setFilesystem(FileUtil::chooseFilesystem(coalescing));

	// Prepare flush jobs.
	assert(getRecords() > 0);	// This partition cannot be empty.
	assert(jobCounter_ == 0 && errors_ == 0);
	DBUG_PRINT("sparrow_transient", ("Sending flush jobs for transient partition %s.%s.%llu",
		master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial())));
	FlushJob* job = new FlushJob(this, mainPartition, columnIds_.length() + 1, 1);
	if (hasString_) {
		// Send string job, which will in turn fire string-dependent jobs.
		job->getWorkerJobs().append(new StringJob(this, mainPartition));
	} else {
		// Write data file.
		job->getWriterJobs().append(new WriteJob(this, mainPartition, DATA_FILE, 0));
	}

	// Compute and write indexes not using strings.
	for (uint32_t i = 0; i < columnIds_.length(); ++i) {
		if (!indexStringFlags_[i]) {
			job->getWorkerJobs().append(new IndexJob(this, mainPartition, i));
		}
	}
	Flush::sendJob(job);
	flush_ = true;
}

// Optimize strings using existing string file, if any.
void TransientPartition::flushStrings(PersistentPartitionGuard mainPartition) _THROW_(SparrowException) {
	SPARROW_ENTER("TransientPartition::flushStrings");
	FileSection stringsSection;
	const bool newFile = mainPartition->getSerial() > getSerial();
	ReadGuard guard(lock_);
	if (!newFile) {
		// Optimize strings from existing main partition.
		{
			PartitionReader dataReader(*mainPartition, DATA_FILE, BlockCacheHint::smallForward0_);
			const FileHeaderBase& header = dataReader.getHeader();
			stringsSection = header.getStringsSection();
		}
		PartitionReader reader(*mainPartition, STRING_FILE, BlockCacheHint::largeForward0_);
		reader.seek(stringsSection.getOffset());
		DBUG_PRINT("sparrow_transient", ("Optimizing strings for transient partition %s.%s.%llu",
			master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial())));
		binBuffer_.optimize(reader, stringsSection.getSize());
	}

	// Flush strings.
	char filename[FN_REFLEN];
	mainPartition->getFileName(STRING_FILE, filename);
	const PartitionFile partitionFile(*mainPartition, STRING_FILE);
	const SimpleWriteCacheHint writeHint(partitionFile, 3);
	{
		FileWriter writer(filename, FILE_TYPE_STRING, newFile ? FILE_MODE_CREATE : FILE_MODE_UPDATE, &writeHint,
			stringsSection.getOffset() + stringsSection.getSize());
		const uint64_t save = writer.getFileSize();
		stringOffset_ = writer.getFileOffset();
		stringSize_ = binBuffer_.flush(writer);
		if (writer.getFileSize() == 0) {
			writer << "SPARROW";
		}
		writer.write();
		Atomic::add64(&dataSize_, writer.getFileSize() - save);
		if (stringSize_ + stringsSection.getSize() <= master_->getStringOptimization()) {
			stringOffset_ = stringsSection.getOffset();
			stringSize_ += stringsSection.getSize();
		}
	}
	DBUG_PRINT("sparrow_transient", ("Written strings for transient partition %s.%s.%llu to main partition %llu",
		master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), static_cast<ulonglong>(mainPartition->getSerial())));

	// We're going to trigger new jobs: jobs that needed that the SPS file was up to date first. So first, increment the jobCounter_ accordingly. 
	for (uint32_t i = 0; i < columnIds_.length(); ++i) {
		if (indexStringFlags_[i]) {
			incJobCounter();
		}
	}

	// Write data file.
	Writer::sendJob(new WriteJob(this, mainPartition, DATA_FILE, 0));

	// Compute and write indexes using strings.
	for (uint32_t i = 0; i < columnIds_.length(); ++i) {
		if (indexStringFlags_[i]) {
			Worker::sendJob(new IndexJob(this, mainPartition, i));
		}
	}
}

template class Sort<Indirector, RowComparator>;

// Computes the given index.
void TransientPartition::compute(PersistentPartitionGuard mainPartition, const uint32_t id) {
	SPARROW_ENTER("TransientPartition::compute");
	ReadGuard guard(lock_);
	const uint32_t rows = getRecords();

#ifndef NDEBUG
	uint64_t tstart = my_micro_time();
#endif

	// Sort index indirector.
	Indirector* indirector = new Indirector();
	for (uint32_t i = 0; i < rows; ++i) {
		indirector->append(i);
	}
	const RowComparator comparator(*this, columnIds_[id]);
	Sort<Indirector, RowComparator>::quickSort(*indirector, comparator, 0, rows);
#ifndef NDEBUG
	const uint32_t indexId = indexIds_[id];
	const Str duration(Str::fromDuration((my_micro_time() - tstart) / 1000));
	DBUG_PRINT("sparrow_transient", ("Computed index %u of transient partition %s.%s.%llu in %s", indexId,
		master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), duration.c_str()));
#endif

	// Send job to write generated data.
	Writer::sendJob(new WriteJob(this, mainPartition, id, indirector));
}

// Generates and writes a given index or data file. For data files, indirector is NULL and id = DATA_FILE.
void TransientPartition::write(PersistentPartitionGuard mainPartition, const uint32_t id, const Indirector* indirector) _THROW_(SparrowException) {
	SPARROW_ENTER("TransientPartition::write");
#ifndef NDEBUG
	const uint64_t tstart = my_micro_time();
#endif

	// If id == DATA_FILE, we write the data file.
	const bool isDataFile = id == DATA_FILE;
	const bool newFile = mainPartition->getDataRecords() == 0;
	const uint32_t fileId = isDataFile ? DATA_FILE : indexIds_[id];
	const uint32_t rows = getRecords();

	ReadGuard guard(lock_);

	// Prepare tree construction.
	TreeNodes nodes;
	uint32_t nodeSize = 0;
	if (!isDataFile) {
		const ColumnIds& columnIds = columnIds_[id];
		ColumnPos	columnPos;
		getColumnPos(columnPos, columnIds);
		uint32_t start = 0;
		uint32_t previousRow = 0;
		SYSxvector<uint32_t> count;
		for (uint32_t row = 0; row < rows; ++row) {
			const uint32_t currentRow = (*indirector)[row];
			if (row == 0) {
				start = row;
			} else {
				if (compare(columnPos, previousRow, currentRow, false) != 0) {
					count.append(row - start);
					start = row;
				}
			}
			previousRow = currentRow;
		}
		count.append(rows - start);
		const uint32_t nDistinct = count.length();
		RecordWriter treeRecordWriter(accessors_, &columnIds);
		nodeSize = treeRecordWriter.getSize() + 8;	// TODO row size. We need two row numbers in nodes
		nodes = TreeNodes(nDistinct);
		uint64_t k = 0;
		start = 0;
		for (uint32_t i = 0; i < nDistinct; ++i) {
			const uint32_t end = start + count[i];
			if (static_cast<uint64_t>(nDistinct) * i >= k) {
				k += nDistinct;
				nodes.append(TreeNode(start, end - 1));
			}
			start = end;
		}

		// Make sure the last node is the last element of the list.
		if (nodes.last().getEnd() + 1 < rows) {
			nodes.last() = TreeNode(rows - count.last(), count.last() - 1);
		}
		assert(nodes.length() == nDistinct);
	}

	assert(getColumnAlterSerial() == mainPartition->getColumnAlterSerial() && getEmptyColumns().containsTheSame(mainPartition->getSkippedColumns()) == true);

	// Determine stored columns
	ColumnIds	columnsIds;
	if (isDataFile)	{
		// Build the list of valid columns (= columns that are not empty)
		for (uint32_t i=0; i<accessors_.length(); ++i) {
			if (!(emptyColumnsIds_.contains(accessors_[i]->getColumnId()))) {
				columnsIds.append(accessors_[i]->getColumnId());
			}
		}
	} else {
		columnsIds = columnIds_[id];
	}

	RecordWriter recordWriter(accessors_, &columnsIds);
	const uint32_t recordSize = isDataFile ? recordWriter.getSize() : 4;	// TODO row size.
	const TimePeriod period = getPeriodNoLock();
	const uint64_t recordOffset = mainPartition->getDataRecords();

	// Start writing file.
	char filename[FN_REFLEN];
	if (isDataFile) {
		mainPartition->getFileName(fileId, filename);
	} else {
		master_->getFileName(PersistentPartition::currentVersion_, getFilesystem(), period, fileId, getSerial(), mainPartition->getSerial(), filename);
	}
	const PartitionFile partitionFile(*mainPartition, fileId);
	const SimpleWriteCacheHint writeHint(partitionFile, 3);
	FileWriter writer(filename, isDataFile ? FILE_TYPE_DATA : FILE_TYPE_INDEX, (!isDataFile || newFile) ? FILE_MODE_CREATE : FILE_MODE_UPDATE,
		isDataFile ? &writeHint : 0, isDataFile ? (DataFileHeader::size() + recordOffset * recordSize) : 0);
	const uint64_t save = writer.getFileSize();
	if (isDataFile) {
		if (newFile) {
			const DataFileHeader header(recordSize, rows, stringOffset_, stringSize_, period.getMin(), period.getMax());
			writer << header;
		}

		// Write data records.
		for (uint32_t row = 0; row < rows; ++row) {
			recordWriter.write(writer, row);
		}
	} else {
		const uint32_t nNodes = nodes.length();
		const IndexFileHeader header(fileId, recordSize, rows, nodeSize, nNodes, period.getMin(), period.getMax());
		writer << header;

		// Write index records (row numbers in data file).
		for (uint32_t row = 0; row < rows; ++row) {
			writer << static_cast<uint32_t>(recordOffset + (*indirector)[row]);
		}

		// Write tree.
		const TreeOrder& treeOrder = TreeOrder::get(nNodes);
		RecordWriter treeRecordWriter(accessors_, &columnIds_[id]);
		for (uint32_t i = 0; i < nNodes; ++i) {
			const uint32_t inode = treeOrder.getListIndex(i, nNodes);
			assert(inode < nNodes);
			const TreeNode& node = nodes[inode];
			const uint32_t start = node.getStart();
			const uint32_t end = node.getEnd();
			writer << start << end;
			treeRecordWriter.write(writer, (*indirector)[start]);
		}
	}
	writer.write();
	if (isDataFile && !newFile) {
		const TimePeriod mainPeriod = mainPartition->getPeriod();
		const DataFileHeader header(recordSize, mainPartition->getDataRecords() + rows, stringOffset_, stringSize_,
			std::min(mainPeriod.getMin(), period.getMin()), std::max(mainPeriod.getMax(), period.getMax()));
		writer.seek(0, header.size());
		writer << header;
		writer.write();
	}
	const uint64_t size = writer.getFileSize() - save;
	if (isDataFile) {
		Atomic::add64(&dataSize_, size);
	} else {
		Atomic::add64(&indexSize_, size);
	}
#ifndef NDEBUG
	const Str duration(Str::fromDuration((my_micro_time() - tstart) / 1000));
	if (fileId == DATA_FILE) {
		DBUG_PRINT("sparrow_transient", ("Written data of transient partition %s.%s.%llu to main partition %llu in %s", 
			master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), static_cast<ulonglong>(mainPartition->getSerial()), duration.c_str()));
	} else {
		DBUG_PRINT("sparrow_transient", ("Written index %u of partition %s.%s.%llu in %s", fileId,
			master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), duration.c_str()));
	}
#endif
}

void TransientPartition::endFlush(PersistentPartitionGuard mainPartition) {
	if (decJobCounter()) {
		try {
			bool	mutated = mutate(mainPartition);
			getMaster()->allowUpdate();
			getMaster()->endFlush(mainPartition->getSerial());
			if (!mutated) {
				resetFlush();
				flush(flushTimestamp_);
			}
		} catch(const SparrowException& e) {
			getMaster()->allowUpdate();
			getMaster()->endFlush(mainPartition->getSerial());
			throw;
		}
	}
}

bool TransientPartition::mutate(PersistentPartitionGuard mainPartition) _THROW_(SparrowException) {
	SPARROW_ENTER("TransientPartition::mutate");
	DBUG_PRINT("sparrow_transient", ("Mutating transient partition %s.%s.%llu to a persistent partition",
		master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial())));
	bool	mutated = false;
	TransientMutationGuard mutationGuard;
	const bool newMain = mainPartition->getDataRecords() == 0;
	if (errors_ == 0) {
		// Create a new persistent partition from the transient one for the indexes.
		PersistentPartition* newPartition = new PersistentPartition(PersistentPartition::currentVersion_, master_.get(), getSerial(), mainPartition.get(),
			getFilesystem(), getIndexAlterSerial(), getColumnAlterSerial(),
			getPeriodNoLock(), getRecords(), 0, getIndexSize(), 0, mainPartition->getDataRecords(), mainPartition->getSkippedColumns());
		// Use an AutoPtr to automatically delete this new partition if an exception is thrown
		AutoPtr<PersistentPartition> newPartitionGuard(newPartition);

		// Alterations could occurred while the partition was transient.
		bool doAlter = false;
		{
			WriteGuard guard(master_->getLock());
			doAlter = master_->getIndexAlterSerial() > getIndexAlterSerial();

			// Update this master file.
			mainPartition->addDataSize(getDataSize());
			mainPartition->addDataRecords(getRecords());
			master_->mutatePartition(this, mainPartition.get(), newPartition);
			newPartitionGuard.release();	// Release the AutoPtr so that the partition does not get deleted when the object goes out of scope.

			// Write this master file to disk.
			master_->toDisk();
			mutated = true;
		}
		if (doAlter) {
			master_->startIndexAlter(false);
		}

		// Try to coalesce.
		master_->coalesce();
	} else {
		if (newMain) {
			// This will drop the newly created main partition.
			mainPartition->releaseRef();
		}
		if (++flush_tries_ == 2) {
			WriteGuard guard(master_->getLock());
			master_->mutatePartition(this, 0, 0);
			mutated = true;
		}
	}
	return mutated;
}

// Wait until all on-going flushes are completed.
// STATIC
void TransientPartition::waitForFlushs() {
	SPARROW_ENTER("TransientPartition::waitForFlushs");
	Guard flushGuard(TransientPartition::condLock_);
	while (flushs_ > 0) {
		if (flushCond_.wait(1000, true)) {
			break;
		}
	}
}

// Navigation methods. Data are ordered only by timestamp, so using another index
// requires an indirector built when the transient partition has been "snapshoted";
// see QueryInfo::updateIndirector().

template class BinarySearch<ComparatorTransient>;

Position TransientPartition::indexFind(Context& context, const uint32_t partition, const KeyValue& key,
	const SearchFlag searchFlag) const {
	SPARROW_ENTER("TransientPartition::indexFind");
	QueryInfo& queryInfo = context.getQueryInfo();
	const PartitionSnapshot* snapshot = queryInfo.getSnapshot(this);
	if (snapshot == NULL) {
		spw_print_information("[indexFind] No snapshot for partition %llu (%llu) for table %s.%s. Partition info: done %u, flushing %u, errors %u", 
			static_cast<ulonglong>(this->getSerial()), static_cast<ulonglong>(this->getDataSerial()), master_->getDatabase().c_str(), 
			master_->getTable().c_str(), this->done_, this->flush_, this->errors_);
		return Position(partition);
	}
	const uint32_t rows = snapshot->getRows();
	if (rows == 0) {
		spw_print_information("[indexFind] Empty snapshot for partition %llu (%llu) for table %s.%s. Partition info: done %u, flushing %u, errors %u", 
			static_cast<ulonglong>(this->getSerial()), static_cast<ulonglong>(this->getDataSerial()), master_->getDatabase().c_str(),
			master_->getTable().c_str(), this->done_, this->flush_, this->errors_);
		return Position(partition);
	}
	assert(rows > 0);
	ComparatorTransient comparator(context, *this, snapshot->getIndirector(), key, queryInfo.getCurrentKey().getKey());
	const Position pos(partition, BinarySearch<ComparatorTransient>::find(comparator, 0, rows, searchFlag));
	if (pos.isValid()) {
		const uint32_t row = pos.getRow();
		return Position(partition, snapshot->getIndirector()[row], row);
	} else {
		return pos;
	}
}

Position TransientPartition::indexFirst(Context& context, const uint32_t partition) const {
	SPARROW_ENTER("TransientPartition::indexFirst");
	const QueryInfo& queryInfo = context.getQueryInfo();
	const PartitionSnapshot* snapshot = queryInfo.getSnapshot(this);
	if (snapshot == NULL) {
		spw_print_information("[indexFirst] No snapshot for partition %llu (%llu) for table %s.%s. Partition info: done %u, flushing %u, errors %u", 
			static_cast<ulonglong>(this->getSerial()), static_cast<ulonglong>(this->getDataSerial()), master_->getDatabase().c_str(),
			master_->getTable().c_str(), this->done_, this->flush_, this->errors_);
		return Position(partition);
	}
	const uint32_t rows = snapshot->getRows();
	if (rows == 0) {
		spw_print_information("[indexFirst] Empty snapshot for partition %llu (%llu) for table %s.%s. Partition info: done %u, flushing %u, errors %u", 
			static_cast<ulonglong>(this->getSerial()), static_cast<ulonglong>(this->getDataSerial()), master_->getDatabase().c_str(),
			master_->getTable().c_str(), this->done_, this->flush_, this->errors_);
		return Position(partition);
	}
	assert(snapshot->getRows() > 0);
	return Position(partition, snapshot->getIndirector()[0], 0);
}

Position TransientPartition::indexLast(Context& context, const uint32_t partition) const {
	SPARROW_ENTER("TransientPartition::indexLast");
	const QueryInfo& queryInfo = context.getQueryInfo();
	const PartitionSnapshot* snapshot = queryInfo.getSnapshot(this);
	if (snapshot == NULL) {
		spw_print_information("[indexLast] No snapshot for partition %llu (%llu) for table %s.%s. Partition info: done %u, flushing %u, errors %u", 
			static_cast<ulonglong>(this->getSerial()), static_cast<ulonglong>(this->getDataSerial()), master_->getDatabase().c_str(),
			master_->getTable().c_str(), this->done_, this->flush_, this->errors_);
		return Position(partition);
	}
	const uint32_t rows = snapshot->getRows();
	if (rows == 0) {
		spw_print_information("[indexLast] Empty snapshot for partition %llu (%llu) for table %s.%s. Partition info: done %u, flushing %u, errors %u", 
			static_cast<ulonglong>(this->getSerial()), static_cast<ulonglong>(this->getDataSerial()), master_->getDatabase().c_str(),
			master_->getTable().c_str(), this->done_, this->flush_, this->errors_);
		return Position(partition);
	}
	assert(rows > 0);
	return Position(partition, snapshot->getIndirector()[rows - 1], rows - 1);
}

Position TransientPartition::indexNext(Context& context, const Position& position) const {
	SPARROW_ENTER("TransientPartition::indexNext");
	const QueryInfo& queryInfo = context.getQueryInfo();
	const PartitionSnapshot* snapshot = queryInfo.getSnapshot(this);
	if (snapshot == NULL) {
		spw_print_information("[indexNext] No snapshot for partition %llu (%llu) for table %s.%s. Partition info: done %u, flushing %u, errors %u", 
			static_cast<ulonglong>(this->getSerial()), static_cast<ulonglong>(this->getDataSerial()), master_->getDatabase().c_str(),
			master_->getTable().c_str(), this->done_, this->flush_, this->errors_);
		return Position(position.getPartition());
	}
	const uint32_t rows = snapshot->getRows();
	if (rows == 0) {
		spw_print_information("[indexNext] Empty snapshot for partition %llu (%llu) for table %s.%s. Partition info: done %u, flushing %u, errors %u", 
			static_cast<ulonglong>(this->getSerial()), static_cast<ulonglong>(this->getDataSerial()), master_->getDatabase().c_str(),
			master_->getTable().c_str(), this->done_, this->flush_, this->errors_);
		return Position(position.getPartition());
	}
	assert(rows > 0);
	const uint32_t row = position.getIndexHint();
	if (row + 1 == rows) {
		return Position(position.getPartition());
	} else {
		return Position(position.getPartition(), snapshot->getIndirector()[row + 1], row + 1);
	}
}

Position TransientPartition::indexPrevious(Context& context, const Position& position) const {
	SPARROW_ENTER("TransientPartition::indexPrevious");
	const QueryInfo& queryInfo = context.getQueryInfo();
	const PartitionSnapshot* snapshot = queryInfo.getSnapshot(this);
	if (snapshot == NULL) {
		spw_print_information("[indexPrevious] No snapshot for partition %llu (%llu) for table %s.%s. Partition info: done %u, flushing %u, errors %u", 
			static_cast<ulonglong>(this->getSerial()), static_cast<ulonglong>(this->getDataSerial()), master_->getDatabase().c_str(),
			master_->getTable().c_str(), this->done_, this->flush_, this->errors_);
		return Position(position.getPartition());
	}
	const uint32_t rows = snapshot->getRows();
	if (rows == 0) {
		spw_print_information("[indexPrevious] Empty snapshot for partition %llu (%llu) for table %s.%s. Partition info: done %u, flushing %u, errors %u", 
			static_cast<ulonglong>(this->getSerial()), static_cast<ulonglong>(this->getDataSerial()), master_->getDatabase().c_str(),
			master_->getTable().c_str(), this->done_, this->flush_, this->errors_);
		return Position(position.getPartition());
	}
	assert(snapshot->getRows() > 0);
	const uint32_t row = position.getIndexHint();
	if (row == 0) {
		return Position(position.getPartition());
	} else {
		return Position(position.getPartition(), snapshot->getIndirector()[row - 1], row - 1);
	}
}

Position TransientPartition::moveNext(Context& context, const Position& position) const {
	SPARROW_ENTER("TransientPartition::moveNext");
	const QueryInfo& queryInfo = context.getQueryInfo();
	const PartitionSnapshot* snapshot = queryInfo.getSnapshot(this);
	if (snapshot == NULL) {
		spw_print_information("[moveNext] No snapshot for partition %llu (%llu) for table %s.%s. Partition info: done %u, flushing %u, errors %u", 
			static_cast<ulonglong>(this->getSerial()), static_cast<ulonglong>(this->getDataSerial()), master_->getDatabase().c_str(),
			master_->getTable().c_str(), this->done_, this->flush_, this->errors_);
		return Position(position.getPartition());
	}
	const uint32_t rows = snapshot->getRows();
	if (rows == 0) {
		spw_print_information("[moveNext] Empty snapshot for partition %llu (%llu) for table %s.%s. Partition info: done %u, flushing %u, errors %u", 
			static_cast<ulonglong>(this->getSerial()), static_cast<ulonglong>(this->getDataSerial()), master_->getDatabase().c_str(),
			master_->getTable().c_str(), this->done_, this->flush_, this->errors_);
		return Position(position.getPartition());
	}
	assert(rows > 0);
	const uint32_t newRow = position.getRow() + 1;
	if (newRow < rows) {
		return Position(position.getPartition(), newRow);
	} else {
		return Position(position.getPartition());
	}
}

Position TransientPartition::movePrevious(Context& context, const Position& position) const {
	SPARROW_ENTER("TransientPartition::movePrevious");
	assert(context.getQueryInfo().getSnapshot(this)->getRows() > 0);
	if (position.getRow() > 0) {
		return Position(position.getPartition(), position.getRow() - 1);
	} else {
		return Position(position.getPartition());
	}
}

Position TransientPartition::moveAbsolute(Context& context, const Position& position) const {
	SPARROW_ENTER("TransientPartition::moveAbsolute");
	const QueryInfo& queryInfo = context.getQueryInfo();
	const PartitionSnapshot* snapshot = queryInfo.getSnapshot(this);
	if (snapshot == NULL) {
		spw_print_information("[moveAbsolute] No snapshot for partition %llu (%llu) for table %s.%s. Partition info: done %u, flushing %u, errors %u", 
			static_cast<ulonglong>(this->getSerial()), static_cast<ulonglong>(this->getDataSerial()), master_->getDatabase().c_str(),
			master_->getTable().c_str(), this->done_, this->flush_, this->errors_);
		return Position(position.getPartition());
	}
	const uint32_t rows = snapshot->getRows();
	if (rows == 0) {
		spw_print_information("[moveAbsolute] Empty snapshot for partition %llu (%llu) for table %s.%s. Partition info: done %u, flushing %u, errors %u", 
			static_cast<ulonglong>(this->getSerial()), static_cast<ulonglong>(this->getDataSerial()), master_->getDatabase().c_str(),
			master_->getTable().c_str(), this->done_, this->flush_, this->errors_);
		return Position(position.getPartition());
	}
	assert(rows > 0);
	if (position.getRow() < rows) {
		return position;
	} else {
		return Position(position.getPartition());
	}
}

Position TransientPartition::moveFirst(Context& context, const uint32_t partition) const {
	SPARROW_ENTER("TransientPartition::moveFirst");
	assert(context.getQueryInfo().getSnapshot(this)->getRows() > 0);
	return Position(partition, 0);
}

Position TransientPartition::moveLast(Context& context, const uint32_t partition) const {
	SPARROW_ENTER("TransientPartition::moveLast");
	const QueryInfo& queryInfo = context.getQueryInfo();
	const PartitionSnapshot* snapshot = queryInfo.getSnapshot(this);
	if (snapshot == NULL) {
		spw_print_information("[moveLast] No snapshot for partition %llu (%llu) for table %s.%s. Partition info: done %u, flushing %u, errors %u", 
			static_cast<ulonglong>(this->getSerial()), static_cast<ulonglong>(this->getDataSerial()), master_->getDatabase().c_str(),
			master_->getTable().c_str(), this->done_, this->flush_, this->errors_);
		return Position(partition);
	}
	const uint32_t rows = snapshot->getRows();
	if (rows == 0) {
		spw_print_information("[moveLast] Empty snapshot for partition %llu (%llu) for table %s.%s. Partition info: done %u, flushing %u, errors %u", 
			static_cast<ulonglong>(this->getSerial()), static_cast<ulonglong>(this->getDataSerial()), master_->getDatabase().c_str(),
			master_->getTable().c_str(), this->done_, this->flush_, this->errors_);
		return Position(partition);
	}
	assert(rows > 0);
	return Position(partition, rows - 1);
}

uint32_t TransientPartition::recordsInRange(Context& context, const uint32_t partition, const key_range* minKey, const key_range* maxKey) const {
	SPARROW_ENTER("TransientPartition::recordsInRange");
	QueryInfo& queryInfo = context.getQueryInfo();
	const TableFields& fields = context.getShare().getMappedFields();
	const PartitionSnapshot* snapshot = queryInfo.getSnapshot(this);
	if (snapshot == NULL) {
		spw_print_information("[recordsInRange] No snapshot for partition %llu (%llu) for table %s.%s. Partition info: done %u, flushing %u, errors %u", 
			static_cast<ulonglong>(this->getSerial()), static_cast<ulonglong>(this->getDataSerial()), master_->getDatabase().c_str(),
			master_->getTable().c_str(), this->done_, this->flush_, this->errors_);
		return 0;
	}
	const uint32_t rows = snapshot->getRows();
	if (rows == 0) {
		spw_print_information("[recordsInRange] Empty snapshot for partition %llu (%llu) for table %s.%s. Partition info: done %u, flushing %u, errors %u", 
			static_cast<ulonglong>(this->getSerial()), static_cast<ulonglong>(this->getDataSerial()), master_->getDatabase().c_str(),
			master_->getTable().c_str(), this->done_, this->flush_, this->errors_);
		return 0;
	}
	assert(rows > 0);
	uint8_t* curKey = queryInfo.getCurrentKey().getKey();
	uint32_t count = 0;
	const bool incMin = (minKey != 0 && minKey->flag == HA_READ_KEY_EXACT);
	const bool incMax = (maxKey != 0 && maxKey->flag == HA_READ_AFTER_KEY);
	Position pos(0);
	for (uint32_t row = 0; row < rows; ++row) {
		pos.setRow(row);
		int cmpMin;
		if (minKey == 0) {
			cmpMin = 1;
		} else {
			const KeyValue minKeyValue(minKey);
			if (!readKey(context, pos, true, minKeyValue.getMap(), curKey, true)) {
				return 0;
			}
			cmpMin = queryInfo.compareKeys(fields, queryInfo.getCurrentKey(), minKeyValue);
		}
		int cmpMax;
		if (maxKey == 0) {
			cmpMax = -1;
		} else {
			const KeyValue maxKeyValue(maxKey);
			if (!readKey(context, pos, true, maxKeyValue.getMap(), curKey, true)) {
				return 0;
			}
			cmpMax = queryInfo.compareKeys(fields, queryInfo.getCurrentKey(), maxKeyValue);
		}
		if ((cmpMin > 0 || (incMin && cmpMin == 0))
			&& (cmpMax < 0 || (incMax && cmpMax == 0))) {
			count++;
		}
	}
	return count;
}

// Reads a given index record and sets MySQL fields.
bool TransientPartition::readKey(Context& context, const Position& position, const bool forward,
	const key_part_map keyPartMap, uint8_t* buffer, const bool keyFormat) const {
	SPARROW_ENTER("TransientPartition::readKey");
	assert(position.isValid());
	uint32_t row = position.getRow();
	const QueryInfo& queryInfo = context.getQueryInfo();
	const TableFields& fields = context.getShare().getMappedFields();
	const KEY& keyInfo = queryInfo.getKeyInfo();
	for (uint32_t i = 0; i < keyInfo.user_defined_key_parts; ++i) {
		if ((keyPartMap & (1 << i)) == 0) {
			continue;
		}
		const KEY_PART_INFO& keyPartInfo = keyInfo.key_part[i];
		int fieldId = keyPartInfo.fieldnr - 1;
		const FieldBase& field = *fields[fieldId];
		const ColumnAccessor& accessor = *accessors_[fieldId];
		field.readTransient(accessor.getValue(row), accessor.isNull(row), buffer, keyFormat);
		if (keyFormat) {
			buffer += (field.isNullable() ? 1 : 0) + field.getLength(true);
		}
	}
	return true;
}

// Reads a given data record and sets MySQL fields.
bool TransientPartition::readData(Context& context, const Position& position, uint8_t* buffer, const BlockCacheHint& hint) const {
	SPARROW_ENTER("TransientPartition::readData");
	assert(position.isValid());
	const uint32_t row = position.getRow();
	TABLE& table = context.getTable();

	// In case of update, need to read all fields.
	const bool forUpdate = !bitmap_is_clear_all(table.write_set);
	memset(buffer, 0, table.s->null_bytes);
	const TableFields& fields = context.getShare().getMappedFields();
	const uint32_t n = fields.length();
	for (uint32_t i = 0; i < n; ++i) {
		if (forUpdate || bitmap_is_set(table.read_set, i)) {
			const ColumnAccessor& accessor = *accessors_[i];
			fields[i]->readTransient(accessor.getValue(row), accessor.isNull(row), buffer, false);
		}
	}
	return true;
}

bool TransientPartition::updateData(Context& context, const Position& position, const uint8_t* buffer) {
	SPARROW_ENTER("TransientPartition::updateData");
	assert(position.isValid());
	WriteGuard guard(lock_, false, true);
	const uint32_t row = position.getRow();
	const TableFields& fields = context.getShare().getMappedFields();
	const uint32_t n = fields.length();
	for (uint32_t i = 0; i < n; ++i) {
		ColumnAccessor& accessor = *accessors_[i];
		if (!context.isUpdatableColumn(accessor.getColumnId())) {
			continue;
		}
		const FieldBase& field = *fields[i];
		uint64_t v;
		if (field.readMySqlTransient(buffer, v)) {
			accessor.setNull(row);
		} else {
			accessor.writeValue(row, v);
			if (field.isNullable()) {
				accessor.resetNull(row);
			}
		}
	}
	return true;
}

// STATIC
// Force flush of some transient partitions to free space in the tuple buffer. Start with partitions bigger than 1MB, and among those, flush the oldest ones first.
//	Then, if it's not enough, flush smaller partitions (size threshold = 500KB), and if that is not enough, divide again the threshold by 2 and iterate again. And so on.
//  Flushing small partitions will pollute IOs while not freeing much memory. 
uint64_t TransientPartition::flushOldestPartitions(const uint64_t& sizeToFlush) {
	if (sizeToFlush == 0)
		return 0;

	Guard	guard(PartLock_);
	uint64_t	timestamp = Scheduler::now();
	uint32_t	n = 0;
	DBUG_PRINT("sparrow_transient", ("Making room in the tuple buffer: size to free %llu, nb partitions %u, nb part being flushed %u (%llu).", 
		static_cast<ulonglong>(sizeToFlush), AllTransPartitions_.entries(), FlushingPartitions_.entries(), static_cast<ulonglong>(sizeFlushing_)));
	uint64_t	sizeFlushed = 0, sizeThreshold = 1024*1024;
	while (sizeFlushed < sizeToFlush) {
		uint32_t	i = 0; 
		while (sizeFlushed < sizeToFlush && i < AllTransPartitions_.entries()) {
			TransientPartition*		partition = AllTransPartitions_[i];
			assert(partition != NULL);
			const uint64_t	partSize = partition->getCachedSize();
			if (partSize < sizeThreshold) {
				++i;
				continue;
			}
			flushingPartitionNoLock(partition);
			sizeFlushed += partSize;
			++n;
			DBUG_PRINT("sparrow_transient", ("Forcing flush of partition %u/%u, %s.%s.%llu, size %llu.",
				i, AllTransPartitions_.entries(), partition->getMaster()->getDatabase().c_str(), partition->getMaster()->getTable().c_str(), 
				static_cast<ulonglong>(partition->getSerial()), static_cast<ulonglong>(partSize)));
			partition->flushTimestamp_ = timestamp;
			Scheduler::addTask(new FlushTask(partition->getMaster(), partition->getSerial()), timestamp);
			Atomic::inc64(&SparrowStatus::get().flushForced_);
		}
		if (AllTransPartitions_.isEmpty()) {
			break;
		}
			sizeThreshold /= 2;
		}
	DBUG_PRINT("sparrow_transient", ("Triggered flush of %u partitions for a total size of %llu (aimed for %llu). Total size of partitions being flushed %llu", 
		n, static_cast<ulonglong>(sizeFlushed), static_cast<ulonglong>(sizeToFlush), static_cast<ulonglong>(sizeFlushing_)));
	return sizeFlushed;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// SizeGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

SizeGuard::~SizeGuard() {
	SPARROW_ENTER("SizeGuard::~SizeGuard");
	const int64_t	partSize = partition_.getSize();
	const int64_t delta = partSize - initialSize_;
	volatile uint64_t& tupleBufferSize = SparrowStatus::get().tupleBufferSize_;
	{
		Guard guard(TransientPartition::condLock_);
		DBUG_PRINT("sparrow_transient", ("Tuple buffer size changed from %llu to %llu (delta %lld)", static_cast<ulonglong>(tupleBufferSize), 
			static_cast<ulonglong>(tupleBufferSize + delta), static_cast<longlong>(delta)));
		tupleBufferSize += delta;
		TransientPartition::updateFlushingSize(&partition_, delta);

		const uint64_t	thresholdSize = (sparrow_max_tuple_buffer_size * sparrow_tuple_buffer_threshold) / 100;
		if (tupleBufferSize < thresholdSize) {
			TransientPartition::insertCond_.signalAll(true);
			}

		const uint64_t	sizeFlushing = TransientPartition::getSizeFlushing();
		assert(tupleBufferSize >= sizeFlushing);
		uint64_t	usedTupleBufferSize = tupleBufferSize - sizeFlushing;
		DBUG_PRINT("sparrow_transient", ("Used tuple buffer size: %llu", static_cast<ulonglong>(usedTupleBufferSize)));
		if (usedTupleBufferSize >= thresholdSize) {
			TransientPartition::flushOldestPartitions(usedTupleBufferSize - thresholdSize + 1);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PartitionSnapshot
//////////////////////////////////////////////////////////////////////////////////////////////////////

template class Sort<ConcurrentIndirector, RowComparator>;

void PartitionSnapshot::updateIndirector(const uint32_t index) {
	assert(rows_ > 0);
	if (index == DATA_FILE) {
		delete indirector_;
		indirector_ = 0;
	} else {
		if (indirector_ == 0) {
			indirector_ = new ConcurrentIndirector();
		} else {
			indirector_->clear();
		}

		// Initializes indirector if we use an index other than the timestamp index.
		// This indirector is used for searching quickly in the transient partition.
		for (uint32_t row = 0; row < rows_; ++row) {
			indirector_->append(row);
		}
		const RowComparator comparator(*partition_.get(), partition_->getColumnIds(index));
		Sort<ConcurrentIndirector, RowComparator>::quickSort(*indirector_, comparator, 0, rows_);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ComparatorTransient
//////////////////////////////////////////////////////////////////////////////////////////////////////

ComparatorTransient::ComparatorTransient(Context& context, const TransientPartition& partition, const ConcurrentIndirector& indirector, const KeyValue& key, uint8_t* buffer)
	: context_(context), partition_(partition), queryInfo_(context.getQueryInfo()), fields_(context.getShare().getMappedFields()), key_(key),
	indirector_(indirector), tempKey_(buffer, key.getMap()) {
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// RecordWriter
//////////////////////////////////////////////////////////////////////////////////////////////////////

RecordWriter::RecordWriter(const ColumnAccessors& accessors, const ColumnIds* columnIds) {
	if (columnIds == 0) {
		// Data file: all columns.
		accessors_ = accessors;
	} else {
		// Index file or data files with skipped columns.
		accessors_.resize(columnIds->length());
		for (uint32_t i = 0; i < columnIds->length(); ++i) {
			uint32_t	id = (*columnIds)[i];
			uint32_t	j = 0;
			for (; j < accessors.entries(); ++j) {
				if (accessors[j]->getColumnId() == id)
					break;
			}
			assert(j < accessors.entries());
			if (j == accessors.entries()) {
				throw SparrowException::create(false, "Can't find column Id %u in valid column list. Failed to build RecordWriter.", id);
			}
			accessors_.append(accessors[j]);
		}
	}
	bits_ = 0;
	size_ = 0;
	for (uint32_t i = 0; i < accessors_.length(); ++i) {
		const Column& column = accessors_[i]->getColumn();
		size_ += column.getDataSize();
		bits_ += column.getBits();
	}
	size_ += (bits_ + 7) / 8;
}

void RecordWriter::write(ByteBuffer& buffer, const uint32_t row) _THROW_(SparrowException) {
	// Write bits first.
	const uint8_t bitLength = (bits_ + 7) / 8;
	uint8_t bits[SPARROW_MAX_BIT_SIZE];
	if (bitLength > 0) {
		memset(bits, 0, sizeof(bits));
		uint32_t n = 0;
		for (uint32_t i = 0; i < accessors_.length(); ++i) {
			const uint8_t bitValues = accessors_[i]->getBitValues(row);
			const uint32_t nbits = accessors_[i]->getColumn().getBits();
			for (uint32_t j = 0; j < nbits; ++j) {
				if (bitValues & (1 << j)) {
					bits[n / 8] |= (1 << (n % 8));
				}
				n++;
			}
		}
		buffer << ByteBuffer(bits, bitLength);
	}

	// Write column values.
	for (uint32_t i = 0; i < accessors_.length(); ++i) {
		accessors_[i]->write(buffer, row);
	}
}

}

