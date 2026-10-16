#include "Replay.h"

#include "Geist/Logging.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

namespace Replay
{
namespace
{
	void WriteU8(FILE* f, uint8_t v) { fwrite(&v, 1, 1, f); }
	void WriteU16(FILE* f, uint16_t v)
	{
		const uint8_t b[2] = { static_cast<uint8_t>(v & 0xff), static_cast<uint8_t>((v >> 8) & 0xff) };
		fwrite(b, 1, 2, f);
	}
	void WriteU32(FILE* f, uint32_t v)
	{
		const uint8_t b[4] = {
			static_cast<uint8_t>(v & 0xff),
			static_cast<uint8_t>((v >> 8) & 0xff),
			static_cast<uint8_t>((v >> 16) & 0xff),
			static_cast<uint8_t>((v >> 24) & 0xff)
		};
		fwrite(b, 1, 4, f);
	}
	void WriteI16(FILE* f, int16_t v) { WriteU16(f, static_cast<uint16_t>(v)); }

	bool ReadU8(FILE* f, uint8_t& v) { return fread(&v, 1, 1, f) == 1; }
	bool ReadU16(FILE* f, uint16_t& v)
	{
		uint8_t b[2];
		if (fread(b, 1, 2, f) != 2) return false;
		v = static_cast<uint16_t>(b[0] | (static_cast<uint16_t>(b[1]) << 8));
		return true;
	}
	bool ReadU32(FILE* f, uint32_t& v)
	{
		uint8_t b[4];
		if (fread(b, 1, 4, f) != 4) return false;
		v = static_cast<uint32_t>(b[0])
			| (static_cast<uint32_t>(b[1]) << 8)
			| (static_cast<uint32_t>(b[2]) << 16)
			| (static_cast<uint32_t>(b[3]) << 24);
		return true;
	}
	bool ReadI16(FILE* f, int16_t& v)
	{
		uint16_t u = 0;
		if (!ReadU16(f, u)) return false;
		v = static_cast<int16_t>(u);
		return true;
	}

	bool WriteHeader(FILE* f, const Header& h)
	{
		WriteU32(f, kMagic);
		WriteU16(f, h.formatVersion);
		WriteU16(f, h.gameVersionPatch);
		WriteU32(f, h.mapSeed);
		WriteU8(f, h.simPlayers);
		WriteU8(f, h.inputPlayers);
		WriteU8(f, h.localSlot);
		WriteU16(f, h.turnLength);
		WriteU16(f, h.inputDelay);
		for (int i = 0; i < kMaxPlayers; ++i)
			WriteU8(f, h.colors[i]);
		WriteU16(f, 0); // reserved
		return ferror(f) == 0;
	}
} // namespace

void EnsureDataDirs()
{
	std::error_code ec;
	fs::create_directories(kReplaysDir, ec);
	fs::create_directories(kSavesDir, ec);
}

std::string LastReplayPath()
{
	return std::string(kReplaysDir) + "/" + kLastReplayName;
}

std::string MakeTimestampedReplayPath(uint32_t mapSeed)
{
	EnsureDataDirs();
	using clock = std::chrono::system_clock;
	const auto now = clock::now();
	const std::time_t t = clock::to_time_t(now);
	std::tm tm{};
#if defined(_WIN32)
	localtime_s(&tm, &t);
#else
	localtime_r(&t, &tm);
#endif
	std::ostringstream oss;
	oss << kReplaysDir << "/replay_"
		<< std::put_time(&tm, "%Y%m%d_%H%M%S")
		<< "_seed" << mapSeed << ".plr";
	return oss.str();
}

bool Writer::Start(const Header& header)
{
	Stop();
	EnsureDataDirs();
	m_Header = header;
	m_Header.formatVersion = kFormatVersion;
	m_Header.gameVersionPatch = Net::kVersionPatch;
	m_Path = MakeTimestampedReplayPath(header.mapSeed);
	m_File = fopen(m_Path.c_str(), "wb");
	if (!m_File)
	{
		Log("Replay: failed to open " + m_Path + " for writing");
		m_Path.clear();
		return false;
	}
	if (!WriteHeader(m_File, m_Header))
	{
		Log("Replay: failed to write header");
		Stop();
		return false;
	}
	Flush();
	Log("Replay: recording -> " + m_Path);
	return true;
}

bool Writer::AppendTurn(uint32_t turn, const Net::PlayerCommand* cmds, int numCmds)
{
	if (!m_File || !cmds || numCmds <= 0)
		return false;
	if (turn == kGameFinishedTurn)
		return false;
	WriteU32(m_File, turn);
	WriteU8(m_File, static_cast<uint8_t>(numCmds));
	for (int i = 0; i < numCmds; ++i)
	{
		WriteU8(m_File, cmds[i].playerSlot);
		WriteU8(m_File, cmds[i].action);
		WriteI16(m_File, cmds[i].cellX);
		WriteI16(m_File, cmds[i].cellZ);
		WriteU16(m_File, cmds[i].extra);
	}
	const bool ok = ferror(m_File) == 0;
	Flush();
	return ok;
}

bool Writer::AppendGameFinished(int winnerSlot)
{
	if (!m_File)
		return false;
	// turn = kGameFinishedTurn, numCmds = 1 sentinel: slot=winner (or 0xFF draw), action=None
	WriteU32(m_File, kGameFinishedTurn);
	WriteU8(m_File, 1);
	const uint8_t slot = (winnerSlot >= 0 && winnerSlot < kMaxPlayers)
		? static_cast<uint8_t>(winnerSlot)
		: static_cast<uint8_t>(0xFF);
	WriteU8(m_File, slot);
	WriteU8(m_File, Net::kActionNone);
	WriteI16(m_File, 0);
	WriteI16(m_File, 0);
	WriteU16(m_File, 0);
	const bool ok = ferror(m_File) == 0;
	Flush();
	return ok;
}

void Writer::Flush()
{
	if (m_File)
		fflush(m_File);
}

void Writer::Stop()
{
	if (!m_File)
		return;
	Flush();
	fclose(m_File);
	m_File = nullptr;

	// Refresh last.plr as a full copy of this recording.
	if (!m_Path.empty())
	{
		std::error_code ec;
		const fs::path last = LastReplayPath();
		fs::remove(last, ec);
		fs::copy_file(m_Path, last, fs::copy_options::overwrite_existing, ec);
		if (ec)
			Log("Replay: could not update last.plr: " + ec.message());
		else
			Log("Replay: saved " + m_Path);
	}
	m_Path.clear();
}

bool LoadFile(const std::string& path, Document& out, std::string& error)
{
	out = {};
	error.clear();
	FILE* f = fopen(path.c_str(), "rb");
	if (!f)
	{
		error = "Could not open " + path;
		return false;
	}

	uint32_t magic = 0;
	if (!ReadU32(f, magic) || magic != kMagic)
	{
		error = "Not a Planitia replay (bad magic)";
		fclose(f);
		return false;
	}
	Header& h = out.header;
	if (!ReadU16(f, h.formatVersion) || !ReadU16(f, h.gameVersionPatch)
		|| !ReadU32(f, h.mapSeed)
		|| !ReadU8(f, h.simPlayers) || !ReadU8(f, h.inputPlayers) || !ReadU8(f, h.localSlot)
		|| !ReadU16(f, h.turnLength) || !ReadU16(f, h.inputDelay))
	{
		error = "Truncated replay header";
		fclose(f);
		return false;
	}
	for (int i = 0; i < kMaxPlayers; ++i)
	{
		if (!ReadU8(f, h.colors[i]))
		{
			error = "Truncated replay header (colors)";
			fclose(f);
			return false;
		}
	}
	uint16_t reserved = 0;
	if (!ReadU16(f, reserved))
	{
		error = "Truncated replay header (reserved)";
		fclose(f);
		return false;
	}

	if (h.formatVersion != kFormatVersion)
	{
		error = "Unsupported replay format version " + std::to_string(h.formatVersion);
		fclose(f);
		return false;
	}
	if (h.gameVersionPatch != Net::kVersionPatch)
	{
		error = "Replay is for game patch " + std::to_string(h.gameVersionPatch)
			+ "; this build is " + std::to_string(Net::kVersionPatch);
		fclose(f);
		return false;
	}
	if (h.inputPlayers == 0 || h.inputPlayers > kMaxPlayers || h.simPlayers == 0 || h.simPlayers > kMaxPlayers)
	{
		error = "Invalid player counts in replay header";
		fclose(f);
		return false;
	}

	// Read turn records until EOF / truncation / GameFinished trailer.
	while (true)
	{
		TurnRecord rec{};
		if (!ReadU32(f, rec.turn))
			break; // clean EOF
		uint8_t numCmds = 0;
		if (!ReadU8(f, numCmds))
			break;

		if (rec.turn == kGameFinishedTurn)
		{
			// Trailer: one sentinel cmd, playerSlot = winner (0xFF = draw).
			uint8_t slot = 0xFF, action = 0;
			int16_t cx = 0, cz = 0;
			uint16_t extra = 0;
			if (numCmds >= 1
				&& ReadU8(f, slot) && ReadU8(f, action)
				&& ReadI16(f, cx) && ReadI16(f, cz) && ReadU16(f, extra))
			{
				out.hasGameFinished = true;
				out.winnerSlot = (slot == 0xFF) ? -1 : static_cast<int>(slot);
			}
			break;
		}

		if (numCmds != h.inputPlayers)
			break;
		rec.cmds.resize(numCmds);
		bool ok = true;
		for (uint8_t i = 0; i < numCmds; ++i)
		{
			Net::PlayerCommand& c = rec.cmds[i];
			c.turn = rec.turn;
			ok = ok && ReadU8(f, c.playerSlot) && ReadU8(f, c.action)
				&& ReadI16(f, c.cellX) && ReadI16(f, c.cellZ) && ReadU16(f, c.extra);
			if (!ok)
				break;
		}
		if (!ok)
			break;
		out.turns.push_back(std::move(rec));
	}

	fclose(f);
	if (out.turns.empty())
	{
		error = "Replay has no turns";
		return false;
	}
	return true;
}

bool FindNewestReplayPath(std::string& outPath)
{
	EnsureDataDirs();
	outPath.clear();

	const fs::path last = LastReplayPath();
	std::error_code ec;
	if (fs::is_regular_file(last, ec))
	{
		outPath = last.string();
		return true;
	}

	fs::path newest;
	fs::file_time_type newestTime{};
	bool found = false;
	for (const auto& entry : fs::directory_iterator(kReplaysDir, ec))
	{
		if (!entry.is_regular_file(ec))
			continue;
		if (entry.path().extension() != ".plr")
			continue;
		const auto t = entry.last_write_time(ec);
		if (ec)
			continue;
		if (!found || t > newestTime)
		{
			newest = entry.path();
			newestTime = t;
			found = true;
		}
	}
	if (!found)
		return false;
	outPath = newest.string();
	return true;
}

} // namespace Replay
