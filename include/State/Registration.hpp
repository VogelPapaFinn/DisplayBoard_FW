#pragma once

// Project includes
#include "Can.hpp"
#include "State.hpp"
#include "SystemContext.hpp"

class Registration : public State
{
public:
	Registration(SystemContext* p_sysCon);

	void enter() override;

private:
	/*
	 *	Private Functions
	 */
	void registerToEvents();

	void registerAtMaster() const;

	void handleCanFrame(const Can::Frame* p_frame);

	/*
	 *	Private Variables
	 */
	bool configured_ = false;
};
