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
		mKillCam = ENV_get_value_number("killcam", 1, "Extra");

		mDrawDistance = static_cast<std::int16_t>(ENV_get_value_number("draw_distance", 64, "Extra"));
		if (mDrawDistance < 10)
		{
			mDrawDistance = 10;
		}
		if (mDrawDistance > 120)
		{
			mDrawDistance = 120;	// The whole map is 128 mapsquares across
		}

		// Fog: starts at fog_start % of the draw distance and is complete at the draw distance.
		mFogStart = static_cast<std::int16_t>(ENV_get_value_number("fog_start", 70, "Extra"));
		if (mFogStart < 10) { mFogStart = 10; }
		if (mFogStart > 95) { mFogStart = 95; }

		// Crinkles (3D relief on some flat walls):
		// 0 = off (flat walls), 1 = always full 3D (default), 2 = original (grows in as you get near)
		mCrinkleMode = static_cast<std::int16_t>(ENV_get_value_number("crinkle_mode", 1, "Extra"));

		// Drivers running away from the player:
		// 0 = nobody, 1 = only fugitives from the "uciekinier" cheat, 2 = + criminals, 3 = + every driver
		mCarFlee = static_cast<std::int16_t>(ENV_get_value_number("car_flee", 1, "Extra"));

		// Chance (%) that a shot hitting a car bursts a tyre (shotguns: twice that)
		mTyreShotChance = static_cast<std::int16_t>(ENV_get_value_number("tyre_shot_chance", 35, "Extra"));
		if (mTyreShotChance < 0) { mTyreShotChance = 0; }
		if (mTyreShotChance > 100) { mTyreShotChance = 100; }

		mFogDistance = static_cast<std::int16_t>(ENV_get_value_number("fog_distance", 60, "Extra"));
		if (mFogDistance < 10)
		{
			mFogDistance = 10;
		}
	}

	static ConfigExtras& getInstance()
	{
		static ConfigExtras instance;
		return instance;
	}
	
	bool mCustomTurnRateEnabled = false;
	bool mLegolasAim = false;
	bool mKillCam = true;			// Slow motion + orbiting camera when the last enemy goes down
	std::int16_t mCustomTurnRateValue = 70;
	std::int32_t mMax_range_blocks = 1600;
	std::int16_t mMouseShootCooldown = 10;
	bool mMouseInput = false;
	bool mShowHitMarker = false;
	std::int16_t mDrawDistance = 64;
	std::int16_t mFogStart = 70;
	std::int16_t mCrinkleMode = 1;
	std::int16_t mCarFlee = 1;
	std::int16_t mTyreShotChance = 35;
	std::int16_t mFogDistance = 50;
};