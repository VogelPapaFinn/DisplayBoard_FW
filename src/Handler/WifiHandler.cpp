#include "Handler/WifiHandler.hpp"

// Project includes
#include "Can.hpp"
#include "CanGroupsAndFunctions.hpp"
#include "Events.hpp"
#include "WifiJoin.hpp"

// espidf includes
#include "esp_event.h"
#include "esp_log.h"

/*
 *	constexpr
 */
constexpr auto TAG = "WifiHandler";

/*
 *	Public function implementations
 */
WifiHandler::WifiHandler(SystemContext* p_sysCon)
{
	sysCon_ = p_sysCon;

	/*
	 *	Register event handlers
	 */
	esp_event_handler_instance_register(
		SYSTEM_EVENT_BASE, CAN_FRAME_RECEIVED,
		[](void* p_handler, esp_event_base_t, int32_t, void* p_payload)
		{
			/*
			 *	Get the instance
			 */
			if (p_handler == nullptr) {
				return;
			}

			WifiHandler* handler = static_cast<WifiHandler*>(p_handler);

			/*
			 *	Get the payload
			 */
			if (p_payload == nullptr) {
				return;
			}

			Can::Frame* frame = static_cast<Can::Frame*>(p_payload);

			/*
			 *	Act depending on the type of frame
			 */
			if (frame->group != CanFrameGroups::GROUP::WIFI) {
				return;
			}
			switch (frame->function) {
				case CanFrameGroups::WIFI::SET_MASTER_IP:
					handler->setMasterIp(frame);
					break;
				case CanFrameGroups::WIFI::SET_SSID:
					handler->setSsid(frame);
					break;
				case CanFrameGroups::WIFI::SET_PASSWORD:
					handler->setPassword(frame);
					break;
				case CanFrameGroups::WIFI::JOIN_WIFI:
					handler->join();
					break;
				default:;
			}
		},
		this, nullptr);
}

/*
 *	Private Functions Implementations
 */
void WifiHandler::setMasterIp(const Can::Frame* p_frame) const
{
	sysCon_->wifi->setMasterIp({p_frame->data[0], p_frame->data[1], p_frame->data[2], p_frame->data[3]});
}

void WifiHandler::setSsid(const Can::Frame* p_frame) const
{
	// Get the current ssid
	std::string ssid = sysCon_->wifi->getSsid();

	// Append the new data
	for (uint8_t i = 0; i < p_frame->dataLengthCode; i++) {
		ssid += p_frame->data[i];
	}

	// Save it
	sysCon_->wifi->setSsid(ssid);
}

void WifiHandler::setPassword(const Can::Frame* p_frame) const
{
	// Get the current password
	std::string password = sysCon_->wifi->getPassword();

	// Append the new data
	for (uint8_t i = 0; i < p_frame->dataLengthCode; i++) {
		password += p_frame->data[i];
	}

	// Save it
	sysCon_->wifi->setPassword(password);
}

void WifiHandler::join() const
{
	/*
	 *	Notify the master on a successful connect
	 */
	sysCon_->wifi->callOnSuccess(
		[this]
		{
			Can::Frame frame;
			frame.sender = (*sysCon_->config->getJson())["canID"].as<uint8_t>();
			frame.target = CAN_MASTER_ID;
			frame.group = CanFrameGroups::GROUP::WIFI;
			frame.function = CanFrameGroups::WIFI::JOIN_WIFI;
			frame.answer = true;

			sysCon_->can->queueFrame(frame);
		});

	/*
	 *	Connect to the AP
	 */
	sysCon_->wifi->start();
}
