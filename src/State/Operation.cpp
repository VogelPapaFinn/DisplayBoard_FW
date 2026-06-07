#include "State/Operation.hpp"

/*
 *	Public Function Implementations
 */
Operation::Operation() : State(State::OPERATING) {}

void Operation::enter() {}

void Operation::handleCanFrame(const Can::Frame& frame)
{
	if (frame.group != CanFrame::GROUP::SENSOR) {
		return;
	}

	if (frame.sender != CAN_MASTER_ID) {
		return;
	}

	esp_rom_printf("Received Frame %s\n", frame.toString().c_str());

	// Act depending on the function type
	switch (frame.function) {
		case CanFrame::SENSOR::BROADCAST_DATA:
			{
				if (frame.dataLengthCode <= 7) {
					return;
				}

				Event event(Event::NEW_SENSOR_DATA);
				for (uint8_t i = 0; i < frame.dataLengthCode; i++) {
					event.canData[i] = frame.data[i];
				}
				core_->getGui()->queueEventFromISR(event);
			}
			break;
		default:;
	}
}
