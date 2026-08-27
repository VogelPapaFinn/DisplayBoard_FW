#pragma once

// Project includes
#include "State.hpp"

class Operation : public State
{
public:
	Operation(SystemContext* p_sysCon);

	void enter() override;

private:
	/*
	 *	Private Functions
	 */
	void registerToEvents();
};
