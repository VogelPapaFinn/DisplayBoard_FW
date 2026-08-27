#pragma once

// C++ includes
#include <cstdint>

/*
 *	Prototypes to prevent circular inclusion
 */
class Can;
class Filesystem;
class Config;
class WifiJoin;
class ST77916;

/**
 * \brief Represents the system context containing various core components.
 */
struct SystemContext
{
	//! The CAN bus interface
	Can* can = nullptr;

	//! The filesystem interface
	Filesystem* filesystem = nullptr;

	//! The configuration interface
	Config* config = nullptr;

	WifiJoin* wifi = nullptr;
};