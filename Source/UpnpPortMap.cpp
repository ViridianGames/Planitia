#include "UpnpPortMap.h"

#include "Geist/Logging.h"

#include "miniupnpc.h"
#include "upnpcommands.h"
#include "upnperrors.h"

#include <cstdio>
#include <cstring>

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
		m_LastError = "UPnP discover failed (no IGD / UPnP disabled?) err=" + std::to_string(error);
		Log("UpnpPortMap: " + m_LastError);
		return false;
	}

	UPNPUrls urls{};
	IGDdatas data{};
	char lanaddr[64] = {};
	char wanaddr[64] = {};
	const int igd = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr), wanaddr, sizeof(wanaddr));
	freeUPNPDevlist(devlist);
	devlist = nullptr;

	if (igd != 1 && igd != 2)
	{
		m_LastError = "No valid UPnP IGD found (code " + std::to_string(igd) + ")";
		Log("UpnpPortMap: " + m_LastError);
		FreeUPNPUrls(&urls);
		return false;
	}

	m_LanIp = lanaddr;
	m_ControlUrl = urls.controlURL ? urls.controlURL : "";
	m_ServiceType = data.first.servicetype;

	char extIp[64] = {};
	const int ipRc = UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, extIp);
	if (ipRc == UPNPCOMMAND_SUCCESS && extIp[0] != '\0')
		m_ExternalIp = extIp;
	else if (wanaddr[0] != '\0')
		m_ExternalIp = wanaddr;
	else
		m_ExternalIp.clear();

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
		m_LastError = std::string("UPNP_AddPortMapping failed: ") + strupnperror(addRc);
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
		+ " public=" + (m_ExternalIp.empty() ? "?" : m_ExternalIp));

	// Keep control URL strings for Close() — copy before FreeUPNPUrls.
	// m_ControlUrl / m_ServiceType already hold copies.
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
