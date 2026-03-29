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
#include "stdafx.h"
#include "emule.h"
#include "PreferencesDlg.h"
#include "Wizard.h"
#include "emuledlg.h"
#include "StatisticsDlg.h"
#include "opcodes.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

namespace
{
	enum
	{
		CONNECTION_PROFILE_CUSTOM = 0
	};

	struct SConnectionProfile
	{
		LPCTSTR pszName;
		UINT nDownloadKBitPerSec;
		UINT nUploadKBitPerSec;
	};

	/** Generic connection tiers used by the connection wizard. */
	static const SConnectionProfile _aConnectionProfiles[] =
	{
		{ NULL, 0u, 0u },
		{ _T("Mobile / Fixed Wireless"), 50000u, 10000u },
		{ _T("DSL"), 25000u, 5000u },
		{ _T("VDSL"), 100000u, 40000u },
		{ _T("Cable"), 250000u, 25000u },
		{ _T("Cable Gigabit"), 1000000u, 50000u },
		{ _T("Fiber 300"), 300000u, 300000u },
		{ _T("Fiber Gigabit"), 1000000u, 1000000u },
		{ _T("Fiber Multi-Gig"), 2500000u, 2500000u }
	};

	/** Formats profile speeds in Mbit/s for the preset list. */
	CString FormatProfileRate(UINT nKBitPerSec)
	{
		CString strRate;
		strRate.Format(_T("%u"), nKBitPerSec / 1000u);
		return strRate;
	}
}


// CConnectionWizardDlg dialog

IMPLEMENT_DYNAMIC(CConnectionWizardDlg, CDialog)

BEGIN_MESSAGE_MAP(CConnectionWizardDlg, CDialog)
	ON_BN_CLICKED(IDC_WIZ_APPLY_BUTTON, OnBnClickedApply)
	ON_BN_CLICKED(IDC_WIZ_CANCEL_BUTTON, OnBnClickedCancel)
	ON_BN_CLICKED(IDC_WIZ_LOWDOWN_RADIO, OnBnClickedWizLowdownloadRadio)
	ON_BN_CLICKED(IDC_WIZ_MEDIUMDOWN_RADIO, OnBnClickedWizMediumdownloadRadio)
	ON_BN_CLICKED(IDC_WIZ_HIGHDOWN_RADIO, OnBnClickedWizHighdownloadRadio)
	ON_NOTIFY(NM_CLICK, IDC_PROVIDERS, OnNmClickProviders)
END_MESSAGE_MAP()

CConnectionWizardDlg::CConnectionWizardDlg(CWnd *pParent /*=NULL*/)
	: CDialog(CConnectionWizardDlg::IDD, pParent)
	, m_icoWnd()
	, m_iTotalDownload()
{
}

CConnectionWizardDlg::~CConnectionWizardDlg()
{
	if (m_icoWnd)
		VERIFY(::DestroyIcon(m_icoWnd));
}

void CConnectionWizardDlg::DoDataExchange(CDataExchange *pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PROVIDERS, m_profileList);
	DDX_Radio(pDX, IDC_WIZ_LOWDOWN_RADIO, m_iTotalDownload);
}

/** Converts the stored KB/s preference values into the wizard's kbit/s inputs. */
UINT CConnectionWizardDlg::GetRateInKBitsPerSec(uint32 nRateInKBytesPerSec)
{
	return ((UINT)nRateInKBytesPerSec * 1024u + 500u) / 1000u * 8u;
}

/** Reuses the current max-sources setting to preselect the download activity profile. */
int CConnectionWizardDlg::GetDefaultConcurrentDownloadPreset()
{
	const UINT nMaxSourcesPerFile = thePrefs.GetMaxSourcePerFileDefault();
	if (nMaxSourcesPerFile >= 600u)
		return 2;
	if (nMaxSourcesPerFile >= 200u)
		return 1;
	return 0;
}

/** Populates the manual bandwidth fields in the units currently shown by the dialog. */
void CConnectionWizardDlg::SetBandwidthInputs(UINT nDownloadKBitPerSec, UINT nUploadKBitPerSec)
{
	SetDlgItemInt(IDC_WIZ_TRUEDOWNLOAD_BOX, nDownloadKBitPerSec, FALSE);
	SetDlgItemInt(IDC_WIZ_TRUEUPLOAD_BOX, nUploadKBitPerSec, FALSE);
}

/** Builds the modern generic speed-tier list for the connection wizard. */
void CConnectionWizardDlg::InitProfileList()
{
	m_profileList.InsertColumn(0, GetResString(IDS_TYPE), LVCFMT_LEFT, 138);
	m_profileList.InsertColumn(1, GetResString(IDS_WIZ_DOWN), LVCFMT_RIGHT, 42);
	m_profileList.InsertColumn(2, GetResString(IDS_WIZ_UP), LVCFMT_RIGHT, 42);
	m_profileList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	m_profileList.InsertItem(CONNECTION_PROFILE_CUSTOM, GetResString(IDS_WIZARD_CUSTOM));
	m_profileList.SetItemText(CONNECTION_PROFILE_CUSTOM, 1, GetResString(IDS_WIZARD_ENTERBELOW));
	m_profileList.SetItemText(CONNECTION_PROFILE_CUSTOM, 2, GetResString(IDS_WIZARD_ENTERBELOW));

	for (int i = 1; i < _countof(_aConnectionProfiles); ++i) {
		m_profileList.InsertItem(i, _aConnectionProfiles[i].pszName);
		m_profileList.SetItemText(i, 1, FormatProfileRate(_aConnectionProfiles[i].nDownloadKBitPerSec));
		m_profileList.SetItemText(i, 2, FormatProfileRate(_aConnectionProfiles[i].nUploadKBitPerSec));
	}

	m_profileList.SetSelectionMark(CONNECTION_PROFILE_CUSTOM);
	m_profileList.SetItemState(CONNECTION_PROFILE_CUSTOM, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
}

/** Loads either the manual values or the selected preset into the wizard fields. */
void CConnectionWizardDlg::LoadSelectedProfile()
{
	const int iSelection = m_profileList.GetNextItem(-1, LVNI_SELECTED);
	if (iSelection == CONNECTION_PROFILE_CUSTOM || iSelection < 0 || iSelection >= _countof(_aConnectionProfiles)) {
		SetBandwidthInputs(
			GetRateInKBitsPerSec(thePrefs.GetMaxGraphDownloadRate()),
			GetRateInKBitsPerSec(thePrefs.GetMaxGraphUploadRate(true)));
		CheckRadioButton(IDC_KBITS, IDC_KBYTES, IDC_KBITS);
	} else {
		SetBandwidthInputs(_aConnectionProfiles[iSelection].nDownloadKBitPerSec, _aConnectionProfiles[iSelection].nUploadKBitPerSec);
		CheckRadioButton(IDC_KBITS, IDC_KBYTES, IDC_KBITS);
	}
}

void CConnectionWizardDlg::OnBnClickedApply()
{
	UINT download = GetDlgItemInt(IDC_WIZ_TRUEDOWNLOAD_BOX, NULL, TRUE);
	if (download <= 0) {
		GetDlgItem(IDC_WIZ_TRUEDOWNLOAD_BOX)->SetFocus();
		return;
	}

	UINT upload = GetDlgItemInt(IDC_WIZ_TRUEUPLOAD_BOX, NULL, TRUE);
	if (upload <= 0) {
		GetDlgItem(IDC_WIZ_TRUEUPLOAD_BOX)->SetFocus();
		return;
	}

	if (IsDlgButtonChecked(IDC_KBITS)) {
		upload = (upload / 8 * 1000 + 512) / 1024;
		download = (download / 8 * 1000 + 512) / 1024;
	} else {
		upload = (upload * 1000 + 512) / 1024;
		download = (download * 1000 + 512) / 1024;
	}

	thePrefs.maxGraphDownloadRate = download;
	thePrefs.maxGraphUploadRate = upload;

	if (upload > 0 && download > 0) {
		thePrefs.m_maxupload = upload * 4 / 5;
		if (upload < 4 && download > upload * 3) {
			thePrefs.m_maxdownload = thePrefs.m_maxupload * 3;
			download = upload * 3;
		} else if (upload < 10 && download > upload * 4) {
			thePrefs.m_maxdownload = thePrefs.m_maxupload * 4;
			download = upload * 4;
		} else if (upload < 20 && download > upload * 5) {
			thePrefs.m_maxdownload = thePrefs.m_maxupload * 5;
			download = upload * 5;
		} else
			thePrefs.m_maxdownload = download * 9 / 10;

		theApp.emuledlg->statisticswnd->SetARange(false, thePrefs.maxGraphUploadRate);
		theApp.emuledlg->statisticswnd->SetARange(true, thePrefs.maxGraphDownloadRate);

		/** Keep the legacy tuning heuristic, but extend it for modern upstream speeds. */
		if (upload <= 7)
			thePrefs.maxconnections = 80;
		else if (upload < 12)
			thePrefs.maxconnections = 200;
		else if (upload < 25)
			thePrefs.maxconnections = 400;
		else if (upload < 50)
			thePrefs.maxconnections = 600;
		else if (upload < 125)
			thePrefs.maxconnections = 800;
		else
			thePrefs.maxconnections = 1000;

		/** Preserve the existing per-file source heuristic driven by bandwidth and activity. */
		static const UINT srcperfile[5][3] =
		{
			{ 100u,  60u,  40u},
			{ 300u, 200u, 100u},
			{ 500u, 400u, 350u},
			{ 800u, 600u, 400u},
			{1000u, 750u, 500u}
		};

		int i;
		if (download <= 7)
			i = 0;
		else if (download < 62)
			i = 1;
		else if (download < 187)
			i = 2;
		else if (download <= 312)
			i = 3;
		else
			i = 4;

		thePrefs.maxsourceperfile = srcperfile[i][m_iTotalDownload];
	}
	theApp.emuledlg->preferenceswnd->m_wndConnection.LoadSettings();
	CDialog::OnOK();
}

void CConnectionWizardDlg::OnBnClickedCancel()
{
	CDialog::OnCancel();
}

void CConnectionWizardDlg::OnBnClickedWizLowdownloadRadio()
{
	m_iTotalDownload = 0;
}

void CConnectionWizardDlg::OnBnClickedWizMediumdownloadRadio()
{
	m_iTotalDownload = 1;
}

void CConnectionWizardDlg::OnBnClickedWizHighdownloadRadio()
{
	m_iTotalDownload = 2;
}

BOOL CConnectionWizardDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	InitWindowStyles(this);

	SetIcon(m_icoWnd = theApp.LoadIcon(_T("Wizard")), FALSE);

	m_iTotalDownload = GetDefaultConcurrentDownloadPreset();
	CheckRadioButton(IDC_WIZ_LOWDOWN_RADIO, IDC_WIZ_HIGHDOWN_RADIO, IDC_WIZ_LOWDOWN_RADIO + m_iTotalDownload);
	CheckRadioButton(IDC_KBITS, IDC_KBYTES, IDC_KBITS);

	InitProfileList();
	LoadSelectedProfile();
	SetCustomItemsActivation();

	Localize();

	return TRUE;
}

void CConnectionWizardDlg::OnNmClickProviders(LPNMHDR, LRESULT *pResult)
{
	LoadSelectedProfile();
	SetCustomItemsActivation();
	*pResult = 0;
}

void CConnectionWizardDlg::Localize()
{
	SetWindowText(GetResString(IDS_CONNECTION) + _T(" ") + GetResString(IDS_PRESETS));
	SetDlgItemText(IDC_WIZ_CONCURENTDOWN_FRAME, GetResString(IDS_CONCURDWL));
	SetDlgItemText(IDC_WIZ_HOTBUTTON_FRAME, GetResString(IDS_WIZ_CTFRAME));
	SetDlgItemText(IDC_WIZ_TRUEUPLOAD_TEXT, GetResString(IDS_WIZ_TRUEUPLOAD_TEXT));
	SetDlgItemText(IDC_WIZ_TRUEDOWNLOAD_TEXT, GetResString(IDS_WIZ_TRUEDOWNLOAD_TEXT));
	SetDlgItemText(IDC_KBITS, GetResString(IDS_KBITSSEC));
	SetDlgItemText(IDC_KBYTES, GetResString(IDS_KBYTESSEC));
	SetDlgItemText(IDC_WIZ_APPLY_BUTTON, GetResString(IDS_PW_APPLY));
	SetDlgItemText(IDC_WIZ_CANCEL_BUTTON, GetResString(IDS_CANCEL));
}

void CConnectionWizardDlg::SetCustomItemsActivation()
{
	BOOL bActive = (m_profileList.GetNextItem(-1, LVNI_SELECTED) == CONNECTION_PROFILE_CUSTOM);
	GetDlgItem(IDC_WIZ_TRUEUPLOAD_BOX)->EnableWindow(bActive);
	GetDlgItem(IDC_WIZ_TRUEDOWNLOAD_BOX)->EnableWindow(bActive);
	GetDlgItem(IDC_KBITS)->EnableWindow(bActive);
	GetDlgItem(IDC_KBYTES)->EnableWindow(bActive);
}
