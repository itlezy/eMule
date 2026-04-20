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
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "StdAfx.h"
#include "UPnPImplWrapper.h"
#include "UPnPImpl.h"
#include "UPnPImplMiniLib.h"
#include "UPnPImplPcpNatPmp.h"
#include "Preferences.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CUPnPImplWrapper::CUPnPImplWrapper()
{
	switch (thePrefs.GetUPnPBackendMode()) {
		case UPNP_BACKEND_IGD_ONLY:
			m_liAvailable.AddTail(new CUPnPImplMiniLib());
			break;
		case UPNP_BACKEND_PCP_NATPMP_ONLY:
			m_liAvailable.AddTail(new CUPnPImplPcpNatPmp());
			break;
		case UPNP_BACKEND_AUTOMATIC:
		default:
			m_liAvailable.AddTail(new CUPnPImplMiniLib());
			m_liAvailable.AddTail(new CUPnPImplPcpNatPmp());
			break;
	}
	if (m_liAvailable.IsEmpty())
		m_liAvailable.AddTail(new CUPnPImplNone());
	Init();
}

CUPnPImplWrapper::~CUPnPImplWrapper()
{
	while (!m_liAvailable.IsEmpty())
		delete m_liAvailable.RemoveHead();
	while (!m_liUsed.IsEmpty())
		delete m_liUsed.RemoveHead();
	m_pActiveImpl = NULL;
}

void CUPnPImplWrapper::Init()
{
	ASSERT(!m_liAvailable.IsEmpty());
	m_pActiveImpl = m_liAvailable.RemoveHead();
	m_liUsed.AddTail(m_pActiveImpl);
}

void CUPnPImplWrapper::Reset()
{
	while (!m_liUsed.IsEmpty())
		m_liAvailable.AddTail(m_liUsed.RemoveHead());
	Init();
}

bool CUPnPImplWrapper::SwitchImplentation()
{
	if (m_liAvailable.IsEmpty())
		return false;

	m_pActiveImpl = m_liAvailable.RemoveHead();
	m_liUsed.AddTail(m_pActiveImpl);
	return true;
}
