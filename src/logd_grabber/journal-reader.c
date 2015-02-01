#include <stdlib.h>
#include <string.h>
#include <systemd/sd-journal.h>

#include "core/log.h"
#include "event.h"
#include "journal-reader.h"
#include "logd-grabber.h"
#include "macro.h"

#define GET_ACTION_FROM_EVENT_TYPE(eventType) ((eventType) >> 16)
#define GET_OBJECT_FROM_EVENT_TYPE(eventType) ((eventType) & 0xffff)

static sd_journal *journal;

int jr_init(void)
{
	int ret = 0;

	ret = sd_journal_open(&journal, SD_JOURNAL_LOCAL_ONLY);
	CHECK_RET(ret, "sd_journal_open");

	ret = sd_journal_add_match(journal, "CODE_FUNC=logd_event", 0);
	CHECK_RET(ret, "sd_journal_add_match");

	ret = sd_journal_seek_tail(journal);
	CHECK_RET(ret, "sd_journal_seek_tail");

	ret = sd_journal_previous(journal);
	CHECK_RET(ret, "sd_journal_previous");

	return 0;
}

int jr_exit(void)
{
	if (journal)
		sd_journal_close(journal);

	return 0;
}

int jr_get_socket(void)
{
	return sd_journal_get_fd(journal);
}

int get_next_event(struct logd_grabber_event *event)
{
	const char type_field[] = "LOGD_EVENT_TYPE";
	const char message_field[] = "MESSAGE";
	const char application_field[] = "SYSLOG_IDENTIFIER";
	char *event_type;
	char *event_message;
	char *event_application;
	int ret;

	while (1) {
		uint64_t event_date;
		size_t len;

		if (!sd_journal_next(journal)) {
			ret = sd_journal_process(journal);
			if (ret < 0) {
				_E("sd_journal_process failed");
				return ret;
			}

			return 1;
		}
		ret = sd_journal_get_realtime_usec(journal, &event_date);
		CHECK_RET(ret, "sd_journal_get_realtime_usec");
		event->date = event_date / USEC_PER_SEC;

		ret = sd_journal_get_data(journal, type_field,
			(const void**)&event_type, &len);
		CHECK_RET(ret, "sd_journal_get_data");

		if (event_type) {
			event_type = strdup(event_type + sizeof(type_field));
			ret = sd_journal_get_data(journal, message_field,
				(const void**)&event_message, &len);
			if (event_message) {
				event_message = strdup(event_message +
					sizeof(message_field));
			}

			ret = sd_journal_get_data(journal, application_field,
				(const void**)&event_application, &len);
			if (event_application) {
				event_application = strdup(event_application +
					sizeof(application_field));
			}
			break;
		}
	}

	event->application = event_application;
	event->message = event_message;
	event->type = atoi(event_type);
	free(event_type);
	event->object = GET_OBJECT_FROM_EVENT_TYPE(event->type);
	event->action = GET_ACTION_FROM_EVENT_TYPE(event->type);

	return 0;
}
