///////////////////////////////////////////////////////////////////////////
//
// Name:     NETSESSION.H
// Purpose:  ENet listen-server / client transport for Planitia lockstep.
//
///////////////////////////////////////////////////////////////////////////

#ifndef _PLANITIA_NETSESSION_H_
#define _PLANITIA_NETSESSION_H_

#include "NetProtocol.h"
#include "UpnpPortMap.h"

#include <functional>
#include <string>
#include <vector>

struct _ENetHost;
struct _ENetPeer;
typedef _ENetHost ENetHost;
typedef _ENetPeer ENetPeer;

class NetSession
{
public:
	enum class Mode
	{
		Offline = 0,
		Host,
		Client,
	};

	using PacketHandler = std::function<void(Net::PacketType type, const uint8_t* data, size_t size, int peerIndex)>;
	using PeerHandler = std::function<void(int peerIndex)>;

	NetSession() = default;
	~NetSession();

	bool Init();
	void Shutdown();

	bool Host(uint16_t port, int maxClients = kMaxPlayers - 1);
	bool Join(const std::string& ip, uint16_t port);
	void Disconnect();

	void Service(PacketHandler handler);
	void SetPeerHandlers(PeerHandler onConnect, PeerHandler onDisconnect);
	void Broadcast(const std::vector<uint8_t>& bytes, bool reliable = true);
	void SendToPeer(int peerIndex, const std::vector<uint8_t>& bytes, bool reliable = true);
	void SendToHost(const std::vector<uint8_t>& bytes, bool reliable = true);
	// Drop one peer immediately (host: reject bad Hello; after VersionReject send).
	void DisconnectPeer(int peerIndex);

	Mode GetMode() const { return m_Mode; }
	bool IsOnline() const { return m_Mode != Mode::Offline && m_Host != nullptr; }
	int ConnectedPeerCount() const;
	uint16_t Port() const { return m_Port; }
	const std::string& Status() const { return m_Status; }
	const std::string& LastError() const { return m_LastError; }

	// UPnP (host only): public invite if mapping succeeded.
	bool UpnpActive() const { return m_Upnp.IsMapped(); }
	std::string PublicInvite() const { return m_Upnp.PublicInvite(); }
	const std::string& UpnpError() const { return m_Upnp.LastError(); }
	const std::string& UpnpLanIp() const { return m_Upnp.LanIp(); }

private:
	Mode m_Mode = Mode::Offline;
	ENetHost* m_Host = nullptr;
	ENetPeer* m_ServerPeer = nullptr; // client mode
	uint16_t m_Port = 43000;
	std::string m_Status = "Offline";
	std::string m_LastError;
	bool m_ENetReady = false;
	UpnpPortMap m_Upnp;
	PeerHandler m_OnPeerConnect;
	PeerHandler m_OnPeerDisconnect;
};

#endif
