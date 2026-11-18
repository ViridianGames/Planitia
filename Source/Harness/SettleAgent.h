#ifndef _PLANITIA_SETTLE_AGENT_H_
#define _PLANITIA_SETTLE_AGENT_H_

#include "GameTypes.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class GameSim;

struct SeatCommand
{
	bool pass = true;
	PlayerAction action = PlayerAction::Flatten;
	int x = 0;
	int z = 0;
	uint16_t extra = 0;
};

struct SeatVillageInfo
{
	int x = 0;
	int z = 0;
	int size = 0;
	int food = 0;
	int villagers = 0;
};

// What one seat can see, and what the built-in settler would do with it.
struct SeatInspect
{
	SeatCommand suggestion;
	std::vector<SeatVillageInfo> villages;
	std::vector<std::pair<int, int>> flatten;
	std::vector<std::pair<int, int>> bless;
	int population = 0;
	int food = 0;
	int blessed = 0;
	int prepared = 0;
	int villageCount = 0;
	float mana = 0.0f;
	float cooldown = 0.0f;
	bool eliminated = false;
};

SeatInspect InspectSeat(const GameSim& sim, int slot);

const char* SeatActionWireName(PlayerAction action);
bool SeatActionFromWireName(const std::string& name, PlayerAction& out);

// A deliberate pass parses as success. Malformed or illegal input returns false and sets a pass.
bool ParseSeatCommandLine(const std::string& line, SeatCommand& out, std::string& error);

#endif
