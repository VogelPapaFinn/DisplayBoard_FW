#include "State/Registration.hpp"

// Project includes
#include "Event.hpp"

/*
 *	Public Function Implementations
 */
Registration::Registration() : State(State::REGISTRATION) {}

void Registration::enter() { registerAtMaster(); }

void Registration::handleCanFrame(const Can::Frame& frame)
{
	if (frame.group != CanFrame::GROUP::CONFIGURATION) {
		return;
	}

	if (frame.sender != MASTER_CAN_ID) {
		return;
	}

	if (frame.target != core_->getCanId()) {
		return;
	}

	esp_rom_printf("Received Frame %s\n", frame.toString().c_str());

	// Act depending on the function type
	switch (frame.function) {
		case CanFrame::SET_ID:
			{
				if (frame.dataLengthCode <= 0) {
					return;
				}

				core_->setCanId(frame.data[0]);

				confirmNewId();
			}
			break;

		case CanFrame::CONFIRM_ID:
			{
			}
			break;

		case CanFrame::SET_SCREEN:
			{
				if (frame.dataLengthCode <= 0) {
					return;
				}

				core_->getGui()->queueEventFromISR(Event(Event::TYPE::SET_SCREEN, frame.data[0]));

				confirmScreen();
			}
			break;

		case CanFrame::WAKE_UP:
			{
				core_->getGui()->queueEventFromISR(Event(Event::TYPE::WAKE_UP));
			}
			break;

		default:
			{
				esp_rom_printf("default\n");
			}
			break;
	}
}

/*
 *	Private Function Implementations
 */
void Registration::confirmNewId() const
{
	Can::Frame txFrame;

	txFrame.sender = core_->getCanId();
	txFrame.target = 1;
	txFrame.group = CanFrame::GROUP::CONFIGURATION;
	txFrame.function = CanFrame::CONFIGURATION::SET_ID;
	txFrame.answer = 1;

	core_->getCan()->queueFrame(txFrame);
}

void Registration::registerAtMaster() const
{
	Can::Frame txFrame;

	txFrame.sender = core_->getCanId();
	txFrame.target = MASTER_CAN_ID;
	txFrame.group = CanFrame::GROUP::CONFIGURATION;
	txFrame.function = CanFrame::CONFIGURATION::REGISTER_AT_MASTER;
	txFrame.answer = 0;

	core_->getCan()->queueFrame(txFrame);
}

void Registration::confirmScreen() const
{
	Can::Frame txFrame;

	txFrame.sender = core_->getCanId();
	txFrame.target = MASTER_CAN_ID;
	txFrame.group = CanFrame::GROUP::CONFIGURATION;
	txFrame.function = CanFrame::CONFIGURATION::SET_SCREEN;
	txFrame.answer = 1;

	core_->getCan()->queueFrame(txFrame);
}
