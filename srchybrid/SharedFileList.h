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
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include "SharedStartupCachePolicy.h"
#include "MapKey.h"
#include "FileIdentifier.h"

class CKnownFileList;
class CServerConnect;
class CPartFile;
class CKnownFile;
class CPublishKeywordList;
class CSafeMemFile;
class CServer;
class CCollection;
typedef CMap<CCKey, const CCKey&, CKnownFile*, CKnownFile*> CKnownFilesMap;

struct UnknownFile_Struct
{
	CString strName;
	CString strDirectory;
	CString strSharedDirectory;
	ULONGLONG ullQueuedTimestampUs = 0;
};

class CSharedFileList
{
	friend class CSharedFilesCtrl;
	friend class CClientReqSocket;

public:
	/**
	 * @brief Exposes the current background startup-cache saver phase for shutdown UI and diagnostics.
	 */
	enum class StartupCacheSavePhase : uint8
	{
		Idle,
		BuildingRecords,
		WritingFile,
		ApplyingResult
	};

	/**
	 * @brief Reports the latest background startup-cache save progress snapshot.
	 */
	struct StartupCacheSaveProgress
	{
		StartupCacheSavePhase ePhase = StartupCacheSavePhase::Idle;
		ULONGLONG uCompletedDirectories = 0;
		ULONGLONG uTotalDirectories = 0;
		bool bRunning = false;
		bool bDirty = false;
		bool bWaitingForFollowUp = false;
	};

	explicit CSharedFileList(CServerConnect *in_server);
	~CSharedFileList();
	CSharedFileList(const CSharedFileList&) = delete;
	CSharedFileList& operator=(const CSharedFileList&) = delete;

	void	SendListToServer();
	void	Reload();
	void	Save() const;
	void	Process();
	void	Publish();
	void	RebuildMetaData();
	void	DeletePartFileInstances() const;
	void	PublishNextTurn()						{ m_lastPublishED2KFlag = true; }
	void	ClearED2KPublishInfo();
	void	ClearKadSourcePublishInfo();

	static void	CreateOfferedFilePacket(CKnownFile *cur_file, CSafeMemFile &files, const CServer *pServer, const CUpDownClient *pClient = NULL);

	bool	SafeAddKFile(CKnownFile *toadd, bool bOnlyAdd = false);
	void	RepublishFile(CKnownFile *pFile);
	void	SetOutputCtrl(CSharedFilesCtrl *in_ctrl);
	/**
	 * @brief Starts deferred shared-file hashing after startup UI initialization finishes.
	 */
	void	StartDeferredHashing();
	/**
	 * @brief Reports whether the initial startup-only deferred shared hash queue is still draining.
	 */
	bool	IsStartupDeferredHashingActive() const	{ return m_bStartupDeferredHashingActive; }
	/**
	 * @brief Starts one background startup-cache save when the cache is dirty and no worker is active.
	 */
	bool	RequestStartupCacheSave(bool bImmediate = false);
	/**
	 * @brief Reports whether startup-cache persistence still has dirty or in-flight work left.
	 */
	bool	HasPendingStartupCacheSaveWork() const;
	/**
	 * @brief Reports whether the background startup-cache saver is currently running.
	 */
	bool	IsStartupCacheSaveRunning() const;
	/**
	 * @brief Copies the latest startup-cache save progress snapshot for shutdown UI polling.
	 */
	void	GetStartupCacheSaveProgress(StartupCacheSaveProgress &rProgress) const;
	/**
	 * @brief Applies one worker-produced startup-cache save result on the UI thread.
	 */
	void	HandleStartupCacheSaveCompletion(void *pResult);
	bool	RemoveFile(CKnownFile *pFile, bool bDeleted = false);	// removes a specific shared file from the list
	void	UpdateFile(const CKnownFile *toupdate);
	void	AddFileFromNewlyCreatedCollection(const CString &rstrFilePath)	{ CheckAndAddSingleFile(rstrFilePath); }

	// GUI is not initially updated
	bool	AddSingleSharedFile(const CString &rstrFilePath, bool bNoUpdate = false); // includes updating sharing preferences, calls CheckAndAddSingleSharedFile afterwards
	bool	AddSingleSharedDirectory(const CString &rstrFilePath, bool bNoUpdate = false);
	bool	ExcludeFile(const CString &strFilePath);	// excludes a specific file from being shared and removes it from the list if it exists

	void	AddKeywords(CKnownFile *pFile);
	void	RemoveKeywords(CKnownFile *pFile);

	void	CopySharedFileMap(CKnownFilesMap &Files_Map);

	CKnownFile*	GetFileByID(const uchar *hash) const;
	CKnownFile*	GetFileByIdentifier(const CFileIdentifierBase &rFileIdent, bool bStrict = false) const;
	CKnownFile*	GetFileByIndex(INT_PTR index) const; // slow
	CKnownFile*	GetFileNext(POSITION &pos) const;
	CKnownFile*	GetFileByAICH(const CAICHHash &rHash) const; // slow

	bool	IsFilePtrInList(const CKnownFile *file) const; // slow
	bool	IsUnsharedFile(const uchar *auFileHash) const;
	bool	ShouldBeShared(const CString &sDirPath, LPCTSTR const pFilePath, bool bMustBeShared) const;
	bool	ContainsSingleSharedFiles(const CString &strDirectory) const; // includes subdirs
	CString	GetPseudoDirName(const CString &strDirectoryName);
	CString	GetDirNameByPseudo(const CString &strPseudoName) const;

	uint64	GetDatasize(uint64 &pbytesLargest) const;
	INT_PTR	GetCount()								{ return m_Files_map.GetCount(); }
	INT_PTR	GetHashingCount()						{ return waitingforhash_list.GetCount() + currentlyhashing_list.GetCount(); }
	bool	ProbablyHaveSingleSharedFiles() const	{ return bHaveSingleSharedFiles && !m_liSingleSharedFiles.IsEmpty(); } // might not be always up-to-date, could give false "true"s, not a problem currently

	void	HashFailed(UnknownFile_Struct *hashed);	// SLUGFILLER: SafeHash
	void	FileHashingFinished(CKnownFile *file);

	bool	GetPopularityRank(const CKnownFile *pFile, uint32 &rnOutSession, uint32 &rnOutTotal) const;

	CCriticalSection m_mutWriteList; // don't acquire other locks while having this one in the main thread or make sure deadlocks are impossible
	static uint8 GetRealPrio(uint8 in)				{ return (in < 4) ? in + 1 : 0; }
	void	ResetPseudoDirNames()					{ m_mapPseudoDirNames.RemoveAll(); }

protected:
	bool	AddFile(CKnownFile *pFile);
	void	AddFilesFromDirectory(const CString &rstrDirectory);
	void	FindSharedFiles(bool bAllowStartupCache = true);
	bool	AddKnownSharedFile(CKnownFile *pFile, const CString &strFoundDirectory, const CString &strFoundFilePath);

	void	HashNextFile();
	bool	IsHashing(const CString &rstrDirectory, const CString &rstrName);
	void	RemoveFromHashing(const CKnownFile *hashed);
	void	LoadSingleSharedFilesList();

	void	CheckAndAddSingleFile(const CString &strDirectory, const WIN32_FIND_DATA &findData);
	bool	CheckAndAddSingleFile(const CString &rstrFilePath); // add specific files without editing sharing preferences

private:
	using SharedStartupCacheRecordMap = std::unordered_map<std::wstring, SharedStartupCachePolicy::DirectoryRecord>;
	using SharedStartupCacheVolumeRecordMap = std::unordered_map<std::wstring, SharedStartupCachePolicy::VolumeRecord>;

	/**
	 * @brief Captures the current startup-validation state for one shared directory.
	 */
	struct DirectoryStartupState
	{
		LongPathSeams::FileSystemObjectIdentity identity = {};
		bool bHasIdentity = false;
		LONGLONG utcDirectoryDate = -1;
		bool bHasTrustedNtfsJournalState = false;
		SharedStartupCachePolicy::VolumeRecord volumeRecord = {};
		LongPathSeams::UsnFileReference directoryFileReference = {};
	};

	/**
	 * @brief Memoizes the journal-delta decision for one cached NTFS volume during startup.
	 */
	struct StartupCacheVolumeValidationState
	{
		bool bInitialized = false;
		bool bRescanAllDirectories = false;
		std::unordered_set<LongPathSeams::UsnFileReference, LongPathSeams::UsnFileReferenceHasher> changedDirectoryFileReferences;
	};

	/**
	 * @brief Aggregates one shared-directory scan pass into stable machine-readable counters.
	 */
	struct StartupScanStats
	{
		ULONGLONG uRequestedDirectories = 0;
		ULONGLONG uDedupedDirectories = 0;
		ULONGLONG uDuplicateDirectories = 0;
		ULONGLONG uDirectoriesFromCache = 0;
		ULONGLONG uDirectoriesRescanned = 0;
		ULONGLONG uInaccessibleDirectories = 0;
		ULONGLONG uDirectoriesOver248Chars = 0;
		ULONGLONG uPathsOver260Chars = 0;
		ULONGLONG uKnownFilesAccepted = 0;
		ULONGLONG uFilesQueuedForHash = 0;
		ULONGLONG uFilesIgnored = 0;
	};

	/**
	 * @brief Captures one shared directory block for worker-side startup-cache persistence.
	 */
	struct StartupCacheSaveDirectorySnapshot
	{
		CString strDirectoryPath;
		std::vector<SharedStartupCachePolicy::FileRecord> files;
		bool bHasPendingHash = false;
	};

	/**
	 * @brief Owns one immutable worker-side startup-cache persistence request.
	 */
	struct StartupCacheSaveSnapshot
	{
		CString strCachePath;
		std::vector<StartupCacheSaveDirectorySnapshot> directories;
	};

	/**
	 * @brief Owns one completed worker-side startup-cache save result until the UI thread applies it.
	 */
	struct StartupCacheSaveResult
	{
		bool bWriteSucceeded = false;
		ULONGLONG ullCompletedTick = 0;
		SharedStartupCacheRecordMap records;
		SharedStartupCacheVolumeRecordMap volumeRecords;
	};

	/**
	 * @brief Carries one owner pointer plus immutable snapshot into the background save worker.
	 */
	struct StartupCacheSaveThreadRequest
	{
		CSharedFileList *pOwner = NULL;
		StartupCacheSaveSnapshot snapshot;
	};

	void	AddDirectory(const CString &strDir, CStringList &dirlist, std::unordered_set<std::wstring> &rAddedDirectoryKeys, bool bAllowStartupCache);
	void	CollectSharedDirectories(CStringList &dirlist) const;
	bool	TryLoadStartupCache();
	void	MarkStartupCacheDirty();
	bool	TryRehydrateSharedDirectoryFromCache(const CString &strDirectory);
	/**
	 * @brief Builds one startup-cache directory block and records any shared NTFS volume guard it depends on.
	 */
	bool	BuildStartupCacheRecord(const CString &strDirectory, SharedStartupCachePolicy::DirectoryRecord &rRecord, SharedStartupCacheVolumeRecordMap &rVolumeRecords) const;
	bool	HasPendingHashForDirectory(const CString &strDirectory) const;
	/**
	 * @brief Resolves the current directory state used by both generic and NTFS startup-cache validation.
	 */
	bool	GetDirectoryStartupState(const CString &strDirectory, DirectoryStartupState &rState) const;
	bool	GetFileStartupState(const CString &strFilePath, LONGLONG &rUtcFileDate, ULONGLONG &rullFileSize) const;
	/**
	 * @brief Computes the one-scan-per-volume NTFS journal verdict for cached directories on the same local volume.
	 */
	bool	EnsureStartupCacheVolumeValidation(const CString &strDirectory, const SharedStartupCachePolicy::DirectoryRecord &rRecord);
	/**
	 * @brief Collects every cached directory reference guarded by one shared NTFS volume record.
	 */
	void	CollectTrackedStartupCacheDirectoryRefs(const SharedStartupCachePolicy::VolumeRecord &rVolumeRecord, std::unordered_set<LongPathSeams::UsnFileReference, LongPathSeams::UsnFileReferenceHasher> &rTrackedDirectoryRefs) const;
	static CString GetStartupCachePath();
	static std::wstring MakeStartupCacheKey(const CString &strDirectory);
	static std::wstring MakeStartupCacheVolumeKey(const CString &strVolumeKey);
	static bool ReadStartupCacheString(CSafeBufferedFile &file, CString &rValue);
	static void WriteStartupCacheString(CSafeBufferedFile &file, const CString &strValue);
	static UINT AFX_CDECL StartupCacheSaveThreadProc(LPVOID pParam);
	bool	CaptureStartupCacheSaveSnapshot(StartupCacheSaveSnapshot &rSnapshot) const;
	bool	BuildStartupCacheRecordFromSnapshot(const StartupCacheSaveDirectorySnapshot &rDirectory, SharedStartupCachePolicy::DirectoryRecord &rRecord, SharedStartupCacheVolumeRecordMap &rVolumeRecords) const;
	void	RunStartupCacheSaveWorker(const StartupCacheSaveSnapshot &rSnapshot, StartupCacheSaveResult &rResult);
	static bool	WriteStartupCacheFile(const CString &strFullPath, const SharedStartupCacheVolumeRecordMap &rVolumeRecords, const std::vector<SharedStartupCachePolicy::DirectoryRecord> &rRecords);
	void	UpdateStartupCacheSaveProgress(StartupCacheSavePhase ePhase, ULONGLONG uCompletedDirectories, ULONGLONG uTotalDirectories);

	CKnownFilesMap m_Files_map;
	CMap<CSKey, const CSKey&, bool, bool>		 m_UnsharedFiles_map;
	CMapStringToString m_mapPseudoDirNames;
	CPublishKeywordList *m_keywords;
	CTypedPtrList<CPtrList, UnknownFile_Struct*> waitingforhash_list;
	CTypedPtrList<CPtrList, UnknownFile_Struct*> currentlyhashing_list;	// SLUGFILLER: SafeHash
	CServerConnect	 *server;
	CSharedFilesCtrl *output;
	CStringList		 m_liSingleSharedFiles;
	CStringList		 m_liSingleExcludedFiles;
#if defined(_BETA) || defined(_DEVBUILD)
	CString			m_strBetaFileName; //beta test file name
#endif

	INT_PTR	m_currFileSrc;
	INT_PTR	m_currFileNotes;
	time_t	m_lastPublishKadSrc;
	time_t	m_lastPublishKadNotes;
	ULONGLONG m_lastPublishED2K;
	bool	m_lastPublishED2KFlag;
	bool	bHaveSingleSharedFiles;
	bool	m_bStartupCacheDirty;
	bool	m_bStartupDeferredHashingActive;
	ULONGLONG m_nLastStartupCacheSave;
	ULONGLONG m_nStartupCacheDirtyTick;
	ULONGLONG m_uStartupHashCompletedFiles;
	ULONGLONG m_uStartupHashFailedFiles;
	StartupScanStats m_startupScanStats;
	SharedStartupCacheRecordMap m_startupCacheRecords;
	SharedStartupCacheVolumeRecordMap m_startupCacheVolumes;
	std::unordered_map<std::wstring, StartupCacheVolumeValidationState> m_startupCacheVolumeValidation;
	mutable CCriticalSection m_mutStartupCacheSave;
	bool	m_bStartupCacheSaveRunning;
	bool	m_bStartupCacheSaveRunAfterCurrent;
	StartupCacheSavePhase m_eStartupCacheSavePhase;
	ULONGLONG m_uStartupCacheSaveDirectoriesDone;
	ULONGLONG m_uStartupCacheSaveDirectoriesTotal;
};

class CAddFileThread : public CWinThread
{
	DECLARE_DYNCREATE(CAddFileThread)
protected:
	CAddFileThread();
public:
	virtual BOOL InitInstance();
	virtual int	Run();
	void	SetValues(CSharedFileList *pOwner, LPCTSTR directory, LPCTSTR filename, LPCTSTR strSharedDir, CPartFile *partfile = NULL);
	bool	ImportParts();
	uint16	SetPartToImport(LPCTSTR import);
private:
	CSharedFileList	*m_pOwner;
	CPartFile	*m_partfile;
	CString		m_strDirectory;
	CString		m_strFilename;
	CString		m_strSharedDir;
	CString		m_strImport;
	CArray<uint16, uint16>	m_PartsToImport;
};
