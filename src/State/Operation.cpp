#include "State/Operation.hpp"

// Project includes
#include "Can.hpp"
#include "CanGroupsAndFunctions.hpp"
#include "Gui.hpp"
#include "WifiJoin.hpp"

// espidf includes
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

/*
 *	constexpr
 */
constexpr auto TAG = "Operation";

/*
 *	Public Function Implementations
 */
Operation::Operation(SystemContext* p_sysCon) : State(State::OPERATING) { sysCon_ = p_sysCon; }

void Operation::enter()
{
	/*
	 *	Register to all events
	 */
	registerToEvents();
}

/*
 *	Private Function Implementations
 */
void Operation::registerToEvents()
{
	/*
	 *	CAN frame received
	 */
	eventHandlers_.push_back(std::make_tuple(SYSTEM_EVENT_BASE, CAN_FRAME_RECEIVED, esp_event_handler_instance_t()));
	esp_event_handler_instance_register(
		SYSTEM_EVENT_BASE, CAN_FRAME_RECEIVED,
		[](void* p_state, esp_event_base_t, int32_t, void* p_payload)
		{
			/*
			 *	Get the state ptr
			 */
			if (p_state == nullptr) {
				return;
			}

			// Convert it
			Operation* state = static_cast<Operation*>(p_state);

			/*
			 *	Get the payload
			 */
			if (p_payload == nullptr) {
				return;
			}

			Can::Frame* frame = static_cast<Can::Frame*>(p_payload);


			/*
			 *	Pass it to the CAN frame handler
			 */
			state->handleCanFrame(frame);
		},
		this, &get<2>(eventHandlers_.back()));
}

void Operation::handleCanFrame(const Can::Frame* p_frame) const
{
	/*
	 *	New Sensor Data
	 */
	if (p_frame->group == CanFrameGroups::SENSOR && p_frame->function == CanFrameGroups::SENSOR::BROADCAST_DATA &&
		p_frame->dataLengthCode == 8) {
		const Gui::SensorData data = {.fuelLevel = p_frame->data[0],
									  .oilPressure = static_cast<bool>(p_frame->data[1]),
									  .waterTemperature = p_frame->data[2],
									  .rpm = static_cast<uint16_t>((p_frame->data[3] << 8) + p_frame->data[4]),
									  .speed = p_frame->data[5],
									  .leftIndicatorActive = static_cast<bool>(p_frame->data[6]),
									  .rightIndicatorActive = static_cast<bool>(p_frame->data[7])};

		esp_event_post(SYSTEM_EVENT_BASE, NEW_SENSOR_DATA, &data, sizeof(data), portMAX_DELAY);
	}

	/*
	 *	Execute Update
	 */
	if (p_frame->group == CanFrameGroups::WIFI && p_frame->function == CanFrameGroups::WIFI::EXECUTE_UPDATE) {
		ESP_LOGI(TAG, "Starting OTA update");

		/*
		 *	Build the Webaddress
		 */
		const auto& masterIp = sysCon_->wifi->getMasterIp();
		const std::string downloadPath = std::format("http://{}.{}.{}.{}/display-update.bin", masterIp[0], masterIp[1], masterIp[2], masterIp[3]);

		/*
		 *	Create the configurations
		 */
		const esp_http_client_config_t config = {
			.url = downloadPath.c_str(),
			.timeout_ms = 5000,
			.keep_alive_enable = true,
		};

		const esp_https_ota_config_t otaConfig = {
			.http_config = &config,
		};

		/*
		 *	Execute the Update
		 */
		auto ret = esp_https_ota(&otaConfig);
		if (ret == ESP_OK) {
			/*
			 *	Update successful
			 */
			ESP_LOGI(TAG, "OTA Update successful!");

			Can::Frame txFrame;
			txFrame.sender = (*sysCon_->config->getJson())["canID"].as<uint8_t>();
			txFrame.target = CAN_MASTER_ID;
			txFrame.group = CanFrameGroups::GROUP::WIFI;
			txFrame.function = CanFrameGroups::WIFI::EXECUTE_UPDATE;
			txFrame.answer = true;

			sysCon_->can->queueFrame(txFrame);

			return;
		}

		/*
		 *	Update failed
		 */
		ESP_LOGE(TAG, "OTA Update error: %s", esp_err_to_name(ret));
	}
}
