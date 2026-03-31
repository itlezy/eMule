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
#include "emuledlg.h"
#include "SharedFilesWnd.h"
#include "PPgDirectories.h"
#include "otherfunctions.h"
#include "InputBox.h"
#include "Preferences.h"
#include "HelpIDs.h"
#include "UserMsgs.h"
#include "opcodes.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CPPgDirectories, CPropertyPage)

BEGIN_MESSAGE_MAP(CPPgDirectories, CPropertyPage)
	ON_BN_CLICKED(IDC_SELTEMPDIR, OnBnClickedSeltempdir)
	ON_BN_CLICKED(IDC_SELINCDIR, OnBnClickedSelincdir)
	ON_EN_CHANGE(IDC_INCFILES, OnSettingsChange)
	ON_EN_CHANGE(IDC_TEMPFILES, OnSettingsChange)
	ON_BN_CLICKED(IDC_AUTORESCANSHAREDFOLDERS, OnBnClickedAutoRescanSharedFolders)
	ON_BN_CLICKED(IDC_AUTOSHARENEWSUBDIRS, OnSettingsChange)
	ON_EN_CHANGE(IDC_AUTORESCANSHAREDINTERVAL, OnSettingsChange)
	ON_BN_CLICKED(IDC_UNCADD, OnBnClickedAddUNC)
	ON_WM_HELPINFO()
	ON_BN_CLICKED(IDC_SELTEMPDIRADD, OnBnClickedSeltempdiradd)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

CPPgDirectories::CPPgDirectories()
	: CPreferencesPage(CPPgDirectories::IDD)
	, m_icoBrowse()
{
}

void CPPgDirectories::DoDataExchange(CDataExchange *pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SHARESELECTOR, m_ShareSelector);
}

BOOL CPPgDirectories::OnInitDialog()
{
	CWaitCursor curWait; // initialization of that dialog may take a while
	CPropertyPage::OnInitDialog();
	InitWindowStyles(this);

	CWnd *pIncomingWnd = GetDlgItem(IDC_INCFILES);
	CWnd *pTempWnd = GetDlgItem(IDC_TEMPFILES);
	HWND hSelIncDirWnd = ::GetDlgItem(m_hWnd, IDC_SELINCDIR);
	HWND hSelTempDirWnd = ::GetDlgItem(m_hWnd, IDC_SELTEMPDIR);

	/** Guard dialog-template controls before wiring edit limits and buddy buttons. */
	if (CEdit *pIncomingEdit = static_cast<CEdit*>(pIncomingWnd))
		pIncomingEdit->SetLimitText(MAX_PATH);
	if (pIncomingWnd != NULL && hSelIncDirWnd != NULL)
		AddBuddyButton(pIncomingWnd->m_hWnd, hSelIncDirWnd);
	if (hSelIncDirWnd != NULL)
		InitAttachedBrowseButton(hSelIncDirWnd, m_icoBrowse);

	if (pTempWnd != NULL && hSelTempDirWnd != NULL)
		AddBuddyButton(pTempWnd->m_hWnd, hSelTempDirWnd);
	if (hSelTempDirWnd != NULL)
		InitAttachedBrowseButton(hSelTempDirWnd, m_icoBrowse);

	if (CWnd *pTempDirAddWnd = GetDlgItem(IDC_SELTEMPDIRADD))
		pTempDirAddWnd->ShowWindow(thePrefs.IsExtControlsEnabled() ? SW_SHOW : SW_HIDE);

	LoadSettings();
	Localize();
	UpdateAutoRescanControls();
	ApplyWidePageLayout({ IDC_SELINCDIR, IDC_SELTEMPDIR, IDC_SELTEMPDIRADD, IDC_UNCADD });
	InitializePageToolTips({
		{ IDC_INCFILES, _T("Completed downloads are moved here and shared from here. Choosing a non-empty folder means every valid file already inside becomes part of your shared set.") },
		{ IDC_TEMPFILES, _T("Temporary download storage. Multiple folders can be separated with the '|' character. Keep them on fast, stable local disks and never reuse the incoming directory here.") },
		{ IDC_SHARESELECTOR, _T("Tree of shared folders. Tick only locations you intentionally publish; fixed shares survive rescans, and unavailable paths are handled according to the advanced tweak settings.") },
		{ IDC_AUTORESCANSHAREDFOLDERS, _T("Automatically rescans shared folders for added or removed files. Useful for import-heavy folders, but each rescan costs disk I/O on large trees.") },
		{ IDC_AUTORESCANSHAREDINTERVAL, _T("Seconds between automatic shared-folder rescans. The value must stay reasonably high so the scanner does not fight with active transfers or the disk cache.") },
		{ IDC_AUTOSHARENEWSUBDIRS, _T("Extends a shared parent folder to new child folders created later. Convenient for structured libraries, but it can also expose folders you forgot would be created under that tree.") }
	});

	return TRUE;  // return TRUE unless you set the focus to the control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CPPgDirectories::LoadSettings()
{
	SetDlgItemText(IDC_INCFILES, thePrefs.m_strIncomingDir);

	CString tempfolders;
	for (INT_PTR i = 0; i < thePrefs.GetTempDirCount(); ++i) {
		if (i > 0)
			tempfolders += _T('|');
		tempfolders += thePrefs.GetTempDir(i);
	}
	SetDlgItemText(IDC_TEMPFILES, tempfolders);

	m_ShareSelector.SetSharedDirectories(thePrefs.shareddir_list);
	CheckDlgButton(IDC_AUTORESCANSHAREDFOLDERS, thePrefs.IsAutoRescanSharedFolders() ? BST_CHECKED : BST_UNCHECKED);
	SetDlgItemInt(IDC_AUTORESCANSHAREDINTERVAL, thePrefs.GetAutoRescanSharedFoldersIntervalSec(), FALSE);
	CheckDlgButton(IDC_AUTOSHARENEWSUBDIRS, thePrefs.IsAutoShareNewSharedSubdirs() ? BST_CHECKED : BST_UNCHECKED);
}

void CPPgDirectories::UpdateAutoRescanControls()
{
	const BOOL bEnableInterval = (IsDlgButtonChecked(IDC_AUTORESCANSHAREDFOLDERS) == BST_CHECKED);
	if (CWnd *pIntervalLabelWnd = GetDlgItem(IDC_AUTORESCANSHAREDINTERVALLBL))
		pIntervalLabelWnd->EnableWindow(bEnableInterval);
	if (CWnd *pIntervalWnd = GetDlgItem(IDC_AUTORESCANSHAREDINTERVAL))
		pIntervalWnd->EnableWindow(bEnableInterval);
}

void CPPgDirectories::OnBnClickedSelincdir()
{
	TCHAR buffer[MAX_PATH];
	buffer[GetDlgItemText(IDC_INCFILES, buffer, MAX_PATH)] = _T('\0');
	if (SelectDir(GetSafeHwnd(), buffer, GetResString(IDS_SELECT_INCOMINGDIR)))
		SetDlgItemText(IDC_INCFILES, buffer);
}

void CPPgDirectories::OnBnClickedSeltempdir()
{
	TCHAR buffer[MAX_PATH];
	buffer[GetDlgItemText(IDC_TEMPFILES, buffer, MAX_PATH)] = _T('\0');
	if (SelectDir(GetSafeHwnd(), buffer, GetResString(IDS_SELECT_TEMPDIR)))
		SetDlgItemText(IDC_TEMPFILES, buffer);
}

BOOL CPPgDirectories::OnApply()
{
	CString strIncomingDir;
	GetDlgItemText(IDC_INCFILES, strIncomingDir);
	MakeFoldername(strIncomingDir);
	if (strIncomingDir.IsEmpty()) {
		strIncomingDir = thePrefs.GetDefaultDirectory(EMULE_INCOMINGDIR, true); // will create the directory here if it doesn't exist
		SetDlgItemText(IDC_INCFILES, strIncomingDir);
	} else if (thePrefs.IsInstallationDirectory(strIncomingDir)) {
		ErrorBalloon(IDC_INCFILES, IDS_WRN_INCFILE_RESERVED);
		return FALSE;
	}

	const CString &sOldIncoming(thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR));
	if (strIncomingDir.CompareNoCase(sOldIncoming) != 0 && strIncomingDir.CompareNoCase(thePrefs.GetDefaultDirectory(EMULE_INCOMINGDIR, false)) != 0) {
		// if the user chooses a non-default directory which already contains files,
		// inform him that all those files will be shared
		bool bExistingFile = false;
		CFileFind ff;
		for (BOOL bFound = ff.FindFile(strIncomingDir + _T('*')); bFound && !bExistingFile;) {
			bFound = ff.FindNextFile();
			if (ff.IsDirectory() || ff.IsSystem() || ff.IsTemporary() || ff.GetLength() == 0 || ff.GetLength() > MAX_EMULE_FILE_SIZE)
				continue;

			if (ExtensionIs(ff.GetFileName(), _T(".lnk")))
				continue;

			// ignore real THUMBS.DB files -- seems that lot of ppl have 'thumbs.db' files without the 'System' file attribute
			bExistingFile = (ff.GetFileName().CompareNoCase(_T("thumbs.db")) != 0);
		}
		if (bExistingFile && LocMessageBox(IDS_WRN_INCFILE_EXISTS, MB_OKCANCEL | MB_ICONINFORMATION, 0) == IDCANCEL)
			return FALSE;
	}

	// checking specified tempdir(s)
	CString strTempDir;
	GetDlgItemText(IDC_TEMPFILES, strTempDir);
	if (strTempDir.IsEmpty()) {
		strTempDir = thePrefs.GetDefaultDirectory(EMULE_TEMPDIR, true); // will create the directory if it doesn't exist
		SetDlgItemText(IDC_TEMPFILES, strTempDir);
	}

	bool testtempdirchanged = false;
	CStringArray temptempfolders;
	for (int iPos = 0; iPos >= 0;) {
		CString atmp(strTempDir.Tokenize(_T("|"), iPos));
		if (atmp.Trim().IsEmpty())
			continue;
		UINT uid;
		if (EqualPaths(strIncomingDir, atmp))
			uid = IDS_WRN_INCTEMP_SAME;
		else if (thePrefs.IsInstallationDirectory(atmp))
			uid = IDS_WRN_TEMPFILES_RESERVED;
		else
			uid = 0;
		if (uid) {
			ErrorBalloon(IDC_TEMPFILES, uid);
			return FALSE;
		}

		bool bDup = false;
		for (INT_PTR i = temptempfolders.GetCount(); --i >= 0;)	// avoid duplicate tempdirs
			if (atmp.CompareNoCase(temptempfolders[i]) == 0) {
				bDup = true;
				break;
			}

		if (!bDup) {
			temptempfolders.Add(atmp);
			if (thePrefs.GetTempDirCount() < temptempfolders.GetCount()
				|| atmp.CompareNoCase(thePrefs.GetTempDir(temptempfolders.GetCount() - 1)) != 0)
			{
				testtempdirchanged = true;
			}
		}
	}

	if (temptempfolders.IsEmpty())
		temptempfolders.Add(thePrefs.GetDefaultDirectory(EMULE_TEMPDIR, true));

	if (temptempfolders.GetCount() != thePrefs.GetTempDirCount())
		testtempdirchanged = true;

	// applying tempdirs
	if (testtempdirchanged) {
		thePrefs.tempdir.RemoveAll();
		for (INT_PTR i = 0; i < temptempfolders.GetCount(); ++i) {
			CString toadd(temptempfolders[i]);
			MakeFoldername(toadd);
			if (!::PathFileExists(toadd))
				::CreateDirectory(toadd, NULL);
			if (::PathFileExists(toadd))
				thePrefs.tempdir.Add(toadd);
		}
	}
	if (thePrefs.tempdir.IsEmpty())
		thePrefs.tempdir.Add(thePrefs.GetDefaultDirectory(EMULE_TEMPDIR, true));

	thePrefs.m_strIncomingDir = strIncomingDir;
	MakeFoldername(thePrefs.m_strIncomingDir);

	BOOL bTranslated = FALSE;
	const UINT uAutoRescanInterval = GetDlgItemInt(IDC_AUTORESCANSHAREDINTERVAL, &bTranslated, FALSE);
	if (IsDlgButtonChecked(IDC_AUTORESCANSHAREDFOLDERS) == BST_CHECKED) {
		if (!bTranslated || uAutoRescanInterval < 600) {
			ErrorBalloon(IDC_AUTORESCANSHAREDINTERVAL, IDS_ERR_SHARED_AUTORESCAN_INTERVAL);
			return FALSE;
		}
	}
	thePrefs.SetAutoRescanSharedFolders(IsDlgButtonChecked(IDC_AUTORESCANSHAREDFOLDERS) == BST_CHECKED);
	thePrefs.SetAutoRescanSharedFoldersIntervalSec(bTranslated ? uAutoRescanInterval : 1200);
	thePrefs.SetAutoShareNewSharedSubdirs(IsDlgButtonChecked(IDC_AUTOSHARENEWSUBDIRS) == BST_CHECKED);

	thePrefs.shareddir_list.RemoveAll();
	m_ShareSelector.GetSharedDirectories(thePrefs.shareddir_list);

	// check shared directories for reserved folder names
	for (POSITION pos = thePrefs.shareddir_list.GetHeadPosition(); pos != NULL;) {
		POSITION posLast = pos;
		if (!thePrefs.IsShareableDirectory(thePrefs.shareddir_list.GetNext(pos)))
			thePrefs.shareddir_list.RemoveAt(posLast);
	}

	// on changing incoming dir, update directories for categories with the same path
	if (sOldIncoming.CompareNoCase(thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR)) != 0) {
		thePrefs.GetCategory(0)->strIncomingPath = thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR);
		bool bAskedOnce = false;
		for (INT_PTR cat = thePrefs.GetCatCount(); --cat > 0;) { //skip 0
			const CString &oldpath(thePrefs.GetCatPath(cat));
			if (oldpath.Left(sOldIncoming.GetLength()).CompareNoCase(sOldIncoming) == 0) {
				if (!bAskedOnce) {
					bAskedOnce = true;
					if (LocMessageBox(IDS_UPDATECATINCOMINGDIRS, MB_YESNO, 0) == IDNO)
						break;
				}
				thePrefs.GetCategory(cat)->strIncomingPath = thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR) + oldpath.Mid(sOldIncoming.GetLength());
			}
		}
		thePrefs.SaveCats();
	}


	if (testtempdirchanged)
		LocMessageBox(IDS_SETTINGCHANGED_RESTART, MB_OK, 0);

	theApp.emuledlg->sharedfileswnd->Reload();

	SetModified(0);
	return CPropertyPage::OnApply();
}

BOOL CPPgDirectories::OnCommand(WPARAM wParam, LPARAM lParam)
{
	if (wParam == UM_ITEMSTATECHANGED)
		SetModified();
	else if (wParam == ID_HELP) {
		OnHelp();
		return TRUE;
	}
	return CPropertyPage::OnCommand(wParam, lParam);
}

void CPPgDirectories::OnBnClickedAutoRescanSharedFolders()
{
	UpdateAutoRescanControls();
	SetModified();
}

void CPPgDirectories::Localize()
{
	if (m_hWnd) {
		SetWindowText(GetResString(IDS_PW_DIR));

		SetDlgItemText(IDC_INCOMING_FRM, GetResString(IDS_PW_INCOMING));
		SetDlgItemText(IDC_TEMP_FRM, GetResString(IDS_PW_TEMP));
		SetDlgItemText(IDC_SHARED_FRM, GetResString(IDS_PW_SHARED));
		SetDlgItemText(IDC_AUTORESCANSHAREDFOLDERS, GetResString(IDS_SHARED_AUTORESCAN));
		SetDlgItemText(IDC_AUTORESCANSHAREDINTERVALLBL, GetResString(IDS_SHARED_AUTORESCAN_INTERVAL));
		SetDlgItemText(IDC_AUTOSHARENEWSUBDIRS, GetResString(IDS_SHARED_AUTOSUBDIRS));
	}
}

void CPPgDirectories::OnBnClickedAddUNC()
{
	InputBox inputbox;
	inputbox.SetLabels(GetResString(IDS_UNCFOLDERS), GetResString(IDS_UNCFOLDERS), _T("\\\\Server\\Share"));
	if (inputbox.DoModal() != IDOK)
		return;
	CString unc(inputbox.GetInput());

	// basic UNC check
	if (!::PathIsUNC(unc)) {
		LocMessageBox(IDS_ERR_BADUNC, MB_ICONERROR, 0);
		return;
	}
	slosh(unc);

	for (POSITION pos = thePrefs.shareddir_list.GetHeadPosition(); pos != NULL;)
		if (EqualPaths(thePrefs.shareddir_list.GetNext(pos), unc))
			return;

	if (m_ShareSelector.AddUNCShare(unc))
		SetModified();
}

void CPPgDirectories::OnHelp()
{
	theApp.ShowHelp(eMule_FAQ_Preferences_Directories);
}

BOOL CPPgDirectories::OnHelpInfo(HELPINFO*)
{
	OnHelp();
	return TRUE;
}

void CPPgDirectories::OnBnClickedSeltempdiradd()
{
	CString paths;
	GetDlgItemText(IDC_TEMPFILES, paths);

	TCHAR buffer[MAX_PATH];
	//GetDlgItemText(IDC_TEMPFILES, buffer, _countof(buffer));

	if (SelectDir(GetSafeHwnd(), buffer, GetResString(IDS_SELECT_TEMPDIR))) {
		paths.AppendFormat(_T("|%s"), (LPCTSTR)buffer);
		SetDlgItemText(IDC_TEMPFILES, paths);
	}
}

void CPPgDirectories::OnDestroy()
{
	CPropertyPage::OnDestroy();
	if (m_icoBrowse) {
		VERIFY(::DestroyIcon(m_icoBrowse));
		m_icoBrowse = NULL;
	}
}

void CPPgDirectories::ErrorBalloon(int iEdit, UINT uid)
{
	if (CEdit *pEdit = static_cast<CEdit*>(GetDlgItem(iEdit)))
		pEdit->ShowBalloonTip(GetResString(IDS_ERROR), GetResString(uid), TTI_ERROR);
}
