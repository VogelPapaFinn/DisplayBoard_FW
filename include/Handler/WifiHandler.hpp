#pragma once

// Project includes
#include "SystemContext.hpp"
#include "Can.hpp"

class WifiHandler
{
public:
	WifiHandler(SystemContext* p_sysCon);

private:
	/*
	 *	Private Functions
	 */
	void setMasterIp(const Can::Frame* p_frame) const;

	void setSsid(const Can::Frame* p_frame) const;

	void setPassword(const Can::Frame* p_frame) const;

	void join() const;

	/*
	 *	Private Variables
	 */
	SystemContext* sysCon_ = nullptr;
};