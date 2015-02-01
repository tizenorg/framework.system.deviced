#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-journal.h>
#include "core/log.h"
#include "logd.h"
#include "macro.h"

#define ADD_ACTION_STR(action, str) [action]=str

#define GET_ACTION_STR(table, action) table[action]
/*
 * If you want add action,
 * you must add string of each action
 * using ADD_ACTION_STR macro.
 */
static const char *action_string[LOGD_ACTION_MAX] = {
	ADD_ACTION_STR(LOGD_NONE_ACTION, "none"),
	ADD_ACTION_STR(LOGD_ON, "on"),
	ADD_ACTION_STR(LOGD_START, "start"),
	ADD_ACTION_STR(LOGD_CONTINUE, "continue"),
	ADD_ACTION_STR(LOGD_STOP, "stop"),
	ADD_ACTION_STR(LOGD_OFF, "off"),
	ADD_ACTION_STR(LOGD_CHANGED, "changed"),
};

static const char *get_value(enum logd_object object,
	 enum logd_action action, ...)
{
	char *text = NULL;
	va_list vl;

	va_start(vl, action);

	switch (object | LOGD_SHIFT_ACTION(action)) {
	case LOGD_BATTERY_SOC | LOGD_SHIFT_ACTION(LOGD_CHANGED):
	case LOGD_DISPLAY | LOGD_SHIFT_ACTION(LOGD_CHANGED):
	case LOGD_POWER_MODE | LOGD_SHIFT_ACTION(LOGD_CHANGED):
		if (vasprintf(&text, "%d", vl) < 0)
			goto error;
		break;
	case LOGD_FOREGRD_APP | LOGD_SHIFT_ACTION(LOGD_CHANGED):
		if (vasprintf(&text, "%s", vl) < 0)
			goto error;
		break;
	default:
		/*
		 * we already have defined the string of each action
		 * as it is initialized.
		 * we check avaliable of action and object here.
		 * GET_ACTION_STR will return the string
		 * which is proper of action.
		 */
		if (action < LOGD_ACTION_MAX && object < LOGD_OBJECT_MAX) {
			text = strdup(GET_ACTION_STR(action_string, action));
			break;
		}
		_E("invalid logd_object or logd_action: %d, %d", object, action);
		goto error;
	}
	va_end(vl);
	return text;

error:
	va_end(vl);
	return NULL;
}

API int logd_event(enum logd_object object, enum logd_action action, ...)
{
	va_list vl;
	const char *message = NULL;
	int ret;

	va_start(vl, action);
	message = get_value(object, action);
	if (!message) {
		va_end(vl);
		return LOGD_ERROR_INVALID_PARAM;
	}
	va_end(vl);

	ret = sd_journal_send("LOGD_EVENT_TYPE=%d",
	object | LOGD_SHIFT_ACTION(action), "MESSAGE=%s", message, NULL);
	free((void *) message);

	return ret < 0 ? LOGD_ERROR_SDJOURNAL : LOGD_ERROR_OK;
}
