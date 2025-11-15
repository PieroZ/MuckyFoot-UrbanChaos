// free_roam.camera.h
#pragma once
#include "fc.h"
#include	<cstdint>
#include	<cmath>

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

	static FreeRoamCamera& GetInstance()
	{
		static FreeRoamCamera instance;
		return instance;
	}

	FreeRoamCamera()
		: PositionX(0), PositionY(0), PositionZ(0),
		Pitch(0.0f), Yaw(0.0f), Roll(0.0f),
		targetYaw(0.0f), targetPitch(0.0f), smoothYaw(0.0f), smoothPitch(0.0f),
		FieldOfView(90.0f), IsActive(false)
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
};
