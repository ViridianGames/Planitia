#ifndef _PLANITIA_WALKER_SPRITES_H_
#define _PLANITIA_WALKER_SPRITES_H_

#include "raylib.h"

struct Unit;

// 8-direction × N-frame walk sheets for villagers (U7 WalkSheet layout).
// Color: Images/VillagerWalkFixed.png
// Team shirt overlay: Images/VillagerWalkMask.png (white+alpha matte, tinted at draw)
namespace WalkerSprites
{
	bool EnsureLoaded();
	void Unload();
	bool IsReady();

	// Camera-relative 8-way billboard + team-colored shirt mask.
	void DrawWalker(const Unit& unit, const Camera3D& camera, Color teamColor);
}

#endif
