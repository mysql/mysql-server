// File: $Id$
// Author: K. John Wu <John.Wu at acm.org>
//         Lawrence Berkeley National Laboratory
// Copyright (c) 2000-2016 University of California
#ifndef IBIS_FILEMANAGER_H
#define IBIS_FILEMANAGER_H
/// @file
/// Defines a simple file manager.
///
/// @note Use malloc and realloc to manage memory when the file content is
/// actually in memory.  The main reason for doing so is to malloc for
/// resizing.  This may potentially cause problems with memory allocation
/// through the new operator provided by C++ compiler.
///
#include "util.h"

#include <set>		// std::set
#include <map>		// std::map

/// @ingroup FastBitIBIS
/// This fileManager is intended to allow different objects to share the
/// same open file.  It does not manage writing of files.
class FASTBIT_CXX_DLLSPEC ibis::fileManager {
public:

    /// Hint passed to the function @c getFile.  The main choice is whether
    /// to use memory map or use the read function to access the content of
    /// a file.
    enum ACCESS_PREFERENCE {
	MMAP_LARGE_FILES,	// files > minMapSize are mapped if possible
	PREFER_READ,		// read the whole file into memory
	PREFER_MMAP		// try to use mmap if possible
    };

    template<typename T>
    int getFile(const char* name, array_t<T>& arr,
		ACCESS_PREFERENCE pref=MMAP_LARGE_FILES);
    template<typename T>
    int tryGetFile(const char* name, array_t<T>& arr,
		   ACCESS_PREFERENCE pref=MMAP_LARGE_FILES);

    /// Prints status information about the file manager.
    void printStatus(std::ostream& out) const;
    /// Close the file, remove the record about it from the file manager.
    void flushFile(const char* name);
    /// Close all files in the named directory, including subdirectories.
    void flushDir(const char* name);
    /// Close all files and remove all records of them.
    void clear();

    /// Returns a pointer to the one and only file manager.
    static fileManager& instance();
    /// Returns the value of a simple counter.  It is not thread-safe!
    static time_t iBeat() {return hbeat++;}
    /// Returns the number of pages accessed by function read from stdlib.h.
    const double& pageCount() const {return page_count;}
    /// Returns the page size (in bytes) used by the file system.
    static uint32_t pageSize() {return pagesize;}
    inline void recordPages(off_t start, off_t stop);
    static inline void increaseUse(size_t inc, const char* evt);
    static inline void decreaseUse(size_t dec, const char* evt);
    /// Signal to the file manager that some memory have been freed.
    void signalMemoryAvailable() const;

    /// A function object to be used to register external cleaners.
    class cleaner {
    public:
	virtual void operator()() const = 0;
	virtual ~cleaner() {};
    };
    void addCleaner(const cleaner* cl);
    void removeCleaner(const cleaner* cl);

    class roFile; // forward declaration of fileManager::roFile
    class storage; // forward declaration of fileManager::storage
#if defined(HAVE_FILE_MAP)
    class rofSegment; // forward declaration of fileManager::rofSegment
#endif
    friend class roFile;
    friend class storage;
    int getFile(const char* name, storage** st,
		ACCESS_PREFERENCE pref=MMAP_LARGE_FILES);
    int tryGetFile(const char* name, storage** st,
		   ACCESS_PREFERENCE pref=MMAP_LARGE_FILES);
    static storage* getFileSegment(const char* name, const int fdes,
				   const off_t b, const off_t e);

    // /// Obtain a read lock on the file manager.
    // inline void gainReadAccess(const char* mesg) const;
    // /// Release a read lock on the file manager.
    // inline void releaseAccess(const char* mesg) const;
    /// A read lock on the file manager.  Any object using a file under the
    // /// management of the file manager should hold a readLock.
    // class readLock {
    // public:
    // 	/// Constructor.  Acquires a read lock.
    // 	readLock(const char* m) : mesg(m) {
    // 	    ibis::fileManager::instance().gainReadAccess(m);
    // 	}
    // 	/// Destructor.  Releases the read lock.
    // 	~readLock() {
    // 	    ibis::fileManager::instance().releaseAccess(mesg);
    // 	}
    // private:
    // 	/// A free-form message.  Typically used to identify the holder of
    // 	/// the lock.
    // 	const char* mesg;
    // };

    /// Return the current cache size in bytes.
    static uint64_t currentCacheSize() {return maxBytes;}
    /// Change the size of memory cache allocated to the file manager.
    static int adjustCacheSize(uint64_t);
    /// Returns the number of bytes currently on records.
    static uint64_t bytesInUse() {return ibis::fileManager::totalBytes();}
    /// Return the number of bytes free.
    static uint64_t bytesFree();

    /// Return the count of files that are memory mapped.
    unsigned int getMaxOpenMmapFiles() const;
    /// Return the size in bytes of files that are memory mapped.
    uint64_t getMaxMmapBytes() const;

    /// A buffer is intended to be a temporary workspace in memory.  The
    /// constructor allocates a certain amount of memory, default to 16 MB;
    /// the destructor release the memory.  Its size can not be changed.
    template <typename T>
    class buffer {
    public:
	/// Constructor.  Default size is 16 MB.
	buffer(size_t sz=0);
	/// Destructor.  Release the memory allocated.
	~buffer();

	/// Return the ith value.  It does not perform array bounds check!
	T& operator[](size_t i) {return buf[i];}
	/// Return the ith value.  It does not perform array bounds check!
	const T& operator[](size_t i) const {return buf[i];}
	/// Address of the buffer allocated.
	T* address() const {return buf;}
	/// The number of elements in the buffer.  NOT the number of bytes.
	size_t size() const {return nbuf;}
	/// Increase the size of the buffer.
	size_t resize(size_t sz=0);
	/// Swap the content of two buffers.
	void swap(buffer<T>& other) throw () {
	    T* btmp = buf;
	    buf = other.buf;
	    other.buf = btmp;
	    size_t ntmp = nbuf;
	    nbuf = other.nbuf;
	    other.nbuf = ntmp;
	}

    private:
	T* buf; ///!< The address of the buffer.
	size_t nbuf; ///!< The number of elements in the buffer.

	buffer(const buffer<T>&);
	buffer<T>& operator=(const buffer<T>&);
    }; // buffer

protected:
    fileManager();
    ~fileManager();

    void recordFile(roFile*);
    void unrecordFile(roFile*);

    // parameters for controlling the resource usage
    /// The total number of bytes of all managed objects.
    static ibis::util::sharedInt64 totalBytes;
    /// The maximum number of bytes allowed.
    static uint64_t maxBytes;
    /// The Maximum number of files that can be kept open at the same time.
    static unsigned int maxOpenFiles;

    // not implemented, to prevent compiler from generating these functions
    fileManager(const fileManager& rhs);
    fileManager& operator=(const fileManager& rhs);

private:
    typedef std::map< const char*, roFile*,
		      std::less< const char* > > fileList;
    typedef std::set< const cleaner* > cleanerList;
    typedef std::set< const char*, std::less< const char* > > nameList;
    /// Files that are memory mapped.
    fileList mapped;
    /// Files that have been read into the main memory.
    fileList incore;
    /// Files that are being read by the function getFile.
    nameList reading;
    /// List of external cleaners.
    cleanerList cleaners;
    /// The number of pages read by read from @c unistd.h.
    double page_count;
    /// the minimum size of a file before it is memory mapped.
    uint32_t minMapSize;
    /// Number of threads waiting for memory.
    uint32_t nwaiting;
    /// The conditional variable for reading list.
    pthread_cond_t readCond;

    /// The multiple read single write lock
    //mutable pthread_rwlock_t lock;
    /// Control access to incore and mapped
    mutable pthread_mutex_t mutex;
    /// Conditional variable.  Used to control waiting for I/O operations
    /// and memory allocations.
    mutable pthread_cond_t cond;

    /// A simple counter.  No mutex lock.
    static time_t hbeat;
    /// The number of bytes in a page.
    static uint32_t pagesize;

    int unload(size_t size);	// try to unload size bytes
    void invokeCleaners() const;// invoke external cleaners
    //inline void gainWriteAccess(const char* m) const;

    // class writeLock;
    // class softWriteLock;
    // friend class writeLock;
    // friend class softWriteLock;
}; // class fileManager

/// The storage class treats all memory as @a char*.
/// It only uses malloc family of functions to manage the memory allocation
/// and deallocation.
///
/// @note This class intends to hold a piece of memory managed by
/// ibis::fileManager.  If an object of this type is acquired through
/// ibis::fileManager::getFile, the ownership of the object belongs to the
/// file manager, therefore the caller should not delete the object.  Of
/// course, the object created through explicit call to a constructor is
/// owned by the user code.
class FASTBIT_CXX_DLLSPEC ibis::fileManager::storage {
public:
    storage();
    explicit storage(size_t n); // allocate n bytes
    virtual ~storage() {clear();}

    storage(const char* fname, const off_t begin, const off_t end);
    storage(const int fdes, const off_t begin, const off_t end);
    storage(const char* begin, const char* end);
    storage(char* addr, size_t num);
    storage(const storage& rhs);
    storage& operator=(const storage& rhs);
    void copy(const storage& rhs);

    /// Pointer to the file name supporting this storage object.  It
    /// returns nil for in-memory storage.
    const char* filename() const {return name;}

    /// Is the storage object empty?
    bool empty() const {return (m_begin == 0 || m_begin >= m_end);}
    /// Return the size (bytes) of the object.
    size_t size() const {
	return (m_begin!=0 && m_begin<m_end ? m_end-m_begin : 0);}
    /// Return the number of bytes contained in the object.
    size_t bytes() const {
	return (m_begin!=0 && m_begin<m_end ? m_end-m_begin : 0);}
    void enlarge(size_t nelm=0);

    /// Release the control of the memory to the caller as a raw pointer.
    virtual void* release();
    /// Starting address of the storage object.
    char* begin() {return m_begin;}
    /// Ending address of the storage object.
    const char* end() const {return m_end;}
    /// Starting address of the storage object.
    const char* begin() const {return m_begin;}
    /// Unchecked index operator.  Returns the character at position i.
    char operator[](size_t i) const {return m_begin[i];}

    virtual void beginUse();
    virtual void endUse();
    /// Number of current accesses to this object.
    unsigned inUse() const {return nref();}
    /// Number of past accesses to this object.
    unsigned pastUse() const {return nacc;}

    /// Is the storage a file map ?
    virtual bool isFileMap() const {return false;}
    // IO functions
    virtual void printStatus(std::ostream& out) const;
    off_t read(const char* fname, const off_t begin, const off_t end);
    off_t read(const int fdes, const off_t begin, const off_t end);
    void  write(const char* file) const;

    inline void swap(storage& rhs) throw ();
//      // compares storage objects according to starting addresses
//      struct less : std::binary_function< storage*, storage*, bool > {
//  	bool operator()(const storage* x, const storage* y) const {
//  	    return (x->begin() < y->begin());
//  	}
//      };

protected:
    /// Name of the file.  NULL (0) if no file is involved.
    char* name;
    /// Beginning of the storage.
    char* m_begin;
    /// End of the storage.
    char* m_end;
    /// Number of accesses in the past.
    unsigned nacc;
    /// Number of (active) references to this storage.
    ibis::util::sharedInt32 nref;

    virtual void clear(); // free memory/close file
}; // class fileManager::storage

/// This class manages content of a whole (read-only) file.
/// It inherits the basic information stored in fileManager::storage and is
/// intended to process read-only files.
///
/// @note This class intends to hold a piece of memory managed by
/// ibis::fileManager.  If an object of this type is acquired through
/// ibis::fileManager::getFile, the ownership of the object belongs to the
/// file manager, therefore the caller should not delete the object.
class FASTBIT_CXX_DLLSPEC ibis::fileManager::roFile
    : public ibis::fileManager::storage {
public:
    virtual ~roFile() {clear();}

    // functions for recording access statistics
    virtual void beginUse();
    virtual void endUse();
    // is the read-only file mapped ?
    virtual bool isFileMap() const {return (mapped != 0);}
    int disconnectFile();

    // IO functions
    virtual void printStatus(std::ostream& out) const;
    void read(const char* file);
#if defined(HAVE_FILE_MAP)
    void mapFile(const char* file);
#endif

//      // compares storage objects according to file names
//      struct less : std::binary_function< roFile*, roFile*, bool > {
//  	bool operator()(const roFile* x, const roFile* y) const {
//  	    return strcmp(x->filename(), y->filename()) < 0;
//  	}
//      };
protected:
    roFile();
    // Read the whole file into memory.
    void doRead(const char* file);
    // Read the specified segment of the file into memory.
    void doRead(const char* file, off_t b, off_t e);
#if defined(HAVE_FILE_MAP)
    void doMap(const char* file, off_t b, off_t e, int opt=0);
#endif

    /// The function assigns a score to a file.  It is used by
    /// ibis::fileManager::unload to determine what files to remove.  Files
    /// with the smallest scores are the target for removal.
    float score() const {
	float sc = FLT_MAX;
	time_t now = time(0);
	if (opened >= now) {
	    sc = static_cast<float>(1e-4 * size() + nacc);
	}
	else if (lastUse >= now) {
	    sc = static_cast<float>(sqrt(5e-6*size()) + nacc +
				    (now - opened));
	}
	else {
	    sc = static_cast<float>((sqrt(1e-6*size() + now - opened) +
				     (static_cast<double>(nacc) /
				      (now - opened))) / (now - lastUse));
	}
	return sc;
    }

    friend class ibis::fileManager;
    virtual void clear(); // free memory/close file
    virtual void* release() {return 0;}

    void printBody(std::ostream& out) const;

private:
    /// time first created, presumably when the file was opened
    time_t opened;
    /// time of last use
    time_t lastUse;
    /// 0 not a mapped file, otherwise yes
    unsigned mapped;

#if defined(_WIN32) && defined(_MSC_VER)
    HANDLE fdescriptor; // HANDLE to the open file
    HANDLE fmap; // HANDLE to the mapping object
    LPVOID map_begin; // actual address returned by MapViewOfFile
#elif (HAVE_MMAP+0 > 0)
    int fdescriptor; // descriptor of the open file
    size_t fsize;    // the size of the mapped portion of file
    void *map_begin; // actual address returned by mmap
#endif

    // not implemented, to prevent automatic generation
    roFile(const roFile&);
    roFile& operator=(const roFile&);
}; // class fileManager::roFile

#if defined(HAVE_FILE_MAP)
/// This class is used to store information about a portion of a memory
/// mapped file.  The main reason this is a derived class of roFile is to
/// make this one not shareable.
class FASTBIT_CXX_DLLSPEC ibis::fileManager::rofSegment
    : public ibis::fileManager::roFile {
public:
    rofSegment(const char *fn, off_t b, off_t e);
    virtual ~rofSegment() {};
    virtual void printStatus(std::ostream& out) const;

private:
    rofSegment(); // no default constructor
    rofSegment(const rofSegment&); // no copy constructor
    rofSegment& operator=(const rofSegment&); // no assignment operator

    std::string filename_; // name of the file
    off_t begin_, end_;    // the start and the end address of the file map
}; // ibis::fileManager::rofSegment
#endif

// /// A write lock for controlling access to the two internal lists.
// class ibis::fileManager::writeLock {
// public:
//     /// Constructor.  Acquires a write lock.
//     writeLock(const char* m) : mesg(m)
//     {ibis::fileManager::instance().gainWriteAccess(mesg);}
//     /// Destructor.  Releases the write lock.
//     ~writeLock() {ibis::fileManager::instance().releaseAccess(mesg);}
// private:
//     const char* mesg;

//     writeLock(const writeLock&);
//     writeLock& operator=(const writeLock&);
// }; // ibis::fileManager::writeLock

// /// A soft write lock for controlling access to the two internal lists.
// class ibis::fileManager::softWriteLock {
// public:
//     softWriteLock(const char* m);
//     ~softWriteLock();
//     /// Has a write lock been acquired?  Returns true or false to indicate
//     /// yes or no.
//     bool isLocked() const {return(locked_==0);}

// private:
//     const char* mesg;
//     const int locked_;

//     softWriteLock(const softWriteLock&);
//     softWriteLock& operator=(const softWriteLock&);
// }; // ibis::fileManager::softWriteLock

inline uint64_t ibis::fileManager::bytesFree() {
    if (maxBytes == 0)
	ibis::fileManager::instance();
    return (maxBytes > ibis::fileManager::totalBytes() ?
	    maxBytes - ibis::fileManager::totalBytes() : 0);
} // ibis::fileManager::bytesFree

// /// Release the read/write lock.
// inline void ibis::fileManager::releaseAccess(const char* mesg) const {
//     int ierr = pthread_rwlock_unlock(&lock);
//     if (0 == ierr) {
// 	LOGGER(ibis::gVerbose > 9)
// 	    << "fileManager::releaseAccess   on "
// 	    << static_cast<const void*>(&lock) << " for " << mesg;
//     }
//     else {
// 	LOGGER(ibis::gVerbose >= 0)
// 	    << "Warning -- fileManager::releaseAccess   on "
// 	    << static_cast<const void*>(&lock) << " for " << mesg
// 	    << " failed with the error code " << ierr << " -- "
// 	    << strerror(ierr);
//     }
// } // ibis::fileManager::releaseAccess

// /// Gain read access.  It blocks when waiting to acquire the read lock.
// inline void ibis::fileManager::gainReadAccess(const char* mesg) const {
//     int ierr = pthread_rwlock_rdlock(&lock);
//     if (0 == ierr) {
// 	LOGGER(ibis::gVerbose > 9)
// 	    << "fileManager::gainReadAccess  on "
// 	    << static_cast<const void*>(&lock) << " for " << mesg;
//     }
//     else {
// 	LOGGER(ibis::gVerbose >= 0)
// 	    << "Warning -- fileManager::gainReadAccess  on "
// 	    << static_cast<const void*>(&lock) << " for " << mesg
// 	    << " failed with the error code " << ierr << " -- "
// 	    << strerror(ierr);
//     }
// } // ibis::fileManager::gainReadAccess

// /// Gain write access.  It blocks when waiting to acquire the write lock.
// inline void ibis::fileManager::gainWriteAccess(const char* mesg) const {
//     int ierr = pthread_rwlock_wrlock(&lock);
//     if (0 == ierr) {
// 	LOGGER(ibis::gVerbose > 9)
// 	    << "fileManager::gainWriteAccess on "
// 	    << static_cast<const void*>(&lock) << " for " << mesg;
//     }
//     else {
// 	LOGGER(ibis::gVerbose >= 0)
// 	    << "Warning -- fileManager::gainWriteAccess on "
// 	    << static_cast<const void*>(&lock) << " for " << mesg
// 	    << " failed with the error code " << ierr << " -- "
// 	    << strerror(ierr);
//     }
// } // ibis::fileManager::gainWriteAccess

/// Given the starting and ending addresses, this function computes the
/// number of pages involved.  Used by derived classes to record page
/// accesses.
inline void ibis::fileManager::recordPages(off_t start, off_t stop) {
    if (stop - start > 0) {
	start = (start / pagesize) * pagesize;
	page_count += ceil(static_cast<double>((stop - start)) / pagesize);
    }
} // ibis::fileManager::recordPages

inline void
ibis::fileManager::increaseUse(size_t inc, const char* evt) {
    ibis::fileManager::totalBytes += inc;
    LOGGER(inc > 0 && evt != 0 && *evt != 0 && ibis::gVerbose > 9)
	<< evt << " added " << inc << " bytes to increase totalBytes to "
	<< ibis::fileManager::totalBytes();
} // ibis::fileManager::increaseUse

inline void
ibis::fileManager::decreaseUse(size_t dec, const char* evt) {
    ibis::fileManager::totalBytes -= dec;
    LOGGER(dec > 0 && evt != 0 && *evt != 0 && ibis::gVerbose > 9)
	<< evt << " removed " << dec << " bytes to decrease totalBytes to "
	<< ibis::fileManager::totalBytes();
} // ibis::fileManager::decreaseUse

/// Swap the content of the storage objects.
///
/// @note It does not swap the reference counts!  Since changing the
/// storage object requires the client code to update the pointers they
/// hold.  The only way this function is used is to reallocate storage for
/// array_t objects.  In that case, one of the storage object is a
/// temporary one with a reference count of 0 (zero).  It is important to
/// keep that count 0 so the temporary storage object can be freed
/// afterward.  Suggested by Zeid Derhally (2010/02).
inline void
ibis::fileManager::storage::swap(ibis::fileManager::storage& rhs) throw () {
    {char* tmp = name; name = rhs.name; rhs.name = tmp;}
    {char* tmp = m_begin; m_begin = rhs.m_begin; rhs.m_begin = tmp;}
    {char* tmp = m_end; m_end = rhs.m_end; rhs.m_end = tmp;}
    {unsigned itmp = nacc; nacc = rhs.nacc; rhs.nacc = itmp;}
    //nref.swap(rhs.nref);
} // ibis::fileManager::storage::swap
#endif // IBIS_FILEMANAGER_H
