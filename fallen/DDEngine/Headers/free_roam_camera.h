#pragma once
#include "fc.h"
#include	<cstdint>

struct FreeRoamCamera
{
	std::int64_t PositionX;
	std::int64_t PositionY;
	std::int64_t PositionZ;
	std::int64_t Pitch;
	std::int64_t Yaw;
	std::int64_t Roll;
	float FieldOfView;
	bool IsActive;

	static FreeRoamCamera& GetInstance()
	{
		static FreeRoamCamera instance;
		return instance;
	}

	FreeRoamCamera()
		: PositionX(0.0f), PositionY(0.0f), PositionZ(0.0f),
		  Pitch(0.0f), Yaw(0.0f), Roll(0.0f),
		  FieldOfView(90.0f), IsActive(false)
	{
	}

	void CopyFromFC(FC_Cam* fc)
	{
		PositionX = fc->x;
		PositionY = fc->y;
		PositionZ = fc->z;
		Pitch = fc->pitch;
		Yaw = fc->yaw;
		Roll = fc->roll;
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
};