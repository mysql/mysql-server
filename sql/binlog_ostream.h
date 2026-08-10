/* Copyright (c) 2018, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef BINLOG_OSTREAM_INCLUDED
#define BINLOG_OSTREAM_INCLUDED

#include <openssl/evp.h>
#include "sql/basic_ostream.h"
#include "sql/rpl_log_encryption.h"

// True if binlog cache is reset.
#ifndef NDEBUG
extern bool binlog_cache_is_reset;
#endif

/**
   Copy data from an input stream to an output stream.

   @param[in] istream   the input stream where data will be copied from
   @param[out] ostream  the output stream where data will be copied into
   @param[out] ostream_error It will be set to true if an error happens on
                             ostream and the pointer is not null. It is valid
                             only when the function returns true.

   @retval false Success
   @retval true Error happens in either the istream or ostream.
*/
template <class ISTREAM, class OSTREAM>
bool stream_copy(ISTREAM *istream, OSTREAM *ostream,
                 bool *ostream_error = nullptr) {
  DBUG_TRACE;
  unsigned char *buffer = nullptr;
  my_off_t length = 0;

  bool ret = istream->begin(&buffer, &length);
  while (!ret && length > 0) {
    if (ostream->write(buffer, length)) {
      if (ostream_error != nullptr) *ostream_error = true;
      return true;
    }

    ret = istream->next(&buffer, &length);
  }
  return ret;
}

/**
   A binlog cache implementation based on IO_CACHE.
*/
class IO_CACHE_binlog_cache_storage : public Truncatable_ostream {
 public:
  IO_CACHE_binlog_cache_storage();
  IO_CACHE_binlog_cache_storage &operator=(
      const IO_CACHE_binlog_cache_storage &) = delete;
  IO_CACHE_binlog_cache_storage(const IO_CACHE_binlog_cache_storage &) = delete;
  ~IO_CACHE_binlog_cache_storage() override;

  /**
     Opens the binlog cache. It creates a memory buffer as long as cache_size.
     The buffer will be extended up to max_cache_size when writing data. The
     data exceeding max_cache_size will be written into a temporary file.

     @param[in] dir  Where the temporary file will be created
     @param[in] prefix  Prefix of the temporary file name
     @param[in] cache_size  Size of the memory buffer.
     @param[in] max_cache_size  Maximum size of the memory buffer
     @retval false  Success
     @retval true  Error
  */
  bool open(const char *dir, const char *prefix, my_off_t cache_size,
            my_off_t max_cache_size);
  void close();

  bool write(const unsigned char *buffer, my_off_t length) override;
  bool truncate(my_off_t offset) override;
  /* purecov: inspected */
  /* binlog cache doesn't need seek operation. Setting true to return error */
  bool seek(my_off_t offset [[maybe_unused]]) override { return true; }
  /**
     Reset status and drop all data. It looks like a cache never was used after
     reset.
  */
  bool reset();
  /**
     Returns the file name if a temporary file is opened, otherwise nullptr is
     returned.
  */
  const char *tmp_file_name() const;
  /**
     Returns the count of calling temporary file's write()
  */
  size_t disk_writes() const;

  /**
     Initializes the binlog cache for reading and returns the data at the
     beginning.
     buffer is controlled by binlog cache implementation, so caller should
     not release it. If the function sets *length to 0 and no error happens,
     it has reached the end of the cache.

     @param[out] buffer  It points to buffer where data is read.
     @param[out] length  Length of the data in the buffer.
     @retval false  Success
     @retval true  Error
  */
  bool begin(unsigned char **buffer, my_off_t *length);
  /**
     Returns next piece of data. buffer is controlled by binlog cache
     implementation, so caller should not release it. If the function sets
     *length to 0 and no error happens, it has reached the end of the cache.

     @param[out] buffer  It points to buffer where data is read.
     @param[out] length  Length of the data in the buffer.
     @retval false  Success
     @retval true  Error
  */
  bool next(unsigned char **buffer, my_off_t *length);
  my_off_t length() const;
  bool flush() override { return false; }
  bool sync() override { return false; }

  /*
    ----------------------------------------------------------------------------
    Support for renaming a binlog cache temporary file to a binary log file
    (see Binlog_commit_by_rotate). To rename the temporary file, enough space
    must be reserved at the beginning of the file. The space is required for the
    Format description, Previous_gtids and GTID events that describe the state
    of the binary log the file becomes. The reserved header is hidden from
    callers: get_byte_position()/length() still return the length of the binlog
    data written to the cache, not the file length.
    ----------------------------------------------------------------------------
  */

  /**
    It returns the actual length of the temporary file which includes the
    reserved space at the beginning of the file.
  */
  my_off_t get_file_end_pos() const;

  /**
    Reserved bytes at the beginning of the temporary file. It could be 0 for the
    cases in which reserving space is not supported. See
    init_file_reserved_bytes().
  */
  my_off_t file_reserved_bytes() const { return m_file_reserved_bytes; }

  /**
    It lazily initializes the reserved space of the temporary file on the first
    write and returns the reserved bytes. Returns 0 if reserving space is
    disabled.
  */
  my_off_t get_file_reserved_size();

  /**
    It is called after renaming the temporary file to a binary log file. The
    file now is a binary log file, so detach it from the binlog cache.
  */
  void detach_temp_file();

  /**
    Flush and sync the data of the temporary file into storage.

    @retval true  An error occurred while syncing the file.
    @retval false The file was synced successfully.
  */
  bool sync_temp_file();

  /**
    Returns true if the temporary file encryption is enabled.
  */
  bool is_encryption_enabled() const {
    return m_io_cache.m_encryptor != nullptr ||
           m_io_cache.m_decryptor != nullptr;
  }

  IO_CACHE *get_io_cache() { return &m_io_cache; }
  my_off_t get_max_cache_size() const { return m_max_cache_size; }

 private:
  IO_CACHE m_io_cache;
  my_off_t m_max_cache_size = 0;

  /**
    Stores the bytes reserved at the beginning of the temporary file. It is 0
    for the cases in which reserving space is not supported (encryption enabled,
    feature disabled). It is cleared by reset().
  */
  my_off_t m_file_reserved_bytes = 0;

  /**
    The raw length of the temporary file, which includes the reserved space.
  */
  my_off_t raw_length() const;

  /**
    Reserve the required space at the beginning of the temporary file. It
    creates the temporary file if it does not exist yet. It is called by
    get_file_reserved_size() the first time anything is written into the cache.
  */
  void init_file_reserved_bytes();

  /**
    Generate a unique name for the (KEEP) temporary file. The file is created in
    the #binlog_cache_files directory next to the binary log files, so that it
    can be renamed to a binary log file at commit time.

    @param[out] name  Buffer of at least FN_REFLEN bytes to hold the name.
  */
  void generate_tmp_file_name(char *name);
  /**
    Enable IO Cache temporary file encryption.

    @retval false Success.
    @retval true Error.
  */
  bool enable_encryption();
  /**
    Disable IO Cache temporary file encryption.
  */
  void disable_encryption();
  /**
    Generate a new password for the temporary file encryption.

    This function is called by reset() that is called every time a transaction
    commits to cleanup the binary log cache. The file password shall vary not
    only per temporary file, but also per transaction being committed within a
    single client connection.

    @retval false Success.
    @retval true Error.
  */
  bool setup_ciphers_password();
};

/**
   Byte container that provides a storage for serializing session
   binlog events. This way of arranging the classes separates storage layer
   and binlog layer, hides the implementation detail of low level storage.
*/
class Binlog_cache_storage : public Basic_ostream {
 public:
  ~Binlog_cache_storage() override;

  bool open(my_off_t cache_size, my_off_t max_cache_size);
  void close();

  bool write(const unsigned char *buffer, my_off_t length) override {
    assert(m_pipeline_head != nullptr);
    return m_pipeline_head->write(buffer, length);
  }
  /**
     Truncates some data at the end of the binlog cache.

     @param[in] offset  Where the binlog cache will be truncated to.
     @retval false  Success
     @retval true  Error
  */
  bool truncate(my_off_t offset) { return m_pipeline_head->truncate(offset); }

  /**
     Reset status and drop all data. It looks like a cache was never used
     after reset.
  */
  bool reset() { return m_file.reset(); }
  /**
     Returns the count of disk writes
  */
  size_t disk_writes() const { return m_file.disk_writes(); }
  /**
     Returns the name of the temporary file.
  */
  const char *tmp_file_name() const { return m_file.tmp_file_name(); }

  /**
     Copy all data to a output stream. This function hides the internal
     implementation of storage detail. So it will not disturb the callers
     if the implementation of Binlog_cache_storage is changed. If we add
     a pipeline stream in this class, then we need to change the implementation
     of this function. But callers are not affected.

     @param[out] ostream Where the data will be copied into
     @param[out] ostream_error It will be set to true if an error happens on
                               ostream and the pointer is not null. It is valid
                               only when the function returns true.
     @retval false  Success
     @retval true  Error happens in either the istream or ostream.
  */
  bool copy_to(Basic_ostream *ostream, bool *ostream_error = nullptr) {
    DBUG_TRACE;
    return stream_copy(&m_file, ostream, ostream_error);
  }

  /**
     Returns data length.
  */
  my_off_t length() const { return m_file.length(); }
  /**
     Returns true if binlog cache is empty.
  */
  bool is_empty() const { return length() == 0; }

  /*
    Accessors used to rename the transactional cache temporary file to a binary
    log file (see Binlog_commit_by_rotate).
  */

  /** It returns the reserved bytes at the beginning of the temporary file. */
  my_off_t file_reserved_bytes() const { return m_file.file_reserved_bytes(); }

  /**
    It lazily initializes and returns the reserved space of the temporary file.
  */
  my_off_t get_file_reserved_size() { return m_file.get_file_reserved_size(); }

  /**
    It returns the actual length of the temporary file which includes the
    reserved space.
  */
  my_off_t get_file_end_pos() const { return m_file.get_file_end_pos(); }

  /** Detach the temporary file after it was renamed to a binary log file. */
  void detach_temp_file() { m_file.detach_temp_file(); }

  /** Flush and sync the temporary file to storage. */
  bool sync_temp_file() { return m_file.sync_temp_file(); }

  /** Returns true if the temporary file encryption is enabled. */
  bool is_encryption_enabled() { return m_file.is_encryption_enabled(); }

  IO_CACHE_binlog_cache_storage *get_io_cache_storage() { return &m_file; }

 private:
  Truncatable_ostream *m_pipeline_head = nullptr;
  IO_CACHE_binlog_cache_storage m_file;
};

/**
  It is an Truncatable_ostream which provides encryption feature. It can be
  setup into an stream pipeline. In the pipeline, it encrypts the data
  from up stream and then feeds the encrypted data into down stream.
*/
class Binlog_encryption_ostream : public Truncatable_ostream {
 public:
  ~Binlog_encryption_ostream() override;

  /**
    Initialize the context used in the encryption stream and write encryption
    header into down stream.

    @param[in] down_ostream The stream for storing encrypted data.

    @retval false Success
    @retval true Error.
  */
  bool open(std::unique_ptr<Truncatable_ostream> down_ostream);

  /**
    Initialize the context used in the encryption stream based on the
    header passed as parameter. It shall be used when opening an ostream for
    a stream that was already encrypted (the cypher password already exists).

    @param[in] down_ostream the stream for storing encrypted data.
    @param[in] header the encryption header to setup the cypher.

    @retval false Success.
    @retval true Error.
  */
  bool open(std::unique_ptr<Truncatable_ostream> down_ostream,
            std::unique_ptr<Rpl_encryption_header> header);

  /**
    Re-encrypt the encrypted binary/relay log file header by replacing its
    binlog encryption key id with the current one and its encrypted file
    password with the new one, which is got by encrypting its file password
    with the current binlog encryption key.

    @retval false Success with an empty error message.
    @retval true Error with an error message.
  */
  std::pair<bool, std::string> reencrypt();

  void close();
  bool write(const unsigned char *buffer, my_off_t length) override;
  bool truncate(my_off_t offset) override;
  bool seek(my_off_t offset) override;
  bool flush() override;
  bool sync() override;
  /**
    Return the encrypted file header size.

    @return the encrypted file header size.
  */
  int get_header_size();

 private:
  std::unique_ptr<Truncatable_ostream> m_down_ostream;
  std::unique_ptr<Rpl_encryption_header> m_header;
  std::unique_ptr<Stream_cipher> m_encryptor;
};
#endif  // BINLOG_OSTREAM_INCLUDED
