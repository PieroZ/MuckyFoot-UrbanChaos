// free_roam.camera.h
#pragma once
#include "fc.h"
#include	<cstdint>
#include	<cmath>

// Forward declaration
struct Thing;

struct FreeRoamCamera
{
	// Position left as int64_t to preserve existing usage; change if you prefer floats.
	std::int64_t PositionX;
	std::int64_t PositionY;
	std::int64_t PositionZ;

	// Use floats for orientation (angles) so we can store fractional radians/degrees.
	float Pitch;
	float Yaw;
	float Roll;

	// Fields used for smooth mouse control (persist with the camera)
	float targetYaw;
	float targetPitch;
	float smoothYaw;
	float smoothPitch;

	float FieldOfView;
	bool IsActive;
	bool FollowPlayer;

	static FreeRoamCamera& GetInstance()
	{
		static FreeRoamCamera instance;
		return instance;
	}

	FreeRoamCamera()
		: PositionX(0), PositionY(0), PositionZ(0),
		Pitch(0.0f), Yaw(0.0f), Roll(0.0f),
		targetYaw(0.0f), targetPitch(0.0f), smoothYaw(0.0f), smoothPitch(0.0f),
		FieldOfView(90.0f), IsActive(false), FollowPlayer(false)
	{
	}

	void CopyFromFC(FC_Cam* fc)
	{
		PositionX = fc->x;
		PositionY = fc->y;
		PositionZ = fc->z;
		// Cast/conversion in case FC_Cam stores integers or floats
		Pitch = static_cast<float>(fc->pitch);
		Yaw = static_cast<float>(fc->yaw);
		Roll = static_cast<float>(fc->roll);
	}

	void CopyToFC(FC_Cam* fc)
	{
		fc->x = PositionX;
		fc->y = PositionY;
		fc->z = PositionZ;
		fc->pitch = Pitch;
		fc->yaw = Yaw;
		fc->roll = Roll;
	}

	// Call this when entering FreeRoam to initialize smoothing targets from current camera angles
	void InitFreeRoamTargetsFromCurrent()
	{
		targetYaw = smoothYaw = Yaw;
		targetPitch = smoothPitch = Pitch;
	}

	// Update camera position to folloow behind the player
	void UpdateFollowPlayerCamera(Thing* player, float distance = 512.0f, float height = 128.0f)
	{
		if (!player || !FollowPlayer) return;

		//Get player position
		SLONG px = player->WorldPos.X;
		SLONG py = player->WorldPos.Y;
		SLONG pz = player->WorldPos.Z;

		// Calculate camera offset based on yaw and pitch
		const float FIXED_TO_RAD = (2.0f * 3.14159265358979323846f) / (2048.0f * 256.0f);
		float yaw_rad = Yaw * FIXED_TO_RAD;
		float pitch_rad = Pitch * FIXED_TO_RAD;

		float cos_pitch = cosf(pitch_rad);

		// Calculate offset vector (pointing away from where camera is looking)
		float offsetX = sinf(yaw_rad) * cos_pitch * distance;
		float offsetZ = cosf(yaw_rad) * cos_pitch * distance;
		float offsetY = -sinf(pitch_rad) * distance;

		// Postion camera behind player
		PositionX = px + static_cast<std::int64_t>(offsetX * 256.0f);
		PositionY = py + static_cast<std::int64_t>(height * 256.0f) + static_cast<std::int64_t>(offsetY * 256.0f);
		PositionZ = pz + static_cast<std::int64_t>(offsetZ * 256.0f);
	}
};
