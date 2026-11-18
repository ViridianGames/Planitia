#include "Harness/SettleAgent.h"
#include "Harness/StdioSeat.h"
#include "GameGlobals.h"
#include "GameSim.h"
#include "Geist/Logging.h"
#include "MatchChecksum.h"
#include "NetProtocol.h"
#include "Replay.h"
#include "json.hpp"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	constexpr const char* kMapConfig = "Data/Maps/MainMenuTerrain.txt";
	constexpr double kStallSeconds = 60.0;
	constexpr double kSampleSeconds = 15.0;
	constexpr int kGrowthNeed = 3;
	constexpr uint16_t kTurnLength = 2;
	constexpr uint16_t kInputDelay = 2;

	enum class SeatKind
	{
		Settle,
		Stdio
	};

	struct SeatRun
	{
		SeatKind kind = SeatKind::Settle;
		std::string command;
		std::unique_ptr<StdioSeat> io;
		int startPopulation = 0;
		int endPopulation = 0;
		int commands = 0;
		int passes = 0;
		int rejected = 0;
		std::string lastAction = "none";
		bool haveSig = false;
		int sigPop = 0;
		int sigFood = 0;
		int sigBlessed = 0;
		int sigPrepared = 0;
		double sigSince = 0.0;
		bool stalled = false;
	};

	void PrintUsage()
	{
		std::cerr
			<< "usage:\n"
			<< "  planitia-harness run [--seconds N] [--seed N] [--seat settle | --seat stdio:<command>]...\n"
			<< "  planitia-harness show <result.json>\n"
			<< "Run from Planitia/Redist. Default is two settle seats, 180 match seconds, seed 7777.\n";
	}

	bool ParseUnsigned(const std::string& text, unsigned& out)
	{
		if (text.empty())
			return false;
		unsigned long value = 0;
		for (char c : text)
		{
			if (c < '0' || c > '9')
				return false;
			value = value * 10ul + static_cast<unsigned long>(c - '0');
			if (value > 0xFFFFFFFFul)
				return false;
		}
		out = static_cast<unsigned>(value);
		return true;
	}

	double MatchSeconds(uint32_t tick)
	{
		return static_cast<double>(tick) * (static_cast<double>(kSimTickMs) / 1000.0);
	}

	const char* KindName(SeatKind kind)
	{
		return kind == SeatKind::Settle ? "settle" : "stdio";
	}

	void BeginHeadlessMatch(unsigned seed, int players)
	{
		if (!g_Terrain)
			g_Terrain = std::make_unique<Terrain>();
		g_Terrain->InitSimulation(kMapConfig);
		g_Sim.StartSkirmish(seed, 0, players);
	}

	nlohmann::json CellList(const std::vector<std::pair<int, int>>& cells)
	{
		nlohmann::json out = nlohmann::json::array();
		for (const auto& cell : cells)
			out.push_back({ { "x", cell.first }, { "z", cell.second } });
		return out;
	}

	nlohmann::json Observation(int slot, uint32_t turn, double simSeconds, const std::vector<SeatInspect>& views)
	{
		const Player& you = g_Sim.GetPlayer(slot);
		const SeatInspect& self = views[static_cast<size_t>(slot)];
		nlohmann::json villages = nlohmann::json::array();
		for (const SeatVillageInfo& village : self.villages)
		{
			villages.push_back({
				{ "x", village.x },
				{ "z", village.z },
				{ "size", village.size },
				{ "food", village.food },
				{ "villagers", village.villagers },
			});
		}

		nlohmann::json others = nlohmann::json::array();
		for (int i = 0; i < static_cast<int>(views.size()); ++i)
		{
			if (i == slot)
				continue;
			nlohmann::json cells = nlohmann::json::array();
			for (const SeatVillageInfo& village : views[static_cast<size_t>(i)].villages)
				cells.push_back({ { "x", village.x }, { "z", village.z } });
			others.push_back({
				{ "slot", i },
				{ "population", views[static_cast<size_t>(i)].population },
				{ "villages", cells },
			});
		}

		return {
			{ "turn", turn },
			{ "slot", slot },
			{ "matchSeconds", simSeconds },
			{ "you", {
				{ "mana", you.m_Mana },
				{ "manaMax", you.m_ManaMax },
				{ "cooldown", you.m_SpellCooldown },
				{ "population", you.m_Population },
				{ "eliminated", you.m_Eliminated },
				{ "villages", villages },
			} },
			{ "others", others },
			{ "flatten", CellList(self.flatten) },
			{ "bless", CellList(self.bless) },
		};
	}

	SeatCommand Decide(SeatRun& seat, int slot, uint32_t turn, double simSeconds, const std::vector<SeatInspect>& views)
	{
		if (seat.kind == SeatKind::Settle)
			return views[static_cast<size_t>(slot)].suggestion;

		SeatCommand cmd;
		if (!seat.io || !seat.io->Alive())
			return cmd;

		std::string reply;
		if (!seat.io->Exchange(Observation(slot, turn, simSeconds, views).dump(), reply))
			return cmd;

		std::string error;
		if (!ParseSeatCommandLine(reply, cmd, error))
		{
			(void)error;
			++seat.rejected;
			cmd = {};
		}
		return cmd;
	}

	Net::PlayerCommand ToRecord(uint32_t turn, int slot, const SeatCommand& cmd)
	{
		Net::PlayerCommand out{};
		out.turn = turn;
		out.playerSlot = static_cast<uint8_t>(slot);
		if (cmd.pass)
		{
			out.action = Net::kActionNone;
			return out;
		}
		out.action = static_cast<uint8_t>(cmd.action);
		out.cellX = static_cast<int16_t>(cmd.x);
		out.cellZ = static_cast<int16_t>(cmd.z);
		out.extra = cmd.extra;
		return out;
	}

	void NoteProgress(SeatRun& seat, int slot, const SeatInspect& view, double simSeconds, std::string& failure)
	{
		const bool same = seat.haveSig
			&& seat.sigPop == view.population
			&& seat.sigFood == view.food
			&& seat.sigBlessed == view.blessed
			&& seat.sigPrepared == view.prepared;
		if (!same)
		{
			seat.haveSig = true;
			seat.sigPop = view.population;
			seat.sigFood = view.food;
			seat.sigBlessed = view.blessed;
			seat.sigPrepared = view.prepared;
			seat.sigSince = simSeconds;
			return;
		}
		if (!seat.stalled && !view.suggestion.pass && (simSeconds - seat.sigSince) >= kStallSeconds)
		{
			seat.stalled = true;
			if (failure.empty())
				failure = "slot " + std::to_string(slot) + " stalled for 60s of match time with work available";
		}
	}

	void PushSample(nlohmann::json& samples, double simSeconds, const std::vector<SeatRun>& seats, const std::vector<SeatInspect>& views)
	{
		nlohmann::json row;
		row["simSeconds"] = simSeconds;
		row["seats"] = nlohmann::json::array();
		for (size_t i = 0; i < seats.size(); ++i)
		{
			const SeatInspect& view = views[i];
			row["seats"].push_back({
				{ "slot", static_cast<int>(i) },
				{ "population", view.population },
				{ "villages", view.villageCount },
				{ "food", view.food },
				{ "mana", view.mana },
				{ "blessed", view.blessed },
				{ "prepared", view.prepared },
				{ "lastAction", seats[i].lastAction },
			});
		}
		samples.push_back(std::move(row));
	}

	std::vector<SeatInspect> InspectAll(int players)
	{
		std::vector<SeatInspect> views;
		views.reserve(static_cast<size_t>(players));
		for (int slot = 0; slot < players; ++slot)
			views.push_back(InspectSeat(g_Sim, slot));
		return views;
	}

	std::string VerifyReplay(const std::string& path, unsigned seed, int players, const std::vector<uint32_t>& checksums, bool matchOver, int winner)
	{
		Replay::Document doc;
		std::string error;
		if (!Replay::LoadFile(path, doc, error))
			return error;
		if (doc.header.mapSeed != seed
			|| doc.header.simPlayers != static_cast<uint8_t>(players)
			|| doc.header.inputPlayers != static_cast<uint8_t>(players)
			|| doc.header.localSlot != 0
			|| doc.header.turnLength != kTurnLength
			|| doc.header.inputDelay != kInputDelay)
		{
			return "replay header mismatch";
		}
		if (doc.turns.size() != checksums.size())
			return "replay turn count mismatch";

		BeginHeadlessMatch(doc.header.mapSeed, doc.header.simPlayers);
		for (size_t i = 0; i < doc.turns.size(); ++i)
		{
			const Replay::TurnRecord& rec = doc.turns[i];
			if (rec.turn != static_cast<uint32_t>(i))
				return "replay diverged at turn " + std::to_string(i);

			const Net::PlayerCommand* bySlot[kMaxPlayers] = {};
			for (const Net::PlayerCommand& cmd : rec.cmds)
			{
				if (cmd.playerSlot < kMaxPlayers)
					bySlot[cmd.playerSlot] = &cmd;
			}
			for (int slot = 0; slot < players; ++slot)
			{
				const Net::PlayerCommand* cmd = bySlot[slot];
				if (!cmd || cmd->action == Net::kActionNone)
					continue;
				g_Sim.ApplyPlayerCommand(
					static_cast<PlayerAction>(cmd->action),
					cmd->playerSlot,
					cmd->cellX,
					cmd->cellZ,
					cmd->extra);
			}
			for (int tick = 0; tick < doc.header.turnLength; ++tick)
				g_Sim.TickOnce();

			const uint32_t sum = PlanitiaMatchChecksum(g_Sim);
			if (sum != checksums[i])
				return "checksum mismatch on turn " + std::to_string(static_cast<unsigned>(i));
		}

		if (g_Sim.MatchOver() != matchOver)
			return "match-over mismatch";
		if (g_Sim.WinnerSlot() != winner)
			return "winner mismatch";
		if (doc.hasGameFinished && doc.winnerSlot != winner)
			return "winner mismatch";
		if (!doc.hasGameFinished && matchOver)
			return "replay is missing the game-finished trailer";
		return "";
	}

	std::string MakeResultPath(unsigned seed)
	{
		using clock = std::chrono::system_clock;
		const std::time_t now = clock::to_time_t(clock::now());
		std::tm tm{};
		localtime_r(&now, &tm);
		std::ostringstream oss;
		oss << "Data/HarnessRuns/run_"
			<< std::put_time(&tm, "%Y%m%d_%H%M%S")
			<< "_seed" << seed << ".json";
		return oss.str();
	}

	int ShowResult(const std::string& path)
	{
		std::ifstream in(path);
		if (!in)
		{
			std::cerr << "Could not open " << path << "\n";
			return 1;
		}
		nlohmann::json doc;
		try
		{
			doc = nlohmann::json::parse(in);
		}
		catch (const nlohmann::json::exception& ex)
		{
			std::cerr << ex.what() << "\n";
			return 1;
		}

		const auto text = [&](const char* key) -> std::string
		{
			if (!doc.contains(key) || doc[key].is_null())
				return "";
			if (doc[key].is_string())
				return doc[key].get<std::string>();
			return doc[key].dump();
		};
		const bool success = doc.value("success", false);
		const bool verified = doc.value("verified", false);
		std::cout << "seed " << doc.value("seed", 0) << "\n";
		std::cout << "sim " << doc.value("simSeconds", 0.0) << "s / requested " << doc.value("seconds", 0) << "s\n";
		std::cout << "wall " << doc.value("wallSeconds", 0.0) << "s\n";
		std::cout << "turns " << doc.value("turns", 0) << "\n";
		std::cout << "verified " << (verified ? "yes" : "no") << "\n";
		std::cout << "matchOver " << (doc.value("matchOver", false) ? "yes" : "no")
			<< " winner " << doc.value("winner", -1) << "\n";
		std::cout << "success " << (success ? "yes" : "no") << "\n";
		const std::string failure = text("failure");
		if (!failure.empty())
			std::cout << "failure: " << failure << "\n";
		const std::string verifyError = text("verifyError");
		if (!verifyError.empty())
			std::cout << "verify: " << verifyError << "\n";
		if (doc.contains("seats") && doc["seats"].is_array())
		{
			for (const auto& seat : doc["seats"])
			{
				std::cout << "slot " << seat.value("slot", -1)
					<< " " << seat.value("kind", std::string("?"))
					<< " population " << seat.value("startPopulation", 0)
					<< " -> " << seat.value("endPopulation", 0)
					<< " commands " << seat.value("commands", 0)
					<< " passes " << seat.value("passes", 0)
					<< " rejected " << seat.value("rejected", 0)
					<< "\n";
			}
		}
		std::cout << "replay: " << text("replay") << "\n";
		return 0;
	}

	int Run(unsigned seconds, unsigned seed, std::vector<SeatRun>& seats)
	{
		if (!std::filesystem::exists(kMapConfig))
		{
			std::cerr << "Data/Maps/MainMenuTerrain.txt not found. Run planitia-harness from Planitia/Redist.\n";
			return 1;
		}

		std::error_code ec;
		std::filesystem::create_directories("Data/HarnessRuns", ec);
		if (ec)
		{
			std::cerr << "Could not create Data/HarnessRuns: " << ec.message() << "\n";
			return 1;
		}
		SetLogFileName("Data/HarnessRuns/harness.log");

		const auto wallStart = std::chrono::steady_clock::now();
		const int players = static_cast<int>(seats.size());
		BeginHeadlessMatch(seed, players);

		for (int slot = 0; slot < players; ++slot)
		{
			SeatRun& seat = seats[static_cast<size_t>(slot)];
			if (seat.kind != SeatKind::Stdio)
				continue;
			seat.io = std::make_unique<StdioSeat>();
			std::string error;
			if (!seat.io->Start(seat.command, error))
			{
				std::cerr << "slot " << slot << ": " << error << "\n";
				return 1;
			}
		}

		std::vector<SeatInspect> views = InspectAll(players);
		for (int slot = 0; slot < players; ++slot)
		{
			SeatRun& seat = seats[static_cast<size_t>(slot)];
			seat.startPopulation = views[static_cast<size_t>(slot)].population;
			seat.endPopulation = seat.startPopulation;
		}

		std::string failure;
		for (int slot = 0; slot < players; ++slot)
			NoteProgress(seats[static_cast<size_t>(slot)], slot, views[static_cast<size_t>(slot)], 0.0, failure);

		nlohmann::json samples = nlohmann::json::array();
		PushSample(samples, 0.0, seats, views);
		double nextSample = kSampleSeconds;
		double lastSampleAt = 0.0;

		Replay::Header header;
		header.mapSeed = seed;
		header.simPlayers = static_cast<uint8_t>(players);
		header.inputPlayers = static_cast<uint8_t>(players);
		header.localSlot = 0;
		header.turnLength = kTurnLength;
		header.inputDelay = kInputDelay;
		for (int i = 0; i < kMaxPlayers; ++i)
			header.colors[i] = static_cast<uint8_t>(i);

		Replay::Writer writer;
		if (!writer.Start(header))
		{
			std::cerr << "Could not start the replay.\n";
			return 1;
		}

		std::vector<uint32_t> checksums;
		uint32_t turn = 0;
		while (MatchSeconds(g_Sim.Tick()) < static_cast<double>(seconds) && !g_Sim.MatchOver())
		{
			const double simSeconds = MatchSeconds(g_Sim.Tick());
			views = InspectAll(players);

			std::vector<SeatCommand> decided(static_cast<size_t>(players));
			for (int slot = 0; slot < players; ++slot)
				decided[static_cast<size_t>(slot)] = Decide(seats[static_cast<size_t>(slot)], slot, turn, simSeconds, views);

			Net::PlayerCommand recorded[kMaxPlayers]{};
			for (int slot = 0; slot < players; ++slot)
			{
				SeatRun& seat = seats[static_cast<size_t>(slot)];
				const SeatCommand& cmd = decided[static_cast<size_t>(slot)];
				if (cmd.pass)
				{
					++seat.passes;
					seat.lastAction = "none";
				}
				else
				{
					++seat.commands;
					seat.lastAction = SeatActionWireName(cmd.action);
					g_Sim.ApplyPlayerCommand(cmd.action, slot, cmd.x, cmd.z, cmd.extra);
				}
				recorded[slot] = ToRecord(turn, slot, cmd);
			}

			if (!writer.AppendTurn(turn, recorded, players))
			{
				if (failure.empty())
					failure = "failed to record turn " + std::to_string(turn);
				break;
			}

			for (int tick = 0; tick < kTurnLength; ++tick)
				g_Sim.TickOnce();
			checksums.push_back(PlanitiaMatchChecksum(g_Sim));

			const double after = MatchSeconds(g_Sim.Tick());
			views = InspectAll(players);
			for (int slot = 0; slot < players; ++slot)
			{
				seats[static_cast<size_t>(slot)].endPopulation = views[static_cast<size_t>(slot)].population;
				NoteProgress(seats[static_cast<size_t>(slot)], slot, views[static_cast<size_t>(slot)], after, failure);
			}
			if (after + 1e-9 >= nextSample)
			{
				PushSample(samples, after, seats, views);
				lastSampleAt = after;
				nextSample += kSampleSeconds;
			}
			++turn;
		}

		const bool matchOver = g_Sim.MatchOver();
		const int winner = g_Sim.WinnerSlot();
		const double simSeconds = MatchSeconds(g_Sim.Tick());
		if (samples.empty() || lastSampleAt != simSeconds)
			PushSample(samples, simSeconds, seats, views);

		if (matchOver && !writer.AppendGameFinished(winner) && failure.empty())
			failure = "failed to record the game-finished trailer";

		// Stop() clears the writer path. Keep the path, then close.
		const std::string replayPath = writer.Path();
		writer.Stop();

		const std::string verifyError = VerifyReplay(replayPath, seed, players, checksums, matchOver, winner);
		const bool verified = verifyError.empty();
		if (!verified && failure.empty())
			failure = verifyError;

		if (failure.empty())
		{
			for (int slot = 0; slot < players; ++slot)
			{
				const SeatRun& seat = seats[static_cast<size_t>(slot)];
				if (seat.kind != SeatKind::Settle)
					continue;
				const int rose = seat.endPopulation - seat.startPopulation;
				if (rose < kGrowthNeed)
				{
					failure = "slot " + std::to_string(slot) + " population rose by " + std::to_string(rose) + ", need 3";
					break;
				}
			}
		}

		const double wallSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - wallStart).count();
		const bool success = failure.empty() && verified;

		nlohmann::json result;
		result["seed"] = seed;
		result["seconds"] = seconds;
		result["simSeconds"] = simSeconds;
		result["wallSeconds"] = wallSeconds;
		result["replay"] = replayPath;
		result["verified"] = verified;
		result["verifyError"] = verifyError.empty() ? nlohmann::json(nullptr) : nlohmann::json(verifyError);
		result["matchOver"] = matchOver;
		result["winner"] = winner;
		result["turns"] = checksums.size();
		result["success"] = success;
		result["failure"] = failure.empty() ? nlohmann::json(nullptr) : nlohmann::json(failure);
		result["seats"] = nlohmann::json::array();
		for (int slot = 0; slot < players; ++slot)
		{
			const SeatRun& seat = seats[static_cast<size_t>(slot)];
			const int rose = seat.endPopulation - seat.startPopulation;
			result["seats"].push_back({
				{ "slot", slot },
				{ "kind", KindName(seat.kind) },
				{ "startPopulation", seat.startPopulation },
				{ "endPopulation", seat.endPopulation },
				{ "commands", seat.commands },
				{ "passes", seat.passes },
				{ "rejected", seat.rejected },
				{ "grew", rose >= kGrowthNeed },
			});
		}
		result["samples"] = samples;

		const std::string resultPath = MakeResultPath(seed);
		{
			std::ofstream out(resultPath);
			if (!out)
			{
				std::cerr << "Could not write " << resultPath << "\n";
				return 1;
			}
			out << result.dump(2) << "\n";
		}

		std::cout << std::fixed << std::setprecision(2);
		std::cout << "replay: " << replayPath << "\n";
		std::cout << "verified: " << (verified ? "yes" : "no") << "\n";
		std::cout << "wall: " << wallSeconds << "s\n";
		std::cout << "sim: " << simSeconds << "s\n";
		for (int slot = 0; slot < players; ++slot)
		{
			const SeatRun& seat = seats[static_cast<size_t>(slot)];
			std::cout << "slot " << slot << " " << KindName(seat.kind)
				<< " population " << seat.startPopulation << " -> " << seat.endPopulation << "\n";
		}
		if (!failure.empty())
			std::cout << "failure: " << failure << "\n";
		std::cout << "result: " << resultPath << "\n";
		return success ? 0 : 1;
	}
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		PrintUsage();
		return 1;
	}

	const std::string command = argv[1];
	if (command == "show")
	{
		if (argc != 3)
		{
			PrintUsage();
			return 1;
		}
		return ShowResult(argv[2]);
	}
	if (command != "run")
	{
		PrintUsage();
		return 1;
	}

	unsigned seconds = 180;
	unsigned seed = 7777;
	std::vector<SeatRun> seats;
	for (int i = 2; i < argc; ++i)
	{
		const std::string arg = argv[i];
		auto needValue = [&](const char* flag) -> const char*
		{
			if (i + 1 >= argc)
			{
				std::cerr << flag << " needs a value\n";
				return nullptr;
			}
			return argv[++i];
		};

		if (arg == "--seconds")
		{
			const char* value = needValue("--seconds");
			if (!value || !ParseUnsigned(value, seconds) || seconds == 0)
			{
				std::cerr << "--seconds needs a positive integer\n";
				return 1;
			}
		}
		else if (arg == "--seed")
		{
			const char* value = needValue("--seed");
			if (!value || !ParseUnsigned(value, seed))
			{
				std::cerr << "--seed needs an unsigned integer\n";
				return 1;
			}
		}
		else if (arg == "--seat")
		{
			const char* value = needValue("--seat");
			if (!value)
				return 1;
			const std::string spec = value;
			SeatRun seat;
			if (spec == "settle")
			{
				seat.kind = SeatKind::Settle;
			}
			else if (spec.rfind("stdio:", 0) == 0)
			{
				seat.kind = SeatKind::Stdio;
				seat.command = spec.substr(6);
				if (seat.command.empty())
				{
					std::cerr << "--seat stdio: needs a command\n";
					return 1;
				}
			}
			else
			{
				std::cerr << "Unknown seat \"" << spec << "\". Use settle or stdio:<command>.\n";
				return 1;
			}
			seats.push_back(std::move(seat));
		}
		else
		{
			std::cerr << "Unknown argument " << arg << "\n";
			PrintUsage();
			return 1;
		}
	}

	if (seats.empty())
	{
		seats.push_back(SeatRun{});
		seats.push_back(SeatRun{});
	}
	if (seats.size() < 2 || seats.size() > static_cast<size_t>(kMaxPlayers))
	{
		std::cerr << "A match needs 2 to 4 seats.\n";
		return 1;
	}
	return Run(seconds, seed, seats);
}
