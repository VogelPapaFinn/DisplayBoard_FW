#pragma once

// Project includes
#include "State.hpp"

class Operation : public State
{
public:
	Operation();

	void enter() override;

	void handleCanFrame(const Can::Frame& frame) override;

private:
};
