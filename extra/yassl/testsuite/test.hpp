// test.hpp

#ifndef yaSSL_TEST_HPP
#define yaSSL_TEST_HPP

#include "runtime.hpp"
#include "openssl/ssl.h"   /* openssl compatibility test */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <process.h>
    #define SOCKET_T unsigned int
#else
    #include <string.h>
    #include <unistd.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <sys/ioctl.h>
    #include <sys/time.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <pthread.h>
    #define SOCKET_T int
#endif /* _WIN32 */


#if !defined(_SOCKLEN_T) && (defined(_WIN32) || defined(__NETWARE__))
    typedef int socklen_t;
#endif


// Check type of third arg to accept
#if defined(__hpux)
// HPUX doesn't use socklent_t for third parameter to accept
    typedef int*       ACCEPT_THIRD_T;
#else
    typedef socklen_t* ACCEPT_THIRD_T;
#endif


// Check if _POSIX_THREADS should be forced
#if !defined(_POSIX_THREADS) && (defined(__NETWARE__) || defined(__hpux))
// HPUX does not define _POSIX_THREADS as it's not _fully_ implemented
// Netware supports pthreads but does not announce it
#define _POSIX_THREADS
#endif


#ifndef _POSIX_THREADS
    typedef unsigned int  THREAD_RETURN;
    typedef unsigned long THREAD_TYPE;
    #define YASSL_API __stdcall
#else
    typedef void*         THREAD_RETURN;
    typedef pthread_t     THREAD_TYPE;
    #define YASSL_API 
#endif


struct tcp_ready {
#ifdef _POSIX_THREADS
    pthread_mutex_t mutex_;
    pthread_cond_t  cond_;
    bool            ready_;   // predicate

    tcp_ready() : ready_(false)
    {
        pthread_mutex_init(&mutex_, 0);
        pthread_cond_init(&cond_, 0);
    }

    ~tcp_ready()
    {
        pthread_mutex_destroy(&mutex_);
        pthread_cond_destroy(&cond_);
    }
#endif
};    


struct func_args {
    int    argc;
    char** argv;
    int    return_code;
    tcp_ready* signal_;

    func_args(int c = 0, char** v = 0) : argc(c), argv(v) {}

    void SetSignal(tcp_ready* p) { signal_ = p; }
};

typedef THREAD_RETURN YASSL_API THREAD_FUNC(void*);

void start_thread(THREAD_FUNC, func_args*, THREAD_TYPE*);
void join_thread(THREAD_TYPE);

// yaSSL
const char* const    yasslIP   = "127.0.0.1";
const unsigned short yasslPort = 11111;


// client
const char* const cert = "../certs/client-cert.pem";
const char* const key  = "../certs/client-key.pem";

const char* const certSuite = "../../certs/client-cert.pem";
const char* const keySuite  = "../../certs/client-key.pem";

const char* const certDebug = "../../../certs/client-cert.pem";
const char* const keyDebug  = "../../../certs/client-key.pem";


// server
const char* const svrCert = "../certs/server-cert.pem";
const char* const svrKey  = "../certs/server-key.pem";

const char* const svrCert2 = "../../certs/server-cert.pem";
const char* const svrKey2  = "../../certs/server-key.pem";

const char* const svrCert3 = "../../../certs/server-cert.pem";
const char* const svrKey3  = "../../../certs/server-key.pem";


// server dsa
const char* const dsaCert = "../certs/dsa-cert.pem";
const char* const dsaKey  = "../certs/dsa512.der";

const char* const dsaCert2 = "../../certs/dsa-cert.pem";
const char* const dsaKey2  = "../../certs/dsa512.der";

const char* const dsaCert3 = "../../../certs/dsa-cert.pem";
const char* const dsaKey3  = "../../../certs/dsa512.der";


// CA 
const char* const caCert  = "../certs/ca-cert.pem";
const char* const caCert2 = "../../certs/ca-cert.pem";
const char* const caCert3 = "../../../certs/ca-cert.pem";


using namespace yaSSL;


inline void err_sys(const char* msg)
{
    printf("yassl error: %s\n", msg);
    exit(EXIT_FAILURE);
}


static int PasswordCallBack(char* passwd, int sz, int rw, void* userdata)
{
    strncpy(passwd, "12345678", sz);
    return 8;
}


inline void store_ca(SSL_CTX* ctx)
{
    // To allow testing from serveral dirs
    if (SSL_CTX_load_verify_locations(ctx, caCert, 0) != SSL_SUCCESS)
        if (SSL_CTX_load_verify_locations(ctx, caCert2, 0) != SSL_SUCCESS)
            if (SSL_CTX_load_verify_locations(ctx, caCert3, 0) != SSL_SUCCESS)
                err_sys("failed to use certificate: certs/cacert.pem");

    // load client CA for server verify
    if (SSL_CTX_load_verify_locations(ctx, cert, 0) != SSL_SUCCESS)
        if (SSL_CTX_load_verify_locations(ctx, certSuite, 0) != SSL_SUCCESS)
            if (SSL_CTX_load_verify_locations(ctx, certDebug,0) != SSL_SUCCESS)
                err_sys("failed to use certificate: certs/client-cert.pem");
}


// client
inline void set_certs(SSL_CTX* ctx)
{
    store_ca(ctx);
    SSL_CTX_set_default_passwd_cb(ctx, PasswordCallBack);

    // To allow testing from serveral dirs
    if (SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM)
        != SSL_SUCCESS)
        if (SSL_CTX_use_certificate_file(ctx, certSuite, SSL_FILETYPE_PEM)
            != SSL_SUCCESS)
            if (SSL_CTX_use_certificate_file(ctx, certDebug, SSL_FILETYPE_PEM)
                != SSL_SUCCESS)
                err_sys("failed to use certificate: certs/client-cert.pem");
    
    // To allow testing from several dirs
    if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM)
         != SSL_SUCCESS) 
         if (SSL_CTX_use_PrivateKey_file(ctx, keySuite, SSL_FILETYPE_PEM)
            != SSL_SUCCESS) 
                if (SSL_CTX_use_PrivateKey_file(ctx,keyDebug,SSL_FILETYPE_PEM)
                    != SSL_SUCCESS) 
                    err_sys("failed to use key file: certs/client-key.pem");
}


// server
inline void set_serverCerts(SSL_CTX* ctx)
{
    store_ca(ctx);
    SSL_CTX_set_default_passwd_cb(ctx, PasswordCallBack);

    // To allow testing from serveral dirs
    if (SSL_CTX_use_certificate_file(ctx, svrCert, SSL_FILETYPE_PEM)
        != SSL_SUCCESS)
        if (SSL_CTX_use_certificate_file(ctx, svrCert2, SSL_FILETYPE_PEM)
            != SSL_SUCCESS)
            if (SSL_CTX_use_certificate_file(ctx, svrCert3, SSL_FILETYPE_PEM)
                != SSL_SUCCESS)
                err_sys("failed to use certificate: certs/server-cert.pem");
    
    // To allow testing from several dirs
    if (SSL_CTX_use_PrivateKey_file(ctx, svrKey, SSL_FILETYPE_PEM)
         != SSL_SUCCESS) 
         if (SSL_CTX_use_PrivateKey_file(ctx, svrKey2, SSL_FILETYPE_PEM)
            != SSL_SUCCESS) 
                if (SSL_CTX_use_PrivateKey_file(ctx, svrKey3,SSL_FILETYPE_PEM)
                    != SSL_SUCCESS) 
                    err_sys("failed to use key file: certs/server-key.pem");
}


// dsa server
inline void set_dsaServerCerts(SSL_CTX* ctx)
{
    store_ca(ctx);

    // To allow testing from serveral dirs
    if (SSL_CTX_use_certificate_file(ctx, dsaCert, SSL_FILETYPE_PEM)
        != SSL_SUCCESS)
        if (SSL_CTX_use_certificate_file(ctx, dsaCert2, SSL_FILETYPE_PEM)
            != SSL_SUCCESS)
            if (SSL_CTX_use_certificate_file(ctx, dsaCert3, SSL_FILETYPE_PEM)
                != SSL_SUCCESS)
                err_sys("failed to use certificate: certs/dsa-cert.pem");
    
    // To allow testing from several dirs
    if (SSL_CTX_use_PrivateKey_file(ctx, dsaKey, SSL_FILETYPE_ASN1)
         != SSL_SUCCESS) 
         if (SSL_CTX_use_PrivateKey_file(ctx, dsaKey2, SSL_FILETYPE_ASN1)
            != SSL_SUCCESS) 
                if (SSL_CTX_use_PrivateKey_file(ctx, dsaKey3,SSL_FILETYPE_ASN1)
                    != SSL_SUCCESS) 
                    err_sys("failed to use key file: certs/dsa512.der");
}


inline void set_args(int& argc, char**& argv, func_args& args)
{
    argc = args.argc;
    argv = args.argv;
    args.return_code = -1; // error state
}


inline void tcp_socket(SOCKET_T& sockfd, sockaddr_in& addr)
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    addr.sin_port = htons(yasslPort);
    addr.sin_addr.s_addr = inet_addr(yasslIP);
}


inline void tcp_close(SOCKET_T& sockfd)
{
#ifdef _WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif
    sockfd = -1;
}


inline void tcp_connect(SOCKET_T& sockfd)
{
    sockaddr_in addr;
    tcp_socket(sockfd, addr);

    if (connect(sockfd, (const sockaddr*)&addr, sizeof(addr)) != 0)
    {
        tcp_close(sockfd);
        err_sys("tcp connect failed");
    }
}


inline void tcp_listen(SOCKET_T& sockfd)
{
    sockaddr_in addr;
    tcp_socket(sockfd, addr);

    if (bind(sockfd, (const sockaddr*)&addr, sizeof(addr)) != 0)
    {
        tcp_close(sockfd);
        err_sys("tcp bind failed");
    }
    if (listen(sockfd, 3) != 0)
    {
        tcp_close(sockfd);
        err_sys("tcp listen failed");
    }
}


inline void tcp_accept(SOCKET_T& sockfd, SOCKET_T& clientfd, func_args& args)
{
    tcp_listen(sockfd);

    sockaddr_in client;
    socklen_t client_len = sizeof(client);

#if defined(_POSIX_THREADS) && defined(NO_MAIN_DRIVER)
    // signal ready to tcp_accept
    tcp_ready& ready = *args.signal_;
    pthread_mutex_lock(&ready.mutex_);
    ready.ready_ = true;
    pthread_cond_signal(&ready.cond_);
    pthread_mutex_unlock(&ready.mutex_);
#endif

    clientfd = accept(sockfd, (sockaddr*)&client, (ACCEPT_THIRD_T)&client_len);

    if (clientfd == -1)
    {
        tcp_close(sockfd);
        err_sys("tcp accept failed");
    }
}


inline void showPeer(SSL* ssl)
{
    X509* peer = SSL_get_peer_certificate(ssl);
    if (peer) {
        char* issuer  = X509_NAME_oneline(X509_get_issuer_name(peer), 0, 0);
        char* subject = X509_NAME_oneline(X509_get_subject_name(peer), 0, 0);

        printf("peer's cert info:\n");
        printf("issuer : %s\n", issuer);
        printf("subject: %s\n", subject);

        free(subject);
        free(issuer);
    }
    else
        printf("peer has no cert!\n");
}



inline DH* set_tmpDH(SSL_CTX* ctx)
{
    static unsigned char dh512_p[] =
    {
      0xDA,0x58,0x3C,0x16,0xD9,0x85,0x22,0x89,0xD0,0xE4,0xAF,0x75,
      0x6F,0x4C,0xCA,0x92,0xDD,0x4B,0xE5,0x33,0xB8,0x04,0xFB,0x0F,
      0xED,0x94,0xEF,0x9C,0x8A,0x44,0x03,0xED,0x57,0x46,0x50,0xD3,
      0x69,0x99,0xDB,0x29,0xD7,0x76,0x27,0x6B,0xA2,0xD3,0xD4,0x12,
      0xE2,0x18,0xF4,0xDD,0x1E,0x08,0x4C,0xF6,0xD8,0x00,0x3E,0x7C,
      0x47,0x74,0xE8,0x33,
    };

    static unsigned char dh512_g[] =
    {
      0x02,
    };

    DH* dh;
    if ( (dh = DH_new()) ) {
        dh->p = BN_bin2bn(dh512_p, sizeof(dh512_p), 0);
        dh->g = BN_bin2bn(dh512_g, sizeof(dh512_g), 0);
    }
    if (!dh->p || !dh->g) {
        DH_free(dh);
        dh = 0;
    }
    SSL_CTX_set_tmp_dh(ctx, dh);
    return dh;
}


#endif // yaSSL_TEST_HPP

