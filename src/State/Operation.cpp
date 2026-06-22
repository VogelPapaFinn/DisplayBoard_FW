#include "State/Operation.hpp"

// espidf includes
#include "driver/temperature_sensor.h"

/*
 *	constexpr
 */
constexpr auto TAG = "Operation";

/*
 *	Can rx callback function
 */
static void espTempTask(void* param)
{
	temperature_sensor_handle_t sensor;
	temperature_sensor_config_t sensorConfig = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);

	if (temperature_sensor_install(&sensorConfig, &sensor) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to install internal temperature sensor");
		vTaskDelete(nullptr);
	}

	if (temperature_sensor_enable(sensor) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to enable internal temperature sensor");
		vTaskDelete(nullptr);
	}

	auto core = Core::get();

	while (true) {
		float temp = 0.0f;
		esp_err_t ret = temperature_sensor_get_celsius(sensor, &temp);

		if (ret == ESP_OK) {
			core->getGui()->setInternalTemp(temp);
		}

		vTaskDelay(pdMS_TO_TICKS(5000));
	}
}

/*
 *	Public Function Implementations
 */
Operation::Operation() : State(State::OPERATING) {}

void Operation::enter()
{
	// Start task to measure and display the ESP32 temperature
	if (xTaskCreate(espTempTask, "ESP32TempTask", 2048 * 3, NULL, 2, &espTempTask_) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create task to measure and display the ESP32 internal temperature");
		return;
	}
}

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
					core_->getGui()->queueEvent(event);
				}
				break;

			default:;
		}
	}

	/*
	 *	Wifi Frames
	 */
	else if (frame.group == CanFrame::WIFI) {
		if (frame.sender != CAN_MASTER_ID) {
			return;
		}

		// Act depending on the function type
		switch (frame.function) {
			case CanFrame::WIFI::SET_MASTER_IP:
				{
					core_->getWifi()->setMasterIp({frame.data[0], frame.data[1], frame.data[2], frame.data[3]});
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

					xQueueSend(mainEventQueue, &event, portMAX_DELAY);
				}
				break;

			case CanFrame::WIFI::EXECUTE_UPDATE:
				{
					if (frame.target != core_->getCanId()) {
						return;
					}

					const auto mainEventQueue = core_->getMainEventQueue();

					Event event;
					event.type = Event::EXECUTE_UPDATE;

					xQueueSend(mainEventQueue, &event, portMAX_DELAY);
				}
				break;

			default:;
		}
	}
}
