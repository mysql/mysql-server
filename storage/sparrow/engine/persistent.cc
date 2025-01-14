/*
	Persistent partition.
*/

#include "persistent.h"
#include "transient.h"
#include "context.h"
#include "fileutil.h"
#include "alter.h"
#include "coalescing.h"
#include "../handler/hasparrow.h"

#include "../engine/log.h"


namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PersistentPartition
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Partition version history:
// 0	Initial version, with built-in index_0 for timestamp.
// 1	No more timestamp index.
// 2	Append to data files, coalesce index files, store strings in separate file.
const uint32_t PersistentPartition::currentVersion_ = 2;

const uint32_t PersistentPartition::appendVersion_ = 2;

PersistentPartition::~PersistentPartition() {
	SPARROW_ENTER("PersistentPartition::~PersistentPartition");
	const Master* master = master_.get();
	if (master != 0) {
		DBUG_PRINT("sparrow_purge", ("Destroying persistent partition %s.%s.%llu",
			master->getDatabase().c_str(), master->getTable().c_str(), static_cast<ulonglong>(getSerial())));
		uint32_t nbIndexes;
		{
			ReadGuard guard(master->getLock());
			nbIndexes = master->getIndexes().length();
		}

		try {
			// Remove partition files.
			// Note the files may have been deleted before, so silently ignore errors.
			char name[FN_REFLEN];
			bool deleteIndexFiles = true;
			if (getVersion() >= PersistentPartition::appendVersion_) {
				if (isMain()) {
					FileCache::releaseFile(FileId(getFileName(DATA_FILE, name), FILE_TYPE_DATA, FILE_MODE_READ), true);
					FileCache::releaseFile(FileId(getFileName(STRING_FILE, name), FILE_TYPE_STRING, FILE_MODE_READ), true);
					deleteIndexFiles = false;
				}
			} else {
				FileCache::releaseFile(FileId(getFileName(DATA_FILE, name), FILE_TYPE_DATA, FILE_MODE_READ), true);
			}
			if (deleteIndexFiles) {
				for (uint32_t i = 0; i < nbIndexes; ++i) {
					FileCache::releaseFile(FileId(getFileName(i, name), FILE_TYPE_INDEX, FILE_MODE_READ), true);
				}
			}

			// Try to remove the parent directories (will remove only if it is empty).
			char parentHour[FN_REFLEN];
			rmdir(FileUtil::getParent(name, parentHour));
			char parentDay[FN_REFLEN];
			rmdir(FileUtil::getParent(parentHour, parentDay));
		} catch(const SparrowException& e) {
			e.toLog();
		}
	}
}

// Create a partition reader. Need read or write lock on master file!
PartitionReader* PersistentPartition::createReader(const uint32_t index, const bool isString, const BlockCacheHint& hint) const _THROW_(SparrowException) {
	if (getVersion() >= PersistentPartition::appendVersion_) {
		if (isString) {
			return new PartitionReader(*getMainPartition(), STRING_FILE, hint);
		} else if (index == DATA_FILE && !isMain()) {
			return new PartitionReader(*getMainPartition(), index, hint);
		}
	}
	return new PartitionReader(*this, getFileId(index, isString), hint);
}

FileHeaderBase* PersistentPartition::readHeader(const uint32_t fileId, FileReader& reader) const _THROW_(SparrowException) {
	FileHeaderBase*		header = 0;
	bool	ok = true;
	try {
		header = readHeader2(fileId, reader);
	} catch(const SparrowException& e) {
		if (fileId == DATA_FILE || fileId == STRING_FILE)
			throw;	// Cannot repair
		if (!sparrow_auto_partition_repair)
			throw;	// Automatic repair is disabled
		spw_print_warning("%s", e.getText());
		ok = false;
	}

	if (!ok) {
		char name[FN_REFLEN];
		reader.getFileName(name);
		spw_print_information("Rebuilding %s...", name);
		reader.close(true);
		try {
			const_cast<PersistentPartition*>(this)->rebuildIndex(fileId);
		} catch(const SparrowException& e) {
			MasterRepairTask*	repairTask = new MasterRepairTask(master_.get());
			Scheduler::addTask(repairTask, Scheduler::now(), true);
			throw SparrowException::create(false, "Failed to rebuild %s: %s", name, e.getText());
		}
		reader.open();
		spw_print_information("Rebuilt %s successfully.", name);
		reader.seek(0);
		header = readHeader2(fileId, reader);
	}
	return header;
}

FileHeaderBase* PersistentPartition::readHeader2(const uint32_t fileId, FileReader& reader) const _THROW_(SparrowException) {
	try {
		FileHeaderBase* header = 0;
		if (getVersion() >= PersistentPartition::appendVersion_) {
			if (fileId == DATA_FILE) {
				DataFileHeader* h = new DataFileHeader();
				reader >> *h;
				header = h;
			} else if (fileId == STRING_FILE) {
				StringFileHeader* h = new StringFileHeader();
				reader >> *h;
				header = h;
			} else {
				IndexFileHeader* h = new IndexFileHeader();
				reader >> *h;
				header = h;
			}
		} else {
			uint8_t dummy;
			uint8_t format;
			reader >> dummy >> format >> dummy >> dummy;
			reader.setVersion(format);
			if (format > 1) {
				reader.seek(reader.getFileSize() - FileHeader::size(format));
			}
			FileHeader* h = new FileHeader();
			reader >> *h;
			if (fileId != DATA_FILE) {
				if (format < 3) {
					h->initialize(getMaster().getTreeNodeSize(fileId));
				} else {
					h->initialize();
				}
			}
			header = h;
		}
		assert(header != 0);
		return header;
	} catch(const SparrowException& e) {
		char name[FN_REFLEN];
		throw SparrowException::create(false, "Cannot read header of file %s: %s", reader.getFileName(name), e.getText());
	}
}

template class BinarySearch<ComparatorPersistent>;

// Search tree for a given key. If necessary, use binary search on index records to refine the position.
Position PersistentPartition::searchTree(Context& context, const uint32_t partition, PartitionReader& reader,
	PartitionReader& stringReader, const KeyValue& key, const SearchFlag searchFlag, const bool refine) const {
	SPARROW_ENTER("PersistentPartition::searchTree");

	// Note the tree cannot be empty.
	QueryInfo& queryInfo = context.getQueryInfo();
	const TableFields& fields = context.getShare().getMappedFields();
	const uint32_t index = queryInfo.getIndex();
	TABLE& table = context.getTable();
	const bool stopOnFirstMatch = queryInfo.stopOnFirstMatch(key);
	const RecordWrapper& treeReader = context.getRecordWrapper(reader.getColumnAlterSerial(), index, true);
	uint8_t* curKey = queryInfo.getCurrentKey().getKey();
	uint8_t* minKey = queryInfo.getMinKey().getKey();
	uint8_t* maxKey = queryInfo.getMaxKey().getKey();
	const uint32_t keyLength = queryInfo.getKeyLength();
	uint32_t minNode = UINT_MAX;
	uint32_t minStart = UINT_MAX;
	uint32_t minEnd = UINT_MAX;
	uint32_t maxNode = UINT_MAX;
	uint32_t maxStart = UINT_MAX;
	uint32_t maxEnd = UINT_MAX;
	bool exactMatch = false;
	uint32_t node = 0;
	const FileHeaderBase& header = reader.getHeader();
	const uint32_t nodes = header.getNodes();
	const bool treeComplete = header.isTreeComplete();
	while (node < nodes) {
		reader.seekTree(node);
		uint32_t start;	// TODO row size
		uint32_t end;
		reader >> start >> end;
		treeReader.readUsingTableBitmap(table, reader, stringReader, true, curKey, true);
		const int cmp = queryInfo.compareKeys(fields, queryInfo.getCurrentKey(), key);
		if (cmp == 0) {					// Node is equal to key.
			exactMatch = true;
			if (searchFlag == SearchFlag::EQ || searchFlag == SearchFlag::GE
				|| searchFlag == SearchFlag::LE || searchFlag == SearchFlag::LE_LAST) {
				if (stopOnFirstMatch) {
					minNode = node;
					minStart = start;
					minEnd = end;
					maxNode = node;
					maxStart = start;
					maxEnd = end;
					break;
				} else if (treeComplete) {
					memcpy(minKey, curKey, keyLength);
					minNode = node;
					minStart = start;
					minEnd = end;					
					memcpy(maxKey, curKey, keyLength);
					maxNode = node;
					maxStart = start;
					maxEnd = end;
					if (searchFlag == SearchFlag::LE_LAST) {
						// Goto right (greater) child.
						node = node * 2 + 2;
					} else {
						// Goto left (smaller) child.
						node = node * 2 + 1;
					}
				} else {
					if (searchFlag == SearchFlag::LE_LAST) {
						memcpy(minKey, curKey, keyLength);
						minNode = node;
						minStart = start;
						minEnd = end;

						// Goto right (greater) child.
						node = node * 2 + 2;
					} else {
						memcpy(maxKey, curKey, keyLength);
						maxNode = node;
						maxStart = start;
						maxEnd = end;

						// Goto left (smaller) child.
						node = node * 2 + 1;
					}
				}
			} else if (searchFlag == SearchFlag::GT) {
				memcpy(minKey, curKey, keyLength);
				minNode = node;
				minStart = start;
				minEnd = end;

				// Goto right (greater) child.
				node = node * 2 + 2;
			} else if (searchFlag == SearchFlag::LT) {
				memcpy(maxKey, curKey, keyLength);
				maxNode = node;
				maxStart = start;
				maxEnd = end;

				// Goto left (smaller) child.
				node = node * 2 + 1;
			}
		} else if (cmp > 0) {			// Node is greater than key.
			// Check if node is smaller than maxKey.
			if (maxStart == UINT_MAX || queryInfo.compareKeys(fields, queryInfo.getCurrentKey(), queryInfo.getMaxKey()) <= 0) {
				memcpy(maxKey, curKey, keyLength);
				maxNode = node;
				maxStart = start;
				maxEnd = end;
			}
			// Goto left (smaller) child.
			node = node * 2 + 1;
		} else if (cmp < 0) {			// Node is smaller than key.
			// Check if node is larger than minKey.
			if (minStart == UINT_MAX || queryInfo.compareKeys(fields, queryInfo.getCurrentKey(), queryInfo.getMinKey()) >= 0) {
				memcpy(minKey, curKey, keyLength);
				minNode = node;
				minStart = start;
				minEnd = end;
			}
			// Goto right (greater) child.
			node = node * 2 + 2;
		}
	}
	uint32_t dataRow = 0;
	uint32_t indexRow = UINT_MAX;
	uint32_t indexStart = UINT_MAX;
	uint32_t indexEnd = UINT_MAX;
	uint32_t treeNode = UINT_MAX;
	if (exactMatch && stopOnFirstMatch) {
		// Exact match found.
		if (searchFlag == SearchFlag::EQ || searchFlag == SearchFlag::GE
			|| searchFlag == SearchFlag::LE || searchFlag == SearchFlag::LE_LAST) {
			indexRow = searchFlag == SearchFlag::LE_LAST ? minEnd : minStart;
			indexStart = minStart;
			indexEnd = minEnd;
			treeNode = minNode;
		} else if (searchFlag == SearchFlag::GT) {
			if (minEnd + 1 == header.getRecords()) {
				return Position(partition);
			} else {
				indexRow = minEnd + 1;
				indexStart = indexRow;
				if (indexRow == maxStart) {
					// Contiguous intervals: can set index end and tree node.
					indexEnd = maxEnd;
					treeNode = maxNode;
				}
			}
		} else if (searchFlag == SearchFlag::LT) {
			if (maxStart == 0) {
				return Position(partition);
			} else {
				indexRow = maxStart - 1;
				indexEnd = indexRow;
				if (indexRow == minEnd) {
					// Contiguous intervals: can set index start and tree node.
					indexStart = minStart;
					treeNode = minNode;
				}
			}
		}
	} else if (treeComplete) {
		// Tree is complete.
		if (exactMatch) {
			// Exact match.
			if (searchFlag == SearchFlag::EQ || searchFlag == SearchFlag::GE || searchFlag == SearchFlag::GT) {
				if (maxStart == UINT_MAX) {
					return Position(partition);
				} else {
					indexRow = maxStart;
					indexStart = maxStart;
					indexEnd = maxEnd;
					treeNode = maxNode;
				}
			} else if (searchFlag == SearchFlag::LE || searchFlag == SearchFlag::LE_LAST
				|| searchFlag == SearchFlag::LT) {
				if (minEnd == UINT_MAX) {
					return Position(partition);
				} else {
					indexRow = minEnd;
					indexStart = minStart;
					indexEnd = minEnd;
					treeNode = minNode;
				}
			}
		} else {
			// No exact match.
			if (searchFlag == SearchFlag::EQ) {
				return Position(partition);
			} else if (searchFlag == SearchFlag::GE || searchFlag == SearchFlag::GT) {
				if (maxStart == UINT_MAX) {
					return Position(partition);
				} else {
					indexRow = maxStart;
					indexStart = maxStart;
					indexEnd = maxEnd;
					treeNode = maxNode;
				}
			} else if (searchFlag == SearchFlag::LE || searchFlag == SearchFlag::LE_LAST
				|| searchFlag == SearchFlag::LT) {
				if (minEnd == UINT_MAX) {
					return Position(partition);
				} else {
					indexRow = minEnd;
					indexStart = minStart;
					indexEnd = minEnd;
					treeNode = minNode;
				}
			}
		}
	} else {
		// Tree is not complete.
		// Use binary search on index records to refine position.
		const RecordWrapper& recordWrapper = context.getRecordWrapper(reader.getColumnAlterSerial(), index, false);
		ComparatorPersistent comparator(context, recordWrapper, reader, stringReader, key, curKey);
		const uint32_t bsStart = (minEnd == UINT_MAX) ? 0 : minEnd;
		const uint32_t bsEnd = (maxStart == UINT_MAX) ? static_cast<uint32_t>(header.getRecords()) - 1 : maxStart;
		indexRow = BinarySearch<ComparatorPersistent>::find(comparator, bsStart, bsEnd - bsStart + 1, searchFlag);
		if (indexRow == UINT_MAX) {
			return Position(partition);
		}
	}
	if (refine) {
		reader.seekRecord(indexRow);
		reader >> dataRow;
	}
	return Position(partition, dataRow, indexRow, indexStart, indexEnd, treeNode);
}

Position PersistentPartition::indexFind(Context& context, const uint32_t partition, const KeyValue& key, const SearchFlag searchFlag) const {
	SPARROW_ENTER("PersistentPartition::indexFind");
	try {
		QueryInfo& queryInfo = context.getQueryInfo();
		const uint32_t index = queryInfo.getIndex();
		PartitionReaderGuard readerGuard(context, partition, index, false, BlockCacheHint::largeForward1_);
		PartitionReaderGuard stringReaderGuard(context, partition, index, true, BlockCacheHint::mediumAround2_);
		return searchTree(context, partition, readerGuard.get(), stringReaderGuard.get(), key, searchFlag, true);
	} catch(const SparrowException& e) {
		e.toLog();
		return Position(partition);
	}
}

Position PersistentPartition::indexFirst(Context& context, const uint32_t partition) const {
	SPARROW_ENTER("PersistentPartition::indexFirst");
	try {
		// Use smallest node in tree (guaranteed to be the smallest index value).
		const QueryInfo& queryInfo = context.getQueryInfo();
		const uint32_t index = queryInfo.getIndex();
		uint32_t node;
		uint32_t start;
		uint32_t end;
		{
			PartitionReaderGuard readerGuard(context, partition, index, false, BlockCacheHint::largeForward1_);
			PartitionReader& reader = readerGuard.get();
			node = reader.getHeader().getMinNode();
			reader.seekTree(node);
			reader >> start >> end;
		}
		PartitionReaderGuard readerGuard(context, partition, index, false, BlockCacheHint::largeForward1_);
		PartitionReader& reader = readerGuard.get();
		reader.seekRecord(start);
		uint32_t row;
		reader >> row;
		return Position(partition, row, start, start, end, node);
	} catch(const SparrowException& e) {
		e.toLog();
		return Position(partition);
	}
}

Position PersistentPartition::indexLast(Context& context, const uint32_t partition) const {
	SPARROW_ENTER("PersistentPartition::indexLast");
	try {
		// Use largest node in tree (guaranteed to be the largest index value).
		const QueryInfo& queryInfo = context.getQueryInfo();
		const uint32_t index = queryInfo.getIndex();
		uint32_t node;
		uint32_t start;
		uint32_t end;
		{
			PartitionReaderGuard readerGuard(context, partition, index, false, BlockCacheHint::largeForward1_);
			PartitionReader& reader = readerGuard.get();
			node = reader.getHeader().getMaxNode();
			reader.seekTree(node);
			reader >> start >> end;
		}
		PartitionReaderGuard readerGuard(context, partition, index, false, BlockCacheHint::largeBackward1_);
		PartitionReader& reader = readerGuard.get();
		reader.seekRecord(end);
		uint32_t row;
		reader >> row;
		return Position(partition, row, end, start, end, node);
	} catch(const SparrowException& e) {
		e.toLog();
		return Position(partition);
	}
}

Position PersistentPartition::indexNext(Context& context, const Position& position) const {
	SPARROW_ENTER("PersistentPartition::indexNext");
	const uint32_t partition = position.getPartition();
	if (!position.isValid() || !position.hasIndexHint()) {
		return Position(partition);
	}
	try {
		const QueryInfo& queryInfo = context.getQueryInfo();
		const uint32_t index = queryInfo.getIndex();
		uint32_t indexRow = position.getIndexHint();
		uint32_t start = position.getStartHint();
		uint32_t end = position.getEndHint();
		uint32_t node = position.getTreeHint();
		const uint32_t records = getRecords();
		if (indexRow + 1 < records) {
			indexRow++;
			if (node != UINT_MAX && indexRow > end) {
				// The interval hint is no longer valid and the tree is complete; find next node in tree.
				PartitionReaderGuard readerGuard(context, partition, index, false, BlockCacheHint::largeAround1_);
				PartitionReader& reader = readerGuard.get();
				const FileHeaderBase& header = reader.getHeader();
				if (header.isTreeComplete()) {
					node = header.getNextNode(node);
					reader.seekTree(node);
					reader >> start >> end;
				} else {
					start = UINT_MAX;
					end = UINT_MAX;
					node = UINT_MAX;
				}
			}
			PartitionReaderGuard readerGuard(context, partition, index, false, BlockCacheHint::largeForward1_);
			PartitionReader& reader = readerGuard.get();
			reader.seekRecord(indexRow);
			uint32_t row;
			reader >> row;
			return Position(partition, row, indexRow, start, end, node);
		} else {
			return Position(partition);
		}
	} catch(const SparrowException& e) {
		e.toLog();
		return Position(partition);
	}
}

Position PersistentPartition::indexPrevious(Context& context, const Position& position) const {
	SPARROW_ENTER("PersistentPartition::indexPrevious");
	const uint32_t partition = position.getPartition();
	if (!position.isValid() || !position.hasIndexHint()) {
		return Position(partition);
	}
	try {
		const QueryInfo& queryInfo = context.getQueryInfo();
		const uint32_t index = queryInfo.getIndex();
		uint32_t indexRow = position.getIndexHint();
		uint32_t start = position.getStartHint();
		uint32_t end = position.getEndHint();
		uint32_t node = position.getTreeHint();
		if (indexRow > 0) {
			indexRow--;
			if (node != UINT_MAX && indexRow < start) {
				// The interval hint is no longer valid and the tree is complete; find previous node in tree.
				PartitionReaderGuard readerGuard(context, partition, index, false, BlockCacheHint::largeAround1_);
				PartitionReader& reader = readerGuard.get();
				const FileHeaderBase& header = reader.getHeader();
				if (header.isTreeComplete()) {
					node = header.getPrevNode(node);
					reader.seekTree(node);
					reader >> start >> end;
				} else {
					start = UINT_MAX;
					end = UINT_MAX;
					node = UINT_MAX;
				}
			}
			PartitionReaderGuard readerGuard(context, partition, index, false, BlockCacheHint::largeBackward1_);
			PartitionReader& reader = readerGuard.get();
			reader.seekRecord(indexRow);
			uint32_t row;
			reader >> row;
			return Position(partition, row, indexRow, start, end, node);
		} else {
			return Position(partition);
		}
	} catch(const SparrowException& e) {
		e.toLog();
		return Position(partition);
	}
}

Position PersistentPartition::moveNext(Context& context, const Position& position) const {
	SPARROW_ENTER("PersistentPartition::moveNext");
	const uint32_t partition = position.getPartition();
	const RowNumber row = position.getRow() + 1;
	if (row < static_cast<RowNumber>(getRecordOffset() + getRecords())) {
		return Position(partition, row);
	} else {
		return Position(partition);
	}
}

Position PersistentPartition::movePrevious(Context& context, const Position& position) const {
	SPARROW_ENTER("PersistentPartition::movePrevious");
	const uint32_t partition = position.getPartition();
	const RowNumber row = position.getRow();
	if (row == static_cast<RowNumber>(getRecordOffset())) {
		return Position(partition);
	} else {
		return Position(partition, row - 1);
	}
}

Position PersistentPartition::moveAbsolute(Context& context, const Position& position) const {
	SPARROW_ENTER("PersistentPartition::moveAbsolute");
	return position;
}

Position PersistentPartition::moveFirst(Context& context, const uint32_t partition) const {
	SPARROW_ENTER("PersistentPartition::moveFirst");
	return Position(partition, static_cast<RowNumber>(getRecordOffset()));
}

Position PersistentPartition::moveLast(Context& context, const uint32_t partition) const {
	SPARROW_ENTER("PersistentPartition::moveLast");
	PartitionReaderGuard readerGuard(context, partition, DATA_FILE, false, BlockCacheHint::largeBackward2_);
	PartitionReader& reader = readerGuard.get();
	const FileHeaderBase& header = reader.getHeader();
	return Position(partition, static_cast<RowNumber>(getRecordOffset() + header.getRecords() - 1));
}

// Use tree to count records in range. The result may be overestimated.
uint32_t PersistentPartition::recordsInRange(Context& context, const uint32_t partition, const key_range* minKey, const key_range* maxKey) const {
	SPARROW_ENTER("PersistentPartition::recordsInRange");
	try {
		QueryInfo& queryInfo = context.getQueryInfo();
		const uint32_t index = queryInfo.getIndex();
		uint32_t minRow = UINT_MAX;
		uint32_t maxRow = UINT_MAX;
		if (minKey == 0) {
			minRow = 0;
		} else {
			const KeyValue minKeyValue(minKey);
			PartitionReaderGuard readerGuard(context, partition, index, false, BlockCacheHint::largeForward1_);
			PartitionReaderGuard stringReaderGuard(context, partition, index, true, BlockCacheHint::mediumAround2_);

			// Min key flag is either HA_READ_AFTER_KEY or HA_READ_KEY_EXACT.
			const Position pos = searchTree(context, partition, readerGuard.get(), stringReaderGuard.get(), minKeyValue,
				minKey->flag == HA_READ_KEY_EXACT ? SearchFlag::GE : SearchFlag::GT, false);
			minRow = pos.getIndexHint();
		}
		if (maxKey == 0) {
			maxRow = getRecords() - 1;
		} else {
			const KeyValue maxKeyValue(maxKey);
			PartitionReaderGuard readerGuard(context, partition, index, false, BlockCacheHint::largeForward1_);
			PartitionReaderGuard stringReaderGuard(context, partition, index, true, BlockCacheHint::mediumAround2_);

			// Max key flag is either HA_READ_BEFORE_KEY or HA_READ_AFTER_KEY.
			const Position pos = searchTree(context, partition, readerGuard.get(), stringReaderGuard.get(), maxKeyValue,
				maxKey->flag == HA_READ_AFTER_KEY ? SearchFlag::LE_LAST : SearchFlag::LT, false);
			maxRow = pos.getIndexHint();
		}
		if (minRow != UINT_MAX && maxRow != UINT_MAX && minRow <= maxRow) {
			return maxRow - minRow + 1;
		} else {
			return 0;
		}
	} catch(const SparrowException& e) {
		e.toLog();
		return 0;
	}
}

// Reads a given index record and sets MySQL fields.
bool PersistentPartition::readKey(Context& context, const Position& position, const bool forward,
	const key_part_map keyPartMap, uint8_t* buffer, const bool keyFormat) const {
	SPARROW_ENTER("PersistentPartition::readKey");
	if (!position.hasIndexHint()) {
		return false;
	}
	const QueryInfo& queryInfo = context.getQueryInfo();
	const uint32_t index = queryInfo.getIndex();
	const bool useTree = position.hasTreeHint();
	const BlockCacheHint& hint = useTree ? BlockCacheHint::largeForward1_ : (forward ? BlockCacheHint::largeForward1_ : BlockCacheHint::largeBackward1_);
	PartitionReaderGuard readerGuard(context, position.getPartition(), index, false, hint);
	PartitionReaderGuard stringReaderGuard(context, position.getPartition(), index, true, BlockCacheHint::mediumAround2_);
	PartitionReader& reader = readerGuard.get();
	const RecordWrapper& recordWrapper = context.getRecordWrapper(getColumnAlterSerial(), index, useTree);
	if (useTree) {
		reader.seekTreeData(position.getTreeHint());
	} else {
		reader.seekRecordData(position.getIndexHint());
	}
	recordWrapper.readUsingKeyPartMap(reader, stringReaderGuard.get(), keyPartMap, buffer, keyFormat);
	return true;
}

// Reads a given data record and sets MySQL fields.
bool PersistentPartition::readData(Context& context, const Position& position, uint8_t* buffer, const BlockCacheHint& hint) const {
	SPARROW_ENTER("PersistentPartition::readData");
	assert(position.isValid());
	QueryInfo& queryInfo = context.getQueryInfo();

	// Reset NULL flags.
	TABLE& table = context.getTable();
	memset(buffer, 0, table.s->null_bytes);
	if (queryInfo.isCoveringIndex() && position.hasIndexHint()) {
		return readKey(context, position, true, queryInfo.getDataMap(), buffer, false);
	} else {
		PartitionReaderGuard readerGuard(context, position.getPartition(), DATA_FILE, false, hint);
		PartitionReader& reader = readerGuard.get();
		reader.seekRecord(position.getRow());
		const RecordWrapper* recordWrapper = NULL;
		if (skippedColumnIds_.isEmpty()) {
			recordWrapper = &context.getRecordWrapper(getColumnAlterSerial(), DATA_FILE, false);
		} else {
			recordWrapper = &context.getRecordWrapper(getColumnAlterSerial(), mainPartition_->getSerial(), getSkippedColumns());
		}
		//const RecordWrapper& recordWrapper = context.getRecordWrapper(getColumnAlterSerial(), DATA_FILE, false);
		PartitionReaderGuard stringReaderGuard(context, position.getPartition(), DATA_FILE, true, BlockCacheHint::mediumAround2_);
		recordWrapper->readUsingTableBitmap(table, reader, stringReaderGuard.get(), false, buffer, false);
		return true;
	}
}


bool PersistentPartition::updateData(Context& context, const Position& position, const uint8_t* buffer) {
	SPARROW_ENTER("PersistentPartition::updateData");
	assert(position.isValid());

	// Read existing record into memory.
	PartitionReaderGuard readerGuard(context, position.getPartition(), DATA_FILE, false, BlockCacheHint::smallAround2_);
	PartitionReader& reader = readerGuard.get();
	const uint64_t offset = reader.seekRecord(position.getRow());
	const RecordWrapper& recordWrapper = context.getRecordWrapper(getColumnAlterSerial(), DATA_FILE, false);
	const uint32_t recordSize = recordWrapper.getSize();
	ByteBuffer data(static_cast<uint8_t*>(IOContext::getTempBuffer1(recordSize)), recordSize);
	reader >> data;
	data.position(recordWrapper.getBitSize());
	data.limit(recordSize);
	uint8_t* newRecord = data.getData();

	// Update record.
	const TableFields& fields = recordWrapper.getFields();
	const uint32_t n = fields.length();
	uint32_t col = 0;
	uint32_t bitOffset = 0;
	for (uint32_t i = 0; i < n; ++i) {
		const FieldBase& field = *fields[i];
		if (!field.isMapped()) {
			bitOffset += field.getBits();
			continue;
		}
		if (!context.isUpdatableColumn(col++)) {
			data.advance(field.getSize());
			bitOffset += field.getBits();
			continue;
		}
		if (field.readMySqlPersistent(buffer, data)) {
			newRecord[bitOffset / 8] |= (1 << (bitOffset % 8));
		} else if (field.isNullable()) {
			newRecord[bitOffset / 8] &= ~(1 << (bitOffset % 8));
		}
		bitOffset += field.getBits();
	}

	// Write modified record to data file.
	char filename[FN_REFLEN];
	mainPartition_->getFileName(DATA_FILE, filename);
	const PartitionFile partitionFile(*mainPartition_, DATA_FILE);
	const SimpleWriteCacheHint writeHint(partitionFile, 3);
	FileWriter writer(filename, FILE_TYPE_DATA, FILE_MODE_UPDATE, &writeHint, offset, recordSize);
	data.position(0);
	data.limit(recordSize);
	writer << data;
	writer.write();
	return true;
}


// Alter this persistent partition so it fits the current table/index definition.
// If task is null, alterations are performed synchronously.
// Otherwise, alteration tasks are sent asynchronously to the alteration queue
void PersistentPartition::alter(const Task* task) _THROW_(SparrowException) {
	SPARROW_ENTER("PersistentPartition::alter");
	Alterations alterations;
	uint32_t newSerial = 0;
	{
		ReadGuard guard(master_->getLock());
		newSerial = master_->getIndexAlterSerial();
		const Alterations& masterAlterations = master_->getIndexAlterations();
		for (uint32_t i = 0; i < masterAlterations.length(); ++i) {
			const Alteration& masterAlteration = masterAlterations[i];
			if (masterAlteration.getSerial() > getIndexAlterSerial()) {
				alterations.append(masterAlteration);
			}
		}
	}

	// Optimize alterations: create + drop = NOP (but keep drop + create).
	for (uint32_t i = 0; i < alterations.length(); ++i) {
		const Alteration& alteration = alterations[i];
		if (alteration.getType() == ALT_ADD_INDEX) {
			const uint32_t id = alteration.getId();
			for (uint32_t j = i + 1; j < alterations.length(); ++j) {
				const Alteration& nextAlteration = alterations[j];
				if (nextAlteration.getType() == ALT_DROP_INDEX && nextAlteration.getId() == id) {
					alterations.removeAt(j);
					alterations.removeAt(i--);	// Wrapping.
					break;
				}
			}
		}
	}
#ifndef NDEBUG
	if (!alterations.isEmpty()) {
		DBUG_PRINT("sparrow_alter", ("Altering partition %s.%s.%llu", master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial())));
	}
#endif
	if (task == 0) {
		DBUG_PRINT("sparrow_alter", ("synchronous alteration"));
		AlterationStats stats;
		for (uint32_t i = 0; i < alterations.length(); ++i) {
			const Alteration& alteration = alterations[i];
			const AlterationType type = alteration.getType();
			try {
				if (type == ALT_ADD_INDEX) {
					stats += createIndex(alteration.getId(), task);
				} else if (type == ALT_DROP_INDEX) {
					stats += dropIndex(alteration.getId());
				}
			} catch(const SparrowException& e) {
				e.toLog();
			}
		}
		setIndexAlterSerial(newSerial);
		dataSize_ += stats.getDeltaDataSize();
		indexSize_ += stats.getDeltaIndexSize();

		WriteGuard guard(master_->getLock());
		master_->resetCoalescingTimestamp();
	} else {
		uint32_t* counter = new uint32_t;
		*counter = alterations.length();
		AlterationStats* stats = new AlterationStats();
		for (uint32_t i = 0; i < alterations.length(); ++i) {
			[[maybe_unused]] const Alteration& alteration = alterations[i];
			DBUG_PRINT("sparrow_alter", ("addTask for partition %s.%s.%llu, type %d, alter Id %d, alter serial %d, counter %u", 
				master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()),
				alteration.getType(), alteration.getId(), alteration.getSerial(), *counter));
			Scheduler::addTask(new AlterTask(counter, stats, newSerial, this, alterations[i]));
		}
	}
}

// This method returns the next persistent partition to alter (older), or 0 if none.
PersistentPartitionGuard PersistentPartition::alterationDone(const AlterationStats& stats, const uint32_t newIndexAlterSerial) {
	SPARROW_ENTER("PersistentPartition::alterationDone");
	DBUG_PRINT("sparrow_alter", ("Partition %llu, setting IndexAlterSerial from %u to %u", static_cast<ulonglong>(getSerial()), indexAlterSerial_, newIndexAlterSerial));

	setIndexAlterSerial(newIndexAlterSerial);
	dataSize_ += stats.getDeltaDataSize();
	indexSize_ += stats.getDeltaIndexSize();
	{
		// Update master file.
		WriteGuard guard(master_->getLock());
		master_->setDataSize(master_->getDataSize() + stats.getDeltaDataSize());
		master_->setIndexSize(master_->getIndexSize() + stats.getDeltaIndexSize());
		master_->indexAlterationDone();
		master_->resetCoalescingTimestamp();
		master_->toDisk();
	}
	DBUG_PRINT("sparrow_alter", ("Updated Master file. Find next partition (older)"));

	// Find next partition (older).
	{
		ReadGuard guard(master_->getLock());
		const Partitions& partitions = master_->getPartitions();
		uint32_t i;
		partitions.bsearch(*this, i, 2);
		for (;;) {
			if (i == partitions.length()) {
				if (i == 0) {
					break;
				}
				i--;
			}
			Partition* partition = partitions[i];
			if (partition->getSerial() < getSerial() && partition->isIndexAlterable() && !partition->isReady() && !partition->isTemporary()) {
				DBUG_PRINT("sparrow_alter", ("Next older partition: %llu", static_cast<ulonglong>(partition->getSerial())));
				return PersistentPartitionGuard(static_cast<PersistentPartition*>(partition));
			}
			if (i-- == 0) {
				break;
			}
		}
	}

	// Nothing found, but check if alter needs to be restarted, in case alterations were stacked meanwhile.
	DBUG_PRINT("sparrow_alter", ("Nothing found for %s.%s.%llu. Check if alter need to be restarted.", 
		master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial())));
	if (!master_->startIndexAlter(true)) {
		// No more alteration: start coalescing, if necessary.
		DBUG_PRINT("sparrow_alter", ("No more alteration: start coalescing, if necessary"));
		master_->coalesce();
	}
	return PersistentPartitionGuard();
}

template class Sort<Indirector, AlterComparator>;

AlterationStats PersistentPartition::createIndex(const uint32_t index, const Task* task) _THROW_(SparrowException) {
	SPARROW_ENTER("PersistentPartition::createIndex");
#ifndef NDEBUG
	uint64_t tstart = my_micro_time();
	Str descr;
#endif
	DBUG_PRINT("sparrow_alter", ("Creating index %u", index));
	TableFieldsGuard fieldsGuard;
	TableFields& fields = fieldsGuard.get();
	ColumnIds columnIds;
	{
		ReadGuard guard(master_->getLock());
		master_->getFields(getColumnAlterSerial(), false, fields, &getSkippedColumns());
		columnIds = master_->getIndexes()[index].getColumnIds();
#ifndef NDEBUG
		descr = master_->getIndexes()[index].getName();
		descr += Str("(");
		for (uint32_t i = 0; i < columnIds.length(); ++i) {
			if (i > 0) {
				descr += Str(", ");
			}
			descr += master_->getColumns()[columnIds[i]].getName();
		}
		descr += Str(")");
#endif
	}
	DBUG_PRINT("sparrow_alter", ("Adding index %s on partition %s.%s.%llu", descr.c_str(),
		master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial())));
	UpdateGuard updateGuard(*master_);
	uint64_t totalSize = 0;
	const uint64_t threshold = sparrow_cache0_size / 5;	// 20% of cache level 0.
	const uint64_t dataSize = getMainPartition()->getDataSize();
	DBUG_PRINT("sparrow_alter", ("Data size %llu > %llu ?", static_cast<ulonglong>(dataSize), static_cast<ulonglong>(threshold)));
	if (dataSize > threshold) {
		// If data file is too large, create multiple index files and coalesce them.
		// To do so, create temporary persistent partitions and coalesce them.
		const uint32_t rows = 1 + static_cast<uint32_t>((getRecords() * threshold) / dataSize);
		PersistentPartitions partitions(1 + getRecords() / rows);
		for (uint32_t offset = 0; offset < getRecords(); offset += rows) {
			const uint32_t records = std::min(rows, getRecords() - offset);
			const uint64_t recordOffset = getRecordOffset() + offset;
			DBUG_PRINT("sparrow_alter", ("Creating new temp persistent partition for %u records at offset %llu", records, static_cast<ulonglong>(recordOffset)));
			PersistentPartition* temporary = master_->newTemporaryPersistentPartition(*this, records, recordOffset);
			DBUG_PRINT("sparrow_alter", ("Creating corresponding index file"));
			temporary->createIndexFile(index, task, fields, columnIds, static_cast<uint32_t>(recordOffset), records);
			DBUG_PRINT("sparrow_alter", ("Appending new partition"));
			partitions.append(PersistentPartitionGuard(temporary));
		}
		DBUG_PRINT("sparrow_alter", ("Generating index file"));
		totalSize = Coalescing::generateIndexFile(partitions, index, this, task);
		DBUG_PRINT("sparrow_alter", ("calling coalescingDone"));
		master_->coalescingDone(this, partitions);
	} else {
		// Data file is not too large, create index file in one pass.
		DBUG_PRINT("sparrow_alter", ("calling createIndexFile"));
		totalSize = createIndexFile(index, task, fields, columnIds, static_cast<uint32_t>(getRecordOffset()), getRecords());
	}
#ifndef NDEBUG
	const Str duration(Str::fromDuration((my_micro_time() - tstart) / 1000));
	DBUG_PRINT("sparrow_alter", ("Created index %u for partition %s.%s.%llu in %s", index,
		master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), duration.c_str()));
#endif
	return AlterationStats(0, static_cast<int64_t>(totalSize));
}

void PersistentPartition::rebuildIndex(const uint32_t index) _THROW_(SparrowException) {
	// Remove the corrupted index file, then rebuild the file.
	char filename[FN_REFLEN];
	getFileName(index, filename);
	FileId		fileId(filename, FILE_TYPE_INDEX, FILE_MODE_READ);
	FileCache::releaseFile(fileId, true);

	createIndex(index, NULL);
}

uint64_t PersistentPartition::createIndexFile(const uint32_t index, const Task* task,
	const TableFields& fields, const ColumnIds& columnIds, const uint32_t startRow, const uint32_t rows) _THROW_(SparrowException) {
	Indirector indirector;
	SYSxvector<uint32_t> count;
	{
		PartitionReaderGuard guard1(*this, DATA_FILE, false, BlockCacheHint::largeAround0_);
		PartitionReader& reader1 = guard1.get();
		PartitionReaderGuard stringGuard1(*this, DATA_FILE, true, BlockCacheHint::largeAround0_);
		PartitionReader& stringReader1 = stringGuard1.get();
		PartitionReaderGuard guard2(*this, DATA_FILE, false, BlockCacheHint::largeAround0_);
		PartitionReader& reader2 = guard2.get();
		PartitionReaderGuard stringGuard2(*this, DATA_FILE, true, BlockCacheHint::largeAround0_);
		PartitionReader& stringReader2 = stringGuard2.get();
		const AlterComparator comparator(task, fields, columnIds, getSkippedColumns(), reader1, stringReader1, reader2, stringReader2);
		for (uint32_t i = startRow; i < startRow + rows; ++i) {
			indirector.append(i);
		}
		DBUG_PRINT("sparrow_alter", ("Quick sort of %u rows", rows));
		Sort<Indirector, AlterComparator>::quickSort(indirector, comparator, 0, rows);

		// Count distinct values.
		DBUG_PRINT("sparrow_alter", ("Counting distinct values"));
		uint32_t start = 0;
		uint32_t previousRow = 0;
		for (uint32_t row = 0; row < rows; ++row) {
			if (task != 0 && (row % 16384) == 0 && task->isStopping()) {
				return 0;
			}
			const uint32_t currentRow = indirector[row];
			if (row == 0) {
				start = row;
			} else {
				const int cmp = comparator.compare(previousRow, currentRow, false);
				assert(cmp <= 0);
				if (cmp != 0) {
					count.append(row - start);
					start = row;
				}
			}
			previousRow = currentRow;
		}
		count.append(rows - start);
		DBUG_PRINT("sparrow_alter", ("counted %u", rows - start));
	}

	// Generate index file from indirector.
	PartitionReaderGuard guard(*this, DATA_FILE, false, BlockCacheHint::largeAround0_);
	PartitionReaderGuard stringGuard(*this, DATA_FILE, true, BlockCacheHint::largeAround0_);
	AlterWriter alterWriter(*this, fields, columnIds, getSkippedColumns(), guard.get(), stringGuard.get());
	const uint32_t nNodes = count.length();
	TreeNodes nodes(nNodes);
	uint32_t start = 0;
	for (uint32_t i = 0; i < nNodes; ++i) {
		const uint32_t end = start + count[i];
		nodes.append(TreeNode(start, end - 1));
		start = end;
	}
	assert(nodes.length() == nNodes);
	const uint32_t recordSize = 4;
	const bool isAppend = getVersion() >= PersistentPartition::appendVersion_;
	char filename[FN_REFLEN];
	DBUG_PRINT("sparrow_alter", ("Writing new file"));
	FileWriter writer(getFileName(index, filename), FILE_TYPE_INDEX, FILE_MODE_CREATE);
	if (isAppend) {
		IndexFileHeader dummy;
		writer << dummy;
	} else {
		// Write file format.
		PersistentPartition::writeFileFormat(writer);
	}

	// Write row numbers.
	DBUG_PRINT("sparrow_alter", ("Writing row numbers (%u)", rows));
	for (uint32_t row = 0; row < rows; ++row) {
		if (task != 0 && (row % 16384) == 0 && task->isStopping()) {
			return 0;
		}
		writer << indirector[row];
	}
	uint64_t offset = writer.getFileOffset();

	// Write tree.
	DBUG_PRINT("sparrow_alter", ("Writing tree (%u nodes)", nNodes));
	const TreeOrder& treeOrder = TreeOrder::get(nNodes);
	for (uint32_t i = 0; i < nNodes; ++i) {
		if (task != 0 && (i % 16384) == 0 && task->isStopping()) {
			return 0;
		}
		const uint32_t inode = treeOrder.getListIndex(i, nNodes);
		assert(inode < nNodes);
		const TreeNode& node = nodes[inode];
		const uint32_t start = node.getStart();
		const uint32_t end = node.getEnd();
		writer << start << end;
		alterWriter.writeRecord(writer, indirector[start]);
	}
	const uint64_t treeSize = writer.getFileOffset() - offset;
	uint64_t totalSize;
	if (isAppend) {
		DBUG_PRINT("sparrow_alter", ("Writing header"));
		// Write header.
		writer.write();
		IndexFileHeader header(index, recordSize, rows, static_cast<uint32_t>(treeSize / nNodes), nNodes, period_.getMin(), period_.getMax());
		writer.seek(0, header.size());
		writer << header;
		totalSize = header.getTotalSize();
	} else {
		// Write binary data.
		DBUG_PRINT("sparrow_alter", ("Writing binary data"));
		offset = writer.getFileOffset();
		writer << alterWriter.getBinBuffer();
		const uint64_t binSize = writer.getFileOffset() - offset;

		// Padding to put header at the end of the file, taking into account its adjusted size.
		const FileHeader header(binSize, static_cast<uint32_t>(treeSize), true, static_cast<uint32_t>(treeSize / nNodes),
			recordSize, rows, index, period_.getMin(), period_.getMax());
		const uint64_t target = header.getTotalSize() - FileHeader::size();
		while (writer.getFileOffset() < target) {
			writer << static_cast<uint8_t>(0);
		}

		// Write header.
		DBUG_PRINT("sparrow_alter", ("Write header"));
		writer << header;
		totalSize = header.getTotalSize();
	}
	DBUG_PRINT("sparrow_alter", ("Total size %llu", static_cast<ulonglong>(totalSize)));
	writer.write();
	return totalSize;
}

AlterationStats PersistentPartition::dropIndex(const uint32_t index) {
	SPARROW_ENTER("PersistentPartition::dropIndex");
#ifndef NDEBUG
	{
		ReadGuard guard(master_->getLock());
		DBUG_PRINT("sparrow_alter", ("Removing index %s from partition %s.%s.%llu", master_->getIndexes()[index].getName().c_str(),
			master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial())));
	}
#endif
	if (version_ == 0 && index == 0) {
		return AlterationStats();
	}
#ifndef NDEBUG
	uint64_t tstart = my_micro_time();
#endif
	AlterationStats stats;
	try {
		PartitionReaderGuard guard(*this, index, false, BlockCacheHint::smallForward0_);
		PartitionReader& reader = guard.get();
		const FileHeaderBase& header = reader.getHeader();
		stats = AlterationStats(0, -static_cast<int64_t>(header.getTotalSize()));
	} catch (const SparrowException& e) {
		// Ignore exception: it is usually a "file not found" error because we are
		// dropping an index not yet completely created, so the index file is missing.
	}

	char name[FN_REFLEN];
	FileCache::releaseFile(FileId(getFileName(index, name), FILE_TYPE_INDEX, FILE_MODE_READ), true);		
#ifndef NDEBUG
	const Str duration(Str::fromDuration((my_micro_time() - tstart) / 1000));
	DBUG_PRINT("sparrow_alter", ("Deleted index %u for partition %s.%s.%llu in %s", index,
		master_->getDatabase().c_str(), master_->getTable().c_str(), static_cast<ulonglong>(getSerial()), duration.c_str()));
#endif
	return stats;
}

bool PersistentPartition::makeChecks() const {
	time_t start;
	struct tm t;
	start = static_cast<time_t>(fileTime_ / 1000);
	if (gmtime_r(&start, &t) == 0) {
		return false;
	}
	return Master::checkForCorruption(t);
}

}
