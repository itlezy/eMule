//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#pragma once

#include "types.h"

/**
 * Reproduces the legacy assumption that a connected server session always has a
 * dereferenceable current-server snapshot.
 */
inline bool HasConnectedServerSnapshot(bool bIsConnected, const void *pCurrentServer)
{
	(void)pCurrentServer;
	return bIsConnected;
}

/**
 * Reproduces the legacy capability gate, which only checked the connected
 * state before trusting the current-server capability value.
 */
inline bool HasConnectedServerCapability(bool bIsConnected, const void *pCurrentServer, bool bCapabilityFlag)
{
	(void)pCurrentServer;
	return bIsConnected && bCapabilityFlag;
}

/**
 * Reproduces the legacy server-endpoint match, which trusted the connected
 * state and compared endpoint fields without a separate current-server null guard.
 */
inline bool MatchesConnectedServerEndpoint(bool bIsConnected, const void *pCurrentServer, uint32 nCurrentServerIP, uint16 nCurrentServerPort, uint32 nServerIP, uint16 nServerPort)
{
	(void)pCurrentServer;
	return bIsConnected
		&& nCurrentServerIP == nServerIP
		&& nCurrentServerPort == nServerPort;
}
