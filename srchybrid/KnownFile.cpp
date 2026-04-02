// parts of this file are based on work from pan One (http://home-3.tiscali.nl/~meost/pms/)
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
#include <sys/stat.h>
#ifdef _DEBUG
#include "DebugHelpers.h"
#endif
#include "emule.h"
#include "KnownFileList.h"
#include "SharedFileList.h"
#include "UpDownClient.h"
#include "ClientList.h"
#include "opcodes.h"
#include "ini2.h"
#include "Preferences.h"
#include "PartFile.h"
#include "Packets.h"
#include "ProtocolGuards.h"
#include "Kademlia/Kademlia/SearchManager.h"
#include "Kademlia/Kademlia/Entry.h"
#include "kademlia/kademlia/UDPFirewallTester.h"
#include "SafeFile.h"
#include "shahashset.h"
#include "Log.h"
#include "MD4.h"
#include <vector>
#include "PartFileNumericSeams.h"
#include "KnownFileProgressSeams.h"
#include "ResourceOwnershipSeams.h"
#include "WorkerUiMessageSeams.h"
#include "MappedFileReader.h"
#include "Collection.h"
#include "emuledlg.h"
#include "SharedFilesWnd.h"
#include "MediaInfo.h"
#include "uploaddiskiothread.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

namespace
{
	/**
	 * @brief Opens a shared-read Win32 handle for hashing while keeping long-path support.
	 */
	ScopedHandle OpenHashReadHandleLongPath(const CString &rstrFilePath)
	{
		return ScopedHandle(::CreateFile(PreparePathForLongPath(rstrFilePath), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE
			, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL));
	}

	/**
	 * @brief Accumulates MD4 and AICH state while a file-backed reader streams byte spans.
	 */
	class CCreateHashVisitor : public IMappedFileRangeVisitor
	{
	public:
		CCreateHashVisitor(uchar *pMd4HashOut, CAICHHashTree *pShaHashOut)
			: m_pMd4HashOut(pMd4HashOut)
			, m_pShaHashOut(pShaHashOut)
			, m_nPosCurrentEMBlock()
			, m_nIACHPos()
			, m_pHashAlg((pShaHashOut != NULL) ? CAICHRecoveryHashSet::GetNewHashAlgo() : NULL)
		{
		}

		~CCreateHashVisitor() override
		{
			delete m_pHashAlg;
		}

		void OnMappedFileBytes(const BYTE *pBytes, size_t nByteCount) override
		{
			while (nByteCount != 0) {
				size_t nChunk = nByteCount;
				if (m_pShaHashOut != NULL) {
					const uint64 nBytesToBoundary = EMBLOCKSIZE - m_nIACHPos;
					if (nChunk > nBytesToBoundary)
						nChunk = static_cast<size_t>(nBytesToBoundary);
					m_pHashAlg->Add(pBytes, static_cast<DWORD>(nChunk));
					m_nIACHPos += nChunk;
				}

				if (m_pMd4HashOut != NULL)
					m_md4.Add(pBytes, nChunk);

				if (m_pShaHashOut != NULL && m_nIACHPos == EMBLOCKSIZE) {
					m_pShaHashOut->SetBlockHash(EMBLOCKSIZE, m_nPosCurrentEMBlock, m_pHashAlg);
					m_nPosCurrentEMBlock += EMBLOCKSIZE;
					m_pHashAlg->Reset();
					m_nIACHPos = 0;
				}

				pBytes += nChunk;
				nByteCount -= nChunk;
			}
		}

		void Finish(uint64 Length)
		{
			if (m_pShaHashOut != NULL) {
				if (m_nIACHPos > 0) {
					m_pShaHashOut->SetBlockHash(m_nIACHPos, m_nPosCurrentEMBlock, m_pHashAlg);
					m_nPosCurrentEMBlock += m_nIACHPos;
				}
				ASSERT(m_nPosCurrentEMBlock == Length);
				VERIFY(m_pShaHashOut->ReCalculateHash(m_pHashAlg, false));
				delete m_pHashAlg;
				m_pHashAlg = NULL;
			}

			if (m_pMd4HashOut != NULL) {
				m_md4.Finish();
				md4cpy(m_pMd4HashOut, m_md4.GetHash());
			}
		}

	private:
		uchar *m_pMd4HashOut;
		CAICHHashTree *m_pShaHashOut;
		uint64 m_nPosCurrentEMBlock;
		uint64 m_nIACHPos;
		CMD4 m_md4;
		CAICHHashAlgo *m_pHashAlg;
	};
}

// Meta data version
// -----------------
//	0	untrusted meta data which was received via search results
//	1	trusted meta data, Unicode (strings where not stored correctly)
//	2	0.49c: trusted meta data, Unicode
#define	META_DATA_VER	2

IMPLEMENT_DYNAMIC(CKnownFile, CShareableFile)

CKnownFile::CKnownFile()
	: m_tUtcLastModified((time_t)-1)
	, m_tCompleteSourcesTime() //(time(NULL))
	, m_nCompleteSourcesCount(1)
	, m_nCompleteSourcesCountLo(1)
	, m_nCompleteSourcesCountHi(1)
	, m_pCollection()
	, m_hRead(INVALID_HANDLE_VALUE)
	, nInUse()
	, bNoNewReads()
	, m_timeLastSeen()
	, m_lastPublishTimeKadSrc()
	, m_lastPublishTimeKadNotes()
	, kadFileSearchID()
	, m_lastBuddyIP()
	, m_uMetaDataVer()
	, m_iPartCount()
	, m_iED2KPartCount()
	, m_iUpPriority()
	, m_bAutoUpPriority(thePrefs.GetNewAutoUp())
	, m_PublishedED2K()
	, m_bAICHRecoverHashSetAvailable()
{
	m_iUpPriority = m_bAutoUpPriority ? PR_HIGH : PR_NORMAL;
	statistic.fileParent = this;
	SetLastPublishTimeKadSrc(0, 0);
}

CKnownFile::~CKnownFile()
{
	ASSERT(!nInUse);
	CUploadDiskIOThread::DissociateFile(this);
	delete m_pCollection;
}

#ifdef _DEBUG
void CKnownFile::AssertValid() const
{
	CAbstractFile::AssertValid();

	(void)m_tUtcLastModified;
	(void)statistic;
	(void)m_tCompleteSourcesTime;
	(void)m_nCompleteSourcesCount;
	(void)m_nCompleteSourcesCountLo;
	(void)m_nCompleteSourcesCountHi;
	m_ClientUploadList.AssertValid();
	m_AvailPartFrequency.AssertValid();
	(void)m_strDirectory;
	(void)m_strFilePath;
	(void)m_iPartCount;
	(void)m_iED2KPartCount;
	ASSERT(m_iUpPriority == PR_VERYLOW || m_iUpPriority == PR_LOW || m_iUpPriority == PR_NORMAL || m_iUpPriority == PR_HIGH || m_iUpPriority == PR_VERYHIGH);
	CHECK_BOOL(m_bAutoUpPriority);
	(void)s_ShareStatusBar;
	CHECK_BOOL(m_PublishedED2K);
	(void)kadFileSearchID;
	(void)m_lastPublishTimeKadSrc;
	(void)m_lastPublishTimeKadNotes;
	(void)m_lastBuddyIP;
	(void)wordlist;
}

void CKnownFile::Dump(CDumpContext &dc) const
{
	CAbstractFile::Dump(dc);
}
#endif

CBarShader CKnownFile::s_ShareStatusBar(16);

void CKnownFile::DrawShareStatusBar(CDC &dc, LPCRECT rect, bool onlygreyrect, bool  bFlat) const
{
	s_ShareStatusBar.SetFileSize(GetFileSize());
	s_ShareStatusBar.SetRect(rect);

	if (!m_ClientUploadList.IsEmpty() || m_nCompleteSourcesCountHi > 1) {
		// We have info about chunk frequency in the net, so we will color the chunks we have after perceived availability.
		const COLORREF crMissing = RGB(255, 0, 0);
		s_ShareStatusBar.Fill(crMissing);

		if (!onlygreyrect) {
			uint32 tempCompleteSources = m_nCompleteSourcesCountLo ? m_nCompleteSourcesCountLo - 1 : 0;

			for (INT_PTR i = GetPartCount(); --i >= 0;) {
				uint32 frequency = tempCompleteSources;
				if (!m_AvailPartFrequency.IsEmpty())
					frequency = max(m_AvailPartFrequency[i], tempCompleteSources);

				if (frequency > 0) {
					COLORREF color = RGB(0, (22 * (frequency - 1) >= 210) ? 0 : 210 - (22 * (frequency - 1)), 255);
					uint64 uBegin = PARTSIZE * i;
					s_ShareStatusBar.FillRange(uBegin, uBegin + PARTSIZE, color);
				}
			}
		}
	} else {
		// We have no info about chunk frequency in the net, so just color the chunk we have as black.
		COLORREF crNooneAsked = bFlat ? RGB(0, 0, 0) : RGB(104, 104, 104);
		s_ShareStatusBar.Fill(crNooneAsked);
	}

	s_ShareStatusBar.Draw(dc, rect->left, rect->top, bFlat);
}

void CKnownFile::UpdateFileRatingCommentAvail(bool bForceUpdate)
{
	bool bOldHasComment = m_bHasComment;
	UINT uOldUserRatings = m_uUserRating;

	m_bHasComment = false;
	UINT uRatings = 0;
	UINT uUserRatings = 0;

	for (POSITION pos = m_kadNotes.GetHeadPosition(); pos != NULL;) {
		const Kademlia::CEntry *entry = m_kadNotes.GetNext(pos);
		if (!m_bHasComment && !entry->GetStrTagValue(Kademlia::CKadTagNameString(TAG_DESCRIPTION)).IsEmpty())
			m_bHasComment = true;
		UINT rating = (UINT)entry->GetIntTagValue(Kademlia::CKadTagNameString(TAG_FILERATING));
		if (rating != 0) {
			++uRatings;
			uUserRatings += rating;
		}
	}

	m_uUserRating = uRatings ? (uint32)ROUND(uUserRatings / (float)uRatings) : 0;

	if (bOldHasComment != m_bHasComment || uOldUserRatings != m_uUserRating || bForceUpdate)
		theApp.emuledlg->sharedfileswnd->sharedfilesctrl.UpdateFile(this);
}

float CKnownFile::GetAllTimeUploadRatio() const
{
	const uint64 nFileSize = GetFileSize();
	if (nFileSize == 0)
		return 0.0f;

	// Seeder-biased queue scoring uses a simple historical scarcity signal:
	// bytes uploaded for this file divided by the file size.
	return static_cast<float>(static_cast<double>(statistic.GetAllTimeTransferred()) / static_cast<double>(nFileSize));
}

float CKnownFile::GetSessionUploadRatio() const
{
	const uint64 nFileSize = GetFileSize();
	if (nFileSize == 0)
		return 0.0f;

	return static_cast<float>(static_cast<double>(statistic.GetTransferred()) / static_cast<double>(nFileSize));
}

void CKnownFile::UpdatePartsInfo()
{
	time_t tNow = time(NULL);
	bool bRefresh = (tNow - m_tCompleteSourcesTime > 0);

	// Reset part counters
	if (m_AvailPartFrequency.GetCount() < GetPartCount())
		m_AvailPartFrequency.SetSize(GetPartCount());
	if (GetPartCount())
		memset(&m_AvailPartFrequency[0], 0, GetPartCount() * sizeof m_AvailPartFrequency[0]);

	CArray<uint16, uint16> acount;
	if (bRefresh)
		acount.SetSize(0, m_ClientUploadList.GetCount());
	for (POSITION pos = m_ClientUploadList.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_src = m_ClientUploadList.GetNext(pos);
		//This could be a partfile that just completed. Many of these clients will not have this information.
		if (cur_src->GetUpPartStatus() != NULL && cur_src->GetUpPartCount() == GetPartCount()) {
			for (INT_PTR i = GetPartCount(); --i > 0;)
				m_AvailPartFrequency[i] += static_cast<uint16>(cur_src->IsUpPartAvailable((UINT)i));

			if (bRefresh)
				acount.Add(cur_src->GetUpCompleteSourcesCount());
		}
	}

	if (bRefresh) {
		m_nCompleteSourcesCountLo = m_nCompleteSourcesCountHi = 0;

		if (GetPartCount() > 0) {
			m_nCompleteSourcesCount = m_AvailPartFrequency[0];
			for (INT_PTR i = GetPartCount(); --i >= 0;)
				if (m_nCompleteSourcesCount > m_AvailPartFrequency[i])
					m_nCompleteSourcesCount = m_AvailPartFrequency[i];
		} else
			m_nCompleteSourcesCount = 0;
		acount.Add(m_nCompleteSourcesCount + 1); // plus 1 since we have the complete file too

		int n = (int)acount.GetCount();
		if (n > 0) {
			// SLUGFILLER: heapsortCompletesrc
			for (int r = n / 2; r--;)
				HeapSort(acount, r, n - 1);
			for (int r = n; --r;) {
				uint16 t = acount[r];
				acount[r] = acount[0];
				acount[0] = t;
				HeapSort(acount, 0, r - 1);
			}
			// SLUGFILLER: heapsortCompletesrc

			// calculate range
			int j = (n * 3) >> 2;	// (n * 3) / 4

			//For complete files, trust people you had uploaded more...

			if (n < 20) {
				//For low guess and normal guess count
				//	If we see more sources then the guessed low and normal, use what we see.
				//	If we see less sources then the guessed low, adjust network accounts for 100%,
				//	we account for 0% with what we see and make sure we are still above the normal.
				//For high guess
				//	Adjust 100% network and 0% what we see.
				int i = n >> 1;			// (n / 2)
				if (acount[i] < m_nCompleteSourcesCount)
					m_nCompleteSourcesCountLo = m_nCompleteSourcesCount;
				else
					m_nCompleteSourcesCountLo = acount[i];
				m_nCompleteSourcesCount = m_nCompleteSourcesCountLo;
				m_nCompleteSourcesCountHi = acount[j];
			} else {
				//Many sources.
				//For low guess
				//	Use what we see.
				//For normal guess
				//	Adjust network accounts for 100%, we account for 0% with what we see and make sure we are still above the low.
				//For high guess
				//  Adjust network accounts for 100%, we account for 0% with what we see and make sure we are still above the normal.
				m_nCompleteSourcesCountLo = m_nCompleteSourcesCount;
				m_nCompleteSourcesCount = acount[j];
				if (m_nCompleteSourcesCount < m_nCompleteSourcesCountLo)
					m_nCompleteSourcesCount = m_nCompleteSourcesCountLo;
				int k = (n * 7) >> 3;	// (n * 7) / 8
				m_nCompleteSourcesCountHi = acount[k];
			}
			if (m_nCompleteSourcesCountHi < m_nCompleteSourcesCount)
				m_nCompleteSourcesCountHi = m_nCompleteSourcesCount;
		}
		m_tCompleteSourcesTime = tNow + MIN2S(1);
	}
	if (theApp.emuledlg->sharedfileswnd->m_hWnd)
		theApp.emuledlg->sharedfileswnd->sharedfilesctrl.UpdateFile(this);
}

void CKnownFile::AddUploadingClient(CUpDownClient *client)
{
	if (m_ClientUploadList.Find(client) == NULL) { // to be sure
		m_ClientUploadList.AddTail(client);
		UpdateAutoUpPriority();
	}
}

void CKnownFile::RemoveUploadingClient(CUpDownClient *client)
{
	POSITION pos = m_ClientUploadList.Find(client);
	if (pos != NULL) {
		m_ClientUploadList.RemoveAt(pos);
		UpdateAutoUpPriority();
	}
	if (m_ClientUploadList.IsEmpty()) {
		ASSERT(!nInUse);
		CUploadDiskIOThread::DissociateFile(this);
	}
}

#ifdef _DEBUG
void Dump(const Kademlia::WordList &wordlist)
{
	for (Kademlia::WordList::const_iterator it = wordlist.begin(); it != wordlist.end(); ++it) {
		const CStringW &rstrKeyword(*it);
		TRACE("  %ls\n", (LPCWSTR)rstrKeyword);
	}
}
#endif

void CKnownFile::SetFileName(LPCTSTR pszFileName, bool bReplaceInvalidFileSystemChars, bool bRemoveControlChars)
{
	// The shared files object may not exist early in startup sequence.
	bool bSharedFiles = theApp.sharedfiles && theApp.sharedfiles->GetFileByID(GetFileHash()) == this;

	if (bSharedFiles)
		theApp.sharedfiles->RemoveKeywords(this);

	SetAFileName(pszFileName, bReplaceInvalidFileSystemChars, true, bRemoveControlChars);
	m_verifiedFileType = FILETYPE_UNKNOWN;

	wordlist.clear();
	if (m_pCollection) {
		CStringW sKeyWords(m_pCollection->GetCollectionAuthorKeyString());
		sKeyWords.AppendFormat(_T(" %s"), (LPCTSTR)GetFileName());
		Kademlia::CSearchManager::GetWords(sKeyWords, wordlist);
	} else
		Kademlia::CSearchManager::GetWords((CStringW)GetFileName(), wordlist); //make sure that it is a CStringW

	if (bSharedFiles)
		theApp.sharedfiles->AddKeywords(this);
}

bool CKnownFile::CreateFromFile(LPCTSTR in_directory, LPCTSTR in_filename, LPVOID pvProgressParam)
{
	SetPath(in_directory);
	SetFileName(in_filename);

	// open file
	CString strFilePath(in_directory);
	if (!strFilePath.IsEmpty() && strFilePath.Right(1) != _T("\\"))
		strFilePath += _T("\\");
	strFilePath += in_filename;
	SetFilePath(strFilePath);
	ScopedHandle hFile = OpenHashReadHandleLongPath(strFilePath); // can not use exclusive sharing because we may access a completing part file
	if (!hFile.IsValid()) {
		LogError(GetResString(IDS_ERR_FILEOPEN) + _T(" - %s"), (LPCTSTR)strFilePath, _T(""), (LPCTSTR)GetErrorMessage(::GetLastError()));
		return false;
	}

	// set file size. Zero size is valid for .part files
	LARGE_INTEGER liFileSize = {};
	if (!::GetFileSizeEx(hFile.Get(), &liFileSize)) {
		LogError(_T("Failed to hash file \"%s\" - %s"), (LPCTSTR)strFilePath, (LPCTSTR)GetErrorMessage(::GetLastError()));
		return false;
	}
	__int64 llFileSize = liFileSize.QuadPart;
	if ((uint64)llFileSize > MAX_EMULE_FILE_SIZE) {
		if (llFileSize <= 0)
			LogError(_T("Failed to hash file \"%s\" - %s"), (LPCTSTR)strFilePath, (LPCTSTR)GetErrorMessage(ERROR_HANDLE_EOF));
		else
			LogError(_T("Skipped hashing file \"%s\" - File size exceeds limit."), (LPCTSTR)strFilePath);
		return false; // not supported by network
	}
	SetFileSize((EMFileSize)(uint64)llFileSize);

	m_AvailPartFrequency.SetSize(GetPartCount());
	if (GetPartCount())
		memset(&m_AvailPartFrequency[0], 0, GetPartCount() * sizeof m_AvailPartFrequency[0]);

	// create hashset
	CAICHRecoveryHashSet cAICHHashSet(this, m_nFileSize);
	uint64 togo = (uint64)m_nFileSize;
	UINT hashcount;
	for (hashcount = 0; ; ++hashcount) {
		UINT uSize = (UINT)min(togo, PARTSIZE);
		CAICHHashTree *pBlockAICHHashTree;
		if (togo) {
			pBlockAICHHashTree = cAICHHashSet.m_pHashTree.FindHash(hashcount * PARTSIZE, uSize);
			ASSERT(pBlockAICHHashTree != NULL);
		} else
			pBlockAICHHashTree = NULL; // SHA hash tree doesn't take hash of zero-sized data

		uchar *newhash = new uchar[MDX_DIGEST_SIZE];
		try {
			CreateHash(hFile.Get(), static_cast<uint64>(hashcount) * PARTSIZE, uSize, newhash, pBlockAICHHashTree);
		} catch (CFileException *ex) {
			LogError(_T("Failed to hash file \"%s\" - %s"), (LPCTSTR)strFilePath, (LPCTSTR)CExceptionStr(*ex));
			delete[] newhash;
			ex->Delete();
			return false;
		}

		if (theApp.IsClosing()) { // in case of shutdown while still hashing
			delete[] newhash;
			return false;
		}

		if (!hashcount && uSize < PARTSIZE) {
			m_FileIdentifier.SetMD4Hash(newhash); //one and only part
			delete[] newhash;
		} else
			m_FileIdentifier.GetRawMD4HashSet().Add(newhash);

		togo -= uSize;
		if (!togo)
			if (uSize == PARTSIZE)
				continue;
			else
				break;

		if (theApp.IsClosing()) {
			LogError(_T("Hashing cancelled (closing eMule), file \"%s\""), (LPCTSTR)strFilePath);
			return false;
		}
		if (pvProgressParam) {
			if (reinterpret_cast<CPartFile*>(pvProgressParam)->IsKindOf(RUNTIME_CLASS(CPartFile))
				&& reinterpret_cast<CPartFile*>(pvProgressParam)->IsDeleting()) {
				LogError(_T("Hashing cancelled (pending delete), file \"%s\""), (LPCTSTR)strFilePath);
				return false;
			}

			CKnownFile *pProgressOwner = reinterpret_cast<CKnownFile*>(pvProgressParam);
			const uint64 nFileSize = static_cast<uint64>(GetFileSize());
			if (IsCompatibleKnownFileProgressOwner(
				pProgressOwner->IsKindOf(RUNTIME_CLASS(CKnownFile)) != FALSE,
				static_cast<uint64>(pProgressOwner->GetFileSize()),
				nFileSize))
			{
				const uint64 nBytesHashed = nFileSize >= togo ? nFileSize - togo : 0;
				WPARAM uProgress = static_cast<WPARAM>(CalculateProgressPercent(nBytesHashed, nFileSize));
				ASSERT(uProgress <= 100);
				/** @brief File hashing progress is a best-effort worker-to-UI notification. */
				(void)TryPostWorkerUiMessage(theApp.emuledlg != NULL ? theApp.emuledlg->GetSafeHwnd() : NULL, TM_FILEOPPROGRESS, uProgress, (LPARAM)pvProgressParam);
			}
		}
	}

	if (hashcount)
		m_FileIdentifier.CalculateMD4HashByHashSet(false);

	cAICHHashSet.ReCalculateHash(false);
	if (cAICHHashSet.VerifyHashTree(true)) {
		cAICHHashSet.SetStatus(AICH_HASHSETCOMPLETE);
		m_FileIdentifier.SetAICHHash(cAICHHashSet.GetMasterHash());
		if (!m_FileIdentifier.SetAICHHashSet(cAICHHashSet)) {
			ASSERT(0);
			DebugLogError(_T("CreateFromFile() - failed to create AICH PartHashSet out of RecoveryHashSet - %s"), (LPCTSTR)GetFileName());
		}
		if (!cAICHHashSet.SaveHashSet())
			LogError(LOG_STATUSBAR, GetResString(IDS_SAVEACFAILED));
		else
			SetAICHRecoverHashSetAvailable(true);
	} else
		// now something went pretty wrong
		DebugLogError(LOG_STATUSBAR, _T("Failed to calculate AICH Hashset from file %s"), (LPCTSTR)GetFileName());

	if (pvProgressParam && !theApp.IsClosing()) {
		CKnownFile *pProgressOwner = reinterpret_cast<CKnownFile*>(pvProgressParam);
		if (IsCompatibleKnownFileProgressOwner(
			pProgressOwner->IsKindOf(RUNTIME_CLASS(CKnownFile)) != FALSE,
			static_cast<uint64>(pProgressOwner->GetFileSize()),
			static_cast<uint64>(GetFileSize())))
		{
			WPARAM uProgress = 100;
			ASSERT(uProgress <= 100);
			/** @brief Final hashing progress is also best-effort during shutdown and dialog teardown. */
			(void)TryPostWorkerUiMessage(theApp.emuledlg != NULL ? theApp.emuledlg->GetSafeHwnd() : NULL, TM_FILEOPPROGRESS, uProgress, (LPARAM)pvProgressParam);
		}
	}

	// set last write date
	struct _stat64 st;
	if (statUTC(hFile.Get(), st) == 0) {
		m_tUtcLastModified = (time_t)st.st_mtime;
		AdjustNTFSDaylightFileTime(m_tUtcLastModified, (LPCTSTR)strFilePath);
	}

	// Add file tags
	UpdateMetaDataTags();

	UpdatePartsInfo();

	return true;
}

bool CKnownFile::CreateAICHHashSetOnly()
{
	ASSERT(!IsPartFile());

	ScopedHandle hFile = OpenHashReadHandleLongPath(GetFilePath()); // can not use exclusive sharing because we may access a completing part file
	if (!hFile.IsValid()) {
		LogError(GetResString(IDS_ERR_FILEOPEN) + _T(" - %s"), (LPCTSTR)GetFilePath(), _T(""), (LPCTSTR)GetErrorMessage(::GetLastError()));
		return false;
	}

	// create aich hashset
	CAICHRecoveryHashSet cAICHHashSet(this, m_nFileSize);
	uint64 togo = (uint64)m_nFileSize;
	for (UINT hashcount = 0; togo; ++hashcount) {
		uint64 uSize = min(togo, PARTSIZE);
		CAICHHashTree *pBlockAICHHashTree = cAICHHashSet.m_pHashTree.FindHash(hashcount * PARTSIZE, uSize);
		ASSERT(pBlockAICHHashTree != NULL);
		try {
			CreateHash(hFile.Get(), static_cast<uint64>(hashcount) * PARTSIZE, uSize, NULL, pBlockAICHHashTree);
		} catch (CFileException *ex) {
			LogError(_T("Failed to hash file \"%s\" - %s"), (LPCTSTR)GetFilePath(), (LPCTSTR)CExceptionStr(*ex));
			ex->Delete();
			return false;
		}
		if (theApp.IsClosing()) { // in case of shutdown while still hashing
			return false;
		}
		togo -= uSize;
	}

	cAICHHashSet.ReCalculateHash(false);
	if (cAICHHashSet.VerifyHashTree(true)) {
		cAICHHashSet.SetStatus(AICH_HASHSETCOMPLETE);
		const CAICHHash *pHash = (m_FileIdentifier.HasAICHHash() && m_FileIdentifier.GetAICHHash() != cAICHHashSet.GetMasterHash())
			? &m_FileIdentifier.GetAICHHash() : NULL;
		theApp.knownfiles->AICHHashChanged(pHash, cAICHHashSet.GetMasterHash(), this);
		m_FileIdentifier.SetAICHHash(cAICHHashSet.GetMasterHash());
		if (!m_FileIdentifier.SetAICHHashSet(cAICHHashSet)) {
			ASSERT(0);
			DebugLogError(_T("CreateAICHHashSetOnly() - failed to create AICH PartHashSet out of RecoveryHashSet - %s"), (LPCTSTR)GetFileName());
		}
		if (!cAICHHashSet.SaveHashSet())
			LogError(LOG_STATUSBAR, GetResString(IDS_SAVEACFAILED));
		else
			SetAICHRecoverHashSetAvailable(true);
	} else {
		// now something went pretty wrong
		DebugLogError(LOG_STATUSBAR, _T("Failed to calculate AICH Hashset from file %s"), (LPCTSTR)GetFileName());
	}

	return true;
}

void CKnownFile::SetFileSize(EMFileSize nFileSize)
{
	CAbstractFile::SetFileSize(nFileSize);

	// Examples of part hashes, hash sets and file hashes for different file sizes
	// according the ed2k protocol
	//----------------------------------------------------------------------
	//
	//File size: 3 bytes
	//File hash: 2D55E87D0E21F49B9AD25F98531F3724
	//Nr. hashes: 0
	//
	//
	//File size: 1*PARTSIZE
	//File hash: A72CA8DF7F07154E217C236C89C17619
	//Nr. hashes: 2
	//Hash[  0]: 4891ED2E5C9C49F442145A3A5F608299
	//Hash[  1]: 31D6CFE0D16AE931B73C59D7E0C089C0	*special part hash*
	//
	//
	//File size: 1*PARTSIZE + 1 byte
	//File hash: 2F620AE9D462CBB6A59FE8401D2B3D23
	//Nr. hashes: 2
	//Hash[  0]: 121795F0BEDE02DDC7C5426D0995F53F
	//Hash[  1]: C329E527945B8FE75B3C5E8826755747
	//
	//
	//File size: 2*PARTSIZE
	//File hash: A54C5E562D5E03CA7D77961EB9A745A4
	//Nr. hashes: 3
	//Hash[  0]: B3F5CE2A06BF403BFB9BFFF68BDDC4D9
	//Hash[  1]: 509AA30C9EA8FC136B1159DF2F35B8A9
	//Hash[  2]: 31D6CFE0D16AE931B73C59D7E0C089C0	*special part hash*
	//
	//
	//File size: 3*PARTSIZE
	//File hash: 5E249B96F9A46A18FC2489B005BF2667
	//Nr. hashes: 4
	//Hash[  0]: 5319896A2ECAD43BF17E2E3575278E72
	//Hash[  1]: D86EF157D5E49C5ED502EDC15BB5F82B
	//Hash[  2]: 10F2D5B1FCB95C0840519C58D708480F
	//Hash[  3]: 31D6CFE0D16AE931B73C59D7E0C089C0	*special part hash*
	//
	//
	//File size: 3*PARTSIZE + 1 byte
	//File hash: 797ED552F34380CAFF8C958207E40355
	//Nr. hashes: 4
	//Hash[  0]: FC7FD02CCD6987DCF1421F4C0AF94FB8
	//Hash[  1]: 2FE466AF8A7C06DA3365317B75A5ACFE
	//Hash[  2]: 873D3BF52629F7C1527C6E8E473C1C30
	//Hash[  3]: BCE50BEE7877BB07BB6FDA56BFE142FB
	//

	// File size       Data parts      ED2K parts      ED2K part hashes		AICH part hashes
	// -------------------------------------------------------------------------------------------
	// 1..PARTSIZE-1   1               1               0(!)					0 (!)
	// PARTSIZE        1               2(!)            2(!)					0 (!)
	// PARTSIZE+1      2               2               2					2
	// PARTSIZE*2      2               3(!)            3(!)					2
	// PARTSIZE*2+1    3               3               3					3

	if ((uint64)nFileSize == 0) {
		ASSERT(0);
		m_iPartCount = 0;
		m_iED2KPartCount = 0;
		return;
	}

	// nr. of data parts
	ASSERT(((uint64)nFileSize + (PARTSIZE - 1)) / PARTSIZE <= _UI16_MAX);
	m_iPartCount = (uint16)(((uint64)nFileSize + (PARTSIZE - 1)) / PARTSIZE);

	// nr. of parts to be used with OP_FILESTATUS
	m_iED2KPartCount = (uint16)((uint64)nFileSize / PARTSIZE + 1);
}

bool CKnownFile::LoadTagsFromFile(CFileDataIO &file)
{
	bool bHadAICHHashSetTag = false;
	for (uint32 j = file.ReadUInt32(); j > 0; --j) {
		CTag *newtag = new CTag(file, false);
		switch (newtag->GetNameID()) {
		case FT_FILENAME:
			ASSERT(newtag->IsStr());
			if (newtag->IsStr() && GetFileName().IsEmpty())
				SetFileName(newtag->GetStr());
			break;
		case FT_FILESIZE:
			ASSERT(newtag->IsInt64(true));
			if (newtag->IsInt64(true)) {
				SetFileSize(newtag->GetInt64());
				m_AvailPartFrequency.SetSize(GetPartCount());
				if (GetPartCount())
					memset(&m_AvailPartFrequency[0], 0, GetPartCount() * sizeof m_AvailPartFrequency[0]);
			}
			break;
		case FT_ATTRANSFERRED:
			ASSERT(newtag->IsInt());
			if (newtag->IsInt())
				statistic.SetAllTimeTransferred(newtag->GetInt());
			break;
		case FT_ATTRANSFERREDHI:
			ASSERT(newtag->IsInt());
			if (newtag->IsInt())
				statistic.SetAllTimeTransferred(((uint64)newtag->GetInt() << 32) | (UINT)statistic.GetAllTimeTransferred());
			break;
		case FT_ATREQUESTED:
			ASSERT(newtag->IsInt());
			if (newtag->IsInt())
				statistic.SetAllTimeRequests(newtag->GetInt());
			break;
		case FT_ATACCEPTED:
			ASSERT(newtag->IsInt());
			if (newtag->IsInt())
				statistic.SetAllTimeAccepts(newtag->GetInt());
			break;
		case FT_ULPRIORITY:
			ASSERT(newtag->IsInt());
			if (newtag->IsInt()) {
				m_iUpPriority = (uint8)newtag->GetInt();
				if (m_iUpPriority == PR_AUTO) {
					m_iUpPriority = PR_HIGH;
					m_bAutoUpPriority = true;
				} else {
					if (m_iUpPriority != PR_VERYLOW && m_iUpPriority != PR_LOW && m_iUpPriority != PR_NORMAL && m_iUpPriority != PR_HIGH && m_iUpPriority != PR_VERYHIGH)
						m_iUpPriority = PR_NORMAL;
					m_bAutoUpPriority = false;
				}
			}
			break;
		case FT_KADLASTPUBLISHSRC:
			ASSERT(newtag->IsInt());
			if (newtag->IsInt())
				SetLastPublishTimeKadSrc(newtag->GetInt(), 0);
			if (GetLastPublishTimeKadSrc() > time(NULL) + KADEMLIAREPUBLISHTIMES) {
				//There is a possibility of an older client that saved a random number here.
				SetLastPublishTimeKadSrc(0, 0); //the fix
			}
			break;
		case FT_KADLASTPUBLISHNOTES:
			ASSERT(newtag->IsInt());
			if (newtag->IsInt())
				SetLastPublishTimeKadNotes(newtag->GetInt());
			break;
		case FT_FLAGS:
			// Misc. Flags
			// ------------------------------------------------------------------------------
			// Bits  3-0: Meta data version
			//				0	untrusted meta data which was received via search results
			//				1	trusted meta data, Unicode (strings where not stored correctly)
			//				2	0.49c: trusted meta data, Unicode
			// Bits 31-4: Reserved
			ASSERT(newtag->IsInt());
			if (newtag->IsInt())
				m_uMetaDataVer = newtag->GetInt() & 0x0F;
			break;
		case FT_PERMISSIONS:  // old tags: as they are not needed, take a chance to purge
		case FT_KADLASTPUBLISHKEY:
			ASSERT(newtag->IsInt());
			break;
		case FT_AICH_HASH:
			if (newtag->IsStr()) {
				CAICHHash hash;
				if (DecodeBase32(newtag->GetStr(), hash) == CAICHHash::GetHashSize())
					m_FileIdentifier.SetAICHHash(hash);
				else
					ASSERT(0);
			} else
				ASSERT(0);
			break;
		case FT_LASTSHARED:
			if (newtag->IsInt())
				m_timeLastSeen = newtag->GetInt();
			else
				ASSERT(0);
			break;
		case FT_AICHHASHSET:
			if (newtag->IsBlob()) {
				CSafeMemFile aichHashSetFile(newtag->GetBlob(), newtag->GetBlobSize());
				m_FileIdentifier.LoadAICHHashsetFromFile(aichHashSetFile, false);
				aichHashSetFile.Detach();
				bHadAICHHashSetTag = true;
			} else
				ASSERT(0);
			break;
		default:
			ConvertED2KTag(newtag);
			if (newtag) {
				m_taglist.Add(newtag);
				newtag = NULL;
			}
		}
		delete newtag;
	}
	if (bHadAICHHashSetTag)
		if (!m_FileIdentifier.VerifyAICHHashSet())
			DebugLogError(_T("Failed to load AICH Part HashSet for file %s"), (LPCTSTR)GetFileName());
		//else
		//	DebugLog(_T("Succeeded to load AICH Part HashSet for file %s"), (LPCTSTR)GetFileName());

	// 05-Jan-2004 [bc]: ed2k and Kad are already full of totally wrong and/or not properly
	// attached meta data. Take the chance to clean any available meta data tags and provide
	// only tags which were determined by us.
	// It's a brute force method, but that wrong meta data is driving me crazy because
	// wrong meta data is even worse than missing meta data.
	if (m_uMetaDataVer == 0)
		RemoveMetaDataTags();
	else if (m_uMetaDataVer == 1) {
		// Meta data tags v1 did not store Unicode strings correctly.
		// Remove broken Unicode string meta data tags from v1, but keep the integer tags.
		RemoveBrokenUnicodeMetaDataTags();
		m_uMetaDataVer = META_DATA_VER;
	}

	return true;
}

bool CKnownFile::LoadDateFromFile(CFileDataIO &file)
{
	m_tUtcLastModified = (time_t)file.ReadUInt32();
	return true;
}

bool CKnownFile::LoadFromFile(CFileDataIO &file)
{
	// SLUGFILLER: SafeHash - load first, verify later
	bool ret = LoadDateFromFile(file);
	ret &= m_FileIdentifier.LoadMD4HashsetFromFile(file, false);
	ret &= LoadTagsFromFile(file);
	UpdatePartsInfo();
	return ret && m_FileIdentifier.HasExpectedMD4HashCount(); //Final hash-count verification, needs to be done after the tags are loaded.
	// SLUGFILLER: SafeHash
}

bool CKnownFile::WriteToFile(CFileDataIO &file)
{
	// date
	file.WriteUInt32((uint32)m_tUtcLastModified);

	// hashset
	m_FileIdentifier.WriteMD4HashsetToFile(file);

	uint32 uTagCount = 0;
	ULONGLONG uTagCountFilePos = file.GetPosition();
	file.WriteUInt32(uTagCount);

	CTag nametag(FT_FILENAME, GetFileName());
	nametag.WriteTagToFile(file, UTF8strOptBOM);
	++uTagCount;

	CTag sizetag(FT_FILESIZE, m_nFileSize, IsLargeFile());
	sizetag.WriteTagToFile(file);
	++uTagCount;

	//AICH file hash
	if (m_FileIdentifier.HasAICHHash()) {
		CTag aichtag(FT_AICH_HASH, m_FileIdentifier.GetAICHHash().GetString());
		aichtag.WriteTagToFile(file);
		++uTagCount;
	}

	// last shared
	static bool bDbgWarnedOnZero = false;
	if (!bDbgWarnedOnZero && m_timeLastSeen == 0) {
		DebugLog(_T("Unknown last seen date on stored file(s), upgrading from old version?"));
		bDbgWarnedOnZero = true;
	}
	time_t tNow = time(NULL);
	ASSERT(m_timeLastSeen <= tNow);
	time_t timeLastShared = (m_timeLastSeen > 0 && m_timeLastSeen <= tNow) ? m_timeLastSeen : tNow;
	CTag lastSharedTag(FT_LASTSHARED, (uint32)timeLastShared);
	lastSharedTag.WriteTagToFile(file);
	++uTagCount;

	// to tidy up known.met and known2.met do not store the tags for long time not seen/shared known files
	bool keep = !ShouldPartiallyPurgeFile();
	if (keep) { //"may be purged" tags
		// AICH Part HashSet
		// no point in permanently storing the AICH part hashset if we need to rehash the file anyway to fetch the full recovery hashset
		// Also the tag will make the known.met incompatible with emule version prior 0.44a - but that one is nearly 6 years old
		if (m_FileIdentifier.HasAICHHash() && m_FileIdentifier.HasExpectedAICHHashCount()) {
			bool bWriteHashSet = false;
			uint32 nAICHHashSetSize = 0;
			/** @brief Derive the serialized AICH hashset span through checked multiply-add math before allocating the buffer. */
			if (!PartFileNumericSeams::TryDeriveAICHHashSetSize(CAICHHash::GetHashSize(), m_FileIdentifier.GetAvailableAICHPartHashCount(), &nAICHHashSetSize)) {
				ASSERT(0);
				DebugLogError(_T("Skipped storing AICH Part HashSet because the serialized size overflowed"));
			} else {
				std::vector<BYTE> hashBuffer(nAICHHashSetSize);
				CSafeMemFile hashSetFile(hashBuffer.data(), static_cast<UINT>(hashBuffer.size()));
				try {
					m_FileIdentifier.WriteAICHHashsetToFile(hashSetFile);
					bWriteHashSet = true;
				} catch (CFileException *ex) {
					ASSERT(0);
					DebugLogError(_T("Memfile Error while storing AICH Part HashSet"));
					ex->Delete();
				}
				if (bWriteHashSet) {
					CTag tagAICHHashSet(FT_AICHHASHSET, hashBuffer.size(), hashBuffer.data());
					tagAICHHashSet.WriteTagToFile(file);
					++uTagCount;
				}
			}
		}
	}

	// statistics - never purge
	if (statistic.GetAllTimeTransferred()) {
		CTag attag1(FT_ATTRANSFERRED, (uint32)statistic.GetAllTimeTransferred());
		attag1.WriteTagToFile(file);
		++uTagCount;

		CTag attag4(FT_ATTRANSFERREDHI, (uint32)(statistic.GetAllTimeTransferred() >> 32));
		attag4.WriteTagToFile(file);
		++uTagCount;
	}

	if (statistic.GetAllTimeRequests()) {
		CTag attag2(FT_ATREQUESTED, statistic.GetAllTimeRequests());
		attag2.WriteTagToFile(file);
		++uTagCount;
	}

	if (statistic.GetAllTimeAccepts()) {
		CTag attag3(FT_ATACCEPTED, statistic.GetAllTimeAccepts());
		attag3.WriteTagToFile(file);
		++uTagCount;
	}

	// "may be purged" tags, part 2
	if (keep) {
		// priority N permission
		CTag priotag(FT_ULPRIORITY, IsAutoUpPriority() ? PR_AUTO : m_iUpPriority);
		priotag.WriteTagToFile(file);
		++uTagCount;


		if (m_lastPublishTimeKadSrc) {
			CTag kadLastPubSrc(FT_KADLASTPUBLISHSRC, (uint32)m_lastPublishTimeKadSrc);
			kadLastPubSrc.WriteTagToFile(file);
			++uTagCount;
		}

		if (m_lastPublishTimeKadNotes) {
			CTag kadLastPubNotes(FT_KADLASTPUBLISHNOTES, (uint32)m_lastPublishTimeKadNotes);
			kadLastPubNotes.WriteTagToFile(file);
			++uTagCount;
		}

		if (m_uMetaDataVer > 0) {
			// Misc. Flags
			// ------------------------------------------------------------------------------
			// Bits  3-0: Meta data version
			//				0	untrusted meta data which was received via search results
			//				1	trusted meta data, Unicode (strings where not stored correctly)
			//				2	0.49c: trusted meta data, Unicode
			// Bits 31-4: Reserved
			ASSERT(m_uMetaDataVer <= 0x0F);
			uint32 uFlags = m_uMetaDataVer & 0x0F;
			CTag tagFlags(FT_FLAGS, uFlags);
			tagFlags.WriteTagToFile(file);
			++uTagCount;
		}

		// other tags
		for (INT_PTR i = 0; i < m_taglist.GetCount(); ++i) {
			if (m_taglist[i]->IsStr() || m_taglist[i]->IsInt()) {
				m_taglist[i]->WriteTagToFile(file, UTF8strOptBOM);
				++uTagCount;
			}
		}
	}

	file.Seek(uTagCountFilePos, CFile::begin);
	file.WriteUInt32(uTagCount);
	file.Seek(0, CFile::end);

	return true;
}

void CKnownFile::CreateHash(CFile *pFile, uint64 Length, uchar *pMd4HashOut, CAICHHashTree *pShaHashOut)
{
	ASSERT(!Length || pFile);
	ASSERT(pMd4HashOut != NULL || pShaHashOut != NULL);

	uchar   X[64 * 128];
	CCreateHashVisitor hashVisitor(pMd4HashOut, pShaHashOut);

	for (uint64 Required = Length; Required;) {
		UINT len = (UINT)(min(Required, (uint64)_countof(X)) / 64);
		UINT uRead = len ? len * 64 : (UINT)Required;
		VERIFY(pFile->Read(X, uRead) == uRead);
		hashVisitor.OnMappedFileBytes(X, uRead);
		Required -= uRead;
	}

	hashVisitor.Finish(Length);
}

void CKnownFile::CreateHash(HANDLE hFile, uint64 nOffset, uint64 Length, uchar *pMd4HashOut, CAICHHashTree *pShaHashOut)
{
	ASSERT(hFile != NULL && hFile != INVALID_HANDLE_VALUE);
	ASSERT(pMd4HashOut != NULL || pShaHashOut != NULL);

	CCreateHashVisitor hashVisitor(pMd4HashOut, pShaHashOut);
	DWORD dwError = ERROR_SUCCESS;
	if (!VisitMappedFileRange(hFile, nOffset, Length, hashVisitor, &dwError)) {
		if (dwError == ERROR_SUCCESS)
			dwError = ERROR_READ_FAULT;
		CFileException::ThrowOsError((LONG)dwError);
	}
	hashVisitor.Finish(Length);
}

bool CKnownFile::CreateHash(const uchar *pucData, uint32 uSize, uchar *pucHash, CAICHHashTree *pShaHashOut)
{
	try {
		CMemFile file(const_cast<uchar*>(pucData), uSize);
		CreateHash(&file, uSize, pucHash, pShaHashOut);
		return true;
	} catch (CFileException *ex) {
		ex->Delete();
	}
	return false;
}

Packet*	CKnownFile::CreateSrcInfoPacket(const CUpDownClient *forClient, uint8 byRequestedVersion, uint16 nRequestedOptions) const
{
	if (m_ClientUploadList.IsEmpty())
		return NULL;

	if (!md4equ(forClient->GetUploadFileID(), GetFileHash())) {
		// should never happen
		DEBUG_ONLY(DebugLogError(_T("*** %hs - client (%s) upload file \"%s\" does not match file \"%s\""), __FUNCTION__, (LPCTSTR)forClient->DbgGetClientInfo(), (LPCTSTR)DbgGetFileInfo(forClient->GetUploadFileID()), (LPCTSTR)GetFileName()));
		ASSERT(0);
		return NULL;
	}

	// check whether client has either no download status at all or a download status which is valid for this file
	if (!(forClient->GetUpPartCount() == 0 && forClient->GetUpPartStatus() == NULL)
		&& !(forClient->GetUpPartCount() == GetPartCount() && forClient->GetUpPartStatus() != NULL))
	{
		// should never happen
		DEBUG_ONLY(DebugLogError(_T("*** %hs - part count (%u) of client (%s) does not match part count (%u) of file \"%s\""), __FUNCTION__, forClient->GetUpPartCount(), (LPCTSTR)forClient->DbgGetClientInfo(), GetPartCount(), (LPCTSTR)GetFileName()));
		ASSERT(0);
		return NULL;
	}

	CSafeMemFile data(1024);

	uint8 byUsedVersion;
	bool bIsSX2Packet;
	if (forClient->SupportsSourceExchange2() && byRequestedVersion > 0) {
		// the client uses SourceExchange2 and requested the highest version he knows
		// and we send the highest version we know, but of course not higher than his request
		byUsedVersion = min(byRequestedVersion, (uint8)SOURCEEXCHANGE2_VERSION);
		bIsSX2Packet = true;
		data.WriteUInt8(byUsedVersion);

		// we don't support any special SX2 options yet, reserved for later use
		if (nRequestedOptions != 0)
			DebugLogWarning(_T("Client requested unknown options for SourceExchange2: %u (%s)"), nRequestedOptions, (LPCTSTR)forClient->DbgGetClientInfo());
	} else {
		byUsedVersion = forClient->GetSourceExchange1Version();
		bIsSX2Packet = false;
		if (forClient->SupportsSourceExchange2())
			DebugLogWarning(_T("Client which announced to support SX2 sent SX1 packet instead (%s)"), (LPCTSTR)forClient->DbgGetClientInfo());
	}

	uint16 nCount = 0;
	data.WriteHash16(forClient->GetUploadFileID());
	data.WriteUInt16(nCount);
	uint32 cDbgNoSrc = 0;
	for (POSITION pos = m_ClientUploadList.GetHeadPosition(); pos != NULL;) {
		const CUpDownClient *cur_src = m_ClientUploadList.GetNext(pos);
/*
		// some rare issue seen in crash dumps, hopefully fixed already, but to be sure we double check here
		// TODO: remove check next version, as it uses resources and shouldn't be necessary
		if (!theApp.clientlist->IsValidClient(cur_src)) {
#if defined(_BETA) || defined(_DEVBUILD)
			throw new CUserException();
#endif
			ASSERT(0);
			DebugLogError(_T("Invalid client in uploading list for file %s"), (LPCTSTR)GetFileName());
			return NULL;
		}
*/
		if (cur_src->HasLowID() || cur_src == forClient || !(cur_src->GetUploadState() == US_UPLOADING || cur_src->GetUploadState() == US_ONUPLOADQUEUE))
			continue;
		if (!cur_src->IsEd2kClient())
			continue;

		bool bNeeded = false;
		const uint8 *rcvstatus = forClient->GetUpPartStatus();
		if (rcvstatus) {
			ASSERT(forClient->GetUpPartCount() == GetPartCount());
			const uint8 *srcstatus = cur_src->GetUpPartStatus();
			if (srcstatus) {
				ASSERT(cur_src->GetUpPartCount() == GetPartCount());
				if (cur_src->GetUpPartCount() == forClient->GetUpPartCount()) {
					for (INT_PTR x = GetPartCount(); --x >= 0;)
						if (srcstatus[x] && !rcvstatus[x]) {
							// We know the receiving client needs a chunk from this client.
							bNeeded = true;
							break;
						}
				} else {
					// should never happen
					//if (thePrefs.GetVerbose())
					DEBUG_ONLY(DebugLogError(_T("*** %hs - found source (%s) with wrong part count (%u) attached to file \"%s\" (partcount=%u)"), __FUNCTION__, (LPCTSTR)cur_src->DbgGetClientInfo(), cur_src->GetUpPartCount(), (LPCTSTR)GetFileName(), GetPartCount()));
				}
			} else {
				++cDbgNoSrc;
				// This client doesn't support upload chunk status. So just send it and hope for the best.
				bNeeded = true;
			}
		} else {
			ASSERT(forClient->GetUpPartCount() == 0);
			TRACE(_T("%hs, requesting client has no chunk status - %s"), __FUNCTION__, (LPCTSTR)forClient->DbgGetClientInfo());
			// remote client does not support upload chunk status, search sources which have at least one complete part
			// we could even sort the list of sources by available chunks to return as much sources as possible which
			// have the most available chunks. but this could be a noticeable performance problem.
			const uint8 *srcstatus = cur_src->GetUpPartStatus();
			if (srcstatus) {
				ASSERT(cur_src->GetUpPartCount() == GetPartCount());
				for (INT_PTR x = GetPartCount(); --x >= 0;)
					if (srcstatus[x]) {
						// this client has at least one chunk
						bNeeded = true;
						break;
					}
			} else {
				// This client doesn't support upload chunk status. So just send it and hope for the best.
				bNeeded = true;
			}
		}

		if (bNeeded) {
			++nCount;
			uint32 dwID = (byUsedVersion >= 3) ? cur_src->GetUserIDHybrid() : cur_src->GetIP();
			data.WriteUInt32(dwID);
			data.WriteUInt16(cur_src->GetUserPort());
			data.WriteUInt32(cur_src->GetServerIP());
			data.WriteUInt16(cur_src->GetServerPort());
			if (byUsedVersion >= 2)
				data.WriteHash16(cur_src->GetUserHash());
			if (byUsedVersion >= 4) {
				// ConnectSettings - SourceExchange V4
				// 4 Reserved (!)
				// 1 DirectCallback Supported/Available
				// 1 CryptLayer Required
				// 1 CryptLayer Requested
				// 1 CryptLayer Supported
				const uint8 uSupportsCryptLayer = static_cast<uint8>(cur_src->SupportsCryptLayer());
				const uint8 uRequestsCryptLayer = static_cast<uint8>(cur_src->RequestsCryptLayer());
				const uint8 uRequiresCryptLayer = static_cast<uint8>(cur_src->RequiresCryptLayer());
				//const uint8 uDirectUDPCallback = static_cast<uint8>(cur_src->SupportsDirectUDPCallback());
				const uint8 byCryptOptions = /*(uDirectUDPCallback << 3) |*/ (uRequiresCryptLayer << 2) | (uRequestsCryptLayer << 1) | (uSupportsCryptLayer << 0);
				data.WriteUInt8(byCryptOptions);
			}
			if (nCount > 500)
				break;
		}
	}
	TRACE(_T("%hs: Out of %u clients, %u had no valid chunk status\n"), __FUNCTION__, m_ClientUploadList.GetCount(), cDbgNoSrc);
	if (!nCount)
		return 0;
	data.Seek(bIsSX2Packet ? 17 : 16, CFile::begin);
	data.WriteUInt16((uint16)nCount);

	Packet *result = new Packet(data, OP_EMULEPROT);
	result->opcode = bIsSX2Packet ? OP_ANSWERSOURCES2 : OP_ANSWERSOURCES;
	// (1+)16+2+501*(4+2+4+2+16+1) = 14547 (14548) bytes max.
	if (result->size > 354)
		result->PackPacket();
	if (thePrefs.GetDebugSourceExchange())
		AddDebugLogLine(false, _T("SXSend: Client source response SX2=%s, Version=%u; Count=%u, %s, File=\"%s\""), bIsSX2Packet ? _T("Yes") : _T("No"), byUsedVersion, nCount, (LPCTSTR)forClient->DbgGetClientInfo(), (LPCTSTR)GetFileName());
	return result;
}

void CKnownFile::SetFileComment(LPCTSTR pszComment)
{
	if (m_strComment.Compare(pszComment) != 0) {
		SetLastPublishTimeKadNotes(0);
		CIni ini(thePrefs.GetFileCommentsFilePath(), md4str(GetFileHash()));
		ini.WriteStringUTF8(_T("Comment"), pszComment);
		m_strComment = pszComment;

		for (POSITION pos = m_ClientUploadList.GetHeadPosition(); pos != NULL;)
			m_ClientUploadList.GetNext(pos)->SetCommentDirty();
	}
}

void CKnownFile::SetFileRating(UINT uRating)
{
	if (m_uRating != uRating) {
		SetLastPublishTimeKadNotes(0);
		CIni ini(thePrefs.GetFileCommentsFilePath(), md4str(GetFileHash()));
		ini.WriteInt(_T("Rate"), uRating);
		m_uRating = uRating;

		for (POSITION pos = m_ClientUploadList.GetHeadPosition(); pos != NULL;)
			m_ClientUploadList.GetNext(pos)->SetCommentDirty();
	}
}

void CKnownFile::UpdateAutoUpPriority()
{
	if (IsAutoUpPriority()) {
		uint8 curPri = GetUpPriority();
		uint8 newPri;
		if (GetQueuedCount() > 20)
			newPri = PR_LOW;
		else if (GetQueuedCount() > 1)
			newPri = PR_NORMAL;
		else
			newPri = PR_HIGH;
		if (newPri != curPri) {
			SetUpPriority(newPri);
			theApp.emuledlg->sharedfileswnd->sharedfilesctrl.UpdateFile(this);
		}
	}
}

void CKnownFile::SetUpPriority(uint8 iNewUpPriority, bool bSave)
{
	m_iUpPriority = iNewUpPriority;
	ASSERT(m_iUpPriority == PR_VERYLOW || m_iUpPriority == PR_LOW || m_iUpPriority == PR_NORMAL || m_iUpPriority == PR_HIGH || m_iUpPriority == PR_VERYHIGH);

	if (IsPartFile() && bSave)
		static_cast<CPartFile*>(this)->SavePartFile();
}

void CKnownFile::RemoveMetaDataTags(UINT uTagType)
{
	static const struct
	{
		uint8	nID;
		uint8	nType;
	} _aEmuleMetaTags[] =
	{
		{ FT_MEDIA_ARTIST,  TAGTYPE_STRING },
		{ FT_MEDIA_ALBUM,   TAGTYPE_STRING },
		{ FT_MEDIA_TITLE,   TAGTYPE_STRING },
		{ FT_MEDIA_LENGTH,  TAGTYPE_UINT32 },
		{ FT_MEDIA_BITRATE, TAGTYPE_UINT32 },
		{ FT_MEDIA_CODEC,   TAGTYPE_STRING }
	};

	// 05-J�n-2004 [bc]: ed2k and Kad are already full of totally wrong and/or not properly attached meta data.
	// Take the chance to clean any available meta data tags and provide only tags which were determined by us.
	// Remove all meta tags. Never ever trust the meta tags received from other clients or servers.
	for (unsigned j = 0; j < _countof(_aEmuleMetaTags); ++j)
		if (uTagType == 0 || (uTagType == _aEmuleMetaTags[j].nType))
			for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
				const CTag *pTag = m_taglist[i];
				if (pTag->GetNameID() == _aEmuleMetaTags[j].nID) {
					delete pTag;
					m_taglist.RemoveAt(i);
				}
			}

	m_uMetaDataVer = 0;
}

void CKnownFile::RemoveBrokenUnicodeMetaDataTags()
{
	static const struct
	{
		uint8	nID;
		uint8	nType;
	} _aEmuleMetaTags[] =
	{
		{ FT_MEDIA_ARTIST,  TAGTYPE_STRING },
		{ FT_MEDIA_ALBUM,   TAGTYPE_STRING },
		{ FT_MEDIA_TITLE,   TAGTYPE_STRING },
		{ FT_MEDIA_CODEC,   TAGTYPE_STRING }	// This one actually contains only ASCII
	};

	for (int j = (int)_countof(_aEmuleMetaTags); --j >= 0;) {
		const uint8 nameID = _aEmuleMetaTags[j].nID;
		for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
			// Meta data strings of older eMule versions did store Unicode strings as MBCS strings,
			// which means that - depending on the Unicode string content - particular characters
			// got lost. Unicode characters which cannot get converted into the local codepage
			// will get replaced by Windows with a '?' character. So, to estimate if we have a
			// broken Unicode string (due to the conversion between Unicode/MBCS), we search the
			// strings for '?' characters. This is not 100% perfect, as it would also give
			// false results for strings which do contain the '?' character by intention. It also
			// would give wrong results for particular characters which got mapped to ASCII chars
			// due to the conversion from Unicode->MBCS. But at least it prevents us from deleting
			// all the existing meta data strings.
			const CTag *pTag = m_taglist[i];
			if (pTag->GetNameID() == nameID && pTag->IsStr() && _tcschr(pTag->GetStr(), _T('?')) != NULL) {
				delete pTag;
				m_taglist.RemoveAt(i);
			}
		}
	}
}

// Max. string length which is used for string meta tags like TAG_MEDIA_TITLE, TAG_MEDIA_ARTIST, ...
#define	MAX_METADATA_STR_LEN	80

void TruncateED2KMetaData(CString &rstrData)
{
	if (rstrData.Trim().GetLength() > MAX_METADATA_STR_LEN) {
		rstrData.Truncate(MAX_METADATA_STR_LEN);
		rstrData.Trim();
	}
}

void CKnownFile::UpdateMetaDataTags()
{
	// 05-J�n-2004 [bc]: ed2k and Kad are already full of totally wrong and/or improperly
	// attached meta data. Take the chance to clean any available meta data tags
	// and provide only tags which were determined by us.
	RemoveMetaDataTags();

	if (thePrefs.GetExtractMetaData() == 0)
		return;

	const EED2KFileType eFileType = GetED2KFileTypeID(GetFileName());
	if (eFileType != ED2KFT_AUDIO && eFileType != ED2KFT_VIDEO)
		return;

	const CString strFullPath(GetFilePath());
	if (!strFullPath.IsEmpty()) {
		SMediaInfo mi;
		mi.strFileName = GetFileName();
		bool bMediaInfoAvailable = false;
		try {
			if (GetMediaInfoDllInfo(strFullPath, GetFileSize(), &mi, false, true, &bMediaInfoAvailable)) {
				mi.InitFileLength();
				UINT uLengthSec = (UINT)mi.fFileLengthSec;
				CString strCodec;
				uint32 uBitrate = 0;

				if (mi.iVideoStreams > 0) {
					strCodec = mi.strVideoFormat;
					if (mi.video.dwBitRate == _UI32_MAX) {
						if (uLengthSec != 0)
							uBitrate = (uint32)((((uint64)GetFileSize() / uLengthSec) * 8ull + 500) / 1000);
					} else if (mi.video.dwBitRate != 0)
						uBitrate = (mi.video.dwBitRate + 500) / 1000;
				} else if (mi.iAudioStreams > 0) {
					strCodec = mi.strAudioFormat;
					if (mi.audio.nAvgBytesPerSec == _UI32_MAX) {
						if (uLengthSec != 0)
							uBitrate = (uint32)((((uint64)GetFileSize() / uLengthSec) * 8ull + 500) / 1000);
					} else if (mi.audio.nAvgBytesPerSec != 0)
						uBitrate = (uint32)((mi.audio.nAvgBytesPerSec * 8ull + 500) / 1000);
				}

				if (uLengthSec) {
					AddTagUnique(new CTag(FT_MEDIA_LENGTH, (uint32)uLengthSec));
					m_uMetaDataVer = META_DATA_VER;
				}

				if (!strCodec.IsEmpty()) {
					AddTagUnique(new CTag(FT_MEDIA_CODEC, strCodec));
					m_uMetaDataVer = META_DATA_VER;
				}

				if (uBitrate) {
					AddTagUnique(new CTag(FT_MEDIA_BITRATE, uBitrate));
					m_uMetaDataVer = META_DATA_VER;
				}

				TruncateED2KMetaData(mi.strTitle);
				if (!mi.strTitle.IsEmpty()) {
					AddTagUnique(new CTag(FT_MEDIA_TITLE, mi.strTitle));
					m_uMetaDataVer = META_DATA_VER;
				}

				TruncateED2KMetaData(mi.strAuthor);
				if (!mi.strAuthor.IsEmpty()) {
					AddTagUnique(new CTag(FT_MEDIA_ARTIST, mi.strAuthor));
					m_uMetaDataVer = META_DATA_VER;
				}

				TruncateED2KMetaData(mi.strAlbum);
				if (!mi.strAlbum.IsEmpty()) {
					AddTagUnique(new CTag(FT_MEDIA_ALBUM, mi.strAlbum));
					m_uMetaDataVer = META_DATA_VER;
				}

				if (thePrefs.GetVerbose()) {
					AddDebugLogLine(false, _T("Shared meta extraction: \"%s\" length=%u codec=\"%s\" title=\"%s\" artist=\"%s\" album=\"%s\"")
						, (LPCTSTR)strFullPath, uLengthSec, (LPCTSTR)strCodec
						, (LPCTSTR)mi.strTitle, (LPCTSTR)mi.strAuthor, (LPCTSTR)mi.strAlbum);
				}
			} else if (thePrefs.GetVerbose()) {
				if (!bMediaInfoAvailable)
					AddDebugLogLine(false, _T("Shared meta extraction: MediaInfo.dll missing or incompatible for \"%s\""), (LPCTSTR)strFullPath);
				else
					AddDebugLogLine(false, _T("Shared meta extraction: no MediaInfo data found for \"%s\""), (LPCTSTR)strFullPath);
			}
		} catch (...) {
			if (thePrefs.GetVerbose())
				AddDebugLogLine(false, _T("Unhandled exception while extracting file meta data from \"%s\""), (LPCTSTR)strFullPath);
			ASSERT(0);
		}
	}
}

void CKnownFile::SetPublishedED2K(bool val)
{
	m_PublishedED2K = val;
	theApp.emuledlg->sharedfileswnd->sharedfilesctrl.UpdateFile(this);
}

bool CKnownFile::PublishNotes()
{
	time_t tNow = time(NULL);
	if (tNow >= m_lastPublishTimeKadNotes && (!GetFileComment().IsEmpty() || GetFileRating() > 0)) {
		m_lastPublishTimeKadNotes = tNow + KADEMLIAREPUBLISHTIMEN;
		return true;
	}
	return false;
}

bool CKnownFile::PublishSrc()
{
	uint32 lastBuddyIP;
	time_t tNow = time(NULL);
	if (theApp.IsFirewalled()
		&& (Kademlia::CUDPFirewallTester::IsFirewalledUDP(true) || !Kademlia::CUDPFirewallTester::IsVerified()))
	{
		CUpDownClient *buddy = theApp.clientlist->GetBuddy();
		if (!buddy)
			return false;
		lastBuddyIP = buddy->GetIP();
		if (lastBuddyIP != m_lastBuddyIP) {
			SetLastPublishTimeKadSrc(tNow + KADEMLIAREPUBLISHTIMES, lastBuddyIP);
			return true;
		}
	} else
		lastBuddyIP = 0;

	if (tNow < m_lastPublishTimeKadSrc)
		return false;

	SetLastPublishTimeKadSrc(tNow + KADEMLIAREPUBLISHTIMES, lastBuddyIP);
	return true;
}

bool CKnownFile::IsMovie() const
{
	return GetED2KFileTypeID(GetFileName()) == ED2KFT_VIDEO;
}

bool CKnownFile::ImportParts()
{
	// General idea from xmrb's CKnownFile::ImportParts()
	// Unlike xmrb's version which scans entire file designated for import and then tries
	// to match each PARTSIZE bytes with all parts of partfile, my version assumes that
	// in file you're importing all parts stay on same place as they should be in partfile
	// (for example you're importing damaged version of file to recover some parts from ED2K)
	// That way it works much faster and almost always it is what expected from this
	// function. --Rowaa[SR13].
	// CHANGE BY SIROB, Only compute missing full chunk part
	if (!IsPartFile()) {
		LogError(LOG_STATUSBAR, GetResString(IDS_IMPORTPARTS_ERR_ALREADYCOMPLETE));
		return false;
	}

	CPartFile *partfile = static_cast<CPartFile*>(this);
	if (partfile->GetFileOp() == PFOP_IMPORTPARTS) {
		partfile->SetFileOp(PFOP_NONE); //cancel import
		return false;
	}

	CFileDialog dlg(true, NULL, NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY);
	if (dlg.DoModal() != IDOK)
		return false;

	CAddFileThread *addfilethread = (CAddFileThread*)AfxBeginThread(RUNTIME_CLASS(CAddFileThread), THREAD_PRIORITY_LOWEST, 0, CREATE_SUSPENDED);
	if (addfilethread) {
		const CString &pathName = dlg.GetPathName();
		partfile->SetFileOpProgress(0);
		addfilethread->SetValues(theApp.sharedfiles, partfile->GetPath(), partfile->m_hpartfile.GetFileName(), partfile);
		partfile->SetFileOp(addfilethread->SetPartToImport(pathName) ? PFOP_IMPORTPARTS : PFOP_HASHING);
		addfilethread->ResumeThread();
	}
	return true;
}

CString CKnownFile::GetInfoSummary(bool bNoFormatCommands) const
{
	CString strFolder(GetPath());
	unslosh(strFolder);

	CString strAccepts, strRequests;
	strRequests.Format(_T("%u (%u)"), statistic.GetRequests(), statistic.GetAllTimeRequests());
	strAccepts.Format(_T("%u (%u)"), statistic.GetAccepts(), statistic.GetAllTimeAccepts());
	CString strTransferred(CastItoXBytes(statistic.GetTransferred()));
	strTransferred.AppendFormat(_T(" (%s)"), (LPCTSTR)CastItoXBytes(statistic.GetAllTimeTransferred()));
	CString strType(GetFileTypeDisplayStr());
	if (strType.IsEmpty())
		strType += _T('-');
	CString dbgInfo;
#ifdef _DEBUG
	dbgInfo.Format(_T("\nAICH Part HashSet: %s\nAICH Rec HashSet: %s"), m_FileIdentifier.HasExpectedAICHHashCount() ? _T("Yes") : _T("No")
		, IsAICHRecoverHashSetAvailable() ? _T("Yes") : _T("No"));
#endif

	CString info(GetFileName());
	info.AppendFormat(_T("\n")
		_T("eD2K %s %s\n")
		_T("%s: %s\n")
		_T("%s %s\n")
		_T("%s\n")
		_T("%s: %s\n")
		_T("%s: %s\n\n")
		_T("%s: %s\n")
		_T("%s: %s\n")
		_T("%s: %s\n")
		_T("%s: %s%s")
		, (LPCTSTR)GetResString(IDS_FD_HASH), (LPCTSTR)md4str(GetFileHash())
		, (LPCTSTR)GetResString(IDS_AICHHASH), (LPCTSTR)m_FileIdentifier.GetAICHHash().GetString()
		, (LPCTSTR)GetResString(IDS_FD_SIZE), (LPCTSTR)CastItoXBytes(GetFileSize())
		, bNoFormatCommands ? _T("") : _T("<br_head>")
		, (LPCTSTR)GetResString(IDS_TYPE), (LPCTSTR)strType
		, (LPCTSTR)GetResString(IDS_FOLDER), (LPCTSTR)strFolder
		, (LPCTSTR)GetResString(IDS_PRIORITY), (LPCTSTR)GetUpPriorityDisplayString()
		, (LPCTSTR)GetResString(IDS_SF_REQUESTS), (LPCTSTR)strRequests
		, (LPCTSTR)GetResString(IDS_SF_ACCEPTS), (LPCTSTR)strAccepts
		, (LPCTSTR)GetResString(IDS_SF_TRANSFERRED), (LPCTSTR)strTransferred, (LPCTSTR)dbgInfo
	);
	return info;
}

CString CKnownFile::GetUpPriorityDisplayString() const
{
	UINT uid;
	switch (GetUpPriority()) {
	case PR_VERYLOW:
		uid = IDS_PRIOVERYLOW;
		break;
	case PR_LOW:
		uid = IsAutoUpPriority() ? IDS_PRIOAUTOLOW : IDS_PRIOLOW;
		break;
	case PR_NORMAL:
		uid = IsAutoUpPriority() ? IDS_PRIOAUTONORMAL : IDS_PRIONORMAL;
		break;
	case PR_HIGH:
		uid = IsAutoUpPriority() ? IDS_PRIOAUTOHIGH : IDS_PRIOHIGH;
		break;
	case PR_VERYHIGH:
		uid = IDS_PRIORELEASE;
		break;
	default:
		uid = 0;
	}
	return uid ? GetResString(uid) : CString();
}

bool CKnownFile::ShouldPartiallyPurgeFile() const
{
	return thePrefs.DoPartiallyPurgeOldKnownFiles()
		&& m_timeLastSeen > 0
		&& time(NULL) >= m_timeLastSeen + OLDFILES_PARTIALLYPURGE;
}
