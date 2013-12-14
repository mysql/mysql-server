/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

/**
  @file

  This file is the net layer API for the MySQL client/server protocol.

  Write and read of logical packets to/from socket.

  Writes are cached into net_buffer_length big packets.
  Read packets are reallocated dynamicly when reading big packets.
  Each logical packet has the following pre-info:
  3 byte length & 1 byte package-number.

  This file needs to be written in C as it's used by the libmysql client as a
  C file.
*/

/*
  HFTODO this must be hidden if we don't want client capabilities in 
  embedded library
 */
#include <my_global.h>
#include <mysql.h>
#include <mysql_com.h>
#include <mysqld_error.h>
#include <my_sys.h>
#include <m_string.h>
#include <my_net.h>
#include <violite.h>
#include <signal.h>
#include <errno.h>
#include "probes_mysql.h"

#include <algorithm>

using std::min;
using std::max;

#ifdef EMBEDDED_LIBRARY
#undef MYSQL_SERVER
#undef MYSQL_CLIENT
#define MYSQL_CLIENT
#endif /*EMBEDDED_LIBRARY */


/*
  The following handles the differences when this is linked between the
  client and the server.

  This gives an error if a too big packet is found.
  The server can change this, but because the client can't normally do this
  the client should have a bigger max_allowed_packet.
*/

#ifdef MYSQL_SERVER
/*
  The following variables/functions should really not be declared
  extern, but as it's hard to include sql_priv.h here, we have to
  live with this for a while.
*/
#ifdef HAVE_QUERY_CACHE
#define USE_QUERY_CACHE
extern void query_cache_insert(const char *packet, ulong length,
                               unsigned pkt_nr);
#endif /* HAVE_QUERY_CACHE */
#define update_statistics(A) A
#else /* MYSQL_SERVER */
#define update_statistics(A)
#define thd_increment_bytes_sent(N)
#endif

#ifdef MYSQL_SERVER
/* Additional instrumentation hooks for the server */
#include "mysql_com_server.h"
#endif

#define VIO_SOCKET_ERROR  ((size_t) -1)
#define MAX_PACKET_LENGTH (256L*256L*256L-1)

static my_bool net_write_buff(NET *, const uchar *, ulong);

/** Init with packet info. */

my_bool my_net_init(NET *net, Vio* vio)
{
  DBUG_ENTER("my_net_init");
  net->vio = vio;
  my_net_local_init(net);			/* Set some limits */
  if (!(net->buff=(uchar*) my_malloc((size_t) net->max_packet+
             NET_HEADER_SIZE + COMP_HEADER_SIZE,
             MYF(MY_WME))))
    DBUG_RETURN(1);
  net->buff_end=net->buff+net->max_packet;
  net->error=0; net->return_status=0;
  net->pkt_nr=net->compress_pkt_nr=0;
  net->write_pos=net->read_pos = net->buff;
  net->last_error[0]=0;
  net->compress=0; net->reading_or_writing=0;
  net->where_b = net->remain_in_buf=0;
  net->last_errno=0;
  net->unused= 0;
#ifdef MYSQL_SERVER
  net->extension= NULL;
#endif

  if (vio)
  {
    /* For perl DBI/DBD. */
    net->fd= vio_fd(vio);
    vio_fastsend(vio);
  }
  DBUG_RETURN(0);
}


void net_end(NET *net)
{
  DBUG_ENTER("net_end");
  my_free(net->buff);
  net->buff=0;
  DBUG_VOID_RETURN;
}


/** Realloc the packet buffer. */

my_bool net_realloc(NET *net, size_t length)
{
  uchar *buff;
  size_t pkt_length;
  DBUG_ENTER("net_realloc");
  DBUG_PRINT("enter",("length: %lu", (ulong) length));

  if (length >= net->max_packet_size)
  {
    DBUG_PRINT("error", ("Packet too large. Max size: %lu",
                         net->max_packet_size));
    /* @todo: 1 and 2 codes are identical. */
    net->error= 1;
    net->last_errno= ER_NET_PACKET_TOO_LARGE;
#ifdef MYSQL_SERVER
    my_error(ER_NET_PACKET_TOO_LARGE, MYF(0));
#endif
    DBUG_RETURN(1);
  }
  pkt_length = (length+IO_SIZE-1) & ~(IO_SIZE-1); 
  /*
    We must allocate some extra bytes for the end 0 and to be able to
    read big compressed blocks + 1 safety byte since uint3korr() in
    net_read_packet() may actually read 4 bytes depending on build flags and
    platform.
  */
  if (!(buff= (uchar*) my_realloc((char*) net->buff, pkt_length +
                                  NET_HEADER_SIZE + COMP_HEADER_SIZE + 1,
                                  MYF(MY_WME))))
  {
    /* @todo: 1 and 2 codes are identical. */
    net->error= 1;
    net->last_errno= ER_OUT_OF_RESOURCES;
    /* In the server the error is reported by MY_WME flag. */
    DBUG_RETURN(1);
  }
  net->buff=net->write_pos=buff;
  net->buff_end=buff+(net->max_packet= (ulong) pkt_length);
  DBUG_RETURN(0);
}


/**
  Clear (reinitialize) the NET structure for a new command.

  @remark Performs debug checking of the socket buffer to
          ensure that the protocol sequence is correct.

  @param net          NET handler
  @param check_buffer  Whether to check the socket buffer.
*/

void net_clear(NET *net,
               my_bool check_buffer __attribute__((unused)))
{
  DBUG_ENTER("net_clear");

#if !defined(EMBEDDED_LIBRARY)
  /* Ensure the socket buffer is empty, except for an EOF (at least 1). */
  DBUG_ASSERT(!check_buffer || (vio_pending(net->vio) <= 1));
#endif

  /* Ready for new command */
  net->pkt_nr= net->compress_pkt_nr= 0;
  net->write_pos= net->buff;

  DBUG_VOID_RETURN;
}


/** Flush write_buffer if not empty. */

my_bool net_flush(NET *net)
{
  my_bool error= 0;
  DBUG_ENTER("net_flush");
  if (net->buff != net->write_pos)
  {
    error= net_write_packet(net, net->buff,
                            (size_t) (net->write_pos - net->buff));
    net->write_pos= net->buff;
  }
  /* Sync packet number if using compression */
  if (net->compress)
    net->pkt_nr=net->compress_pkt_nr;
  DBUG_RETURN(error);
}


/**
  Whether a I/O operation should be retried later.

  @param net          NET handler.
  @param retry_count  Maximum number of interrupted operations.

  @retval TRUE    Operation should be retried.
  @retval FALSE   Operation should not be retried. Fatal error.
*/

static my_bool
net_should_retry(NET *net, uint *retry_count __attribute__((unused)))
{
  my_bool retry;

#if !defined(MYSQL_SERVER) && defined(THREAD_SAFE_CLIENT)
  /*
    In the thread safe client library, interrupted I/O operations
    are always retried.  Otherwise, its either a timeout or a
    unrecoverable error.
  */
  retry= vio_should_retry(net->vio);
#else
  /*
    In the non-thread safe client library, or in the server,
    interrupted I/O operations are retried up to a limit.
    In this scenario, pthread_kill can be used to wake up
    (interrupt) threads waiting for I/O.
  */
  retry= vio_should_retry(net->vio) && ((*retry_count)++ < net->retry_count);
#endif

  return retry;
}


/*****************************************************************************
** Write something to server/client buffer
*****************************************************************************/

/**
  Write a logical packet with packet header.

  Format: Packet length (3 bytes), packet number (1 byte)
  When compression is used, a 3 byte compression length is added.

  @note If compression is used, the original packet is modified!
*/

my_bool my_net_write(NET *net, const uchar *packet, size_t len)
{
  uchar buff[NET_HEADER_SIZE];
  int rc;

  if (unlikely(!net->vio)) /* nowhere to write */
    return 0;

  MYSQL_NET_WRITE_START(len);

  DBUG_EXECUTE_IF("simulate_net_write_failure", {
                  my_error(ER_NET_ERROR_ON_WRITE, MYF(0));
                  return 1;
                  };
                 );

  /*
    Big packets are handled by splitting them in packets of MAX_PACKET_LENGTH
    length. The last packet is always a packet that is < MAX_PACKET_LENGTH.
    (The last packet may even have a length of 0)
  */
  while (len >= MAX_PACKET_LENGTH)
  {
    const ulong z_size = MAX_PACKET_LENGTH;
    int3store(buff, z_size);
    buff[3]= (uchar) net->pkt_nr++;
    if (net_write_buff(net, buff, NET_HEADER_SIZE) ||
        net_write_buff(net, packet, z_size))
    {
      MYSQL_NET_WRITE_DONE(1);
      return 1;
    }
    packet += z_size;
    len-=     z_size;
  }
  /* Write last packet */
  int3store(buff,len);
  buff[3]= (uchar) net->pkt_nr++;
  if (net_write_buff(net, buff, NET_HEADER_SIZE))
  {
    MYSQL_NET_WRITE_DONE(1);
    return 1;
  }
#ifndef DEBUG_DATA_PACKETS
  DBUG_DUMP("packet_header", buff, NET_HEADER_SIZE);
#endif
  rc= MY_TEST(net_write_buff(net,packet,len));
  MYSQL_NET_WRITE_DONE(rc);
  return rc;
}


/**
  Send a command to the server.

    The reason for having both header and packet is so that libmysql
    can easy add a header to a special command (like prepared statements)
    without having to re-alloc the string.

    As the command is part of the first data packet, we have to do some data
    juggling to put the command in there, without having to create a new
    packet.
  
    This function will split big packets into sub-packets if needed.
    (Each sub packet can only be 2^24 bytes)

  @param net		NET handler
  @param command	Command in MySQL server (enum enum_server_command)
  @param header	Header to write after command
  @param head_len	Length of header
  @param packet	Query or parameter to query
  @param len		Length of packet

  @retval
    0	ok
  @retval
    1	error
*/

my_bool
net_write_command(NET *net,uchar command,
      const uchar *header, size_t head_len,
      const uchar *packet, size_t len)
{
  size_t length=len+1+head_len;			/* 1 extra byte for command */
  uchar buff[NET_HEADER_SIZE+1];
  uint header_size=NET_HEADER_SIZE+1;
  int rc;
  DBUG_ENTER("net_write_command");
  DBUG_PRINT("enter",("length: %lu", (ulong) len));

  MYSQL_NET_WRITE_START(length);

  buff[4]=command;				/* For first packet */

  if (length >= MAX_PACKET_LENGTH)
  {
    /* Take into account that we have the command in the first header */
    len= MAX_PACKET_LENGTH - 1 - head_len;
    do
    {
      int3store(buff, MAX_PACKET_LENGTH);
      buff[3]= (uchar) net->pkt_nr++;
      if (net_write_buff(net, buff, header_size) ||
          net_write_buff(net, header, head_len) ||
          net_write_buff(net, packet, len))
      {
        MYSQL_NET_WRITE_DONE(1);
        DBUG_RETURN(1);
      }
      packet+= len;
      length-= MAX_PACKET_LENGTH;
      len= MAX_PACKET_LENGTH;
      head_len= 0;
      header_size= NET_HEADER_SIZE;
    } while (length >= MAX_PACKET_LENGTH);
    len=length;         /* Data left to be written */
  }
  int3store(buff,length);
  buff[3]= (uchar) net->pkt_nr++;
  rc= MY_TEST(net_write_buff(net, buff, header_size) ||
              (head_len && net_write_buff(net, header, head_len)) ||
              net_write_buff(net, packet, len) || net_flush(net));
  MYSQL_NET_WRITE_DONE(rc);
  DBUG_RETURN(rc);
}


/**
  Caching the data in a local buffer before sending it.

   Fill up net->buffer and send it to the client when full.

    If the rest of the to-be-sent-packet is bigger than buffer,
    send it in one big block (to avoid copying to internal buffer).
    If not, copy the rest of the data to the buffer and return without
    sending data.

  @param net		Network handler
  @param packet	Packet to send
  @param len		Length of packet

  @note
    The cached buffer can be sent as it is with 'net_flush()'.
    In this code we have to be careful to not send a packet longer than
    MAX_PACKET_LENGTH to net_write_packet() if we are using the compressed
    protocol as we store the length of the compressed packet in 3 bytes.

  @retval
    0	ok
  @retval
    1
*/

static my_bool
net_write_buff(NET *net, const uchar *packet, ulong len)
{
  ulong left_length;
  if (net->compress && net->max_packet > MAX_PACKET_LENGTH)
    left_length= (ulong) (MAX_PACKET_LENGTH - (net->write_pos - net->buff));
  else
    left_length= (ulong) (net->buff_end - net->write_pos);

#ifdef DEBUG_DATA_PACKETS
  DBUG_DUMP("data", packet, len);
#endif
  if (len > left_length)
  {
    if (net->write_pos != net->buff)
    {
      /* Fill up already used packet and write it */
      memcpy(net->write_pos, packet, left_length);
      if (net_write_packet(net, net->buff,
                           (size_t) (net->write_pos - net->buff) + left_length))
        return 1;
      net->write_pos= net->buff;
      packet+= left_length;
      len-= left_length;
    }
    if (net->compress)
    {
      /*
        We can't have bigger packets than 16M with compression
        Because the uncompressed length is stored in 3 bytes
      */
      left_length= MAX_PACKET_LENGTH;
      while (len > left_length)
      {
        if (net_write_packet(net, packet, left_length))
          return 1;
        packet+= left_length;
        len-= left_length;
      }
    }
    if (len > net->max_packet)
      return net_write_packet(net, packet, len);
    /* Send out rest of the blocks as full sized blocks */
  }
  memcpy(net->write_pos, packet, len);
  net->write_pos+= len;
  return 0;
}


/**
  Write a determined number of bytes to a network handler.

  @param  net     NET handler.
  @param  buf     Buffer containing the data to be written.
  @param  count   The length, in bytes, of the buffer.

  @return TRUE on error, FALSE on success.
*/

static my_bool
net_write_raw_loop(NET *net, const uchar *buf, size_t count)
{
  unsigned int retry_count= 0;

  while (count)
  {
    size_t sentcnt= vio_write(net->vio, buf, count);

    /* VIO_SOCKET_ERROR (-1) indicates an error. */
    if (sentcnt == VIO_SOCKET_ERROR)
    {
      /* A recoverable I/O error occurred? */
      if (net_should_retry(net, &retry_count))
        continue;
      else
        break;
    }

    count-= sentcnt;
    buf+= sentcnt;
    update_statistics(thd_increment_bytes_sent(sentcnt));
  }

  /* On failure, propagate the error code. */
  if (count)
  {
    /* Socket should be closed. */
    net->error= 2;

    /* Interrupted by a timeout? */
    if (vio_was_timeout(net->vio))
      net->last_errno= ER_NET_WRITE_INTERRUPTED;
    else
      net->last_errno= ER_NET_ERROR_ON_WRITE;

#ifdef MYSQL_SERVER
    my_error(net->last_errno, MYF(0));
#endif
  }

  return MY_TEST(count);
}


/**
  Compress and encapsulate a packet into a compressed packet.

  @param          net      NET handler.
  @param          packet   The packet to compress.
  @param[in,out]  length   Length of the packet.

  A compressed packet header is compromised of the packet
  length (3 bytes), packet number (1 byte) and the length
  of the original (uncompressed) packet.

  @return Pointer to the (new) compressed packet.
*/

static uchar *
compress_packet(NET *net, const uchar *packet, size_t *length)
{
  uchar *compr_packet;
  size_t compr_length;
  const uint header_length= NET_HEADER_SIZE + COMP_HEADER_SIZE;

  compr_packet= (uchar *) my_malloc(*length + header_length, MYF(MY_WME));

  if (compr_packet == NULL)
    return NULL;

  memcpy(compr_packet + header_length, packet, *length);

  /* Compress the encapsulated packet. */
  if (my_compress(compr_packet + header_length, length, &compr_length))
  {
    /*
      If the length of the compressed packet is larger than the
      original packet, the original packet is sent uncompressed.
    */
    compr_length= 0;
  }

  /* Length of the compressed (original) packet. */
  int3store(&compr_packet[NET_HEADER_SIZE], compr_length);
  /* Length of this packet. */
  int3store(compr_packet, *length);
  /* Packet number. */
  compr_packet[3]= (uchar) (net->compress_pkt_nr++);

  *length+= header_length;

  return compr_packet;
}


/**
  Write a MySQL protocol packet to the network handler.

  @param  net     NET handler.
  @param  packet  The packet to write.
  @param  length  Length of the packet.

  @remark The packet might be encapsulated into a compressed packet.

  @return TRUE on error, FALSE on success.
*/

my_bool
net_write_packet(NET *net, const uchar *packet, size_t length)
{
  my_bool res;
  DBUG_ENTER("net_write_packet");

#if defined(MYSQL_SERVER) && defined(USE_QUERY_CACHE)
  query_cache_insert((char*) packet, length, net->pkt_nr);
#endif

  /* Socket can't be used */
  if (net->error == 2)
    DBUG_RETURN(TRUE);

  net->reading_or_writing= 2;

#ifdef HAVE_COMPRESS
  const bool do_compress= net->compress;
  if (do_compress)
  {
    if ((packet= compress_packet(net, packet, &length)) == NULL)
    {
      net->error= 2;
      net->last_errno= ER_OUT_OF_RESOURCES;
      /* In the server, allocation failure raises a error. */
      net->reading_or_writing= 0;
      DBUG_RETURN(TRUE);
    }
  }
#endif /* HAVE_COMPRESS */

#ifdef DEBUG_DATA_PACKETS
  DBUG_DUMP("data", packet, length);
#endif

  res= net_write_raw_loop(net, packet, length);

#ifdef HAVE_COMPRESS
  if (do_compress)
    my_free((void *) packet);
#endif

  net->reading_or_writing= 0;

  DBUG_RETURN(res);
}

/*****************************************************************************
** Read something from server/clinet
*****************************************************************************/

/**
  Read a determined number of bytes from a network handler.

  @param  net     NET handler.
  @param  count   The number of bytes to read.

  @return TRUE on error, FALSE on success.
*/

static my_bool net_read_raw_loop(NET *net, size_t count)
{
  bool eof= false;
  unsigned int retry_count= 0;
  uchar *buf= net->buff + net->where_b;

  while (count)
  {
    size_t recvcnt= vio_read(net->vio, buf, count);

    /* VIO_SOCKET_ERROR (-1) indicates an error. */
    if (recvcnt == VIO_SOCKET_ERROR)
    {
      /* A recoverable I/O error occurred? */
      if (net_should_retry(net, &retry_count))
        continue;
      else
        break;
    }
    /* Zero indicates end of file. */
    else if (!recvcnt)
    {
      eof= true;
      break;
    }

    count-= recvcnt;
    buf+= recvcnt;
    update_statistics(thd_increment_bytes_received(recvcnt));
  }

  /* On failure, propagate the error code. */
  if (count)
  {
    /* Socket should be closed. */
    net->error= 2;

    /* Interrupted by a timeout? */
    if (!eof && vio_was_timeout(net->vio))
      net->last_errno= ER_NET_READ_INTERRUPTED;
    else
      net->last_errno= ER_NET_READ_ERROR;

#ifdef MYSQL_SERVER
    my_error(net->last_errno, MYF(0));
#endif
  }

  return MY_TEST(count);
}


/**
  Read the header of a packet. The MySQL protocol packet header
  consists of the length, in bytes, of the payload (packet data)
  and a serial number.

  @remark The encoded length is the length of the packet payload,
          which does not include the packet header.

  @remark The serial number is used to ensure that the packets are
          received in order. If the packet serial number does not
          match the expected value, a error is returned.

  @param  net  NET handler.

  @return TRUE on error, FALSE on success.
*/

static my_bool net_read_packet_header(NET *net)
{
  uchar pkt_nr;
  size_t count= NET_HEADER_SIZE;
  my_bool rc;

  if (net->compress)
    count+= COMP_HEADER_SIZE;

#ifdef MYSQL_SERVER
  struct st_net_server *server_extension;

  server_extension= static_cast<st_net_server*> (net->extension);

  if (server_extension != NULL)
  {
    void *user_data= server_extension->m_user_data;
    DBUG_ASSERT(server_extension->m_before_header != NULL);
    DBUG_ASSERT(server_extension->m_after_header != NULL);

    server_extension->m_before_header(net, user_data, count);
    rc= net_read_raw_loop(net, count);
    server_extension->m_after_header(net, user_data, count, rc);
  }
  else
#endif
  {
    rc= net_read_raw_loop(net, count);
  }

  if (rc)
    return TRUE;

  DBUG_DUMP("packet_header", net->buff + net->where_b, NET_HEADER_SIZE);

  pkt_nr= net->buff[net->where_b + 3];

  /*
    Verify packet serial number against the truncated packet counter.
    The local packet counter must be truncated since its not reset.
  */
  if (pkt_nr != (uchar) net->pkt_nr)
  {
    /* Not a NET error on the client. XXX: why? */
#if defined(MYSQL_SERVER)
    my_error(ER_NET_PACKETS_OUT_OF_ORDER, MYF(0));
#elif defined(EXTRA_DEBUG)
    /*
      We don't make noise server side, since the client is expected
      to break the protocol for e.g. --send LOAD DATA .. LOCAL where
      the server expects the client to send a file, but the client
      may reply with a new command instead.
    */
    fprintf(stderr, "Error: packets out of order (found %u, expected %u)\n",
            (uint) pkt_nr, net->pkt_nr);
    DBUG_ASSERT(pkt_nr == net->pkt_nr);
#endif
    return TRUE;
  }

  net->pkt_nr++;

  return FALSE;
}


/**
  Read one (variable-length) MySQL protocol packet.
  A MySQL packet consists of a header and a payload.

  @remark Reads one packet to net->buff + net->where_b.
  @remark Long packets are handled by my_net_read().
  @remark The network buffer is expanded if necessary.

  @return The length of the packet, or @packet_error on error.
*/

static ulong net_read_packet(NET *net, size_t *complen)
{
  size_t pkt_len, pkt_data_len;

  *complen= 0;

  net->reading_or_writing= 1;

  /* Retrieve packet length and number. */
  if (net_read_packet_header(net))
    goto error;

  net->compress_pkt_nr= net->pkt_nr;

#ifdef HAVE_COMPRESS
  if (net->compress)
  {
    /*
      The following uint3korr() may read 4 bytes, so make sure we don't
      read unallocated or uninitialized memory. The right-hand expression
      must match the size of the buffer allocated in net_realloc().
    */
    DBUG_ASSERT(net->where_b + NET_HEADER_SIZE + sizeof(uint32) <=
                net->max_packet + NET_HEADER_SIZE + COMP_HEADER_SIZE + 1);

    /*
      If the packet is compressed then complen > 0 and contains the
      number of bytes in the uncompressed packet.
    */
    *complen= uint3korr(&(net->buff[net->where_b + NET_HEADER_SIZE]));
  }
#endif

  /* The length of the packet that follows. */
  pkt_len= uint3korr(net->buff+net->where_b);

  /* End of big multi-packet. */
  if (!pkt_len)
    goto end;

  pkt_data_len = max(pkt_len, *complen) + net->where_b;

  /* Expand packet buffer if necessary. */
  if ((pkt_data_len >= net->max_packet) && net_realloc(net, pkt_data_len))
    goto error;

  /* Read the packet data (payload). */
  if (net_read_raw_loop(net, pkt_len))
    goto error;

end:
  net->reading_or_writing= 0;
  return pkt_len;

error:
  net->reading_or_writing= 0;
  return packet_error;
}


/**
  Read a packet from the client/server and return it without the internal
  package header.

  If the packet is the first packet of a multi-packet packet
  (which is indicated by the length of the packet = 0xffffff) then
  all sub packets are read and concatenated.

  If the packet was compressed, its uncompressed and the length of the
  uncompressed packet is returned.

  @return
  The function returns the length of the found packet or packet_error.
  net->read_pos points to the read data.
*/

ulong
my_net_read(NET *net)
{
  size_t len, complen;

  MYSQL_NET_READ_START();

#ifdef HAVE_COMPRESS
  if (!net->compress)
  {
#endif
    len= net_read_packet(net, &complen);
    if (len == MAX_PACKET_LENGTH)
    {
      /* First packet of a multi-packet.  Concatenate the packets */
      ulong save_pos = net->where_b;
      size_t total_length= 0;
      do
      {
        net->where_b += len;
        total_length += len;
        len= net_read_packet(net, &complen);
      } while (len == MAX_PACKET_LENGTH);
      if (len != packet_error)
        len+= total_length;
      net->where_b = save_pos;
    }
    net->read_pos = net->buff + net->where_b;
    if (len != packet_error)
      net->read_pos[len]=0;		/* Safeguard for mysql_use_result */
    MYSQL_NET_READ_DONE(0, len);
    return len;
#ifdef HAVE_COMPRESS
  }
  else
  {
    /* We are using the compressed protocol */

    ulong buf_length;
    ulong start_of_packet;
    ulong first_packet_offset;
    uint read_length, multi_byte_packet=0;

    if (net->remain_in_buf)
    {
      buf_length= net->buf_length;		/* Data left in old packet */
      first_packet_offset= start_of_packet= (net->buf_length -
                                             net->remain_in_buf);
      /* Restore the character that was overwritten by the end 0 */
      net->buff[start_of_packet]= net->save_char;
    }
    else
    {
      /* reuse buffer, as there is nothing in it that we need */
      buf_length= start_of_packet= first_packet_offset= 0;
    }
    for (;;)
    {
      ulong packet_len;

      if (buf_length - start_of_packet >= NET_HEADER_SIZE)
      {
        read_length = uint3korr(net->buff+start_of_packet);
        if (!read_length)
        { 
          /* End of multi-byte packet */
          start_of_packet += NET_HEADER_SIZE;
          break;
        }
        if (read_length + NET_HEADER_SIZE <= buf_length - start_of_packet)
        {
          if (multi_byte_packet)
          {
            /* Remove packet header for second packet */
            memmove(net->buff + first_packet_offset + start_of_packet,
              net->buff + first_packet_offset + start_of_packet +
              NET_HEADER_SIZE,
              buf_length - start_of_packet);
            start_of_packet += read_length;
            buf_length -= NET_HEADER_SIZE;
          }
          else
            start_of_packet+= read_length + NET_HEADER_SIZE;

          if (read_length != MAX_PACKET_LENGTH)	/* last package */
          {
            multi_byte_packet= 0;		/* No last zero len packet */
            break;
          }
          multi_byte_packet= NET_HEADER_SIZE;
          /* Move data down to read next data packet after current one */
          if (first_packet_offset)
          {
            memmove(net->buff,net->buff+first_packet_offset,
              buf_length-first_packet_offset);
            buf_length-=first_packet_offset;
            start_of_packet -= first_packet_offset;
            first_packet_offset=0;
          }
          continue;
        }
      }
      /* Move data down to read next data packet after current one */
      if (first_packet_offset)
      {
        memmove(net->buff,net->buff+first_packet_offset,
          buf_length-first_packet_offset);
        buf_length-=first_packet_offset;
        start_of_packet -= first_packet_offset;
        first_packet_offset=0;
      }

      net->where_b=buf_length;
      if ((packet_len= net_read_packet(net, &complen)) == packet_error)
      {
        MYSQL_NET_READ_DONE(1, 0);
        return packet_error;
      }
      if (my_uncompress(net->buff + net->where_b, packet_len,
                        &complen))
      {
        net->error= 2;			/* caller will close socket */
        net->last_errno= ER_NET_UNCOMPRESS_ERROR;
#ifdef MYSQL_SERVER
        my_error(ER_NET_UNCOMPRESS_ERROR, MYF(0));
#endif
        MYSQL_NET_READ_DONE(1, 0);
        return packet_error;
      }
      buf_length+= complen;
    }

    net->read_pos=      net->buff+ first_packet_offset + NET_HEADER_SIZE;
    net->buf_length=    buf_length;
    net->remain_in_buf= (ulong) (buf_length - start_of_packet);
    len = ((ulong) (start_of_packet - first_packet_offset) - NET_HEADER_SIZE -
           multi_byte_packet);
    net->save_char= net->read_pos[len];	/* Must be saved */
    net->read_pos[len]=0;		/* Safeguard for mysql_use_result */
  }
#endif /* HAVE_COMPRESS */
  MYSQL_NET_READ_DONE(0, len);
  return len;
}


void my_net_set_read_timeout(NET *net, uint timeout)
{
  DBUG_ENTER("my_net_set_read_timeout");
  DBUG_PRINT("enter", ("timeout: %d", timeout));
  net->read_timeout= timeout;
  if (net->vio)
    vio_timeout(net->vio, 0, timeout);
  DBUG_VOID_RETURN;
}


void my_net_set_write_timeout(NET *net, uint timeout)
{
  DBUG_ENTER("my_net_set_write_timeout");
  DBUG_PRINT("enter", ("timeout: %d", timeout));
  net->write_timeout= timeout;
  if (net->vio)
    vio_timeout(net->vio, 1, timeout);
  DBUG_VOID_RETURN;
}

