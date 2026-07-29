#pragma once

#include "env.h"
#include <cstdint>

struct ConfigExtras
{
	ConfigExtras()
	{
		Update();
	};
	ConfigExtras(const ConfigExtras&) = delete;
	void operator=(const ConfigExtras&) = delete;

	void Update()
	{
		mCustomTurnRateValue = static_cast<std::int16_t>(ENV_get_value_number("custom_turn_rate_value", 70, "Extra"));
		mCustomTurnRateEnabled = ENV_get_value_number("custom_turn_rate", 0, "Extra");
		mMouseInput = ENV_get_value_number("mouse_input", 0, "Extra");
		mMax_range_blocks = ENV_get_value_number("max_range_blocks", 1600, "Extra");
		mMouseShootCooldown = ENV_get_value_number("mouse_shoot_cooldown", 10, "Extra");
		mLegolasAim = ENV_get_value_number("legolas_aim", 0, "Extra");
	}

	static ConfigExtras& getInstance()
	{
		static ConfigExtras instance;
		return instance;
	}
	
	bool mCustomTurnRateEnabled = false;
	bool mLegolasAim = false;
	std::int16_t mCustomTurnRateValue = 70;
	std::int32_t mMax_range_blocks = 1600;
	std::int16_t mMouseShootCooldown = 10;
	bool mMouseInput = false;
	bool mShowHitMarker = false;
};