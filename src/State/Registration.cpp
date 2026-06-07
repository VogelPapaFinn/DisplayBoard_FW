#include "State/Registration.hpp"

// Project includes
#include "Event.hpp"

/*
 *	Public Function Implementations
 */
Registration::Registration() : State(State::REGISTRATION) {}

void Registration::enter()
{
	jsonConfig_ = core_->getConfig();
	if (!jsonConfig_->isNull()) {
		const auto& canID = (*jsonConfig_)["canID"];
		if (canID) {
			core_->setCanId(canID.as<unsigned int>());
		}
	}

	registerAtMaster();
}

void Registration::handleCanFrame(const Can::Frame& frame)
{
	if (frame.group != CanFrame::GROUP::CONFIGURATION) {
		return;
	}

	if (frame.sender != CAN_MASTER_ID) {
		return;
	}

	esp_rom_printf("Received Frame %s\n", frame.toString().c_str());

	// Act depending on the function type
	switch (frame.function) {
		case CanFrame::SET_ID:
			{
				if (frame.target != core_->getCanId()) {
					return;
				}

				if (frame.dataLengthCode <= 0) {
					return;
				}

				(*jsonConfig_)["canID"] = frame.data[0];
				core_->saveConfig();

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
				if (frame.target != core_->getCanId()) {
					return;
				}

				if (frame.dataLengthCode <= 0) {
					return;
				}

				Event event(Event::TYPE::SET_SCREEN);
				event.intData = frame.data[0];
				core_->getGui()->queueEventFromISR(event);

				confirmScreen();
			}
			break;

		case CanFrame::WAKE_UP:
			{
				core_->getGui()->queueEventFromISR(Event(Event::TYPE::WAKE_UP));

				Event event(Event::REGISTRATION_FINISHED);
				xQueueSend(core_->getMainEventQueue(), &event, portMAX_DELAY);
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
	txFrame.target = CAN_MASTER_ID;
	txFrame.group = CanFrame::GROUP::CONFIGURATION;
	txFrame.function = CanFrame::CONFIGURATION::SET_ID;
	txFrame.answer = 1;

	core_->getCan()->queueFrame(txFrame);
}

void Registration::registerAtMaster() const
{
	Can::Frame txFrame;

	txFrame.sender = core_->getCanId();
	txFrame.target = CAN_MASTER_ID;
	txFrame.group = CanFrame::GROUP::CONFIGURATION;
	txFrame.function = CanFrame::CONFIGURATION::REGISTER_AT_MASTER;
	txFrame.answer = 0;

	core_->getCan()->queueFrame(txFrame);
}

void Registration::confirmScreen() const
{
	Can::Frame txFrame;

	txFrame.sender = core_->getCanId();
	txFrame.target = CAN_MASTER_ID;
	txFrame.group = CanFrame::GROUP::CONFIGURATION;
	txFrame.function = CanFrame::CONFIGURATION::SET_SCREEN;
	txFrame.answer = 1;

	core_->getCan()->queueFrame(txFrame);
}
