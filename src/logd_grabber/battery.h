#ifndef __BATTERY_H__
#define __BATTERY_H__

#include <time.h>

#include "event.h"
#include "logd-grabber.h"

int battery_init();
int battery_level_at(time_t date);
int battery_send_estimate_lifetime(int socket);
int battery_send_check_points(int socket);
int battery_exit();

int battery_level_changed_event_handler(struct logd_grabber_event *event);
int battery_charger_event_handler(struct logd_grabber_event *event);
int battery_power_mode_changed_event_handler(struct logd_grabber_event *event);

#endif /* __BATTERY_H__ */
