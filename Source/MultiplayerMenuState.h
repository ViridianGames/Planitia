#ifndef _PLANITIA_MULTIPLAYERMENUSTATE_H_
#define _PLANITIA_MULTIPLAYERMENUSTATE_H_

#include "Geist/State.h"

#include <cstdint>
#include <string>
#include <vector>

// Lobby: Host / Join / Start Match, then transition into MainState.
class MultiplayerMenuState : public State
{
public:
	MultiplayerMenuState() = default;
	~MultiplayerMenuState() override = default;

	void Init(const std::string& configfile) override;
	void Shutdown() override;
	void Update() override;
	void Draw() override;
	void OnEnter() override;
	void OnExit() override;

private:
	void RefreshHostInvite();
	bool ParseInvite(const std::string& text, std::string& outIp, uint16_t& outPort) const;
	std::string CurrentInvite() const;

	std::string m_JoinIp = "127.0.0.1";
	uint16_t m_JoinPort = 43000;
	std::string m_HostInvite;           // LAN invite e.g. "192.168.1.10:43000"
	std::string m_PublicInvite;         // Internet invite if UPnP succeeded
	std::vector<std::string> m_LocalIps;
	bool m_TransitionToMain = false;
};

#endif
