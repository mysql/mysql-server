/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* Write and read of logical packets to/from socket
** Writes are cached into net_buffer_length big packets.
** Read packets are reallocated dynamicly when reading big packets.
** Each logical packet has the following pre-info:
** 3 byte length & 1 byte package-number.
*/

#ifdef EMBEDDED_LIBRARY
#define net_read_timeout net_read_timeout1
#define net_write_timeout net_write_timeout1
#endif

#ifdef __WIN__
#include <winsock.h>
#endif
#include <global.h>
#include <mysql_com.h>
#include <violite.h>
#include <my_sys.h>
#include <m_string.h>
#include "mysql.h"
#include "mysqld_error.h"
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <assert.h>

extern "C" {

#ifdef MYSQL_SERVER
ulong max_allowed_packet=65536;
extern ulong net_read_timeout,net_write_timeout;
extern uint test_flags;
#else

/*
** Give error if a too big packet is found
** The server can change this with the -O switch, but because the client
** can't normally do this the client should have a bigger max_allowed_packet.
*/

ulong max_allowed_packet=~0L;
ulong net_read_timeout=  NET_READ_TIMEOUT;
ulong net_write_timeout= NET_WRITE_TIMEOUT;
#endif
ulong net_buffer_length=8192;	/* Default length. Enlarged if necessary */

#if !defined(__WIN__) && !defined(MSDOS)
#include <sys/socket.h>
#else
#undef MYSQL_SERVER			/* Win32 can't handle interrupts */
#endif
#if !defined(MSDOS) && !defined(__WIN__) && !defined(HAVE_BROKEN_NETINET_INCLUDES) && !defined(__BEOS__)
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#if !defined(alpha_linux_port)
#include <netinet/tcp.h>
#endif
#endif
#include "mysqld_error.h"
#ifdef MYSQL_SERVER
#include "my_pthread.h"
#include "thr_alarm.h"
void sql_print_error(const char *format,...);
#define RETRY_COUNT mysqld_net_retry_count
extern ulong mysqld_net_retry_count;
#else
typedef my_bool thr_alarm_t;
typedef my_bool ALARM;
#define thr_alarm_init(A) (*A)=0
#define thr_alarm_in_use(A) (*(A))
#define thr_end_alarm(A)
#define thr_alarm(A,B,C) local_thr_alarm((A),(B),(C))
inline int local_thr_alarm(my_bool *A,int B __attribute__((unused)),ALARM *C __attribute__((unused)))
{
  *A=1;
  return 0;
}
#define thr_got_alarm(A) 0
#define RETRY_COUNT 1
#endif

#ifdef MYSQL_SERVER
extern ulong bytes_sent, bytes_received;
extern pthread_mutex_t LOCK_bytes_sent , LOCK_bytes_received;
#else
#undef statistic_add
#define statistic_add(A,B,C)
#endif

#define TEST_BLOCKING		8
static int net_write_buff(NET *net,const char *packet,ulong len);

#define MAX_THREE_BYTES 255L*255L*255L

	/* Init with packet info */

int my_net_init(NET *net, Vio* vio)
{
  if (!(net->buff=(uchar*) my_malloc(net_buffer_length+ 
				     NET_HEADER_SIZE + COMP_HEADER_SIZE,
				     MYF(MY_WME))))
    return 1;
  if (net_buffer_length > max_allowed_packet)
    max_allowed_packet=net_buffer_length;
  net->buff_end=net->buff+(net->max_packet=net_buffer_length);
  net->vio = vio;
  net->no_send_ok = 0;
  net->error=0; net->return_errno=0; net->return_status=0;
  net->timeout=(uint) net_read_timeout;		/* Timeout for read */
  net->pkt_nr=0;
  net->write_pos=net->read_pos = net->buff;
  net->last_error[0]=0;
  net->compress=0; net->reading_or_writing=0;
  net->where_b = net->remain_in_buf=0;
  net->last_errno=0;

  if (vio != 0)					/* If real connection */
  {
    net->fd  = vio_fd(vio);			/* For perl DBI/DBD */
#if defined(MYSQL_SERVER) && !defined(___WIN__) && !defined(__EMX__)
    if (!(test_flags & TEST_BLOCKING))
      vio_blocking(vio, FALSE);
#endif
    vio_fastsend(vio);
  }
  return 0;
}

void net_end(NET *net)
{
  my_free((gptr) net->buff,MYF(MY_ALLOW_ZERO_PTR));
  net->buff=0;
}

/* Realloc the packet buffer */

static my_bool net_realloc(NET *net, ulong length)
{
  uchar *buff;
  ulong pkt_length;
  if (length >= max_allowed_packet)
  {
    DBUG_PRINT("error",("Packet too large (%lu)", length));
    net->error=1;
    net->last_errno=ER_NET_PACKET_TOO_LARGE;
    return 1;
  }
  pkt_length = (length+IO_SIZE-1) & ~(IO_SIZE-1); 
  /* We must allocate some extra bytes for the end 0 and to be able to
     read big compressed blocks */
  if (!(buff=(uchar*) my_realloc((char*) net->buff, pkt_length +
				 NET_HEADER_SIZE + COMP_HEADER_SIZE,
				 MYF(MY_WME))))
  {
    net->error=1;
#ifdef MYSQL_SERVER
    net->last_errno=ER_OUT_OF_RESOURCES;
#endif
    return 1;
  }
  net->buff=net->write_pos=buff;
  net->buff_end=buff+(net->max_packet=pkt_length);
  return 0;
}

	/* Remove unwanted characters from connection */

void net_clear(NET *net)
{
#ifndef EXTRA_DEBUG
  int count;
  bool is_blocking=vio_is_blocking(net->vio);
  if (is_blocking)
    vio_blocking(net->vio, FALSE);
  if (!vio_is_blocking(net->vio))		/* Safety if SSL */
  {
    while ( (count = vio_read(net->vio, (char*) (net->buff),
			      net->max_packet)) > 0)
      DBUG_PRINT("info",("skipped %d bytes from file: %s",
			 count,vio_description(net->vio)));
    if (is_blocking)
      vio_blocking(net->vio, TRUE);
  }
#endif /* EXTRA_DEBUG */
  net->pkt_nr=0;				/* Ready for new command */
  net->write_pos=net->buff;
}

	/* Flush write_buffer if not empty. */

int net_flush(NET *net)
{
  int error=0;
  DBUG_ENTER("net_flush");
  if (net->buff != net->write_pos)
  {
    error=net_real_write(net,(char*) net->buff,
			 (uint) (net->write_pos - net->buff));
    net->write_pos=net->buff;
  }
  DBUG_RETURN(error);
}


/*****************************************************************************
** Write something to server/client buffer
*****************************************************************************/

/*
** Write a logical packet with packet header
** Format: Packet length (3 bytes), packet number(1 byte)
**         When compression is used a 3 byte compression length is added
** NOTE: If compression is used the original package is modified!
*/

int
my_net_write(NET *net,const char *packet,ulong len)
{
  uchar buff[NET_HEADER_SIZE];
  /*
    Big packets are handled by splitting them in packets of MAX_THREE_BYTES
    length. The last packet is always a packet that is < MAX_THREE_BYTES.
    (The last packet may even have a lengt of 0)
  */
  while (len >= MAX_THREE_BYTES)
  {
    const ulong z_size = MAX_THREE_BYTES;
    int3store(buff, z_size);
    buff[3]= (net->compress) ? 0 : (uchar) (net->pkt_nr++);
    if (net_write_buff(net, (char*) buff, NET_HEADER_SIZE) ||
	net_write_buff(net, packet, z_size))
      return 1;
    packet += z_size;
    len-=     z_size;
  }
  /* Write last packet */
  int3store(buff,len);
  buff[3]= (net->compress) ? 0 : (uchar) (net->pkt_nr++);
  if (net_write_buff(net,(char*) buff,NET_HEADER_SIZE))
    return 1;
  return net_write_buff(net,packet,len);
}

/*
  Send a command to the server.
  As the command is part of the first data packet, we have to do some data
  juggling to put the command in there, without having to create a new
  packet.
  This function will split big packets into sub-packets if needed.
  (Each sub packet can only be 2^24 bytes)
*/

int
net_write_command(NET *net,uchar command,const char *packet,ulong len)
{
  uint length=len+1;				/* 1 extra byte for command */
  uchar buff[NET_HEADER_SIZE+1];
  uint header_size=NET_HEADER_SIZE+1;
  buff[4]=command;				/* For first packet */

  if (length >= MAX_THREE_BYTES)
  {
    /* Take into account that we have the command in the first header */
    len= MAX_THREE_BYTES -1;
    do
    {
      int3store(buff, MAX_THREE_BYTES);
      buff[3]= (net->compress) ? 0 : (uchar) (net->pkt_nr++);
      if (net_write_buff(net,(char*) buff, header_size) ||
	  net_write_buff(net,packet,len))
	return 1;
      packet+= len;
      length-= MAX_THREE_BYTES;
      len=MAX_THREE_BYTES;
      header_size=NET_HEADER_SIZE;
    } while (length >= MAX_THREE_BYTES);
    len=length;					/* Data left to be written */
  }
  int3store(buff,length);
  buff[3]= (net->compress) ? 0 : (uchar) (net->pkt_nr++);
  return test(net_write_buff(net,(char*) buff,header_size) ||
	      net_write_buff(net,packet,len) || net_flush(net));
}

/*
  Caching the data in a local buffer before sending it.
  One can force the buffer to be flushed with 'net_flush'.
*/

static int
net_write_buff(NET *net,const char *packet,ulong len)
{
  uint left_length=(uint) (net->buff_end - net->write_pos);

  while (len > left_length)
  {
    memcpy((char*) net->write_pos,packet,left_length);
    if (net_real_write(net,(char*) net->buff,net->max_packet))
      return 1;
    net->write_pos=net->buff;
    packet+=left_length;
    len-=left_length;
    left_length=net->max_packet;
  }
  memcpy((char*) net->write_pos,packet,len);
  net->write_pos+=len;
  return 0;
}


/*
  Read and write one packet using timeouts.
  If needed, the packet is compressed before sending.
*/

int
net_real_write(NET *net,const char *packet,ulong len)
{
  int length;
  char *pos,*end;
  thr_alarm_t alarmed;
#if !defined(__WIN__)
  ALARM alarm_buff;
#endif
  uint retry_count=0;
  my_bool net_blocking = vio_is_blocking(net->vio);
  DBUG_ENTER("net_real_write");

  if (net->error == 2)
    DBUG_RETURN(-1);				/* socket can't be used */

  net->reading_or_writing=2;
#ifdef HAVE_COMPRESS
  if (net->compress)
  {
    ulong complen;
    uchar *b;
    uint header_length=NET_HEADER_SIZE+COMP_HEADER_SIZE;
    if (!(b=(uchar*) my_malloc(len + NET_HEADER_SIZE + COMP_HEADER_SIZE,
				    MYF(MY_WME))))
    {
#ifdef MYSQL_SERVER
      net->last_errno=ER_OUT_OF_RESOURCES;
      net->error=2;
#endif
      net->reading_or_writing=0;
      DBUG_RETURN(1);
    }
    memcpy(b+header_length,packet,len);

    if (my_compress((byte*) b+header_length,&len,&complen))
    {
      DBUG_PRINT("warning",
		 ("Compression error; Continuing without compression"));
      complen=0;
    }
    int3store(&b[NET_HEADER_SIZE],complen);
    int3store(b,len);
    b[3]=(uchar) (net->pkt_nr++);
    len+= header_length;
    packet= (char*) b;
  }
#endif /* HAVE_COMPRESS */

  /* DBUG_DUMP("net",packet,len); */
#ifdef MYSQL_SERVER
  thr_alarm_init(&alarmed);
  if (net_blocking)
    thr_alarm(&alarmed,(uint) net_write_timeout,&alarm_buff);
#else
  alarmed=0;
#endif /* MYSQL_SERVER */

  pos=(char*) packet; end=pos+len;
  while (pos != end)
  {
    if ((int) (length=vio_write(net->vio,pos,(int) (end-pos))) <= 0)
    {
      my_bool interrupted = vio_should_retry(net->vio);
#if (!defined(__WIN__) && !defined(__EMX__))
      if ((interrupted || length==0) && !thr_alarm_in_use(&alarmed))
      {
        if (!thr_alarm(&alarmed,(uint) net_write_timeout,&alarm_buff))
        {                                       /* Always true for client */
	  if (!vio_is_blocking(net->vio))
	  {
	    while (vio_blocking(net->vio, TRUE) < 0)
	    {
	      if (vio_should_retry(net->vio) && retry_count++ < RETRY_COUNT)
		continue;
#ifdef EXTRA_DEBUG
	      fprintf(stderr,
		      "%s: my_net_write: fcntl returned error %d, aborting thread\n",
		      my_progname,vio_errno(net->vio));
#endif /* EXTRA_DEBUG */
	      net->error=2;                     /* Close socket */
	      goto end;
	    }
	  }
	  retry_count=0;
	  continue;
	}
      }
      else
#endif /* (!defined(__WIN__) && !defined(__EMX__)) */
	if (thr_alarm_in_use(&alarmed) && !thr_got_alarm(&alarmed) &&
	    interrupted)
      {
	if (retry_count++ < RETRY_COUNT)
	    continue;
#ifdef EXTRA_DEBUG
	  fprintf(stderr, "%s: write looped, aborting thread\n",
		  my_progname);
#endif /* EXTRA_DEBUG */
      }
#if defined(THREAD_SAFE_CLIENT) && !defined(MYSQL_SERVER)
      if (vio_errno(net->vio) == EINTR)
      {
	DBUG_PRINT("warning",("Interrupted write. Retrying..."));
	continue;
      }
#endif /* defined(THREAD_SAFE_CLIENT) && !defined(MYSQL_SERVER) */
      net->error=2;				/* Close socket */
#ifdef MYSQL_SERVER
      net->last_errno= (interrupted ? ER_NET_WRITE_INTERRUPTED :
			ER_NET_ERROR_ON_WRITE);
#endif /* MYSQL_SERVER */
      break;
    }
    pos+=length;
    statistic_add(bytes_sent,length,&LOCK_bytes_sent);
  }
#ifndef __WIN__
 end:
#endif
#ifdef HAVE_COMPRESS
  if (net->compress)
    my_free((char*) packet,MYF(0));
#endif
  if (thr_alarm_in_use(&alarmed))
  {
    thr_end_alarm(&alarmed);
    vio_blocking(net->vio, net_blocking);
  }
  net->reading_or_writing=0;
  DBUG_RETURN(((int) (pos != end)));
}


/*****************************************************************************
** Read something from server/clinet
*****************************************************************************/

#ifdef MYSQL_SERVER

/*
  Help function to clear the commuication buffer when we get a too
  big packet
*/

static void my_net_skip_rest(NET *net, ulong remain, thr_alarm_t *alarmed)
{
  ALARM alarm_buff;
  uint retry_count=0;
  if (!thr_alarm_in_use(&alarmed))
  {
    if (!thr_alarm(alarmed,net->timeout,&alarm_buff) ||
	(!vio_is_blocking(net->vio) && vio_blocking(net->vio,TRUE) < 0))
      return;					/* Can't setup, abort */
  }
  while (remain > 0)
  {
    ulong length;
    if ((int) (length=vio_read(net->vio,(char*) net->buff,remain)) <= 0L)
    {
      my_bool interrupted = vio_should_retry(net->vio);
      if (!thr_got_alarm(&alarmed) && interrupted)
      {						/* Probably in MIT threads */
	if (retry_count++ < RETRY_COUNT)
	  continue;
      }
      return;
    }
    remain -= length;
    statistic_add(bytes_received,length,&LOCK_bytes_received);
  }
}
#endif /* MYSQL_SERVER */


/*
  Reads one packet to net->buff + net->where_b
  Returns length of packet.  Long packets are handled by my_net_read().
  This function reallocates the net->buff buffer if necessary.
*/

static uint
my_real_read(NET *net, ulong *complen)
{
  uchar *pos;
  long length;
  uint i,retry_count=0;
  ulong len=packet_error;
  thr_alarm_t alarmed;
#if (!defined(__WIN__) && !defined(__EMX__)) || defined(MYSQL_SERVER)
  ALARM alarm_buff;
#endif
  my_bool net_blocking=vio_is_blocking(net->vio);
  ulong remain= (net->compress ? NET_HEADER_SIZE+COMP_HEADER_SIZE :
		 NET_HEADER_SIZE);
  *complen = 0;

  net->reading_or_writing=1;
  thr_alarm_init(&alarmed);
#ifdef MYSQL_SERVER
  if (net_blocking)
    thr_alarm(&alarmed,net->timeout,&alarm_buff);
#endif /* MYSQL_SERVER */

    pos = net->buff + net->where_b;		/* net->packet -4 */
    for (i=0 ; i < 2 ; i++)
    {
      while (remain > 0)
      {
	/* First read is done with non blocking mode */
        if ((int) (length=vio_read(net->vio,(char*) pos,remain)) <= 0L)
        {
          my_bool interrupted = vio_should_retry(net->vio);

	  DBUG_PRINT("info",("vio_read returned %d,  errno: %d",
			     length, vio_errno(net->vio)));
#if (!defined(__WIN__) && !defined(__EMX__)) || defined(MYSQL_SERVER)
	  /*
	    We got an error that there was no data on the socket. We now set up
	    an alarm to not 'read forever', change the socket to non blocking
	    mode and try again
	  */
	  if ((interrupted || length == 0) && !thr_alarm_in_use(&alarmed))
	  {
	    if (!thr_alarm(&alarmed,net->timeout,&alarm_buff)) /* Don't wait too long */
	    {
              if (!vio_is_blocking(net->vio))
              {
                while (vio_blocking(net->vio,TRUE) < 0)
                {
                  if (vio_should_retry(net->vio) &&
		      retry_count++ < RETRY_COUNT)
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
                  net->error=2;                 /* Close socket */
#ifdef MYSQL_SERVER
		  net->last_errno=ER_NET_FCNTL_ERROR;
#endif
		  goto end;
                }
              }
	      retry_count=0;
	      continue;
	    }
	  }
#endif /* (!defined(__WIN__) && !defined(__EMX__)) || defined(MYSQL_SERVER) */
	  if (thr_alarm_in_use(&alarmed) && !thr_got_alarm(&alarmed) &&
	      interrupted)
	  {					/* Probably in MIT threads */
	    if (retry_count++ < RETRY_COUNT)
	      continue;
#ifdef EXTRA_DEBUG
	    fprintf(stderr, "%s: read looped with error %d, aborting thread\n",
		    my_progname,vio_errno(net->vio));
#endif /* EXTRA_DEBUG */
	  }
#if defined(THREAD_SAFE_CLIENT) && !defined(MYSQL_SERVER)
	  if (vio_should_retry(net->vio))
	  {
	    DBUG_PRINT("warning",("Interrupted read. Retrying..."));
	    continue;
	  }
#endif
	  DBUG_PRINT("error",("Couldn't read packet: remain: %d  errno: %d  length: %d  alarmed: %d", remain,vio_errno(net->vio),length,alarmed));
	  len= packet_error;
	  net->error=2;				/* Close socket */
#ifdef MYSQL_SERVER
	  net->last_errno= (interrupted ? ER_NET_READ_INTERRUPTED :
			    ER_NET_READ_ERROR);
#endif
	  goto end;
	}
	remain -= (ulong) length;
	pos+= (ulong) length;
	statistic_add(bytes_received,(ulong) length,&LOCK_bytes_received);
      }
      if (i == 0)
      {					/* First parts is packet length */
	ulong helping;
	if (net->buff[net->where_b + 3] != (uchar) net->pkt_nr)
	{
	  if (net->buff[net->where_b] != (uchar) 255)
	  {
	    DBUG_PRINT("error",
		       ("Packets out of order (Found: %d, expected %d)",
			(int) net->buff[net->where_b + 3],
			(uint) (uchar) net->pkt_nr));
#ifdef EXTRA_DEBUG
	    fprintf(stderr,"Packets out of order (Found: %d, expected %d)\n",
		    (int) net->buff[net->where_b + 3],
		    (uint) (uchar) net->pkt_nr);
#endif
	  }
	  len= packet_error;
#ifdef MYSQL_SERVER
	  net->last_errno=ER_NET_PACKETS_OUT_OF_ORDER;
#endif
	  goto end;
	}
	net->pkt_nr++;
#ifdef HAVE_COMPRESS
	if (net->compress)
	{
	  /* complen is > 0 if package is really compressed */
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
#ifdef MYSQL_SERVER
	    if (i == 1)
	      my_net_skip_rest(net, len, &alarmed);
#endif
	    len= packet_error;		/* Return error */
	    goto end;
	  }
	}
	pos=net->buff + net->where_b;
	remain = len;
      }
    }

end:
  if (thr_alarm_in_use(&alarmed))
  {
    thr_end_alarm(&alarmed);
    vio_blocking(net->vio, net_blocking);
  }
  net->reading_or_writing=0;
  return(len);
}


/*
  Read a packet from the client/server and return it without the internal
  package header.
  If the packet is the first packet of a multi-packet packet
  (which is indicated by the length of the packet = 0xffffff) then
  all sub packets are read and concatenated.
  If the packet was compressed, its uncompressed and the length of the
  uncompressed packet is returned.

  The function returns the length of the found packet or packet_error.
  net->read_pos points to the read data.
*/

ulong
my_net_read(NET *net)
{
  ulong len,complen;

#ifdef HAVE_COMPRESS
  if (!net->compress)
  {
#endif
    len = my_real_read(net,&complen);
    if (len == MAX_THREE_BYTES)
    {
      /* First packet of a multi-packet.  Concatenate the packets */
      int save_pos = net->where_b;
      ulong total_length=0;
      do
      {
	net->where_b += len;
	total_length += len;
	len = my_real_read (net,&complen);
      } while (len == MAX_THREE_BYTES);
      if (len != packet_error)
	len+= total_length;
      net->where_b = save_pos;
    }
    net->read_pos = net->buff + net->where_b;
    if (len != packet_error)
      net->read_pos[len]=0;		/* Safeguard for mysql_use_result */
    return len;
#ifdef HAVE_COMPRESS
  }
  else
  {
    /* We are using the compressed protocol */

    ulong buf_length=       net->buf_length;
    ulong start_of_packet=  net->buf_length - net->remain_in_buf;
    ulong first_packet_offset=start_of_packet;
    uint read_length, multi_byte_packet=0;

    if (net->remain_in_buf)
    {
      /* Restore the character that was overwritten by the end 0 */
      net->buff[start_of_packet]=net->save_char;
    }
    else
    {
      /* reuse buffer, as there is noting in it that we need */
      buf_length=start_of_packet=first_packet_offset=0;
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

	  if (read_length != MAX_THREE_BYTES)	    /* last package */
	  {
	    multi_byte_packet= 0;		// No last zero length packet
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
	return packet_error;
      if (my_uncompress((byte*) net->buff + net->where_b, &packet_len,
			&complen))
      {
	net->error=2;			/* caller will close socket */
#ifdef MYSQL_SERVER
	net->last_errno=ER_NET_UNCOMPRESS_ERROR;
#endif
	return packet_error;
      }
      buf_length+=packet_len;
    }

    net->read_pos=      net->buff+ first_packet_offset + NET_HEADER_SIZE;
    net->buf_length=    buf_length;
    net->remain_in_buf= buf_length - start_of_packet;
    len = ((uint) (start_of_packet - first_packet_offset) - NET_HEADER_SIZE -
           multi_byte_packet);
    net->save_char= net->read_pos[len];	/* Must be saved */
    net->read_pos[len]=0;		/* Safeguard for mysql_use_result */
  }
#endif /* HAVE_COMPRESS */
  return len;
}

}
