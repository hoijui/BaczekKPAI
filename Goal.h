#pragma once

#include <cassert>
#include <vector>
#include <string>
#include <queue>
#include <boost/ptr_container/ptr_unordered_map.hpp>
#include <boost/signal.hpp>
#include <boost/variant.hpp>

#include "float3.h"

enum Type {
	ATTACK_UNIT,
	ATTACK_AREA,
	DEFEND_AREA,
	BUILD_UNIT,
	BUILD_EXPANSION,
	BUILD_WEAPON,
	MOVE,
	RETREAT,
	NO_TYPE,
};


class Goal
{
public:
	Goal(void) {
		id = global_id;
		flags = 0;
		priority = 0;
		type = NO_TYPE;
	}

	Goal(int priority, Type type)
	{
		id = global_id;
		flags = 0;
		this->priority = priority;
		this->type = type;
	}

	~Goal() {};

	typedef boost::variant<int, float3, std::string> param_variant;
	typedef std::vector<param_variant> param_vector;
	
	typedef boost::signal<void (Goal&)> on_complete_sig;
	typedef boost::signal<void (Goal&)> on_abort_sig;
	typedef boost::signal<void (Goal&)> on_start_sig;
	typedef boost::signal<void (Goal&)> on_suspend_sig;
	typedef boost::signal<void (Goal&)> on_continue_sig;
	typedef boost::signals::connection connection;

	static const int FINISHED = 0x0001;
	static const int COMPLETED = 0x0002;
	static const int ABORTED = 0x0004;
	static const int EXECUTING = 0x0008;
	static const int SUSPENDED = 0x0010;

	static int global_id;

	int id;
	int priority;
	int flags;
	Type type;
	param_vector params;
	std::vector<int> nextGoals;

	on_complete_sig onComplete;
	on_abort_sig onAbort;
	on_start_sig onStart;
	on_suspend_sig onSuspend;
	on_continue_sig onContinue;

	bool operator<(const Goal& o) { return id < o.id; }
	bool operator==(const Goal& o) { return id == o.id; }

	connection OnComplete(on_complete_sig::slot_function_type f)
	{
		return onComplete.connect(f);
	}

	connection OnAbort(on_abort_sig::slot_function_type f)
	{
		return onAbort.connect(f);
	}

	connection OnStart(on_start_sig::slot_function_type f)
	{
		return onStart.connect(f);
	}

	connection OnContinue(on_continue_sig::slot_function_type f)
	{
		return onContinue.connect(f);
	}

	connection OnSuspend(on_suspend_sig::slot_function_type f)
	{
		return onSuspend.connect(f);
	}

	bool is_finished() { return (flags & FINISHED); }

	void start() {
		assert(!is_finished());
		flags = EXECUTING;
		onStart(*this);
	}
	void suspend() {
		assert(!is_finished());
		flags = SUSPENDED;
		onSuspend(*this);
	}
	void continue_() {
		assert(!is_finished());
		flags = EXECUTING;
		onContinue(*this);
	}
	void complete() {
		assert(!is_finished());
		flags = FINISHED | COMPLETED;
		onComplete(*this);
	}
	void abort() {
		assert(!is_finished());
		flags = FINISHED | ABORTED;
		onAbort(*this);
	}

	static int CreateGoal(int priority, Type type);

	static Goal* GetGoal(int id);
};

typedef boost::ptr_unordered_map<int, Goal> GoalSet;

extern GoalSet g_goals;

class goal_priority_less : std::binary_function<int, int, bool> {
public:
	bool operator()(int a, int b) const
	{
		GoalSet::iterator it1 = g_goals.find(a);
		if (it1 == g_goals.end())
			return false;
		GoalSet::iterator it2 = g_goals.find(b);
		if (it2 == g_goals.end())
			return true;
		const Goal* aa = it1->second;
		const Goal* bb = it2->second;
		return aa->priority < bb->priority;
	}
};


typedef std::priority_queue<int, std::vector<int>, goal_priority_less> GoalQueue;

inline Goal* Goal::GetGoal(int id)
{
	GoalSet::iterator it = g_goals.find(id);
	if (it == g_goals.end())
		return 0;
	// FIXME is that safe?
	return it->second;
}