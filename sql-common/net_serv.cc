/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file

  This file is the net layer API for the MySQL client/server protocol.

  Write and read of logical packets to/from socket.

  Writes are cached into net_buffer_length big packets.
  Read packets are reallocated dynamicly when reading big packets.
  Each logical packet has the following pre-info:
  3 byte length & 1 byte package-number.
*/

#include <string.h>
#include <sys/types.h>
#include <algorithm>

#include "my_byteorder.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_sys.h"
#include "mysql.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "violite.h"

using std::max;
using std::min;

#ifdef MYSQL_SERVER
#include "sql/psi_memory_key.h"
#else
#define key_memory_NET_buff 0
#define key_memory_NET_compress_packet 0
#endif

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
  extern, but as it's hard to include sql_class.h here, we have to
  live with this for a while.
*/
extern void thd_increment_bytes_sent(size_t length);
extern void thd_increment_bytes_received(size_t length);

/* Additional instrumentation hooks for the server */
#include "mysql_com_server.h"
#endif

#define VIO_SOCKET_ERROR ((size_t)-1)

static bool net_write_buff(NET *, const uchar *, size_t);

/** Init with packet info. */

bool my_net_init(NET *net, Vio *vio) {
  DBUG_ENTER("my_net_init");
  net->vio = vio;
  my_net_local_init(net); /* Set some limits */
  if (!(net->buff = (uchar *)my_malloc(
            key_memory_NET_buff,
            (size_t)net->max_packet + NET_HEADER_SIZE + COMP_HEADER_SIZE,
            MYF(MY_WME))))
    DBUG_RETURN(1);
  net->buff_end = net->buff + net->max_packet;
  net->error = 0;
  net->return_status = 0;
  net->pkt_nr = net->compress_pkt_nr = 0;
  net->write_pos = net->read_pos = net->buff;
  net->last_error[0] = 0;
  net->compress = 0;
  net->reading_or_writing = 0;
  net->where_b = net->remain_in_buf = 0;
  net->last_errno = 0;
#ifdef MYSQL_SERVER
  net->extension = NULL;
#endif

  if (vio) {
    /* For perl DBI/DBD. */
    net->fd = vio_fd(vio);
    vio_fastsend(vio);
  }
  DBUG_RETURN(0);
}

void net_end(NET *net) {
  DBUG_ENTER("net_end");
  my_free(net->buff);
  net->buff = 0;
  DBUG_VOID_RETURN;
}

void net_claim_memory_ownership(NET *net) { my_claim(net->buff); }

/** Realloc the packet buffer. */

bool net_realloc(NET *net, size_t length) {
  uchar *buff;
  size_t pkt_length;
  DBUG_ENTER("net_realloc");
  DBUG_PRINT("enter", ("length: %lu", (ulong)length));

  if (length >= net->max_packet_size) {
    DBUG_PRINT("error",
               ("Packet too large. Max size: %lu", net->max_packet_size));
    /* @todo: 1 and 2 codes are identical. */
    net->error = 1;
    net->last_errno = ER_NET_PACKET_TOO_LARGE;
#ifdef MYSQL_SERVER
    my_error(ER_NET_PACKET_TOO_LARGE, MYF(0));
#endif
    DBUG_RETURN(1);
  }
  pkt_length = (length + IO_SIZE - 1) & ~(IO_SIZE - 1);
  /*
    We must allocate some extra bytes for the end 0 and to be able to
    read big compressed blocks in
    net_read_packet() may actually read 4 bytes depending on build flags and
    platform.
  */
  if (!(buff = (uchar *)my_realloc(
            key_memory_NET_buff, (char *)net->buff,
            pkt_length + NET_HEADER_SIZE + COMP_HEADER_SIZE, MYF(MY_WME)))) {
    /* @todo: 1 and 2 codes are identical. */
    net->error = 1;
    net->last_errno = ER_OUT_OF_RESOURCES;
    /* In the server the error is reported by MY_WME flag. */
    DBUG_RETURN(1);
  }
  net->buff = net->write_pos = buff;
  net->buff_end = buff + (net->max_packet = (ulong)pkt_length);
  DBUG_RETURN(0);
}

/**
  Clear (reinitialize) the NET structure for a new command.

  @remark Performs debug checking of the socket buffer to
          ensure that the protocol sequence is correct.

  @param net          NET handler
  @param check_buffer  Whether to check the socket buffer.
*/

void net_clear(NET *net, bool check_buffer MY_ATTRIBUTE((unused))) {
  DBUG_ENTER("net_clear");

  /* Ensure the socket buffer is empty, except for an EOF (at least 1). */
  DBUG_ASSERT(!check_buffer || (vio_pending(net->vio) <= 1));

  /* Ready for new command */
  net->pkt_nr = net->compress_pkt_nr = 0;
  net->write_pos = net->buff;

  DBUG_VOID_RETURN;
}

/** Flush write_buffer if not empty. */

bool net_flush(NET *net) {
  bool error = 0;
  DBUG_ENTER("net_flush");
  if (net->buff != net->write_pos) {
    error =
        net_write_packet(net, net->buff, (size_t)(net->write_pos - net->buff));
    net->write_pos = net->buff;
  }
  /* Sync packet number if using compression */
  if (net->compress) net->pkt_nr = net->compress_pkt_nr;
  DBUG_RETURN(error);
}

/**
  Whether a I/O operation should be retried later.

  @param net          NET handler.
  @param retry_count  Maximum number of interrupted operations.

  @retval true    Operation should be retried.
  @retval false   Operation should not be retried. Fatal error.
*/

static bool net_should_retry(NET *net,
                             uint *retry_count MY_ATTRIBUTE((unused))) {
  bool retry;

#ifndef MYSQL_SERVER
  /*
    In the  client library, interrupted I/O operations are always retried.
    Otherwise, it's either a timeout or an unrecoverable error.
  */
  retry = vio_should_retry(net->vio);
#else
  /*
    In the server, interrupted I/O operations are retried up to a limit.
    In this scenario, pthread_kill can be used to wake up
    (interrupt) threads waiting for I/O.
  */
  retry = vio_should_retry(net->vio) && ((*retry_count)++ < net->retry_count);
#endif

  return retry;
}

/* clang-format off */
/**
  @page page_protocol_basic_packets MySQL Packets

  If a MySQL client or server wants to send data, it:
  - Splits the data into packets of size 2<sup>24</sup> bytes
  - Prepends to each chunk a packet header

  @section sect_protocol_basic_packets_packet Protocol::Packet

  Data between client and server is exchanged in packets of max 16MByte size.

  <table>
  <caption>Payload</caption>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref a_protocol_type_int1 "int&lt;3&gt;"</td>
      <td>payload_length</td>
      <td>Length of the payload. The number of bytes in the packet beyond
          the initial 4 bytes that make up the packet header.</td></tr>
  <tr><td>@ref a_protocol_type_int1 "int&lt;1&gt;"</td>
      <td>sequence_id</td>
      <td>@ref sect_protocol_basic_packets_sequence_id</td></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_var "string&lt;var&gt;"</td>
      <td>payload</td>
      <td>payload of the packet</td></tr>
  </table>

  Example:

  @todo: Reference COM_QUIT
  A COM_QUIT looks like this:
  <table><tr>
  <td>
  ~~~~~~~~~~~~~~~~~~~~~
  01 00 00 00 01
  ~~~~~~~~~~~~~~~~~~~~~
  </td><td>
  - length: 1
  - sequence_id: x00
  - payload: 0x01
  </td></tr></table>

  @sa my_net_write(), net_write_command(), net_write_buff(), my_net_read(),
  net_send_ok()

  @section sect_protocol_basic_packets_sending_mt_16mb Sending More Than 16Mb

  If the payload is larger than or equal to 2<sup>24</sup>-1 bytes the length
  is set to 2<sup>24</sup>-1 (`ff ff ff`) and a additional packets are sent
  with the rest of the payload until the payload of a packet is less
  than 2<sup>24</sup>-1 bytes.

  Sending a payload of 16 777 215 (2<sup>24</sup>-1) bytes looks like:

  ~~~~~~~~~~~~~~~~
  ff ff ff 00 ...
  00 00 00 01
  ~~~~~~~~~~~~~~~~

  @section sect_protocol_basic_packets_sequence_id Sequence ID

  The sequence-id is incremented with each packet and may wrap around.
  It starts at 0 and is reset to 0 when a new command begins in the
  @ref page_protocol_command_phase.

  @section sect_protocol_basic_packets_describing_packets Describing Packets

  In this document we describe each packet by first defining its payload and
  provide an example showing each packet that is sent, including its packet
  header:
  <pre>
  &lt;packetname&gt;
    &lt;description&gt;

    direction: client -&gt; server
    response: &lt;response&gt;

    payload:
      &lt;type&gt;        &lt;description&gt;
  </pre>

  Example:
  ~~~~~~~~~~~~~~~~~~~~~
  01 00 00 00 01
  ~~~~~~~~~~~~~~~~~~~~~

  @note Some packets have optional fields or a different layout depending on
  the @ref group_cs_capabilities_flags.

  If a field has a fixed value, its description shows it as a hex value in
  brackets like this: `[00]`
*/
/* clang-format on */

/*****************************************************************************
** Write something to server/client buffer
*****************************************************************************/

/**
  Write a logical packet with packet header.

  Format: Packet length (3 bytes), packet number (1 byte)
  When compression is used, a 3 byte compression length is added.

  @note If compression is used, the original packet is modified!
*/

bool my_net_write(NET *net, const uchar *packet, size_t len) {
  uchar buff[NET_HEADER_SIZE];

  if (unlikely(!net->vio)) /* nowhere to write */
    return false;

  DBUG_EXECUTE_IF("simulate_net_write_failure", {
    my_error(ER_NET_ERROR_ON_WRITE, MYF(0));
    return 1;
  };);

  /*
    Big packets are handled by splitting them in packets of MAX_PACKET_LENGTH
    length. The last packet is always a packet that is < MAX_PACKET_LENGTH.
    (The last packet may even have a length of 0)
  */
  while (len >= MAX_PACKET_LENGTH) {
    const ulong z_size = MAX_PACKET_LENGTH;
    int3store(buff, z_size);
    buff[3] = (uchar)net->pkt_nr++;
    if (net_write_buff(net, buff, NET_HEADER_SIZE) ||
        net_write_buff(net, packet, z_size)) {
      return 1;
    }
    packet += z_size;
    len -= z_size;
  }
  /* Write last packet */
  int3store(buff, static_cast<uint>(len));
  buff[3] = (uchar)net->pkt_nr++;
  if (net_write_buff(net, buff, NET_HEADER_SIZE)) {
    return 1;
  }
#ifndef DEBUG_DATA_PACKETS
  DBUG_DUMP("packet_header", buff, NET_HEADER_SIZE);
#endif
  return net_write_buff(net, packet, len);
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

bool net_write_command(NET *net, uchar command, const uchar *header,
                       size_t head_len, const uchar *packet, size_t len) {
  size_t length = len + 1 + head_len; /* 1 extra byte for command */
  uchar buff[NET_HEADER_SIZE + 1];
  uint header_size = NET_HEADER_SIZE + 1;
  DBUG_ENTER("net_write_command");
  DBUG_PRINT("enter", ("length: %lu", (ulong)len));

  buff[4] = command; /* For first packet */

  if (length >= MAX_PACKET_LENGTH) {
    /* Take into account that we have the command in the first header */
    len = MAX_PACKET_LENGTH - 1 - head_len;
    do {
      int3store(buff, MAX_PACKET_LENGTH);
      buff[3] = (uchar)net->pkt_nr++;
      if (net_write_buff(net, buff, header_size) ||
          net_write_buff(net, header, head_len) ||
          net_write_buff(net, packet, len)) {
        DBUG_RETURN(1);
      }
      packet += len;
      length -= MAX_PACKET_LENGTH;
      len = MAX_PACKET_LENGTH;
      head_len = 0;
      header_size = NET_HEADER_SIZE;
    } while (length >= MAX_PACKET_LENGTH);
    len = length; /* Data left to be written */
  }
  int3store(buff, static_cast<uint>(length));
  buff[3] = (uchar)net->pkt_nr++;
  bool rc = net_write_buff(net, buff, header_size) ||
            (head_len && net_write_buff(net, header, head_len)) ||
            net_write_buff(net, packet, len) || net_flush(net);
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

static bool net_write_buff(NET *net, const uchar *packet, size_t len) {
  ulong left_length;
  if (net->compress && net->max_packet > MAX_PACKET_LENGTH)
    left_length = (ulong)(MAX_PACKET_LENGTH - (net->write_pos - net->buff));
  else
    left_length = (ulong)(net->buff_end - net->write_pos);

#ifdef DEBUG_DATA_PACKETS
  DBUG_DUMP("data", packet, len);
#endif
  if (len > left_length) {
    if (net->write_pos != net->buff) {
      /* Fill up already used packet and write it */
      memcpy(net->write_pos, packet, left_length);
      if (net_write_packet(net, net->buff,
                           (size_t)(net->write_pos - net->buff) + left_length))
        return 1;
      net->write_pos = net->buff;
      packet += left_length;
      len -= left_length;
    }
    if (net->compress) {
      /*
        We can't have bigger packets than 16M with compression
        Because the uncompressed length is stored in 3 bytes
      */
      left_length = MAX_PACKET_LENGTH;
      while (len > left_length) {
        if (net_write_packet(net, packet, left_length)) return 1;
        packet += left_length;
        len -= left_length;
      }
    }
    if (len > net->max_packet) return net_write_packet(net, packet, len);
    /* Send out rest of the blocks as full sized blocks */
  }
  if (len > 0) memcpy(net->write_pos, packet, len);
  net->write_pos += len;
  return 0;
}

/**
  Write a determined number of bytes to a network handler.

  @param  net     NET handler.
  @param  buf     Buffer containing the data to be written.
  @param  count   The length, in bytes, of the buffer.

  @return true on error, false on success.
*/

static bool net_write_raw_loop(NET *net, const uchar *buf, size_t count) {
  unsigned int retry_count = 0;

  while (count) {
    size_t sentcnt = vio_write(net->vio, buf, count);

    /* VIO_SOCKET_ERROR (-1) indicates an error. */
    if (sentcnt == VIO_SOCKET_ERROR) {
      /* A recoverable I/O error occurred? */
      if (net_should_retry(net, &retry_count))
        continue;
      else
        break;
    }

    count -= sentcnt;
    buf += sentcnt;
#ifdef MYSQL_SERVER
    thd_increment_bytes_sent(sentcnt);
#endif
  }

  /* On failure, propagate the error code. */
  if (count) {
    /* Socket should be closed. */
    net->error = 2;

    /* Interrupted by a timeout? */
    if (vio_was_timeout(net->vio))
      net->last_errno = ER_NET_WRITE_INTERRUPTED;
    else
      net->last_errno = ER_NET_ERROR_ON_WRITE;

#ifdef MYSQL_SERVER
    my_error(net->last_errno, MYF(0));
#endif
  }

  return count != 0;
}

/* clang-format off */
/**
  @page page_protocol_basic_compression Compression

  Compression is:
    - its own protocol layer
    - transparent to the other MySQL protocol layers
    - compressing a string of bytes (which may even be a part of
      @ref sect_protocol_basic_packets_packet)

  It is enabled if:
    - the server announces ::CLIENT_COMPRESS in its
      @ref page_protocol_connection_phase_packets_protocol_handshake and
    - the client requests it too in its
      @ref page_protocol_connection_phase_packets_protocol_handshake_response
      packet and
    - After the server finishes the @ref page_protocol_connection_phase
      with an @ref page_protocol_basic_ok_packet.

   @subpage page_protocol_basic_compression_packet
*/

/**
  @page page_protocol_basic_compression_packet Compressed Packet

  The compressed packet consists of a @ref sect_protocol_basic_compression_packet_header
  and a payload which is either a @ref sect_protocol_basic_compression_packet_compressed_payload
  or @ref sect_protocol_basic_compression_packet_uncompressed_payload.

  @sa ::compress_packet, ::CLIENT_COMPRESS

  @section sect_protocol_basic_compression_packet_header Compressed Packet Header

  <table>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref a_protocol_type_int3 "int&lt;3&gt;"</td>
  <td>length of compressed payload</td>
  <td>raw packet length minus the size of the compressed packet header
     (7 bytes) itself.</td></tr>
  <tr><td>@ref a_protocol_type_int1 "int&lt;1&gt;"</td>
  <td>compressed sequence id</td>
  <td>Sequence ID of the compressed packets, reset in the same way as the
     @ref sect_protocol_basic_packets_packet, but incremented independently</td></tr>
  </table>

  @section sect_protocol_basic_compression_packet_compressed_payload Compressed Payload

  If the length of *length of payload before compression* is more than 0 the
  @ref sect_protocol_basic_compression_packet_header is followed by the
  compressed payload.

  It uses the *deflate* algorithm as described in
  [RFC 1951](http://tools.ietf.org/html/rfc1951.html) and implemented in
  [zlib](http://zlib.org/). The header of the compressed packet has the
  parameters of the `uncompress()` function in mind:

  ~~~~~~~~~~~~~
  ZEXTERN int ZEXPORT uncompress OF((Bytef *dest,   uLongf *destLen,
                                 const Bytef *source, uLong sourceLen));
  ~~~~~~~~~~~~~

  The payload can be anything from a piece of a MySQL Packet to several
  MySQL Packets. The client or server may bundle several MySQL packets,
  compress it and send it as one compressed packet.

  @subsection sect_protocol_basic_compression_packet_compressed_payload_single Example: One MySQL Packet

  A ::COM_QUERY for `select "012345678901234567890123456789012345"` without
  ::CLIENT_COMPRESS has a *payload length* of 46 bytes and looks like:

  ~~~~~~~~~~~~~
  2e 00 00 00 03 73 65 6c    65 63 74 20 22 30 31 32    .....select "012
  33 34 35 36 37 38 39 30    31 32 33 34 35 36 37 38    3456789012345678
  39 30 31 32 33 34 35 36    37 38 39 30 31 32 33 34    9012345678901234
  35 22                                                 5"
  ~~~~~~~~~~~~~

  With ::CLIENT_COMPRESS the packet is:

  ~~~~~~~~~~~~~
  22 00 00 00 32 00 00 78    9c d3 63 60 60 60 2e 4e    "...2..x..c```.N
  cd 49 4d 2e 51 50 32 30    34 32 36 31 35 33 b7 b0    .IM.QP20426153..
  c4 cd 52 02 00 0c d1 0a    6c                         ..R.....l
  ~~~~~~~~~~~~~

  <table>
  <tr><th>comp-length</th><th>seq-id</th><th>uncomp-len</th><th>Compressed Payload</th></tr>
  <tr><td>`22 00 00`</td><td>`00`</td><td>`32 00 00`</td><td>compress("\x2e\x00\x00\x00\x03select ...")`</td></td>
  </table>

  The compressed packet is 41 bytes long and splits into:

  ~~~~~~~~~~~~~~
  raw packet length                      -> 41
  compressed payload length   = 22 00 00 -> 34 (41 - 7)
  sequence id                 = 00       ->  0
  uncompressed payload length = 32 00 00 -> 50
  ~~~~~~~~~~~~~~

  @subsection sect_protocol_basic_compression_packet_compressed_payload_multi Example: Several MySQL Packets

  Executing `SELECT repeat("a", 50)` results in uncompressed  ProtocolText::Resultset like:
  ~~~~~~~~~~~~~
  01 00 00 01 01 25 00 00    02 03 64 65 66 00 00 00    .....%....def...
  0f 72 65 70 65 61 74 28    22 61 22 2c 20 35 30 29    .repeat("a", 50)
  00 0c 08 00 32 00 00 00    fd 01 00 1f 00 00 05 00    ....2...........
  00 03 fe 00 00 02 00 33    00 00 04 32 61 61 61 61    .......3...2aaaa
  61 61 61 61 61 61 61 61    61 61 61 61 61 61 61 61    aaaaaaaaaaaaaaaa
  61 61 61 61 61 61 61 61    61 61 61 61 61 61 61 61    aaaaaaaaaaaaaaaa
  61 61 61 61 61 61 61 61    61 61 61 61 61 61 05 00    aaaaaaaaaaaaaa..
  00 05 fe 00 00 02 00                                  .......
  ~~~~~~~~~~~~~

  which consists of 5 @ref sect_protocol_basic_packets_packet :

  - `01 00 00 01 01`
  - `25 00 00 02 03 64 65 66 00 00 00 0f 72 65 70 65 61 74 28 22 61 22 2c 20 35 30 29 00 0c 08 00 32 00 00 00 fd 01 00 1f 00 00`
  - `05 00 00 03 fe 00 00 02 00`
  - `33 00 00 04 32 61 61 61 61 ...`
  - `05 00 00 05 fe 00 00 02 00`

  If compression is enabled a compressed packet containing the compressed
  version of all 5 packets is sent to the client:

  ~~~~~~~~~~~~~
  4a 00 00 01 77 00 00 78    9c 63 64 60 60 64 54 65    J...w..x.cd``dTe
  60 60 62 4e 49 4d 63 60    60 e0 2f 4a 2d 48 4d 2c    ``bNIMc``./J-HM,
  d1 50 4a 54 d2 51 30 35    d0 64 e0 e1 60 30 02 8a    .PJT.Q05.d..`0..
  ff 65 64 90 67 60 60 65    60 60 fe 07 54 cc 60 cc    .ed.g``e``..T.`.
  c0 c0 62 94 48 32 00 ea    67 05 eb 07 00 8d f9 1c    ..b.H2..g.......
  64                                                    d
  ~~~~~~~~~~~~~

  @note sending a MySQL Packet of the size 2<sup>24</sup>-5 to 2<sup>24</sup>-1
  via compression leads to at least one extra compressed packet.
  If the uncompressed MySQL Packet is like
  ~~~~~~~~~~~~~~
  fe ff ff 03 ... -- length = 2^24-2, sequence id = 3
  ~~~~~~~~~~~~~~
  compressing it would result in the *length of payload before compression*
  in the @ref sect_protocol_basic_compression_packet_header being:
  ~~~~~~~~~~~~~~
  length of mysql packet payload:       2^24-2
  length of mysql packet header:        4
  length of payload before compression: 2^24+2
  ~~~~~~~~~~~~~~
  which can not be represented in one compressed packet.
  Instead two or more packets have to be sent.

  @section sect_protocol_basic_compression_packet_uncompressed_payload Uncompressed Payload

  For small packets it may be to costly to compress the packet:
  - compressing the packet may lead to more data and sending the
    data uncompressed
  - CPU overhead may be not worth to compress the data
    @par Tip
    Usually payloads less than 50 bytes (::MIN_COMPRESS_LENGTH) aren't compressed.


  To send an @ref sect_protocol_basic_compression_packet_uncompressed_payload :
  - set *length of payload before compression* in
    @ref sect_protocol_basic_compression_packet_header to 0
  - The @ref sect_protocol_basic_compression_packet_compressed_payload contains
    the @ref sect_protocol_basic_compression_packet_uncompressed_payload instead.

  Sending a `SELECT 1` query as @ref sect_protocol_basic_compression_packet_uncompressed_payload
  to the server looks like:
  ~~~~~~~~~~~~~~
  0d 00 00 00 00 00 00 09    00 00 00 03 53 45 4c 45    ............SELE
  43 54 20 31                                           CT 1
  ~~~~~~~~~~~~~~

  decodes into:
  ~~~~~~~~~~~~~~
  raw packet length                      -> 20
  compressed payload length   = 0d 00 00 -> 13 (20 - 7)
  sequence id                 = 00       ->  0
  uncompressed payload length = 00 00 00 -> uncompressed
  ~~~~~~~~~~~~~~

  ... with the *uncompressed payload* starting right after the 7 byte header:

  ~~~~~~~~~~~~~~
  09 00 00 00 03 53 45 4c 45 43 54 20 31 -- SELECT 1
  ~~~~~~~~~~~~~~
*/
/* clang-format on */

/**
  Compress and encapsulate a packet into a compressed packet.

  @param          net      NET handler.
  @param          packet   The packet to compress.
  @param[in,out]  length   Length of the packet.

  See @ref sect_protocol_basic_compression_packet_header for a
  description of the header structure.

  @return Pointer to the (new) compressed packet.
*/

static uchar *compress_packet(NET *net, const uchar *packet, size_t *length) {
  uchar *compr_packet;
  size_t compr_length;
  const uint header_length = NET_HEADER_SIZE + COMP_HEADER_SIZE;

  compr_packet = (uchar *)my_malloc(key_memory_NET_compress_packet,
                                    *length + header_length, MYF(MY_WME));

  if (compr_packet == NULL) return NULL;

  memcpy(compr_packet + header_length, packet, *length);

  /* Compress the encapsulated packet. */
  if (my_compress(compr_packet + header_length, length, &compr_length)) {
    /*
      If the length of the compressed packet is larger than the
      original packet, the original packet is sent uncompressed.
    */
    compr_length = 0;
  }

  /* Length of the compressed (original) packet. */
  int3store(&compr_packet[NET_HEADER_SIZE], static_cast<uint>(compr_length));
  /* Length of this packet. */
  int3store(compr_packet, static_cast<uint>(*length));
  /* Packet number. */
  compr_packet[3] = (uchar)(net->compress_pkt_nr++);

  *length += header_length;

  return compr_packet;
}

/**
  Write a MySQL protocol packet to the network handler.

  @param  net     NET handler.
  @param  packet  The packet to write.
  @param  length  Length of the packet.

  @remark The packet might be encapsulated into a compressed packet.

  @return true on error, false on success.
*/

bool net_write_packet(NET *net, const uchar *packet, size_t length) {
  bool res;
  DBUG_ENTER("net_write_packet");

  /* Socket can't be used */
  if (net->error == 2) DBUG_RETURN(true);

  net->reading_or_writing = 2;

  const bool do_compress = net->compress;
  if (do_compress) {
    if ((packet = compress_packet(net, packet, &length)) == NULL) {
      net->error = 2;
      net->last_errno = ER_OUT_OF_RESOURCES;
      /* In the server, allocation failure raises a error. */
      net->reading_or_writing = 0;
      DBUG_RETURN(true);
    }
  }

#ifdef DEBUG_DATA_PACKETS
  DBUG_DUMP("data", packet, length);
#endif

  res = net_write_raw_loop(net, packet, length);

  if (do_compress) my_free((void *)packet);

  net->reading_or_writing = 0;

  DBUG_RETURN(res);
}

/*****************************************************************************
** Read something from server/clinet
*****************************************************************************/

/**
  Read a determined number of bytes from a network handler.

  @param  net     NET handler.
  @param  count   The number of bytes to read.

  @return true on error, false on success.
*/

static bool net_read_raw_loop(NET *net, size_t count) {
  bool eof = false;
  unsigned int retry_count = 0;
  uchar *buf = net->buff + net->where_b;

  while (count) {
    size_t recvcnt = vio_read(net->vio, buf, count);

    /* VIO_SOCKET_ERROR (-1) indicates an error. */
    if (recvcnt == VIO_SOCKET_ERROR) {
      /* A recoverable I/O error occurred? */
      if (net_should_retry(net, &retry_count))
        continue;
      else
        break;
    }
    /* Zero indicates end of file. */
    else if (!recvcnt) {
      eof = true;
      break;
    }

    count -= recvcnt;
    buf += recvcnt;
#ifdef MYSQL_SERVER
    thd_increment_bytes_received(recvcnt);
#endif
  }

  /* On failure, propagate the error code. */
  if (count) {
    /* Socket should be closed. */
    net->error = 2;

    /* Interrupted by a timeout? */
    if (!eof && vio_was_timeout(net->vio))
      net->last_errno = ER_NET_READ_INTERRUPTED;
    else
      net->last_errno = ER_NET_READ_ERROR;

#ifdef MYSQL_SERVER
    my_error(net->last_errno, MYF(0));
#endif
  }

  return count != 0;
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

  @return true on error, false on success.
*/

static bool net_read_packet_header(NET *net) {
  uchar pkt_nr;
  size_t count = NET_HEADER_SIZE;
  bool rc;

  if (net->compress) count += COMP_HEADER_SIZE;

#ifdef MYSQL_SERVER
  NET_SERVER *server_extension;

  server_extension = static_cast<NET_SERVER *>(net->extension);

  if (server_extension != NULL) {
    void *user_data = server_extension->m_user_data;
    DBUG_ASSERT(server_extension->m_before_header != NULL);
    DBUG_ASSERT(server_extension->m_after_header != NULL);

    server_extension->m_before_header(net, user_data, count);
    rc = net_read_raw_loop(net, count);
    server_extension->m_after_header(net, user_data, count, rc);
  } else
#endif
  {
    rc = net_read_raw_loop(net, count);
  }

  if (rc) return true;

  DBUG_DUMP("packet_header", net->buff + net->where_b, NET_HEADER_SIZE);

  pkt_nr = net->buff[net->where_b + 3];

  /*
    Verify packet serial number against the truncated packet counter.
    The local packet counter must be truncated since its not reset.
  */
  if (pkt_nr != (uchar)net->pkt_nr) {
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
    my_message_local(ERROR_LEVEL,
                     "packets out of order (found %u, expected %u)",
                     (uint)pkt_nr, net->pkt_nr);
    DBUG_ASSERT(pkt_nr == net->pkt_nr);
#endif
    return true;
  }

  net->pkt_nr++;

  return false;
}

/**
  Read one (variable-length) MySQL protocol packet.
  A MySQL packet consists of a header and a payload.

  @remark Reads one packet to net->buff + net->where_b.
  @remark Long packets are handled by my_net_read().
  @remark The network buffer is expanded if necessary.

  @return The length of the packet, or @c packet_error on error.
*/

static size_t net_read_packet(NET *net, size_t *complen) {
  size_t pkt_len, pkt_data_len;

  *complen = 0;

  net->reading_or_writing = 1;

  /* Retrieve packet length and number. */
  if (net_read_packet_header(net)) goto error;

  net->compress_pkt_nr = net->pkt_nr;

  if (net->compress) {
    /*
      The right-hand expression
      must match the size of the buffer allocated in net_realloc().
    */
    DBUG_ASSERT(net->where_b + NET_HEADER_SIZE + 3 <=
                net->max_packet + NET_HEADER_SIZE + COMP_HEADER_SIZE);

    /*
      If the packet is compressed then complen > 0 and contains the
      number of bytes in the uncompressed packet.
    */
    *complen = uint3korr(&(net->buff[net->where_b + NET_HEADER_SIZE]));
  }

  /* The length of the packet that follows. */
  pkt_len = uint3korr(net->buff + net->where_b);

  /* End of big multi-packet. */
  if (!pkt_len) goto end;

  pkt_data_len = max(pkt_len, *complen) + net->where_b;

  /* Expand packet buffer if necessary. */
  if ((pkt_data_len >= net->max_packet) && net_realloc(net, pkt_data_len))
    goto error;

  /* Read the packet data (payload). */
  if (net_read_raw_loop(net, pkt_len)) goto error;

end:
  net->reading_or_writing = 0;
  return pkt_len;

error:
  net->reading_or_writing = 0;
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

ulong my_net_read(NET *net) {
  size_t len, complen;

  if (!net->compress) {
    len = net_read_packet(net, &complen);
    if (len == MAX_PACKET_LENGTH) {
      /* First packet of a multi-packet.  Concatenate the packets */
      ulong save_pos = net->where_b;
      size_t total_length = 0;
      do {
        net->where_b += len;
        total_length += len;
        len = net_read_packet(net, &complen);
      } while (len == MAX_PACKET_LENGTH);
      if (len != packet_error) len += total_length;
      net->where_b = save_pos;
    }
    net->read_pos = net->buff + net->where_b;
    if (len != packet_error)
      net->read_pos[len] = 0; /* Safeguard for mysql_use_result */
    return static_cast<ulong>(len);
  } else {
    /* We are using the compressed protocol */

    size_t buf_length;
    ulong start_of_packet;
    ulong first_packet_offset;
    uint read_length, multi_byte_packet = 0;

    if (net->remain_in_buf) {
      buf_length = net->buf_length; /* Data left in old packet */
      first_packet_offset = start_of_packet =
          (net->buf_length - net->remain_in_buf);
      /* Restore the character that was overwritten by the end 0 */
      net->buff[start_of_packet] = net->save_char;
    } else {
      /* reuse buffer, as there is nothing in it that we need */
      buf_length = start_of_packet = first_packet_offset = 0;
    }
    for (;;) {
      size_t packet_len;

      if (buf_length - start_of_packet >= NET_HEADER_SIZE) {
        read_length = uint3korr(net->buff + start_of_packet);
        if (!read_length) {
          /* End of multi-byte packet */
          start_of_packet += NET_HEADER_SIZE;
          break;
        }
        if (read_length + NET_HEADER_SIZE <= buf_length - start_of_packet) {
          if (multi_byte_packet) {
            /*
              It's never the buffer on the first loop iteration that will have
              multi_byte_packet on.
              Thus there shall never be a non-zero first_packet_offset here.
            */
            DBUG_ASSERT(first_packet_offset == 0);
            /* Remove packet header for second packet */
            memmove(net->buff + start_of_packet,
                    net->buff + start_of_packet + NET_HEADER_SIZE,
                    buf_length - start_of_packet - NET_HEADER_SIZE);
            start_of_packet += read_length;
            buf_length -= NET_HEADER_SIZE;
          } else
            start_of_packet += read_length + NET_HEADER_SIZE;

          if (read_length != MAX_PACKET_LENGTH) /* last package */
          {
            multi_byte_packet = 0; /* No last zero len packet */
            break;
          }
          multi_byte_packet = NET_HEADER_SIZE;
          /* Move data down to read next data packet after current one */
          if (first_packet_offset) {
            memmove(net->buff, net->buff + first_packet_offset,
                    buf_length - first_packet_offset);
            buf_length -= first_packet_offset;
            start_of_packet -= first_packet_offset;
            first_packet_offset = 0;
          }
          continue;
        }
      }
      /* Move data down to read next data packet after current one */
      if (first_packet_offset) {
        memmove(net->buff, net->buff + first_packet_offset,
                buf_length - first_packet_offset);
        buf_length -= first_packet_offset;
        start_of_packet -= first_packet_offset;
        first_packet_offset = 0;
      }

      net->where_b = buf_length;
      if ((packet_len = net_read_packet(net, &complen)) == packet_error) {
        return packet_error;
      }
      if (my_uncompress(net->buff + net->where_b, packet_len, &complen)) {
        net->error = 2; /* caller will close socket */
        net->last_errno = ER_NET_UNCOMPRESS_ERROR;
#ifdef MYSQL_SERVER
        my_error(ER_NET_UNCOMPRESS_ERROR, MYF(0));
#endif
        return packet_error;
      }
      buf_length += complen;
    }

    net->read_pos = net->buff + first_packet_offset + NET_HEADER_SIZE;
    net->buf_length = buf_length;
    net->remain_in_buf = (ulong)(buf_length - start_of_packet);
    len = ((ulong)(start_of_packet - first_packet_offset) - NET_HEADER_SIZE -
           multi_byte_packet);
    /*
      Save byte to restore when processing remaining buffer. Skip ahead when
      the packet is a zero packet terminated (in case of multiple of 0xffffff).
    */
    if (net->remain_in_buf)
      net->save_char = net->read_pos[len + multi_byte_packet];
    net->read_pos[len] = '\0';  // Safeguard for mysql_use_result.
  }
  return static_cast<ulong>(len);
}

void my_net_set_read_timeout(NET *net, uint timeout) {
  DBUG_ENTER("my_net_set_read_timeout");
  DBUG_PRINT("enter", ("timeout: %d", timeout));
  net->read_timeout = timeout;
  if (net->vio) vio_timeout(net->vio, 0, timeout);
  DBUG_VOID_RETURN;
}

void my_net_set_write_timeout(NET *net, uint timeout) {
  DBUG_ENTER("my_net_set_write_timeout");
  DBUG_PRINT("enter", ("timeout: %d", timeout));
  net->write_timeout = timeout;
  if (net->vio) vio_timeout(net->vio, 1, timeout);
  DBUG_VOID_RETURN;
}

void my_net_set_retry_count(NET *net, uint retry_count) {
  DBUG_ENTER("my_net_set_retry_count");
  DBUG_PRINT("enter", ("retry_count: %d", retry_count));
  net->retry_count = retry_count;
  if (net->vio) net->vio->retry_count = retry_count;
  DBUG_VOID_RETURN;
}
