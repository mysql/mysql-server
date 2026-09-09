/*
 * Copyright (c) 2009-2012 Niels Provos and Nick Mathewson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "../util-internal.h"
#include "event2/event-config.h"

#ifdef _WIN32
#include <winsock2.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef EVENT__HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <sys/time.h>
#include <unistd.h>
#endif
#include <errno.h>

#include "event2/event.h"
#include "event2/util.h"

#include "regress.h"

static int was_et = 0;

static int base_supports_et(struct event_base *base)
{
	return
		(!strcmp(event_base_get_method(base), "epoll") ||
		!strcmp(event_base_get_method(base), "epoll (with changelist)") ||
		!strcmp(event_base_get_method(base), "kqueue"));
}

static void
read_cb(evutil_socket_t fd, short event, void *arg)
{
	char buf;
	int len;

	len = recv(fd, &buf, sizeof(buf), 0);

	called++;
	if (event & EV_ET)
		was_et = 1;

	if (!len)
		event_del(arg);
}

static void
test_edgetriggered(void *data_)
{
	struct basic_test_data *data = data_;
	struct event_base *base = data->base;
	evutil_socket_t *pair = data->pair;
	struct event *ev = NULL;
	const char *test = "test string";
	int supports_et;

	/* On Linux 3.2.1 (at least, as patched by Fedora and tested by Nick),
	 * doing a "recv" on an AF_UNIX socket resets the readability of the
	 * socket, even though there is no state change, so we don't actually
	 * get edge-triggered behavior.  Yuck!  Linux 3.1.9 didn't have this
	 * problem.
	 */

	called = was_et = 0;

	tt_int_op(send(pair[0], test, (int)strlen(test)+1, 0), >, 0);
	tt_int_op(shutdown(pair[0], EVUTIL_SHUT_WR), ==, 0);

	supports_et = base_supports_et(base);
	TT_BLATHER(("Checking for edge-triggered events with %s, which should %s"
				"support edge-triggering", event_base_get_method(base),
				supports_et?"":"not "));

	/* Initialize one event */
	ev = event_new(base, pair[1], EV_READ|EV_ET|EV_PERSIST, read_cb, &ev);
	tt_assert(ev != NULL);
	tt_int_op(event_add(ev, NULL), ==, 0);

	/* We're going to call the dispatch function twice.  The first invocation
	 * will read a single byte from pair[1] in either case.  If we're edge
	 * triggered, we'll only see the event once (since we only see transitions
	 * from no data to data), so the second invocation of event_base_loop will
	 * do nothing.  If we're level triggered, the second invocation of
	 * event_base_loop will also activate the event (because there's still
	 * data to read). */
	tt_int_op(event_base_loop(base,EVLOOP_NONBLOCK|EVLOOP_ONCE), ==, 0);
	tt_int_op(event_base_loop(base,EVLOOP_NONBLOCK|EVLOOP_ONCE), ==, 0);

	if (supports_et) {
		tt_int_op(called, ==, 1);
		tt_assert(was_et);
	} else {
		tt_int_op(called, ==, 2);
		tt_assert(!was_et);
	}

end:
	if (ev) {
		event_del(ev);
		event_free(ev);
	}
}

static void
test_edgetriggered_mix_error(void *data_)
{
	struct basic_test_data *data = data_;
	struct event_base *base = NULL;
	struct event *ev_et=NULL, *ev_lt=NULL;

#ifdef EVENT__DISABLE_DEBUG_MODE
	if (1)
		tt_skip();
#endif

	if (!libevent_tests_running_in_debug_mode)
		event_enable_debug_mode();

	base = event_base_new();

	/* try mixing edge-triggered and level-triggered to make sure it fails*/
	ev_et = event_new(base, data->pair[0], EV_READ|EV_ET, read_cb, ev_et);
	tt_assert(ev_et);
	ev_lt = event_new(base, data->pair[0], EV_READ, read_cb, ev_lt);
	tt_assert(ev_lt);

	/* Add edge-triggered, then level-triggered.  Get an error. */
	tt_int_op(0, ==, event_add(ev_et, NULL));
	tt_int_op(-1, ==, event_add(ev_lt, NULL));
	tt_int_op(EV_READ, ==, event_pending(ev_et, EV_READ, NULL));
	tt_int_op(0, ==, event_pending(ev_lt, EV_READ, NULL));

	tt_int_op(0, ==, event_del(ev_et));
	/* Add level-triggered, then edge-triggered.  Get an error. */
	tt_int_op(0, ==, event_add(ev_lt, NULL));
	tt_int_op(-1, ==, event_add(ev_et, NULL));
	tt_int_op(EV_READ, ==, event_pending(ev_lt, EV_READ, NULL));
	tt_int_op(0, ==, event_pending(ev_et, EV_READ, NULL));

end:
	if (ev_et)
		event_free(ev_et);
	if (ev_lt)
		event_free(ev_lt);
	if (base)
		event_base_free(base);
}

static int read_notification_count;
static int last_read_notification_was_et;
static void
read_notification_cb(evutil_socket_t fd, short event, void *arg)
{
	read_notification_count++;
	last_read_notification_was_et = (event & EV_ET);
}

static int write_notification_count;
static int last_write_notification_was_et;
static void
write_notification_cb(evutil_socket_t fd, short event, void *arg)
{
	write_notification_count++;
	last_write_notification_was_et = (event & EV_ET);
}

/* After two or more events have been registered for the same
 * file descriptor using EV_ET, if one of the events is
 * deleted, then the epoll_ctl() call issued by libevent drops
 * the EPOLLET flag resulting in level triggered
 * notifications.
 */
static void
test_edge_triggered_multiple_events(void *data_)
{
	struct basic_test_data *data = data_;
	struct event *read_ev = NULL;
	struct event *write_ev = NULL;
	const char c = 'A';
	struct event_base *base = data->base;
	evutil_socket_t *pair = data->pair;

	if (!base_supports_et(base)) {
		tt_skip();
		return;
	}

	read_notification_count = 0;
	last_read_notification_was_et = 0;
	write_notification_count = 0;
	last_write_notification_was_et = 0;

	/* Make pair[1] readable */
	tt_int_op(send(pair[0], &c, 1, 0), >, 0);

	read_ev = event_new(base, pair[1], EV_READ|EV_ET|EV_PERSIST,
		read_notification_cb, NULL);
	write_ev = event_new(base, pair[1], EV_WRITE|EV_ET|EV_PERSIST,
		write_notification_cb, NULL);

	event_add(read_ev, NULL);
	event_add(write_ev, NULL);
	event_base_loop(base, EVLOOP_NONBLOCK|EVLOOP_ONCE);
	event_base_loop(base, EVLOOP_NONBLOCK|EVLOOP_ONCE);

	tt_assert(last_read_notification_was_et);
	tt_int_op(read_notification_count, ==, 1);
	tt_assert(last_write_notification_was_et);
	tt_int_op(write_notification_count, ==, 1);

	event_del(read_ev);

	/* trigger acitivity second time for the backend that can have multiple
	 * events for one fd (like kqueue) */
	close(pair[0]);
	pair[0] = -1;

	/* Verify that we are still edge-triggered for write notifications */
	event_base_loop(base, EVLOOP_NONBLOCK|EVLOOP_ONCE);
	event_base_loop(base, EVLOOP_NONBLOCK|EVLOOP_ONCE);
	tt_assert(last_write_notification_was_et);
	tt_int_op(write_notification_count, ==, 2);

end:
	if (read_ev)
		event_free(read_ev);
	if (write_ev)
		event_free(write_ev);
}

struct testcase_t edgetriggered_testcases[] = {
	{ "et", test_edgetriggered,
	  TT_FORK|TT_NEED_BASE|TT_NEED_SOCKETPAIR, &basic_setup, NULL },
	{ "et_mix_error", test_edgetriggered_mix_error,
	  TT_FORK|TT_NEED_SOCKETPAIR|TT_NO_LOGS, &basic_setup, NULL },
	{ "et_multiple_events", test_edge_triggered_multiple_events,
	  TT_FORK|TT_NEED_BASE|TT_NEED_SOCKETPAIR, &basic_setup, NULL },
	END_OF_TESTCASES
};
