#ifndef __EVENT_H__
#define __EVENT_H__

#include <Ecore.h>
#include <Eina.h>
#include "logd.h"

struct logd_grabber_event {
	const char *application;
	int type;
	enum logd_object object;
	enum logd_action action;
	time_t date;
	const char *message;
};

typedef int (*event_handler_func)(const struct logd_grabber_event *);

int event_exit();
int event_init();
int store_event(struct logd_grabber_event *event);
void event_socket_cb(void *user_data);

#endif /* __EVENT_H__ */
