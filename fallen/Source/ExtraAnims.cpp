#include "ExtraAnims.h"
#include <string>
#include "animate.h"
//#include "Person.h"

#include "Anim.h"
#include <map>

void setup_additional_anims(GameKeyFrame* global_anim_array[4][450], struct GameKeyFrameChunk* game_chunk)
{

	//int beta_animations_arr[] = { ANIM_BIKE_MOUNT, ANIM_BIKE_RIDE, ANIM_BIKE_LEAN_LEFT_FOOT,ANIM_BIKE_LEAN_RIGHT_FOOT, ANIM_BIKE_LEAN_LEFT, ANIM_BIKE_LEAN_RIGHT, ANIM_BIKE_LEAN, ANIM_BATMAN, ANIM_GRAPPLING_HOOK_WINDUP, ANIM_GRAPPLING_HOOK_RELEASE, ANIM_GRAPPLING_HOOK_PICKUP };

	//if (game_chunk[ANIM_TYPE_PZI].ElementCount != 0)
	//{
	//	for (int animId : beta_animations_arr)
	//	{
	//		global_anim_array[0][animId] = game_chunk[ANIM_TYPE_PZI].AnimList[animId];
	//		global_anim_array[1][animId] = game_chunk[ANIM_TYPE_PZI].AnimList[animId];
	//		global_anim_array[2][animId] = game_chunk[ANIM_TYPE_PZI].AnimList[animId];
	//		global_anim_array[3][animId] = game_chunk[ANIM_TYPE_PZI].AnimList[animId];
	//	}
	//}
	using namespace std;
	std::map<int, int> dance_move_to_anim_id;
	// ANIM_DANCE_BOOGIE
	// ANIM_DANCE_WOOGIE
	// ANIM_DANCE_HEADBANG

	dance_move_to_anim_id[ANIM_BATMAN] = 1;
	dance_move_to_anim_id[ANIM_DANCE_BOOGIE] = 3;
	dance_move_to_anim_id[ANIM_DANCE_WOOGIE] = 6;
	dance_move_to_anim_id[ANIM_DANCE_HEADBANG] = 5;

	dance_move_to_anim_id[ANIM_T1] = 2;
	dance_move_to_anim_id[ANIM_T2] = 4;
	dance_move_to_anim_id[ANIM_T3] = 7;
	dance_move_to_anim_id[ANIM_T4] = 8;
	dance_move_to_anim_id[ANIM_T5] = 9;

	// Get an iterator pointing to the first element in the
  // map
	std::map<int, int>::iterator it = dance_move_to_anim_id.begin();
	while (it != dance_move_to_anim_id.end())
	{
		global_anim_array[0][it->first] = game_chunk[4].AnimList[it->second]; // ANIM_TYPE_PZI
		global_anim_array[1][it->first] = game_chunk[4].AnimList[it->second];
		global_anim_array[2][it->first] = game_chunk[4].AnimList[it->second];
		global_anim_array[3][it->first] = game_chunk[4].AnimList[it->second];
		++it;
	}

	//int beta_animations_arr[] = { ANIM_BATMAN };
	//
	//if (game_chunk[ANIM_TYPE_PZI].ElementCount != 0)
	//{
	//	for (int animId : beta_animations_arr)
	//	{
	//		global_anim_array[0][animId] = game_chunk[ANIM_TYPE_PZI].AnimList[1];
	//		global_anim_array[1][animId] = game_chunk[ANIM_TYPE_PZI].AnimList[1];
	//		global_anim_array[2][animId] = game_chunk[ANIM_TYPE_PZI].AnimList[1];
	//		global_anim_array[3][animId] = game_chunk[ANIM_TYPE_PZI].AnimList[1];
	//	}
	//}
}