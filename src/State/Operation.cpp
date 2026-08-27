#include "State/Operation.hpp"

// Project includes
#include "Can.hpp"
#include "CanGroupsAndFunctions.hpp"
#include "Gui.hpp"

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
			 *	Ignore it, if its not new sensor data
			 */
			if (frame->group != CanFrameGroups::SENSOR || frame->function != CanFrameGroups::SENSOR::BROADCAST_DATA ||
				frame->dataLengthCode != 8) {
				return;
			}

			/*
			 *	Throw event
			 */
			const Gui::SensorData data = {.fuelLevel = frame->data[0],
										  .oilPressure = static_cast<bool>(frame->data[1]),
										  .waterTemperature = frame->data[2],
										  .rpm = static_cast<uint16_t>((frame->data[3] << 8) + frame->data[4]),
										  .speed = frame->data[5],
										  .leftIndicatorActive = static_cast<bool>(frame->data[6]),
										  .rightIndicatorActive = static_cast<bool>(frame->data[7])};

			esp_event_post(SYSTEM_EVENT_BASE, NEW_SENSOR_DATA, &data, sizeof(data), portMAX_DELAY);
		},
		this, &get<2>(eventHandlers_.back()));
}
