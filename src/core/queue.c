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


#include <dlfcn.h>
#include "data.h"
#include "core.h"
#include "queue.h"
#include "log.h"
#include "list.h"

#define PREDEFINE_ACT_FUNC_STR		"predefine_action"
#define IS_ACCESSIBLE_FUNC_STR		"is_accessible"
#define UI_VIEWABLE_FUNC_STR			"ui_viewable"

static dd_list *predef_act_list;
static dd_list *run_queue;

static struct action_entry *find_action_entry(char *type)
{
	dd_list *tmp;
	struct action_entry *data;

	DD_LIST_FOREACH(predef_act_list, tmp, data) {
		if (!strcmp(data->type, type))
			return data;
	}

	return NULL;
}

int register_action(char *type,
				 int (*predefine_action) (),
				 int (*ui_viewable) (),
				 int (*is_accessible) (int))
{
	struct action_entry *data;

	data = malloc(sizeof(struct action_entry));

	if (data == NULL) {
		_E("Malloc failed");
		return -1;
	}

	data->type = NULL;
	if (find_action_entry(type) != NULL)
		goto err;

	data->handle = NULL;
	data->predefine_action = predefine_action;
	if (data->predefine_action == NULL)
		goto err;

	data->is_accessible = is_accessible;
	data->ui_viewable = ui_viewable;
	data->owner_pid = getpid();
	data->type = strdup(type);
	data->path = strdup("");

	DD_LIST_PREPEND(predef_act_list, data);

	_D("add predefine action entry suceessfully - %s",
		  data->type);
	return 0;
 err:
	if (data->type != NULL)
		_E("adding predefine action entry failed - %s",
			      data->type);
	free(data);
	return -1;
}

int register_msg(struct sysnoti *msg)
{
	struct action_entry *data;

	data = malloc(sizeof(struct action_entry));

	if (data == NULL) {
		_E("Malloc failed");
		return -1;
	}

	if (find_action_entry(msg->type) != NULL)
		goto err;

	data->handle = dlopen(msg->path, RTLD_LAZY);
	if (!data->handle) {
		_E("cannot find such library");
		goto err;
	}

	data->predefine_action = dlsym(data->handle, PREDEFINE_ACT_FUNC_STR);
	if (data->predefine_action == NULL) {
		_E("cannot find predefine_action symbol : %s",
			      PREDEFINE_ACT_FUNC_STR);
		goto err;
	}

	data->is_accessible = dlsym(data->handle, IS_ACCESSIBLE_FUNC_STR);
	data->ui_viewable = dlsym(data->handle, UI_VIEWABLE_FUNC_STR);
	data->owner_pid = msg->pid;
	data->type = strdup(msg->type);
	data->path = strdup(msg->path);

	DD_LIST_PREPEND(predef_act_list, data);

	_D("add predefine action entry suceessfully - %s",
		  data->type);
	return 0;
 err:
	_E("adding predefine action entry failed - %s", msg->type);
	free(data);
	return -1;
}

int notify_action(char *type, int argc, ...)
{
	dd_list *tmp;
	struct action_entry *data;
	va_list argptr;
	int i;
	int ret;
	char *args = NULL;
	char *argv[SYSMAN_MAXARG];

	if (argc > SYSMAN_MAXARG || type == NULL)
		return -1;

	DD_LIST_FOREACH(predef_act_list, tmp, data) {
		if (strcmp(data->type, type))
			continue;
		va_start(argptr, argc);
		for (i = 0; i < argc; i++) {
			args = va_arg(argptr, char *);
			if (args != NULL)
				argv[i] = strdup(args);
			else
				argv[i] = NULL;
		}
		va_end(argptr);
		ret=run_queue_add(data, argc, argv);
		ret=core_action_run();
		return 0;
	}

	return 0;
}

int notify_msg(struct sysnoti *msg, int sockfd)
{
	dd_list *tmp;
	struct action_entry *data;
	int ret;

	DD_LIST_FOREACH(predef_act_list, tmp, data) {
		if (strcmp(data->type, msg->type))
			continue;
		if (data->is_accessible != NULL
		    && data->is_accessible(sockfd) == 0) {
			_E("%d cannot call that predefine module", msg->pid);
			return -1;
		}
		ret=run_queue_add(data, msg->argc, msg->argv);
		ret=core_action_run();
		return 0;
	}

	_E("cannot found action");
	return -1;
}

int run_queue_add(struct action_entry *act_entry, int argc, char **argv)
{
	struct run_queue_entry *rq_entry;
	int i;

	rq_entry = malloc(sizeof(struct run_queue_entry));

	if (rq_entry == NULL) {
		_E("Malloc failed");
		return -1;
	}

	rq_entry->state = STATE_INIT;
	rq_entry->action_entry = act_entry;
	rq_entry->forked_pid = 0;
	if ( argc < 0 ) {
		rq_entry->argc = 0;
	} else {
		rq_entry->argc = argc;
		for (i = 0; i < argc; i++)
			rq_entry->argv[i] = argv[i];
	}

	DD_LIST_PREPEND(run_queue, rq_entry);

	return 0;
}

int run_queue_run(enum run_state state,
		     int (*run_func) (void *, struct run_queue_entry *),
		     void *user_data)
{
	dd_list *tmp;
	struct run_queue_entry *rq_entry;

	DD_LIST_FOREACH(run_queue, tmp, rq_entry) {
		if (rq_entry->state == state)
			run_func(user_data, rq_entry);
	}

	return 0;
}

struct run_queue_entry *run_queue_find_bypid(int pid)
{
	dd_list *tmp;
	struct run_queue_entry *rq_entry;

	DD_LIST_FOREACH(run_queue, tmp, rq_entry) {
		if (rq_entry->forked_pid == pid)
			return rq_entry;
	}

	return NULL;
}

int run_queue_del(struct run_queue_entry *entry)
{
	dd_list *tmp;
	struct run_queue_entry *rq_entry;
	int i;

	DD_LIST_FOREACH(run_queue, tmp, rq_entry) {
		if (rq_entry == entry) {
			DD_LIST_REMOVE(run_queue, rq_entry);
			for (i = 0; i < rq_entry->argc; i++) {
				if (rq_entry->argv[i])
					free(rq_entry->argv[i]);
			}
			free(rq_entry);
		}
	}

	return 0;
}

int run_queue_del_bypid(int pid)
{
	dd_list *tmp;
	struct run_queue_entry *rq_entry;
	int i;

	DD_LIST_FOREACH(run_queue, tmp, rq_entry) {
		if (rq_entry->forked_pid == pid) {
			DD_LIST_REMOVE(run_queue, rq_entry);
			for (i = 0; i < rq_entry->argc; i++) {
				if (rq_entry->argv[i])
					free(rq_entry->argv[i]);
			}
			free(rq_entry);
		}
	}

	return 0;
}
