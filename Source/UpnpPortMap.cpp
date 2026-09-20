#include "UpnpPortMap.h"

#include "Geist/Logging.h"

#include "miniupnpc.h"
#include "upnpcommands.h"
#include "upnperrors.h"

#include <cstdio>
#include <cstring>

namespace
{
	// miniupnpc UPNP_GetValidIGD return codes (see miniupnpc.h).
	const char* IgdCodeName(int igd)
	{
		switch (igd)
		{
		case -1: return "internal error";
		case 0: return "no IGD found";
		case 1: return "valid connected IGD";
		case 2: return "connected IGD with reserved/non-routable WAN IP";
		case 3: return "IGD found but reported not connected";
		case 4: return "UPnP device found but not recognized as IGD";
		default: return "unknown";
		}
	}

	void LogDiscoveredDevices(UPNPDev* devlist)
	{
		int n = 0;
		for (UPNPDev* d = devlist; d; d = d->pNext)
		{
			++n;
			Log(std::string("UpnpPortMap: device[") + std::to_string(n) + "] "
				+ (d->descURL ? d->descURL : "?")
				+ " st=" + (d->st ? d->st : "?"));
		}
		if (n == 0)
			Log("UpnpPortMap: discover returned empty device list");
		else
			Log("UpnpPortMap: discover found " + std::to_string(n) + " UPnP device(s)");
	}
}

UpnpPortMap::~UpnpPortMap()
{
	Close();
}

void UpnpPortMap::ClearUrls()
{
	m_ControlUrl.clear();
	m_ServiceType.clear();
}

std::string UpnpPortMap::PublicInvite() const
{
	if (!m_Mapped || m_ExternalIp.empty() || m_ExternalPort == 0)
		return {};
	// Skip non-routable "external" IPs some IGDs report.
	if (m_ExternalIp.rfind("192.168.", 0) == 0
		|| m_ExternalIp.rfind("10.", 0) == 0
		|| m_ExternalIp.rfind("172.", 0) == 0
		|| m_ExternalIp.rfind("127.", 0) == 0
		|| m_ExternalIp == "0.0.0.0")
		return {};
	return m_ExternalIp + ":" + std::to_string(m_ExternalPort);
}

bool UpnpPortMap::OpenUdp(uint16_t externalPort, uint16_t localPort, const char* description, unsigned int leaseSeconds)
{
	Close();

	m_LastError.clear();
	const int discoverMs = 2000;
	int error = 0;
	UPNPDev* devlist = upnpDiscover(discoverMs, nullptr, nullptr, 0, 0, 2, &error);
	if (!devlist)
	{
		m_LastError = "UPnP discover failed (no IGD / UPnP disabled on router?) err=" + std::to_string(error);
		Log("UpnpPortMap: " + m_LastError);
		return false;
	}

	LogDiscoveredDevices(devlist);

	UPNPUrls urls{};
	IGDdatas data{};
	char lanaddr[64] = {};
	char wanaddr[64] = {};
	const int igd = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr), wanaddr, sizeof(wanaddr));
	freeUPNPDevlist(devlist);
	devlist = nullptr;

	Log(std::string("UpnpPortMap: GetValidIGD -> ") + std::to_string(igd)
		+ " (" + IgdCodeName(igd) + ")"
		+ " lan=" + (lanaddr[0] ? lanaddr : "?")
		+ " wan=" + (wanaddr[0] ? wanaddr : "?")
		+ " control=" + (urls.controlURL ? urls.controlURL : "?"));

	// 1 = ideal, 2 = WAN IP private (double-NAT / CGNAT), 3 = IGD says not connected.
	// Codes 2 and 3 still populate urls/data; try AddPortMapping anyway (matches upnpc -ignore).
	// 4 = non-IGD UPnP device — still try if control URL exists.
	if (igd <= 0)
	{
		m_LastError = std::string("No UPnP IGD found (") + IgdCodeName(igd) + ", code " + std::to_string(igd) + ")";
		Log("UpnpPortMap: " + m_LastError);
		FreeUPNPUrls(&urls);
		return false;
	}

	if (igd >= 3)
	{
		Log(std::string("UpnpPortMap: proceeding despite GetValidIGD=") + std::to_string(igd)
			+ " (" + IgdCodeName(igd) + ")");
	}

	m_LanIp = lanaddr;
	m_ControlUrl = urls.controlURL ? urls.controlURL : "";
	m_ServiceType = data.first.servicetype;

	if (m_ControlUrl.empty() || m_ServiceType.empty())
	{
		m_LastError = "IGD missing control URL / service type (code " + std::to_string(igd) + ")";
		Log("UpnpPortMap: " + m_LastError);
		FreeUPNPUrls(&urls);
		ClearUrls();
		return false;
	}

	char extIp[64] = {};
	const int ipRc = UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, extIp);
	if (ipRc == UPNPCOMMAND_SUCCESS && extIp[0] != '\0')
		m_ExternalIp = extIp;
	else if (wanaddr[0] != '\0')
		m_ExternalIp = wanaddr;
	else
		m_ExternalIp.clear();

	Log("UpnpPortMap: external IP query rc=" + std::to_string(ipRc)
		+ " ext=" + (m_ExternalIp.empty() ? "?" : m_ExternalIp));

	char extPortStr[16];
	char inPortStr[16];
	std::snprintf(extPortStr, sizeof(extPortStr), "%u", static_cast<unsigned>(externalPort));
	std::snprintf(inPortStr, sizeof(inPortStr), "%u", static_cast<unsigned>(localPort));

	char leaseStr[16];
	std::snprintf(leaseStr, sizeof(leaseStr), "%u", leaseSeconds);

	const int addRc = UPNP_AddPortMapping(
		urls.controlURL,
		data.first.servicetype,
		extPortStr,
		inPortStr,
		lanaddr,
		description ? description : "Planitia",
		"UDP",
		nullptr,
		leaseStr);

	if (addRc != UPNPCOMMAND_SUCCESS)
	{
		m_LastError = std::string("AddPortMapping failed: ") + strupnperror(addRc)
			+ " (IGD was " + IgdCodeName(igd) + ")";
		Log("UpnpPortMap: " + m_LastError + " lan=" + m_LanIp + " ext=" + m_ExternalIp);
		FreeUPNPUrls(&urls);
		ClearUrls();
		return false;
	}

	m_Mapped = true;
	m_ExternalPort = externalPort;
	m_LocalPort = localPort;
	Log("UpnpPortMap: mapped UDP " + std::to_string(externalPort)
		+ " -> " + m_LanIp + ":" + std::to_string(localPort)
		+ " public=" + (m_ExternalIp.empty() ? "?" : m_ExternalIp)
		+ " igdCode=" + std::to_string(igd));

	FreeUPNPUrls(&urls);
	return true;
}

void UpnpPortMap::Close()
{
	if (!m_Mapped)
	{
		ClearUrls();
		m_ExternalIp.clear();
		m_LanIp.clear();
		m_ExternalPort = 0;
		m_LocalPort = 0;
		return;
	}

	if (!m_ControlUrl.empty() && !m_ServiceType.empty() && m_ExternalPort != 0)
	{
		char extPortStr[16];
		std::snprintf(extPortStr, sizeof(extPortStr), "%u", static_cast<unsigned>(m_ExternalPort));
		const int rc = UPNP_DeletePortMapping(
			m_ControlUrl.c_str(),
			m_ServiceType.c_str(),
			extPortStr,
			"UDP",
			nullptr);
		if (rc != UPNPCOMMAND_SUCCESS)
			Log(std::string("UpnpPortMap: DeletePortMapping failed: ") + strupnperror(rc));
		else
			Log("UpnpPortMap: removed UDP mapping for port " + std::to_string(m_ExternalPort));
	}

	m_Mapped = false;
	m_ExternalPort = 0;
	m_LocalPort = 0;
	m_ExternalIp.clear();
	m_LanIp.clear();
	ClearUrls();
	m_LastError.clear();
}
