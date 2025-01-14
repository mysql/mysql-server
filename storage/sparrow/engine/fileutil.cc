/*
	File utilities.
*/

#include "fileutil.h"
#include "cache.h"
#include "persistent.h"
#include "internalapi.h"
#include "purge.h"

#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../engine/log.h"
#include "sql/mysqld.h"


#ifdef _WIN32
#include "winioctl.h"
#endif

#ifndef _WIN32
#include <unistd.h>
#include <sys/statvfs.h>
#endif

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileUtil
//////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
const char FileUtil::separator_ = '\\';
#else
const char FileUtil::separator_ = '/';
#endif
uint32_t FileUtil::sectorSize_;
uint32_t FileUtil::pageSize_;
Filesystems FileUtil::filesystems_;
FilesystemIds FileUtil::filesystemIds_;
Filesystems FileUtil::coalescingFilesystems_;
FilesystemIds FileUtil::coalescingFilesystemIds_;
Filesystems FileUtil::allFilesystems_;
Lock FileUtil::lock_(true, "FileUtil::lock_");
struct rand_struct FileUtil::rnd_;

// Miscellaneous initializations.
void FileUtil::initialize() _THROW_(SparrowException) {
#ifdef _WIN32
	// Gets the sector size of the MySQL data directory.
	DWORD sectorsPerCluster, bytesPerSector, freeClusters, totalClusters;
	if (GetDiskFreeSpace(mysql_real_data_home, &sectorsPerCluster, &bytesPerSector,
		&freeClusters, &totalClusters) != 0) {
		sectorSize_ = static_cast<uint32_t>(bytesPerSector);
	}

	// Try to get the physical sector size in case the drive emulates a 512bytes sector size for compatibility with legacy hardware
	if (strlen(mysql_real_data_home) > 2 && mysql_real_data_home[1] == ':') {
		char	drive[16];
		sprintf(drive, "\\\\.\\%c:", mysql_real_data_home[0]);

		STORAGE_PROPERTY_QUERY Query;
		STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR Alignment = {0};
		ZeroMemory(&Query, sizeof(Query));
		HANDLE  hFile = CreateFileA( drive, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile==INVALID_HANDLE_VALUE) {
			spw_print_information("Could not get physical sector size of data drive %s (error %lu opening drive). Using logical sector size, %uB.",
				drive, GetLastError(), sectorSize_);
		} else {
			Query.QueryType  = PropertyStandardQuery;
			Query.PropertyId = StorageAccessAlignmentProperty;

			DWORD		Bytes = 0;
			BOOL	res = DeviceIoControl( hFile, IOCTL_STORAGE_QUERY_PROPERTY, &Query, sizeof(STORAGE_PROPERTY_QUERY), &Alignment,
				sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR), &Bytes, NULL);
			if (res == FALSE) {
				spw_print_information("Could not get physical sector size of data drive %s (error %lu). Using logical sector size, %uB.",
					drive, GetLastError(), sectorSize_);
			} else {
				spw_print_information("Data drive %s has a physical sector size of %uB and a logical sector size of %uB. Using physical sector size for unbuffered IOs.",
					drive, sectorSize_, Alignment.BytesPerPhysicalSector);
				sectorSize_ = Alignment.BytesPerPhysicalSector;
			}
			CloseHandle(hFile);
		}
	} else {
		spw_print_information("Using logical sector size of %u for unbuffered IOs.", sectorSize_);
	}

	// Gets the system page size.
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	pageSize_ = systemInfo.dwPageSize;
#else
	// On Unix, retrieve the sector size from the system variables.
	sectorSize_ = sparrow_disk_sector_size;

	// Gets the system page size.
	pageSize_ = getpagesize();
#endif

	// List file systems.
	const time_t start_time = time(0);
	randominit(&rnd_, static_cast<ulong>(start_time), static_cast<ulong>(start_time) / 2);
	filesystems_.append(new Filesystem(mysql_real_data_home));
	if (sparrow_filesystems != 0) {
		Str copy(sparrow_filesystems);
		char* tmp;
		char* token = my_strtok_r(const_cast<char*>(copy.c_str()), ",", &tmp);
		while (token != 0) {
			if (doesFileExist(token)) {
				filesystems_.append(new Filesystem(token));
			} else {
				spw_print_error("Sparrow will not use file system %s because it does not exist; please check variable sparrow_filesystems", token);
			}
			token = my_strtok_r(0, ",", &tmp);
		}
	}
	allFilesystems_ = filesystems_;
	if (sparrow_coalescing_filesystems != 0) {
		Str copy(sparrow_coalescing_filesystems);
		char* tmp;
		char* token = my_strtok_r(const_cast<char*>(copy.c_str()), ",", &tmp);
		while (token != 0) {
			if (doesFileExist(token)) {
				Filesystem* fs = new Filesystem(token);
				coalescingFilesystems_.append(fs);
				allFilesystems_.append(fs);
			} else {
				spw_print_error("Sparrow will not use coalescing file system %s because it does not exist; please check variable sparrow_coalescing_filesystems", token);
			}
			token = my_strtok_r(0, ",", &tmp);
		}
	}
	getFreeDiskSpace();
}

// Gets the parent directory of the given path name.
// Returns length of parent directory or 0 if no parent.
// STATIC
const char* FileUtil::getParent(const char* path, char* buffer) {
	int l = static_cast<int>(strlen(path));
	while (l >= 0 && path[l] == separator_) {
		l--;
	}
	while (l >= 0 && path[l] != separator_) {
		l--;
	}
	if (l <= 0) {
		return 0;
	}
	strncpy(buffer, path, l);
	buffer[l] = 0;
	return buffer;
}

// Recursively creates all required directories (if they do not exist) for the given file name.
// STATIC
void FileUtil::createDirectories(const char* path) {
	// Try to create parent directory.
	char parent[FN_REFLEN];
	if (getParent(path, parent) == 0) {
		return;
	}

	// Check if directory already exists.
	if (doesFileExist(parent)) {
		return;
	}

	// Recurse.
	createDirectories(parent);
	my_mkdir(parent, 0777, MYF(0));
}

// Recursively deletes all files and directories under the given directory.
// STATIC
void FileUtil::deleteDirectory(const char* path) {
	MY_DIR* dir = my_dir(path, MYF(MY_DONT_SORT|MY_WANT_STAT));
	if (dir != 0) {
		for (uint i = 0; i < dir->number_off_files; ++i) {
			const fileinfo& file = dir->dir_entry[i];
			const char* filename = file.name;
			// Ignore "." and "..".
			if (filename[0] == '.'
				&& (filename[1] == 0 || (filename[1] == '.' && filename[2] == 0))) {
				continue;
			}
			char fullFilename[FN_REFLEN];
			snprintf(fullFilename, sizeof(fullFilename), "%s%c%s", path, FileUtil::separator_, filename);
			if (MY_S_ISDIR(file.mystat->st_mode)) {
				deleteDirectory(fullFilename);
			} else {
				//spw_print_information("Deleting file %s",fullFilename);
				int		err = my_delete(fullFilename, MYF(0));
				if ( err != 0 && my_errno() != ENOENT ) {
					char	errMsg[MYSYS_STRERROR_SIZE];
					my_strerror(errMsg, sizeof(errMsg), my_errno());
					spw_print_information("Failed to delete %s: error code %d (%s)",fullFilename, my_errno(), errMsg);
				}
			}
		}
		my_dirend(dir);
		rmdir(path);
	}
}

// Recursively scans a directory and returns files or directories matching the given extension.
// The level is 1 to scan only files or directory directly under the directory.
// If parameter forFiles is true, only files are returned. Otherwise only directories are returned.
// STATIC
void FileUtil::scanDirectory(const char* path, const char* extension,
	const uint32_t level, Files& files, const bool forFiles) _THROW_(SparrowException) {
	const size_t extLength = strlen(extension);
#ifdef _WIN32
	Str pathx(path);
	Str simplePath;
	int pos = pathx.length() - 1;
	if (pathx.c_str()[pos] == '\\') {
		simplePath = Str(pathx.c_str(), pos);
		pathx += Str("*");
	} else {
		simplePath = pathx;
		pathx += Str("\\*");
	}
	path = pathx.c_str();
	WIN32_FIND_DATA data;
	DirGuard guard(path, &data);
	for (;;) {
		const char* name = data.cFileName;
		if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
			char fullName[8192];
			snprintf(fullName, sizeof(fullName), "%s\\%s", simplePath.c_str(), name);
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				if (!forFiles) {
					const size_t length = strlen(name);
					if (length > extLength && strcmp(name + length - extLength, extension) == 0) {
						files.append(Str(fullName));
					}
				}
				if (level > 1) {
					scanDirectory(fullName, extension, level - 1, files, forFiles);
				}
			} else if (forFiles) {
				const size_t length = strlen(name);
				if (length > extLength && strcmp(name + length - extLength, extension) == 0) {
					files.append(Str(fullName));
				}
			}
		}
		if (FindNextFile(guard.get(), &data) == 0) {
			if (GetLastError() == ERROR_NO_MORE_FILES) {
				break;
			}
			throw SparrowException::create(true, "Cannot scan directory %s", path);
		}
	}
#else
	Str pathx(path);
	int pos = pathx.length() - 1;
	if (pathx.c_str()[pos] == '/') {
		pathx = Str(pathx.c_str(), pos);
	}
	path = pathx.c_str();
	DirGuard guard(path);
	DIR* dir = guard.get();
	//char direntBuf[sizeof(struct dirent) + _POSIX_PATH_MAX + 100];
	struct dirent* entry = 0;
	for (;;) {
		errno = 0;
		entry = readdir(dir);
		///int result = readdir_r(dir, reinterpret_cast<struct dirent*>(direntBuf), &entry);
		// if (result != 0) {
		// 	throw SparrowException::create(true, "Cannot read directory %s", path);
		// }
		if (entry == 0) {
			if (errno != 0) {
				throw SparrowException::create(true, "Cannot read directory %s", path);
			}
			break;
		}
		const char* name = entry->d_name;
		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
			continue;
		}
		char fullName[8192];
		snprintf(fullName, sizeof(fullName), "%s/%s", path, name);
		MY_STAT stat;
		if (my_stat(fullName, &stat, MYF(0)) == 0) {
			throw SparrowException::create(true, "Cannot get stats of %s", fullName);
		}
		if (S_ISDIR(stat.st_mode)) {
			if (!forFiles) {
				const size_t length = strlen(name);
				if (length > extLength && strcmp(name + length - extLength, extension) == 0) {
					files.append(Str(fullName));
				}
			}
			if (level > 1) {
				scanDirectory(fullName, extension, level - 1, files, forFiles);
			}
		} else if (forFiles) {
			const size_t length = strlen(name);
			if (length > extLength && strcmp(name + length - extLength, extension) == 0) {
				files.append(Str(fullName));
			}
		}
	}
#endif
}


// Recursively scans a directory and returns files or directories matching the given extension.
// The level is 1 to scan only files or directory directly under the directory.
// If parameter forFiles is true, only files are returned. Otherwise only directories are returned.
// STATIC
void FileUtil::dbg_dump_content(const char* path) _THROW_(SparrowException) {

	spw_print_information("[DBG] dumping content of %s",path);
#ifdef _WIN32
	Str pathx(path);
	Str simplePath;
	int pos = pathx.length() - 1;
	if (pathx.c_str()[pos] == '\\') {
		simplePath = Str(pathx.c_str(), pos);
		pathx += Str("*");
	} else {
		simplePath = pathx;
		pathx += Str("\\*");
	}
	path = pathx.c_str();
	WIN32_FIND_DATA data;
	DirGuard guard(path, &data);
	for (;;) {
		const char* name = data.cFileName;
		if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
			char fullName[8192];
			snprintf(fullName, sizeof(fullName), "%s\\%s", simplePath.c_str(), name);
			spw_print_information("[DBG] %s",fullName);
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				dbg_dump_content(fullName);
			}
		}
		if (FindNextFile(guard.get(), &data) == 0) {
			if (GetLastError() == ERROR_NO_MORE_FILES) {
				break;
			}
			throw SparrowException::create(true, "Cannot scan directory %s", path);
		}
	}
#else
	Str pathx(path);
	int pos = pathx.length() - 1;
	if (pathx.c_str()[pos] == '/') {
		pathx = Str(pathx.c_str(), pos);
	}
	path = pathx.c_str();
	DirGuard guard(path);
	DIR* dir = guard.get();
	//char direntBuf[sizeof(struct dirent) + _POSIX_PATH_MAX + 100];
	struct dirent* entry = 0;
	for (;;) {
		errno = 0;
		entry = readdir(dir);
		// int result = readdir_r(dir, reinterpret_cast<struct dirent*>(direntBuf), &entry);
		// if (result != 0) {
		// 	throw SparrowException::create(true, "Cannot read directory %s", path);
		// }
		if (entry == 0) {
			if (errno != 0) {
				throw SparrowException::create(true, "Cannot read directory %s", path);
			}
			break;
		}
		const char* name = entry->d_name;
		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
			continue;
		}
		char fullName[8192];
		snprintf(fullName, sizeof(fullName), "%s/%s", path, name);
		MY_STAT stat;
		if (my_stat(fullName, &stat, MYF(0)) == 0) {
			throw SparrowException::create(true, "Cannot get stats of %s", fullName);
		}
		spw_print_information("[DBG] %s, rights %u, uid %u, gid %u, size %ld",fullName, 
			stat.st_mode, stat.st_uid, stat.st_gid, stat.st_size);
		if (S_ISDIR(stat.st_mode)) {
			dbg_dump_content(fullName);
		} 
	}
#endif
}


// Checks if a file/directory exists.
// STATIC
bool FileUtil::doesFileExist(const char* path) {
	MY_STAT stat;
	return (my_stat(path, &stat, MYF(0)) != 0);
}

// Gets the size of a file.
// STATIC
uint64_t FileUtil::getFileSize(File file) _THROW_(SparrowException) {
#ifdef _WIN32
	LARGE_INTEGER size;
	if (file != -1 && GetFileSizeEx(my_get_osfhandle(file), &size)) {
		return static_cast<uint64_t>(size.QuadPart);
	}
#else
	struct stat s;
	if (file != -1 && fstat(file, &s) == 0) {
		return static_cast<uint64_t>(s.st_size);
	}
#endif
	throw SparrowException::create(true, "Cannot get file size");
}

// STATIC
void FileUtil::rename(const char* from, const char* to) _THROW_(SparrowException) {
#ifdef _WIN32
	do {
		if (MoveFileEx(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)) 
			break;
		if (GetLastError() == ERROR_SHARING_VIOLATION) {
			my_sleep(100000);		// 100ms
		} else {
			throw SparrowException::create(true, "Cannot rename file or directory \"%s\" to \"%s\"", from, to);
		}
	} while (true);
#else
	do {
		if (::rename(from, to) == 0) 
			break;
		if (errno == EBUSY) {
			my_sleep(100000);		// 100ms
		} else {
			throw SparrowException::create(true, "Cannot rename file or directory \"%s\" to \"%s\"", from, to);
		}
	} while (true);
#endif	
}

// STATIC
Str FileUtil::getDatabaseName(const char* path) {
	const int len = path == 0 ? 0 : static_cast<int>(strlen(path));
	if (len == 0) {
		return Str();
	}
	const char* t = path + len - 1;
	while (t != path && *t != '/' && *t != '\\') {
		t--;
	}
	if (*t != '/' && *t != '\\') {
		return Str();
	}
	const char* saved = t;
	t--;
	while (t != path && *t != '/' && *t != '\\') {
		t--;
	}
	if (*t != '/' && *t != '\\') {
		return Str();
	}
	t++;
	return Str(t, static_cast<int>(saved - t));
}

// STATIC
Str FileUtil::getTableName(const char* path) {
	const int len = path == 0 ? 0 : static_cast<int>(strlen(path));
	if (len == 0) {
		return Str();
	}
	const char* dot = 0;
	const char* t = path + len - 1;
	while (t != path && *t != '/' && *t != '\\') {
		if (*t == '.') {
			dot = t;
		}
		t--;
	}
	if (*t != '/' && *t != '\\') {
		return Str();
	}
	t++;
	if (dot == 0) {
		return Str(t);
	} else {
		return Str(t, static_cast<int>(dot - t));
	}
}

// Gets the free disk space.
// STATIC
uint64_t FileUtil::getFreeDiskSpace() {
	SPARROW_ENTER("FileUtil::getFreeDiskSpace");
	const uint64_t free = updateStats(filesystems_, filesystemIds_, false);
	const uint64_t coalescingFree = updateStats(coalescingFilesystems_, coalescingFilesystemIds_, true);
	Atomic::set64(&SparrowStatus::get().freeDiskSpace_, free + coalescingFree);
	return free;
}

// STATIC
uint64_t FileUtil::updateStats(Filesystems& filesystems, FilesystemIds& filesystemIds, const bool coalescing) {
	uint64_t total = 0;
	uint64_t minFree = ULLONG_MAX;
	uint64_t maxFree = 0;
	const uint32_t n = filesystems.length();
	const uint64_t margin = Purge::getSecurityMargin();
	for (uint32_t i = 0; i < n; ++i) {
		const uint64_t f = filesystems[i]->computeStats();
		total += f;
		const uint64_t af = f > margin ? f - margin : 0;
		if (af != 0) {
			minFree = std::min(minFree, af);
			maxFree = std::max(maxFree, af);
		}
	}

	double unit = minFree;
	if  ( (maxFree/minFree) > 100 ) {
		unit = maxFree/100.0;
		//spw_print_information("Updating FS stats. Free space: min %llu (%lluMB), max %llu (%lluMB), total %llu (%lluMB). Limiting computing unit to %f",
		//	static_cast<ulonglong>(minFree), static_cast<ulonglong>(minFree/(1024*1024)), static_cast<ulonglong>(maxFree), static_cast<ulonglong>(maxFree/(1024*1024)),
		//	static_cast<ulonglong>(total), static_cast<ulonglong>(total/(1024*1024)), unit );
	}

	// Prepare array for weighted round robin.
	const uint32_t factor = 5;
	FilesystemIds tmp(minFree == ULLONG_MAX ? n : static_cast<uint32_t>((factor * total) / unit));
	for (uint32_t i = 0; i < n; ++i) {
		if (minFree != ULLONG_MAX) {
			const Filesystem& fs = *filesystems[i];
			const uint64_t f = fs.getFree();
			const uint64_t af = f > margin ? f - margin : 0;
			if (af != 0) {
				const uint32_t count = static_cast<uint32_t>((factor * af) / unit);
				for (uint32_t j = 0; j < count; ++j) {
					tmp.append(i);
				}
			}
			// We want an empty result if there is no space left on coalescing file systems.
		} else if (!coalescing) {
			tmp.append(i);
		}
	}
	FilesystemIds ids(tmp.capacity());
	while (!tmp.isEmpty()) {
		const uint32_t i = static_cast<uint32_t>(my_rnd(&rnd_) * tmp.length());
		ids.append(tmp[i]);
		tmp.removeAt(i);
	}
	{
		Guard guard(lock_);
		filesystemIds = ids;
	}
	return total;
}

void FileUtil::getDiskStats(uint64_t& totalFree, uint64_t& totalUsed, uint64_t& totalSize)
{
	SPARROW_ENTER("FileUtil::getDiskStats");
	totalSize = 0;
	totalUsed = 0;
	totalFree = 0;
	const uint32_t n = filesystems_.length();
	const uint32_t na = allFilesystems_.length();
	for (uint32_t i = 0; i < na; ++i)
	{
		const bool coalescing = i >= n;
		const Filesystem& fs = coalescing ? *coalescingFilesystems_[i - n] : *filesystems_[i];	
		const uint64_t u = fs.getUsed();
		totalUsed += u;
		const uint64_t s = fs.getSize();
		totalSize += s;
		const uint64_t f = fs.getFree();
		totalFree += f;
	}
}

// Chooses the file system to write to.
// Use a weighted round robin, where weights are computed using file system free space.
// STATIC
uint32_t FileUtil::chooseFilesystem(const bool coalescing) {
	SPARROW_ENTER("FileUtil::chooseFilesystem");
	Guard guard(lock_);
	if (coalescing && !coalescingFilesystemIds_.isEmpty()) {
		static uint32_t coalescingId = 0;
		return COALESCING_FILESYSTEM + coalescingFilesystemIds_[coalescingId++ % coalescingFilesystemIds_.length()];
	} else {
		static uint32_t id = 0;
		return filesystemIds_[id++ % filesystemIds_.length()];
	}
}

// STATIC
const char* FileUtil::getFilesystemPath(const uint32_t filesystem) {
	if (filesystem >= COALESCING_FILESYSTEM) {
		const uint32_t length = coalescingFilesystems_.length();
		const uint32_t i = filesystem - COALESCING_FILESYSTEM;
		return length == 0 ? filesystems_[0]->getPath().c_str() : coalescingFilesystems_[i >= length ? length - 1 : i]->getPath().c_str();
	} else {
		const uint32_t length = filesystems_.length();
		return filesystems_[filesystem >= length ? length - 1 : filesystem]->getPath().c_str();
	}
}

// Report status of file systems.
// STATIC
void FileUtil::report(PrintBuffer& buffer) _THROW_(SparrowException) {
	SPARROW_ENTER("FileUtil::report");
	buffer << "\nFilesystems:\n\n";
	const char* h[] = { "Path", "Size", "Used", "Free" };
	SYSvector<Str> headers(sizeof(h) / sizeof(h[0]));
	for (uint32_t i = 0; i < headers.capacity(); ++i) {
		headers.append(Str(h[i]));
	}
	char tmp[1024];
	uint64_t totalSize = 0;
	uint64_t totalUsed = 0;
	uint64_t totalFree = 0;
	SYSslist<Str> strings;
	const uint32_t n = filesystems_.length();
	const uint32_t na = allFilesystems_.length();
	for (uint32_t i = 0; i < na; ++i) {
		const bool coalescing = i >= n;
		const Filesystem& fs = coalescing ? *coalescingFilesystems_[i - n] : *filesystems_[i];
		Str name(fs.getPath());
		if (coalescing) {
			name += Str(" (coalescing)");
		}
		strings.append(name);
		const uint64_t s = fs.getSize();
		strings.append(Str::fromSize(s));
		totalSize += s;
		const uint64_t u = fs.getUsed();
		snprintf(tmp, sizeof(tmp), "%s (%.1f%%)", Str::fromSize(u).c_str(), (100.0 * u) / s);
		strings.append(Str(tmp));
		totalUsed += u;
		const uint64_t f = fs.getFree();
		snprintf(tmp, sizeof(tmp), "%s (%.1f%%)", Str::fromSize(f).c_str(), (100.0 * f) / s);
		strings.append(Str(tmp));
		totalFree += f;
	}
	strings.append(Str("TOTAL"));
	strings.append(Str::fromSize(totalSize));
	snprintf(tmp, sizeof(tmp), "%s (%.1f%%)", Str::fromSize(totalUsed).c_str(), (100.0 * totalUsed) / totalSize);
	strings.append(Str(tmp));
	snprintf(tmp, sizeof(tmp), "%s (%.1f%%)", Str::fromSize(totalFree).c_str(), (100.0 * totalFree) / totalSize);
	strings.append(Str(tmp));
	InternalApi::printGrid(buffer, headers, strings, 4);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Filesystem
//////////////////////////////////////////////////////////////////////////////////////////////////////

uint64_t Filesystem::computeStats() {
	SPARROW_ENTER("Filesystem::computeStats");
	uint64_t s = 0;
	uint64_t u = 0;
	uint64_t f = 0;
#ifdef _WIN32
	DWORD sectorsPerCluster, bytesPerSector, freeClusters, totalClusters;
	if (GetDiskFreeSpace(getPath().c_str(), &sectorsPerCluster, &bytesPerSector,
		&freeClusters, &totalClusters) != 0) {
		s = static_cast<uint64_t>(totalClusters) * sectorsPerCluster * bytesPerSector;
		f = static_cast<uint64_t>(freeClusters) * sectorsPerCluster * bytesPerSector;
	}
#elif defined(__MACH__) 
	struct statvfs vfs;
	if (statvfs(getPath().c_str(), &vfs) == 0) {
		s = static_cast<uint64_t>(vfs.f_frsize) * vfs.f_blocks;
		f = static_cast<uint64_t>(vfs.f_frsize) * vfs.f_bavail;
	}
#else
	struct statvfs64 vfs;
	if (statvfs64(getPath().c_str(), &vfs) == 0) {
		s = static_cast<uint64_t>(vfs.f_frsize) * vfs.f_blocks;
		f = static_cast<uint64_t>(vfs.f_frsize) * vfs.f_bavail;
	}
#endif
	u = s > f ? s - f : 0;
	size_ = s;
	used_ = u;
	Atomic::set64(&free_, f);
#ifndef NDEBUG
	const Str ssize(Str::fromSize(s));
	const Str sused(Str::fromSize(u));
	const Str sfree(Str::fromSize(f));
	DBUG_PRINT("sparrow_purge", ("File system %s: %s size, %s used, %s free", getPath().c_str(), ssize.c_str(), sused.c_str(), sfree.c_str()));
#endif
	return f;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ReferencedBlocks
//////////////////////////////////////////////////////////////////////////////////////////////////////

volatile uint32_t ReferencedBlocks::lockedBlocks_ = 0;
volatile uint32_t ReferencedBlocks::maxLockedBlocks_ = 0;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileHeader
//////////////////////////////////////////////////////////////////////////////////////////////////////

// File format history:
// 1	Initial format: header, tree, binary, records.
// 2	Format suitable for coalescing: records, binary, tree, header.
// 3	Extend header with tree information.
const uint8_t FileHeader::currentFileFormat_ = 3;

// Header sizes for each format.
const uint32_t FileHeader::sizes_[] = { 0, 57, 89, FileHeader::computeSize() };

// STATIC
uint32_t FileHeader::computeSize() {
	uint8_t bytes[1024];
	ByteBuffer buffer(bytes, sizeof(bytes));
	buffer << FileHeader();
	return static_cast<uint32_t>(buffer.position());
}

FileHeader::FileHeader() {
}

FileHeader::FileHeader(const uint64_t binSize, const uint32_t treeSize, const bool treeComplete, const uint32_t nodeSize, const uint32_t recordSize,
	const uint32_t records, const uint32_t index, const uint64_t start, const uint64_t end) 
	: format_(FileHeader::currentFileFormat_), recordSize_(recordSize), records_(records),
	treeComplete_(treeComplete), nodeSize_(0), nodes_(0), index_(index), start_(start), end_(end), treeOrder_(0) {
	// Here is the file layout:
	// +--------+---------+------+-------------+---------+--------+
	// | Format | Records | Tree | Binary Data | Padding | Header |
	// +--------+---------+------+-------------+---------+--------+
	// Note padding is necessary because the whole file must have a size adjusted to the sector size,
	// and the header must be at the end of the file.
	uint64_t offset = 4;	// Format is coded on 4 bytes.
	recordsSection_ = FileSection(offset, static_cast<uint64_t>(records) * recordSize);
	offset += recordsSection_.getSize();
	treeSection_ = FileSection(offset, treeSize);
	offset += treeSize;
	binSection_ = FileSection(offset, binSize);
	offset += binSize;
	const uint64_t totalSize = offset + FileHeader::size();
	totalSize_ = FileUtil::adjustSizeToSectorSize(totalSize);	// Adjust total size on sector size.
	genTime_ = static_cast<uint32_t>(std::time(nullptr));
	initialize(nodeSize);
}

void FileHeader::initialize(const uint32_t nodeSize /* = 0 */) {
	if (index_ != DATA_FILE) {
		if (nodeSize > 0) {
			nodeSize_ = nodeSize;
			nodes_ = static_cast<uint32_t>(treeSection_.getSize() / nodeSize_);
			assert(treeSection_.getSize() % nodeSize_ == 0);
		}
		treeOrder_ = &TreeOrder::get(nodes_);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DataFileHeader
//////////////////////////////////////////////////////////////////////////////////////////////////////

const uint32_t DataFileHeader::size_ = DataFileHeader::computeSize();

// STATIC
uint32_t DataFileHeader::computeSize() {
	uint8_t bytes[1024];
	ByteBuffer buffer(bytes, sizeof(bytes));
	buffer << DataFileHeader();
	return static_cast<uint32_t>(buffer.position());
}

DataFileHeader::DataFileHeader() {
}

DataFileHeader::DataFileHeader(const uint32_t recordSize, const uint64_t records, const uint64_t stringOffset, const uint64_t stringSize, const uint64_t start, const uint64_t end) 
	: genTime_(static_cast<uint32_t>(std::time(nullptr))), recordsSection_(size_, records * recordSize),
	recordSize_(recordSize), records_(records), stringsSection_(stringOffset, stringSize), start_(start), end_(end) {
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// IndexFileHeader
//////////////////////////////////////////////////////////////////////////////////////////////////////

const uint32_t IndexFileHeader::size_ = IndexFileHeader::computeSize();

// STATIC
uint32_t IndexFileHeader::computeSize() {
	uint8_t bytes[1024];
	ByteBuffer buffer(bytes, sizeof(bytes));
	buffer << IndexFileHeader();
	return static_cast<uint32_t>(buffer.position());
}

IndexFileHeader::IndexFileHeader() {
}

IndexFileHeader::IndexFileHeader(const uint32_t index, const uint32_t recordSize, const uint64_t records,
	const uint32_t nodeSize, const uint64_t nodes, const uint64_t start, const uint64_t end) 
	: genTime_(static_cast<uint32_t>(std::time(nullptr))), index_(index), recordsSection_(size_, records * recordSize),
	recordSize_(recordSize), records_(records), treeSection_(size_ + records * recordSize, nodes * nodeSize),
	nodeSize_(nodeSize), nodes_(nodes), start_(start), end_(end) {
	treeOrder_ = &TreeOrder::get(static_cast<uint32_t>(nodes_));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PartitionReader
//////////////////////////////////////////////////////////////////////////////////////////////////////

PartitionReader::PartitionReader(const PersistentPartition& partition, const uint32_t fileId, const BlockCacheHint& hint) _THROW_(SparrowException)
	: FileReader(partition, fileId, this), hint_(hint), columnAlterSerial_(partition.getColumnAlterSerial()) {
	header_ = partition.readHeader(fileId, *this);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PartitionReaders
//////////////////////////////////////////////////////////////////////////////////////////////////////

PartitionReader* PartitionReaders::get(const uint32_t n, PersistentPartition& partition, const uint32_t index, const bool isString, const BlockCacheHint& hint) _THROW_(SparrowException) {
	assert(index != STRING_FILE);
	const uint32_t level = hint.getLevel();
	assert(level == 1 || level == 2);
	const uint32_t i = n * 8 + (index == DATA_FILE ? 0 : 4) + (isString ? 0 : 2) + (level - 1);
	PartitionReader* reader = PartitionReadersBase::operator[](i);
	const uint32_t fileId = partition.getFileId(index, isString);
	if (reader == 0 || reader->getFileId() != fileId || reader->getBlockHint() != hint) {
		delete reader;
		PartitionReadersBase::operator[](i) = 0;
		reader = partition.createReader(index, isString, hint);
		PartitionReadersBase::operator[](i) = reader;
	}
	return reader;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileWriter
//////////////////////////////////////////////////////////////////////////////////////////////////////

// STATIC
const char* FileWriter::SUFFIX  = ".tmp";

FileWriter::~FileWriter() {
	// Releases and closes the file (clear flag is true).
	IO::flush(entry_->getValue().getFile(), filename_);
	const FileMode mode = getMode();
	FileCache::get().release(entry_, 0, true, true);
	if (mode == FILE_MODE_CREATE) {
		if (failed_) {
			// Drop temporary file.
			my_delete(filename_, MYF(0));
		} else {
			// Rename the new file.
			char newname[FN_REFLEN];
			strcpy(newname, filename_);
			newname[strlen(newname) - strlen(SUFFIX)] = 0;
			try {
				FileUtil::rename(filename_, newname);
			} catch(const SparrowException& e) {
				e.toLog();
			}
		}
	}
}

// Absolute seek within the file.
// The parameter length is the number of bytes the caller expects to modify after offset.
void FileWriter::seek(const uint64_t offset, const uint64_t length) _THROW_(SparrowException) {
	try {
		if (start_ != ULLONG_MAX && offset >= start_ && offset < start_ + limit()) {
			position(offset - start_);
		} else {
			const uint64_t newStart = offset - (offset % sparrow_cache_block_size);
			const uint64_t newLength = offset + length - newStart;
			const uint64_t remaining = newStart <= size_ ? size_ - newStart : 0;
			const uint64_t size = std::min(limit(), std::min(newLength, remaining));
			if (size > 0) {
				uint8_t* data = getData();
				if (cacheHint_ == 0) {
					entry_->getValue().read(newStart, data, static_cast<uint32_t>(FileUtil::adjustSizeToSectorSize(size)));
				} else {
					// Caching is enabled: get data from the cache.
					uint64_t end = newStart + size;
					const uint32_t modulo = end % sparrow_cache_block_size;
					if (modulo != 0) {
						end += sparrow_cache_block_size - modulo;
					}
					const uint32_t level = cacheHint_->getLevel();
					for (uint64_t ioffset = newStart; ioffset < end; ioffset += sparrow_cache_block_size) {
						BlockCacheEntry* entry = BlockCache::get().acquire(level,
							FileOffset(cacheHint_->getPartitionFile(), ioffset), BlockCacheHint::smallForward3_, true, false);
						assert(entry->isValid());
						FileBlock& block = entry->getValue();
						memcpy(data + ioffset - newStart, block.getData(), block.getLength());
						BlockCache::get().release(entry, entry->getLevel(), false, false);
					}
				}
			}
			start_ = newStart;
			position(offset - newStart);
		}
		mark_ = position();
		length_ = length;
	} catch(const SparrowException& e) {
		failed_ = true;
		throw;
	}
}

// Write buffered data to the file.
void FileWriter::write() _THROW_(SparrowException) {
	try {
		assert(position() >= mark_);
		if (mark_ == position()) {
			return;
		}
		uint64_t start = FileUtil::adjustPosToSectorSize(start_ + mark_);
		uint64_t offset = start - start_;
		uint64_t toWrite = position() - offset;
		if (start + toWrite < size_) {
			toWrite = FileUtil::adjustSizeToSectorSize(toWrite);
		}
		const uint32_t written = entry_->getValue().write(start, getData() + offset, static_cast<uint32_t>(toWrite));
		size_ = std::max(start + written, size_);
		if (cacheHint_ != 0) {
			// Caching is enabled: update cache with written data.
			start -= start % sparrow_cache_block_size;
			uint64_t end = start_ + position();
			const uint32_t modulo = end % sparrow_cache_block_size;
			if (modulo != 0) {
				end += sparrow_cache_block_size - modulo;
			}
			BlockCacheEntriesGuard cacheEntriesGuard;
			BlockCacheEntries& cacheEntries = cacheEntriesGuard.get();
			const uint32_t n = static_cast<uint32_t>((end - start) / sparrow_cache_block_size);
			FileOffset* ids = static_cast<FileOffset*>(IOContext::getTempBuffer3(n * sizeof(FileOffset)));
			const PartitionFile& file = cacheHint_->getPartitionFile();
			offset = start;
			for (uint32_t i = 0; i < n; ++i, offset += sparrow_cache_block_size) {
				ids[i] = FileOffset(file, offset);
			}
			BlockCache::get().acquireMultiple(cacheHint_->getLevel(), ids, n, cacheEntries);
			offset = start;
			for (uint32_t j=0; j<cacheEntries.entries(); ++j) {
				BlockCacheEntry* entry = cacheEntries[j];
				assert(offset < size_);
				const uint32_t length = static_cast<uint32_t>(std::min(static_cast<uint64_t>(sparrow_cache_block_size), size_ - offset));
				const FileBlock block(getData() + offset - start_, length);
				entry->getValue().replace(block);
				entry->setValid(true);
				offset += sparrow_cache_block_size;
			}
		}
	} catch(const SparrowException& e) {
		failed_ = true;
		throw;
	}
}

}
