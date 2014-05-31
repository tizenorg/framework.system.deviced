/*
 * deviced
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef __QUEUE_H__
#define __QUEUE_H__

#include "sysnoti.h"

struct action_entry {
	int owner_pid;
	void *handle;
	char *type;
	char *path;
	int (*predefine_action) ();
	int (*ui_viewable) ();
	int (*is_accessible) (int caller_sockfd);
};

enum run_state {
	STATE_INIT,
	STATE_RUNNING,
	STATE_DONE
};

struct run_queue_entry {
	enum run_state state;
	struct action_entry *action_entry;
	int forked_pid;
	int argc;
	char *argv[SYSMAN_MAXARG];
};

int register_action(char *type,
				 int (*predefine_action) (),
				 int (*ui_viewable) (),
				 int (*is_accessible) (int));
int notify_action(char *type, int argc, ...);

int register_msg(struct sysnoti *msg);
int notify_msg(struct sysnoti *msg, int sockfd);

int run_queue_run(enum run_state state,
		     int (*run_func) (void *, struct run_queue_entry *),
		     void *user_data);

struct run_queue_entry *run_queue_find_bypid(int pid);
int run_queue_add(struct action_entry *act_entry, int argc, char **argv);
int run_queue_del(struct run_queue_entry *entry);
int run_queue_del_bypid(int pid);

#endif /* __QUEUE_H__ */
