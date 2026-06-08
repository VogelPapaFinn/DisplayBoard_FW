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

				(*jsonConfig_)["screen"] = frame.data[0];

				confirmScreen();
			}
			break;

		case CanFrame::SET_ROTATION:
		{
			if (frame.target != core_->getCanId()) {
				return;
			}

			if (frame.dataLengthCode <= 0) {
				return;
			}

			// Save new rotation
			(*jsonConfig_)["rotation"] = frame.data[0];
			core_->saveConfig();

			// Apply & confirm new rotation
			core_->getDisplayDriver()->setRotated(frame.data[0]);
			confirmRotation();
		}
			break;

		case CanFrame::REGISTRATION_COMPLETED:
		{
			if (frame.target != core_->getCanId()) {
				return;
			}

			core_->saveConfig();

			// Build the GUI
			Event event(Event::TYPE::SET_SCREEN);
			event.intData = (*jsonConfig_)["screen"];
			core_->getGui()->queueEventFromISR(event);
		}
			break;

		case CanFrame::WAKE_UP:
			{
				core_->getGui()->queueEventFromISR(Event(Event::TYPE::WAKE_UP));

				const Event event(Event::REGISTRATION_FINISHED);
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
	esp_rom_printf("Confirm ID!\n");

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
	esp_rom_printf("Register at master!\n");

	Can::Frame txFrame;

	txFrame.sender = core_->getCanId();
	txFrame.target = CAN_MASTER_ID;
	txFrame.group = CanFrame::GROUP::CONFIGURATION;
	txFrame.function = CanFrame::CONFIGURATION::REGISTER_AT_MASTER;
	txFrame.dataLengthCode = 2;
	txFrame.answer = 0;

	txFrame.data[0] = (*jsonConfig_)["screen"].as<uint8_t>();
	txFrame.data[1] = (*jsonConfig_)["rotation"].as<uint8_t>();

	core_->getCan()->queueFrame(txFrame);
}

void Registration::confirmScreen() const
{
	esp_rom_printf("Confirm Screen!\n");

	Can::Frame txFrame;

	txFrame.sender = core_->getCanId();
	txFrame.target = CAN_MASTER_ID;
	txFrame.group = CanFrame::GROUP::CONFIGURATION;
	txFrame.function = CanFrame::CONFIGURATION::SET_SCREEN;
	txFrame.answer = 1;

	core_->getCan()->queueFrame(txFrame);
}

void Registration::confirmRotation() const
{
	esp_rom_printf("Confirm rotation!\n");

	Can::Frame txFrame;

	txFrame.sender = core_->getCanId();
	txFrame.target = CAN_MASTER_ID;
	txFrame.group = CanFrame::GROUP::CONFIGURATION;
	txFrame.function = CanFrame::CONFIGURATION::SET_ROTATION;
	txFrame.answer = 1;

	core_->getCan()->queueFrame(txFrame);
}
