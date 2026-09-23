///////////////////////////////////////////////////////////////////////////
//
// Name:     LOCKSTEP.H
// Purpose:  Turn barrier + command buffers for Planitia listen-server lockstep.
//
///////////////////////////////////////////////////////////////////////////

#ifndef _PLANITIA_LOCKSTEP_H_
#define _PLANITIA_LOCKSTEP_H_

#include "NetProtocol.h"
#include "NetSession.h"
#include "Player.h"

#include <array>
#include <cstdint>
#include <climits>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class GameSim;

class LockstepController
{
public:
	void ResetOffline(int localSlot = 0);
	void ConfigureMatch(int numPlayers, int localSlot, uint16_t turnLength, uint16_t inputDelay = 2);

	void SubmitLocalAction(PlayerAction action, int cellX, int cellZ, uint16_t extra = 0);
	void OnNetworkPacket(Net::PacketType type, const uint8_t* data, size_t size, int peerIndex, NetSession& net);

	// Service barrier: when all cmds for current turn are in, apply + tick + checksum.
	void Update(NetSession& net, GameSim& sim);

	uint32_t CurrentTurn() const { return m_Turn; }
	int TurnLength() const { return m_TurnLength; }
	int NumPlayers() const { return m_NumPlayers; }
	int LocalSlot() const { return m_LocalSlot; }
	bool IsDesynced() const { return m_Desynced; }
	bool MatchRunning() const { return m_MatchRunning; }
	void SetMatchRunning(bool v) { m_MatchRunning = v; }
	void PauseMatch(const std::string& reason = "Paused");
	const std::string& Status() const { return m_Status; }

	uint32_t AssignedSlotForPeer(int peerIndex) const;
	void OnPeerHelloAsHost(int peerIndex, const std::string& name, NetSession& net);
	void OnPeerDisconnectedAsHost(int peerIndex);

	// Host: broadcast start; everyone should call BeginMatch afterward.
	void HostBroadcastStart(NetSession& net, uint32_t mapSeed);
	// simPlayers = villages/teams in GameSim; inputPlayers = how many slots must submit cmds
	// (offline: simPlayers=2, inputPlayers=1 so AI team doesn't block the barrier).
	void BeginMatch(GameSim& sim, uint32_t mapSeed, int simPlayers, int localSlot, uint16_t turnLength, int inputPlayers = -1, uint16_t inputDelay = 2);
	int InputDelay() const { return m_InputDelay; }

	int OccupiedRemoteSlots() const;
	std::string PeerNameForSlot(int slot) const;
	PlayerColorId ColorForSlot(int slot) const;

	// Client: set on Welcome so the lobby can print "You are Blue".
	bool TakePendingColorNotify(std::string& outColorName);

	// Host/client: version mismatch or other join rejection for lobby console.
	bool TakePendingNetError(std::string& outMessage);

private:
	void EnsureLocalCommandSubmitted(NetSession& net);
	void BroadcastOrSendCommand(const Net::PlayerCommand& cmd, NetSession& net);
	void StoreCommand(const Net::PlayerCommand& cmd);
	bool HaveCommand(int slot, uint32_t turn) const;
	Net::PlayerCommand GetCommand(int slot, uint32_t turn) const;
	void PruneOldCommands();
	bool AllCommandsReady() const;
	void AdvanceTurn(NetSession& net, GameSim& sim);
	uint32_t ComputeChecksum(const GameSim& sim) const;
	void ApplyTurnBundlePayload(uint32_t turn, uint8_t numP, const uint8_t*& p, const uint8_t* end);

	uint32_t m_Turn = 0;
	int m_TurnLength = 2; // sim ticks per barrier
	int m_InputDelay = 2; // schedule local input this many turns ahead
	int m_NumPlayers = 1;
	int m_LocalSlot = 0;
	bool m_MatchRunning = false;
	bool m_Desynced = false;
	bool m_LocalSubmitted = false; // current-turn cmd present + sent
	std::string m_Status = "Offline lockstep";

	// Current-turn view (filled before AdvanceTurn / from TurnBundle).
	std::array<std::optional<Net::PlayerCommand>, kMaxPlayers> m_Commands{};
	// Per-slot commands keyed by turn (supports input_delay pipelining).
	std::array<std::unordered_map<uint32_t, Net::PlayerCommand>, kMaxPlayers> m_CmdsByTurn{};
	std::array<int, kMaxPlayers> m_PeerToSlot{}; // host: peerIndex -> slot (slot0=host)
	std::array<std::string, kMaxPlayers> m_SlotNames{}; // display names by player slot
	std::array<PlayerColorId, kMaxPlayers> m_SlotColors{};
	int FindFreeJoinSlot() const;
	PlayerColorId PickRandomJoinerColor() const;
	void ApplyColorsToSim(GameSim& sim, int simPlayers) const;

	// Checksums are per executed turn - never compare across different turns.
	uint32_t m_LastChecksumTurn = UINT32_MAX;
	uint32_t m_LastLocalChecksum = 0;
	bool m_HasLocalChecksum = false;

	// Latest local intent; scheduled onto turn (m_Turn + m_InputDelay) when flushed.
	Net::PlayerCommand m_PendingAction{};
	bool m_HasPendingAction = false;

	// Client: StartMatch packet deferred until Update can touch GameSim.
	bool m_HasPendingStart = false;
	uint32_t m_PendingStartSeed = 0;
	int m_PendingStartPlayers = 2;

	bool m_PendingNetError = false;
	std::string m_PendingNetErrorMsg;
	uint16_t m_PendingStartTurnLen = 2;
	uint16_t m_PendingStartInputDelay = 2;

	// Commands that arrived before BeginMatch (same ENet service burst as StartMatch).
	std::vector<Net::PlayerCommand> m_EarlyCommands;

	// Client: host sent TurnBundle - apply on next Update.
	bool m_PendingTurnBundle = false;

	// Wall-clock pace: one turn represents turnLength x 33ms of sim time.
	double m_NextAdvanceEarliest = 0.0;
	double m_NextCmdResendTime = 0.0;
	double m_NextBundleResendTime = 0.0;

	// PerfCounter: wall-clock start of current barrier wait (-1 = not waiting).
	double m_NetworkWaitStart = -1.0;

	// Last TurnBundle bytes - retransmit while waiting for next-turn cmds (lost-packet recovery).
	std::vector<uint8_t> m_LastTurnBundle;
	uint32_t m_LastTurnBundleTurn = UINT32_MAX;

	// TurnBundle that arrived before MatchRunning (with StartMatch).
	bool m_HasEarlyTurnBundle = false;
	uint32_t m_EarlyBundleTurn = 0;
	uint8_t m_EarlyBundleNumP = 0;
	std::vector<uint8_t> m_EarlyBundlePayload;

	bool m_PendingColorNotify = false;
	std::string m_PendingColorName;
};

#endif
