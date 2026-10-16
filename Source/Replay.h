#ifndef _PLANITIA_REPLAY_H_
#define _PLANITIA_REPLAY_H_

#include "NetProtocol.h"
#include "Player.h"

#include <cstdint>
#include <string>
#include <vector>

// Lockstep replay: same seed + same per-turn commands => same match.
namespace Replay
{
	constexpr uint32_t kMagic = 0x504C5250u; // 'PLRP'
	constexpr uint16_t kFormatVersion = 1;
	// Special turn id written after the last real turn when the match ends.
	constexpr uint32_t kGameFinishedTurn = 0xFFFFFFFFu;
	constexpr const char* kReplaysDir = "Data/Replays";
	constexpr const char* kSavesDir = "Data/Saves";
	constexpr const char* kLastReplayName = "last.plr";

	struct Header
	{
		uint16_t formatVersion = kFormatVersion;
		uint16_t gameVersionPatch = 0;
		uint32_t mapSeed = 0;
		uint8_t simPlayers = 2;
		uint8_t inputPlayers = 1;
		uint8_t localSlot = 0;
		uint16_t turnLength = 2;
		uint16_t inputDelay = 2;
		uint8_t colors[kMaxPlayers] = { 0, 1, 2, 3 };
	};

	struct TurnRecord
	{
		uint32_t turn = 0;
		std::vector<Net::PlayerCommand> cmds; // size == inputPlayers
	};

	struct Document
	{
		Header header{};
		std::vector<TurnRecord> turns;
		bool hasGameFinished = false;
		int winnerSlot = -1; // from GameFinished trailer; -1 = draw / unknown
	};

	// Ensure Data/Replays and Data/Saves exist under the working directory.
	void EnsureDataDirs();

	// --- Recording (append-friendly) ---
	class Writer
	{
	public:
		~Writer() { Stop(); }

		bool Start(const Header& header);
		bool IsOpen() const { return m_File != nullptr; }
		bool AppendTurn(uint32_t turn, const Net::PlayerCommand* cmds, int numCmds);
		// Trailer so playback knows to keep simulating until MatchOver.
		bool AppendGameFinished(int winnerSlot);
		void Flush();
		void Stop(); // flush, close, refresh last.plr

		const std::string& Path() const { return m_Path; }

	private:
		FILE* m_File = nullptr;
		std::string m_Path;
		Header m_Header{};
	};

	// --- Loading / discovery ---
	bool LoadFile(const std::string& path, Document& out, std::string& error);
	// Newest by modification time among Data/Replays/*.plr (prefers last.plr if present).
	bool FindNewestReplayPath(std::string& outPath);
	std::string LastReplayPath();
	std::string MakeTimestampedReplayPath(uint32_t mapSeed);
}

#endif
