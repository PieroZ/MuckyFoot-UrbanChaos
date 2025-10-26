#pragma once

#include "env.h"
#include <cstdint>

struct ConfigExtras
{
	ConfigExtras()
	{
		mCustomTurnRateValue = static_cast<std::int16_t>(ENV_get_value_number("custom_turn_rate_value", 70, "Extra"));
		mCustomTurnRateEnabled = ENV_get_value_number("custom_turn_rate", 0, "Extra");
	};
	ConfigExtras(const ConfigExtras&) = delete;
	void operator=(const ConfigExtras&) = delete;


	static ConfigExtras& getInstance()
	{
		static ConfigExtras instance;
		return instance;
	}
	
	bool mCustomTurnRateEnabled = false;
	std::int16_t mCustomTurnRateValue = 70;
};