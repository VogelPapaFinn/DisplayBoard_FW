#pragma once

// Project includes
#include "State.hpp"
#include "Can.hpp"

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

	void handleCanFrame(const Can::Frame* p_frame) const;
};
