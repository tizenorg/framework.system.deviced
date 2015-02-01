#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "core/log.h"
#include "timer.h"

int update_timer_exp(struct timer *tm, int timeout)
{
	struct timespec now;
	struct itimerspec new_value;

	clock_gettime(CLOCK_REALTIME, &now);
	new_value.it_value.tv_sec = now.tv_sec + timeout;
	new_value.it_value.tv_nsec = now.tv_nsec;

	new_value.it_interval.tv_sec = timeout;
	new_value.it_interval.tv_nsec = 0;

	if (timerfd_settime(tm->fd, TFD_TIMER_ABSTIME, &new_value, NULL) == -1) {
		_E("timerfd_settime: %s", strerror(errno));
		return -errno;
	}

	return 0;
}

struct timer* create_timer(int timeout, void(*cb)(void*), void *user_data)
{
	int ret;
	struct timer *tm;

	tm = malloc(sizeof(struct timer));
	if (!tm) {
		_E("malloc failed");
		return NULL;
	}

	tm->cb = cb;
	tm->user_data = user_data;
	tm->fd = timerfd_create(CLOCK_REALTIME, 0);
	if (tm->fd == -1) {
		_E("timerfd_create failed: %s", strerror(errno));
		free(tm);
		return NULL;
	}

	tm->timeout = timeout;

	ret = update_timer_exp(tm, timeout);
	if (ret < 0) {
		close(tm->fd);
		free(tm);
		_E("update_timer_exp failed");
		return NULL;
	}

	return tm;
}

void process_timer(struct timer *tm)
{
	int s;
	uint64_t exp;

	if (!tm)
		return;
	s = read(tm->fd, &exp, sizeof(uint64_t));
	tm->cb(tm->user_data);
}

void delete_timer(struct timer *tm)
{
	if (!tm)
		return;

	close(tm->fd);
	free(tm);
}
