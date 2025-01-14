/*
	Cache.
*/

#include "cache.h"
#include "io.h"
#include "fileutil.h"
#include "master.h"
#include "persistent.h"
#include "purge.h"

namespace Sparrow {


//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileCache
//////////////////////////////////////////////////////////////////////////////////////////////////////

FileCache* FileCache::cache_ = 0;
PSI_file_key FileCache::dataKey_;
PSI_file_key FileCache::indexKey_;
PSI_file_key FileCache::stringKey_;
PSI_file_key FileCache::miscKey_;
PSI_file_info FileCache::psiInfo_[] = {
	{ &FileCache::dataKey_, "data", 0, PSI_VOLATILITY_UNKNOWN, PSI_DOCUMENT_ME},
	{ &FileCache::indexKey_, "index", 0, PSI_VOLATILITY_UNKNOWN, PSI_DOCUMENT_ME},
	{ &FileCache::stringKey_, "string", 0, PSI_VOLATILITY_UNKNOWN, PSI_DOCUMENT_ME},
	{ &FileCache::miscKey_, "misc", 0, PSI_VOLATILITY_UNKNOWN, PSI_DOCUMENT_ME}
};

// STATIC
void FileCache::initialize() _THROW_(SparrowException) {
	cache_ = new FileCache(sparrow_open_files);
}

FileCache::FileCache(uint32_t entries) : Cache<FileId, FileHandle, 1, int>("FileCache", &entries, 0,
	SparrowStatus::get().fileCacheAcquires_, SparrowStatus::get().fileCacheReleases_,
	SparrowStatus::get().fileCacheMisses_, SparrowStatus::get().fileCacheHits_, SparrowStatus::get().fileCacheSlowHits_) {
	SPARROW_ENTER("FileCache::FileCache");
#ifdef HAVE_PSI_INTERFACE
	mysql_file_register("sparrow", FileCache::psiInfo_, array_elements(FileCache::psiInfo_));
#endif
	DBUG_PRINT("sparrow_memory", ("Creating file cache with %u entries", entries));
}

// STATIC
void FileCache::releaseFile(const FileId& id, const bool remove) {
	FileCacheEntry* entry = FileCache::get().acquire(0, id, 0, false, false);
	if (entry != 0) {
		FileCache::get().release(entry, 0, true, false);
	}
	if (remove) {
		// Delete file.
		int		err = my_delete(id.getName(), MYF(0));
		if ( err != 0 && my_errno() != ENOENT ) {
			char	errMsg[MYSYS_STRERROR_SIZE];
			my_strerror(errMsg, sizeof(errMsg), my_errno());
			spw_print_information("Failed to delete %s: error code %d (%s)",id.getName(), my_errno(), errMsg);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// BlockCache
//////////////////////////////////////////////////////////////////////////////////////////////////////

BlockCache* BlockCache::cache_ = 0;

// Using large chunks reduces memory "fragmentation" on Solaris, which adds "guard" pages around
// mmap'ed areas. 8MB - 64K is the size used by IVServer, set experimentally.
uint32_t BlockCache::chunkSize_ = 8 * 1024 * 1024 - 64 * 1024;

BlockCache::BlockCache(uint32_t* entries, BlockCacheEntry** cacheEntries) _THROW_(SparrowException)
	: Cache<FileOffset, FileBlock, 4, BlockCacheHint>("BlockCache", entries, cacheEntries,
	SparrowStatus::get().blockCacheAcquires_, SparrowStatus::get().blockCacheReleases_,
	SparrowStatus::get().blockCacheMisses_, SparrowStatus::get().blockCacheHits_, SparrowStatus::get().blockCacheSlowHits_) {
	SPARROW_ENTER("BlockCache::BlockCache");
	DBUG_PRINT("sparrow_memory", ("Block size is %u bytes", sparrow_cache_block_size));
	DBUG_PRINT("sparrow_memory", ("Creating level 0 block cache with %u entries (%llu MB)", entries[0], (static_cast<ulonglong>(entries[0]) * sparrow_cache_block_size) / 1024 / 1024));
	DBUG_PRINT("sparrow_memory", ("Creating level 1 block cache with %u entries (%llu MB)", entries[1], (static_cast<ulonglong>(entries[1]) * sparrow_cache_block_size) / 1024 / 1024));
	DBUG_PRINT("sparrow_memory", ("Creating level 2 block cache with %u entries (%llu MB)", entries[2], (static_cast<ulonglong>(entries[2]) * sparrow_cache_block_size) / 1024 / 1024));
	DBUG_PRINT("sparrow_memory", ("Creating level 3 block cache with %u entries (%llu MB)", entries[3], (static_cast<ulonglong>(entries[3]) * sparrow_cache_block_size) / 1024 / 1024));
	DBUG_PRINT("sparrow_memory", ("Total size of block cache is %llu MB", ((static_cast<ulonglong>(entries[0]) + static_cast<ulonglong>(entries[1])
		+ static_cast<ulonglong>(entries[2]) + static_cast<ulonglong>(entries[3])) * sparrow_cache_block_size) / 1024 / 1024));

	LvlCacheStat	stat_per_lvl[4];
	stat_per_lvl[0].hits_ = &SparrowStatus::get().blockCacheLvl0Hits_;
	stat_per_lvl[1].hits_ = &SparrowStatus::get().blockCacheLvl1Hits_;
	stat_per_lvl[2].hits_ = &SparrowStatus::get().blockCacheLvl2Hits_;
	stat_per_lvl[3].hits_ = &SparrowStatus::get().blockCacheLvl3Hits_;
	stat_per_lvl[0].slowHits_ = &SparrowStatus::get().blockCacheLvl0SlowHits_;
	stat_per_lvl[1].slowHits_ = &SparrowStatus::get().blockCacheLvl1SlowHits_;
	stat_per_lvl[2].slowHits_ = &SparrowStatus::get().blockCacheLvl2SlowHits_;
	stat_per_lvl[3].slowHits_ = &SparrowStatus::get().blockCacheLvl3SlowHits_;
	stat_per_lvl[0].misses_ = &SparrowStatus::get().blockCacheLvl0Misses_;
	stat_per_lvl[1].misses_ = &SparrowStatus::get().blockCacheLvl1Misses_;
	stat_per_lvl[2].misses_ = &SparrowStatus::get().blockCacheLvl2Misses_;
	stat_per_lvl[3].misses_ = &SparrowStatus::get().blockCacheLvl3Misses_;
	init_lvl_stat( stat_per_lvl );
}

// Initialize cache. Memory is allocated once and for all.
// STATIC
void BlockCache::initialize() _THROW_(SparrowException) {
	// Check cache block size is a multiple of sector size.
	if (FileUtil::adjustSizeToSectorSize(sparrow_cache_block_size) != sparrow_cache_block_size) {
		throw SparrowException::create(false, "sparrow_cache_block_size (%u bytes) is not a multiple of the sector size (%u bytes)", sparrow_cache_block_size, FileUtil::getSectorSize());
	}

#ifdef _WIN32
	// Check cache block size is a multiple of page size. This is necessary on Windows only because
	// of limitations of ReadFileScatter().
	if (sparrow_cache_block_size % FileUtil::getPageSize() != 0) {
		throw SparrowException::create(false, "sparrow_cache_block_size (%u bytes) is not a multiple of the page size (%u bytes)", sparrow_cache_block_size, FileUtil::getPageSize());
	}
#endif

	// Check write block size is multiple of cache block size.
	if (sparrow_write_block_size % sparrow_cache_block_size != 0) {
		throw SparrowException::create(false, "sparrow_write_block_size (%u bytes) is not a multiple of sparrow_cache_block_size (%u bytes)", sparrow_write_block_size, sparrow_cache_block_size);
	}

	// Check read block sizes are a multiple of cache block size.
	if (sparrow_small_read_block_size < sparrow_cache_block_size ||
		(sparrow_small_read_block_size % sparrow_cache_block_size) != 0) {
		throw SparrowException::create(false, "sparrow_small_read_block_size (%u bytes) is not a multiple of sparrow_cache_block_size (%u bytes)", sparrow_small_read_block_size, sparrow_cache_block_size);
	}
	if (sparrow_medium_read_block_size < sparrow_cache_block_size ||
		(sparrow_medium_read_block_size % sparrow_cache_block_size) != 0) {
		throw SparrowException::create(false, "sparrow_medium_read_block_size (%u bytes) is not a multiple of sparrow_cache_block_size (%u bytes)", sparrow_medium_read_block_size, sparrow_cache_block_size);
	}
	if (sparrow_large_read_block_size < sparrow_cache_block_size ||
		(sparrow_large_read_block_size % sparrow_cache_block_size) != 0) {
		throw SparrowException::create(false, "sparrow_large_read_block_size (%u bytes) is not a multiple of sparrow_cache_block_size (%u bytes)", sparrow_large_read_block_size, sparrow_cache_block_size);
	}

	// Check small < medium < large read block sizes.
	if (sparrow_small_read_block_size >= sparrow_medium_read_block_size) {
		throw SparrowException::create(false, "sparrow_small_read_block_size (%u bytes) is larger than sparrow_medium_read_block_size (%u bytes)", sparrow_small_read_block_size, sparrow_medium_read_block_size);
	}
	if (sparrow_medium_read_block_size >= sparrow_large_read_block_size) {
		throw SparrowException::create(false, "sparrow_medium_read_block_size (%u bytes) is larger than sparrow_large_read_block_size (%u bytes)", sparrow_medium_read_block_size, sparrow_large_read_block_size);
	}

	// Check size of cache 0 is greater than read block sizes.
	if (sparrow_cache0_size < sparrow_large_read_block_size) {
		throw SparrowException::create(false, "sparrow_cache0_size (%llu bytes) is smaller than sparrow_large_read_block_size (%u bytes)", static_cast<ulonglong>(sparrow_cache0_size), sparrow_large_read_block_size);
	}

	// Check size of caches 1..3 is greater than cache block size.
	if (sparrow_cache1_size < sparrow_cache_block_size) {
		throw SparrowException::create(false, "sparrow_cache1_size (%llu bytes) is smaller than sparrow_cache_block_size (%u bytes)", static_cast<ulonglong>(sparrow_cache1_size), sparrow_cache_block_size);
	}
	if (sparrow_cache2_size < sparrow_cache_block_size) {
		throw SparrowException::create(false, "sparrow_cache2_size (%llu bytes) is smaller than sparrow_cache_block_size (%u bytes)", static_cast<ulonglong>(sparrow_cache2_size), sparrow_cache_block_size);
	}
	if (sparrow_cache3_size < sparrow_cache_block_size) {
		throw SparrowException::create(false, "sparrow_cache3_size (%llu bytes) is smaller than sparrow_cache_block_size (%u bytes)", static_cast<ulonglong>(sparrow_cache3_size), sparrow_cache_block_size);
	}

	// Initialize memory locking.
	ByteBuffer::initialize();

	// Initialize IO.
	IO::initialize();

	// Max number of blocks locked by partition readers.
	uint64_t minCacheSize = std::min(sparrow_cache0_size, sparrow_cache1_size);
	minCacheSize = std::min(minCacheSize, sparrow_cache2_size);
	minCacheSize = std::min(minCacheSize, sparrow_cache3_size);
	ReferencedBlocks::maxLockedBlocks_ = static_cast<uint32_t>(minCacheSize / 2 / sparrow_cache_block_size);

	// Round chunk size to the nearest multiple of sparrow_cache_block_size.
	chunkSize_ = (chunkSize_ / sparrow_cache_block_size) * sparrow_cache_block_size;
	if (chunkSize_ == 0) {
		chunkSize_ = sparrow_cache_block_size;
	}
	uint32_t entriesPerChunk = chunkSize_ / sparrow_cache_block_size;

	// Allocate entries for all cache levels.
	uint32_t entries[4];
	entries[0] = static_cast<uint32_t>(sparrow_cache0_size / sparrow_cache_block_size);
	entries[1] = static_cast<uint32_t>(sparrow_cache1_size / sparrow_cache_block_size);
	entries[2] = static_cast<uint32_t>(sparrow_cache2_size / sparrow_cache_block_size);
	entries[3] = static_cast<uint32_t>(sparrow_cache3_size / sparrow_cache_block_size);
	uint32_t totalEntries = entries[0] + entries[1] + entries[2] + entries[3];
	BlockCacheEntry* cacheEntries[4];
	memset(cacheEntries, 0, sizeof(cacheEntries));
	uint32_t i = 0;
	uint64_t allocated = 0;
	uint32_t index = 0;
	uint32_t level = 0;
	while (i < totalEntries) {
		uint32_t chunkEntries = std::min(totalEntries - i, entriesPerChunk);
		uint32_t size = chunkEntries * sparrow_cache_block_size;
		uint8_t* buffer = ByteBuffer::mmap(size);
		if (buffer == 0) {
			throw SparrowException::create(true, "Cache initialization: cannot allocate %uKB (already allocated %uMB)",
				size / 1024, static_cast<uint32_t>(allocated / (1024ULL * 1024)));
		}
		for (uint32_t offset = 0; offset < size; offset += sparrow_cache_block_size) {
			BlockCacheEntry* entry = new BlockCacheEntry(level, FileOffset(), FileBlock(buffer + offset));
			if (cacheEntries[level] != 0) {
				cacheEntries[level]->prev_ = entry;
				entry->next_ = cacheEntries[level];
			}
			cacheEntries[level] = entry;
			if (++index == entries[level]) {
				level++;
				index = 0;
			}
		}
		allocated += size;
		i += chunkEntries;
	}

	// Build cache.
	cache_ = new BlockCache(entries, cacheEntries);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PartitionFile
//////////////////////////////////////////////////////////////////////////////////////////////////////

PartitionFile::PartitionFile(const PersistentPartition& partition, const uint32_t fileId)
	: id_(partition.getMaster().getId()), fileId_(fileId),
	serial_(fileId == DATA_FILE || fileId == STRING_FILE ? partition.getDataSerial() : partition.getSerial()) {
}

const char* PartitionFile::getFileName(char* name) const _THROW_(SparrowException) {
	MasterGuard master = MasterId::get(id_);
	if (master == 0) {
		throw SparrowException::create(false, "PartitionFile: cannot find master with id %u", id_);
	}
	PartitionGuard partition = master->getPartition(serial_);
	if (partition == 0) {
		throw SparrowException::create(false, "PartitionFile: cannot find persistent partition %s.%s.%llu",
			master->getDatabase().c_str(), master->getTable().c_str(), static_cast<ulonglong>(serial_));
	}
	return static_cast<PersistentPartition*>(partition.get())->getFileName(getFileId(), name);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileHandle
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Opens file.
void FileHandle::initialize(const FileId& id, SYSpVector<CacheEntry<FileId, FileHandle, int>, 64>& entries, const int hint) _THROW_(SparrowException) {
	SPARROW_ENTER("FileHandle::initialize");
#ifndef _WIN32
	if (lock_ == 0) {
		const char*		name = id.getName();
		const unsigned int		max_len = 64;
		if (strlen(name) > max_len) name += strlen(name) - max_len;
		lock_ = new Lock(false, (Str("FileHandle::lock_(") + Str(name) + Str(")")).c_str());
	}
#endif
	const FileType type = id.getType();
	switch (type) {
		case FILE_TYPE_DATA: key_ = FileCache::dataKey_; break;
		case FILE_TYPE_INDEX: key_ = FileCache::indexKey_; break;
		case FILE_TYPE_STRING: key_ = FileCache::stringKey_; break;
		case FILE_TYPE_MISC: key_ = FileCache::miscKey_; break;
		default: assert(0);
	}
	name_ = id.getName();
	const FileMode mode = id.getMode();
	if (mode == FILE_MODE_CREATE) {
		IO_STAT_CREATE(this);
		file_ = IO::open(name_, mode);
	} else {
		IO_STAT_OPEN(this);
		file_ = IO::open(name_, mode);
	}
	Atomic::inc64(&SparrowStatus::get().ioOpens_);
}

// Closes file.
void FileHandle::clear() {
	if (file_ != -1) {
		{
			IO_STAT_CLOSE(this);
			IO::close(file_);
		}
		file_ = -1;
		Atomic::inc64(&SparrowStatus::get().ioCloses_);
	}
}

// Seeks and reads from file (atomically).
// Useful when multiple threads are likely to read from a file.
uint32_t FileHandle::read(const uint64_t offset, uint8_t* data, const uint32_t size) const _THROW_(SparrowException) {
	IO_STAT_OTHER(this, PSI_FILE_READ, size);
	const uint32_t readBytes = IO::read(file_, name_, offset, data, size);
	IO_STAT_BYTES(readBytes);
	Atomic::add64(&SparrowStatus::get().ioReadBytes_, readBytes);
	Atomic::inc64(&SparrowStatus::get().ioReads_);
	return readBytes;
}

uint32_t FileHandle::readMultiple(const uint64_t offset, uint8_t** data, const uint32_t size) const _THROW_(SparrowException) {
	IO_STAT_OTHER(this, PSI_FILE_READ, size);
#ifndef _WIN32
	const uint32_t readBytes = IO::readMultiple(file_, lock_, name_, offset, data, size);
#else
	const uint32_t readBytes = IO::readMultiple(file_, 0, name_, offset, data, size);
#endif
	IO_STAT_BYTES(readBytes);
	Atomic::add64(&SparrowStatus::get().ioReadBytes_, readBytes);
	Atomic::inc64(&SparrowStatus::get().ioReads_);
	return readBytes;
}

// Writes to file.
uint32_t FileHandle::write(const uint64_t offset, uint8_t* data, const uint32_t size) const _THROW_(SparrowException) {
	IO_STAT_OTHER(this, PSI_FILE_WRITE, size);
	const uint32_t writtenBytes = IO::write(file_, name_, offset, data, size);
	IO_STAT_BYTES(writtenBytes);
	Atomic::inc64(&SparrowStatus::get().ioWrites_);

	// Trigger purge if necessary.
	const uint64_t count = Atomic::add64(&SparrowStatus::get().ioWrittenBytes_, writtenBytes);
	const uint64_t n = Purge::getSecurityMargin() / 2;
	if (count / n != (count - writtenBytes) / n) {
		Purge::wakeUp();
	}
	return writtenBytes;
}

// Get file size.
uint64_t FileHandle::getSize() const _THROW_(SparrowException) {
	if (file_ == -1) {
		return 0;
	} else {
		IO_STAT_OTHER(this, PSI_FILE_STAT, 0);
		return FileUtil::getFileSize(file_);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileOffset
//////////////////////////////////////////////////////////////////////////////////////////////////////

// STATIC
// Initializes a file block from a given file and offset.
void FileOffset::expandId(const FileOffset& id, const BlockCacheHint& hint, FileOffset*& ids, uint32_t& n) _THROW_(SparrowException) {
	// Create the key, and set its name.
	const uint32_t fileId = id.getFileId();
	FileId key(fileId == DATA_FILE ? FILE_TYPE_DATA : (fileId == STRING_FILE ? FILE_TYPE_STRING : FILE_TYPE_INDEX));
	id.getFileName(key.getName());

	// Get file from file cache.
	FileCacheGuard guard(FileCache::get(), 0, key, 0, false);
	const FileHandle& handle = guard.get()->getValue();

	// Get file size and compute start offset and read size.
	const uint64_t fileSize = handle.getSize();
	const uint64_t offset = id.getOffset();
	if (offset >= fileSize) {
		throw SparrowException::create(false, "Cannot read file %s at offset %llu: offset is greater than file size (%llu)", 
			key.getName(), static_cast<ulonglong>(offset), static_cast<ulonglong>(fileSize));
	}
	const uint32_t readBlockSize = hint.getReadBlockSize();
	uint32_t readSize = 0;
	uint64_t ioffset = 0;
	switch (hint.getDirection()) {
	case BlockCacheHint::FORWARD: {
		readSize = static_cast<uint32_t>(std::min(fileSize - offset, static_cast<uint64_t>(readBlockSize)));
		ioffset = offset;
		break;
								  }
	case BlockCacheHint::BACKWARD: {
		const uint64_t limit = std::min(fileSize, offset + static_cast<uint64_t>(sparrow_cache_block_size));
		readSize = static_cast<uint32_t>(std::min(limit, static_cast<uint64_t>(readBlockSize)));
		const uint32_t blocks = (readSize + sparrow_cache_block_size - 1) / sparrow_cache_block_size - 1;
		ioffset = offset - blocks * sparrow_cache_block_size;
		break;
								   }
	case BlockCacheHint::AROUND: {
		uint64_t half = readBlockSize / 2;
		const uint64_t modulo = half % sparrow_cache_block_size;
		if (modulo != 0) {
			half += half - modulo; 
		}
		ioffset = offset > half ? offset - half : 0;
		const uint64_t limit = std::min(fileSize, offset + half);
		readSize = static_cast<uint32_t>(limit - ioffset);
		break;
								 }
	default:
		assert(0);
	}

	// Get block entries for scattered read-ahead or read-backward.
	// Some blocks may be already initialized. Exclude current block.
	n = (readSize + sparrow_cache_block_size - 1) / sparrow_cache_block_size - 1;
	ids = static_cast<FileOffset*>(IOContext::getTempBuffer3(n * sizeof(FileOffset)));
	uint32_t j = 0;
	for (uint32_t i = 0; i <= n; ++i, ioffset += sparrow_cache_block_size) {
		if (ioffset != offset) {
			ids[j++] = FileOffset(id, ioffset);
		}
	}
	assert(j == n);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileBlock
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Initializes a file block from a given file and offset.
void FileBlock::initialize( const FileOffset& id, SYSpVector<CacheEntry<FileOffset, FileBlock, BlockCacheHint>, 64>& entries, 
	const BlockCacheHint& hint ) _THROW_(SparrowException)
{
	// Create the key, and set its name.
	const uint32_t fileId = id.getFileId();
	FileId key(fileId == DATA_FILE ? FILE_TYPE_DATA : (fileId == STRING_FILE ? FILE_TYPE_STRING : FILE_TYPE_INDEX));
	id.getFileName(key.getName());

	// Get file from file cache.
	FileCacheGuard guard(FileCache::get(), 0, key, 0, false);
	const FileHandle& handle = guard.get()->getValue();

	// Get file size and compute start offset and read size.
	const uint64_t fileSize = handle.getSize();
	const uint64_t offset = id.getOffset();

	uint32_t dataIndex = 0;
	for ( ; dataIndex<entries.entries(); ++dataIndex ) {
		if ( offset < entries[dataIndex]->getId().getOffset() )
			break;
	}

	// Exclude leading and trailing entries already filled. Take care of current block.
	int first = -1;
	int last = -1;
	for ( uint32_t j=0; j<entries.entries(); ++j ) {
		BlockCacheEntry* entry = entries[j];
		if (entry->getValue().getLength() == 0) {
			if (first == -1) {
				first = j;
			}
			last = j;
		}
	}
	if (last >= 0) {
		// Some entries are not in the cache: read multiple blocks at once.
		uint32_t skip = 0;
		uint32_t blocks = 0;
		switch (hint.getDirection()) {
		case BlockCacheHint::FORWARD:
			blocks = 2 + last;
			break;
		case BlockCacheHint::BACKWARD:
			blocks = 1 + entries.entries() - first;
			skip = first;
			break;
		case BlockCacheHint::AROUND:
			if (dataIndex >= static_cast<uint32_t>(first)) {
				if (dataIndex > static_cast<uint32_t>(last)) {
					blocks = dataIndex - first + 1;
				} else {
					blocks = last - first + 2;
				}
				skip = first;
			} else {
				blocks = last - dataIndex + 2;
				skip = dataIndex;
			}
			break;
		default:
			assert(0);
		}

		// Skip leading filled entries by iterating on the list.
		uint32_t	j = skip;
		dataIndex -= skip;
		uint8_t** data = static_cast<uint8_t**>(IOContext::getTempBuffer3(blocks * sizeof(uint8_t*)));
		uint64_t startOffset = hint.getDirection() == BlockCacheHint::FORWARD ? offset : 0;
		for (uint32_t i = 0; i < blocks; ++i) {
			if (i == dataIndex) {
				data[i] = data_;
				if (i == 0) {
					startOffset = offset;
				}
			} else {
				BlockCacheEntry* entry = entries[j];
				const FileBlock& block = entry->getValue();
				if (block.getLength() == 0) {
					data[i] = block.getData();
					if (i == 0) {
						startOffset = entry->getId().getOffset();
					}
				} else {
					assert(i != 0);
					data[i] = 0;
				}
				++j;
			}
		}
		uint32_t readBytes = handle.readMultiple(startOffset, data, blocks * sparrow_cache_block_size);
		if (readBytes < FileUtil::getSectorSize()) {
			throw SparrowException::create(false, "Cannot read at most %u bytes from file %s at offset %llu; expected at least %u bytes and got %u bytes",
				blocks * sparrow_cache_block_size, key.getName(), static_cast<ulonglong>(startOffset), FileUtil::getSectorSize(), readBytes);
		}

		// Complete initialization of cache entries.
		length_ = static_cast<uint32_t>(std::min(fileSize - offset, static_cast<uint64_t>(sparrow_cache_block_size)));
		readBytes -= length_;
		for (uint32_t j=skip; j<entries.entries(); ++j) {
			BlockCacheEntry* entry = entries[j];
			FileBlock& block = entry->getValue();
			if (block.getLength() == 0) {
				block.length_ = std::min(readBytes, sparrow_cache_block_size);
				entry->setValid(true);
				readBytes -= block.length_;
			} else {
				readBytes -= sparrow_cache_block_size;
			}
		}
	} else {
		// All other entries, if any, are already in the cache: read just one block at given offset.
		length_ = handle.read(offset, data_, sparrow_cache_block_size);
	}
	assert(length_ > 0);
}


void FileBlock::replace(const FileBlock& value) {
	length_ = value.length_;
	memcpy(data_, value.data_, length_);
}


}

