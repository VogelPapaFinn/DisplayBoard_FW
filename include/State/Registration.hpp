#pragma once

// Project includes
#include "State.hpp"

class Registration : public State
{
public:
	Registration();

	void enter() override;

	void handleCanFrame(const Can::Frame& frame) override;

private:
	/*
	 *	Private Functions
	 */
	void confirmNewId() const;

	void registerAtMaster() const;

	void confirmScreen() const;
};
