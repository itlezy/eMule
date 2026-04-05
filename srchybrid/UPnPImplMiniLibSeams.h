#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>

#define EMULE_TEST_HAVE_UPNP_MINILIB_SEAMS 1

/**
 * @brief Reports whether a queried MiniUPnP mapping already targets the requested LAN IP and internal port.
 */
inline bool DoesMiniUPnPMappingMatchRequest(const char *pachMappedLanIp, const char *pachMappedInternalPort, const char *pachExpectedLanIp, uint16_t nExpectedPort)
{
	if (pachMappedLanIp == NULL || pachMappedInternalPort == NULL || pachExpectedLanIp == NULL)
		return false;
	if (pachMappedLanIp[0] == '\0' || pachExpectedLanIp[0] == '\0')
		return false;
	if (strcmp(pachMappedLanIp, pachExpectedLanIp) != 0)
		return false;

	char achExpectedPort[8] = {};
	if (sprintf_s(achExpectedPort, "%hu", nExpectedPort) <= 0)
		return false;

	return strcmp(pachMappedInternalPort, achExpectedPort) == 0;
}
