#ifndef _LOGD_DEVICES_H_
#define _LOGD_DEVICES_H_

#include "logd.h"
#include "logd-db.h"

#ifdef __cplusplus
extern "C" {
#endif

int devices_init(void);
int devices_finalize(void);
int store_devices_workingtime(enum logd_object device, int state);
int get_current_battery_level(void);
int store_new_power_mode(time_t time_stamp, enum logd_power_mode old_mode,
	enum logd_power_mode new_mode, time_t duration, int battery_level_change);
float* load_discharging_speed(int long_period);

#ifdef __cplusplus
}
#endif

#endif /* _LOGD_DEVICES_H_ */

