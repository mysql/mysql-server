/*
   Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <LogBuffer.hpp>
#include <portlib/NdbCondition.h>
#include <portlib/NdbThread.h>
#include <BaseString.hpp>
#include <basestring_vsnprintf.h>

LogBuffer::LogBuffer(size_t size) :
    m_log_buf(0),
    m_max_size(size),
    m_size(0),
    m_lost_count(0)
{
  m_log_buf = (char*)malloc(size+1);
  assert(m_log_buf != NULL);

  m_read_ptr = m_log_buf;
  m_write_ptr = m_log_buf;
  m_buf_end = m_log_buf;
  m_top = m_log_buf + size;

  m_mutex = NdbMutex_Create();
  m_cond = NdbCondition_Create();
  assert(checkInvariants());
}

LogBuffer::~LogBuffer()
{
  assert(checkInvariants());
  free(m_log_buf);
  NdbCondition_Destroy(m_cond);
  NdbMutex_Destroy(m_mutex);
}

char*
LogBuffer::getWritePtr(size_t bytes) const
{
  char* ret = NULL;

  if(bytes == 0)
  {
    return ret;
  }

  if(m_write_ptr == m_read_ptr)
  {
    // Border cases: size zero or full

    if(m_size == 0 && m_max_size >= bytes)
    {
      /* Log buffer empty and has enough contiguous space
       * to fit in 'bytes'*/
      assert(m_write_ptr == m_log_buf);
      ret = m_write_ptr;
    }
    else
    {
      // Log buffer is either full or empty with insufficient space
      ret = NULL;
    }
  }
  else if(m_write_ptr > m_read_ptr)
  {
    // m_write_ptr is ahead of m_read_ptr
    if((size_t)(m_top - m_write_ptr) >= bytes)
    {
      /**
       * There's sufficient space between write_ptr
       * and top of the buffer.
       */
      ret = m_write_ptr;
    }
    else if((size_t)(m_read_ptr - m_log_buf) >= bytes)
    {
      /**
       * There's enough space b/w start of buffer and read_ptr.
       * Make buf_end point to write_ptr - 1. Wrap around write_ptr.
       */
      ret = m_log_buf;
    }
  }
  else if((size_t)(m_read_ptr - m_write_ptr) >= bytes)
  {
    /**
     * m_write_ptr is behind m_read_ptr
     * and there's enough space
     */
    ret = m_write_ptr;
  }

  return ret;
}

void
LogBuffer::wrapWritePtr()
{
  // deal with wrap around.
  m_buf_end = m_write_ptr - 1;
  m_write_ptr = m_log_buf;
}

void
LogBuffer::updateWritePtr(size_t written_bytes)
{
  assert(getWritePtr(written_bytes) != NULL);

  m_write_ptr = m_write_ptr + written_bytes; // update m_write_ptr
  assert(m_write_ptr <= m_top);
  m_size += written_bytes;

  if(m_buf_end < (m_write_ptr -1)) //update m_buf_end if required
  {
    m_buf_end = m_write_ptr - 1;
  }

  if(m_write_ptr == m_top)
  {
    /**
     * Wrap around- if m_write_ptr reaches the top of buffer,
     * wrap it around to point to the start of buffer.
     */
    m_write_ptr = m_log_buf;
  }
}

bool
LogBuffer::checkForBufferSpace(size_t write_bytes)
{
  bool ret = true;
  assert(checkInvariants());
  if(m_lost_count)// there are lost bytes
  {
    char* write_ptr = NULL;
    const char* lost_msg = "\n*** %u BYTES LOST ***\n";
    int lost_msg_len = basestring_snprintf(NULL, 0, lost_msg, m_lost_count);
    assert(lost_msg_len > 0);

    // append the lost msg
    if((write_ptr = getWritePtr(write_bytes + lost_msg_len + 1)))
    {
      basestring_snprintf(write_ptr, lost_msg_len + 1, lost_msg, m_lost_count);
      m_lost_count = 0; // make lost count 0
      if(write_ptr == m_log_buf && m_write_ptr != m_log_buf)
      {
        // need to wrap the write ptr
        wrapWritePtr();
      }
      updateWritePtr(lost_msg_len);
    }
    else
    {
      // no space for lost msg and write_bytes
      m_lost_count += write_bytes;
      ret = false;
    }
  }
  assert(checkInvariants());
  return ret;
}

size_t
LogBuffer::append(void* buf, size_t write_bytes)
{
  Guard g(m_mutex);
  assert(checkInvariants());
  assert(write_bytes <= m_max_size);
  char* write_ptr = NULL;
  size_t ret = 0;
  bool buffer_was_empty = (m_size == 0);

  if(write_bytes == 0)
  {
    // nothing to be appended
    return ret;
  }
  // preliminary check for space availability
  if(!checkForBufferSpace(write_bytes))
  {
    // this append is not possible since there's no space for log message
    return ret;
  }

  write_ptr = getWritePtr(write_bytes);
  if(write_ptr)
  {
    memcpy(write_ptr, buf, write_bytes);
    if(write_ptr == m_log_buf && m_write_ptr != m_log_buf)
    {
      //need to wrap the write ptr
      wrapWritePtr();
    }
    updateWritePtr(write_bytes);
    ret = write_bytes;
    if(buffer_was_empty)
    {
      // signal consumers if log buf was empty previously
      NdbCondition_Signal(m_cond);
    }
  }
  else
  {
    // insufficient space to write
    m_lost_count += write_bytes;
    ret = 0;
  }
  assert(checkInvariants());
  return ret;
}

int
LogBuffer::append(const char* fmt, va_list ap, size_t len, bool append_ln)
{
  Guard g(m_mutex);
  assert(checkInvariants());
  char* write_ptr = NULL;
  int ret = 0;
  int res = 0;
  bool buffer_was_empty = (m_size == 0);

  // extra byte for null termination, will be discarded
  size_t write_bytes = len + 1 + append_ln;

  if(write_bytes == 1)
  {
    // nothing to be appended
    return ret;
  }
  assert(write_bytes > 0);
  assert(write_bytes <= m_max_size);

  // preliminary check for space availability, -1 to exclude space for trailing NULL
  if(!checkForBufferSpace(write_bytes - 1))
  {
    // this append is not possible since there's no space for log message.
    ret = 0;
  }
  else // print actual msg
  {
    write_ptr= getWritePtr(write_bytes);

    if(write_ptr)
    {
      res = basestring_vsnprintf(write_ptr, write_bytes, fmt, ap);
      assert(res >= 0);

      if(append_ln)
      {
        write_ptr[write_bytes - 2] = '\n';
      }
      if(write_ptr == m_log_buf && m_write_ptr != m_log_buf)
      {
        //need to wrap the write ptr
        wrapWritePtr();
      }
      updateWritePtr(write_bytes - 1);
      ret = write_bytes - 1;
      if(buffer_was_empty)
      {
        // Signal consumers if log buf was empty previously.
        NdbCondition_Signal(m_cond);
      }
    }
    else
    {
      /**
       * Insufficient space to write, lost count doesn't include
       * the null byte at the end of string.
       */
      m_lost_count += write_bytes - 1;
      ret = 0;
    }
  }

  assert(checkInvariants());
  return ret;
}

size_t
LogBuffer::get(char* buf, size_t buf_size, uint timeout_ms)
{
  Guard g(m_mutex);
  assert(checkInvariants());
  size_t size = buf_size; // max. number of bytes that can be copied to buf
  int cond_ret = 0;

  if(buf_size == 0)
  {
    return 0;
  }

  // Wait until there's something in the buffer or until timeout
  while((m_size == 0) && (cond_ret == 0))
  {
    /**
     * Log buffer is empty, block until signal is received
     * or until timeout.
     */
    cond_ret = NdbCondition_WaitTimeout(m_cond, m_mutex, timeout_ms);
    if(cond_ret != 0)
    {
      assert(cond_ret == ETIMEDOUT);
    }
  }


  if(m_size == 0)
  {
    // log buffer empty even after timeout, return
    return 0;
  }
  else if(m_size < buf_size)
  {
    // change num of bytes to be copied to available bytes
    size = m_size;
  }

  // Bytes present in log buffer for sure at this point.

  if(m_write_ptr <= m_read_ptr && ((size_t)(m_buf_end - m_read_ptr + 1) < size))
  {
    // Read and copy to buf in parts.
    size_t first_part_size, second_part_size;
    first_part_size = m_buf_end - m_read_ptr + 1;
    second_part_size = size - first_part_size;

    memcpy(buf, m_read_ptr, first_part_size);
    memcpy(buf+first_part_size, m_log_buf, second_part_size);
    m_read_ptr = m_log_buf + second_part_size;
    m_size -= size;
  }
  else
  {
    /**
     * wptr >= rptr, read in one go, or
     * write_ptr <= read_ptr && bytes between read_ptr and end of buff is greater
     * than size
     */
    memcpy(buf, m_read_ptr, size);
    m_read_ptr += size; // update m_read_ptr
    m_size -= size;

    if((m_read_ptr == m_buf_end + 1) && m_read_ptr != m_write_ptr)
    {
      /* m_read_ptr has read up until m_buf_end,
       * make m_read_ptr wrap around to start of log buffer.
       */
      m_read_ptr = m_log_buf;
    }
  }

  if(m_read_ptr < m_write_ptr)
  {
    /**
     * m_read_ptr could have wrapped around through m_buf_end,
     * reassign m_buf_end to point to one byte before m_write_ptr.
     */
    m_buf_end = m_write_ptr - 1;
  }

  if(m_read_ptr == m_write_ptr)
  {
    /**
     * Make m_read_ptr, m_write_ptr, m_buf_end point
     * to start of log buffer (like it is initially).
     *
     * This makes an append of the maximum possible contiguous length possible
     */
    m_read_ptr = m_write_ptr = m_buf_end = m_log_buf;
  }

  assert(checkInvariants());
  return size;
}

size_t
LogBuffer::getSize() const
{
  return m_size;
}

size_t
LogBuffer::getLostCount() const
{
  return m_lost_count;
}

bool
LogBuffer::checkInvariants() const
{
  assert(m_read_ptr <= m_buf_end); // equal if log buf is empty or has one byte
  assert(m_size <= m_max_size);
  assert(m_write_ptr < m_top);

  if(m_size == 0)
  {
     assert(m_read_ptr == m_log_buf);
     assert(m_write_ptr == m_log_buf);
     assert(m_buf_end == m_log_buf);
  }
  else
  {
    if(m_read_ptr != m_write_ptr)
    {
      if(m_read_ptr < m_write_ptr)
      {
        assert(m_size == (size_t)(m_write_ptr - m_read_ptr));
      }
      else
      {
        assert(m_size == (size_t)((m_write_ptr - m_log_buf) + (m_buf_end - m_read_ptr) + 1));
      }
    }
  }
  return true;
}

#ifdef TEST_LOGBUFFER

LogBuffer* buf_t1;
LogBuffer* buf_t2;
LogBuffer* buf_t3;
LogBuffer* buf_t4;

bool stop_t2 = false;
bool stop_t3 = false;

int total_bytes_read_t3 = 0;
int bytes_lost_t3 = 0;
int bytes_written_t3 = 0;
int total_to_write_t3 = 0;


void clearbuf(char* buf, uint size)
{
  memset(buf, '*', size);
}

void fun(const char* fmt, ...)
  ATTRIBUTE_FORMAT(printf, 1, 2);

void fun(const char* fmt, ...)
{
  va_list arguments;

  va_start(arguments, fmt);
  int len = basestring_vsnprintf(NULL, 0, fmt, arguments);
  va_end(arguments);

  va_start(arguments, fmt);
  buf_t2->append(fmt, arguments, len);
  va_end(arguments);
}

void* thread_producer1(void* dummy)
{
  BaseString string;
  for(int i = 1; i <= 1000; i++)
  {
    if(i%40 == 0)
    {
      sleep(1);
    }
    string = string.assfmt("Log %*d\n", 5, i);
    buf_t2->append((void*)string.c_str(), string.length());
  }
  NdbThread_Exit(NULL);
  return NULL;
}

void* thread_producer2(void* dummy)
{
  for(int i = 1;i <= 1000; i++)
  {
    if(i%40 == 0)
    {
      sleep(1);
    }
    fun("Log %*d\n", 5, -i);
  }
  NdbThread_Exit(NULL);
  return NULL;
}

void* thread_producer3(void* dummy)
{
  char buf[10];
  memset(buf, '$', 10);
  size_t to_write_bytes = 0;
  srand((unsigned int)time(NULL));
  int sleep_when = rand() % 10 + 1;
  for(int i = 0; i < 20; i++)
  {
    if(i % sleep_when == 0)
    {
      sleep(1);
    }
    to_write_bytes = rand() % 10 + 1;
    total_to_write_t3 += to_write_bytes;
    int ret = buf_t3->append((void*)buf, to_write_bytes);
    if(ret)
    {
      printf("Write: %d bytes\n", ret);
      bytes_written_t3 += ret;
    }
    else
    {
      printf("Lost: %lu bytes\n", (long unsigned)to_write_bytes);
      bytes_lost_t3 += to_write_bytes;
    }
  }

  NdbThread_Exit(NULL);
  return NULL;
}

void* thread_consumer1(void* dummy)
{
  char buf[256];
  size_t bytes = 0;
  int i = 0;
  size_t get_bytes= 256;
  setbuf(stdout, NULL);
  while(!stop_t2)
  {
    get_bytes= 256;
    if(i == 20)
    {
      sleep(3); // simulate slow IO
    }
    if((bytes = buf_t2->get(buf, get_bytes)))
    {
      fwrite(buf, bytes, 1, stdout);
    }
    i++;
  }

  // flush remaining logs
  char* flush = (char*)malloc(buf_t2->getSize());
  bytes = buf_t2->get(flush, buf_t2->getSize());
  fwrite(flush, bytes, 1, stdout);
  free(flush);

  // print lost bytes if any
  size_t lost_count = buf_t2->getLostCount();
  if(lost_count)
  {
    fprintf(stdout, "\n*** %lu BYTES LOST ***\n", (unsigned long)lost_count);
  }

  NdbThread_Exit(NULL);
  return NULL;
}

void* thread_consumer2(void* dummy)
{
  total_bytes_read_t3 = 0;
  char buf[10];
  size_t to_read = 0;
  size_t bytes_read = 0;
  size_t bytes_flushed = 0;

  while(!stop_t3)
  {
    to_read = rand() % 10 + 1;
    bytes_read = buf_t3->get(buf, to_read);
    total_bytes_read_t3 += bytes_read;
    printf("Read: %lu bytes\n", (long unsigned)bytes_read);
  }

  char* flush = (char*)malloc(buf_t3->getSize());
  bytes_flushed = buf_t3->get(flush, buf_t3->getSize());
  total_bytes_read_t3 += bytes_flushed;
  free(flush);

  NdbThread_Exit(NULL);
  return NULL;
}

#include <util/NdbTap.hpp>
TAPTEST(LogBuffer)
{
  ndb_init();

  buf_t1 = new LogBuffer(10);
  buf_t2 = new LogBuffer(512);
  buf_t3 = new LogBuffer(20);
  buf_t4 = new LogBuffer(20);

  printf("--------TESTCASE 1- BORDER CASES--------\n\n");
  size_t bytes = 0;
  const uint bufsize = 11;
  char buf1[bufsize];

  /**
   * Note: There are a few comments below that represent the contents of the LogBuffer
   * at the particular line of code during execution.
   * '*' represents a free byte
   * '#' represents the top of the buffer (empty byte, should never be written by append()/get())
   *  All other characters represent the content of the occupied byte.
   */

  // should return 0 after sleeping for 1s since the log buffer is empty
  // **********#
  bytes = buf_t1->get(buf1, 5, 1000);
  // **********#
  OK(bytes == 0);
  clearbuf(buf1, bufsize);
  printf("Sub-test 1 OK\n");


  // **********#
  OK(buf_t1->append((void*)"123", 3) == 3);
  // 123*******#
  // should return 3 immediately
  bytes = buf_t1->get(buf1, 5, 1000);
  // **********#
  OK(bytes == 3);
  buf1[bytes] = '\0';
  OK(strcmp(buf1, "123") == 0);
  clearbuf(buf1, bufsize);
  printf("Sub-test 2 OK\n");


  va_list empty_ap;
  // append string of max. length that the log buffer can hold
  // **********#
  OK(buf_t1->append("123456789", empty_ap, 9) == 9);
  // 123456789*#
  bytes = buf_t1->get(buf1, 10);
  // **********#
  OK(bytes == 9);
  buf1[bytes] = '\0';
  OK(strcmp("123456789", buf1) == 0);
  printf("Sub-test 3 OK\n");
  clearbuf(buf1, bufsize);


  // **********#
  OK(buf_t1->append((void*)"01234", 5) == 5); // w == r, empty logbuf
  // 01234*****#
  OK(buf_t1->append((void*)"56789", 5) == 5); // w > r, no-wrap
  // 0123456789#
  buf_t1->get(buf1, 5); // read in one go, w < r
  // *****56789#
  OK(buf_t1->append((void*)"01234", 5) == 5); // w < r
  // 0123456789#
  clearbuf(buf1, bufsize);
  bytes = buf_t1->get(buf1, 3); // read in one go, w == r
  // 01234***89#
  buf1[bytes] = '\0';
  OK(strcmp("567", buf1) == 0);
  bytes = buf_t1->get(buf1, 10);// read in parts, empty the log buffer
  // **********#
  buf1[bytes] = '\0';
  OK(strcmp(buf1, "8901234") == 0);
  printf("Sub-test 4 OK\n");
  clearbuf(buf1, bufsize);
  assert(buf_t1->getSize() == 0);


  // **********#
  OK(buf_t1->append((void*)"01234", 5) == 5);
  // 01234*****#
  OK(buf_t1->append((void*)"56789", 5) == 5);
  // 0123456789#
  buf_t1->get(buf1, 5);
  // *****56789#
  OK(buf_t1->append((void*)"01234", 5) == 5);
  // 0123456789#
  clearbuf(buf1, bufsize);
  bytes = buf_t1->get(buf1, 3); // read in one go, w == r
  // 01234***89#
  buf1[bytes] = '\0';
  OK(strcmp("567", buf1) == 0);
  bytes = buf_t1->get(buf1, 2);// read in parts, empty the log buffer
  // 01234*****#
  buf1[bytes] = '\0';
  OK(strcmp(buf1, "89") == 0);
  bytes = buf_t1->get(buf1, 3);
  // **34*****#
  buf1[bytes] = '\0';
  OK(strcmp(buf1, "012") == 0); // read in one go, w > r
  bytes = buf_t1->get(buf1, 3);
  // **********#
  buf1[bytes] = '\0';
  OK(strcmp(buf1, "34") == 0);
  clearbuf(buf1, bufsize);
  assert(buf_t1->getSize() == 0);
  printf("Sub-test 5 OK\n");


  // **********#
  OK(buf_t1->append("01234567", empty_ap, 8) == 8);
  // 01234567**#
  bytes = buf_t1->get(buf1, 4);
  // ****4567**#
  buf1[bytes] = '\0';
  OK(strcmp(buf1, "0123") == 0);
  OK(buf_t1->append("012", empty_ap, 3) == 3); // w > r, wrap
  // 012*4567**#
  OK(buf_t1->append((void*)"3", 1) == 1); // w < r
  // 01234567**#
  bytes = buf_t1->get(buf1, 10);
  buf1[bytes] = '\0';
  OK(strcmp(buf1, "45670123") == 0);
  // **********#
  clearbuf(buf1, bufsize);
  assert(buf_t1->getSize() == 0);
  printf("Sub-test 6 OK\n");


  //check functionality after reading in parts
  //append string of length = size_of_buf - 1
  // **********#
  OK(buf_t1->append("123456789", empty_ap, 9) == 9);
  // 123456789*#
  bytes = buf_t1->get(buf1, 9);
  OK(bytes == 9);
  // **********#
  buf1[bytes] = '\0';
  OK(strcmp("123456789", buf1) == 0);
  printf("Sub-test 7 OK\n");
  clearbuf(buf1, bufsize);


  // **********#
  OK(buf_t1->append((void*)"012345678", 9) == 9);
  // 012345678*#
  buf_t1->get(buf1, 4);
  // ****45678*#
  OK(buf_t1->append("90a", empty_ap, 3) == 3); // append in the beginning
  // 90a*45678*#
  OK(buf_t1->get(buf1, 8) == 8); // read in parts
  // **********#
  buf1[8] = '\0';
  OK(strcmp(buf1, "4567890a") == 0);
  OK(buf_t1->append((void*)"123", 0) == 0); // length zero
  OK(buf_t1->append("123", empty_ap, 0) == 0); // length zero
  assert(buf_t1->getSize() == 0);
  printf("Sub-test 8 OK\n");
  clearbuf(buf1, bufsize);


  // **********#
  buf_t1->append((void*)"01234", 5);
  // 01234*****#
  buf_t1->append((void*)"56789", 5);
  // 0123456789#
  OK(buf_t1->append("will fail", empty_ap, 9) == 0); // full log buffer
  // 0123456789#
  OK(buf_t1->append("will fail", empty_ap, 9) == 0); // ,,
  // 0123456789#
  OK(buf_t1->getLostCount() == 18);
  clearbuf(buf1, bufsize);

  printf("Sub-test 9 OK\n");

  printf("\n--------TESTCASE 1 COMPLETE--------\n\n");;


  printf("--------TESTCASE 2- TWO PRODUCERS, ONE CONSUMER--------\n\n");
  struct NdbThread* log_threadvar1;
  struct NdbThread* prod_threadvar1;
  struct NdbThread* prod_threadvar2;
  prod_threadvar1 = NdbThread_Create(thread_producer1,
                       (void**)NULL,
                       0,
                       (char*)"thread_test1",
                       NDB_THREAD_PRIO_MEAN);

  prod_threadvar2 = NdbThread_Create(thread_producer2,
                         (void**)NULL,
                         0,
                         (char*)"thread_test2",
                         NDB_THREAD_PRIO_MEAN);

  log_threadvar1 = NdbThread_Create(thread_consumer1,
                     (void**)NULL,
                     0,
                     (char*)"thread_io1",
                     NDB_THREAD_PRIO_MEAN);

  NdbThread_WaitFor(prod_threadvar1, NULL);
  NdbThread_WaitFor(prod_threadvar2, NULL);
  stop_t2 = true;
  NdbThread_WaitFor(log_threadvar1, NULL);

  NdbThread_Destroy(&log_threadvar1);
  NdbThread_Destroy(&prod_threadvar1);
  NdbThread_Destroy(&prod_threadvar2);

  printf("\n--------TESTCASE 2 COMPLETE--------\n\n");

  printf("--------TESTCASE 3- RANDOM READS & WRITES--------\n\n");

  struct NdbThread* log_threadvar2;
  struct NdbThread* prod_threadvar3;
  prod_threadvar3 = NdbThread_Create(thread_producer3,
                         (void**)NULL,
                         0,
                         (char*)"thread_test3",
                         NDB_THREAD_PRIO_MEAN);

  log_threadvar2 = NdbThread_Create(thread_consumer2,
                     (void**)NULL,
                     0,
                     (char*)"thread_io2",
                     NDB_THREAD_PRIO_MEAN);
  NdbThread_WaitFor(prod_threadvar3, NULL);
  stop_t3 = true;
  NdbThread_WaitFor(log_threadvar2, NULL);
  printf("Total bytes to have been written = %d\n", total_to_write_t3);
  printf("Total bytes written successfully = %d\n", bytes_written_t3);
  printf("Total bytes lost = %d\n", bytes_lost_t3);
  printf("Total bytes read = %d\n", total_bytes_read_t3);
  assert(bytes_written_t3 == total_bytes_read_t3);
  if(bytes_lost_t3 == 0)
  {
    assert(total_to_write_t3 == bytes_written_t3);
  }

  NdbThread_Destroy(&log_threadvar2);
  NdbThread_Destroy(&prod_threadvar3);

  printf("\n--------TESTCASE 3 COMPLETE--------\n\n");

  delete buf_t1;
  delete buf_t2;
  delete buf_t3;
  delete buf_t4;

  ndb_end(0);
  return 1;
}

#endif
