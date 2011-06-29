#ifndef yaSSL_transport_types_h__
#define yaSSL_transport_types_h__

/* Type of transport functions used for sending and receiving data. */
typedef long (*yaSSL_recv_func_t) (void *, void *, size_t);
typedef long (*yaSSL_send_func_t) (void *, const void *, size_t);

#endif
