#ifndef __JOURNAL_READER_H__
#define __JOURNAL_READER_H__

#include "logd-grabber.h"

int jr_init(void);
int jr_exit(void);
int jr_get_socket(void);
int get_next_event(struct logd_grabber_event *event);

#endif /* __JOURNAL_READER_H__ */
