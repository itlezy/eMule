//this file is part of eMule
//Copyright (C)2004-2026 Merkur ( devs@emule-project.net / https://www.emule-project.net )
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
#include <comdef.h>
#include <Iads.h>

#define LOGON_WITH_PROFILE			0x00000001
#define LOGON_NETCREDENTIALS_ONLY	0x00000002

enum eResult
{
	RES_OK_NEED_RESTART = 0,
	RES_OK,
	RES_FAILED
};

typedef HRESULT(WINAPI *TADsGetObject)
(
	LPWSTR	lpszPathName,
	REFIID	iid,
	LPVOID	*ppObject
);

typedef HRESULT(WINAPI *TADsBuildEnumerator)
(
	IADsContainer *pADsContainer,
	IEnumVARIANT **ppEnumVariant
);

typedef HRESULT(WINAPI *TADsEnumerateNext)
(
	IEnumVARIANT *pEnumVariant,
	ULONG cElements,
	VARIANT *pvar,
	ULONG *pcElementsFetched
);

typedef _com_ptr_t<_com_IIID<IADsContainer, &IID_IADsContainer> >  IADsContainerPtr;
typedef _com_ptr_t<_com_IIID<IADs, &IID_IADs> >  IADsPtr;
typedef _com_ptr_t<_com_IIID<IADsUser, &IID_IADsUser> >  IADsUserPtr;
typedef _com_ptr_t<_com_IIID<IADsAccessControlEntry, &IID_IADsAccessControlEntry> >  IIADsAccessControlEntryPtr;
typedef _com_ptr_t<_com_IIID<IADsSecurityDescriptor, &IID_IADsSecurityDescriptor> >  IADsSecurityDescriptorPtr;
typedef _com_ptr_t<_com_IIID<IADsWinNTSystemInfo, &IID_IADsWinNTSystemInfo> >  IADsWinNTSystemInfoPtr;
#define MHeapAlloc(x) (::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, x))
#define MHeapFree(x)  (::HeapFree(::GetProcessHeap(), 0, x))

#define EMULEACCOUNTW L"eMule_Secure"

class CSecRunAsUser
{
public:
	CSecRunAsUser();
	~CSecRunAsUser();

	eResult	RestartSecure();
	bool	IsRunningEmuleAccount() const	{ return m_bRunningAsEmule; }
	bool	IsRunningRestricted() const		{ return m_bRunningRestricted; }
	bool	IsRunningSecure() const			{ return m_bRunningRestricted || m_bRunningAsEmule; }
	CStringW	GetCurrentUserW() const;

protected:
	eResult	PrepareUser();
	eResult	RestartAsUser();
	eResult	RestartAsRestricted();

	bool	SetDirectoryPermissions();
	bool	CreateEmuleUser(IADsContainerPtr pUsers);
	static CStringW	CreateRandomPW();
	bool	SetObjectPermission(const CString &strDirFile, DWORD lGrantedAccess);
	bool	LoadAPI();
	void	FreeAPI();

private:
	CStringW ADSPath;
	CStringW m_strPassword;
	CStringW m_strDomain;
	CStringW m_strCurrentUser;
	HMODULE m_hACTIVEDS_DLL;
	bool m_bRunningAsEmule;
	bool m_bRunningRestricted;

	TADsGetObject ADsGetObject;
	TADsBuildEnumerator ADsBuildEnumerator;
	TADsEnumerateNext ADsEnumerateNext;
};
