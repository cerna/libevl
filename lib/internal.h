/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _LIB_EVL_INTERNAL_H
#define _LIB_EVL_INTERNAL_H

#include <time.h>
#include <stdarg.h>
#include <evl/thread.h>
#include <uapi/evl/types.h>

extern __thread __attribute__ ((tls_model (EVL_TLS_MODEL)))
fundle_t evl_current;

extern __thread __attribute__ ((tls_model (EVL_TLS_MODEL)))
int evl_efd;

static inline fundle_t evl_get_current(void)
{
	return evl_current;
}

static inline struct evl_user_window *
evl_get_current_window(void)
{
	return evl_current ? evl_current_window : NULL;
}

struct evl_element_ids;

int arch_evl_init(void);

int attach_evl_clocks(void);

void init_proxy_streams(void);

int create_evl_element(const char *type,
		       const char *name,
		       void *attrs,
		       struct evl_element_ids *eids);

int open_evl_element_vargs(const char *type,
			const char *fmt, va_list ap);

int open_evl_element(const char *type,
		     const char *path, ...);

int create_evl_file(const char *type);

extern int (*arch_clock_gettime)(clockid_t clk_id,
				 struct timespec *tp);

extern void *evl_shared_memory;

extern int evl_ctlfd;

extern int evl_mono_clockfd;

extern int evl_real_clockfd;

#endif /* _LIB_EVL_INTERNAL_H */
