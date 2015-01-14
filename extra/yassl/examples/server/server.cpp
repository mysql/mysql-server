/*
   Copyright (c) 2006, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING. If not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
   MA  02110-1301  USA.
*/

/* server.cpp */

// takes 2 optional command line argument to make scripting
// if the first  command line argument is 'n' client auth is disabled
// if the second command line argument is 'd' DSA certs are used instead of RSA

#include "../../testsuite/test.hpp"


void ServerError(SSL_CTX* ctx, SSL* ssl, SOCKET_T& sockfd, const char* msg)
{
    SSL_CTX_free(ctx);
    SSL_free(ssl);
    tcp_close(sockfd);
    err_sys(msg);
}


#ifdef NON_BLOCKING
    void NonBlockingSSL_Accept(SSL* ssl, SSL_CTX* ctx, SOCKET_T& clientfd)
    {
        int ret = SSL_accept(ssl);
        int err = SSL_get_error(ssl, 0);
        while (ret != SSL_SUCCESS && (err == SSL_ERROR_WANT_READ ||
                                      err == SSL_ERROR_WANT_WRITE)) {
            if (err == SSL_ERROR_WANT_READ)
                printf("... server would read block\n");
            else
                printf("... server would write block\n");
            #ifdef _WIN32
                Sleep(1000);
            #else
                sleep(1);
            #endif
            ret = SSL_accept(ssl);
            err = SSL_get_error(ssl, 0);
        }
        if (ret != SSL_SUCCESS)
            ServerError(ctx, ssl, clientfd, "SSL_accept failed");
    }
#endif


THREAD_RETURN YASSL_API server_test(void* args)
{
#ifdef _WIN32
    WSADATA wsd;
    WSAStartup(0x0002, &wsd);
#endif

    SOCKET_T sockfd   = 0;
    SOCKET_T clientfd = 0;
    int      argc     = 0;
    char**   argv     = 0;

    set_args(argc, argv, *static_cast<func_args*>(args));
#ifdef SERVER_READY_FILE
    set_file_ready("server_ready", *static_cast<func_args*>(args));
#endif
    tcp_accept(sockfd, clientfd, *static_cast<func_args*>(args));

    tcp_close(sockfd);

    SSL_METHOD* method = TLSv1_server_method();
    SSL_CTX*    ctx = SSL_CTX_new(method);

    //SSL_CTX_set_cipher_list(ctx, "RC4-SHA:RC4-MD5");
    
    // should we disable client auth
    if (argc >= 2 && argv[1][0] == 'n')
        printf("disabling client auth\n");
    else
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, 0);

    // are we using DSA certs
    if (argc >= 3 && argv[2][0] == 'd') {
        printf("using DSA certs\n");
        set_dsaServerCerts(ctx);
    }
    else {
        set_serverCerts(ctx);
    }
    DH* dh = set_tmpDH(ctx);

    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, clientfd);

#ifdef NON_BLOCKING
    NonBlockingSSL_Accept(ssl, ctx, clientfd);
#else
    if (SSL_accept(ssl) != SSL_SUCCESS)
        ServerError(ctx, ssl, clientfd, "SSL_accept failed");
#endif
     
    showPeer(ssl);
    printf("Using Cipher Suite: %s\n", SSL_get_cipher(ssl));

    char command[1024];
    int input = SSL_read(ssl, command, sizeof(command));
    if (input > 0) {
        command[input] = 0;
        printf("First client command: %s\n", command);
    }

    char msg[] = "I hear you, fa shizzle!";
    if (SSL_write(ssl, msg, sizeof(msg)) != sizeof(msg))
        ServerError(ctx, ssl, clientfd, "SSL_write failed");

    DH_free(dh);
    SSL_CTX_free(ctx);
    SSL_shutdown(ssl);
    SSL_free(ssl);

    tcp_close(clientfd);

    ((func_args*)args)->return_code = 0;
    return 0;
}


#ifndef NO_MAIN_DRIVER

    int main(int argc, char** argv)
    {
        func_args args;

        args.argc = argc;
        args.argv = argv;

        server_test(&args);
        yaSSL_CleanUp();

        return args.return_code;
    }

#endif // NO_MAIN_DRIVER

