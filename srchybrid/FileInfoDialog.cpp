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
#include "stdafx.h"
#include "eMule.h"
#include "FileInfoDialog.h"
#include "OtherFunctions.h"
#include "Log.h"
#include "MediaInfo.h"
#include "PartFile.h"
#include "Preferences.h"
#include "UserMsgs.h"
#include "SplitterControl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// SMediaInfoThreadResult

struct SMediaInfoThreadResult
{
	~SMediaInfoThreadResult()
	{
		delete paMediaInfo;
	}
	CArray<SMediaInfo> *paMediaInfo = NULL;
	CStringA strInfo;
};

/////////////////////////////////////////////////////////////////////////////
// CGetMediaInfoThread

class CGetMediaInfoThread : public CWinThread
{
	DECLARE_DYNCREATE(CGetMediaInfoThread)

protected:
	CGetMediaInfoThread()
		: m_hWndOwner()
		, m_hFont()
	{
	}

public:
	virtual BOOL InitInstance();
	virtual int	Run();
	void SetValues(HWND hWnd, const CSimpleArray<CObject*> *paFiles, HFONT hFont)
	{
		m_hWndOwner = hWnd;
		for (int i = 0; i < paFiles->GetSize(); ++i)
			m_aFiles.Add(static_cast<CShareableFile*>((*paFiles)[i]));
		m_hFont = hFont;
	}

private:
	bool GetMediaInfo(HWND hWndOwner, const CShareableFile *pFile, SMediaInfo *mi, bool bSingleFile);
	CSimpleArray<const CShareableFile*> m_aFiles;
	HWND m_hWndOwner;
	HFONT m_hFont;
};


/////////////////////////////////////////////////////////////////////////////
// CFileInfoDialog dialog

IMPLEMENT_DYNAMIC(CFileInfoDialog, CResizablePage)

BEGIN_MESSAGE_MAP(CFileInfoDialog, CResizablePage)
	ON_MESSAGE(UM_MEDIA_INFO_RESULT, OnMediaInfoResult)
	ON_MESSAGE(UM_DATA_CHANGED, OnDataChanged)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

CFileInfoDialog::CFileInfoDialog()
	: CResizablePage(CFileInfoDialog::IDD)
	, m_paFiles()
	, m_bDataChanged()
	, m_bReducedDlg()
{
	m_strCaption = GetResString(IDS_CONTENT_INFO);
	m_psp.pszTitle = m_strCaption;
	m_psp.dwFlags |= PSP_USETITLE;
}

BOOL CFileInfoDialog::OnInitDialog()
{
	CWaitCursor curWait; // we may get quite busy here.
	CResizablePage::OnInitDialog();
	InitWindowStyles(this);

	if (!m_bReducedDlg) {
		AddAnchor(IDC_FILESIZE, TOP_LEFT, TOP_RIGHT);
		AddAnchor(IDC_FULL_FILE_INFO, TOP_LEFT, BOTTOM_RIGHT);

		m_fi.LimitText(_I32_MAX);
		m_fi.SendMessage(EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(3, 3));
		m_fi.SetAutoURLDetect();
		m_fi.SetEventMask(m_fi.GetEventMask() | ENM_LINK);
	} else {
		GetDlgItem(IDC_FILESIZE)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_FULL_FILE_INFO)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_FD_XI1)->ShowWindow(SW_HIDE);

		CRect rc;
		GetDlgItem(IDC_FILESIZE)->GetWindowRect(rc);
		int nDelta = rc.Height();

		CSplitterControl::ChangeHeight(GetDlgItem(IDC_GENERAL), -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_LENGTH), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_FORMAT), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_FD_XI3), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_VCODEC), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_VBITRATE), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_VWIDTH), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_VASPECT), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_VFPS), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_FD_XI6), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_FD_XI8), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_FD_XI10), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_FD_XI12), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_STATIC_LANGUAGE), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_FD_XI4), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_ACODEC), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_ABITRATE), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_ACHANNEL), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_ASAMPLERATE), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_ALANGUAGE), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_FD_XI5), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_FD_XI9), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_FD_XI7), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_FD_XI14), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_FD_XI13), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_FD_XI2), 0, -nDelta);
		CSplitterControl::ChangePos(GetDlgItem(IDC_STATICFI), 0, -nDelta);
	}

	// General Group
	AddAnchor(IDC_GENERAL, TOP_LEFT, TOP_RIGHT);

	// Video Group
	AddAnchor(IDC_FD_XI3, TOP_LEFT, TOP_CENTER);

	// Audio Group - Labels
	AddAnchor(IDC_FD_XI4, TOP_CENTER, TOP_RIGHT);
	AddAnchor(IDC_FD_XI6, TOP_CENTER, TOP_CENTER);
	AddAnchor(IDC_FD_XI8, TOP_CENTER, TOP_CENTER);
	AddAnchor(IDC_FD_XI10, TOP_CENTER, TOP_CENTER);
	AddAnchor(IDC_FD_XI12, TOP_CENTER, TOP_CENTER);
	AddAnchor(IDC_STATIC_LANGUAGE, TOP_CENTER, TOP_CENTER);

	// Audio Group - Values
	AddAnchor(IDC_ACODEC, TOP_CENTER, TOP_RIGHT);
	AddAnchor(IDC_ABITRATE, TOP_CENTER, TOP_RIGHT);
	AddAnchor(IDC_ACHANNEL, TOP_CENTER, TOP_RIGHT);
	AddAnchor(IDC_ASAMPLERATE, TOP_CENTER, TOP_RIGHT);
	AddAnchor(IDC_ALANGUAGE, TOP_CENTER, TOP_RIGHT);

	AddAllOtherAnchors();

	CResizablePage::UpdateData(FALSE);
	Localize();
	return TRUE;
}

void CFileInfoDialog::OnDestroy()
{
	// This property sheet's window may get destroyed and re-created several times although
	// the corresponding C++ class is kept -> explicitly reset ResizableLib state
	RemoveAllAnchors();

	CResizablePage::OnDestroy();
}

BOOL CFileInfoDialog::OnSetActive()
{
	if (!CResizablePage::OnSetActive())
		return FALSE;
	if (m_bDataChanged) {
		InitDisplay(GetResString(IDS_FSTAT_WAITING));

		CGetMediaInfoThread *pThread = (CGetMediaInfoThread*)AfxBeginThread(RUNTIME_CLASS(CGetMediaInfoThread), THREAD_PRIORITY_LOWEST, 0, CREATE_SUSPENDED);
		if (pThread) {
			pThread->SetValues(m_hWnd, m_paFiles, (HFONT)GetDlgItem(IDC_FD_XI1)->GetFont()->m_hObject);
			pThread->ResumeThread();
		}
		m_pFiles.RemoveAll();
		for (int i = m_paFiles->GetSize(); --i >= 0;)
			m_pFiles.Add((*m_paFiles)[i]);
		m_bDataChanged = false;
	}
	return TRUE;
}

LRESULT CFileInfoDialog::OnDataChanged(WPARAM, LPARAM)
{
	m_bDataChanged = true;
	return 1;
}

IMPLEMENT_DYNCREATE(CGetMediaInfoThread, CWinThread)

BOOL CGetMediaInfoThread::InitInstance()
{
	DbgSetThreadName("GetMediaInfo");
	InitThreadLocale();
	return TRUE;
}

int CGetMediaInfoThread::Run()
{
	(void)::CoInitialize(NULL);

	HWND hwndRE = ::CreateWindow(MSFTEDIT_CLASS, NULL, ES_MULTILINE | ES_READONLY | WS_DISABLED, 0, 0, 200, 200, NULL, NULL, NULL, NULL);
	ASSERT(hwndRE);
	if (hwndRE && m_hFont)
		::SendMessage(hwndRE, WM_SETFONT, (WPARAM)m_hFont, 0);

	CArray<SMediaInfo> *paMediaInfo = NULL;
	try {
		CRichEditStream re;
		re.Attach(hwndRE);
		re.LimitText(_I32_MAX);
		PARAFORMAT pf;
		pf.cbSize = (UINT)sizeof pf;
		if (re.GetParaFormat(pf)) {
			pf.dwMask |= PFM_TABSTOPS;
			pf.cTabCount = 1;
			pf.rgxTabs[0] = 3000;
			re.SetParaFormat(pf);
		}
		re.Detach();

		const int arcnt = m_aFiles.GetSize();
		paMediaInfo = new CArray<SMediaInfo>;
		for (int i = 0; i < arcnt; ++i) {
			SMediaInfo mi;
			mi.bOutputFileName = arcnt > 1;
			mi.strFileName = m_aFiles[i]->GetFileName();
			mi.strInfo.Attach(hwndRE);
			mi.strInfo.InitColors();
			if (!::IsWindow(m_hWndOwner) || !GetMediaInfo(m_hWndOwner, m_aFiles[i], &mi, (arcnt == 1))) {
				mi.strInfo.Detach();
				delete paMediaInfo;
				paMediaInfo = NULL;
				break;
			}
			mi.strInfo.Detach();
			paMediaInfo->Add(mi);
		}
	} catch (...) {
		ASSERT(0);
		delete paMediaInfo;
		paMediaInfo = NULL;
	}

	SMediaInfoThreadResult *pThreadRes = new SMediaInfoThreadResult;
	pThreadRes->paMediaInfo = paMediaInfo;
	CRichEditStream re;
	re.Attach(hwndRE);
	re.GetRTFText(pThreadRes->strInfo);
	re.Detach();
	VERIFY(DestroyWindow(hwndRE));

	// Usage of 'PostMessage': The idea is to post a message to the window in that other
	// thread and never deadlock (because of the post). This is safe, but leads to the problem
	// that we may create memory leaks in case the target window is currently in the process
	// of getting destroyed! E.g. if the target window gets destroyed after we put the message
	// into the queue, we have no chance of determining that and the memory wouldn't get freed.
	//if (!::IsWindow(m_hWndOwner) || !::PostMessage(m_hWndOwner, UM_MEDIA_INFO_RESULT, 0, (LPARAM)pThreadRes))
	//	delete pThreadRes;

	// Usage of 'SendMessage': Using 'SendMessage' seems to be dangerous because of potential
	// deadlocks. Basically it depends on what the target thread/window is currently doing
	// whether there is a risk for a deadlock. However, even with extensive stress testing
	// there does not show any problem. The worse thing which can happen, is that we call
	// 'SendMessage', then the target window gets destroyed (while we are still waiting in
	// 'SendMessage') and would get blocked. Though, this does not happen, it seems that Windows
	// is catching that case internally and lets our 'SendMessage' call return (with a result
	// of '0'). If that happened, the 'IsWindow(m_hWndOwner)' returns FALSE, which positively
	// indicates that the target window was destroyed while we were waiting in 'SendMessage'.
	// So, everything should be fine (with that special scenario) with using 'SendMessage'.
	// Let's be brave. :)
	if (!::IsWindow(m_hWndOwner) || !::SendMessage(m_hWndOwner, UM_MEDIA_INFO_RESULT, 0, (LPARAM)pThreadRes))
		delete pThreadRes;

	::CoUninitialize();
	return 0;
}

void CFileInfoDialog::InitDisplay(LPCTSTR pStr)
{
	SetDlgItemText(IDC_FORMAT, pStr);
	SetDlgItemText(IDC_FILESIZE, pStr);
	SetDlgItemText(IDC_LENGTH, pStr);
	SetDlgItemText(IDC_VCODEC, pStr);
	SetDlgItemText(IDC_VBITRATE, pStr);
	SetDlgItemText(IDC_VWIDTH, pStr);
	SetDlgItemText(IDC_VASPECT, pStr);
	SetDlgItemText(IDC_VFPS, pStr);
	SetDlgItemText(IDC_ACODEC, pStr);
	SetDlgItemText(IDC_ACHANNEL, pStr);
	SetDlgItemText(IDC_ASAMPLERATE, pStr);
	SetDlgItemText(IDC_ABITRATE, pStr);
	SetDlgItemText(IDC_ALANGUAGE, pStr);
	SetDlgItemText(IDC_FULL_FILE_INFO, _T(""));

}

LRESULT CFileInfoDialog::OnMediaInfoResult(WPARAM, LPARAM lParam)
{
	SetDlgItemText(IDC_FD_XI3, GetResString(IDS_VIDEO));
	SetDlgItemText(IDC_FD_XI4, GetResString(IDS_AUDIO));
	InitDisplay(_T("-"));

	SMediaInfoThreadResult *pThreadRes = (SMediaInfoThreadResult*)lParam;
	if (pThreadRes == NULL)
		return 1;
	CArray<SMediaInfo> *paMediaInfo = pThreadRes->paMediaInfo;
	if (paMediaInfo == NULL) {
		delete pThreadRes;
		return 1;
	}

	if (paMediaInfo->GetSize() != m_paFiles->GetSize()) {
		InitDisplay(_T(""));
		delete pThreadRes;
		return 1;
	}

	uint64 uTotalFileSize = 0;
	SMediaInfo ami;
	bool bDiffVideoStreamCount = false;
	bool bDiffVideoCompression = false;
	bool bDiffVideoWidth = false;
	bool bDiffVideoHeight = false;
	bool bDiffVideoFrameRate = false;
	bool bDiffVideoBitRate = false;
	bool bDiffVideoAspectRatio = false;
	bool bDiffAudioStreamCount = false;
	bool bDiffAudioCompression = false;
	bool bDiffAudioChannels = false;
	bool bDiffAudioSamplesPerSec = false;
	bool bDiffAudioAvgBytesPerSec = false;
	bool bDiffAudioLanguage = false;
	for (int i = 0; i < paMediaInfo->GetSize(); ++i) {
		SMediaInfo &mi = (*paMediaInfo)[i];

		mi.InitFileLength();
		uTotalFileSize += (uint64)mi.ulFileSize;
		if (i == 0)
			ami = mi;
		else {
			if (ami.strFileFormat != mi.strFileFormat)
				ami.strFileFormat.Empty();

			if (ami.strMimeType != mi.strMimeType)
				ami.strMimeType.Empty();

			ami.fFileLengthSec += mi.fFileLengthSec;
			ami.bFileLengthEstimated |= mi.bFileLengthEstimated;

			ami.fVideoLengthSec += mi.fVideoLengthSec;
			ami.bVideoLengthEstimated |= mi.bVideoLengthEstimated;
			if (ami.iVideoStreams == 0 && mi.iVideoStreams > 0 || ami.iVideoStreams > 0 && mi.iVideoStreams == 0) {
				if (ami.iVideoStreams == 0)
					ami.iVideoStreams = mi.iVideoStreams;
				bDiffVideoStreamCount = true;
				bDiffVideoCompression = true;
				bDiffVideoWidth = true;
				bDiffVideoHeight = true;
				bDiffVideoFrameRate = true;
				bDiffVideoBitRate = true;
				bDiffVideoAspectRatio = true;
			} else {
				bDiffVideoStreamCount |= (ami.iVideoStreams != mi.iVideoStreams);
				bDiffVideoCompression |= (ami.strVideoFormat != mi.strVideoFormat);
				bDiffVideoWidth |= (ami.video.bmiHeader.biWidth != mi.video.bmiHeader.biWidth);
				bDiffVideoHeight |= (ami.video.bmiHeader.biHeight != mi.video.bmiHeader.biHeight);
				bDiffVideoFrameRate |= (ami.fVideoFrameRate != mi.fVideoFrameRate);
				bDiffVideoBitRate |= (ami.video.dwBitRate != mi.video.dwBitRate);
				bDiffVideoAspectRatio |= (ami.fVideoAspectRatio != mi.fVideoAspectRatio);
			}

			ami.fAudioLengthSec += mi.fAudioLengthSec;
			ami.bAudioLengthEstimated |= mi.bAudioLengthEstimated;
			if (ami.iAudioStreams == 0 && mi.iAudioStreams > 0 || ami.iAudioStreams > 0 && mi.iAudioStreams == 0) {
				if (ami.iAudioStreams == 0)
					ami.iAudioStreams = mi.iAudioStreams;
				bDiffAudioStreamCount = true;
				bDiffAudioCompression = true;
				bDiffAudioChannels = true;
				bDiffAudioSamplesPerSec = true;
				bDiffAudioAvgBytesPerSec = true;
				bDiffAudioLanguage = true;
			} else {
				bDiffAudioStreamCount |= (ami.iAudioStreams != mi.iAudioStreams);
				bDiffAudioCompression |= (ami.strAudioFormat != mi.strAudioFormat);
				bDiffAudioChannels |= (ami.audio.nChannels != mi.audio.nChannels);
				bDiffAudioSamplesPerSec |= (ami.audio.nSamplesPerSec != mi.audio.nSamplesPerSec);
				bDiffAudioAvgBytesPerSec |= (ami.audio.nAvgBytesPerSec != mi.audio.nAvgBytesPerSec);
				bDiffAudioLanguage |= (ami.strAudioLanguage.CompareNoCase(mi.strAudioLanguage) != 0);
			}
		}
	}

	CString buffer(ami.strFileFormat);
	if (!ami.strMimeType.IsEmpty()) {
		if (!buffer.IsEmpty())
			buffer += _T("; MIME type=");
		buffer += ami.strMimeType;
	}
	SetDlgItemText(IDC_FORMAT, buffer);

	if (uTotalFileSize)
		SetDlgItemText(IDC_FILESIZE, CastItoXBytes(uTotalFileSize));

	if (ami.fFileLengthSec) {
		buffer = CastSecondsToHM((time_t)ami.fFileLengthSec);
		if (ami.bFileLengthEstimated)
			buffer.AppendFormat(_T(" (%s)"), (LPCTSTR)GetResString(IDS_ESTIMATED));
		SetDlgItemText(IDC_LENGTH, buffer);
	}

	if (ami.iVideoStreams) {
		if (!bDiffVideoStreamCount && ami.iVideoStreams > 1)
			SetDlgItemText(IDC_FD_XI3, GetResString(IDS_VIDEO) + _T(" #1"));

		CString strVideoFormat;
		if (!bDiffVideoCompression)
			strVideoFormat = ami.strVideoFormat;
		SetDlgItemText(IDC_VCODEC, strVideoFormat);

		if (!bDiffVideoBitRate && ami.video.dwBitRate) {
			if (ami.video.dwBitRate == _UI32_MAX)
				buffer = _T("Variable");
			else
				buffer.Format(_T("%lu %s"), (ami.video.dwBitRate + SEC2MS(1) / 2) / SEC2MS(1), (LPCTSTR)GetResString(IDS_KBITSSEC));
			SetDlgItemText(IDC_VBITRATE, buffer);
		} else
			SetDlgItemText(IDC_VBITRATE, _T(""));

		if (!bDiffVideoWidth && ami.video.bmiHeader.biWidth && !bDiffVideoHeight && ami.video.bmiHeader.biHeight) {
			buffer.Format(_T("%i x %i"), abs(ami.video.bmiHeader.biWidth), abs(ami.video.bmiHeader.biHeight));
			SetDlgItemText(IDC_VWIDTH, buffer);
		} else
			SetDlgItemText(IDC_VWIDTH, _T(""));

		if (!bDiffVideoAspectRatio && ami.fVideoAspectRatio) {
			buffer.Format(_T("%.3f"), ami.fVideoAspectRatio);
			const CString &strAR(GetKnownAspectRatioDisplayString((float)ami.fVideoAspectRatio));
			if (!strAR.IsEmpty())
				buffer.AppendFormat(_T("  (%s)"), (LPCTSTR)strAR);
			SetDlgItemText(IDC_VASPECT, buffer);
		} else
			SetDlgItemText(IDC_VASPECT, _T(""));

		if (!bDiffVideoFrameRate && ami.fVideoFrameRate) {
			buffer.Format(_T("%.2f"), ami.fVideoFrameRate);
			SetDlgItemText(IDC_VFPS, buffer);
		} else
			SetDlgItemText(IDC_VFPS, _T(""));
	}

	if (ami.iAudioStreams) {
		if (!bDiffAudioStreamCount && ami.iAudioStreams > 1)
			SetDlgItemText(IDC_FD_XI4, GetResString(IDS_AUDIO) + _T(" #1"));

		CString strAudioFormat;
		if (!bDiffAudioCompression)
			strAudioFormat = ami.strAudioFormat;
		SetDlgItemText(IDC_ACODEC, strAudioFormat);

		LPCTSTR pChan;
		if (!bDiffAudioChannels && ami.audio.nChannels) {
			switch (ami.audio.nChannels) {
			case 1:
				pChan = _T("1 (Mono)");
				break;
			case 2:
				pChan = _T("2 (Stereo)");
				break;
			case 5:
				pChan = _T("5.1 (Surround)");
				break;
			default:
				pChan = NULL;
				SetDlgItemInt(IDC_ACHANNEL, ami.audio.nChannels, FALSE);
			}
		} else
			pChan = _T("");
		if (pChan)
			SetDlgItemText(IDC_ACHANNEL, pChan);

		if (!bDiffAudioSamplesPerSec && ami.audio.nSamplesPerSec) {
			buffer.Format(_T("%.3f kHz"), ami.audio.nSamplesPerSec / 1000.0);
			SetDlgItemText(IDC_ASAMPLERATE, buffer);
		} else
			SetDlgItemText(IDC_ASAMPLERATE, _T(""));

		if (!bDiffAudioAvgBytesPerSec && ami.audio.nAvgBytesPerSec) {
			if (ami.audio.nAvgBytesPerSec == _UI32_MAX)
				buffer = _T("Variable");
			else
				buffer.Format(_T("%u %s"), (UINT)((ami.audio.nAvgBytesPerSec * 16ull + SEC2MS(1)) / SEC2MS(2)), (LPCTSTR)GetResString(IDS_KBITSSEC));
			SetDlgItemText(IDC_ABITRATE, buffer);
		} else
			SetDlgItemText(IDC_ABITRATE, _T(""));

		CString strAudioLanguage;
		if (!bDiffAudioLanguage)
			strAudioLanguage = ami.strAudioLanguage;
		SetDlgItemText(IDC_ALANGUAGE, strAudioLanguage);
	}

	if (!m_bReducedDlg) {
		m_fi.SetRTFText(pThreadRes->strInfo);
		DisableAutoSelect(m_fi);
	}

	delete pThreadRes;
	return 1;
}

bool CGetMediaInfoThread::GetMediaInfo(HWND hWndOwner, const CShareableFile *pFile, SMediaInfo *mi, bool bSingleFile)
{
	if (!pFile)
		return false;
	ASSERT(!pFile->GetFilePath().IsEmpty());

	bool bHasDRM = false;
	if (!pFile->IsPartFile() || static_cast<const CPartFile*>(pFile)->IsCompleteBDSafe(0, 1024)) {
		GetMimeType(pFile->GetFilePath(), mi->strMimeType);
		bHasDRM = GetDRM(pFile->GetFilePath());
		if (bHasDRM) {
			if (!mi->strInfo.IsEmpty())
				mi->strInfo << _T("\r\n");
			mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfRed);
			mi->strInfo.AppendFormat(GetResString(IDS_MEDIAINFO_DRMWARNING), (LPCTSTR)pFile->GetFileName());
			mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfDef);
		}
	}

	mi->ulFileSize = pFile->GetFileSize();

	bool bFoundHeader = false;
	if (pFile->IsPartFile()) {
		// Do *not* pass a part file which does not have the beginning of file to the following code.
		//	- The MP3 reading code will skip all 0-bytes from the beginning of the file and may block
		//	  the main thread for a long time.
		//
		//	- The RIFF reading code will not work without the file header.
		//
		//	- Most (if not all) other code also will not work without the beginning of the file available.
		if (!static_cast<const CPartFile*>(pFile)->IsCompleteSafe(0, 16 * 1024))
			return bFoundHeader || !mi->strMimeType.IsEmpty();
	}

	LPCTSTR const pDot = ::PathFindExtension(pFile->GetFileName());
	CString szExt(&pDot[static_cast<int>(*pDot != _T('\0'))]); //skip the dot
	szExt.MakeLower();

	if (!::IsWindow(hWndOwner))
		return false;

	/** @brief Route all audio/video metadata through the required MediaInfo DLL. */
	const EED2KFileType eFileType = GetED2KFileTypeID(pFile->GetFileName());
	const bool bShouldInspectWithMediaInfo = thePrefs.GetInspectAllFileTypes()
		|| eFileType == ED2KFT_AUDIO
		|| eFileType == ED2KFT_VIDEO;
	bool bMediaInfoAvailable = false;
	if (bShouldInspectWithMediaInfo) {
		try {
			bFoundHeader = GetMediaInfoDllInfo(pFile->GetFilePath(), pFile->GetFileSize(), mi, true, bSingleFile, &bMediaInfoAvailable);
		} catch (...) {
			ASSERT(0);
		}
	}

	if (!::IsWindow(hWndOwner))
		return false;

	if (!bFoundHeader && bShouldInspectWithMediaInfo && !bMediaInfoAvailable) {
		TCHAR szBuff[MAX_PATH];
		DWORD dwModPathLen = ::GetModuleFileName(theApp.m_hInstance, szBuff, _countof(szBuff));
		if (dwModPathLen == 0 || dwModPathLen == _countof(szBuff))
			szBuff[0] = _T('\0');
		CString strInstFolder(szBuff);
		::PathRemoveFileSpec(strInstFolder.GetBuffer(strInstFolder.GetLength()));
		strInstFolder.ReleaseBuffer();
		CString strHint;
		strHint.Format(GetResString(IDS_MEDIAINFO_DLLMISSING), (LPCTSTR)strInstFolder);
		if (!mi->strInfo.IsEmpty())
			mi->strInfo << _T("\r\n");
		mi->strInfo << strHint;
	}

	return bFoundHeader || !mi->strMimeType.IsEmpty() || bHasDRM;
}

void CFileInfoDialog::DoDataExchange(CDataExchange *pDX)
{
	CResizablePage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_FULL_FILE_INFO, m_fi);
}

void CFileInfoDialog::Localize()
{
	if (!m_hWnd)
		return;
	SetTabTitle(IDS_CONTENT_INFO, this);

	SetDlgItemText(IDC_GENERAL, GetResString(IDS_FD_GENERAL));
	SetDlgItemText(IDC_FD_XI2, GetResString(IDS_LENGTH) + _T(':'));
	SetDlgItemText(IDC_FD_XI3, GetResString(IDS_VIDEO));
	SetDlgItemText(IDC_FD_XI4, GetResString(IDS_AUDIO));
	SetDlgItemText(IDC_FD_XI5, GetResString(IDS_CODEC) + _T(':'));
	SetDlgItemText(IDC_FD_XI6, GetResString(IDS_CODEC) + _T(':'));
	SetDlgItemText(IDC_FD_XI7, GetResString(IDS_BITRATE) + _T(':'));
	SetDlgItemText(IDC_FD_XI8, GetResString(IDS_BITRATE) + _T(':'));
	CString sWH;
	sWH.Format(_T("%s x %s:"), (LPCTSTR)GetResString(IDS_WIDTH), (LPCTSTR)GetResString(IDS_HEIGHT));
	SetDlgItemText(IDC_FD_XI9, sWH);
	SetDlgItemText(IDC_FD_XI13, GetResString(IDS_FPS) + _T(':'));
	SetDlgItemText(IDC_FD_XI10, GetResString(IDS_CHANNELS) + _T(':'));
	SetDlgItemText(IDC_FD_XI12, GetResString(IDS_SAMPLERATE) + _T(':'));
	SetDlgItemText(IDC_STATICFI, GetResString(IDS_FILEFORMAT) + _T(':'));
	SetDlgItemText(IDC_FD_XI14, GetResString(IDS_ASPECTRATIO) + _T(':'));
	SetDlgItemText(IDC_STATIC_LANGUAGE, GetResString(IDS_PW_LANG) + _T(':'));
	if (!m_bReducedDlg)
		SetDlgItemText(IDC_FD_XI1, GetResString(IDS_FD_SIZE));
}
