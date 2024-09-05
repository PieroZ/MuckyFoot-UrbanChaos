// What is this error
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include "MFStdLib.h"
#include "memory.h"
#include "../Headers/AllToAnmConverter.h"


AllExporter::AllExporter()
{
}


std::map<int, std::string> generate_anim_names_map()
{
	std::map<int, std::string> anim_map = {
		{100, "CIV_M_START"},
		{102, "CIV_M_ANIM_WALK"},
		{103, "CIV_M_ANIM_RUN"},
		{106, "CIV_M_ANIM_STAND"},
		{105, "CIV_M_ANIM_IDLE_START"},
		{106, "CIV_M_ANIM_IDLE_LOOP"},
		{107, "CIV_M_ANIM_IDLE_WATCH"},
		{108, "CIV_M_ANIM_IDLE_END"},
		{109, "CIV_M_ANIM_SIT_START"},
		{110, "CIV_M_ANIM_SIT_LOOP"},
		{111, "CIV_M_ANIM_SIT_WAIT"},
		{112, "CIV_M_ANIM_SIT_END"},
		{113, "CIV_M_ANIM_SIT_TALK1"},
		{114, "CIV_M_ANIM_SIT_TALK2"},
		{115, "CIV_M_ANIM_SIT_TALK3"},
		{116, "CIV_M_ANIM_HANDS_UP"},
		{117, "CIV_M_ANIM_HANDS_UP_LOOP"},
		{118, "CIV_M_ANIM_HANDS_UP_LIE"},
		{119, "CIV_M_ANIM_TALK1"},
		{120, "CIV_M_ANIM_TALK2"},
		{121, "CIV_M_ANIM_TALK3"},
		{122, "CIV_F_START"},
		{124, "CIV_F_ANIM_WALK"},
		{125, "CIV_F_ANIM_IDLE_BREATHE"},
		{126, "CIV_F_ANIM_IDLE_START"},
		{127, "CIV_F_ANIM_IDLE_LOOP"},
		{128, "CIV_F_ANIM_IDLE_LOOP2"},
		{129, "CIV_F_ANIM_IDLE_STOP"},
		{130, "CIV_F_ANIM_SIT_IDLE1"},
		{131, "CIV_F_ANIM_SIT_IDLE2"},

		// ROPER defines
		{1, "ROPER_WALK"},
		{2, "ROPER_DRAW_GUN"},
		{3, "ROPER_SHOOT"},
		{4, "ROPER_RUN"},
		{5, "ROPER_LAND_ON_FENCE"},
		{6, "ROPER_CLIMB_OVER_FENCE"},
		{7, "ROPER_CLIMB_UP_FENCE"},
		{9, "ROPER_PULL_UP"},
		{10, "ROPER_WALK_MEAN"},
		{11, "ROPER_SWING"},
		{12, "ROPER_ENTER_VAN"},
		{13, "ROPER_EXIT_VAN"},
		{15, "ROPER_STAND"},
		{16, "ROPER_DANGLE"},
		{22, "ROPER_ANIM_JUMP"},
		{23, "ROPER_ANIM_LAND"},
		{24, "ROPER_ANIM_MID_AIR"},
		{30, "ROPER_PUNCH_CLOSE2"},
		{31, "ROPER_PUNCH_CLOSE1"},
		{32, "ROPER_PUNCH_CLOSE3"},
		{47, "ROPER_STANDING_JUMP"},
		{48, "ROPER_FALLING"},
		{49, "ROPER_FIGHT_IDLE"},
		{50, "ROPER_FIGHT_RECOIL"},
		{51, "ROPER_FLY_GRABBING_LEDGE"},
		{52, "ROPER_LAND_VERT"},
		{53, "ROPER_CROUCH_DOWN"},
		{54, "ROPER_IDLE_CROUCH"},
		{56, "ROPER_MOUNT_LADDER"},
		{57, "ROPER_ON_LADDER"},
		{58, "ROPER_OFF_LADDER_TOP"},
		{59, "ROPER_DEATH_SLIDE"},
		{60, "ROPER_TRAVERSE_LEFT"},
		{61, "ROPER_TRAVERSE_RIGHT"},
		{62, "ROPER_FOLDARMS"},
		{64, "ROPER_ARMSFOLDED"},
		{65, "ROPER_ROTATE_LEFT"},
		{66, "ROPER_ROTATE_RIGHT"},
		{68, "ROPER_AIM_GUN"},
		{69, "ROPER_BIKE_RIDE"},
		{70, "ROPER_BIKE_LEAN_LEFT"},
		{71, "ROPER_BIKE_LEAN_RIGHT"},
		{72, "ROPER_TO_FIGHT"},
		{73, "ROPER_FIGHT_FORWARD"},
		{74, "ROPER_FIGHT_BACK"},
		{75, "ROPER_FIGHT_LEFT"},
		{76, "ROPER_FIGHT_RIGHT"},

		// THUG defines
		{2, "THUG_ANIM_WALK"},
		{5, "THUG_ANIM_RUN"},
		{8, "THUG_ANIM_IDLE1"},
		{9, "THUG_ANIM_IDLE2"},
		{41, "THUG_ANIM_FIGHT_IDLE"},

		// CIV defines
		{2, "CIV_ANIM_WALK"},
		{13, "CIV_ANIM_WANKER"},
		{27, "CIV_ANIM_ENTER_TAXI"},
		{28, "CIV_ANIM_SIT"},
		{29, "CIV_ANIM_READ_PAPER1"},
		{30, "CIV_ANIM_READ_PAPER2"},
		{31, "CIV_ANIM_IDLE_PHONE_ANSWER"},
		{32, "CIV_ANIM_IDLE_PHONE_TALK"},
		{45, "CIV_BLOCK"},
		{46, "CIV_FIGHT_IDLE"},
		{52, "CIV_ANIM_DANCE_BOOGIE"},
		{53, "CIV_ANIM_WARM_HANDS"},
		{55, "CIV_ANIM_DANCE_HEADBANG"},
		{56, "CIV_ANIM_HANDS_UP"},
		{57, "CIV_ANIM_HANDS_UP_LOOP"},
		{58, "CIV_ANIM_DANCE_WOOGIE"},

		// Generic ANIM defines
		{2, "ANIM_STAND_HIP"},
		{3, "ANIM_STAND_READY"},
		{8, "ANIM_KICK_ROUND1"},
		{12, "ANIM_JUMP_UP_GRAB"},
		{13, "ANIM_MOUNT_LADDER"},
		{14, "ANIM_UP_LADDER"},
		{16, "ANIM_VAULT"},
		{18, "ANIM_SIDLE"},
		{19, "ANIM_PUNCH1"},
		{22, "ANIM_HEAD_HIT_BIG"},
		{24, "ANIM_GUT_HIT_BIG"},
		{26, "ANIM_HEAD_HIT_SMALL"},
		{27, "ANIM_GUT_HIT_SMALL"},
		{28, "ANIM_HEAD_HIT_BEHIND"},
		{33, "ANIM_KO_BACK"},
		{34, "ANIM_KO_BACK_GU"},
		{35, "ANIM_PUNCH2"},
		{36, "ANIM_KICK2"},
		{37, "ANIM_HAND_OVER_HAND"},
		{39, "ANIM_PUNCH3"},
		{40, "ANIM_GUT_HIT_BIG_BEHIND"},
		{43, "ANIM_CLIMB_RIGHT_FENCE"},
		{45, "ANIM_CLIMB_UP_FENCE"},
		{46, "ANIM_RUN_JUMP_RIGHT"},
		{47, "ANIM_FALL_LONG"},
		{48, "ANIM_PULL_UP"},
		{51, "ANIM_FALLING"},
		{52, "ANIM_FLY_GRABBING_LEDGE"},
		{53, "ANIM_LAND_RIGHT"},
		{54, "ANIM_RUN_JUMP_LEFT"},
		{55, "ANIM_OFF_LADDER_TOP"},
		{56, "ANIM_OFF_LADDER_BOT"},
		{57, "ANIM_LAND_ON_FENCE"},
		{58, "ANIM_CLIMB_OVER_FENCE"},
		{59, "ANIM_STEP_LEFT"},
		{60, "ANIM_STEP_RIGHT"},
		{62, "ANIM_LANDED_ON_FENCE"},
		{64, "ANIM_CAN_PICKUP"},
		{65, "ANIM_CAN_RELEASE"},
		{68, "ANIM_DEATH_SLIDE"},
		{69, "ANIM_BSTEP"},
		{77, "ANIM_IDLE_SCRATCH1"},
		{78, "ANIM_IDLE_SCRATCH2"},
		{80, "ANIM_FATAL_FALL"},
		{81, "ANIM_SHOTGUN_DRAW"},
		{82, "ANIM_SHOTGUN_IDLE_OLD"},
		{83, "ANIM_SHOTGUN_SHOOT"},
		{84, "ANIN_SHOTGUN_WHIP"},
		{85, "ANIM_SHOTGUN_WALK"},
		{86, "ANIM_SNEAK"},
		{87, "ANIM_LOB2"},
		{88, "ANIM_NECK_SNAP"},
		{89, "ANIM_ENTER_CAR"},
		{90, "ANIM_EXIT_CAR"},
		{91, "ANIM_FLIP_LEFT"},
		{92, "ANIM_FLIP_RIGHT"},
		{93, "ANIM_FLIP_RIGHT_CONT"},
		{94, "ANIM_FLIP_LEFT_CONT"},
		{96, "ANIM_LAND_STAND"},
		{99, "ANIM_HOSTAGE_STAND"},
		{100, "ANIM_HOSTAGE_SIT"},
		{101, "ANIM_HOSTAGE_DESPAIR"},
		{102, "ANIM_HOSTAGE_READY"},
		{103, "ANIM_HOSTAGE_RELEASE"},
		{104, "ANIM_KICK3"},
		{105, "ANIM_BACK_FLIP"},
		{109, "ANIM_KO_BEHIND_BIG"},
		{110, "ANIM_KO_BEHIND_BIG_GU"},
		{112, "ANIM_FORWARD_JUMP"},
		{117, "ANIM_WALL_KICK"},
		{118, "ANIM_RUN_JUMP_MLEFT"},
		{121, "ANIM_BACK_HOP"},
		{122, "ANIM_MID_AIR_TWEEN_LEFT"},
		{124, "ANIM_BREATHE"},
		{140, "ANIM_GRAPPLING_HOOK_PICKUP"},
		{137, "ANIM_GRAPPLING_HOOK_WINDUP"},
		{138, "ANIM_GRAPPLING_HOOK_RELEASE"},
		{203, "ANIM_STANDING_JUMP"},
		{204, "ANIM_LAND_VERT"},
		{206, "ANIM_FALLING_QUEUED"},
		{268, "ANIM_MID_AIR_TWEEN_LEFT_AK"},
		{269, "ANIM_RUN_JUMP_LEFT_AK"},
		{270, "ANIM_LAND_RIGHT_AK"},
		{380, "ANIM_STANDING_JUMP_AK"},
		{381, "ANIM_LAND_VERT_AK"},
		{382, "ANIM_FALLING_QUEUED_AK"},
		{390, "ANIM_FALLING_AK"}
	};

	for (const auto& anim : anim_map)
	{
		TRACE("Animation ID: %d, Animation Name: %s\n", anim.first, anim.second.c_str());
		//std::cout << "Animation ID: " << anim.first << ", Animation Name: " << anim.second << std::endl;
	}
	

	return anim_map;
}

#define	CMAT0_MASK	(0x3ff00000)
#define	CMAT1_MASK	(0x000ffc00)
#define	CMAT2_MASK	(0x000003ff)

void	uncompress_matrix2(CMatrix33* cm, Matrix33* m)
{
	SLONG v;

	v = ((cm->M[0] & CMAT0_MASK) << 2) >> 22;
	m->M[0][0] = (v << 6);

	v = ((cm->M[0] & CMAT1_MASK) << 12) >> 22;
	m->M[0][1] = (v << 6);

	v = ((cm->M[0] & CMAT2_MASK) << 22) >> 22;
	m->M[0][2] = (v << 6);

	v = ((cm->M[1] & CMAT0_MASK) << 2) >> 22;
	m->M[1][0] = (v << 6);

	v = ((cm->M[1] & CMAT1_MASK) << 12) >> 22;
	m->M[1][1] = (v << 6);

	v = ((cm->M[1] & CMAT2_MASK) << 22) >> 22;
	m->M[1][2] = (v << 6);

	v = ((cm->M[2] & CMAT0_MASK) << 2) >> 22;
	m->M[2][0] = (v << 6);

	v = ((cm->M[2] & CMAT1_MASK) << 12) >> 22;
	m->M[2][1] = (v << 6);

	v = ((cm->M[2] & CMAT2_MASK) << 22) >> 22;
	m->M[2][2] = (v << 6);
}


void AllExporter::DoStuff(Anim* key_list, GameKeyFrameChunk* p_chunk, KeyFrameChunk* the_chunk)
{
	std::vector<Anim*> AnimList;
	std::map<int, int> framesCountPerAnimNo;
	signed short animCount = p_chunk->MaxAnimFrames; // -1 ?
	signed short framesCount = p_chunk->MaxKeyFrames; // -1 ?
	signed long elementsCount = p_chunk->MaxElements; // bodyPart * framesCount = 15 * 10 ? 

	extern	void	convert_keyframe_to_pointer(GameKeyFrame * p, GameKeyFrameElement * p_ele, GameFightCol * p_fight, SLONG count);
	extern	void	convert_animlist_to_pointer(GameKeyFrame * *p, GameKeyFrame * p_anim, SLONG count);
	extern	void	convert_fightcol_to_pointer(GameFightCol * p, GameFightCol * p_fight, SLONG count);
	
	convert_keyframe_to_pointer(p_chunk->AnimKeyFrames, p_chunk->TheElements, p_chunk->FightCols, p_chunk->MaxKeyFrames);
	convert_animlist_to_pointer(p_chunk->AnimList, p_chunk->AnimKeyFrames, p_chunk->MaxAnimFrames);
	convert_fightcol_to_pointer(p_chunk->FightCols, p_chunk->FightCols, p_chunk->MaxFightCols);

	// Iterate over all elements in .all
	for (int i = 0; i < p_chunk->MaxElements; ++i)
	{
		GameKeyFrameElement animList = p_chunk->TheElements[i];
		Matrix33 uncompressedMatrix;
		uncompress_matrix2(&animList.CMatrix, &uncompressedMatrix);
		//TRACE("YAY");

	}

	//for (int i = 0; i < animCount; ++i)
	//{
	//	Anim* a = new Anim();
	//	a->SetFrameCount(0);
	//	a->SetAnimFlags(0);
	//	a->SetAnimName("Dupa");
	//	a->SetNextAnim(nullptr);
	//	a->SetLastAnim(nullptr);
	//	AnimList.push_back(a);
	//}

	//for (int i = 1; i < animCount; ++i)
	//{
	//	Anim* aPointer = AnimList[i];

	//	//TRACE("Frames count in anim %d = %d\n", i, p_chunk->AnimList[i]);
	//	TRACE("First frame id in anim %d = %d\n", i, p_chunk->AnimList[i]);
	//	if (i != animCount - 1)
	//	{
	//		framesCountPerAnimNo[i] = (int)p_chunk->AnimList[i + 1] - (int)p_chunk->AnimList[i];
	//		TRACE("Frames count in anim %d = %d \n", i, framesCountPerAnimNo[i]);
	//	}
	//	else
	//	{

	//		framesCountPerAnimNo[i] = (int)framesCount - (int)p_chunk->AnimList[i];
	//		TRACE("Frames count in anim %d = %d \n", i, framesCountPerAnimNo[i]);
	//	}
	//}

	//int frameOffset = 1;
	//for (signed short i = 1; i < animCount; ++i)
	//{
	//	int framesInAnimNo = framesCountPerAnimNo[i];
	//	for (int j = frameOffset; j < framesInAnimNo+ frameOffset; ++j)
	//	{
	//		int prevFrameId = (int)p_chunk->AnimKeyFrames[j].PrevFrame;
	//		int nextFrameId = (int)p_chunk->AnimKeyFrames[j].NextFrame;
	//		TRACE("%d -> x -> %d \n", prevFrameId, nextFrameId);
	//	}
	//	frameOffset += framesInAnimNo;
	//}


	//for (int i = 0; i < elementsCount; ++i)
	//{
	//	TRACE("i = %d, %ld, %ld, %ld \n", i, p_chunk->TheElements[i].CMatrix.M[0], p_chunk->TheElements[i].CMatrix.M[1], p_chunk->TheElements[i].CMatrix.M[2]);
	//		//p_chunk->TheElements[0].CMatrix.M[0]
	//}
}

#define	CMAT0_MASK	(0x3ff00000)
#define	CMAT1_MASK	(0x000ffc00)
#define	CMAT2_MASK	(0x000003ff)

void	uncompress_matrix3(CMatrix33* cm, Matrix33* m)
{
	SLONG v;

	v = ((cm->M[0] & CMAT0_MASK) << 2) >> 22;
	m->M[0][0] = (v << 6);

	v = ((cm->M[0] & CMAT1_MASK) << 12) >> 22;
	m->M[0][1] = (v << 6);

	v = ((cm->M[0] & CMAT2_MASK) << 22) >> 22;
	m->M[0][2] = (v << 6);

	v = ((cm->M[1] & CMAT0_MASK) << 2) >> 22;
	m->M[1][0] = (v << 6);

	v = ((cm->M[1] & CMAT1_MASK) << 12) >> 22;
	m->M[1][1] = (v << 6);

	v = ((cm->M[1] & CMAT2_MASK) << 22) >> 22;
	m->M[1][2] = (v << 6);

	v = ((cm->M[2] & CMAT0_MASK) << 2) >> 22;
	m->M[2][0] = (v << 6);

	v = ((cm->M[2] & CMAT1_MASK) << 12) >> 22;
	m->M[2][1] = (v << 6);

	v = ((cm->M[2] & CMAT2_MASK) << 22) >> 22;
	m->M[2][2] = (v << 6);
}
void AllExporter::KeyFrameListStuff(GameKeyFrameChunk* game_chunk, struct	KeyFrameChunk* the_chunk, Anim*& OutputAnimList, int& animCount)
{

	GameKeyFrameElement gameChunkElement2 = game_chunk[0].TheElements[0];
	struct KeyFrame* the_key_frame;
	Matrix33 uncompressedMatrix;
	uncompress_matrix3(&gameChunkElement2.CMatrix, &uncompressedMatrix);

	//KeyFrame* keyFrames = new KeyFrame[];

	//KeyFrameElement* firstElement = new KeyFrameElement[game_chunk[0].MaxElements];

	KeyFrameElement* firstElement = (KeyFrameElement*)MemAlloc(game_chunk[0].MaxElements * sizeof(KeyFrameElement));

	std::map<GameKeyFrameElement*, int> gameKeyFrameElementToId;
	std::map<GameKeyFrameElement*, KeyFrameElement*> gameKeyElementToKeyFrameElement;

	generate_anim_names_map();

	for (int i = 0; i < game_chunk[0].MaxKeyFrames; ++i)
	{
		TRACE("i=%d, TweenStep=%d\n", i, game_chunk[0].AnimKeyFrames[i].TweenStep);
	}

	for (int i = 0; i < game_chunk[0].MaxElements; ++i)
	{
		GameKeyFrameElement gameChunkElement = game_chunk[0].TheElements[i];
		//Matrix33 uncompressedMatrix;
		//uncompress_matrix3(&gameChunkElement.CMatrix, &uncompressedMatrix);

		//uncompressedMatrix.M[0][0] = 32767;
		//uncompressedMatrix.M[0][1] = 0;
		//uncompressedMatrix.M[0][2] = 0;
		//uncompressedMatrix.M[1][0] = 0;
		//uncompressedMatrix.M[1][1] = 32767;
		//uncompressedMatrix.M[1][0] = 0;
		//uncompressedMatrix.M[2][0] = 0;
		//uncompressedMatrix.M[2][0] = 0;
		//uncompressedMatrix.M[2][2] = 32767;

		//for (int dx = 0; dx < 3; dx++)
		//{
		//	for (int dy = 0; dy < 3; dy++)
		//	{
		//		SATURATE(uncompressedMatrix.M[dx][dy], -32767, 32767);
		//	}
		//}
		//TRACE("YAY");

		firstElement[i].CMatrix = gameChunkElement.CMatrix;
		firstElement[i].OffsetX = gameChunkElement.OffsetX;
		firstElement[i].OffsetY = gameChunkElement.OffsetY;
		firstElement[i].OffsetZ = gameChunkElement.OffsetZ;
		firstElement[i].Matrix = uncompressedMatrix;
		firstElement[i].Parent = 0;
		firstElement[i].Next = 0;
		// Is he alive?
		// I'm not sure
		gameKeyFrameElementToId[&game_chunk[0].TheElements[i]] = i;
		gameKeyElementToKeyFrameElement[&game_chunk[0].TheElements[i]] = &firstElement[i];
	}
	//for (int frame = 0; frame < game_chunk[0].MaxKeyFrames; ++frame)
	for (int frame = 0; frame < game_chunk[0].MaxElements / 15; ++frame)
	{

		the_key_frame = &the_chunk->KeyFrames[frame];
		the_key_frame->ChunkID = 0;
		the_key_frame->FrameID = frame;
		the_key_frame->TweenStep = 4;
		the_key_frame->ElementCount = the_chunk->ElementCount;
		the_key_frame->FirstElement = &firstElement[frame * 15];
	}

	the_chunk->KeyFrameCount = game_chunk[0].MaxElements / 15;
	the_chunk->FirstElement = firstElement;
	the_chunk->LastElement = firstElement + game_chunk[0].MaxElements - 1;
	//Anim* animList = new Anim[game_chunk[0].MaxAnimFrames];
	Anim* animList = (Anim*)MemAlloc(game_chunk[0].MaxAnimFrames * sizeof(Anim));

	//KeyFrameElement* firstElement = (KeyFrameElement*)MemAlloc(game_chunk[0].MaxElements * sizeof(KeyFrameElement));

	int usedKeyFramesCount = 0;

	animList[0].SetAnimName("Anim 0");
	animList[0].SetNextAnim(&animList[1]);
	for (int i = 1; i < game_chunk[0].MaxAnimFrames; ++i)
	{
		std::vector< GameKeyFrame*> v_p;
		try
		{
			int frameCountPerAnim = 1;

			GameKeyFrame* po = game_chunk[0].AnimList[i];
			if ((int)po < 0)
			{
				// XD
				animList[i].SetAnimName("Emtpiness");
				if (i < game_chunk[0].MaxAnimFrames - 1)
				{
					animList[i].SetNextAnim(&animList[i + 1]);
				}
				else
				{
					animList[i].SetNextAnim(nullptr);

				}
				animList[i].SetLastAnim(nullptr);

				continue;
			}
			GameKeyFrameElement* gkfe = po->FirstElement;
			v_p.push_back(po);
			GameKeyFrame* next = po->NextFrame;


			GameKeyFrame* previous = po->PrevFrame;
			std::string animName = "Anim " + std::to_string(i);
			char* cstr = animName.data();
			animList[i].SetAnimName(cstr);

			//KeyFrame* kf = new KeyFrame;
			KeyFrame* kf = (KeyFrame*)MemAlloc(sizeof(KeyFrame));
			KeyFrame* currentKf = kf;

			kf->ChunkID = 0;
			kf->Flags = 0;
			
			kf->FrameID = gameKeyFrameElementToId[gkfe] / 15;
			
			kf->TweenStep = 4;
			kf->ElementCount = 15;
			kf->Dx = 0;
			kf->Dy = 0;
			kf->Dz = 0;
			kf->Fixed = true;
			kf->Fight = nullptr;
			kf->FirstElement = gameKeyElementToKeyFrameElement[gkfe];
			kf->NextFrame = kf;
			kf->PrevFrame = kf;

			if (!next)
			{
				// There's nothing else
				// so set ANIM_FLAG_LAST_FRAME to the last frame
				currentKf->Flags |= ANIM_FLAG_LAST_FRAME;
			}

			animList[i].SetFrameList(kf);
			animList[i].SetFrameListEnd(kf);
			animList[i].SetTweakSpeed(255);

			if (i < game_chunk[0].MaxAnimFrames - 1)
			{
				animList[i].SetNextAnim(&animList[i + 1]);
			}
			else
			{
				animList[i].SetNextAnim(nullptr);
			}
			if (i == 1)
			{
				animList[i].SetLastAnim(nullptr);
			}
			else
			{
				animList[i].SetLastAnim(&animList[i - 1]);
			}


			++usedKeyFramesCount;

			//animList[i].SetNextAnim(next);


			while (next)
			{
				if (std::find(v_p.begin(), v_p.end(), next) != v_p.end())
				{
					TRACE("i = %d \n", i);
					long frameCount = v_p.size();
					for (auto&& a : v_p)
					{
						TRACE("%d-> \n", a);

					}
					animList[i].SetFrameCount(frameCountPerAnim);
					animList[i].SetAnimFlags(0);
					animList[i].SetTweakSpeed(255);

					kf->PrevFrame = currentKf;
					currentKf->NextFrame = kf;
					currentKf->Flags |= ANIM_FLAG_LAST_FRAME;


					break;
					/* v contains x */
				}
				else
				{

					v_p.push_back(next);




					if (next && next != po)
					{
						//KeyFrame* next_kf = new KeyFrame;
						KeyFrame* next_kf = (KeyFrame*)MemAlloc(sizeof(KeyFrame));
						GameKeyFrameElement* next_gkfe = next->FirstElement;

						next_kf->ChunkID = 0;
						next_kf->Flags = 0;

						next_kf->FrameID = gameKeyFrameElementToId[next_gkfe]/15;

						next_kf->TweenStep = 4;
						next_kf->ElementCount = 15;
						next_kf->Dx = 0;
						next_kf->Dy = 0;
						next_kf->Dz = 0;
						next_kf->Fixed = true;
						next_kf->Fight = nullptr;
						next_kf->FirstElement = gameKeyElementToKeyFrameElement[next_gkfe];

						next_kf->PrevFrame = kf;
						next_kf->NextFrame = nullptr;
						//if (animList[i].GetFrameListStart() != next_kf)
						{
							animList[i].SetFrameListEnd(next_kf);
						}

						currentKf->NextFrame = next_kf;
						currentKf = next_kf;
					}

					++usedKeyFramesCount;
					++frameCountPerAnim;

					if (!next)
					{
						// I'm never here :)
						animList[i].SetFrameCount(frameCountPerAnim);
						animList[i].SetAnimFlags(0);
						animList[i].SetTweakSpeed(255);

						currentKf->Flags |= ANIM_FLAG_LAST_FRAME;

						break;
					}

					animList[i].SetFrameCount(frameCountPerAnim);
					next = next->NextFrame;

					if (!next)
					{
						// There's nothing else
						// so set ANIM_FLAG_LAST_FRAME to the last frame
						currentKf->Flags |= ANIM_FLAG_LAST_FRAME;
					}
					/* v does not contain x */
				}
			}
		}
		catch (...)
		{
			TRACE("i=%d is fked\n", i);
		}
	}
	TRACE("usedKeyFramesCount=%d\n", usedKeyFramesCount);
	OutputAnimList = animList;

	animCount = game_chunk[0].MaxAnimFrames;

}
