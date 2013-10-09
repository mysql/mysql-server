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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

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

#if defined(__WIN__) || !defined(MYSQL_SERVER)
  /* The following is because alarms doesn't work on windows. */
#ifndef NO_ALARM
#define NO_ALARM
#endif
#endif

#ifndef NO_ALARM
#include "my_pthread.h"
void sql_print_error(const char *format,...);
#else
#define DONT_USE_THR_ALARM
#endif /* NO_ALARM */

#include "thr_alarm.h"

#ifdef MYSQL_SERVER
/*
  The following variables/functions should really not be declared
  extern, but as it's hard to include sql_priv.h here, we have to
  live with this for a while.
*/
extern uint test_flags;
extern ulong bytes_sent, bytes_received, net_big_packet_count;
#ifndef MYSQL_INSTANCE_MANAGER
#ifdef HAVE_QUERY_CACHE
#define USE_QUERY_CACHE
extern void query_cache_insert(const char *packet, ulong length,
                               unsigned pkt_nr);
#endif // HAVE_QUERY_CACHE
#define update_statistics(A) A
#endif /* MYSQL_INSTANCE_MANGER */
#endif /* defined(MYSQL_SERVER) && !defined(MYSQL_INSTANCE_MANAGER) */

#if !defined(MYSQL_SERVER) || defined(MYSQL_INSTANCE_MANAGER)
#define update_statistics(A)
#define thd_increment_bytes_sent(N)
#endif

#define TEST_BLOCKING		8
#define MAX_PACKET_LENGTH (256L*256L*256L-1)

static my_bool net_write_buff(NET *net,const uchar *packet,ulong len);


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
#if defined(MYSQL_SERVER) && !defined(EMBEDDED_LIBRARY)
  net->skip_big_packet= FALSE;
#endif

  if (vio != 0)					/* If real connection */
  {
    net->fd  = vio_fd(vio);			/* For perl DBI/DBD */
#if defined(MYSQL_SERVER) && !defined(__WIN__)
    if (!(test_flags & TEST_BLOCKING))
    {
      my_bool old_mode;
      vio_blocking(vio, FALSE, &old_mode);
    }
#endif
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
    my_real_read() may actually read 4 bytes depending on build flags and
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
  Check if there is any data to be read from the socket.

  @param sd   socket descriptor

  @retval
    0  No data to read
  @retval
    1  Data or EOF to read
  @retval
    -1   Don't know if data is ready or not
*/

#if !defined(EMBEDDED_LIBRARY)

static int net_data_is_ready(my_socket sd)
{
#ifdef HAVE_POLL
  struct pollfd ufds;
  int res;

  ufds.fd= sd;
  ufds.events= POLLIN | POLLPRI;
  if (!(res= poll(&ufds, 1, 0)))
    return 0;
  if (res < 0 || !(ufds.revents & (POLLIN | POLLPRI)))
    return 0;
  return 1;
#else
  fd_set sfds;
  struct timeval tv;
  int res;

#ifndef __WIN__
  /* Windows uses an _array_ of 64 fd's as default, so it's safe */
  if (sd >= FD_SETSIZE)
    return -1;
#define NET_DATA_IS_READY_CAN_RETURN_MINUS_ONE
#endif

  FD_ZERO(&sfds);
  FD_SET(sd, &sfds);

  tv.tv_sec= tv.tv_usec= 0;

  if ((res= select((int) (sd + 1), &sfds, NULL, NULL, &tv)) < 0)
    return 0;
  else
    return test(res ? FD_ISSET(sd, &sfds) : 0);
#endif /* HAVE_POLL */
}

#endif /* EMBEDDED_LIBRARY */

/**
  Remove unwanted characters from connection
  and check if disconnected.

    Read from socket until there is nothing more to read. Discard
    what is read.

    If there is anything when to read 'net_clear' is called this
    normally indicates an error in the protocol.

    When connection is properly closed (for TCP it means with
    a FIN packet), then select() considers a socket "ready to read",
    in the sense that there's EOF to read, but read() returns 0.

  @param net			NET handler
  @param clear_buffer           if <> 0, then clear all data from comm buff
*/

void net_clear(NET *net, my_bool clear_buffer)
{
#if !defined(EMBEDDED_LIBRARY)
  size_t count;
  int ready;
#endif
  DBUG_ENTER("net_clear");

#if !defined(EMBEDDED_LIBRARY)
  if (clear_buffer)
  {
    while ((ready= net_data_is_ready(net->vio->sd)) > 0)
    {
      /* The socket is ready */
      if ((long) (count= vio_read(net->vio, net->buff,
                                  (size_t) net->max_packet)) > 0)
      {
        DBUG_PRINT("info",("skipped %ld bytes from file: %s",
                           (long) count, vio_description(net->vio)));
#if defined(EXTRA_DEBUG)
        fprintf(stderr,"Note: net_clear() skipped %ld bytes from file: %s\n",
                (long) count, vio_description(net->vio));
#endif
      }
      else
      {
        DBUG_PRINT("info",("socket ready but only EOF to read - disconnected"));
        net->error= 2;
        break;
      }
    }
#ifdef NET_DATA_IS_READY_CAN_RETURN_MINUS_ONE
    /* 'net_data_is_ready' returned "don't know" */
    if (ready == -1)
    {
      /* Read unblocking to clear net */
      my_bool old_mode;
      if (!vio_blocking(net->vio, FALSE, &old_mode))
      {
        while ((long) (count= vio_read(net->vio, net->buff,
                                       (size_t) net->max_packet)) > 0)
          DBUG_PRINT("info",("skipped %ld bytes from file: %s",
                             (long) count, vio_description(net->vio)));
        vio_blocking(net->vio, TRUE, &old_mode);
      }
    }
#endif /* NET_DATA_IS_READY_CAN_RETURN_MINUS_ONE */
  }
#endif /* EMBEDDED_LIBRARY */
  net->pkt_nr=net->compress_pkt_nr=0;		/* Ready for new command */
  net->write_pos=net->buff;
  DBUG_VOID_RETURN;
}


/** Flush write_buffer if not empty. */

my_bool net_flush(NET *net)
{
  my_bool error= 0;
  DBUG_ENTER("net_flush");
  if (net->buff != net->write_pos)
  {
    error=test(net_real_write(net, net->buff,
			      (size_t) (net->write_pos - net->buff)));
    net->write_pos=net->buff;
  }
  /* Sync packet number if using compression */
  if (net->compress)
    net->pkt_nr=net->compress_pkt_nr;
  DBUG_RETURN(error);
}


/*****************************************************************************
** Write something to server/client buffer
*****************************************************************************/

/**
  Write a logical packet with packet header.

  Format: Packet length (3 bytes), packet number(1 byte)
  When compression is used a 3 byte compression length is added

  @note
    If compression is used the original package is modified!
*/

my_bool
my_net_write(NET *net,const uchar *packet,size_t len)
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
  rc= test(net_write_buff(net,packet,len));
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
    len=length;					/* Data left to be written */
  }
  int3store(buff,length);
  buff[3]= (uchar) net->pkt_nr++;
  rc= test(net_write_buff(net, buff, header_size) ||
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
    MAX_PACKET_LENGTH to net_real_write() if we are using the compressed
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
      memcpy((char*) net->write_pos,packet,left_length);
      if (net_real_write(net, net->buff, 
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
	if (net_real_write(net, packet, left_length))
	  return 1;
	packet+= left_length;
	len-= left_length;
      }
    }
    if (len > net->max_packet)
      return net_real_write(net, packet, len) ? 1 : 0;
    /* Send out rest of the blocks as full sized blocks */
  }
  memcpy((char*) net->write_pos,packet,len);
  net->write_pos+= len;
  return 0;
}


/**
  Read and write one packet using timeouts.
  If needed, the packet is compressed before sending.

  @todo
    - TODO is it needed to set this variable if we have no socket
*/

int
net_real_write(NET *net,const uchar *packet, size_t len)
{
  size_t length;
  const uchar *pos,*end;
  thr_alarm_t alarmed;
#ifndef NO_ALARM
  ALARM alarm_buff;
#endif
  uint retry_count=0;
  my_bool net_blocking = vio_is_blocking(net->vio);
  DBUG_ENTER("net_real_write");

#if defined(MYSQL_SERVER) && defined(USE_QUERY_CACHE)
  query_cache_insert((char*) packet, len, net->pkt_nr);
#endif

  if (net->error == 2)
    DBUG_RETURN(-1);				/* socket can't be used */

  net->reading_or_writing=2;
#ifdef HAVE_COMPRESS
  if (net->compress)
  {
    size_t complen;
    uchar *b;
    uint header_length=NET_HEADER_SIZE+COMP_HEADER_SIZE;
    if (!(b= (uchar*) my_malloc(len + NET_HEADER_SIZE +
                                COMP_HEADER_SIZE, MYF(MY_WME))))
    {
      net->error= 2;
      net->last_errno= ER_OUT_OF_RESOURCES;
      /* In the server, the error is reported by MY_WME flag. */
      net->reading_or_writing= 0;
      DBUG_RETURN(1);
    }
    memcpy(b+header_length,packet,len);

    if (my_compress(b+header_length, &len, &complen))
      complen=0;
    int3store(&b[NET_HEADER_SIZE],complen);
    int3store(b,len);
    b[3]=(uchar) (net->compress_pkt_nr++);
    len+= header_length;
    packet= b;
  }
#endif /* HAVE_COMPRESS */

#ifdef DEBUG_DATA_PACKETS
  DBUG_DUMP("data", packet, len);
#endif

#ifndef NO_ALARM
  thr_alarm_init(&alarmed);
  if (net_blocking)
    thr_alarm(&alarmed, net->write_timeout, &alarm_buff);
#else
  alarmed=0;
  /* Write timeout is set in my_net_set_write_timeout */
#endif /* NO_ALARM */

  pos= packet;
  end=pos+len;
  while (pos != end)
  {
    if ((long) (length= vio_write(net->vio,pos,(size_t) (end-pos))) <= 0)
    {
      my_bool interrupted = vio_should_retry(net->vio);
#if !defined(__WIN__)
      if ((interrupted || length == 0) && !thr_alarm_in_use(&alarmed))
      {
        if (!thr_alarm(&alarmed, net->write_timeout, &alarm_buff))
        {                                       /* Always true for client */
	  my_bool old_mode;
	  while (vio_blocking(net->vio, TRUE, &old_mode) < 0)
	  {
	    if (vio_should_retry(net->vio) && retry_count++ < net->retry_count)
	      continue;
#ifdef EXTRA_DEBUG
	    fprintf(stderr,
		    "%s: my_net_write: fcntl returned error %d, aborting thread\n",
		    my_progname,vio_errno(net->vio));
#endif /* EXTRA_DEBUG */
	    net->error= 2;                     /* Close socket */
            net->last_errno= ER_NET_PACKET_TOO_LARGE;
#ifdef MYSQL_SERVER
            my_error(ER_NET_PACKET_TOO_LARGE, MYF(0));
#endif
	    goto end;
	  }
	  retry_count=0;
	  continue;
	}
      }
      else
#endif /* !defined(__WIN__) */
	if (thr_alarm_in_use(&alarmed) && !thr_got_alarm(&alarmed) &&
	    interrupted)
      {
	if (retry_count++ < net->retry_count)
	    continue;
#ifdef EXTRA_DEBUG
	  fprintf(stderr, "%s: write looped, aborting thread\n",
		  my_progname);
#endif /* EXTRA_DEBUG */
      }
#if defined(THREAD_SAFE_CLIENT) && !defined(MYSQL_SERVER)
      if (vio_errno(net->vio) == SOCKET_EINTR)
      {
	DBUG_PRINT("warning",("Interrupted write. Retrying..."));
	continue;
      }
#endif /* defined(THREAD_SAFE_CLIENT) && !defined(MYSQL_SERVER) */
      net->error= 2;				/* Close socket */
      net->last_errno= (interrupted ? ER_NET_WRITE_INTERRUPTED :
                               ER_NET_ERROR_ON_WRITE);
#ifdef MYSQL_SERVER
      my_error(net->last_errno, MYF(0));
#endif /* MYSQL_SERVER */
      break;
    }
    pos+=length;
    update_statistics(thd_increment_bytes_sent(length));
  }
#ifndef __WIN__
 end:
#endif
#ifdef HAVE_COMPRESS
  if (net->compress)
    my_free((void*) packet);
#endif
  if (thr_alarm_in_use(&alarmed))
  {
    my_bool old_mode;
    thr_end_alarm(&alarmed);
    vio_blocking(net->vio, net_blocking, &old_mode);
  }
  net->reading_or_writing=0;
  DBUG_RETURN(((int) (pos != end)));
}


/*****************************************************************************
** Read something from server/clinet
*****************************************************************************/

#ifndef NO_ALARM

static my_bool net_safe_read(NET *net, uchar *buff, size_t length,
			     thr_alarm_t *alarmed)
{
  uint retry_count=0;
  while (length > 0)
  {
    size_t tmp;
    if ((long) (tmp= vio_read(net->vio, buff, length)) <= 0)
    {
      my_bool interrupted = vio_should_retry(net->vio);
      if (!thr_got_alarm(alarmed) && interrupted)
      {					/* Probably in MIT threads */
	if (retry_count++ < net->retry_count)
	  continue;
      }
      return 1;
    }
    length-= tmp;
    buff+= tmp;
  }
  return 0;
}

/**
  Help function to clear the commuication buffer when we get a too big packet.

  @param net		Communication handle
  @param remain	Bytes to read
  @param alarmed	Parameter for thr_alarm()
  @param alarm_buff	Parameter for thr_alarm()

  @retval
   0	Was able to read the whole packet
  @retval
   1	Got mailformed packet from client
*/

static my_bool my_net_skip_rest(NET *net, uint32 remain, thr_alarm_t *alarmed,
				ALARM *alarm_buff)
{
  uint32 old=remain;
  DBUG_ENTER("my_net_skip_rest");
  DBUG_PRINT("enter",("bytes_to_skip: %u", (uint) remain));

  /* The following is good for debugging */
  update_statistics(thd_increment_net_big_packet_count(1));

  if (!thr_alarm_in_use(alarmed))
  {
    my_bool old_mode;
    if (thr_alarm(alarmed,net->read_timeout, alarm_buff) ||
	vio_blocking(net->vio, TRUE, &old_mode) < 0)
      DBUG_RETURN(1);				/* Can't setup, abort */
  }
  for (;;)
  {
    while (remain > 0)
    {
      size_t length= min(remain, net->max_packet);
      if (net_safe_read(net, net->buff, length, alarmed))
	DBUG_RETURN(1);
      update_statistics(thd_increment_bytes_received(length));
      remain -= (uint32) length;
    }
    if (old != MAX_PACKET_LENGTH)
      break;
    if (net_safe_read(net, net->buff, NET_HEADER_SIZE, alarmed))
      DBUG_RETURN(1);
    old=remain= uint3korr(net->buff);
    net->pkt_nr++;
  }
  DBUG_RETURN(0);
}
#endif /* NO_ALARM */


/**
  Reads one packet to net->buff + net->where_b.
  Long packets are handled by my_net_read().
  This function reallocates the net->buff buffer if necessary.

  @return
    Returns length of packet.
*/

static ulong
my_real_read(NET *net, size_t *complen)
{
  uchar *pos;
  size_t length;
  uint i,retry_count=0;
  ulong len=packet_error;
  thr_alarm_t alarmed;
#ifndef NO_ALARM
  ALARM alarm_buff;
#endif
  my_bool net_blocking=vio_is_blocking(net->vio);
  uint32 remain= (net->compress ? NET_HEADER_SIZE+COMP_HEADER_SIZE :
		  NET_HEADER_SIZE);
  *complen = 0;

  net->reading_or_writing=1;
  thr_alarm_init(&alarmed);
#ifndef NO_ALARM
  if (net_blocking)
    thr_alarm(&alarmed,net->read_timeout,&alarm_buff);
#else
  /* Read timeout is set in my_net_set_read_timeout */
#endif /* NO_ALARM */

    pos = net->buff + net->where_b;		/* net->packet -4 */
    for (i=0 ; i < 2 ; i++)
    {
      while (remain > 0)
      {
	/* First read is done with non blocking mode */
        if ((long) (length= vio_read(net->vio, pos, remain)) <= 0L)
        {
          my_bool interrupted = vio_should_retry(net->vio);

	  DBUG_PRINT("info",("vio_read returned %ld  errno: %d",
			     (long) length, vio_errno(net->vio)));
#if !defined(__WIN__) || defined(MYSQL_SERVER)
	  /*
	    We got an error that there was no data on the socket. We now set up
	    an alarm to not 'read forever', change the socket to non blocking
	    mode and try again
	  */
	  if ((interrupted || length == 0) && !thr_alarm_in_use(&alarmed))
	  {
	    if (!thr_alarm(&alarmed,net->read_timeout,&alarm_buff)) /* Don't wait too long */
	    {
	      my_bool old_mode;
	      while (vio_blocking(net->vio, TRUE, &old_mode) < 0)
	      {
		if (vio_should_retry(net->vio) &&
		    retry_count++ < net->retry_count)
		  continue;
		DBUG_PRINT("error",
			   ("fcntl returned error %d, aborting thread",
			    vio_errno(net->vio)));
#ifdef EXTRA_DEBUG
		fprintf(stderr,
			"%s: read: fcntl returned error %d, aborting thread\n",
			my_progname,vio_errno(net->vio));
#endif /* EXTRA_DEBUG */
		len= packet_error;
		net->error= 2;                 /* Close socket */
	        net->last_errno= ER_NET_FCNTL_ERROR;
#ifdef MYSQL_SERVER
		my_error(ER_NET_FCNTL_ERROR, MYF(0));
#endif
		goto end;
	      }
	      retry_count=0;
	      continue;
	    }
	  }
#endif /* (!defined(__WIN__) || defined(MYSQL_SERVER) */
	  if (thr_alarm_in_use(&alarmed) && !thr_got_alarm(&alarmed) &&
	      interrupted)
	  {					/* Probably in MIT threads */
	    if (retry_count++ < net->retry_count)
	      continue;
#ifdef EXTRA_DEBUG
	    fprintf(stderr, "%s: read looped with error %d, aborting thread\n",
		    my_progname,vio_errno(net->vio));
#endif /* EXTRA_DEBUG */
	  }
#if defined(THREAD_SAFE_CLIENT) && !defined(MYSQL_SERVER)
	  if (vio_errno(net->vio) == SOCKET_EINTR)
	  {
	    DBUG_PRINT("warning",("Interrupted read. Retrying..."));
	    continue;
	  }
#endif
	  DBUG_PRINT("error",("Couldn't read packet: remain: %u  errno: %d  length: %ld",
			      remain, vio_errno(net->vio), (long) length));
	  len= packet_error;
	  net->error= 2;				/* Close socket */
          net->last_errno= (vio_was_interrupted(net->vio) ?
                                   ER_NET_READ_INTERRUPTED :
                                   ER_NET_READ_ERROR);
#ifdef MYSQL_SERVER
          my_error(net->last_errno, MYF(0));
#endif
	  goto end;
	}
	remain -= (uint32) length;
	pos+= length;
	update_statistics(thd_increment_bytes_received(length));
      }
      if (i == 0)
      {					/* First parts is packet length */
	ulong helping;
        DBUG_DUMP("packet_header", net->buff+net->where_b,
                  NET_HEADER_SIZE);
	if (net->buff[net->where_b + 3] != (uchar) net->pkt_nr)
	{
	  if (net->buff[net->where_b] != (uchar) 255)
	  {
	    DBUG_PRINT("error",
		       ("Packets out of order (Found: %d, expected %u)",
			(int) net->buff[net->where_b + 3],
			net->pkt_nr));
            /* 
              We don't make noise server side, since the client is expected
              to break the protocol for e.g. --send LOAD DATA .. LOCAL where
              the server expects the client to send a file, but the client
              may reply with a new command instead.
            */
#if defined (EXTRA_DEBUG) && !defined (MYSQL_SERVER)
            fflush(stdout);
	    fprintf(stderr,"Error: Packets out of order (Found: %d, expected %d)\n",
		    (int) net->buff[net->where_b + 3],
		    (uint) (uchar) net->pkt_nr);
            fflush(stderr);
            DBUG_ASSERT(0);
#endif
	  }
	  len= packet_error;
          /* Not a NET error on the client. XXX: why? */
#ifdef MYSQL_SERVER
	  my_error(ER_NET_PACKETS_OUT_OF_ORDER, MYF(0));
#endif
	  goto end;
	}
	net->compress_pkt_nr= ++net->pkt_nr;
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
	    number of bytes in the uncompressed packet
	  */
	  *complen=uint3korr(&(net->buff[net->where_b + NET_HEADER_SIZE]));
	}
#endif

	len=uint3korr(net->buff+net->where_b);
	if (!len)				/* End of big multi-packet */
	  goto end;
	helping = max(len,*complen) + net->where_b;
	/* The necessary size of net->buff */
	if (helping >= net->max_packet)
	{
	  if (net_realloc(net,helping))
	  {
#if defined(MYSQL_SERVER) && !defined(NO_ALARM)
	    if (!net->compress &&
                net->skip_big_packet &&
		!my_net_skip_rest(net, (uint32) len, &alarmed, &alarm_buff))
	      net->error= 3;		/* Successfully skiped packet */
#endif
	    len= packet_error;          /* Return error and close connection */
	    goto end;
	  }
	}
	pos=net->buff + net->where_b;
	remain = (uint32) len;
      }
    }

end:
  if (thr_alarm_in_use(&alarmed))
  {
    my_bool old_mode;
    thr_end_alarm(&alarmed);
    vio_blocking(net->vio, net_blocking, &old_mode);
  }
  net->reading_or_writing=0;
#ifdef DEBUG_DATA_PACKETS
  if (len != packet_error)
    DBUG_DUMP("data", net->buff+net->where_b, len);
#endif
  return(len);
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
    len = my_real_read(net,&complen);
    if (len == MAX_PACKET_LENGTH)
    {
      /* First packet of a multi-packet.  Concatenate the packets */
      ulong save_pos = net->where_b;
      size_t total_length= 0;
      do
      {
	net->where_b += len;
	total_length += len;
	len = my_real_read(net,&complen);
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
      if ((packet_len = my_real_read(net,&complen)) == packet_error)
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
#ifdef NO_ALARM
  if (net->vio)
    vio_timeout(net->vio, 0, timeout);
#endif
  DBUG_VOID_RETURN;
}


void my_net_set_write_timeout(NET *net, uint timeout)
{
  DBUG_ENTER("my_net_set_write_timeout");
  DBUG_PRINT("enter", ("timeout: %d", timeout));
  net->write_timeout= timeout;
#ifdef NO_ALARM
  if (net->vio)
    vio_timeout(net->vio, 1, timeout);
#endif
  DBUG_VOID_RETURN;
}
