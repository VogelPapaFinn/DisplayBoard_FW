#include "State/Operation.hpp"

/*
 *	Public Function Implementations
 */
Operation::Operation() : State(State::OPERATING) {}

void Operation::enter() {}

void Operation::handleCanFrame(const Can::Frame& frame)
{
	/*
	 *	Sensor Frames
	 */
	if (frame.group == CanFrame::SENSOR) {
		if (frame.sender != CAN_MASTER_ID) {
			return;
		}

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

	/*
	 *	Wifi Frames
	 */
	else if (frame.group == CanFrame::WIFI) {
		if (frame.sender != CAN_MASTER_ID && frame.sender != CAN_BROADCAST_ID) {
			return;
		}

		// Act depending on the function type
		switch (frame.function) {
			case CanFrame::WIFI::SET_MASTER_IP:
				{
					core_->getWifi()->setMasterIp({frame.data[0], frame.data[1], frame.data[2], frame.data[3]});

					esp_rom_printf("New Master IP: %d.%d.%d.%d \n", frame.data[0], frame.data[1], frame.data[2], frame.data[3]);
				}
				break;

			case CanFrame::WIFI::SET_SSID:
				{
					auto ssid = core_->getWifi()->getSSID();

					for (uint8_t i = 0; i < frame.dataLengthCode; i++) {
						ssid += frame.data[i];
					}

					core_->getWifi()->setSSID(ssid);
				}
				break;

			case CanFrame::WIFI::SET_PASSWORD:
				{
					auto password = core_->getWifi()->getPassword();

					for (uint8_t i = 0; i < frame.dataLengthCode; i++) {
						password += frame.data[i];
					}

					core_->getWifi()->setPassword(password);
				}
				break;

			case CanFrame::WIFI::JOIN_WIFI:
				{
					const auto mainEventQueue = core_->getMainEventQueue();

					Event event;
					event.type = Event::JOIN_WIFI;

					BaseType_t xHigherPriorityTaskWoken = pdFALSE;
					xQueueSendFromISR(mainEventQueue, &event, &xHigherPriorityTaskWoken);
				}
				break;

			case CanFrame::WIFI::EXECUTE_UPDATE:
				{
					const auto mainEventQueue = core_->getMainEventQueue();

					Event event;
					event.type = Event::EXECUTE_UPDATE;

					BaseType_t xHigherPriorityTaskWoken = pdFALSE;
					xQueueSendFromISR(mainEventQueue, &event, &xHigherPriorityTaskWoken);
				}
				break;

			default:;
		}
	}
}
