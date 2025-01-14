/*
	Partition coalescing.
*/

#include "coalescing.h"

#include "../engine/log.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CoalescingWorker
//////////////////////////////////////////////////////////////////////////////////////////////////////

const JobThreadFactory CoalescingWorker::factory_("CoalescingWorker");
JobThreadPool* CoalescingWorker::threadPool_ = 0;
Lock CoalescingWorker::lock_(true, "CoalescingWorker::lock_");

// STATIC
void CoalescingWorker::initialize() _THROW_(SparrowException) {
	// The queue is not bulk because we want jobs to be distributed across workers.
	threadPool_ = new JobThreadPool(CoalescingWorker::factory_, &sparrow_max_coalescing_threads,
		&SparrowStatus::get().coalescingThreads_, "CoalescingWorker::Queue", false);
}

// STATIC
void CoalescingWorker::shutdown() {
	if (threadPool_ != 0) {
		threadPool_->stop();
		delete threadPool_;
		threadPool_ = 0;
	}
}

// STATIC
void CoalescingWorker::sendJob(Job* job) {
	threadPool_->send(job);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CoalescingMainTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

void CoalescingMainTask::run(const uint64_t timestamp) _THROW_(SparrowException) {
	SPARROW_ENTER("CoalescingMainTask::run");
	try {
		const PersistentPartitions allPartitions(partitions_);
		PersistentPartition* coalescedPartition = Coalescing::generateDataFile(partitions_, *this);
		Atomic::inc64(&SparrowStatus::get().coalescingMainTaskProcessed_);
		if (isStopping()) {
			delete coalescedPartition;
		} else {
			WriteGuard guard(get()->getLock());		// Because each created CoalescingIndexTask calls Master::registerCoalescingTask() which requires the lock.
			Coalescing::triggerIndexCoalescing(get(), allPartitions, partitions_, coalescedPartition, indexIds_);
		}
	} catch(const SparrowException& e) {
		spw_print_error("Failed to coalesce data files for table %s.%s: %s",
			get()->getDatabase().c_str(), get()->getTable().c_str(), e.getText());
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CoalescingIndexTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

void CoalescingIndexTask::run(const uint64_t timestamp) _THROW_(SparrowException) {
	SPARROW_ENTER("CoalescingIndexTask::run");
	try {
		if ( Coalescing::generateIndexFile(partitions_, index_, coalescedPartition_, this) == 0 ) {
			flags_->setAborted();
		} else {
			Atomic::inc64(&SparrowStatus::get().coalescingIndexTaskProcessed_);
		}
	} catch(const SparrowException& e) {
		flags_->setAborted();
		spw_print_error("Failed to coalesce index %u files for table %s.%s: %s",
			index_, get()->getDatabase().c_str(), get()->getTable().c_str(), e.getText());
	}
	finished();
	run_ = true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Coalescing
//////////////////////////////////////////////////////////////////////////////////////////////////////

// STATIC
void Coalescing::triggerIndexCoalescing(Master* master, const PersistentPartitions& allPartitions,
	const PersistentPartitions& partitions, PersistentPartition* coalescedPartition, const IndexIds& indexIds) _THROW_(SparrowException) {
	SPARROW_ENTER("Coalescing::triggerIndexCoalescing");
	const uint32_t nbIndexes = indexIds.length();
	if (coalescedPartition->getRecords() > 0 && nbIndexes > 0) {
		char	name[256];
		snprintf(name, sizeof(name), "CoalescingIndexTask_%llu", static_cast<ulonglong>(coalescedPartition->getSerial()));
		CoalescingFlags*	flags = new CoalescingFlags(name);
		for (uint32_t i = 0; i < nbIndexes; ++i) {
			Scheduler::addTask(new CoalescingIndexTask(master, allPartitions, partitions, coalescedPartition, indexIds[i], flags));
		}
	} else {
		master->coalescingDone(coalescedPartition, allPartitions);
	}
}

// Appends the data files of the given persistent partitions and returns the coalesced persistent partition. 
// STATIC
PersistentPartition* Coalescing::generateDataFile(PersistentPartitions& partitions,
	const Task& task) _THROW_(SparrowException) {
	SPARROW_ENTER("Coalescing::generateDataFile");
#ifndef NDEBUG
	const uint64_t tstart = my_micro_time();
	FileHeader debugHeader;
#endif
	const uint32_t nbPartitions = partitions.length();
	assert(nbPartitions > 1);
	PersistentPartition& firstPartition = *partitions[0];
	assert(firstPartition.getVersion() < PersistentPartition::appendVersion_);
	Master& master = firstPartition.getMaster();
	UpdateGuard updateGuard(master);
	uint64_t minTimestamp = ULLONG_MAX;
	uint64_t maxTimestamp = 0;
	uint32_t records = 0;
	for (uint32_t i = 0; i < nbPartitions; ++i) {
		const PersistentPartition& partition = *partitions[i];
		records += partition.getRecords();
		const TimePeriod period = partition.getPeriod();
		minTimestamp = std::min(minTimestamp, period.getMin());
		maxTimestamp = std::max(maxTimestamp, period.getMax());
	}
	TableFieldsGuard fieldsGuard;
	TableFields& fields = fieldsGuard.get();
	PersistentPartition* coalescedPartition = 0;
	const uint32_t columnAlterSerial = firstPartition.getColumnAlterSerial();
	{
		WriteGuard guard(master.getLock());
		master.getFields(columnAlterSerial, true, fields, NULL);
		coalescedPartition = master.newPersistentPartition(firstPartition.getVersion(), SAME_AS_SERIAL, FileUtil::chooseFilesystem(false),
			TimePeriod(minTimestamp, maxTimestamp), records, firstPartition.getIndexAlterSerial(), columnAlterSerial, records, 0, firstPartition.getSkippedColumns());
	}
	const uint32_t nbFields = fields.length();
	uint64_t discardedRecords = 0;
	AutoPtr<PersistentPartition> partitionGuard(coalescedPartition);
	const SerialRecordWrapper recordWrapper(columnAlterSerial, DATA_FILE, false, fields, 0);
	char filename[FN_REFLEN];
	{
		const PartitionFile partitionFile(*coalescedPartition, DATA_FILE);
		const SimpleWriteCacheHint writeHint(partitionFile, 0);
		FileWriter writer(coalescedPartition->getFileName(DATA_FILE, filename), FILE_TYPE_DATA, FILE_MODE_CREATE, &writeHint);
		PersistentPartition::writeFileFormat(writer);

		// Write records.
		BinBuffer binBuffer;
		uint32_t i = 0;
		uint8_t bitArray[SPARROW_MAX_BIT_SIZE];
		while (i < partitions.length()) {
			const PersistentPartition& partition = *partitions[i];
			bool opened = false;
			try {
				PartitionReaderGuard readerGuard(partition, DATA_FILE, false, BlockCacheHint::largeForward0_);
				PartitionReader& reader = readerGuard.get();
				PartitionReaderGuard stringReaderGuard(partition, DATA_FILE, true, BlockCacheHint::largeAround0_);
				PartitionReader& stringReader = stringReaderGuard.get();
				opened = true;
				const FileHeaderBase& header = reader.getHeader();
				reader.seekRecord(0);
				const uint64_t nbRecords = header.getRecords();
				for (uint64_t j = 0; j < nbRecords; ++j) {
					if ((j % 16384) == 0 && task.isStopping()) {
						return partitionGuard.release();
					}
					recordWrapper.readBits(reader, bitArray);
					writer << ByteBuffer(bitArray, recordWrapper.getBitSize());
					int bitOffset = 0;
					for (uint32_t k = 0; k < nbFields; ++k) {
						const FieldBase* field = fields[k];
						if (field == 0) {
							continue;
						}
						const int nbits = field->getBits();
						uint32_t bits = 0;
						for (int b = 0; b < nbits; ++b, ++bitOffset) {
							bits |= ((bitArray[bitOffset / 8] & (1 << (bitOffset % 8))) == 0 ? 0 : 1) << b;
						}
						field->copy(reader, stringReader, bits, writer, &binBuffer);
					}
				}
				++i;
			} catch(const SparrowException& e) {
				if (!opened) {
					discardedRecords += partition.getRecords();
					partitions.removeAt(i);
				} else {
					throw e;
				}
			}
		}

		// Remove discarded records.
		coalescedPartition->decRecords(static_cast<uint32_t>(discardedRecords));

		// Write binary data.
		const uint64_t offset = writer.getFileOffset();
		writer << binBuffer;
		const uint64_t binSize = writer.getFileOffset() - offset;

		// Padding to put header at the end of the file, taking into account its adjusted size.
		const TimePeriod& period = coalescedPartition->getPeriod();
		const FileHeader header(binSize, 0, false, 0, recordWrapper.getSize(), coalescedPartition->getRecords(),
			DATA_FILE, period.getMin(), period.getMax());
#ifndef NDEBUG
		debugHeader = header;
#endif
		const uint64_t dataSize = header.getTotalSize();
		coalescedPartition->setDataSize(dataSize);
		const uint64_t target = dataSize - FileHeader::size();
		while (writer.getFileOffset() < target) {
			writer << static_cast<uint8_t>(0);
		}

		// Write header.
		writer << header;
		writer.write();
	}
#ifndef NDEBUG
	const Str duration(Str::fromDuration((my_micro_time() - tstart) / 1000));
	const Str sizeTotal(Str::fromSize(debugHeader.getTotalSize()));
	const Str sizeRecords(Str::fromSize(debugHeader.getRecordSize() * debugHeader.getRecords()));
	const Str sizeBin(Str::fromSize(debugHeader.getBinSection().getSize()));
	DBUG_PRINT("sparrow_coalescing", ("Written data file of coalesced partition %s.%s.%llu (%u partitions, %s total = %s records + %s bin) in %s", 
			master.getDatabase().c_str(), master.getTable().c_str(), static_cast<ulonglong>(coalescedPartition->getSerial()), nbPartitions,
			sizeTotal.c_str(), sizeRecords.c_str(), sizeBin.c_str(), duration.c_str()));
#endif
	return partitionGuard.release();
}

template class Sort<KeyIndirector, CoalescingComparator>;
template class BinarySearch<CoalescingKeyComparator>;

// Merge the index files of the given partitions for the given index.
// STATIC
uint64_t Coalescing::generateIndexFile(const PersistentPartitions& partitions, const uint32_t index, PersistentPartition* coalescedPartition,
	const Task* task) _THROW_(SparrowException) {
	SPARROW_ENTER("Coalescing::generateIndexFile");
#ifndef NDEBUG
	const uint64_t tstart = my_micro_time();
	FileHeader debugHeader;
#endif
	PersistentPartition& firstPartition = *partitions[0];
	Master& master = firstPartition.getMaster();
	TableFieldsGuard fieldsGuard;
	TableFields& fields = fieldsGuard.get();
	ColumnIds columnIds;
	const uint32_t columnAlterSerial = firstPartition.getColumnAlterSerial();
	{
		WriteGuard guard(master.getLock());
		master.getFields(columnAlterSerial, true, fields, NULL);
		columnIds = master.getIndexes()[index].getColumnIds();

		// If the index files do not exist (because the index references new fields not present when partitions were generated),
		// trigger ADD index on coalesced partition instead of coalescing index files.
		if (!firstPartition.isTemporary()) {
			for (uint32_t i = 0; i < columnIds.length(); ++i) {
				if (fields[columnIds[i]] == 0) {
					const uint32_t indexAlterSerial = master.getIndexAlterSerial();
					const Partitions& mpartitions = master.getPartitions();
					for (uint32_t j = 0; j < mpartitions.length(); ++j) {
						Partition& p = *mpartitions[j];
						if (!p.isTransient() && !p.isTemporary() && p.getIndexAlterSerial() == indexAlterSerial) {
							p.setIndexAlterSerial(indexAlterSerial + 1);
						}
					}
					Alterations alterations = master.getIndexAlterations();
					alterations.append(Alteration(ALT_ADD_INDEX, indexAlterSerial + 1, index));
					master.setIndexAlterations(alterations);
					master.setIndexAlterSerial(indexAlterSerial + 1);
					master.toDisk();
					return 0;
				}
			}
		}
	}
	DBUG_PRINT("sparrow_coalescing", ("Start coalescing %u partitions for index %u into partition %s.%s.%llu", partitions.length(), index,
		master.getDatabase().c_str(), master.getTable().c_str(), static_cast<ulonglong>(coalescedPartition->getSerial())));
	const uint32_t nbPartitions = partitions.length();
	RowOffsets rowOffsets(nbPartitions);
	uint32_t offset = 0;
	for (uint32_t i = 0; i < nbPartitions; ++i) {
		rowOffsets.append(offset);
		offset += static_cast<uint32_t>(partitions[i]->getDataRecords());
	}
	const SerialRecordWrapper recordWrapper(columnAlterSerial, index, false, fields, &columnIds);
	const uint32_t recordSize = recordWrapper.getSize() - 4;	// TODO adjust row size
	const uint32_t nodeSize = recordSize + 2 * 4; // TODO row size
	CoalescingReaders readers(partitions, index);
	Positions positions(nbPartitions);
	KeyIndirector indirector(nbPartitions);
	KeyValues keyValues;
	keyValues.reshape((nbPartitions + 1) * recordSize);
	BinBuffer binBuffer;
	const bool isAppend = coalescedPartition->getVersion() >= PersistentPartition::appendVersion_;
	BinBuffer* pBinBuffer = isAppend ? 0 : &binBuffer;

	// Move readers to first index position.
	offset = 0;
	for (uint32_t i = 0; i < nbPartitions; ++i, offset += recordSize) {
		if (task != 0 && task->isStopping()) {
			return 0;
		}

		// Use smallest node in tree (guaranteed to be the smallest index value).
		indirector.append(i);
		PartitionReaderGuard readerGuard(readers.get(i, 0));
		PartitionReader& reader = readerGuard.get();
		PartitionReaderGuard stringReaderGuard(readers.get(i, 1));
		PartitionReader& stringReader = stringReaderGuard.get();
		const uint32_t node = reader.getHeader().getMinNode();
		reader.seekTree(node);
		uint32_t start;
		uint32_t end;
		reader >> start >> end;
		ByteBuffer b(keyValues.data() + offset, recordSize);
		recordWrapper.readKeyValue(reader, stringReader, b, pBinBuffer);
		reader.seekRecord(start);
		uint32_t row;
		reader >> row;
		positions.append(Position(i, row, start, start, end, node));
	}

	// Sort positions.
	{
		const CoalescingComparator comparator(readers, recordWrapper, keyValues, pBinBuffer);
		Sort<KeyIndirector, CoalescingComparator>::quickSort(indirector, comparator, 0, nbPartitions);
#ifndef NDEBUG
		if (nbPartitions > 1) {
			for (uint32_t i = 0; i < nbPartitions - 1; ++i) {
				ByteBuffer b1(keyValues.data() + indirector[i] * recordSize, recordSize);
				ByteBuffer b2(keyValues.data() + indirector[i + 1] * recordSize, recordSize);
				PartitionReaderGuard stringReaderGuard1(readers.get(indirector[i], 1));
				PartitionReaderGuard stringReaderGuard2(readers.get(indirector[i + 1], 2));
				const int cmp = recordWrapper.compare(b1, stringReaderGuard1.get(), b2, stringReaderGuard2.get(), pBinBuffer);
				assert(cmp <= 0);
			}
		}
#endif
	}
	DBUG_PRINT("sparrow_coalescing", ("Moved readers to first index position, start sorting and writing"));
	uint64_t indexSize = 0;
	char filename[FN_REFLEN];
	{
		FileWriter writer(coalescedPartition->getFileName(index, filename), FILE_TYPE_INDEX, FILE_MODE_CREATE);
		if (isAppend) {
			IndexFileHeader dummy;
			writer << dummy;
		} else {
			PersistentPartition::writeFileFormat(writer);
		}
	
		// Write records and build tree nodes.
		uint32_t currentRow = 0;	// TODO row size
		uint32_t startRow = 0;
		GrowingByteBuffer treeBuffer;	// Contains tree nodes in list order.
		uint32_t nodes = 0;
		uint8_t* cmpKey = keyValues.data() + nbPartitions * recordSize;
		uint32_t partition = indirector[0];
		const uint8_t* currentKey = keyValues.data() + partition * recordSize;
		memcpy(cmpKey, currentKey, recordSize);
		uint32_t cmpPartition = partition;
		while (true) {
			if (task != 0 && (currentRow % 16384) == 0 && task->isStopping()) {
				return 0;
			}
			Position& position = positions[partition];
			assert(position.getPartition() == partition);
			uint32_t row = rowOffsets[partition] + position.getRow();
			writer << row;	// TODO row size
			const uint32_t indexHint = position.getIndexHint() + 1;
			PartitionReaderGuard readerGuard(readers.get(partition, 0));
			PartitionReader& reader = readerGuard.get();
			if (position.hasIntervalHint() && indexHint <= position.getEndHint()) {
				// Stay on the same index value: positions order is unchanged.
				reader.seekRecord(indexHint);
				reader >> row;	// TODO row size
				position = Position(partition, row, indexHint, position.getStartHint(), position.getEndHint(), position.getTreeHint());
			} else {
				PartitionReaderGuard stringReaderGuard(readers.get(partition, 1));
				PartitionReader& stringReader = stringReaderGuard.get();
				const FileHeaderBase& header = reader.getHeader();
				if (indexHint == header.getRecords()) {
					position = Position();
				} else if (header.isTreeComplete()) {				
					const uint32_t node = header.getNextNode(position.getTreeHint());
					reader.seekTree(node);
					uint32_t start;
					uint32_t end;
					reader >> start >> end;	// TODO row size
					ByteBuffer b(currentKey, recordSize);
					recordWrapper.readKeyValue(reader, stringReader, b, pBinBuffer);
					reader.seekRecord(start);// TODO row size
					reader >> row;	// TODO row size
					position = Position(partition, row, start, start, end, node);
				} else {
					reader.seekRecord(indexHint);
					reader >> row;	// TODO row size
					ByteBuffer b(currentKey, recordSize);
					recordWrapper.readKeyValue(reader, stringReader, b, pBinBuffer);
					position = Position(partition, row, indexHint, INVALID_ROW, INVALID_ROW, INVALID_TREE_NODE);
				}
				if (position.isValid()) {
					CoalescingKeyComparator comparator(readers, recordWrapper, keyValues, indirector, pBinBuffer);
					const uint32_t n = indirector.length() - 1;
					const uint32_t insertionPoint = BinarySearch<CoalescingKeyComparator>::find(comparator, 0, n, SearchFlag::GE);
					uint32_t* data = const_cast<uint32_t*>(indirector.data());
					const uint32_t save = data[0];
					memmove(data, data + 1, sizeof(data[0]) * n);
					if (insertionPoint == UINT_MAX || insertionPoint == n) {
						data[n] = save;
					} else {
						memmove(data + insertionPoint + 1, data + insertionPoint, sizeof(data[0]) * (n - insertionPoint));
						data[insertionPoint] = save;
					}
				} else {
					indirector.removeFirst();
				}
				if (indirector.isEmpty()) {
					treeBuffer << startRow << currentRow << ByteBuffer(cmpKey, recordSize);		// TODO row size	
					++nodes;
					++currentRow;
					break;
				} else {
					partition = indirector[0];
					currentKey = keyValues.data() + partition * recordSize;
					ByteBuffer b1(cmpKey, recordSize);
					ByteBuffer b2(currentKey, recordSize);
					PartitionReaderGuard stringReaderGuard1(readers.get(cmpPartition, 1));
					PartitionReaderGuard stringReaderGuard2(readers.get(partition, 2));
					const int cmp = recordWrapper.compare(b1, stringReaderGuard1.get(), b2, stringReaderGuard2.get(), pBinBuffer);
					assert(cmp <= 0);
					if (cmp != 0) {
						treeBuffer << startRow << currentRow << ByteBuffer(cmpKey, recordSize);		// TODO row size
						memcpy(cmpKey, currentKey, recordSize);
						cmpPartition = partition;
						startRow = currentRow + 1;
						++nodes;
					}
				}
			}
			++currentRow;
		}

		// Write tree nodes in tree order.
		const TreeOrder& treeOrder = TreeOrder::get(nodes);
		for (uint32_t i = 0; i < nodes; ++i) {
			const uint32_t node = treeOrder.getListIndex(i, nodes);
			writer << ByteBuffer(treeBuffer.getData() + node * nodeSize, nodeSize);
		}

		const TimePeriod period = coalescedPartition->getPeriod();
		if (isAppend) {
			// Write header.
			writer.write();
			IndexFileHeader header(index, 4, currentRow, nodeSize, nodes, period.getMin(), period.getMax());
			writer.seek(0, header.size());
#ifndef NDEBUG
			debugHeader = FileHeader(0, static_cast<uint32_t>(treeBuffer.position()), true, nodeSize, 4, currentRow, index, period.getMin(), period.getMax());
#endif
			writer << header;
		} else {
			// Write binary data.
			writer << binBuffer;

			// Padding.
			const FileHeader header(static_cast<uint32_t>(binBuffer.position()), static_cast<uint32_t>(treeBuffer.position()), true, nodeSize,
				4, currentRow, index, period.getMin(), period.getMax());
#ifndef NDEBUG
			debugHeader = header;
#endif
			const uint64_t indexSize = header.getTotalSize();
			const uint64_t target = indexSize - FileHeader::size();
			while (writer.getFileOffset() < target) {
				writer << static_cast<uint8_t>(0);
			}

			// Write header.
			writer << header;
		}
		writer.write();
		indexSize = writer.getFileSize();
		if (!firstPartition.isTemporary()) {
			coalescedPartition->addIndexSize(indexSize);
		}
	}
#ifndef NDEBUG
	const Str duration(Str::fromDuration((my_micro_time() - tstart) / 1000));
	const Str sizeTotal(Str::fromSize(debugHeader.getTotalSize()));
	const Str sizeRecords(Str::fromSize(debugHeader.getRecordSize() * debugHeader.getRecords()));
	const Str sizeTree(Str::fromSize(debugHeader.getTreeSection().getSize()));
	DBUG_PRINT("sparrow_coalescing", ("Written index file %u of coalesced partition %s.%s.%llu (%u partitions, %s total = %s records + %s tree) in %s", 
			index, master.getDatabase().c_str(), master.getTable().c_str(), static_cast<ulonglong>(coalescedPartition->getSerial()), nbPartitions,
			sizeTotal.c_str(), sizeRecords.c_str(), sizeTree.c_str(), duration.c_str()));
#endif
	return indexSize;
}

}
