/* === S Y N F I G ========================================================= */
/*!	\file synfig/rendering/common/optimizer/optimizerlist.cpp
**	\brief OptimizerList
**
**	$Id$
**
**	\legal
**	......... ... 2015-2018 Ivan Mahonin
**
**	This package is free software; you can redistribute it and/or
**	modify it under the terms of the GNU General Public License as
**	published by the Free Software Foundation; either version 2 of
**	the License, or (at your option) any later version.
**
**	This package is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**	General Public License for more details.
**	\endlegal
*/
/* ========================================================================= */

/* === H E A D E R S ======================================================= */

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#endif

#include "optimizerlist.h"

#endif

using namespace synfig;
using namespace rendering;

/* === M A C R O S ========================================================= */

/* === G L O B A L S ======================================================= */

/* === P R O C E D U R E S ================================================= */

/* === M E T H O D S ======================================================= */

// these tasks should be removed from list
static bool
can_be_skipped(const Task::Handle &task)
	{ return !task || task.type_is<TaskNone>() || task.type_is<TaskSurface>(); }

// can we make list with two or more elements?
static bool
can_build_list(const Task::Handle &task)
{
	TaskInterfaceTargetAsSource *interface = task.type_pointer<TaskInterfaceTargetAsSource>();
	return interface
		&& interface->is_allowed_target_as_source()
	    && !task->sub_tasks.empty()
	    && !can_be_skipped(task->sub_tasks.front());
}

static bool
can_be_modified(const Task::Handle &task)
{
	if (can_be_skipped(task)) return false; // don't try to modify empty tasks
	if (task.type_is<TaskList>())
	{ // can we modify any task in list?
		for(Task::List::const_iterator i = task->sub_tasks.begin(); i != task->sub_tasks.end(); ++i)
			if (can_be_skipped(*i) || can_build_list(*i))
				return true;
		return false;
	}
	return can_build_list(task); // or can we build a new list?
}

static Task::Handle
replace_target(
	const TaskList::Handle &list,
	const SurfaceResource::Handle &surface,
	const Task::Handle &task,
	int skip_sub_tasks = 0 )
{
	if (!task) return Task::Handle();

	Task::Handle new_task = task;
	if (task->target_surface == surface) {
		new_task = task->clone();
		new_task->target_rect -= TaskList::calc_target_offset(*list, *new_task);
		new_task->trunc_target_rect(list->target_rect);
		new_task->target_surface = list->target_surface;
	}

	// be carefull - here we need 'less' operator instead of 'non-equal'
	for(Task::List::iterator i = new_task->sub_tasks.begin() + skip_sub_tasks; i < new_task->sub_tasks.end(); ++i) {
		if (new_task != task) {
			*i = replace_target(list, surface, *i, false);
		} else {
			Task::Handle sub_task = replace_target(list, surface, *i, false);
			if (sub_task != *i) {
				new_task = task->clone();
				i = new_task->sub_tasks.begin() + (i - task->sub_tasks.begin());
				*i = sub_task;
			}
		}
	}

	return new_task;
}

static void
add_task(
	const TaskList::Handle &list,
	const Task::Handle &task )
{
	if (task.type_is<TaskList>()) {
		for(Task::List::const_iterator i = task->sub_tasks.begin(); i != task->sub_tasks.end(); ++i)
			if (!can_be_skipped(*i)) add_task(list, *i);
		return;
	}

	bool recursive = can_build_list(task);

	Task::Handle new_task = replace_target(list, task->target_surface, task, recursive ? 1 : 0);
	if (recursive)
	{
		add_task(list, new_task->sub_tasks.front());

		if (new_task == task) new_task = task->clone();
		new_task->sub_tasks.front() = new TaskSurface();
		new_task->sub_tasks.front()->assign_target(*new_task);
		if (TaskInterfaceTargetAsSource *interface = task.type_pointer<TaskInterfaceTargetAsSource>())
			interface->on_target_set_as_source(); else assert(false);
	}
	list->sub_tasks.push_back(new_task);
}

OptimizerList::OptimizerList()
{
	category_id = CATEGORY_ID_SPECIALIZED;
	depends_from = CATEGORY_COORDS;
	deep_first = true;
	for_task = true;
}

void
OptimizerList::run(const RunParams& params) const
{
	const Task::Handle &task = params.ref_task;
	if (can_be_modified(task))
	{
		TaskList::Handle list = new TaskList();
		list->assign_target(*task);
		add_task(list, task);
		apply(params, list);
	}
}

/* === E N T R Y P O I N T ================================================= */
