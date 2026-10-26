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

		// Riot shield (item SPECIAL_SHIELD, console cheat "tarcza" / "shield")
		//   shield_block_chance   - % of bullets hitting from the front that the shield stops
		//   shield_damage_percent - % of the damage that frontal bullets still do when they get through
		//   shield_arc            - half-width of the protected front arc in degrees
		//   shield_durability     - how many bullets it can stop before it breaks (0 = unbreakable)
		mShieldBlockChance = static_cast<std::int16_t>(ENV_get_value_number("shield_block_chance", 85, "Extra"));
		if (mShieldBlockChance < 0) { mShieldBlockChance = 0; }
		if (mShieldBlockChance > 100) { mShieldBlockChance = 100; }

		mShieldDamagePercent = static_cast<std::int16_t>(ENV_get_value_number("shield_damage_percent", 25, "Extra"));
		if (mShieldDamagePercent < 0) { mShieldDamagePercent = 0; }
		if (mShieldDamagePercent > 100) { mShieldDamagePercent = 100; }

		mShieldArc = static_cast<std::int16_t>(ENV_get_value_number("shield_arc", 70, "Extra"));
		if (mShieldArc < 10) { mShieldArc = 10; }
		if (mShieldArc > 180) { mShieldArc = 180; }

		mShieldDurability = static_cast<std::int16_t>(ENV_get_value_number("shield_durability", 40, "Extra"));
		if (mShieldDurability < 0) { mShieldDurability = 0; }
		if (mShieldDurability > 10000) { mShieldDurability = 10000; }

		// Security (security.cpp): turrets, computers, EMP
		//   turret_range          - mapsquares a turret can see / shoot (default 8)
		//   turret_damage_player  - damage to the player per hit (player health is 200)
		//   turret_damage_npc     - damage to anybody else per hit
		//   turret_fire_ms        - ms between shots
		//   turret_yaw_offset     - degrees, if the turret model points the wrong way
		//   emp_radius            - mapsquares an EMP reaches
		//   emp_seconds           - how long it knocks things out
		//   terminal_anim         - animation (index in PROTOTYPE_DARCI1.all) for using a computer
		mTurretRange = static_cast<std::int16_t>(ENV_get_value_number("turret_range", 8, "Extra"));
		if (mTurretRange < 1) { mTurretRange = 1; }
		if (mTurretRange > 40) { mTurretRange = 40; }

		mTurretDamagePlayer = static_cast<std::int16_t>(ENV_get_value_number("turret_damage_player", 12, "Extra"));
		mTurretDamageNpc = static_cast<std::int16_t>(ENV_get_value_number("turret_damage_npc", 40, "Extra"));

		mTurretFireMs = static_cast<std::int16_t>(ENV_get_value_number("turret_fire_ms", 280, "Extra"));
		if (mTurretFireMs < 50) { mTurretFireMs = 50; }

		mTurretYawOffset = static_cast<std::int16_t>(ENV_get_value_number("turret_yaw_offset", 0, "Extra"));

		mEmpRadius = static_cast<std::int16_t>(ENV_get_value_number("emp_radius", 7, "Extra"));
		if (mEmpRadius < 1) { mEmpRadius = 1; }
		if (mEmpRadius > 40) { mEmpRadius = 40; }

		mEmpSeconds = static_cast<std::int16_t>(ENV_get_value_number("emp_seconds", 15, "Extra"));
		if (mEmpSeconds < 1) { mEmpSeconds = 1; }
		if (mEmpSeconds > 60) { mEmpSeconds = 60; }

		mTerminalAnim = static_cast<std::int16_t>(ENV_get_value_number("terminal_anim", 265, "Extra"));

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
	std::int16_t mShieldBlockChance = 85;
	std::int16_t mShieldDamagePercent = 25;
	std::int16_t mShieldArc = 70;
	std::int16_t mShieldDurability = 40;
	std::int16_t mTurretRange = 8;
	std::int16_t mTurretDamagePlayer = 12;
	std::int16_t mTurretDamageNpc = 40;
	std::int16_t mTurretFireMs = 280;
	std::int16_t mTurretYawOffset = 0;
	std::int16_t mEmpRadius = 7;
	std::int16_t mEmpSeconds = 15;
	std::int16_t mTerminalAnim = 265;
	std::int16_t mFogDistance = 50;
};