/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* **************************************************************** */
/*                                                                  */
/*  S E R V . T C P                                                 */
/*  * This is an example program that demonstrates the use of       */
/*    stream sockets as an IPC mechanism. This contains the server, */
/*    and is intended to operate in conjunction with the client     */
/*    program found in client.tcp. Together, these two programs     */
/*    demonstrate many of the features of sockets, as well as good  */
/*    conventions for using these features.                         */
/*  * This program provides a service called "example". In order for*/
/*    it to function, an entry for it needs to exist in the         */
/*    ./etc/services file. The port address for this service can be */
/*    any port number that is likely to be unused, such as 22375,   */
/*    for example. The host on which the client will be running     */
/*    must also have the same entry (same port number) in its       */
/*    ./etc/services file.                                          */
/* **************************************************************** */

#include <ndb_global.h>

/******** NDB INCLUDE ******/
#include <NdbApi.hpp>
/***************************/
/*#include <sys/shm.h>*/
#include <pthread.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <time.h>
#include <synch.h>
#include <sched.h>

extern "C" {
#include "utv.h"
#include "vcdrfunc.h"
#include "bcd.h"
}

#ifndef TESTLEV
#define TESTLEV
#endif
//#define DEBUG
//#define MYDEBUG
//#define SETDBG

//#define ops_before_exe 64
#define MAXOPSEXEC 1024

/* Used in nanosleep */
/**** NDB ********/
static  int bTestPassed;
void create_table(Ndb* pMyNdb);
void error_handler(const char* errorText);
/*****************/
static struct timespec tmspec1;
static int server(long int);

/* Function for initiating the cdr-area and make it clean for ongoing calls */

static int s;                          /* connected socket descriptor */
static int ls;                         /* listen socket descriptor */

static struct hostent *hp;             /* pointer to host info for remote host */
static struct servent *sp;             /* pointer to service information */

struct linger linger;           /* allow a lingering, graceful close; */
                                /* used when setting SO_LINGER */

static struct sockaddr_in myaddr_in;   /* for local socket address */
static struct sockaddr_in peeraddr_in; /* for peer socket address */

static FILE *fi;                       /* Log output */
static char temp[600]="";

static int ops_before_exe = 1;  /* Number of operations per execute, default is 1,
				   but it can be changed with the -o parameter. */

/*----------------------------------------------------------------------

     M A I N
     * This routine starts the server. It forks, leaving the child
     to do all the work, so it does not have to be run in the
     background. It sets up the listen socket, and for each incoming
     connection, it forks a child process to process the data. It
     will loop forever, until killed by a signal.

  ----------------------------------------------------------------------*/

/****** NDB *******/
static char *tableName = "VWTABLE";
/******************/

#include <iostream>
using namespace std;

int main(int argc, const char** argv)
{
  ndb_init();
        /******** NDB ***********/
	/*
        Ndb                   MyNdb( "TEST_DB" );
        int                   tTableId;
	*/
        /************************/
	char 	tmpbuf[400];
	/* Loop and status variables */
	int 		i,j,found;

	/* Used by the server */
        int 		addrlen;

	/* return code used with functions */
	int 		rc;

  i = 1;
  while (argc > 1)
  {
    if (strcmp(argv[i], "-o") == 0)
    {
      ops_before_exe = atoi(argv[i+1]);
      if ((ops_before_exe < 1) || (ops_before_exe > MAXOPSEXEC))
      {
	cout << "Number of operations per execute must be at least 1, and at most " << MAXOPSEXEC << endl;
	exit(1);
      }

    }
    else
    {
      cout << "Invalid parameter!" << endl << "Look in cdrserver.C for more info." << endl;
      exit(1);
    }

    argc -= 2;
    i = i + 2;
  }

	
        /* Setup log handling */
        logname(temp,"Cdrserver","Mother","");
	puts(temp);
        fi=fopen(temp,"w");
	if (fi == NULL)
	{
		perror(argv[0]);
		exit(EXIT_FAILURE);
	}
        m2log(fi,"Initiation of program");

        /***** NDB ******/
	/*
        MyNdb.init();
        if (MyNdb.waitUntilReady(30) != 0)
        {
                puts("Not ready");
                exit(-1);
        }
        tTableId = MyNdb.getTable()->openTable(tableName);
        if (tTableId == -1)
        {
                printf("%d: Creating table",getpid());
                create_table(&MyNdb);
        }
	else printf("%d: Table already create",getpid());
	*/

        /****************/

        /* clear out address structures */
        memset ((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
        memset ((char *)&peeraddr_in, 0, sizeof(struct sockaddr_in));

        m2log(fi,"Socket setup starting");

        /* Set up address structure for the listen socket. */
        myaddr_in.sin_family = AF_INET;

        /* The server should listen on the wildcard address,            */
        /*   rather than its own internet address. This is              */
        /*   generally good practice for servers, because on            */
        /*   systems which are connected to more than one               */
        /*   network at once will be able to have one server            */
        /*   listening on all networks at once. Even when the           */
        /*   host is connected to only one network, this is good        */
        /*   practice, because it makes the server program more         */
        /*   portable.                                                  */

        myaddr_in.sin_addr.s_addr = INADDR_ANY;
        /* Find the information for the "cdrserver" server              */
        /*   in order to get the needed port number.                    */

        sp = getservbyname ("cdrserver", "tcp");
        if (sp == NULL) {
                m2log(fi,"Service cdrserver not found in /etc/services");
                m2log(fi,"Terminating.");
                exit(EXIT_FAILURE);
        }

        myaddr_in.sin_port = sp->s_port;

        /* Create the listen socket.i                                   */

        ls = socket (AF_INET, SOCK_STREAM, 0);
        if (ls == -1) {
                m2log(fi,"Unable to create socket");
                m2log(fi,"Terminating.");
                exit(EXIT_FAILURE);
        }
        printf("Socket created\n");
        printf("Wait..........\n");
        /* Bind the listen address to the socket.                       */
        if (bind(ls,(struct sockaddr*)&myaddr_in, sizeof(struct sockaddr_in)) == -1) {
                m2log(fi,"Unable to bind address");
                m2log(fi,"Terminating.");
                exit(EXIT_FAILURE);
        }

        /* Initiate the listen on the socket so remote users            */
        /*   can connect. The listen backlog is set to 5, which         */
        /*   is the largest currently supported.                        */

        if (listen(ls, 5) == -1) {
                m2log(fi,"Unable to listen on socket");
                m2log(fi,"Terminating.");
                exit(EXIT_FAILURE);
        }

        /* Now, all the initialization of the server is                 */
        /*   complete, and any user errors will have already            */
        /*   been detected. Now we can fork the daemon and              */
        /*   return to the user. We need to do a setpgrp                */
        /*   so that the daemon will no longer be associated            */
        /*   with the user's control terminal. This is done             */
        /*   before the fork, so that the child will not be             */
        /*   a process group leader. Otherwise, if the child            */
        /*   were to open a terminal, it would become associated        */
        /*   with that terminal as its control terminal. It is          */
        /*   always best for the parent to do the setpgrp.              */

        m2log(fi,"Socket setup completed");
        m2log(fi,"Start server");

        setpgrp();
	
	/* Initiate the tmspec struct for use with nanosleep() */
        tmspec1.tv_sec = 0;
        tmspec1.tv_nsec = 1;

	printf("Waiting for client to connect.........\n");
	printf("Done\n");
        switch (fork()) {
           case -1: /* Unable to fork, for some reason. */
                m2log(fi,"Failed to start server");
                m2log(fi,"Terminating.");
                fclose(fi);
                perror(argv[0]);
                fprintf(stderr, "%s: unable to fork daemon\n", argv[0]);
                exit(EXIT_FAILURE);

	   break;
           case 0: /* The child process (daemon) comes here.            */
                m2log(fi,"Server started");

               /* Close stdin and stderr so that they will not          */
               /*   be kept open. Stdout is assumed to have been        */
               /*   redirected to some logging file, or /dev/null.      */
               /*   From now on, the daemon will not report any         */
               /*   error messages. This daemon will loop forever,      */
               /*   waiting for connections and forking a child         */
               /*   server to handle each one.                          */

               close((int)stdin);
               close((int)stderr);
               /* Set SIGCLD to SIG_IGN, in order to prevent            */
               /*   the accumulation of zombies as each child           */
               /*   terminates. This means the daemon does not          */
               /*   have to make wait calls to clean them up.           */

               signal(SIGCLD, SIG_IGN);
               for(EVER) {
			if ((checkchangelog(fi,temp))==0)
                          m2log(fi,"Waiting for connection");
                       /* Note that addrlen is passed as a pointer      */
                       /*  so that the accept call can return the       */
                       /*  size of the returned address.                */

                       addrlen = sizeof(struct sockaddr_in);

                       /* This call will block until a new              */
                       /*   connection arrives. Then, it will           */
                       /*   return the address of the connecting        */
                       /*   peer, and a new socket descriptor, s,       */
                       /*   for that connection.                        */

                        s = accept(ls,(struct sockaddr*) &peeraddr_in, &addrlen);
		       #ifdef MYDEBUG
			   puts("accepted");
		       #endif
			if ((checkchangelog(fi,temp))==0)
                          m2log(fi,"Connection attempt from a client");
			if ((checkchangelog(fi,temp))==0)
                          m2log(fi,"Start communication server");

                        if ( s == -1) exit(EXIT_FAILURE);
                        switch (fork()) {
                           case -1: /* Can't fork, just exit. */
				if ((checkchangelog(fi,temp))==0)
                                  m2log(fi,"Start communication server failed.");
                                exit(EXIT_FAILURE);
			   break;
                           case 0: /* Child process comes here. */

				/* Get clients adress and save it in the info area */
				/* Keep track of how many times the client connects to the server */
				printf("Connect attempt from client %u\n",peeraddr_in.sin_addr.s_addr);
                                server(peeraddr_in.sin_addr.s_addr);
                                exit(EXIT_FAILURE);
			   break;
                           default: /* Daemon process comes here. */
                                /* The daemon needs to remember         */
                                /*     to close the new accept socket   */
                                /*     after forking the child. This    */
                                /*     prevents the daemon from running */
                                /*     out of file descriptor space. It */
                                /*     also means that when the server  */
                                /*     closes the socket, that it will  */
                                /*     allow the socket to be destroyed */
                                /*     since it will be the last close. */
                                close(s);
			   break;
                        }
               }
           default: /* Parent process comes here. */
               exit(EXIT_FAILURE);
       }
  return EXIT_SUCCESS;
}

/*----------------------------------------------------------------------

    S E R V E R
    * This is the actual server routine that the daemon forks to
      handle each individual connection. Its purpose is to receive
      the request packets from the remote client, process them,
      and return the results to the client. It will also write some
      logging information to stdout.

  ----------------------------------------------------------------------*/

server(long int servernum)
{
	/******** NDB ***********/
  	Ndb                   MyNdb( "TEST_DB" );
  	int                   tTableId;
	NdbConnection		*MyTransaction;
	NdbOperation		*MyOperation;
	int			check;
	int			c1 = 0;
	int			c2 = 0;
	int			c3 = 0;
	int			c4 = 0;
	int			act_index = 0;
	/************************/
        register unsigned int   reqcnt;         /* keeps count of number of requests */
        register unsigned int   i;          	/* Loop counters */
	register int		x;
        register short          done;           /* Loop variable */
	short int		found;

	/* The server index number */
	int 			thisServer;

	/* Variables used to keep track of some statistics */
	time_t			ourtime;
	time_t			tmptime;
	int 			tmpvalue;
	long int		tmptransfer;
	long int		transfer;
	int			ops = 0;

	/* Variables used by the server */
        char    		buf[400];       /* This example uses 10 byte messages. */
        char    		*inet_ntoa();
        char    		*hostname;      /* points to the remote host's name string */
        int     		len;
        int     		rcvbuf_size;

	long			ctid;

        unsigned char 		uc;

	/* Variables used by the logging facilitiy */
        char    		msg[600];
        char    		crap[600];
        char    		lognamn[600];

        FILE    		*log;

	/* scheduling parameter for pthread */
	struct sched_param 	param1,param2,param3;

        /* Header information */
	/* cdrtype not used */
        /*short           	cdrtype;   */   /* 1 CDR Typ                                            */
        short           	cdrlen;         /* 2 CDR recored length in bytes excluding CDR type     */
        short           	cdrsubtype;     /* 1 CDR subtype                                        */
        unsigned int    	cdrid;          /* 8 CDR unique number of each call                     */
        unsigned int    	cdrtime;        /* 4 CDR Time in seconds                                */
        short           	cdrmillisec;    /* 2 CDR Milliseconds                                   */
        short           	cdrstatus;      /* 1 CDR For future use                                 */
        short           	cdrequipeid;    /* 1 CDR Equipment id                                   */
        int             	cdrreserved1;   /* 4 CDR For future use                                 */

        /* Defined or calculated for each record */
        int             	cdrrestlen;     /*   Unprocessed data left in record in bytes   */

        /* Gemensamma datatyper */
        unsigned short  	parmtype_prev;  /* 1 Parameter type                                     */
        unsigned short  	parmtype;       /* 1 Parameter type                                     */
        unsigned short  	parmlen;        /* 1 Parameter type                                     */

	int			rc;		/* return code for functions */
	
	/* Attribute object used with threads */
	pthread_attr_t 		attr1;
	pthread_attr_t 		attr2;
	pthread_attr_t 		attr3;
	struct cdr_record	*tmpcdrptr,*ftest;
	void			*dat;

	int			error_from_client = 0;

        /* Konstanter           */
        const int       	headerlen = 24;         /*   Length of header record                    */

        parmtype_prev = 99;
	reqcnt = 0;

        /* Close the listen socket inherited from the daemon. */
        close(ls);

	printf("Use the readinfo program to get information about server status\n\n");

	if((checkchangelog(fi,temp))==0)
          c2log(fi,"Communication server started");

        /* Look up the host information for the remote host     */
        /* that we have connected with. Its internet address    */
        /* was returned by the accept call, in the main         */
        /* daemon loop above.                                   */

        hp=gethostbyaddr((char *) &peeraddr_in.sin_addr,sizeof(struct in_addr),peeraddr_in.sin_family);

        if (hp == NULL) {
                /* The information is unavailable for the remote        */
                /* host. Just format its internet address to be         */
                /* printed out in the logging information. The          */
                /* address will be shown in "internet dot format".      */

		/*
                hostname = inet_ntoa(peeraddr_in.sin_addr);
		*/
		sprintf(hostname,"Test");
                logname(lognamn,"Cdrserver","Child",hostname);
        }
        else {
                hostname = hp->h_name; /* point to host's name */
                logname(lognamn,"Cdrserver","Child",hostname);
        }

        log=fopen(lognamn,"w");
	if (log == NULL)
	{
		perror(hostname);
		exit(EXIT_FAILURE);
	}
        n2log(log,"Setup in progress");
        /* Log a startup message. */

        /* The port number must be converted first to host byte */
        /* order before printing. On most hosts, this is not    */
        /* necessary, but the ntohs() call is included here so  */
        /* that this program could easily be ported to a host   */
        /* that does require it.                                */

        BaseString::snprintf(msg,sizeof(msg),"Startup from %s port %u",hostname,ntohs(peeraddr_in.sin_port));
	if ((checkchangelog(fi,temp))==0)
          c2log(fi,msg);
        n2log(log,msg);
        BaseString::snprintf(msg,sizeof(msg),"For further information, see log(%s)",lognamn);
	if ((checkchangelog(fi,temp))==0)
          c2log(fi,msg);

        /* Set the socket for a lingering, graceful close.              */
        /* This will cause a final close of this socket to wait until   */
        /* all * data sent on it has been received by the remote host.  */

        linger.l_onoff  =1;
        linger.l_linger =0;
        if (setsockopt(s, SOL_SOCKET, SO_LINGER,(const char*)&linger,sizeof(linger)) == -1) {
                BaseString::snprintf(msg,sizeof(msg),"Setting SO_LINGER, l_onoff=%d, l_linger=%d",linger.l_onoff,linger.l_linger);
		if ((checkchangelog(log,lognamn))==0)
          		n2log(log,msg);
                goto errout;
        }

        /* Set the socket for a lingering, graceful close.                              */
        /* This will cause a final close of this socket to wait until all * data sent   */
        /* on it has been received by the remote host.                                  */

        rcvbuf_size=64*1024;

        if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,(const char*) &rcvbuf_size,sizeof(rcvbuf_size)) == -1) {
                BaseString::snprintf(msg,sizeof(msg),"Setting SO_RCVBUF = %d",rcvbuf_size);
		if ((checkchangelog(log,lognamn))==0)
          		n2log(log,msg);
                goto errout;
        }

        /* Set nodelay on socket */
        n2log(log,"Port setup complete");

        /* Go into a loop, receiving requests from the remote           */
        /* client. After the client has sent the last request,          */
        /* it will do a shutdown for sending, which will cause          */
        /* an end-of-file condition to appear on this end of the        */
        /* connection. After all of the client's requests have          */
        /* been received, the next recv call will return zero           */
        /* bytes, signalling an end-of-file condition. This is          */
        /* how the server will know that no more requests will          */
        /* follow, and the loop will be exited.                         */

        n2log(log,"Setup completed");

	/* Fetch the process id for the server */

	/* Inititate the variables used for counting transfer rates and rec/sec */
	tmpvalue    = 0;
	tmptime     = 0;
	tmptransfer = 0;
	transfer = 0;

	printf("Client %s connected\nStarting to process the data\n\n",hostname);

	tmpcdrptr = (struct cdr_record*)malloc(sizeof(struct cdr_record));

	/***** NDB ******/
	MyNdb.init();
	if (MyNdb.waitUntilReady(30) != 0)
	{
		puts("Not ready");
		exit(-1);
	}
	tTableId = MyNdb.getTable()->openTable(tableName);
      	if (tTableId == -1) 
	{
		printf("%d: Creating table",getpid());
        	create_table(&MyNdb);
	}	
	else printf("%d: Table already created",getpid());
      
	/****************/

        while (len = recv(s,buf,headerlen,MSG_WAITALL)) {
                if (len == -1) {
        		snprintf(msg,sizeof(msg),"Error from recv");
			if ((checkchangelog(log,lognamn))==0)
          			n2log(log,msg);
                        goto errout; /* error from recv */
                }

                /* The reason this while loop exists is that there              */
                /* is a remote possibility of the above recv returning          */
                /* less than 10 bytes. This is because a recv returns           */
                /* as soon as there is some data, and will not wait for         */
                /* all of the requested data to arrive. Since 10 bytes          */
                /* is relatively small compared to the allowed TCP              */
                /* packet sizes, a partial receive is unlikely. If              */
                /* this example had used 2048 bytes requests instead,           */
                /* a partial receive would be far more likely.                  */
                /* This loop will keep receiving until all 10 bytes             */
                /* have been received, thus guaranteeing that the               */
                /* next recv at the top of the loop will start at               */
                /* the begining of the next request.                            */

                for (;len < headerlen;) {
                        x = recv(s,buf,(headerlen-len),0);
                        if (x == -1) {
        			snprintf(msg,sizeof(msg),"Error from recv");
				if ((checkchangelog(log,lognamn))==0)
          				n2log(log,msg);
                                goto errout; /* error from recv */
                        }
                        len=len+x;
                }

                if (ops == 0) {
            	  MyTransaction = MyNdb.startTransaction();
        	  if (MyTransaction == NULL)
        		error_handler(MyNdb.getNdbErrorString());
                }//if

      		MyOperation = MyTransaction->getNdbOperation(tableName);
      		if (MyOperation == NULL)
        		error_handler(MyTransaction->getNdbErrorString());
                /*------------------------------------------------------*/
                /* Parse header of CDR records                          */
                /*------------------------------------------------------*/

                /*------------------------------------------------------*/
                /* 1. Type of cdr                                       */
                /*------------------------------------------------------*/
		/* Not used for the moment
                cdrtype=(char)buf[0];
		*/
                /*------------------------------------------------------*/
                /* 2. Total length of CDR                               */
                /*------------------------------------------------------*/
                swab(buf+1,buf+1,2);
                memcpy(&cdrlen,buf+1,2);
                /*------------------------------------------------------*/
                /* 3. Partial type of CDR                               */
                /*------------------------------------------------------*/
                cdrsubtype=(char)buf[3];
		switch (cdrsubtype)
		{
			case 0:
			c1++;
			tmpcdrptr->CallAttemptState = 1;
			check = MyOperation->insertTuple();
			break;
			case 1:
			c2++;
			tmpcdrptr->CallAttemptState = 2;
			check = MyOperation->updateTuple();
			break;
			case 2:
			c3++;
			tmpcdrptr->CallAttemptState = 3;
			check = MyOperation->deleteTuple();
			break;
			case 3:
			c4++;
			tmpcdrptr->CallAttemptState = 4;
			check = MyOperation->deleteTuple();
			break;
			if (check == -1)
				error_handler(MyTransaction->getNdbErrorString());
		}
                /*cdrsubtype=(cdrsubtype << 24) >> 24;*/
                /*------------------------------------------------------*/
                /* 4. ID number                                         */
                /*------------------------------------------------------*/
                /*swab(buf+4,buf+4,4);*/ /* ABCD -> BADC */
		/*
                swab(buf+4,buf+4,4);
                swab(buf+5,buf+5,2);
                swab(buf+6,buf+6,2);
                swab(buf+4,buf+4,2);
                swab(buf+5,buf+5,2);
		*/
                memcpy(&cdrid,buf+4,4);
		tmpcdrptr->CallIdentificationNumber = cdrid;
		#ifdef SETDBG
			puts("CIN");
		#endif
		check = MyOperation->equal("CIN",(char*)&cdrid);
		if (check == -1)
			error_handler(MyTransaction->getNdbErrorString());
		#ifdef SETDBG
			puts("CAS");
		#endif

		if (cdrsubtype < 2)
		{
			check = MyOperation->setValue("CAS",(char*)&cdrsubtype);
			if (check == -1)
				error_handler(MyTransaction->getNdbErrorString());
		}
                /*------------------------------------------------------*/
                /* 5. Time stamp                                        */
                /*------------------------------------------------------*/
                swab(buf+12,buf+12,4);
                swab(buf+13,buf+13,2);
                swab(buf+14,buf+14,2);
                swab(buf+12,buf+12,2);
                swab(buf+13,buf+13,2);
                memcpy(&cdrtime,buf+12,4);
		switch (cdrsubtype)
		{
			case 0:
			#ifdef SETDBG
				puts("START_TIME");
			#endif
			check = MyOperation->setValue("START_TIME",(char*)&cdrtime);
			break;
			case 1:
			#ifdef SETDBG
				puts("Start1");
			#endif
			check = MyOperation->setValue("StartOfCharge",(char*)&cdrtime);
			break;
			case 2:
			#ifdef SETDBG
				puts("Start2");
			#endif
			/*
			check = MyOperation->setValue("StopOfCharge",(char*)&cdrtime);
			*/
			check = 0;
			break;
			if (check == -1)
				error_handler(MyTransaction->getNdbErrorString());
		}
                /*------------------------------------------------------*/
                /* 6. Milliseconds                                      */
                /*------------------------------------------------------*/
		/* Not used by application
                swab(buf+16,buf+16,2);
                memcpy(&cdrmillisec,buf+16,2);
		*/
                /*------------------------------------------------------*/
                /* 7. CDR status reserverd for future use               */
                /*------------------------------------------------------*/
		/* Not used by application
                memcpy(&cdrstatus,buf+18,1);
		*/
                /*------------------------------------------------------*/
                /* 8. CDR equipe id, number of sending equipement       */
                /*------------------------------------------------------*/
		/* Not used by application
                memcpy(&cdrequipeid,buf+19,1);
		*/
                /*cdrequipeid=(cdrequipeid << 24) >> 24;*/
                /*------------------------------------------------------*/
                /* 9. CDR reserverd for furter use                      */
                /*------------------------------------------------------*/
		/* Not used by applikation
                swab(buf+20,buf+20,4);
                swab(buf+21,buf+21,2);
                swab(buf+22,buf+22,2);
                swab(buf+20,buf+20,2);
                swab(buf+21,buf+21,2);
                memcpy(&cdrreserved1,buf+20,4);
		*/
                /*------------------------------------------------------*/
                /* calculate length of datapart in record               */
                /* Formula recordlength-headerlen-1                     */
                /*------------------------------------------------------*/
                cdrrestlen=cdrlen-(headerlen-1);
                /*------------------------------------------------------*/
                /* Finished with header                                 */
                /*------------------------------------------------------*/
                /* Read remaining cdr data into buffer for furter       */
                /* handling.                                            */
                /*------------------------------------------------------*/
                len = recv(s,buf,cdrrestlen,MSG_WAITALL);
                if (len == -1) {
        		snprintf(msg,sizeof(msg),"Error from recv");
			if ((checkchangelog(log,lognamn))==0)
          			n2log(log,msg);
                        goto errout; /* error from recv */
                }
                for (;len<cdrrestlen;) {
                        x = recv(s,buf,len-cdrrestlen,0);
                        if (x == -1) {
        			snprintf(msg,sizeof(msg),"Error from recv");
				if ((checkchangelog(log,lognamn))==0)
          				n2log(log,msg);
                                goto errout; /* error from recv */
                        }
                        len=len+x;
                }
                done=FALSE;

		/* Count the transfer/sec */
		tmptransfer += cdrlen;
		if (cdrsubtype > 1) 
		{
				#ifdef SETDBG
					puts("Going to execute");
				#endif
                                ops++;
                                if (ops == ops_before_exe) {
                                  ops = 0;
                		  check = MyTransaction->execute(Commit, CommitAsMuchAsPossible);
                		  if ((check == -1) && (MyTransaction->getNdbError() != 0))
                        		error_handler(MyTransaction->getNdbErrorString());
                		  MyNdb.closeTransaction(MyTransaction);
				  #ifdef SETDBG
					puts("Transaction closed");
				  #endif
                                }//if
			reqcnt++;
			continue;
		}
                for (x=0;x<=cdrrestlen && !done && cdrrestlen > 1;) {
                        uc=buf[x];
                        parmtype=uc;
                        /*parmtype=(parmtype << 24) >> 24;*/ /* Modified in sun worked in hp */

                        parmlen = buf[x+1];
                        /*parmlen =(parmlen << 24) >> 24;*/
                        x+=2;

                        switch (parmtype) {
                                case 4:         /* Called party number */
                                        bcd_decode2(parmlen,&buf[x],crap);
					tmpcdrptr->BSubscriberNumberLength = (char)parmlen;
                                        strcpy(tmpcdrptr->BSubscriberNumber,crap);
					tmpcdrptr->BSubscriberNumber[parmlen] = '\0';
                                        x=x+(parmlen/2);
                                        if (parmlen % 2) x++;
                                        tmpcdrptr->USED_FIELDS |=  B_BSubscriberNumber;
					#ifdef SETDBG
						puts("BNumber");
					#endif
					check = MyOperation->setValue("BNumber",(char*)&tmpcdrptr->BSubscriberNumber);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 9:         /* Calling Partys cataegory */
                                        if (parmlen != 1) printf("ERROR: Calling partys category has wrong length %d\n",parmlen);
                                        else tmpcdrptr->ACategory=(char)buf[x];
                                        x+=parmlen;
                                        tmpcdrptr->USED_FIELDS |= B_ACategory;
					#ifdef SETDBG
						puts("ACategory");
					#endif
					check = MyOperation->setValue("ACategory",(char*)&tmpcdrptr->ACategory);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 10:        /* Calling Party Number */
                                        bcd_decode2(parmlen,&buf[x],crap);
					tmpcdrptr->ASubscriberNumberLength = (char)parmlen;
                                        strcpy(tmpcdrptr->ASubscriberNumber,crap);
					tmpcdrptr->ASubscriberNumber[parmlen] = '\0';
                                        x=x+(parmlen/2);
                                        if (parmlen % 2) x++;
                                        tmpcdrptr->USED_FIELDS |= B_ASubscriberNumber;
					#ifdef SETDBG
						puts("ANumber");
					#endif
					check = MyOperation->setValue("ANumber",(char*)&tmpcdrptr->ASubscriberNumber);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 11:        /* Redirecting number */
                                        bcd_decode2(parmlen,&buf[x],crap);
                                        strcpy(tmpcdrptr->RedirectingNumber,crap);
                                        x=x+(parmlen/2);
                                        if (parmlen % 2) x++;
                                        tmpcdrptr->USED_FIELDS |= B_RedirectingNumber;
					#ifdef SETDBG
						puts("RNumber");
					#endif
					check = MyOperation->setValue("RNumber",(char*)&tmpcdrptr->RedirectingNumber);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 17:        /* Called partys category */
                                        if (parmlen != 1) printf("ERROR: Called partys category has wrong length %d\n",parmlen);
                                        else tmpcdrptr->EndOfSelectionInformation=(char)buf[x];
                                        x+=parmlen;
                                        tmpcdrptr->USED_FIELDS |= B_EndOfSelectionInformation;
					#ifdef SETDBG
						puts("EndOfSelInf");
					#endif
					check = MyOperation->setValue("EndOfSelInf",(char*)&tmpcdrptr->EndOfSelectionInformation);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 18:        /* Release reason */
                                        if (parmlen != 1) printf("ERROR: Release reason has wrong length %d\n",parmlen);
                                        else tmpcdrptr->CauseCode=(char)buf[x];
                                        x+=parmlen;
                                        tmpcdrptr->USED_FIELDS |= B_CauseCode;
					#ifdef SETDBG
						puts("CauseCode");
					#endif
					check = MyOperation->setValue("CauseCode",(char*)&tmpcdrptr->CauseCode);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 19:        /* Redirection information */
                                        switch (parmlen) {
                                                case 1:
                                                        tmpcdrptr->ReroutingIndicator= (char)buf[x];
                                                        tmpcdrptr->USED_FIELDS |= B_ReroutingIndicator;
                                                break;
                                                case 2:
                                                        swab(buf+x,buf+x,2);
                                                        tmpcdrptr->ReroutingIndicator= buf[x];
                                                        tmpcdrptr->USED_FIELDS |= B_ReroutingIndicator;
                                                break;
                                                default :
                                                        BaseString::snprintf(msg,sizeof(msg),"ERROR: Redirection information has wrong length %d\n",parmlen);
	  						if ((checkchangelog(log,lognamn))==0)
                                                          n2log(log,msg);
                                                break;
						#ifdef SETDBG
							puts("RI");
						#endif
						check = MyOperation->setValue("RI",(char*)&tmpcdrptr->ReroutingIndicator);
						if (check == -1)
							error_handler(MyTransaction->getNdbErrorString());
                                        }
                                        x+=parmlen;
                                break;
                                case 32:        /* User to user information */
                                        if (parmlen != 1) printf("ERROR: User to User information has wrong length %d\n",parmlen);
                                        else tmpcdrptr->UserToUserInformation=(char)buf[x];
                                        x+=parmlen;
                                        tmpcdrptr->USED_FIELDS |= B_UserToUserInformation;
					#ifdef SETDBG
						puts("UserToUserInf");
					#endif
					check = MyOperation->setValue("UserToUserInf",(char*)&tmpcdrptr->UserToUserInformation);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 40:        /* Original called number */
                                        bcd_decode2(parmlen,&buf[x],crap);
                                        strcpy(tmpcdrptr->OriginalCalledNumber,crap);
                                        x=x+(parmlen/2);
                                        if (parmlen % 2) x++;
                                        tmpcdrptr->USED_FIELDS |= B_OriginalCalledNumber;
					#ifdef SETDBG
						puts("ONumber");
					#endif
					check = MyOperation->setValue("ONumber",(char*)&tmpcdrptr->OriginalCalledNumber);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 42:        /* User to user indicator */
                                        if (parmlen != 1) printf("ERROR: User to User indicator has wrong length %d\n",parmlen);
                                        else tmpcdrptr->UserToUserIndicatior=(char)buf[x];
                                        x+=parmlen;
                                        tmpcdrptr->USED_FIELDS |= B_UserToUserIndicatior;
					#ifdef SETDBG
						puts("UserToUserInd");
					#endif
					check = MyOperation->setValue("UserToUserInd",(char*)&tmpcdrptr->UserToUserIndicatior);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 63:        /* Location number */
                                        bcd_decode2(parmlen,&buf[x],crap);
                                        strcpy(tmpcdrptr->LocationCode,crap);
                                        x=x+(parmlen/2);
                                        if (parmlen % 2) x++;
                                        tmpcdrptr->USED_FIELDS |= B_LocationCode;
					#ifdef SETDBG
						puts("LocationCode");
					#endif
					check = MyOperation->setValue("LocationCode",(char*)&tmpcdrptr->LocationCode);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 240:       /* Calling Partys cataegory */
                                        if (parmlen != 1) printf("ERROR: Calling partys category has wrong length %d\n",parmlen);
                                        else tmpcdrptr->NetworkIndicator=(char)buf[x];
                                        x+=parmlen;
                                        tmpcdrptr->USED_FIELDS |= B_NetworkIndicator;
					#ifdef SETDBG
						puts("NIndicator");
					#endif
					check = MyOperation->setValue("NIndicator",(char*)&tmpcdrptr->NetworkIndicator);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 241:       /* Calling Partys cataegory */
                                        if (parmlen != 1) printf("ERROR: Calling partys category has wrong length %d\n",parmlen);
                                        else tmpcdrptr->TonASubscriberNumber=(char)buf[x];
                                        x+=parmlen;
                                        tmpcdrptr->USED_FIELDS |= B_TonASubscriberNumber;
					#ifdef SETDBG
						puts("TonANumber");
					#endif
					check = MyOperation->setValue("TonANumber",(char*)&tmpcdrptr->TonASubscriberNumber);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 242:       /* Calling Partys cataegory */
                                        if (parmlen != 1) printf("ERROR: Calling partys category has wrong length %d\n",parmlen);
                                        else tmpcdrptr->TonBSubscriberNumber=(char)buf[x];
                                        x+=parmlen;
                                        tmpcdrptr->USED_FIELDS |= B_TonBSubscriberNumber;
					#ifdef SETDBG
						puts("TonBNumber");
					#endif
					check = MyOperation->setValue("TonBNumber",(char*)&tmpcdrptr->TonBSubscriberNumber);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 243:       /* Calling Partys cataegory */
                                        if (parmlen != 1) printf("ERROR: Calling partys category has wrong length %d\n",parmlen);
                                        else tmpcdrptr->TonRedirectingNumber=(char)buf[x];
                                        x+=parmlen;
                                        tmpcdrptr->USED_FIELDS |= B_TonRedirectingNumber;
					#ifdef SETDBG
						puts("TonRNumber");
					#endif
					check = MyOperation->setValue("TonRNumber",(char*)&tmpcdrptr->TonRedirectingNumber);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 244:       /* Calling Partys cataegory */
                                        if (parmlen != 1) printf("ERROR: Calling partys category has wrong length %d\n",parmlen);
                                        else tmpcdrptr->TonOriginalCalledNumber=(char)buf[x];
                                        x+=parmlen;
                                        tmpcdrptr->USED_FIELDS |= B_TonOriginalCalledNumber;
					#ifdef SETDBG
						puts("TonONumber");
					#endif
					check = MyOperation->setValue("TonONumber",(char*)&tmpcdrptr->TonOriginalCalledNumber);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 245:       /* Calling Partys cataegory */
                                        if (parmlen != 1) printf("ERROR: Calling partys category has wrong length %d\n",parmlen);
                                        else tmpcdrptr->TonLocationCode=(char)buf[x];
                                        x+=parmlen;
                                        tmpcdrptr->USED_FIELDS |= B_TonLocationCode;
					#ifdef SETDBG
						puts("TonLocationCode");
					#endif
					check = MyOperation->setValue("TonLocationCode",(char*)&tmpcdrptr->TonLocationCode);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 252:       /* RINParameter Parameter */
                                        switch (parmlen) {
                                                case 1:
                                                        tmpcdrptr->RINParameter=buf[x];
                                                        tmpcdrptr->USED_FIELDS |= B_RINParameter;
                                                break;
                                                case 2:
                                                        swab(buf+x,buf+x,2);
                                                        tmpcdrptr->RINParameter = buf[x] << 8;
                                                        tmpcdrptr->USED_FIELDS |= B_RINParameter;
                                                break;
                                                default :
                                                        BaseString::snprintf(msg,sizeof(msg),"ERROR: Rin parameter has wrong length %d\n",parmlen);
	  						if ((checkchangelog(log,lognamn))==0)
                                                          n2log(log,msg);
                                                break;
                                        }
                                        x+=parmlen;
					#ifdef SETDBG
						puts("RINParameter");
					#endif
					check = MyOperation->setValue("RINParameter",(char*)&tmpcdrptr->RINParameter);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 253:       /* OriginatingPointCode */
                                        switch (parmlen) {
                                                case 2:
                                                        swab(buf+x,buf+x,2);
                                                        memcpy(&tmpcdrptr->OriginatingPointCode,(buf+x),2);
                                                        tmpcdrptr->USED_FIELDS |= B_OriginatingPointCode;
                                                break;
                                                case 3:
                                                        swab(buf+x,buf+x,2);
                                                        swab(buf+(x+1),buf+(x+1),2);
                                                        swab(buf+x,buf+x,2);
                                                        memcpy(&tmpcdrptr->OriginatingPointCode,(buf+x),3);
                                                        tmpcdrptr->USED_FIELDS |= B_OriginatingPointCode;
                                                break;
                                                default :
                                                        BaseString::snprintf(msg,sizeof(msg),"ERROR: OriginatingPointCode parameter has wrong length %d\n",parmlen);
	  						if ((checkchangelog(log,lognamn))==0)
                                                          n2log(log,msg);
                                                break;
                                        }
                                        x+=parmlen;
					#ifdef SETDBG
						puts("OPC");
					#endif
					check = MyOperation->setValue("OPC",(char*)&tmpcdrptr->OriginatingPointCode);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 254:       /* DestinationPointCode */
                                        switch (parmlen) {
                                                case 2:
                                                        swab(buf+x,buf+x,2);
                                                        memcpy(&tmpcdrptr->DestinationPointCode,(buf+x),2);
							/*
                                                        tmpcdrptr->DestinationPointCode = buf[x] << 8;
							*/
                                                        tmpcdrptr->USED_FIELDS |= B_DestinationPointCode;
                                                break;
                                                case 3:
                                                        swab(buf+x,buf+x,2);
                                                        swab(buf+(x+1),buf+(x+1),2);
                                                        swab(buf+x,buf+x,2);
                                                        memcpy(&tmpcdrptr->DestinationPointCode,(buf+x),3);
                                                        tmpcdrptr->USED_FIELDS |= B_DestinationPointCode;
                                                break;
                                                default :
                                                        BaseString::snprintf(msg,sizeof(msg),"ERROR: DestinationPointCode parameter has wrong length %d\n",parmlen);
	  						if ((checkchangelog(log,lognamn))==0)
                                                          n2log(log,msg);
                                                break;
                                        }
                                        x+=parmlen;
					#ifdef SETDBG
						puts("DPC");
					#endif
					check = MyOperation->setValue("DPC",(char*)&tmpcdrptr->DestinationPointCode);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                case 255:       /* CircuitIdentificationCode */
                                        swab(buf+x,buf+x,2);
                                        memcpy(&tmpcdrptr->CircuitIdentificationCode,(buf+x),2);
                                        tmpcdrptr->USED_FIELDS |= B_CircuitIdentificationCode;
                                        x+=parmlen;
					#ifdef SETDBG
						puts("CIC");
					#endif
					check = MyOperation->setValue("CIC",(char*)&tmpcdrptr->CircuitIdentificationCode);
					if (check == -1)
						error_handler(MyTransaction->getNdbErrorString());
                                break;
                                default:
                                        printf("ERROR: Undefined parmtype %d , previous %d, length %d\n",parmtype,parmtype_prev,parmlen);
                                        BaseString::snprintf(msg,sizeof(msg),"ERROR: Undefined parmtype %d , previous %d, length %d\n",parmtype,parmtype_prev,parmlen);
	  				if ((checkchangelog(log,lognamn))==0)
                                          n2log(log,msg);
                                        if (parmlen == 0) {
                                                x++;
                                        }
                                        x+=parmlen;
                                break;
                        }
                        parmtype_prev=parmtype;
                        if ((cdrrestlen-x) == 1) {
                                done=TRUE;
                        }
                }
		time(&ourtime);
		if (ourtime != tmptime)
		{
			transfer = tmptransfer;
			tmptransfer = 0;
			if (++act_index == 30)
			{
				act_index = 0;
				printf("Transfer=%d\n",transfer);
				printf("Total operations=%d\n",reqcnt);
				printf("CAS1=%d\n",c1/30);
				printf("CAS2=%d\n",c2/30);
				printf("CAS3=%d\n",c3/30);
				c1=0;
				c2=0;
				c3=0;
			}
			tmptime = ourtime;
		}
                switch (cdrsubtype) {
                        case 0:
                                tmpcdrptr->ClientId = servernum;
				#ifdef SETDBG
					puts("ClientId");
				#endif
				check = MyOperation->setValue("ClientId",(char*)&tmpcdrptr->ClientId);
				if (check == -1)
					error_handler(MyTransaction->getNdbErrorString());
                                tmpcdrptr->OurSTART_TIME = ourtime;
				#ifdef SETDBG
					puts("OurSTART_TIME");
				#endif
				check = MyOperation->setValue("OurSTART_TIME",(char*)&tmpcdrptr->OurSTART_TIME);
				if (check == -1)
					error_handler(MyTransaction->getNdbErrorString());
                                tmpcdrptr->USED_FIELDS |= B_START_TIME;
				#ifdef SETDBG
					puts("USED_FIELDS");
				#endif
				check = MyOperation->setValue("USED_FIELDS",(char*)&tmpcdrptr->USED_FIELDS);
				if (check == -1)
					error_handler(MyTransaction->getNdbErrorString());
                        break;

                        case 1:
                                tmpcdrptr->OurTimeForStartOfCharge = ourtime;
				#ifdef SETDBG
					puts("OurStartOfCharge");
				#endif
				check = MyOperation->setValue("OurStartOfCharge",(char*)&tmpcdrptr->OurTimeForStartOfCharge);
				if (check == -1)
					error_handler(MyTransaction->getNdbErrorString());
                                tmpcdrptr->USED_FIELDS |= B_TimeForStartOfCharge;
				#ifdef SETDBG
					puts("USED_FIELDS");
				#endif
				check = MyOperation->setValue("USED_FIELDS",(char*)&tmpcdrptr->USED_FIELDS);
				if (check == -1)
					error_handler(MyTransaction->getNdbErrorString());
                        break;

                        case 2:
                                tmpcdrptr->OurTimeForStopOfCharge = ourtime;
				#ifdef SETDBG
					puts("OurStopOfCharge");
				#endif
				check = MyOperation->setValue("OurStopOfCharge",(char*)&tmpcdrptr->OurTimeForStopOfCharge);
				if (check == -1)
					error_handler(MyTransaction->getNdbErrorString());
                                tmpcdrptr->USED_FIELDS |= B_TimeForStopOfCharge;
				#ifdef SETDBG
					puts("USED_FIELDS");
				#endif
				check = MyOperation->setValue("USED_FIELDS",(char*)&tmpcdrptr->USED_FIELDS);
				if (check == -1)
					error_handler(MyTransaction->getNdbErrorString());
                        break;

                        case 3:
                                tmpcdrptr->CallAttemptState  = 4;
                        break;
			default:
        			snprintf(msg,sizeof(msg),"cdrtype %d unknown",cdrsubtype);
				if ((checkchangelog(log,lognamn))==0)
          				n2log(log,msg);
                                goto errout;
                        break;
                }
                                ops++;
                                if (ops == ops_before_exe) {
                                  ops = 0;
                                  #ifdef SETDBG
                                        puts("Going to execute");
                                  #endif
                                  check = MyTransaction->execute(Commit, CommitAsMuchAsPossible);
                		  if ((check == -1) && (MyTransaction->getNdbError() != 0))
                                        error_handler(MyTransaction->getNdbErrorString());
                                  MyNdb.closeTransaction(MyTransaction);
                                  #ifdef SETDBG
                                        puts("Transaction closed");
                                  #endif

                                  #ifdef SETDBG
                                        puts("New transaction initiated");
                                  #endif
                                }//if
                /* Increment the request count. */
                reqcnt++;

                /* Send a response back to the client. */

                /* if (send(s, buf, 10, 0) != 10) goto errout;  */
        }

        /* The loop has terminated, because there are no                */
        /* more requests to be serviced. As mentioned above,            */
        /* this close will block until all of the sent replies          */
        /* have been received by the remote host. The reason            */
        /* for lingering on the close is so that the server will        */
        /* have a better idea of when the remote has picked up          */
        /* all of the data. This will allow the start and finish        */
        /* times printed in the log file to reflect more accurately     */
        /* the length of time this connection was                       */
        /* The port number must be converted first to host byte         */
        /* order before printing. On most hosts, this is not            */
        /* necessary, but the ntohs() call is included here so          */
        /* that this program could easily be ported to a host           */
        /* that does require it.                                        */

        BaseString::snprintf(msg,sizeof(msg),"Completed %s port %u, %d requests",hostname,ntohs(peeraddr_in.sin_port), reqcnt);
	if ((checkchangelog(fi,temp))==0)
          c2log(fi,msg);
	error_from_client = 1;
        BaseString::snprintf(msg,sizeof(msg),"Communicate with threads");
	if ((checkchangelog(log,lognamn))==0)
          n2log(log,msg);
        BaseString::snprintf(msg,sizeof(msg),"Waiting for threads to return from work");
	if ((checkchangelog(log,lognamn))==0)
          n2log(log,msg);
        BaseString::snprintf(msg,sizeof(msg),"Closing down");
	if ((checkchangelog(log,lognamn))==0)
          n2log(log,msg);
        close(s);
        fclose(log);
        return EXIT_SUCCESS;

errout:
        BaseString::snprintf(msg,sizeof(msg),"Connection with %s aborted on error\n", hostname);
	if ((checkchangelog(log,lognamn))==0)
          n2log(log,msg);
	if ((checkchangelog(fi,temp))==0)
          c2log(fi,msg);
	error_from_client = 1;
        BaseString::snprintf(msg,sizeof(msg),"Communicate with threads");
	if ((checkchangelog(log,lognamn))==0)
          n2log(log,msg);
        BaseString::snprintf(msg,sizeof(msg),"Waiting for threads to return from work");
	if ((checkchangelog(log,lognamn))==0)
          n2log(log,msg);
        BaseString::snprintf(msg,sizeof(msg),"Closing down");
	if ((checkchangelog(log,lognamn))==0)
          n2log(log,msg);
        close(s);
        fclose(log);
	return EXIT_FAILURE;
}

void
create_table(Ndb* pMyNdb)
{

 /****************************************************************
   *    Create table and attributes.
   *
   *    create table basictab1(
   *        col1 int,
   *        col2 int not null,
   *        col3 int not null,
   *        col4 int not null
   *     )
   * 
   ***************************************************************/

  int                   check;
  int                   i;
  NdbSchemaCon          *MySchemaTransaction;
  NdbSchemaOp           *MySchemaOp;
  int			tAttributeSize;

  tAttributeSize = 1;

  cout << "Creating " << tableName << "..." << endl;

   MySchemaTransaction = pMyNdb->startSchemaTransaction();
   if( MySchemaTransaction == NULL )
      error_handler(MySchemaTransaction->getNdbErrorString());

   MySchemaOp = MySchemaTransaction->getNdbSchemaOp();
   if( MySchemaOp == NULL )
      error_handler(MySchemaTransaction->getNdbErrorString());

   // Createtable
   check = MySchemaOp->createTable( tableName,
                                     8,         // Table Size
                                     TupleKey,  // Key Type
                                     40         // Nr of Pages
                                   );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());

   // CallIdentificationNumber Create first column, primary key 
   check = MySchemaOp->createAttribute( "CIN", 
					TupleKey, 
					32,
                                        tAttributeSize,
					UnSigned, MMBased,
                                        NotNullAttribute 
					);
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
  

   // USED_FIELDS Create attributes
   check = MySchemaOp->createAttribute( "USED_FIELDS", NoKey, 32,
                                         tAttributeSize, UnSigned, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // ClientId Create attributes
   check = MySchemaOp->createAttribute( "ClientId", NoKey, 32,
                                         tAttributeSize, UnSigned, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // START_TIME Create attributes
   check = MySchemaOp->createAttribute( "START_TIME", NoKey, 32,
                                         tAttributeSize, UnSigned, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // OurSTART_TIME Create attributes
   check = MySchemaOp->createAttribute( "OurSTART_TIME", NoKey, 32,
                                         tAttributeSize, UnSigned, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // TimeForStartOfCharge Create attributes
   check = MySchemaOp->createAttribute( "StartOfCharge", NoKey, 32,
                                         tAttributeSize, UnSigned, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // TimeForStopOfCharge Create attributes
   check = MySchemaOp->createAttribute( "StopOfCharge", NoKey, 32,
                                         tAttributeSize, UnSigned, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // OurTimeForStartOfCharge Create attributes
   check = MySchemaOp->createAttribute( "OurStartOfCharge", NoKey, 32,
                                         tAttributeSize, UnSigned, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // OurTimeForStopOfCharge Create attributes
   check = MySchemaOp->createAttribute( "OurStopOfCharge", NoKey, 32,
                                         tAttributeSize, UnSigned, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // DestinationPointCode Create attributes
   check = MySchemaOp->createAttribute( "DPC", NoKey, 16,
                                         tAttributeSize, UnSigned, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // OriginatingPointCode Create attributes
   check = MySchemaOp->createAttribute( "OPC", NoKey, 16,
                                         tAttributeSize, UnSigned, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // CircuitIdentificationCode Create attributes
   check = MySchemaOp->createAttribute( "CIC", NoKey, 16,
                                         tAttributeSize, UnSigned, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // ReroutingIndicator Create attributes
   check = MySchemaOp->createAttribute( "RI", NoKey, 16,
                                         tAttributeSize, UnSigned, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // RINParameter Create attributes
   check = MySchemaOp->createAttribute( "RINParameter", NoKey, 16,
                                         tAttributeSize, UnSigned, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // NetworkIndicator Create attributes
   check = MySchemaOp->createAttribute( "NIndicator", NoKey, 8,
                                         tAttributeSize, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // CallAttemptState Create attributes
   check = MySchemaOp->createAttribute( "CAS", NoKey, 8,
                                         tAttributeSize, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // ACategory Create attributes
   check = MySchemaOp->createAttribute( "ACategory", NoKey, 8,
                                         tAttributeSize, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // EndOfSelectionInformation Create attributes
   check = MySchemaOp->createAttribute( "EndOfSelInf", NoKey, 8,
                                         tAttributeSize, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // UserToUserInformation Create attributes
   check = MySchemaOp->createAttribute( "UserToUserInf", NoKey, 8,
                                         tAttributeSize, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // UserToUserIndicator Create attributes
   check = MySchemaOp->createAttribute( "UserToUserInd", NoKey, 8,
                                         tAttributeSize, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // CauseCode Create attributes
   check = MySchemaOp->createAttribute( "CauseCode", NoKey, 8,
                                         tAttributeSize, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // ASubscriberNumber attributes
   check = MySchemaOp->createAttribute( "ANumber", NoKey, 8,
                                         ASubscriberNumber_SIZE, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // ASubscriberNumberLenght attributes
   check = MySchemaOp->createAttribute( "ANumberLength", NoKey, 8,
                                         tAttributeSize, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // TonASubscriberNumber attributes
   check = MySchemaOp->createAttribute( "TonANumber", NoKey, 8,
                                         tAttributeSize, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // BSubscriberNumber attributes
   check = MySchemaOp->createAttribute( "BNumber", NoKey, 8,
                                         BSubscriberNumber_SIZE, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // BSubscriberNumberLength attributes
   check = MySchemaOp->createAttribute( "BNumberLength", NoKey, 8,
                                         tAttributeSize, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // TonBSubscriberNumber attributes
   check = MySchemaOp->createAttribute( "TonBNumber", NoKey, 8,
                                         tAttributeSize, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // RedirectingNumber attributes
   check = MySchemaOp->createAttribute( "RNumber", NoKey, 8,
                                         ASubscriberNumber_SIZE, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // TonRedirectingNumber attributes
   check = MySchemaOp->createAttribute( "TonRNumber", NoKey, 8,
                                         tAttributeSize, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // OriginalCalledNumber attributes
   check = MySchemaOp->createAttribute( "ONumber", NoKey, 8,
                                         ASubscriberNumber_SIZE, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // TonOriginalCalledNumber attributes
   check = MySchemaOp->createAttribute( "TonONumber", NoKey, 8,
                                         tAttributeSize, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // LocationCode attributes
   check = MySchemaOp->createAttribute( "LocationCode", NoKey, 8,
                                         ASubscriberNumber_SIZE, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   // TonLocationCode attributes
   check = MySchemaOp->createAttribute( "TonLocationCode", NoKey, 8,
                                         tAttributeSize, Signed, MMBased,
                                         NullAttribute );
   if( check == -1 )
      error_handler(MySchemaTransaction->getNdbErrorString());
 
   if( MySchemaTransaction->execute() == -1 ) {
       cout << tableName << " already exist" << endl;
       cout << "Message: " << MySchemaTransaction->getNdbErrorString() << endl;
   }
   else
   {
     cout << tableName << " created" << endl;
   }
   pMyNdb->closeSchemaTransaction(MySchemaTransaction);
  
   return;
}

void
error_handler(const char* errorText)
{
  // Test failed
  cout << endl << "ErrorMessage: " << errorText << endl;
  bTestPassed = -1;
}
