#pragma once

// Project includes
#include "State.hpp"

// espidf includes
#include "freertos/FreeRTOS.h"

class Operation : public State
{
public:
	Operation();

	void enter() override;

	void handleCanFrame(const Can::Frame& frame) override;

private:
	TaskHandle_t espTempTask_ = nullptr;
};
