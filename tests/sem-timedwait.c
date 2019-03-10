/*
 * SPDX-License-Identifier: MIT
 */

#include <sys/types.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <evl/thread.h>
#include <evl/clock.h>
#include <evl/sem.h>
#include "helpers.h"

struct test_context {
	struct evl_sem sem;
};

static void *sem_contend(void *arg)
{
	struct test_context *p = arg;
	struct timespec now, timeout;
	int ret, tfd;

	__Tcall_assert(tfd, evl_attach_self("sem-wait-contend:%d", getpid()));
	__Tcall_assert(ret, evl_read_clock(EVL_CLOCK_MONOTONIC, &now));
	timespec_add_ns(&timeout, &now, 10000000); /* 10ms */
	__Fcall_assert(ret, evl_timedget(&p->sem, &timeout));
	__Texpr_assert(ret == -ETIMEDOUT);

	return NULL;
}

int main(int argc, char *argv[])
{
	struct test_context c;
	pthread_t contender;
	int tfd, sfd, ret;
	char *name;

	__Tcall_assert(tfd, evl_attach_self("sem-close-unblock:%d", getpid()));

	name = get_unique_name("monitor", 0);
	__Tcall_assert(sfd, evl_new_sem(&c.sem, EVL_CLOCK_MONOTONIC, 0, name));
	ret = new_thread(&contender, SCHED_FIFO, 1, sem_contend, &c);
	if (ret < 0)
		exit(1);

	pthread_join(contender, NULL);

	evl_close_sem(&c.sem);

	return 0;
}
