#include "BaczekKPAI.h"
#include "Log.h"
#include "Unit.h"
#include "UnitAI.h"
#include "Goal.h"

UnitAI::UnitAI(BaczekKPAI* ai, Unit* owner): owner(owner), ai(ai), currentGoalId(-1)
{
}

UnitAI::~UnitAI(void)
{
}


////////////////////////////////////////////////////////////////////////////////
// operators

bool operator<(const UnitAI& a, const UnitAI& b)
{
	assert(a.owner);
	assert(b.owner);
	return a.owner->id < b.owner->id;
}

bool operator==(const UnitAI& a, const UnitAI& b)
{
	assert(a.owner);
	assert(b.owner);
	return a.owner->id == b.owner->id;
}

////////////////////////////////////////////////////////////////////////////////
// overloads

struct on_complete_clean_current_goal : std::unary_function<Goal&, void> {
	UnitAI* ai;
	on_complete_clean_current_goal(UnitAI* uai):ai(uai) {}
	void operator()(Goal& goal) { ai->currentGoalId = -1; ailog->info() << "cleaning currentGoal on " << ai->owner->id << std::endl; }
};

struct on_complete_clean_producing : std::unary_function<Goal&, void> {
	Unit* unit;
	on_complete_clean_producing(Unit* u):unit(u) {}
	void operator()(Goal& goal) { unit->is_producing = false; ailog->info() << "cleaning is_producing on " << unit->id << std::endl; }
};


GoalProcessor::goal_process_t UnitAI::ProcessGoal(Goal* goal)
{
	if (!goal) {
		return PROCESS_POP_CONTINUE;
	}

	if (goal->is_finished()) {
		return PROCESS_POP_CONTINUE;
	}

	if (currentGoalId >= 0) {
		Goal* current = Goal::GetGoal(currentGoalId);
		if (current && current->is_executing() && current->priority >= goal->priority) {
			return PROCESS_BREAK;
		}
	}

	if (goal->is_executing()) {
		return PROCESS_BREAK;
	}

	ailog->info() << "EXECUTE GOAL: Unit " << owner->id << " executing goal " << goal->id << " type " << goal->type << std::endl;

	switch (goal->type) {
		case BUILD_EXPANSION: {
			if (!owner->is_constructor) {
				ailog->error() << "BUILD_EXPANSION issued to non-constructor unit" << std::endl;
				return PROCESS_POP_CONTINUE;
			}
			Command c;
			c.id = -FindExpansionUnitDefId();
			assert(c.id);
			assert(!goal->params.empty());
			const float3& param = boost::get<float3>(goal->params[0]);
			c.params.push_back(param.x);
			c.params.push_back(param.y);
			c.params.push_back(param.z);
			ai->cb->GiveOrder(owner->id, &c);
			goal->OnComplete(on_complete_clean_current_goal(this));
			goal->OnComplete(on_complete_clean_producing(this->owner));
			owner->is_producing = true;
			goal->start();
			currentGoalId = goal->id;
			break;
		}

		case BUILD_CONSTRUCTOR: {
			if (!owner->is_base) {
				ailog->error() << "BUILD_CONSTRUCTOR issued to non-base unit" << std::endl;
				return PROCESS_POP_CONTINUE;
			}
			if (goal->is_executing()) {
				return PROCESS_BREAK;
			}
			Command c;
			c.id = -FindConstructorUnitDefId();
			ai->cb->GiveOrder(owner->id, &c);
			// FIXME XXX no way to track goal progress!
			goal->complete();
			return PROCESS_BREAK;
		}

		case MOVE:
		case RETREAT: {
			if (goal->params.empty()) {
				ailog->error() << "no params on RETREAT or MOVE goal" << std::endl;
				return PROCESS_POP_CONTINUE;
			}
			float3* param = boost::get<float3>(&goal->params[0]);
			if (!param) {
				ailog->error() << "invalid param on RETREAT or MOVE goal" << std::endl;
				return PROCESS_POP_CONTINUE;
			}
			Command c;
			c.id = CMD_MOVE;
			// TODO formation offset
			c.AddParam(param->x);
			c.AddParam(param->y);
			c.AddParam(param->z);
			ai->cb->GiveOrder(owner->id, &c);
			goal->start();
			currentGoalId = goal->id;
			
			return PROCESS_BREAK;
		}

		default:
			return PROCESS_POP_CONTINUE;
	}
	return PROCESS_BREAK;
}


void UnitAI::Update()
{
	int frameNum = ai->cb->GetCurrentFrame();

	if (frameNum % 30 == 0) {
		std::sort(goals.begin(), goals.end(), goal_priority_less());
		DumpGoalStack("Unit");
		ProcessGoalStack(frameNum);
	}
}


void UnitAI::OwnerKilled()
{
	// signal listeners
	onKilled(*this);

	currentGoalId = -1;
	BOOST_FOREACH(int gid, goals) {
		Goal* g = Goal::GetGoal(gid);
		if (g)
			Goal::RemoveGoal(g);
	}
	owner = 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////
// utils

// TODO move to a data file
int UnitAI::FindExpansionUnitDefId()
{
	assert(owner);
	const UnitDef *ud = ai->cb->GetUnitDef(owner->id);
	assert(ud);
	
	if (ud->name == "assembler") {
		const UnitDef *tobuild = ai->cb->GetUnitDef("socket");
		assert(tobuild);
		return tobuild->id;
	} else if (ud->name == "trojan") {
		const UnitDef *tobuild = ai->cb->GetUnitDef("window");
		assert(tobuild);
		return tobuild->id;
	} else if (ud->name == "gateway") {
		const UnitDef *tobuild = ai->cb->GetUnitDef("port");
		assert(tobuild);
		return tobuild->id;
	}
	return 0;
}

// TODO move to a data file
int UnitAI::FindConstructorUnitDefId()
{
	assert(owner);
	const UnitDef *ud = ai->cb->GetUnitDef(owner->id);
	assert(ud);
	
	if (ud->name == "kernel") {
		const UnitDef *tobuild = ai->cb->GetUnitDef("assembler");
		assert(tobuild);
		return tobuild->id;
	} else if (ud->name == "hole") {
		const UnitDef *tobuild = ai->cb->GetUnitDef("trojan");
		assert(tobuild);
		return tobuild->id;
	} else if (ud->name == "carrier") {
		const UnitDef *tobuild = ai->cb->GetUnitDef("gateway");
		assert(tobuild);
		return tobuild->id;
	}
	return 0;
}

// TODO move to a data file
int UnitAI::FindSpamUnitDefId()
{
	assert(owner);
	const UnitDef *ud = ai->cb->GetUnitDef(owner->id);
	assert(ud);
	
	if (ud->name == "kernel" || ud->name == "socket") {
		const UnitDef *tobuild = ai->cb->GetUnitDef("bit");
		assert(tobuild);
		return tobuild->id;
	} else if (ud->name == "hole" || ud->name == "window") {
		const UnitDef *tobuild = ai->cb->GetUnitDef("bug");
		assert(tobuild);
		return tobuild->id;
	} else if (ud->name == "carrier" || ud->name == "port") {
		const UnitDef *tobuild = ai->cb->GetUnitDef("packet");
		assert(tobuild);
		return tobuild->id;
	}
	return 0;
}


void UnitAI::CompleteCurrentGoal()
{
	if (currentGoalId >= 0) {
		Goal* currentGoal = Goal::GetGoal(currentGoalId);
		if (currentGoal && !currentGoal->is_finished()) {
			currentGoal->complete();
		}
	}
	currentGoalId = -1;
	owner->is_producing = false;
}
