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

#include <ndb_global.h>

#include <NdbTCP.h>
#include <NdbOut.hpp>
#include <NdbThread.h>
#include <NdbSleep.h>
#include <Properties.hpp>
#include <LocalConfig.hpp>
#include <Config.hpp>
#include <InitConfigFileParser.hpp>
#include <IPCConfig.hpp>

static void
fatal(char const* fmt, ...)
{
    va_list ap;
    char buf[200];
    va_start(ap, fmt);
    BaseString::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ndbout << "FATAL: " << buf << endl;
    sleep(1);
    exit(1);
}

static void
debug(char const* fmt, ...)
{
    va_list ap;
    char buf[200];
    va_start(ap, fmt);
    BaseString::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ndbout << buf << endl;
}

// node
struct Node {
    enum Type { MGM = 1, DB = 2, API = 3 };
    Type type;
    unsigned id;		// node id
    static Node* list;
    static unsigned count;
    static Node* find(unsigned n) {
	for (unsigned i = 0; i < count; i++) {
	    if (list[i].id == n)
		return &list[i];
	}
	return 0;
    }
};

unsigned Node::count = 0;
Node* Node::list = 0;

struct Copy {
    int rfd;			// read from
    int wfd;			// write to
    unsigned char* buf;
    unsigned bufsiz;
    NdbThread* thread;
    void run();
    char info[20];
};

// connection between nodes 0-server side 1-client side
// we are client to 0 and server to 1
struct Conn {
    Node* node[2];		// the nodes
    unsigned port;		// server port
    unsigned proxy;		// proxy port
    static unsigned count;
    static unsigned proxycount;
    static Conn* list;
    NdbThread* thread;		// thread handling this connection
    void run();			// run the connection
    int sockfd[2];		// socket 0-on server side 1-client side
    void conn0();		// connect to side 0
    void conn1();		// connect to side 0
    char info[20];
    Copy copy[2];		// 0-to-1 and 1-to-0
};

unsigned Conn::count = 0;
unsigned Conn::proxycount = 0;
Conn* Conn::list = 0;

// global data
static char* hostname = 0;
static struct sockaddr_in hostaddr;
static char* localcfgfile = 0;
static char* initcfgfile = 0;
static unsigned ownnodeid = 0;

static void
properr(const Properties* props, const char* name, int i = -1)
{
    if (i < 0) {
	fatal("get %s failed: errno = %d", name, props->getPropertiesErrno());
    } else {
	fatal("get %s_%d failed: errno = %d", name, i, props->getPropertiesErrno());
    }
}

// read config and load it into our structs
static void
getcfg()
{
    LocalConfig lcfg;
    if (! lcfg.read(localcfgfile)) {
	fatal("read %s failed", localcfgfile);
    }
    ownnodeid = lcfg._ownNodeId;
    debug("ownnodeid = %d", ownnodeid);
    InitConfigFileParser pars(initcfgfile);
    Config icfg;
    if (! pars.getConfig(icfg)) {
	fatal("parse %s failed", initcfgfile);
    }
    Properties* ccfg = icfg.getConfig(ownnodeid);
    if (ccfg == 0) {
	const char* err = "unknown error";
	fatal("getConfig: %s", err);
    }
    ccfg->put("NodeId", ownnodeid);
    ccfg->put("NodeType", "MGM");
    if (! ccfg->get("NoOfNodes", &Node::count)) {
	properr(ccfg, "NoOfNodes", -1);
    }
    debug("Node::count = %d", Node::count);
    Node::list = new Node[Node::count];
    for (unsigned i = 0; i < Node::count; i++) {
	Node& node = Node::list[i];
	const Properties* nodecfg;
	if (! ccfg->get("Node", 1+i, &nodecfg)) {
	    properr(ccfg, "Node", 1+i);
	}
	const char* type;
	if (! nodecfg->get("Type", &type)) {
	    properr(nodecfg, "Type");
	}
	if (strcmp(type, "MGM") == 0) {
	    node.type = Node::MGM;
	} else if (strcmp(type, "DB") == 0) {
	    node.type = Node::DB;
	} else if (strcmp(type, "API") == 0) {
	    node.type = Node::API;
	} else {
	    fatal("prop %s_%d bad Type = %s", "Node", 1+i, type);
	}
	if (! nodecfg->get("NodeId", &node.id)) {
	    properr(nodecfg, "NodeId");
	}
	debug("node id=%d type=%d", node.id, node.type);
    }
    IPCConfig ipccfg(ccfg);
    if (ipccfg.init() != 0) {
	fatal("ipccfg init failed");
    }
    if (! ccfg->get("NoOfConnections", &Conn::count)) {
	properr(ccfg, "NoOfConnections");
    }
    debug("Conn::count = %d", Conn::count);
    Conn::list = new Conn[Conn::count];
    for (unsigned i = 0; i < Conn::count; i++) {
	Conn& conn = Conn::list[i];
	const Properties* conncfg;
	if (! ccfg->get("Connection", i, &conncfg)) {
	    properr(ccfg, "Connection", i);
	}
	unsigned n;
	if (! conncfg->get("NodeId1", &n)) {
	    properr(conncfg, "NodeId1");
	}
	if ((conn.node[0] = Node::find(n)) == 0) {
	    fatal("node %d not found", n);
	}
	if (! conncfg->get("NodeId2", &n)) {
	    properr(conncfg, "NodeId2");
	}
	if ((conn.node[1] = Node::find(n)) == 0) {
	    fatal("node %d not found", n);
	}
	if (! conncfg->get("PortNumber", &conn.port)) {
	    properr(conncfg, "PortNumber");
	}
	conn.proxy = 0;
	const char* proxy;
	if (conncfg->get("Proxy", &proxy)) {
	    conn.proxy = atoi(proxy);
	    if (conn.proxy > 0) {
		Conn::proxycount++;
	    }
	}
	sprintf(conn.info, "conn %d-%d", conn.node[0]->id, conn.node[1]->id);
    }
}

void
Conn::conn0()
{
    int fd;
    while (1) {
	if ((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
	    fatal("%s: create client socket failed: %s", info, strerror(errno));
	}
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr = hostaddr.sin_addr;
#if 0	// coredump
	if (Ndb_getInAddr(&servaddr.sin_addr, hostname) != 0) {
	    fatal("%s: hostname %s lookup failed", info, hostname);
	}
#endif
	if (connect(fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == 0)
	    break;
	if (errno != ECONNREFUSED) {
	    fatal("%s: connect failed: %s", info, strerror(errno));
	}
	close(fd);
	NdbSleep_MilliSleep(100);
    }
    sockfd[0] = fd;
    debug("%s: side 0 connected", info);
}

void
Conn::conn1()
{
    int servfd;
    if ((servfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
	fatal("%s: create server socket failed: %s", info, strerror(errno));
    }
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(proxy);
    const int on = 1;
    setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
    if (bind(servfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) == -1) {
	fatal("%s: bind %d failed: %s", info, proxy, strerror(errno));
    }
    if (listen(servfd, 1) == -1) {
	fatal("%s: listen %d failed: %s", info, proxy, strerror(errno));
    }
    int fd;
    if ((fd = accept(servfd, 0, 0)) == -1) {
	fatal("%s: accept failed: %s", info, strerror(errno));
    }
    sockfd[1] = fd;
    close(servfd);
    debug("%s: side 1 connected", info);
}

void
Copy::run()
{
    debug("%s: start", info);
    int n, m;
    while (1) {
	n = read(rfd, buf, sizeof(buf));
	if (n < 0)
	    fatal("read error: %s", strerror(errno));
	m = write(wfd, buf, n);
	if (m != n)
	    fatal("write error: %s", strerror(errno));
    }
    debug("%s: stop", info);
}

extern "C" void*
copyrun_C(void* copy)
{
    ((Copy*) copy)->run();
    NdbThread_Exit(0);
    return 0;
}

void
Conn::run()
{
    debug("%s: start", info);
    conn1();
    conn0();
    const unsigned siz = 32 * 1024;
    for (int i = 0; i < 2; i++) {
	Copy& copy = this->copy[i];
	copy.rfd = sockfd[i];
	copy.wfd = sockfd[1-i];
	copy.buf = new unsigned char[siz];
	copy.bufsiz = siz;
	sprintf(copy.info, "copy %d-%d", this->node[i]->id, this->node[1-i]->id);
	copy.thread = NdbThread_Create(copyrun_C, (void**)&copy,
	    8192, "copyrun", NDB_THREAD_PRIO_LOW);
	if (copy.thread == 0) {
	    fatal("%s: create thread %d failed errno=%d", i, errno);
	}
    }
    debug("%s: stop", info);
}

extern "C" void*
connrun_C(void* conn)
{
    ((Conn*) conn)->run();
    NdbThread_Exit(0);
    return 0;
}

static void
start()
{
    NdbThread_SetConcurrencyLevel(3 * Conn::proxycount + 2);
    for (unsigned i = 0; i < Conn::count; i++) {
	Conn& conn = Conn::list[i];
	if (! conn.proxy)
	    continue;
	conn.thread = NdbThread_Create(connrun_C, (void**)&conn,
	    8192, "connrun", NDB_THREAD_PRIO_LOW);
	if (conn.thread == 0) {
	    fatal("create thread %d failed errno=%d", i, errno);
	}
    }
    sleep(3600);
}

int
main(int av, char** ac)
{
    ndb_init();
    debug("start");
    hostname = "ndb-srv7";
    if (Ndb_getInAddr(&hostaddr.sin_addr, hostname) != 0) {
	fatal("hostname %s lookup failed", hostname);
    }
    localcfgfile = "Ndb.cfg";
    initcfgfile = "config.txt";
    getcfg();
    start();
    debug("done");
    return 0;
}

// vim: set sw=4 noet:
