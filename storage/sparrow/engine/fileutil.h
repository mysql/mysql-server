/*
	File utilities.
*/

#ifndef _engine_fileutil_h_
#define _engine_fileutil_h_

#include "../handler/plugin.h"	// For configuration parameters.
#include "cache.h"
#include "io.h"

#ifdef _WIN32
#include <direct.h>
#ifndef rmdir
#define rmdir _rmdir
#endif
#else
#include <sys/types.h>
#include <dirent.h>
#endif

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Filesystem
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Filesystem {
private:

	Str path_;
	uint64_t size_;
	uint64_t used_;
	volatile mutable uint64_t free_;

public:

	Filesystem() : free_(0) {
	}
	Filesystem(const char* path) : path_(path), size_(0), used_(0), free_(0) {
	}
	const Str& getPath() const {
		return path_;
	}
	uint64_t getSize() const {
		return size_;
	}
	uint64_t getUsed() const {
		return used_;
	}
	uint64_t getFree() const {
		return Atomic::get64(&free_);
	}
	uint64_t computeStats();
};

typedef SYSpVector<Filesystem, 0> Filesystems;
typedef SYSvector<uint32_t> FilesystemIds;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DirGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DirGuard {
#ifdef _WIN32
private:
	HANDLE h_;
public:
	DirGuard(const char* path, WIN32_FIND_DATA* data) _THROW_(SparrowException) {
		h_ = FindFirstFile(path, data);
		if (h_ == INVALID_HANDLE_VALUE) {
			throw SparrowException::create(true, "Cannot open directory %s", path);
		}
	}
	~DirGuard() {
		FindClose(h_);
	}
	HANDLE get() {
		return h_;
	}
#else
private:
	DIR* dir_;
public:
	DirGuard(const char* path) _THROW_(SparrowException) {
		dir_ = opendir(path);
		if (dir_ == 0) {
			throw SparrowException::create(true, "Cannot open directory %s", path);
		}
	}
	~DirGuard() {
		closedir(dir_);
	}
	DIR* get() {
		return dir_;
	}
#endif
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileUtil
//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef SYSslist<Str> Files;

#define COALESCING_FILESYSTEM	1000

class FileUtil {
private:

	static uint32_t sectorSize_;
	static uint32_t pageSize_;
	static const char separator_;
	static Filesystems filesystems_;
	static FilesystemIds filesystemIds_;
	static Filesystems coalescingFilesystems_;
	static FilesystemIds coalescingFilesystemIds_;
	static Filesystems allFilesystems_;
	static Lock lock_;
	static struct rand_struct rnd_;

private:

	static uint64_t updateStats(Filesystems& filesystems, FilesystemIds& filesystemIds, const bool coalescing);

public:

	static void initialize() _THROW_(SparrowException);

	static void createDirectories(const char* path);

	static void deleteDirectory(const char* path);

	static void scanDirectory(const char* path, const char* extension,
		const uint32_t level, Files& files, const bool forFiles) _THROW_(SparrowException);

	static const char* getParent(const char* path, char* buffer);

	static bool doesFileExist(const char* path);

	static uint64_t getFileSize(File file) _THROW_(SparrowException);

	static void rename(const char* from, const char* to) _THROW_(SparrowException);

	static Str getDatabaseName(const char* path);

	static Str getTableName(const char* path);

	// Gets the sector size of the MySQL data directory.
	static uint32_t getSectorSize() {
		return sectorSize_;
	}

	static void getDiskStats(uint64_t& totalFree, uint64_t& totalUsed, uint64_t& totalSize);

	// Gets the system page size.
	static uint32_t getPageSize() {
		return pageSize_;
	}

	// Adjusts the given size to a multiple of sector size.
	static uint64_t adjustSizeToSectorSize(const uint64_t size) {
		const uint32_t modulo = static_cast<uint32_t>(size % sectorSize_);
		if (modulo == 0) {
			return size;
		} else {
			return size + sectorSize_ - modulo;
		}
	}

	// Adjusts the given position to a multiple of sector size.
	static uint64_t adjustPosToSectorSize(const uint64_t pos) {
		return pos - (pos % sectorSize_);
	}

	// Gets the free disk space.
	static uint64_t getFreeDiskSpace();

	// Chooses the file system to write to, optionally requesting the coalescing file system.
	static uint32_t chooseFilesystem(const bool coalescing);

	// Get the path of a filesystem.
	static const char* getFilesystemPath(const uint32_t filesystem);

	// Get file systems, optionally including coalescing file system.
	static const Filesystems& getFilesystems(const bool withCoalescing) {
		return withCoalescing ? allFilesystems_ : filesystems_;
	}

	// Report status of file systems.
	static void report(PrintBuffer& buffer) _THROW_(SparrowException);

	static void dbg_dump_content(const char* path) _THROW_(SparrowException);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ReferencedBlock
//////////////////////////////////////////////////////////////////////////////////////////////////////

class ReferencedBlock : public SYSidlink<ReferencedBlock> {
private:

	const uint64_t offset_;
	BlockCacheEntry* entry_;

private:

	ReferencedBlock(const ReferencedBlock&);
	ReferencedBlock& operator = (const ReferencedBlock&);

public:
	
	ReferencedBlock(const uint64_t offset, BlockCacheEntry* entry = 0) : offset_(offset), entry_(entry) {
	}

	uint64_t getOffset() const {
		return offset_;
	}

	BlockCacheEntry* getEntry() {
		return entry_;
	}

	bool operator == (const ReferencedBlock& right) const {
		return offset_ == right.offset_;
	}

	uint32_t hash() const {
		return 31 + static_cast<uint32_t>(offset_ ^ (offset_ >> 32));
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ReferencedBlocks
//////////////////////////////////////////////////////////////////////////////////////////////////////

class ReferencedBlocks : public SYSpHash<ReferencedBlock> {
public:

	static volatile uint32_t lockedBlocks_;
	static volatile uint32_t maxLockedBlocks_;

private:

	SYSidlist<ReferencedBlock> lru_;

public:

	ReferencedBlocks() : SYSpHash<ReferencedBlock>(16) {
	}

	void reset(bool clear=false) {
		while (!lru_.isEmpty()) {
			ReferencedBlock* block = lru_.removeFirst();
			BlockCacheEntry* entry = block->getEntry();
			BlockCache::get().release(entry, entry->getLevel(), clear, true);
			Atomic::dec32(&lockedBlocks_);
			remove(block);
			delete block;
		}
	}

	~ReferencedBlocks() {
		reset();
	}

	BlockCacheEntry* get(const FileOffset& offset, const BlockCacheHint& hint) _THROW_(SparrowException) {
		assert(offset.getOffset() % sparrow_cache_block_size == 0);
		const ReferencedBlock key(offset.getOffset());
		ReferencedBlock* block = find(&key);
		if (block == 0) {
			BlockCacheEntry* entry = 0;
			while (!lru_.isEmpty() && lockedBlocks_ > maxLockedBlocks_) {
				ReferencedBlock* block_t = lru_.removeFirst();
				remove(block_t);
				BlockCacheEntry* old = block_t->getEntry();
				delete block_t;
				if (entry == 0) {
					entry = BlockCache::get().releaseAndAcquire(old, hint.getLevel(), offset, hint);
				} else {
					BlockCache::get().release(old, old->getLevel(), false, true);
					Atomic::dec32(&lockedBlocks_);
				}
			}
			if (entry == 0) {
				entry = BlockCache::get().acquire(hint.getLevel(), offset, hint, true, true);
				Atomic::inc32(&lockedBlocks_);
			}
			block = new ReferencedBlock(offset.getOffset(), entry);
			insert(block);
			lru_.append(block);
		} else {
			// Move block to the end of the LRU.
			lru_.remove(block);
			lru_.append(block);
		}
		BlockCacheEntry* entry = block->getEntry();
		if (!entry->isValid()) {
			char name[FN_REFLEN];
			throw SparrowException::create(false, "Cannot read from file %s at offset %llu", entry->getId().getFileName(name), static_cast<ulonglong>(key.getOffset()));
		}
		return entry;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileReader
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Reads data from a file.
// If the file is a partition file, the reader will use the file block cache.

class FileReader : public ByteBuffer, ByteBufferOverflow {
protected:

	ReferencedBlocks blocks_;
	const uint32_t size_;
	const ReadCacheHint* cacheHint_;
	FileCacheEntry* entry_;			// Used with non-cacheable file.
	FileOffset fileOffset_;

protected:

	void read(const uint64_t offset) _THROW_(SparrowException) {
		if (entry_ == 0) {
			const uint64_t adjustedOffset = offset - (offset % sparrow_cache_block_size);
			fileOffset_.setOffset(adjustedOffset);
			BlockCacheEntry* entry = blocks_.get(fileOffset_, cacheHint_->getBlockHint());
			if (!entry->isValid()) {
				char name[FN_REFLEN];
				throw SparrowException::create(false, "Cannot read from file %s at offset %llu", entry->getId().getFileName(name), static_cast<ulonglong>(offset));
			}
			const FileBlock& block = entry->getValue();
			*static_cast<ByteBuffer*>(this) = ByteBuffer(block.getData(), block.getLength(), this, getVersion());

			// Adjust position in buffer.
			position(static_cast<uint32_t>(offset % sparrow_cache_block_size));
		} else {
			// Do not use cache.
			fileOffset_.setOffset(offset);
			limit(entry_->getValue().read(offset, data_, size_));
			position(0);
		}
	}

public:

	FileReader(const PersistentPartition& partition, const uint32_t fileId, const ReadCacheHint* cacheHint)
		: ByteBuffer(0, 0, this), size_(static_cast<uint32_t>(limit())), cacheHint_(cacheHint), entry_(0), fileOffset_(partition, fileId, 0) {
	}

	FileReader(const char* name) _THROW_(SparrowException)
		: ByteBuffer(IOContext::getBuffer(sparrow_medium_read_block_size), this), size_(static_cast<uint32_t>(limit())), cacheHint_(0), entry_(0) {
		try {
			entry_ = FileCache::get().acquire(0, FileId(name, FILE_TYPE_MISC, FILE_MODE_READ), 0, true, true);
			if (!entry_->isValid()) {
				FileCache::get().release(entry_, 0, true, true);
				throw SparrowException::create(false, "Cannot open file %s for reading", name);
			}
			read(0);
		} catch(const SparrowException&) {
			release();
			throw;
		}
	}

	virtual ~FileReader() {
		if (entry_ != 0) {
			FileCache::get().release(entry_, 0, true, true);
		}
	}

	const char* getFileName(char* name) const _THROW_(SparrowException) {
		return fileOffset_.getFileName(name);
	}

	// When guard goes out of scope.
	void release() {
		if (ReferencedBlocks::lockedBlocks_ > ReferencedBlocks::maxLockedBlocks_) {
			blocks_.reset();
		}
	}

	// Releases cached file handle and data. Resets offset to 0. 
	void close(bool clear) {
		position(0);
		fileOffset_.setOffset(0);
		if (entry_ != 0) {
			FileCache::get().release(entry_, 0, true, true);
			entry_ = 0;
		}
		blocks_.reset(clear);
	}

	void open() {

	}

	// Absolute seek within the file.
	uint64_t seek(const uint64_t offset) {
		const uint64_t start = fileOffset_.getOffset();
		if (entry_ == 0) {
			// Check if new offset inside the current block, if any.
			if (!blocks_.isEmpty() && offset >= start && offset < start + limit()) {
				// Yes: adjust position in buffer.
				position(offset - start);
			} else {
				read(offset);
			}
		} else {
			// Do not use cache.
			if (offset >= start && offset < start + limit()) {
				position(offset - start);
			} else {
				read(offset);
			}
		}
		return offset;
	}

	void overflow() override _THROW_(SparrowException) {
		seek(fileOffset_.getOffset() + position());
	}

	bool end() const override {
		return false;	// Let actual EOF generate an exception.
	}

	uint64_t getFileOffset() const {
		return fileOffset_.getOffset() + position();
	}

	uint64_t getFileSize() const {
		FileId key;
		fileOffset_.getFileName(key.getName());
		FileCacheGuard guard(FileCache::get(), 0, key, 0, false);
		const FileHandle& handle = guard.get()->getValue();
		return handle.getSize();
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileWriter
//////////////////////////////////////////////////////////////////////////////////////////////////////

// This class is used to write data to a new file (.tmp renamed at the end) or to update/extend an existing file.
// It has the following limited behavior: it writes data between the last seek and the current position
// when write() is called.
// It can optionnally update the file block cache with written data.

class FileWriter : public ByteBuffer, ByteBufferOverflow {
private:

	static const char* SUFFIX;

	FileCacheEntry* entry_;
	char filename_[FN_REFLEN];
	uint64_t size_;		// File size.
	uint64_t start_;		// Adjusted offset, corresponding to position 0 in buffer.
	uint64_t mark_;
	uint64_t length_;

	const WriteCacheHint* cacheHint_;

	bool failed_;

private:

	FileMode getMode() const {
		return entry_->getId().getMode();
	}

public:

	FileWriter(const char* name, const FileType type, const FileMode mode, const WriteCacheHint* cacheHint = 0,
		const uint64_t offset = 0, const uint32_t length = 0) _THROW_(SparrowException)
		: ByteBuffer(IOContext::getBuffer(sparrow_write_block_size), this), size_(0), start_(0), mark_(0), length_(0), cacheHint_(cacheHint), failed_(false) {
		strcpy(filename_, name);
		if (mode == FILE_MODE_CREATE) {
			// When creating a new file, the file name is suffixed with ".tmp".
			// The file is renamed when this file writer is destroyed.
			strcat(filename_, SUFFIX);
		}
		entry_ = FileCache::get().acquire(0, FileId(filename_, type, mode), 0, true, true);
		if (!entry_->isValid()) {
			FileCache::get().release(entry_, 0, true, true);
			throw SparrowException::create(false, "Cannot open file %s for writing", filename_);
		}
		if (mode == FILE_MODE_UPDATE) {
			start_ = ULLONG_MAX;
			size_ = entry_->getValue().getSize();
			seek(std::min(size_, offset), length);
		}
	}

	virtual ~FileWriter();

	void seek(const uint64_t offset, const uint64_t length) _THROW_(SparrowException);

	void write() _THROW_(SparrowException);

	void overflow() override _THROW_(SparrowException) {
		write();
		const uint64_t offset = start_ + limit();
		if (offset != size_) {
			seek(offset, limit() - mark_);
		} else {
			start_ += limit();
			mark_ = 0;
			position(0);
		}
	}

	bool end() const override {
		return false;	// No EOF when writing.
	}

	uint64_t getFileOffset() const {
		return start_ + position();
	}

	uint64_t getFileSize() const {
		return size_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileHeader
//////////////////////////////////////////////////////////////////////////////////////////////////////

class FileHeader : public FileHeaderBase {
	friend ByteBuffer& operator << (ByteBuffer& buffer, const FileHeader& header);
	friend ByteBuffer& operator >> (ByteBuffer& buffer, FileHeader& header);

private:

	static const uint32_t sizes_[];

	uint32_t format_;					// File format; see FileHeader::currentFileFormat_.
	uint64_t totalSize_;				// Total file size, including padding and header.
	uint32_t genTime_;				// Generation timestamp (seconds since epoch).
	FileSection recordsSection_;	// Section containing all index records.
	uint32_t recordSize_;				// Size of a single record.
	uint32_t records_;				// Number of records.
	FileSection binSection_;		// Section containing binary (string) data.
	FileSection treeSection_;		// Section containing the tree (the tree enables fast navigation).
	bool treeComplete_;				// True if the tree contains all index values.
	uint32_t nodeSize_;				// Size of a single tree node.
	uint32_t nodes_;					// Number of nodes.
	uint32_t index_;					// DATA_FILE if this is a data file, or index id if this is an index file.
	uint64_t start_;					// Start timestamp (milliseconds since epoch).
	uint64_t end_;					// End timestamp (milliseconds since epoch).

	// To browse tree (not persisted).
	const TreeOrder* treeOrder_;

public:

	static const uint8_t currentFileFormat_;

private:

	static uint32_t computeSize();

public:

	FileHeader();
	FileHeader(const uint64_t binSize, const uint32_t treeSize, const bool treeComplete, const uint32_t nodeSize, const uint32_t recordSize,
		const uint32_t records, const uint32_t index, const uint64_t start, const uint64_t end);

	void initialize(const uint32_t nodeSize = 0);

	uint64_t getStart() const override {
		return start_;
	}

	uint64_t getEnd() const override {
		return end_;
	}

	uint64_t getRecords() const override {
		return records_;
	}

	uint32_t getRecordSize() const override {
		return recordSize_;
	}

	bool isTreeComplete() const override {
		return treeComplete_;
	}

	uint32_t getNodes() const override {
		return nodes_;
	}

	uint32_t getMinNode() const override {
		const uint32_t depth = TreeOrder::depth(nodes_);
		return (1 << (depth - 1)) - 1;
	}

	uint32_t getMaxNode() const override {
		const uint32_t depth = TreeOrder::depth(nodes_);
		if (nodes_ == static_cast<uint32_t>((1 << depth) - 1)) {
			return nodes_ - 1;
		} else {
			return (1 << (depth - 1)) - 2;
		}
	}

	uint32_t getPrevNode(const uint32_t node) const override {
		return treeOrder_->getNodeIndex(treeOrder_->getListIndex(node, nodes_) - 1, nodes_);
	}

	uint32_t getNextNode(const uint32_t node) const override {
		return treeOrder_->getNodeIndex(treeOrder_->getListIndex(node, nodes_) + 1, nodes_);
	}

	uint64_t seekTree(FileReader& reader, const uint64_t node) const override {
		const uint64_t offset = node * nodeSize_;
		assert(offset < treeSection_.getSize());
		return reader.seek(treeSection_.getOffset() + offset);
	}

	uint64_t seekTreeData(FileReader& reader, const uint64_t node) const override {
		const uint64_t offset = node * nodeSize_ + 8 /* TODO row size */;
		assert(offset <= treeSection_.getSize());
		return reader.seek(treeSection_.getOffset() + offset);
	}

	uint64_t seekRecord(FileReader& reader, const uint64_t record) const override {
		const uint64_t offset = record * recordSize_;
		assert(offset < recordsSection_.getSize());
		return reader.seek(recordsSection_.getOffset() + offset);
	}

	uint64_t seekRecordData(FileReader& reader, const uint64_t record) const override {
		const uint64_t offset = record * recordSize_ + (index_ == DATA_FILE ? 0 : 4) /* TODO row size */;
		assert(offset < recordsSection_.getSize());
		assert(index_ == DATA_FILE || treeComplete_);
		return reader.seek(recordsSection_.getOffset() + offset);
	}

	uint64_t seekBin(FileReader& reader, const uint64_t offset) const override {
		assert(offset < binSection_.getSize());
		return reader.seek(binSection_.getOffset() + offset);
	}

	const FileSection& getStringsSection() const override {
		assert(0);
		return *static_cast<const FileSection*>(0);
	}

	uint64_t getTotalSize() const override {
		return totalSize_;
	}

	uint32_t getFormat() const {
		return format_;
	}

	const FileSection& getBinSection() const {
		return binSection_;
	}

	const FileSection& getRecordsSection() const {
		return recordsSection_;
	}

	const FileSection& getTreeSection() const {
		return treeSection_;
	}

	static uint32_t size(const uint32_t format = FileHeader::currentFileFormat_) {
		return sizes_[format];
	}
};

inline ByteBuffer& operator << (ByteBuffer& buffer, const FileHeader& header) {
	buffer << header.totalSize_ << header.genTime_ << header.recordsSection_
		<< header.recordSize_ << header.records_ << header.binSection_ << header.treeSection_
		<< header.treeComplete_ << header.nodeSize_ << header.nodes_ << header.index_ << header.start_ << header.end_;
	return buffer;
}

inline ByteBuffer& operator >> (ByteBuffer& buffer, FileHeader& header) {
	const uint32_t version = buffer.getVersion();	// See FileHeader::currentFileFormat_.
	header.format_ = version;
	if (version == 1) {
		uint64_t binSize;
		uint32_t treeSize;
		buffer >> header.totalSize_ >> header.genTime_ >> binSize
			>> treeSize >> header.treeComplete_ >> header.recordSize_ >> header.records_
			>> header.index_ >> header.start_ >> header.end_;
		const uint32_t headerSize = FileHeader::size(version);
		header.recordsSection_ = FileSection(headerSize + treeSize + binSize, static_cast<uint64_t>(header.records_) * header.recordSize_);
		header.binSection_ = FileSection(headerSize + treeSize, binSize);
		header.treeSection_ = FileSection(headerSize, treeSize);
	} else if (version == 2) {
		buffer >> header.totalSize_ >> header.genTime_ >> header.recordsSection_	
			>> header.recordSize_ >> header.records_ >> header.binSection_ >> header.treeSection_
			>> header.treeComplete_ >> header.index_ >> header.start_ >> header.end_;
	} else {
		buffer >> header.totalSize_ >> header.genTime_ >> header.recordsSection_	
			>> header.recordSize_ >> header.records_ >> header.binSection_ >> header.treeSection_
			>> header.treeComplete_ >> header.nodeSize_ >> header.nodes_ >> header.index_ >> header.start_ >> header.end_;
	}
	return buffer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DataFileHeader
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DataFileHeader : public FileHeaderBase {
	friend ByteBuffer& operator << (ByteBuffer& buffer, const DataFileHeader& header);
	friend ByteBuffer& operator >> (ByteBuffer& buffer, DataFileHeader& header);

private:

	uint32_t genTime_;				// Generation timestamp (seconds since epoch).
	FileSection recordsSection_;	// Section containing all data records.
	uint32_t recordSize_;				// Size of a single record.
	uint64_t records_;				// Number of records.
	FileSection stringsSection_;	// Section in strings file for fast loading.
	uint64_t start_;					// Start timestamp (milliseconds since epoch).
	uint64_t end_;					// End timestamp (milliseconds since epoch).

	static const uint32_t size_;

private:

	 static uint32_t computeSize();

public:

	DataFileHeader();
	DataFileHeader(const uint32_t recordSize, const uint64_t records, const uint64_t stringOffset, const uint64_t stringSize, const uint64_t start, const uint64_t end);

	uint64_t getStart() const override {
		return start_;
	}

	uint64_t getEnd() const override {
		return end_;
	}

	uint64_t getRecords() const override {
		return records_;
	}

	uint32_t getRecordSize() const override {
		return recordSize_;
	}

	bool isTreeComplete() const override {
		assert(0);
		return false;
	}

	uint32_t getNodes() const override {
		assert(0);
		return 0;
	}

	uint32_t getMinNode() const override {
		assert(0);
		return 0;
	}

	uint32_t getMaxNode() const override {
		assert(0);
		return 0;
	}

	uint32_t getPrevNode(const uint32_t node) const override {
		assert(0);
		return 0;
	}

	uint32_t getNextNode(const uint32_t node) const override {
		assert(0);
		return 0;
	}

	uint64_t seekTree(FileReader& reader, const uint64_t node) const override {
		assert(0);
		return 0;
	}

	uint64_t seekTreeData(FileReader& reader, const uint64_t node) const override {
		assert(0);
		return 0;
	}

	uint64_t seekRecord(FileReader& reader, const uint64_t record) const override {
		const uint64_t offset = record * recordSize_;
		assert(offset < recordsSection_.getSize());
		return reader.seek(recordsSection_.getOffset() + offset);
	}

	uint64_t seekRecordData(FileReader& reader, const uint64_t record) const override {
		return seekRecord(reader, record);
	}

	uint64_t seekBin(FileReader& reader, const uint64_t offset) const override {
		assert(0);
		return 0;
	}

	const FileSection& getStringsSection() const override {
		return stringsSection_;
	}

	uint64_t getTotalSize() const override {
		return FileUtil::adjustSizeToSectorSize(recordsSection_.getOffset() + recordsSection_.getSize());
	}

	const FileSection& getRecordsSection() const {
		return recordsSection_;
	}

	uint32_t getCacheLevel(const uint64_t offset) const {
		// Level 2: contains data records from recent queries.
		return 2;
	}

	static uint32_t size() {
		return size_;
	}
};

inline ByteBuffer& operator << (ByteBuffer& buffer, const DataFileHeader& header) {
	buffer << header.genTime_ << header.recordsSection_ << header.recordSize_
		<< header.stringsSection_ << header.start_ << header.end_;
	return buffer;
}

inline ByteBuffer& operator >> (ByteBuffer& buffer, DataFileHeader& header) {
	buffer >> header.genTime_ >> header.recordsSection_ >> header.recordSize_
		>> header.stringsSection_ >> header.start_ >> header.end_;
	header.records_ = header.recordsSection_.getCount("records", header.recordSize_);
	return buffer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// IndexFileHeader
//////////////////////////////////////////////////////////////////////////////////////////////////////

class IndexFileHeader : public FileHeaderBase {
	friend ByteBuffer& operator << (ByteBuffer& buffer, const IndexFileHeader& header);
	friend ByteBuffer& operator >> (ByteBuffer& buffer, IndexFileHeader& header);

private:

	uint32_t genTime_;				// Generation timestamp (seconds since epoch).
	uint32_t index_;					// Index id.
	FileSection recordsSection_;	// Section containing all index records.
	uint32_t recordSize_;				// Size of a single record.
	uint64_t records_;				// Number of records.
	FileSection treeSection_;		// Section containing all tree records.
	uint32_t nodeSize_;				// Size of a single tree node.
	uint64_t nodes_;					// Number of nodes.
	uint64_t start_;					// Start timestamp (milliseconds since epoch).
	uint64_t end_;					// End timestamp (milliseconds since epoch).

	// To browse tree (not persisted).
	const TreeOrder* treeOrder_;

	static const uint32_t size_;

private:

	 static uint32_t computeSize();

public:

	IndexFileHeader();
	IndexFileHeader(const uint32_t index, const uint32_t recordSize, const uint64_t records,
		const uint32_t nodeSize, const uint64_t nodes, const uint64_t start, const uint64_t end);

	uint64_t getStart() const override {
		return start_;
	}

	uint64_t getEnd() const override {
		return end_;
	}

	uint64_t getRecords() const override {
		return records_;
	}

	uint32_t getRecordSize() const override {
		return recordSize_;
	}

	bool isTreeComplete() const override {
		return true;
	}

	uint32_t getNodes() const override {
		return static_cast<uint32_t>(nodes_);
	}

	uint32_t getMinNode() const override {
		const uint32_t depth = TreeOrder::depth(static_cast<uint32_t>(nodes_));
		return (1 << (depth - 1)) - 1;
	}

	uint32_t getMaxNode() const override {
		const uint32_t depth = TreeOrder::depth(static_cast<uint32_t>(nodes_));
		if (nodes_ == static_cast<uint32_t>((1 << depth) - 1)) {
			return static_cast<uint32_t>(nodes_ - 1);
		} else {
			return (1 << (depth - 1)) - 2;
		}
	}

	uint32_t getPrevNode(const uint32_t node) const override {
		return treeOrder_->getNodeIndex(treeOrder_->getListIndex(node, static_cast<uint32_t>(nodes_)) - 1, static_cast<uint32_t>(nodes_));
	}

	uint32_t getNextNode(const uint32_t node) const override {
		return treeOrder_->getNodeIndex(treeOrder_->getListIndex(node, static_cast<uint32_t>(nodes_)) + 1, static_cast<uint32_t>(nodes_));
	}

	uint64_t seekTree(FileReader& reader, const uint64_t node) const override {
		const uint64_t offset = node * nodeSize_;
		assert(offset < treeSection_.getSize());
		return reader.seek(treeSection_.getOffset() + offset);
	}

	uint64_t seekTreeData(FileReader& reader, const uint64_t node) const override {
		const uint64_t offset = node * nodeSize_ + 8 /* TODO row size */;
		assert(offset <= treeSection_.getSize());
		return reader.seek(treeSection_.getOffset() + offset);
	}

	uint64_t seekRecord(FileReader& reader, const uint64_t record) const override {
		const uint64_t offset = record * recordSize_;
		assert(offset < recordsSection_.getSize());
		return reader.seek(recordsSection_.getOffset() + offset);
	}

	uint64_t seekRecordData(FileReader& reader, const uint64_t record) const override {
		assert(0);
		return 0;
	}

	uint64_t seekBin(FileReader& reader, const uint64_t offset) const override {
		assert(0);
		return 0;
	}

	const FileSection& getStringsSection() const override {
		assert(0);
		return *static_cast<const FileSection*>(0);
	}

	uint64_t getTotalSize() const override {
		return FileUtil::adjustSizeToSectorSize(treeSection_.getOffset() + treeSection_.getSize());
	}

	static uint32_t size() {
		return size_;
	}
};

inline ByteBuffer& operator << (ByteBuffer& buffer, const IndexFileHeader& header) {
	buffer << header.genTime_ << header.index_ << header.recordsSection_ << header.recordSize_
		<< header.treeSection_ << header.nodeSize_ << header.start_ << header.end_;
	return buffer;
}

inline ByteBuffer& operator >> (ByteBuffer& buffer, IndexFileHeader& header) {
	buffer >> header.genTime_ >> header.index_ >> header.recordsSection_ >> header.recordSize_
		>> header.treeSection_ >> header.nodeSize_ >> header.start_ >> header.end_;
	header.nodes_ = header.treeSection_.getCount("nodes", header.nodeSize_);
	header.treeOrder_ = &TreeOrder::get(static_cast<uint32_t>(header.nodes_));
	header.records_ = header.recordsSection_.getCount("records", header.recordSize_);
	return buffer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// StringFileHeader
//////////////////////////////////////////////////////////////////////////////////////////////////////

class StringFileHeader : public FileHeaderBase {
	friend ByteBuffer& operator >> (ByteBuffer& buffer, StringFileHeader& header);

public:

	StringFileHeader() {
	}

	uint64_t getStart() const override {
		assert(0);
		return 0;
	}

	uint64_t getEnd() const override {
		assert(0);
		return 0;
	}

	uint64_t getRecords() const override {
		assert(0);
		return 0;
	}

	uint32_t getRecordSize() const override {
		assert(0);
		return 0;
	}

	bool isTreeComplete() const override {
		assert(0);
		return false;
	}

	uint32_t getNodes() const override {
		assert(0);
		return 0;
	}

	uint32_t getMinNode() const override {
		assert(0);
		return 0;
	}

	uint32_t getMaxNode() const override {
		assert(0);
		return 0;
	}

	uint32_t getPrevNode(const uint32_t node) const override {
		assert(0);
		return 0;
	}

	uint32_t getNextNode(const uint32_t node) const override {
		assert(0);
		return 0;
	}

	uint64_t seekTree(FileReader& reader, const uint64_t node) const override {
		assert(0);
		return 0;
	}

	uint64_t seekTreeData(FileReader& reader, const uint64_t node) const override {
		assert(0);
		return 0;
	}

	uint64_t seekRecord(FileReader& reader, const uint64_t record) const override {
		assert(0);
		return 0;
	}

	uint64_t seekRecordData(FileReader& reader, const uint64_t record) const override {
		assert(0);
		return 0;
	}

	uint64_t seekBin(FileReader& reader, const uint64_t offset) const override {
		reader.seek(offset);
		return offset;
	}

	const FileSection& getStringsSection() const override {
		assert(0);
		return *static_cast<const FileSection*>(0);
	}

	uint64_t getTotalSize() const override {
		assert(0);
		return 0;
	}
};

inline ByteBuffer& operator >> (ByteBuffer& buffer, StringFileHeader& header) {
	return buffer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PartitionReader
//////////////////////////////////////////////////////////////////////////////////////////////////////

class PartitionReader : public ReadCacheHint, public FileReader {
protected:

	const BlockCacheHint& hint_;
	const uint32_t columnAlterSerial_;
	FileHeaderBase* header_;

private:

	PartitionReader(const PartitionReader&);
	PartitionReader& operator = (const PartitionReader&);

public:

	PartitionReader(const PersistentPartition& partition, const uint32_t fileId, const BlockCacheHint& hint) _THROW_(SparrowException);

	virtual ~PartitionReader() {
		delete header_;
	}

	uint32_t getFileId() const {
		return fileOffset_.getFileId();
	}

	const BlockCacheHint& getBlockHint() const override {
		return hint_;
	}

	uint32_t getColumnAlterSerial() const {
		return columnAlterSerial_;
	}

	uint64_t seekTree(const uint64_t node) {
		return header_->seekTree(*this, node);
	}

	uint64_t seekTreeData(const uint64_t node) {
		return header_->seekTreeData(*this, node);
	}

	uint64_t seekRecord(const uint64_t record) {
		return header_->seekRecord(*this, record);
	}

	uint64_t seekRecordData(const uint64_t record) {
		return header_->seekRecordData(*this, record);
	}

	uint64_t seekBin(const uint64_t offset) {
		return header_->seekBin(*this, offset);
	}

	const FileHeaderBase& getHeader() const {
		return *header_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PartitionReaders
//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef SYSpVector<PartitionReader, 0> PartitionReadersBase;

// Keep readers for both data and index files.
class PartitionReaders : private PartitionReadersBase {
private:

	PartitionReaders& operator = (const PartitionReaders& right);
	PartitionReaders(const PartitionReaders& right);

public:

	void initialize(const uint32_t nbPartitions) {
		clearAndDestroy();
		resize(nbPartitions * 8);
		for (uint32_t i = 0; i < PartitionReadersBase::capacity(); ++i) {
			PartitionReadersBase::append(0);
		}
	}

	PartitionReaders(const uint32_t nbPartitions = 0) {
		initialize(nbPartitions);
	}

	void clear() {
		clearAndDestroy();
	}

	~PartitionReaders() {
		clearAndDestroy();
	}

	PartitionReader* get(const uint32_t n, PersistentPartition& partition, const uint32_t index, const bool isString, const BlockCacheHint& hint) _THROW_(SparrowException);
};

}

#endif /* #ifndef _engine_fileutil_h_ */
