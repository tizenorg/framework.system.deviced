#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include "logd-grabber.h"
#include "event.h"

int display_init();
int brightness_change_event_handler(struct logd_grabber_event *event);
int display_on_off_event_handler(struct logd_grabber_event *event);
float get_display_curr_power_cons();
float get_display_total_power_cons();

#endif /* __DISPLAY_H__ */
