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
#include "emuleDlg.h"
#include "PPgNotify.h"
#include "Preferences.h"
#include "OtherFunctions.h"
#include "HelpIDs.h"
#include "TextToSpeech.h"
#include "TaskbarNotifier.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CPPgNotify, CPropertyPage)

BEGIN_MESSAGE_MAP(CPPgNotify, CPropertyPage)
	ON_WM_HELPINFO()
	ON_BN_CLICKED(IDC_CB_TBN_NOSOUND, OnBnClickedNoSound)
	ON_BN_CLICKED(IDC_CB_TBN_USESOUND, OnBnClickedUseSound)
	ON_BN_CLICKED(IDC_CB_TBN_USESPEECH, OnBnClickedUseSpeech)
	ON_EN_CHANGE(IDC_EDIT_TBN_WAVFILE, OnSettingsChange)
	ON_BN_CLICKED(IDC_BTN_BROWSE_WAV, OnBnClickedBrowseAudioFile)
	ON_BN_CLICKED(IDC_TEST_NOTIFICATION, OnBnClickedTestNotification)
	ON_BN_CLICKED(IDC_CB_TBN_ONNEWDOWNLOAD, OnSettingsChange)
	ON_BN_CLICKED(IDC_CB_TBN_ONDOWNLOAD, OnSettingsChange)
	ON_BN_CLICKED(IDC_CB_TBN_ONLOG, OnSettingsChange)
	ON_BN_CLICKED(IDC_CB_TBN_ONCHAT, OnBnClickedOnChat)
	ON_BN_CLICKED(IDC_CB_TBN_IMPORTATNT, OnSettingsChange)
	ON_BN_CLICKED(IDC_CB_TBN_POP_ALWAYS, OnSettingsChange)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

CPPgNotify::CPPgNotify()
	: CPreferencesPage(CPPgNotify::IDD)
	, m_icoBrowse()
{
}

void CPPgNotify::DoDataExchange(CDataExchange *pDX)
{
	CPropertyPage::DoDataExchange(pDX);
}

BOOL CPPgNotify::OnInitDialog()
{
	CPropertyPage::OnInitDialog();
	InitWindowStyles(this);

	AddBuddyButton(GetDlgItem(IDC_EDIT_TBN_WAVFILE)->m_hWnd, ::GetDlgItem(m_hWnd, IDC_BTN_BROWSE_WAV));
	InitAttachedBrowseButton(::GetDlgItem(m_hWnd, IDC_BTN_BROWSE_WAV), m_icoBrowse);

	ASSERT(IDC_CB_TBN_NOSOUND < IDC_CB_TBN_USESOUND && IDC_CB_TBN_USESOUND < IDC_CB_TBN_USESPEECH);
	int iBtnID;
	switch (thePrefs.notifierSoundType) {
	case ntfstSoundFile:
		iBtnID = IDC_CB_TBN_USESOUND;
		break;
	case ntfstSpeech:
		iBtnID = IDC_CB_TBN_USESPEECH;
		break;
	default:
		ASSERT(thePrefs.notifierSoundType == ntfstNoSound);
		iBtnID = IDC_CB_TBN_NOSOUND;
	}
	CheckRadioButton(IDC_CB_TBN_NOSOUND, IDC_CB_TBN_USESPEECH, iBtnID);

	CheckDlgButton(IDC_CB_TBN_ONDOWNLOAD, thePrefs.notifierOnDownloadFinished ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_CB_TBN_ONNEWDOWNLOAD, thePrefs.notifierOnNewDownload ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_CB_TBN_ONCHAT, thePrefs.notifierOnChat ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_CB_TBN_ONLOG, thePrefs.notifierOnLog ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_CB_TBN_IMPORTATNT, thePrefs.notifierOnImportantError ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_CB_TBN_POP_ALWAYS, thePrefs.notifierOnEveryChatMsg ? BST_CHECKED : BST_UNCHECKED);

	SetDlgItemText(IDC_EDIT_TBN_WAVFILE, thePrefs.notifierSoundFile);

	UpdateControls();
	Localize();
	ApplyWidePageLayout({ IDC_TEST_NOTIFICATION }, { IDC_EDIT_TBN_WAVFILE, IDC_BTN_BROWSE_WAV });
	MoveControlToRight(IDC_BTN_BROWSE_WAV, 12);
	StretchControlToLeftOf(IDC_EDIT_TBN_WAVFILE, IDC_BTN_BROWSE_WAV, 6);
	InitializePageToolTips({
		{ IDC_CB_TBN_NOSOUND, _T("Disables all notification audio. Pop-up events still happen if their event checkboxes are enabled; only the sound channel is suppressed.") },
		{ IDC_CB_TBN_USESOUND, _T("Plays a WAV file when a notification fires. The selected file should be short and local, because the sound is loaded when the event triggers.") },
		{ IDC_EDIT_TBN_WAVFILE, _T("Path to the WAV file used for sound notifications. Keep it local and stable so notifications do not pause on a missing share or removable drive.") },
		{ IDC_CB_TBN_USESPEECH, _T("Uses the installed speech engine instead of a WAV file. Availability depends on the local SAPI setup; when no speech engine exists, this option is disabled.") },
		{ IDC_CB_TBN_ONDOWNLOAD, _T("Triggers a notifier when a download finishes. This is one of the noisiest events on busy clients, so pair it with a modest sound or no sound at all.") },
		{ IDC_CB_TBN_ONNEWDOWNLOAD, _T("Triggers a notifier when a new download is added. Useful during remote or scripted link injection, but often excessive for manual day-to-day use.") },
		{ IDC_CB_TBN_ONCHAT, _T("Triggers a notifier when a chat arrives. The additional chat-message option below refines whether every incoming message should pop or only session starts.") },
		{ IDC_CB_TBN_IMPORTATNT, _T("Triggers the notifier for important errors and major warnings. This is usually the safest event to keep enabled because it highlights issues that need attention.") },
		{ IDC_TEST_NOTIFICATION, _T("Temporarily applies the current dialog settings and shows a sample notifier so you can validate sound, speech, and popup behavior before saving.") }
	});

	GetDlgItem(IDC_CB_TBN_USESPEECH)->EnableWindow(IsSpeechEngineAvailable());

	return TRUE;  // return TRUE unless you set the focus to the control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CPPgNotify::UpdateControls()
{
	bool b = IsDlgButtonChecked(IDC_CB_TBN_USESOUND) != 0;
	GetDlgItem(IDC_EDIT_TBN_WAVFILE)->EnableWindow(b);
	GetDlgItem(IDC_BTN_BROWSE_WAV)->EnableWindow(b);
	GetDlgItem(IDC_CB_TBN_POP_ALWAYS)->EnableWindow(IsDlgButtonChecked(IDC_CB_TBN_ONCHAT));
}

void CPPgNotify::Localize()
{
	if (m_hWnd) {
		SetWindowText(GetResString(IDS_PW_EKDEV_OPTIONS));
		SetDlgItemText(IDC_CB_TBN_USESOUND, GetResString(IDS_PW_TBN_USESOUND));
		SetDlgItemText(IDC_CB_TBN_NOSOUND, GetResString(IDS_NOSOUND));
		SetDlgItemText(IDC_CB_TBN_ONLOG, GetResString(IDS_PW_TBN_ONLOG));
		SetDlgItemText(IDC_CB_TBN_ONCHAT, GetResString(IDS_PW_TBN_ONCHAT));
		SetDlgItemText(IDC_CB_TBN_POP_ALWAYS, GetResString(IDS_PW_TBN_POP_ALWAYS));
		SetDlgItemText(IDC_CB_TBN_ONDOWNLOAD, GetResString(IDS_PW_TBN_ONDOWNLOAD) + _T(" (*)"));
		SetDlgItemText(IDC_CB_TBN_ONNEWDOWNLOAD, GetResString(IDS_TBN_ONNEWDOWNLOAD));
		SetDlgItemText(IDC_TASKBARNOTIFIER, GetResString(IDS_PW_TASKBARNOTIFIER));
		SetDlgItemText(IDC_CB_TBN_IMPORTATNT, GetResString(IDS_PS_TBN_IMPORTANT) + _T(" (*)"));
		SetDlgItemText(IDC_TBN_OPTIONS, GetResString(IDS_PW_TBN_OPTIONS));
		SetDlgItemText(IDC_CB_TBN_USESPEECH, GetResString(IDS_USESPEECH));
		SetDlgItemText(IDC_TEST_NOTIFICATION, GetResString(IDS_TEST));
	}
}

BOOL CPPgNotify::OnApply()
{
	thePrefs.notifierOnDownloadFinished = IsDlgButtonChecked(IDC_CB_TBN_ONDOWNLOAD) != 0;
	thePrefs.notifierOnNewDownload = IsDlgButtonChecked(IDC_CB_TBN_ONNEWDOWNLOAD) != 0;
	thePrefs.notifierOnChat = IsDlgButtonChecked(IDC_CB_TBN_ONCHAT) != 0;
	thePrefs.notifierOnLog = IsDlgButtonChecked(IDC_CB_TBN_ONLOG) != 0;
	thePrefs.notifierOnImportantError = IsDlgButtonChecked(IDC_CB_TBN_IMPORTATNT) != 0;
	thePrefs.notifierOnEveryChatMsg = IsDlgButtonChecked(IDC_CB_TBN_POP_ALWAYS) != 0;

	ApplyNotifierSoundType();
	if (thePrefs.notifierSoundType != ntfstSpeech)
		ReleaseTTS();

	SetModified(FALSE);
	return CPropertyPage::OnApply();
}

void CPPgNotify::ApplyNotifierSoundType()
{
	GetDlgItemText(IDC_EDIT_TBN_WAVFILE, thePrefs.notifierSoundFile);
	if (IsDlgButtonChecked(IDC_CB_TBN_USESOUND))
		thePrefs.notifierSoundType = ntfstSoundFile;
	else if (IsDlgButtonChecked(IDC_CB_TBN_USESPEECH))
		thePrefs.notifierSoundType = IsSpeechEngineAvailable() ? ntfstSpeech : ntfstNoSound;
	else {
		ASSERT(IsDlgButtonChecked(IDC_CB_TBN_NOSOUND));
		thePrefs.notifierSoundType = ntfstNoSound;
	}
}

void CPPgNotify::OnBnClickedOnChat()
{
	UpdateControls();
	SetModified();
}

void CPPgNotify::OnBnClickedBrowseAudioFile()
{
	CString strWavPath;
	GetDlgItemText(IDC_EDIT_TBN_WAVFILE, strWavPath);
	CString buffer;
	if (DialogBrowseFile(buffer, _T("Audio-Files (*.wav)|*.wav||"), strWavPath)) {
		SetDlgItemText(IDC_EDIT_TBN_WAVFILE, buffer);
		SetModified();
	}
}

void CPPgNotify::OnBnClickedNoSound()
{
	UpdateControls();
	SetModified();
}

void CPPgNotify::OnBnClickedUseSound()
{
	UpdateControls();
	SetModified();
}

void CPPgNotify::OnBnClickedUseSpeech()
{
	UpdateControls();
	SetModified();
}

void CPPgNotify::OnBnClickedTestNotification()
{
	// save current pref settings
	bool bCurNotifyOnImportantError = thePrefs.notifierOnImportantError;
	ENotifierSoundType iCurSoundType = thePrefs.notifierSoundType;
	CString strSoundFile(thePrefs.notifierSoundFile);

	// temporary apply current settings from dialog
	thePrefs.notifierOnImportantError = true;
	ApplyNotifierSoundType();

	// play test notification
	CString strTest;
	strTest.Format(GetResString(IDS_MAIN_READY), (LPCTSTR)theApp.m_strCurVersionLong);
	theApp.emuledlg->ShowNotifier(strTest, TBN_IMPORTANTEVENT);

	// restore pref settings
	thePrefs.notifierSoundFile = strSoundFile;
	thePrefs.notifierSoundType = iCurSoundType;
	thePrefs.notifierOnImportantError = bCurNotifyOnImportantError;
}

void CPPgNotify::OnHelp()
{
	theApp.ShowHelp(eMule_FAQ_Preferences_Notifications);
}

BOOL CPPgNotify::OnCommand(WPARAM wParam, LPARAM lParam)
{
	return (wParam == ID_HELP) ? OnHelpInfo(NULL) : __super::OnCommand(wParam, lParam);
}

BOOL CPPgNotify::OnHelpInfo(HELPINFO*)
{
	OnHelp();
	return TRUE;
}

void CPPgNotify::OnDestroy()
{
	CPropertyPage::OnDestroy();
	if (m_icoBrowse) {
		VERIFY(::DestroyIcon(m_icoBrowse));
		m_icoBrowse = NULL;
	}
}
