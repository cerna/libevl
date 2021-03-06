/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <evl/evl.h>
#include <evl/syscall.h>
#include <evl/thread.h>
#include <linux/types.h>
#include <uapi/evl/control.h>
#include <uapi/evl/signal.h>
#include "internal.h"

#define STDLOG_SIZE	32768

static pthread_once_t init_once = PTHREAD_ONCE_INIT;

static int init_status;

static struct sigaction orig_sigevl;

static struct evl_core_info core_info;

int evl_ctlfd = -1;

void *evl_shared_memory = NULL;

static void atfork_unmap_shmem(void)
{
	if (evl_shared_memory) {
		munmap(evl_shared_memory, core_info.shm_size);
		evl_shared_memory = NULL;
	}

	if (evl_ctlfd >= 0) {
		close(evl_ctlfd);
		evl_ctlfd = -1;
	}

	init_once = PTHREAD_ONCE_INIT;
}

static inline int generic_init(void)
{
	int ctlfd, ret;
	void *shmem;

	/*
	 * Failing to open the control device with ENOENT is a clear
	 * sign that we have no EVL core in there. Return with -ENOSYS
	 * to give a clear hint about this.
	 */
	ctlfd = open(EVL_CONTROL_DEV, O_RDWR);
	if (ctlfd < 0) {
		if (errno == ENOENT) {
			fprintf(stderr,	"evl: core not enabled in kernel\n");
			return -ENOSYS;
		}
		return -errno;
	}

	ret = fcntl(ctlfd, F_GETFD, 0);
	if (ret < 0) {
		ret = -errno;
		goto fail;
	}

	ret = fcntl(ctlfd, F_SETFD, ret | O_CLOEXEC);
	if (ret < 0) {
		ret = -errno;
		goto fail;
	}

	ret = ioctl(ctlfd, EVL_CTLIOC_GET_COREINFO, &core_info);
	if (ret) {
		ret = -errno;
		goto fail;
	}

	if (core_info.abi_level != EVL_ABI_LEVEL) {
		fprintf(stderr,
			"evl: ABI mismatch, see -ENOEXEC at https://evlproject.org/"
			"core/user-api/thread/#evl_attach_self\n");
		ret = -ENOEXEC;
		goto fail;
	}

	ret = attach_evl_clocks();
	if (ret)
		goto fail;

	shmem = mmap(NULL, core_info.shm_size, PROT_READ|PROT_WRITE,
		MAP_SHARED, ctlfd, 0);
	if (shmem == MAP_FAILED) {
		ret = -errno;
		goto fail;
	}

	pthread_atfork(NULL, NULL, atfork_unmap_shmem);
	evl_ctlfd = ctlfd;
	evl_shared_memory = shmem;

	return 0;
fail:
	close(ctlfd);

	return ret;
}

int evl_sigevl_handler(int sig, siginfo_t *si, void *ctxt)
{
	if (si->si_code == SI_QUEUE &&
		sigevl_action(si->si_int) == SIGEVL_ACTION_HOME) {
		oob_ioctl(evl_efd, EVL_THRIOC_SWITCH_OOB);
		return 1;
	}

	return 0;
}

#define raw_write_out(__msg)					\
	do {							\
		int __ret;					\
		__ret = write(1, __msg , strlen(__msg));	\
		(void)__ret;					\
	} while (0)

static const char *sigdebug_msg[] = {
	"undefined\n",		/* Should never happen. */
	"switched inband (signal)\n",
	"switched inband (syscall)\n",
	"switched inband (fault)\n",
	"switched inband while holding mutex\n",
	"watchdog triggered\n",
	"mutex lock/unlock imbalance\n",
	"goes to sleep while holding a mutex\n",
};

/* A basic SIGDEBUG handler which only prints out the cause. */

void evl_sigdebug_handler(int sig, siginfo_t *si, void *ctxt)
{
	if (sigdebug_marked(si)) {
		switch (sigdebug_cause(si)) {
		case SIGDEBUG_NONE:
		case SIGDEBUG_MIGRATE_SIGNAL:
		case SIGDEBUG_MIGRATE_SYSCALL:
		case SIGDEBUG_MIGRATE_FAULT:
		case SIGDEBUG_MIGRATE_PRIOINV:
		case SIGDEBUG_WATCHDOG:
		case SIGDEBUG_MUTEX_IMBALANCE:
		case SIGDEBUG_MUTEX_SLEEP:
			raw_write_out(sigdebug_msg[sigdebug_cause(si)]);
			break;
		}
	}
}

static void sigevl_handler(int sig, siginfo_t *si, void *ctxt)
{
	const struct sigaction *const sa = &orig_sigevl;
	sigset_t omask;

	if (evl_sigevl_handler(sig, si, ctxt))
		return;

	if ((!(sa->sa_flags & SA_SIGINFO) && sa->sa_handler == NULL) ||
		((sa->sa_flags & SA_SIGINFO) && sa->sa_sigaction == NULL))
		return;		/* Not sent by the EVL core */

	pthread_sigmask(SIG_SETMASK, &sa->sa_mask, &omask);

	if (!(sa->sa_flags & SA_SIGINFO))
		sa->sa_handler(sig);
	else
		sa->sa_sigaction(sig, si, ctxt);

	pthread_sigmask(SIG_SETMASK, &omask, NULL);
}

static void install_signal_handlers(void)
{
	sigset_t mask, omask;
	struct sigaction sa;

	sigemptyset(&mask);
	sigaddset(&mask, SIGEVL);

	sa.sa_flags = SA_SIGINFO | SA_RESTART;
	sa.sa_sigaction = sigevl_handler;
	sigemptyset(&sa.sa_mask);
	pthread_sigmask(SIG_BLOCK, &mask, &omask);

	sigaction(SIGEVL, &sa, &orig_sigevl);

	if (!(orig_sigevl.sa_flags & SA_NODEFER))
		sigaddset(&orig_sigevl.sa_mask, SIGEVL);

	pthread_sigmask(SIG_SETMASK, &omask, NULL);
}

static inline int do_init(void)
{
	int ret;

	ret = arch_evl_init();
	if (ret)
		return ret;

	ret = mlockall(MCL_CURRENT | MCL_FUTURE);
	if (ret)
		return -errno;

	ret = generic_init();
	if (ret)
		return ret;

	init_proxy_streams();
	install_signal_handlers();

	return 0;
}

static void do_init_once(void)
{
	init_status = do_init();
}

int evl_init(void)
{
	pthread_once(&init_once, do_init_once);

	return init_status;
}

unsigned int evl_detect_fpu(void)
{
	if (evl_init())
		return 0;

	return core_info.fpu_features;
}
