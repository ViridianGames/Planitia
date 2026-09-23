///////////////////////////////////////////////////////////////////////////
//
// Name:     NETPROTOCOL.H
// Purpose:  Listen-server lockstep packet IDs + little-endian serializers.
//
///////////////////////////////////////////////////////////////////////////

#ifndef _PLANITIA_NETPROTOCOL_H_
#define _PLANITIA_NETPROTOCOL_H_

#include "GameTypes.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Net
{
	constexpr uint32_t kMagic = 0x504C4E54u; // 'PLNT'

	// Display version: major.minor.patch  (e.g. "0.1.5").
	// Only the patch digit is the multiplayer compatibility key — bump patch
	// whenever the wire format or lockstep rules change incompatibly.
	constexpr uint16_t kVersionMajor = 0;
	constexpr uint16_t kVersionMinor = 1;
	constexpr uint16_t kVersionPatch = 7;
	constexpr uint16_t kProtocolVersion = kVersionPatch; // Hello/Welcome field
	constexpr uint8_t kChannelReliable = 0;

	inline std::string VersionLabel()
	{
		return std::to_string(kVersionMajor) + "."
			+ std::to_string(kVersionMinor) + "."
			+ std::to_string(kVersionPatch);
	}

	enum class PacketType : uint8_t
	{
		Hello = 1,       // client -> host on connect
		Welcome = 2,     // host -> client (slot assignment)
		StartMatch = 3,  // host -> all (seed, player count)
		PlayerCommand = 4,
		TurnChecksum = 5,
		Chat = 6,        // reserved
		TurnBundle = 7,  // host -> all: full command set for a turn (advance together)
		VersionReject = 8, // host -> client: Hello version mismatch (then disconnect)
	};

	// 0xFF = no-op for this turn (still required so the barrier can advance).
	constexpr uint8_t kActionNone = 0xFF;

	struct PlayerCommand
	{
		uint32_t turn = 0;
		uint8_t playerSlot = 0;
		uint8_t action = kActionNone;
		int16_t cellX = 0;
		int16_t cellZ = 0;
		uint16_t extra = 0;
	};

	struct WelcomeInfo
	{
		uint16_t protocol = kProtocolVersion;
		uint8_t assignedSlot = 0;
		uint8_t maxPlayers = kMaxPlayers;
		uint16_t port = 43000;
		uint16_t turnLength = 2;
		uint16_t inputDelay = 2; // schedule cmds this many turns ahead
		uint8_t assignedColor = 0; // PlayerColorId
	};

	struct StartMatchInfo
	{
		uint32_t mapSeed = 0;
		uint8_t numPlayers = 2;
		uint8_t localHint = 0; // ignored by clients; host's slot is 0
		uint16_t turnLength = 2;
		uint16_t inputDelay = 2;
		uint8_t colors[kMaxPlayers] = { 0, 1, 2, 3 }; // PlayerColorId per slot
	};

	inline void AppendU8(std::vector<uint8_t>& b, uint8_t v) { b.push_back(v); }
	inline void AppendU16(std::vector<uint8_t>& b, uint16_t v)
	{
		b.push_back(static_cast<uint8_t>(v & 0xff));
		b.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
	}
	inline void AppendU32(std::vector<uint8_t>& b, uint32_t v)
	{
		b.push_back(static_cast<uint8_t>(v & 0xff));
		b.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
		b.push_back(static_cast<uint8_t>((v >> 16) & 0xff));
		b.push_back(static_cast<uint8_t>((v >> 24) & 0xff));
	}
	inline void AppendI16(std::vector<uint8_t>& b, int16_t v)
	{
		AppendU16(b, static_cast<uint16_t>(v));
	}

	inline bool ReadU8(const uint8_t*& p, const uint8_t* end, uint8_t& v)
	{
		if (p >= end) return false;
		v = *p++;
		return true;
	}
	inline bool ReadU16(const uint8_t*& p, const uint8_t* end, uint16_t& v)
	{
		if (p + 2 > end) return false;
		v = static_cast<uint16_t>(p[0] | (p[1] << 8));
		p += 2;
		return true;
	}
	inline bool ReadU32(const uint8_t*& p, const uint8_t* end, uint32_t& v)
	{
		if (p + 4 > end) return false;
		v = static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
		p += 4;
		return true;
	}
	inline bool ReadI16(const uint8_t*& p, const uint8_t* end, int16_t& v)
	{
		uint16_t u = 0;
		if (!ReadU16(p, end, u)) return false;
		v = static_cast<int16_t>(u);
		return true;
	}

	inline std::vector<uint8_t> PackHello(const std::string& name)
	{
		std::vector<uint8_t> b;
		AppendU32(b, kMagic);
		AppendU8(b, static_cast<uint8_t>(PacketType::Hello));
		AppendU16(b, kProtocolVersion);
		const uint8_t n = static_cast<uint8_t>(std::min<size_t>(name.size(), 32));
		AppendU8(b, n);
		for (uint8_t i = 0; i < n; ++i)
			AppendU8(b, static_cast<uint8_t>(name[i]));
		return b;
	}

	inline std::vector<uint8_t> PackWelcome(const WelcomeInfo& w)
	{
		std::vector<uint8_t> b;
		AppendU32(b, kMagic);
		AppendU8(b, static_cast<uint8_t>(PacketType::Welcome));
		AppendU16(b, w.protocol);
		AppendU8(b, w.assignedSlot);
		AppendU8(b, w.maxPlayers);
		AppendU16(b, w.port);
		AppendU16(b, w.turnLength);
		AppendU16(b, w.inputDelay);
		AppendU8(b, w.assignedColor);
		return b;
	}

	// theirVersion = what the peer sent; hostVersion = what we require.
	inline std::vector<uint8_t> PackVersionReject(uint16_t theirVersion, uint16_t hostVersion = kProtocolVersion)
	{
		std::vector<uint8_t> b;
		AppendU32(b, kMagic);
		AppendU8(b, static_cast<uint8_t>(PacketType::VersionReject));
		AppendU16(b, theirVersion);
		AppendU16(b, hostVersion);
		return b;
	}

	inline std::vector<uint8_t> PackStartMatch(const StartMatchInfo& s)
	{
		std::vector<uint8_t> b;
		AppendU32(b, kMagic);
		AppendU8(b, static_cast<uint8_t>(PacketType::StartMatch));
		AppendU32(b, s.mapSeed);
		AppendU8(b, s.numPlayers);
		AppendU8(b, s.localHint);
		AppendU16(b, s.turnLength);
		AppendU16(b, s.inputDelay);
		for (int i = 0; i < kMaxPlayers; ++i)
			AppendU8(b, s.colors[i]);
		return b;
	}

	inline std::vector<uint8_t> PackPlayerCommand(const PlayerCommand& c)
	{
		std::vector<uint8_t> b;
		AppendU32(b, kMagic);
		AppendU8(b, static_cast<uint8_t>(PacketType::PlayerCommand));
		AppendU32(b, c.turn);
		AppendU8(b, c.playerSlot);
		AppendU8(b, c.action);
		AppendI16(b, c.cellX);
		AppendI16(b, c.cellZ);
		AppendU16(b, c.extra);
		return b;
	}

	inline std::vector<uint8_t> PackTurnChecksum(uint32_t turn, uint32_t checksum)
	{
		std::vector<uint8_t> b;
		AppendU32(b, kMagic);
		AppendU8(b, static_cast<uint8_t>(PacketType::TurnChecksum));
		AppendU32(b, turn);
		AppendU32(b, checksum);
		return b;
	}

	// Host authority: one packet with every slot's command for `turn`.
	inline std::vector<uint8_t> PackTurnBundle(uint32_t turn, uint8_t numPlayers, const PlayerCommand* cmds)
	{
		std::vector<uint8_t> b;
		AppendU32(b, kMagic);
		AppendU8(b, static_cast<uint8_t>(PacketType::TurnBundle));
		AppendU32(b, turn);
		AppendU8(b, numPlayers);
		for (uint8_t i = 0; i < numPlayers; ++i)
		{
			const PlayerCommand& c = cmds[i];
			AppendU8(b, c.playerSlot);
			AppendU8(b, c.action);
			AppendI16(b, c.cellX);
			AppendI16(b, c.cellZ);
			AppendU16(b, c.extra);
		}
		return b;
	}

	inline bool UnpackHeader(const uint8_t*& p, const uint8_t* end, PacketType& type)
	{
		uint32_t magic = 0;
		uint8_t t = 0;
		if (!ReadU32(p, end, magic) || magic != kMagic) return false;
		if (!ReadU8(p, end, t)) return false;
		type = static_cast<PacketType>(t);
		return true;
	}
}

#endif
