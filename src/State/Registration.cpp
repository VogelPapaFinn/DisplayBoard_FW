#include "State/Registration.hpp"

// Project includes
#include "Can.hpp"
#include "CanGroupsAndFunctions.hpp"
#include "Config.hpp"

/*
 *	constexpr
 */
constexpr auto TAG = "Registration";

/*
 *	Public Function Implementations
 */
Registration::Registration(SystemContext* p_sysCon) : State(State::REGISTRATION)
{
	sysCon_ = p_sysCon;
}

void Registration::enter()
{
	// Register necessary events on the event loop
	registerToEvents();

	// Notify the master about the startup
	registerAtMaster();
}

/*
 *	Private Function Implementations
*/
void Registration::registerToEvents()
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
			Registration* state = static_cast<Registration*>(p_state);

			/*
			 *	Get the payload
			 */
			if (p_payload == nullptr) {
				return;
			}

			Can::Frame* frame = static_cast<Can::Frame*>(p_payload);

			/*
			 *	Only listen to the master
			 */
			if (frame->sender != CAN_MASTER_ID) {
				return;
			}

			/*
			 *	Handle the frame
			 */
			state->handleCanFrame(frame);
		},
		this, &get<2>(eventHandlers_.back()));
}

void Registration::registerAtMaster() const
{
	const auto& json = *sysCon_->config->getJson();

	// Build basic CAN frame
	Can::Frame frame;
	frame.sender = 0; // Send as broadcast
	frame.target = CAN_MASTER_ID;
	frame.group = CanFrameGroups::GROUP::CONFIGURATION;
	frame.function = CanFrameGroups::CONFIGURATION::REGISTER_AT_MASTER;
	frame.dataLengthCode = 3;
	frame.answer = 0;

	// Add the data
	frame.data[0] = json["canID"].as<uint8_t>();
	frame.data[1] = json["screen"].as<uint8_t>();
	frame.data[2] = json["rotation"].as<uint8_t>();

	// Send the frame
	sysCon_->can->queueFrame(frame);
}

void Registration::handleCanFrame(const Can::Frame* p_frame)
{
	if (p_frame->group != CanFrameGroups::GROUP::CONFIGURATION) {
		return;
	}

	// Ref to the json version of the config
	auto& json = *sysCon_->config->getJson();

	// Act depending on the frame function
	switch (p_frame->function) {
		/*
		 *	Bake Configuration
		 */
		case CanFrameGroups::CONFIGURATION::BAKE_CONFIGURATION:
		{
			if (configured_) {
				return;
			}

			ESP_LOGI(TAG, "Baking configuration with canID: %d, screen: %d, rotation: %d", p_frame->data[0], p_frame->data[1], p_frame->data[2]);

			json["canID"] = p_frame->data[0];
			json["screen"] = p_frame->data[1];
			json["rotation"] = p_frame->data[2];

			// Save the config
			sysCon_->config->save();
		} // break; // The break is intentionally removed. When the configuration is baked it can be handled as confirmed

		/*
		 *	Confirm Configuration
		 */
		case CanFrameGroups::CONFIGURATION::CONFIRM_CONFIGURATION:
		{
			if (configured_) {
				return;
			}

			ESP_LOGI(TAG, "Configuration confirmed!");

			// Build the GUI
			const auto screen = json["screen"].as<uint8_t>();
			esp_event_post(SYSTEM_EVENT_BASE, LOAD_SCREEN, &screen, sizeof(screen), portMAX_DELAY);

			configured_ = true;
		} break;

		/*
		 *	Wake Up
		 */
		case CanFrameGroups::CONFIGURATION::WAKE_UP:
		{
			ESP_LOGI(TAG, "Waking up!");

			// Turn it on
			esp_event_post(SYSTEM_EVENT_BASE, TURN_ON, nullptr, 0, portMAX_DELAY);

			// Registration finished
			esp_event_post(SYSTEM_EVENT_BASE, REGISTRATION_COMPLETED, nullptr, 0, portMAX_DELAY);
		} break;

		default: ESP_LOGW(TAG, "Received unidentified CAN frame function");
	}
}
