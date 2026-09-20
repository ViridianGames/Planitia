#include "Lockstep.h"

#include "GameSim.h"
#include "GameTypes.h"
#include "Geist/Logging.h"
#include "Geist/RNG.h"
#include "Terrain.h"
#include "Unit.h"

#include "raylib.h"

#include <memory>

// Declared in GameGlobals; avoid including GameGlobals.h (circular).
extern std::unique_ptr<Terrain> g_Terrain;
extern std::unique_ptr<RNG> g_vitalRNG;

#include <algorithm>
#include <cstring>

void LockstepController::ResetOffline(int localSlot)
{
	m_Turn = 0;
	m_TurnLength = 1;
	m_NumPlayers = 1;
	m_LocalSlot = localSlot;
	m_MatchRunning = false;
	m_Desynced = false;
	m_LocalSubmitted = false;
	m_Status = "Offline lockstep";
	m_Commands.fill(std::nullopt);
	m_PeerToSlot.fill(-1);
	m_SlotNames.fill({});
	m_SlotNames[0] = "Host";
	m_SlotColors.fill(PlayerColorId::Green);
	m_SlotColors[0] = PlayerColorId::Green;
	m_EarlyCommands.clear();
	m_PendingTurnBundle = false;
	m_HasPendingStart = false;
	m_HasPendingAction = false;
	m_LastChecksumTurn = UINT32_MAX;
	m_LastLocalChecksum = 0;
	m_HasLocalChecksum = false;
	m_NextAdvanceEarliest = 0.0;
	m_NextCmdResendTime = 0.0;
	m_NextBundleResendTime = 0.0;
	m_LastTurnBundle.clear();
	m_LastTurnBundleTurn = UINT32_MAX;
	m_HasEarlyTurnBundle = false;
	m_EarlyBundlePayload.clear();
}

void LockstepController::ConfigureMatch(int numPlayers, int localSlot, uint16_t turnLength)
{
	m_NumPlayers = std::clamp(numPlayers, 1, kMaxPlayers);
	m_LocalSlot = std::clamp(localSlot, 0, kMaxPlayers - 1);
	m_TurnLength = std::max<uint16_t>(1, turnLength);
	m_Turn = 0;
	m_Desynced = false;
	m_LocalSubmitted = false;
	m_Commands.fill(std::nullopt);
	m_LastChecksumTurn = UINT32_MAX;
	m_LastLocalChecksum = 0;
	m_HasLocalChecksum = false;
	m_Status = "Match configured (" + std::to_string(m_NumPlayers) + "p, turnLen=" + std::to_string(m_TurnLength) + ")";
}

void LockstepController::PauseMatch(const std::string& reason)
{
	m_MatchRunning = false;
	m_LocalSubmitted = false;
	m_Commands.fill(std::nullopt);
	m_PendingTurnBundle = false;
	m_HasPendingAction = false;
	m_Status = reason;
	Log("Lockstep: " + reason);
}

int LockstepController::FindFreeJoinSlot() const
{
	bool used[kMaxPlayers] = {};
	used[0] = true; // host
	for (int peer = 0; peer < kMaxPlayers; ++peer)
	{
		const int s = m_PeerToSlot[static_cast<size_t>(peer)];
		if (s >= 1 && s < kMaxPlayers)
			used[s] = true;
	}
	for (int s = 1; s < kMaxPlayers; ++s)
	{
		if (!used[s])
			return s;
	}
	return -1;
}

PlayerColorId LockstepController::PickRandomJoinerColor() const
{
	bool used[static_cast<int>(PlayerColorId::Count)] = {};
	used[static_cast<int>(PlayerColorId::Green)] = true; // host
	for (int s = 1; s < kMaxPlayers; ++s)
	{
		if (!m_SlotNames[static_cast<size_t>(s)].empty())
			used[static_cast<int>(m_SlotColors[static_cast<size_t>(s)])] = true;
	}

	PlayerColorId choices[3];
	int n = 0;
	const PlayerColorId pool[3] = {
		PlayerColorId::Red, PlayerColorId::Blue, PlayerColorId::Yellow
	};
	for (PlayerColorId c : pool)
	{
		if (!used[static_cast<int>(c)])
			choices[n++] = c;
	}
	if (n <= 0)
		return PlayerColorId::Red;

	// Non-sim RNG is fine for lobby cosmetics.
	const int pick = GetRandomValue(0, n - 1);
	return choices[pick];
}

void LockstepController::ApplyColorsToSim(GameSim& sim, int simPlayers) const
{
	for (int i = 0; i < simPlayers && i < kMaxPlayers; ++i)
		sim.GetPlayer(i).m_Color = m_SlotColors[static_cast<size_t>(i)];
}

void LockstepController::OnPeerHelloAsHost(int peerIndex, const std::string& name, NetSession& net)
{
	if (peerIndex < 0 || peerIndex >= kMaxPlayers)
		return;

	// Already assigned (duplicate Hello)?
	if (m_PeerToSlot[static_cast<size_t>(peerIndex)] >= 1)
		return;

	const int slot = FindFreeJoinSlot();
	if (slot < 0)
	{
		Log("Lockstep: no free slots (max " + std::to_string(kMaxPlayers) + " players)");
		return;
	}

	m_PeerToSlot[static_cast<size_t>(peerIndex)] = slot;
	std::string display = name.empty() ? ("Player" + std::to_string(slot)) : name;
	m_SlotNames[static_cast<size_t>(slot)] = display;
	m_SlotColors[static_cast<size_t>(slot)] = PickRandomJoinerColor();

	Net::WelcomeInfo w{};
	w.assignedSlot = static_cast<uint8_t>(slot);
	w.maxPlayers = kMaxPlayers;
	w.port = net.Port();
	w.turnLength = static_cast<uint16_t>(m_TurnLength);
	w.assignedColor = static_cast<uint8_t>(m_SlotColors[static_cast<size_t>(slot)]);
	net.SendToPeer(peerIndex, Net::PackWelcome(w));
	m_Status = display + " joined as " + PlayerColorName(m_SlotColors[static_cast<size_t>(slot)])
		+ " (slot " + std::to_string(slot) + ")";
	Log("Lockstep: " + m_Status + " (peer " + std::to_string(peerIndex) + ")");
}

void LockstepController::OnPeerDisconnectedAsHost(int peerIndex)
{
	if (peerIndex < 0 || peerIndex >= kMaxPlayers)
		return;
	const int slot = m_PeerToSlot[static_cast<size_t>(peerIndex)];
	std::string name = "Player";
	std::string colorName;
	if (slot >= 1 && slot < kMaxPlayers)
	{
		if (!m_SlotNames[static_cast<size_t>(slot)].empty())
			name = m_SlotNames[static_cast<size_t>(slot)];
		colorName = PlayerColorName(m_SlotColors[static_cast<size_t>(slot)]);
		m_SlotNames[static_cast<size_t>(slot)].clear();
		m_SlotColors[static_cast<size_t>(slot)] = PlayerColorId::Green;
	}
	m_PeerToSlot[static_cast<size_t>(peerIndex)] = -1;
	m_Status = name + (colorName.empty() ? "" : (" (" + colorName + ")")) + " left";
	Log("Lockstep: " + m_Status + " (peer " + std::to_string(peerIndex)
		+ ", freed slot " + std::to_string(slot) + ")");
}

uint32_t LockstepController::AssignedSlotForPeer(int peerIndex) const
{
	if (peerIndex < 0 || peerIndex >= kMaxPlayers)
		return 0;
	const int s = m_PeerToSlot[peerIndex];
	return s >= 0 ? static_cast<uint32_t>(s) : 0;
}

int LockstepController::OccupiedRemoteSlots() const
{
	int n = 0;
	for (int peer = 0; peer < kMaxPlayers; ++peer)
	{
		if (m_PeerToSlot[static_cast<size_t>(peer)] >= 1)
			++n;
	}
	return n;
}

std::string LockstepController::PeerNameForSlot(int slot) const
{
	if (slot < 0 || slot >= kMaxPlayers)
		return {};
	return m_SlotNames[static_cast<size_t>(slot)];
}

PlayerColorId LockstepController::ColorForSlot(int slot) const
{
	if (slot < 0 || slot >= kMaxPlayers)
		return PlayerColorId::Green;
	return m_SlotColors[static_cast<size_t>(slot)];
}

bool LockstepController::TakePendingColorNotify(std::string& outColorName)
{
	if (!m_PendingColorNotify)
		return false;
	outColorName = m_PendingColorName;
	m_PendingColorNotify = false;
	m_PendingColorName.clear();
	return true;
}

bool LockstepController::TakePendingNetError(std::string& outMessage)
{
	if (!m_PendingNetError)
		return false;
	outMessage = m_PendingNetErrorMsg;
	m_PendingNetError = false;
	m_PendingNetErrorMsg.clear();
	return true;
}

void LockstepController::HostBroadcastStart(NetSession& net, uint32_t mapSeed)
{
	const int peers = net.ConnectedPeerCount();
	const int numPlayers = std::clamp(peers + 1, 1, kMaxPlayers);
	m_SlotColors[0] = PlayerColorId::Green;
	Net::StartMatchInfo s{};
	s.mapSeed = mapSeed;
	s.numPlayers = static_cast<uint8_t>(numPlayers);
	s.localHint = 0;
	s.turnLength = static_cast<uint16_t>(m_TurnLength);
	for (int i = 0; i < kMaxPlayers; ++i)
		s.colors[i] = static_cast<uint8_t>(m_SlotColors[static_cast<size_t>(i)]);
	net.Broadcast(Net::PackStartMatch(s));
	m_Status = "StartMatch broadcast seed=" + std::to_string(mapSeed);
	Log("Lockstep: " + m_Status + " players=" + std::to_string(numPlayers));
}

void LockstepController::BeginMatch(GameSim& sim, uint32_t mapSeed, int simPlayers, int localSlot, uint16_t turnLength, int inputPlayers)
{
	if (inputPlayers < 0)
		inputPlayers = simPlayers;
	ConfigureMatch(inputPlayers, localSlot, turnLength);
	// Offline / local: fixed colors by slot if not already assigned from lobby.
	if (inputPlayers <= 1)
	{
		m_SlotColors[0] = PlayerColorId::Green;
		m_SlotColors[1] = PlayerColorId::Red;
		m_SlotColors[2] = PlayerColorId::Blue;
		m_SlotColors[3] = PlayerColorId::Yellow;
	}
	sim.StartSkirmish(mapSeed, localSlot, simPlayers);
	ApplyColorsToSim(sim, simPlayers);
	m_MatchRunning = true;
	m_Turn = 0;
	m_LocalSubmitted = false;
	m_Commands.fill(std::nullopt);
	m_EarlyCommands.clear();
	m_PendingTurnBundle = false;
	m_HasPendingAction = false;
	m_LastChecksumTurn = UINT32_MAX;
	m_LastLocalChecksum = 0;
	m_HasLocalChecksum = false;
	m_NextAdvanceEarliest = 0.0;
	m_NextCmdResendTime = 0.0;
	m_NextBundleResendTime = 0.0;
	m_LastTurnBundle.clear();
	m_LastTurnBundleTurn = UINT32_MAX;
	m_Status = "Match running";
	Log("Lockstep: BeginMatch seed=" + std::to_string(mapSeed)
		+ " simPlayers=" + std::to_string(simPlayers)
		+ " inputPlayers=" + std::to_string(inputPlayers)
		+ " localSlot=" + std::to_string(localSlot)
		+ " turnLen=" + std::to_string(turnLength));
}

void LockstepController::SubmitLocalAction(PlayerAction action, int cellX, int cellZ, uint16_t extra)
{
	if (!m_MatchRunning || m_Desynced)
		return;

	// M1: command executes on the current turn barrier (no extra schedule delay).
	// turnLength still controls how many sim ticks run once the barrier opens.
	Net::PlayerCommand cmd{};
	cmd.turn = m_Turn;
	cmd.playerSlot = static_cast<uint8_t>(m_LocalSlot);
	cmd.action = static_cast<uint8_t>(action);
	cmd.cellX = static_cast<int16_t>(cellX);
	cmd.cellZ = static_cast<int16_t>(cellZ);
	cmd.extra = extra;
	m_PendingAction = cmd;
	m_HasPendingAction = true;
}

void LockstepController::BroadcastOrSendCommand(const Net::PlayerCommand& cmd, NetSession& net)
{
	// Only clients need to ship commands to the host. The host already has its
	// own command in m_Commands and will include everyone's in TurnBundle.
	if (net.GetMode() == NetSession::Mode::Client)
		net.SendToHost(Net::PackPlayerCommand(cmd));
}

void LockstepController::EnsureLocalCommandSubmitted(NetSession& net)
{
	if (!m_MatchRunning || m_Desynced)
		return;

	if (!m_LocalSubmitted)
	{
		Net::PlayerCommand cmd{};
		cmd.turn = m_Turn;
		cmd.playerSlot = static_cast<uint8_t>(m_LocalSlot);
		if (m_HasPendingAction)
		{
			cmd = m_PendingAction;
			cmd.turn = m_Turn;
			cmd.playerSlot = static_cast<uint8_t>(m_LocalSlot);
			m_HasPendingAction = false;
		}
		else
		{
			cmd.action = Net::kActionNone;
		}

		m_Commands[static_cast<size_t>(m_LocalSlot)] = cmd;
		m_LocalSubmitted = true;
		m_NextCmdResendTime = 0.0; // send immediately
	}

	// Retransmit at ~10Hz until the barrier opens (avoid flooding ENet every frame).
	if (m_Commands[static_cast<size_t>(m_LocalSlot)].has_value()
		&& !AllCommandsReady()
		&& GetTime() >= m_NextCmdResendTime)
	{
		BroadcastOrSendCommand(*m_Commands[static_cast<size_t>(m_LocalSlot)], net);
		m_NextCmdResendTime = GetTime() + 0.1;
	}
}

void LockstepController::OnNetworkPacket(Net::PacketType type, const uint8_t* data, size_t size, int peerIndex, NetSession& net)
{
	const uint8_t* p = data;
	const uint8_t* end = data + size;

	switch (type)
	{
	case Net::PacketType::Hello:
	{
		if (net.GetMode() != NetSession::Mode::Host)
			break;
		uint16_t protocol = 0;
		uint8_t nameLen = 0;
		if (!Net::ReadU16(p, end, protocol)) break;
		if (!Net::ReadU8(p, end, nameLen)) break;
		std::string name;
		name.reserve(nameLen);
		for (uint8_t i = 0; i < nameLen; ++i)
		{
			uint8_t ch = 0;
			if (!Net::ReadU8(p, end, ch)) break;
			name.push_back(static_cast<char>(ch));
		}
		if (protocol != Net::kGameVersion)
		{
			const std::string msg = "Rejected " + (name.empty() ? std::string("client") : name)
				+ ": version mismatch (theirs " + std::to_string(protocol)
				+ ", ours " + std::to_string(Net::kGameVersion) + ")";
			Log("Lockstep: " + msg);
			m_Status = msg;
			m_PendingNetError = true;
			m_PendingNetErrorMsg = msg;
			net.SendToPeer(peerIndex, Net::PackVersionReject(protocol, Net::kGameVersion));
			net.DisconnectPeer(peerIndex);
			break;
		}
		OnPeerHelloAsHost(peerIndex, name, net);
		break;
	}

	case Net::PacketType::VersionReject:
	{
		uint16_t theirs = 0, hostVer = 0;
		if (!Net::ReadU16(p, end, theirs)) break;
		if (!Net::ReadU16(p, end, hostVer)) break;
		(void)theirs;
		const std::string msg = "Join failed: version mismatch (you "
			+ Net::VersionLabel() + ", host v" + std::to_string(hostVer) + ")";
		Log("Lockstep: " + msg);
		m_Status = msg;
		m_PendingNetError = true;
		m_PendingNetErrorMsg = msg;
		net.Disconnect();
		break;
	}

	case Net::PacketType::Welcome:
	{
		uint16_t protocol = 0;
		uint8_t slot = 0, maxP = 0, color = 0;
		uint16_t port = 0, turnLen = 0;
		if (!Net::ReadU16(p, end, protocol)) break;
		if (protocol != Net::kGameVersion)
		{
			const std::string msg = "Join failed: host version v" + std::to_string(protocol)
				+ " != local " + Net::VersionLabel();
			Log("Lockstep: " + msg);
			m_Status = msg;
			m_PendingNetError = true;
			m_PendingNetErrorMsg = msg;
			net.Disconnect();
			break;
		}
		if (!Net::ReadU8(p, end, slot)) break;
		if (!Net::ReadU8(p, end, maxP)) break;
		if (!Net::ReadU16(p, end, port)) break;
		if (!Net::ReadU16(p, end, turnLen)) break;
		if (!Net::ReadU8(p, end, color)) break;
		m_LocalSlot = slot;
		m_TurnLength = turnLen;
		if (slot < kMaxPlayers)
		{
			m_SlotColors[static_cast<size_t>(slot)] = static_cast<PlayerColorId>(color);
			m_SlotNames[static_cast<size_t>(slot)] = "You";
		}
		const char* cname = PlayerColorName(static_cast<PlayerColorId>(color));
		m_Status = std::string("You are ") + cname + " (slot " + std::to_string(slot) + ")";
		Log("Lockstep: " + m_Status);
		m_PendingColorNotify = true;
		m_PendingColorName = cname;
		break;
	}

	case Net::PacketType::StartMatch:
	{
		uint32_t seed = 0;
		uint8_t numP = 0, hint = 0;
		uint16_t turnLen = 0;
		if (!Net::ReadU32(p, end, seed)) break;
		if (!Net::ReadU8(p, end, numP)) break;
		if (!Net::ReadU8(p, end, hint)) break;
		if (!Net::ReadU16(p, end, turnLen)) break;
		for (int i = 0; i < kMaxPlayers; ++i)
		{
			uint8_t c = static_cast<uint8_t>(i);
			if (!Net::ReadU8(p, end, c)) break;
			m_SlotColors[static_cast<size_t>(i)] = static_cast<PlayerColorId>(c);
		}
		m_PendingStartSeed = seed;
		m_PendingStartPlayers = numP;
		m_PendingStartTurnLen = turnLen;
		m_HasPendingStart = true;
		m_Status = "StartMatch received";
		Log("Lockstep: StartMatch seed=" + std::to_string(seed));
		break;
	}

	case Net::PacketType::PlayerCommand:
	{
		Net::PlayerCommand cmd{};
		if (!Net::ReadU32(p, end, cmd.turn)) break;
		if (!Net::ReadU8(p, end, cmd.playerSlot)) break;
		if (!Net::ReadU8(p, end, cmd.action)) break;
		if (!Net::ReadI16(p, end, cmd.cellX)) break;
		if (!Net::ReadI16(p, end, cmd.cellZ)) break;
		if (!Net::ReadU16(p, end, cmd.extra)) break;

		if (cmd.playerSlot >= kMaxPlayers)
			break;

		// Host only *stores* client commands - clients advance from TurnBundle alone.
		// (Rebroadcasting every PlayerCommand flooded ENet's reliable window and
		// eventually soft-locked idle matches around ~turn 800-900.)
		if (net.GetMode() == NetSession::Mode::Host && peerIndex >= 0)
		{
			const int mapped = m_PeerToSlot[peerIndex];
			if (mapped >= 0)
				cmd.playerSlot = static_cast<uint8_t>(mapped);
		}

		if (!m_MatchRunning)
		{
			m_EarlyCommands.push_back(cmd);
			break;
		}

		if (cmd.turn == m_Turn)
			m_Commands[cmd.playerSlot] = cmd;
		break;
	}

	case Net::PacketType::TurnChecksum:
	{
		uint32_t turn = 0, sum = 0;
		if (!Net::ReadU32(p, end, turn)) break;
		if (!Net::ReadU32(p, end, sum)) break;

		// Only compare checksums for the same executed turn.
		if (!m_HasLocalChecksum || turn != m_LastChecksumTurn)
			break;

		if (sum != m_LastLocalChecksum)
		{
			m_Desynced = true;
			m_Status = "DESYNC at turn " + std::to_string(turn);
			Log("Lockstep: " + m_Status
				+ " localHash=" + std::to_string(m_LastLocalChecksum)
				+ " remoteHash=" + std::to_string(sum)
				+ " simTick=" + std::to_string(m_Turn)
				+ " peer=" + std::to_string(peerIndex));
		}
		break;
	}

	case Net::PacketType::TurnBundle:
	{
		if (net.GetMode() != NetSession::Mode::Client || m_Desynced)
			break;

		uint32_t turn = 0;
		uint8_t numP = 0;
		if (!Net::ReadU32(p, end, turn)) break;
		if (!Net::ReadU8(p, end, numP)) break;

		if (!m_MatchRunning)
		{
			// StartMatch + first TurnBundle often arrive together - stash payload.
			m_HasEarlyTurnBundle = true;
			m_EarlyBundleTurn = turn;
			m_EarlyBundleNumP = numP;
			m_EarlyBundlePayload.assign(p, end);
			break;
		}

		if (turn < m_Turn)
			break; // already applied (retransmit)
		if (turn > m_Turn)
		{
			Log("Lockstep: TurnBundle for future turn " + std::to_string(turn)
				+ " (local " + std::to_string(m_Turn) + ")");
			break;
		}

		ApplyTurnBundlePayload(turn, numP, p, end);
		m_PendingTurnBundle = true;
		break;
	}

	default:
		break;
	}
}

bool LockstepController::AllCommandsReady() const
{
	for (int i = 0; i < m_NumPlayers; ++i)
	{
		if (!m_Commands[static_cast<size_t>(i)].has_value())
			return false;
	}
	return true;
}

void LockstepController::ApplyTurnBundlePayload(uint32_t turn, uint8_t numP, const uint8_t*& p, const uint8_t* end)
{
	m_Commands.fill(std::nullopt);
	for (uint8_t i = 0; i < numP && i < kMaxPlayers; ++i)
	{
		Net::PlayerCommand cmd{};
		cmd.turn = turn;
		if (!Net::ReadU8(p, end, cmd.playerSlot)) break;
		if (!Net::ReadU8(p, end, cmd.action)) break;
		if (!Net::ReadI16(p, end, cmd.cellX)) break;
		if (!Net::ReadI16(p, end, cmd.cellZ)) break;
		if (!Net::ReadU16(p, end, cmd.extra)) break;
		if (cmd.playerSlot < kMaxPlayers)
			m_Commands[cmd.playerSlot] = cmd;
	}
}

uint32_t LockstepController::ComputeChecksum(const GameSim& sim) const
{
	uint32_t h = 2166136261u;
	auto mix = [&](uint32_t v)
	{
		h ^= v;
		h *= 16777619u;
	};

	mix(sim.Tick());
	mix(static_cast<uint32_t>(sim.Units().size()));

	std::vector<int> ids;
	ids.reserve(sim.Units().size());
	for (const auto& [id, unit] : sim.Units())
	{
		(void)unit;
		ids.push_back(id);
	}
	std::sort(ids.begin(), ids.end());
	for (int id : ids)
	{
		const Unit* u = sim.GetUnit(id);
		if (!u) continue;
		mix(static_cast<uint32_t>(id));
		mix(static_cast<uint32_t>(u->m_Type));
		mix(static_cast<uint32_t>(u->m_Team));
		union { float f; uint32_t u32; } x{ u->m_Pos.x }, z{ u->m_Pos.z }, hp{ u->m_HitPoints };
		mix(x.u32);
		mix(z.u32);
		mix(hp.u32);
	}

	if (g_Terrain)
	{
		// Sparse height samples for a cheap terrain fingerprint.
		for (int z = 0; z < g_Terrain->m_CellHeight; z += 8)
			for (int x = 0; x < g_Terrain->m_CellWidth; x += 8)
			{
				union { float f; uint32_t u32; } v{ g_Terrain->GetValue(x, z) };
				mix(v.u32);
			}
	}

	if (g_vitalRNG)
	{
		unsigned int seed = 0, index = 0;
		g_vitalRNG->GetRNGState(seed, index);
		mix(seed);
		mix(index);
		mix(g_vitalRNG->GetOriginalSeed());
	}
	return h;
}

void LockstepController::AdvanceTurn(NetSession& net, GameSim& sim)
{
	const uint32_t executedTurn = m_Turn;

	// Apply commands in slot order.
	for (int i = 0; i < m_NumPlayers; ++i)
	{
		const auto& cmd = m_Commands[static_cast<size_t>(i)];
		if (!cmd) continue;
		if (cmd->action == Net::kActionNone)
			continue;
		sim.ApplyPlayerCommand(static_cast<PlayerAction>(cmd->action), cmd->playerSlot, cmd->cellX, cmd->cellZ, cmd->extra);
	}

	for (int t = 0; t < m_TurnLength; ++t)
		sim.TickOnce();

	if (g_Terrain)
		g_Terrain->Update();

	const uint32_t sum = ComputeChecksum(sim);
	m_LastChecksumTurn = executedTurn;
	m_LastLocalChecksum = sum;
	m_HasLocalChecksum = true;

	// Pace next turn to real time (turnLength ticks x 33ms).
	m_NextAdvanceEarliest = GetTime() + (static_cast<double>(m_TurnLength) * (kSimTickMs / 1000.0));

	// Don't spam the log every turn - every 30 turns (~1s) is enough.
	if ((executedTurn % 30u) == 0u)
	{
		Log("Lockstep: checksum turn=" + std::to_string(executedTurn)
			+ " hash=" + std::to_string(sum)
			+ " units=" + std::to_string(sim.Units().size())
			+ " simTick=" + std::to_string(sim.Tick())
			+ " slot=" + std::to_string(m_LocalSlot));
	}

	// Checksums every 30 turns - enough for desync detection without flooding ENet.
	if ((executedTurn % 30u) == 0u)
	{
		const auto bytes = Net::PackTurnChecksum(executedTurn, sum);
		if (net.GetMode() == NetSession::Mode::Host)
			net.Broadcast(bytes);
		else if (net.GetMode() == NetSession::Mode::Client)
			net.SendToHost(bytes);
	}

	++m_Turn;
	m_LocalSubmitted = false;
	m_Commands.fill(std::nullopt);
	m_PendingTurnBundle = false;
	m_Status = "Turn " + std::to_string(m_Turn);
}

void LockstepController::Update(NetSession& net, GameSim& sim)
{
	if (m_HasPendingStart)
	{
		BeginMatch(sim, m_PendingStartSeed, m_PendingStartPlayers, m_LocalSlot, m_PendingStartTurnLen, m_PendingStartPlayers);
		m_HasPendingStart = false;

		for (const auto& cmd : m_EarlyCommands)
		{
			if (cmd.playerSlot < kMaxPlayers && cmd.turn == m_Turn)
				m_Commands[cmd.playerSlot] = cmd;
		}
		m_EarlyCommands.clear();

		// Apply TurnBundle that arrived with StartMatch.
		if (m_HasEarlyTurnBundle && m_EarlyBundleTurn == m_Turn)
		{
			const uint8_t* bp = m_EarlyBundlePayload.data();
			const uint8_t* bend = bp + m_EarlyBundlePayload.size();
			ApplyTurnBundlePayload(m_EarlyBundleTurn, m_EarlyBundleNumP, bp, bend);
			m_PendingTurnBundle = true;
		}
		m_HasEarlyTurnBundle = false;
		m_EarlyBundlePayload.clear();
	}

	if (!m_MatchRunning || m_Desynced)
		return;

	const bool isClient = (net.GetMode() == NetSession::Mode::Client);
	const bool isHost = (net.GetMode() == NetSession::Mode::Host);

	EnsureLocalCommandSubmitted(net);

	// Clients only advance when the host sends a TurnBundle.
	if (isClient)
	{
		if (m_PendingTurnBundle)
			AdvanceTurn(net, sim);
		return;
	}

	// Host: while waiting for the next turn's commands, retransmit the previous
	// TurnBundle so a client that missed it can catch up.
	if (isHost && !AllCommandsReady() && !m_LastTurnBundle.empty()
		&& m_LastTurnBundleTurn + 1 == m_Turn
		&& GetTime() >= m_NextBundleResendTime)
	{
		net.Broadcast(m_LastTurnBundle);
		m_NextBundleResendTime = GetTime() + 0.1;
	}

	// Host: if a client command packet was lost, fill missing slots with no-ops
	// after a short grace period so idle matches can't soft-lock forever.
	if (isHost && !AllCommandsReady() && GetTime() >= m_NextAdvanceEarliest + 0.5)
	{
		bool filled = false;
		for (int i = 0; i < m_NumPlayers; ++i)
		{
			if (m_Commands[static_cast<size_t>(i)].has_value())
				continue;
			Net::PlayerCommand noop{};
			noop.turn = m_Turn;
			noop.playerSlot = static_cast<uint8_t>(i);
			noop.action = Net::kActionNone;
			m_Commands[static_cast<size_t>(i)] = noop;
			filled = true;
		}
		if (filled)
		{
			Log("Lockstep: stall recovery - filled missing cmds with no-ops at turn "
				+ std::to_string(m_Turn));
		}
	}

	// Host / offline: wait for all cmds AND wall-clock turn duration.
	if (AllCommandsReady() && GetTime() >= m_NextAdvanceEarliest)
	{
		if (isHost)
		{
			Net::PlayerCommand cmds[kMaxPlayers]{};
			for (int i = 0; i < m_NumPlayers; ++i)
			{
				if (m_Commands[static_cast<size_t>(i)].has_value())
					cmds[i] = *m_Commands[static_cast<size_t>(i)];
				else
				{
					cmds[i].turn = m_Turn;
					cmds[i].playerSlot = static_cast<uint8_t>(i);
					cmds[i].action = Net::kActionNone;
				}
			}
			m_LastTurnBundle = Net::PackTurnBundle(m_Turn, static_cast<uint8_t>(m_NumPlayers), cmds);
			m_LastTurnBundleTurn = m_Turn;
			net.Broadcast(m_LastTurnBundle);
			m_NextBundleResendTime = GetTime() + 0.1;
		}
		AdvanceTurn(net, sim);
	}
}
