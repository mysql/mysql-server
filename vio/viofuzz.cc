
#include "my_config.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#ifndef _WIN32
#include <netdb.h>
#endif
#include <stdio.h>
#include <stdlib.h>

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_macros.h"
#include "vio/vio_priv.h"

#ifdef FIONREAD_IN_SYS_FILIO
#include <sys/filio.h>
#endif
#ifndef _WIN32
#include <netinet/tcp.h>
#endif
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

static const uint8_t *fuzzBuffer;
static size_t fuzzSize;
static size_t fuzzPos;


void sock_initfuzz(const uint8_t *Data, size_t Size) {
    fuzzPos = 0;
    fuzzSize = Size;
    fuzzBuffer = Data;
}

bool vio_connect_fuzz(Vio *vio, struct sockaddr *addr, socklen_t len,
                        int timeout) {
  int ret, wait;
  int retry_count = 0;
  DBUG_ENTER("vio_socket_connect");

  /* Only for socket-based transport types. */
  DBUG_ASSERT(vio->type == VIO_TYPE_SOCKET || vio->type == VIO_TYPE_TCPIP);

  /* Initiate the connection. */
  ret=0;

  DBUG_RETURN(MY_TEST(ret));
}


int vio_socket_timeout_fuzz(Vio *vio, uint which, bool b) {
    DBUG_ENTER("vio_socket_timeout_fuzz\n");
    return 1;
}


size_t vio_read_buff_fuzz(Vio *vio, uchar *bufp, size_t size) {
    DBUG_ENTER("vio_read_buff_fuzz.\n");
    if (size > fuzzSize - fuzzPos) {
        size = fuzzSize - fuzzPos;
    }
    if (fuzzPos < fuzzSize) {
        memcpy(bufp, fuzzBuffer + fuzzPos, size);
    }
    fuzzPos += size;
    return size;
}

size_t vio_write_buff_fuzz(Vio *vio, const uchar *bufp, size_t size) {
    DBUG_ENTER("vio_write_buff_fuzz\n");
    return size;
}

bool vio_is_connected_fuzz(Vio *vio) {
    DBUG_ENTER("vio_is_connected_fuzz\n");
    return true;
}

bool vio_was_timeout_fuzz(Vio *vio) {
    DBUG_ENTER("vio_was_timeout_fuzz\n");
    return false;
}

int vio_shutdown_fuzz(Vio *vio) {
    DBUG_ENTER("vio_shutdown_fuzz");
    return 0;
}

int vio_keepalive_fuzz(Vio *vio, bool set_keep_alive) {
    DBUG_ENTER("vio_keepalive_fuzz\n");
    return 0;
}
int vio_io_wait_fuzz(Vio *vio, enum enum_vio_io_event event, int timeout) {
    DBUG_ENTER("vio_io_wait_fuzz");
    return 1;
}

int vio_fastsend_fuzz(Vio *vio) {
    DBUG_ENTER("vio_fastsend_fuzz\n");
    return 0;
}
