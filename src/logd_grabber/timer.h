#ifndef __TIMER_H__
#define __TIMER_H__

struct timer {
	int fd;
	int timeout;
	void (*cb)(void*);
	void *user_data;
};

struct timer* create_timer(int timeout, void(*cb)(void*), void *user_data);
void process_timer(struct timer *tm);
int update_timer_exp(struct timer *tm, int timeout);
void delete_timer(struct timer *tm);

#endif /* __TIMER_H__ */

