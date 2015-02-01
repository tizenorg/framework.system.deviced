#include <Eina.h>
#include <string.h>
#include "core/log.h"
#include "battery.h"
#include "display.h"
#include "devices.h"
#include "event.h"
#include "events.h"
#include "journal-reader.h"
#include "macro.h"
#include "nlproc-stat.h"

static Eina_Hash *stored_appids;
static Eina_Hash *event_handlers;

static enum logd_db_query load_apps_cb(int id, const char *app, void *user_data)
{
	char *tmp;

	if (!eina_hash_find(stored_appids, app)) {
		tmp = strdup(app);
		if (!tmp) {
			_E("strdup failed: %s", strerror(errno));
			return LOGD_DB_QUERY_CONTINUE;
		}

		if (eina_hash_add(stored_appids, tmp, (void*)id) == EINA_FALSE) {
			_E("eina_hash_set failed: %s", eina_error_msg_get(eina_error_get()));
		}
	}

	return LOGD_DB_QUERY_CONTINUE;
}

int init_event_handlers(void)
{
	int key;

	event_handlers = eina_hash_int32_new(NULL);
	if (!event_handlers) {
		_E("eina_hash_int32_new failed");
		return -ENOMEM;
	}

	/* LOGD_DISPLAY | LOGD_CHANGED */
	key = LOGD_DISPLAY | LOGD_SHIFT_ACTION(LOGD_CHANGED);
	if (eina_hash_add(event_handlers,
		&key, brightness_change_event_handler) == EINA_FALSE) {
		_E("eina_hash_add failed: %s", eina_error_msg_get(eina_error_get()));
		return -ENOMEM;
	}

	/* LOGD_DISPLAY | LOGD_ON */
	key = LOGD_DISPLAY | LOGD_SHIFT_ACTION(LOGD_ON);
	if (eina_hash_add(event_handlers,
		&key, display_on_off_event_handler) == EINA_FALSE) {
		_E("eina_hash_add failed: %s", eina_error_msg_get(eina_error_get()));
		return -ENOMEM;
	}

	/* LOGD_DISPLAY | LOGD_OFF */
	key = LOGD_DISPLAY | LOGD_SHIFT_ACTION(LOGD_OFF);
	if (eina_hash_add(event_handlers,
		&key, display_on_off_event_handler) == EINA_FALSE) {
		_E("eina_hash_add failed: %s", eina_error_msg_get(eina_error_get()));
		return -ENOMEM;
	}

	/* LOGD_BATTERY_SOC | LOGD_CHANGED */
	key = LOGD_BATTERY_SOC | LOGD_SHIFT_ACTION(LOGD_CHANGED);
	if (eina_hash_add(event_handlers,
		&key, battery_level_changed_event_handler) == EINA_FALSE) {
		_E("eina_hash_add failed: %s", eina_error_msg_get(eina_error_get()));
		return -ENOMEM;
	}

	/* LOGD_CHARGER | LOGD_ON */
	key = LOGD_CHARGER | LOGD_SHIFT_ACTION(LOGD_ON);
	if (eina_hash_add(event_handlers,
		&key, battery_charger_event_handler) == EINA_FALSE) {
		_E("eina_hash_add failed: %s", eina_error_msg_get(eina_error_get()));
		return -ENOMEM;
	}

	/* LOGD_CHARGER | LOGD_OFF */
	key = LOGD_CHARGER | LOGD_SHIFT_ACTION(LOGD_OFF);
	if (eina_hash_add(event_handlers,
		&key, battery_charger_event_handler) == EINA_FALSE) {
		_E("eina_hash_add failed: %s", eina_error_msg_get(eina_error_get()));
		return -ENOMEM;
	}

	/* LOGD_POWER_MODE | LOGD_CHANGED */
	key = LOGD_POWER_MODE | LOGD_SHIFT_ACTION(LOGD_CHANGED);
	if (eina_hash_add(event_handlers,
		&key, battery_power_mode_changed_event_handler) == EINA_FALSE) {
		_E("eina_hash_add failed: %s", eina_error_msg_get(eina_error_get()));
		return -ENOMEM;
	}


	return 0;
}

int event_init()
{
	int ret;

	stored_appids = eina_hash_string_superfast_new(NULL);
	if (!stored_appids) {
		_E("eina_hash_string_superfast_new failed");
		return -ENOMEM;
	}

	ret = logd_load_apps(load_apps_cb, NULL);
	if (ret < 0) {
		_E("logd_load_apps failed");
		return ret;
	}

	ret = init_event_handlers();
	if (ret < 0) {
		_E("init_event_handlers failed");
		return ret;
	}

	return 0;
}

int event_exit()
{
	if (event_handlers)
		eina_hash_free(event_handlers);

	if (stored_appids)
		eina_hash_free(stored_appids);

	return 0;
}

void free_event(struct logd_grabber_event *event)
{
	if (!event)
		return;
	if (event->application)
		free((void*)event->application);
	if (event->message)
		free((void*)event->message);
}

int store_event(struct logd_grabber_event *event)
{
	int id;
	int ret;

	if (event->action == LOGD_ON || event->action == LOGD_OFF)
		if (store_devices_workingtime(event->object, event->action) < 0)
			_E("store_devices_workingtime failed");
	id = (int)eina_hash_find(stored_appids, event->application);
	if (!id) {
		if (logd_store_app(event->application) < 0) {
			_E("logd_store_app failed");
		}

		logd_load_apps(load_apps_cb, NULL);

		id = (int)eina_hash_find(stored_appids, event->application);
		if (!id) {
			_E("eina_hash_find failed");
		}
	}

	ret = logd_store_event(event->type, event->date, id, event->message);
	if (ret < 0) {
		_E("logd_store_event failed");
	}

	return ret;
}

void event_socket_cb(void *user_data)
{
	struct logd_grabber_event event;
	int ret;

	ret = get_next_event(&event);
	if (ret < 0) {
		_E("get_next_event failed");
		return;
	} else if (ret == 0) {
		event_handler_func func;

		_D("object %d, action %d, message: %s\n", event.object,
			event.action, event.message);

		func = (event_handler_func) eina_hash_find(event_handlers, &event.type);
		if (!func) {
			free_event(&event);
			return;
		}
		if (func(&event) < 0) {
			_E("Event handle failed: object %d, action %d, message: %s\n",
				event.object, event.action, event.message);
		}
		if (store_event(&event) < 0) {
			_E("store_event failed");
		}
		free_event(&event);
	}
}
