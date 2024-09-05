
#pragma once
#include "Anim.h"

class AllExporter
{
public:
	AllExporter();
	static void DoStuff(Anim* key_list, GameKeyFrameChunk* p_chunk, KeyFrameChunk* the_chunk);
	static void KeyFrameListStuff(GameKeyFrameChunk* game_chunk, struct	KeyFrameChunk* the_chunk, Anim*& OutputAnimList, int& animCount);
};