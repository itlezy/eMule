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
#include "ring.h"
#include <unordered_map>

struct Requested_Block_Struct;
class CUpDownClient;
typedef CTypedPtrList<CPtrList, CUpDownClient*> CUpDownClientPtrList;

struct UploadingToClient_Struct
{
	UploadingToClient_Struct()
		: m_pClient()
		, m_bIOError()
		, m_bDisableCompression()
	{
	}
	~UploadingToClient_Struct();

	CUpDownClient										*m_pClient;
	CTypedPtrList<CPtrList, Requested_Block_Struct*>	m_BlockRequests_queue;
	CTypedPtrList<CPtrList, Requested_Block_Struct*>	m_DoneBlocks_list;
	CCriticalSection									m_csBlockListsLock; // don't acquire other locks while having this one in any thread other than UploadDiskIOThread or make sure deadlocks are impossible
	bool												m_bIOError;
	bool												m_bDisableCompression;
};
typedef CTypedPtrList<CPtrList, UploadingToClient_Struct*> CUploadingPtrList;

typedef struct
{
	uint64	upBytes;
	uint64	upFriendBytes;
	ULONGLONG timestamp;
} AverageUploadRate;

typedef struct
{
	INT_PTR	slots;
	ULONGLONG timestamp;
} ActiveClientsData;


class CUploadQueue
{

public:
	CUploadQueue();
	~CUploadQueue();

	void	Process();
	void	AddClientToQueue(CUpDownClient *client, bool bIgnoreTimelimit = false);
	bool	HandleUploadSlotTeardown(CUpDownClient *client, LPCTSTR pszReason = NULL, bool removeWaiting = false, bool updatewindow = true, bool earlyabort = false);
	bool	RemoveFromWaitingQueue(CUpDownClient *client, bool updatewindow = true);
	bool	IsOnUploadQueue(CUpDownClient *client)	const	{ return (waitinglist.Find(client) != 0); }
	bool	IsDownloading(const CUpDownClient *client)	const { return (GetUploadingClientStructByClient(client) != NULL); }

	void	UpdateDatarates();
	uint32	GetDatarate() const								{ return datarate; }
	uint32  GetToNetworkDatarate() const;

	INT_PTR	GetWaitingUserCount() const						{ return waitinglist.GetCount(); }
	INT_PTR	GetUploadQueueLength() const					{ return uploadinglist.GetCount(); }
	INT_PTR	GetActiveUploadsCount()	const					{ return m_MaxActiveClientsShortTime; }
	uint32	GetWaitingUserForFileCount(const CSimpleArray<CObject*> &raFiles, bool bOnlyIfChanged);
	uint32	GetDatarateForFile(const CSimpleArray<CObject*> &raFiles) const;
	uint32	GetTargetClientDataRate(bool bMinDatarate) const;

	POSITION GetFirstFromUploadList() const					{ return uploadinglist.GetHeadPosition(); }
	CUpDownClient* GetNextFromUploadList(POSITION &curpos) const { return static_cast<UploadingToClient_Struct*>(uploadinglist.GetNext(curpos))->m_pClient; }
	CUpDownClient* GetQueueClientAt(POSITION &curpos) const	{ return static_cast<UploadingToClient_Struct*>(uploadinglist.GetAt(curpos))->m_pClient; }

	POSITION GetFirstFromWaitingList() const				{ return waitinglist.GetHeadPosition(); }
	CUpDownClient* GetNextFromWaitingList(POSITION &curpos) const { return waitinglist.GetNext(curpos); }
	CUpDownClient* GetWaitClientAt(POSITION &curpos) const	{ return waitinglist.GetAt(curpos); }

	CUpDownClient* GetWaitingClientByIP_UDP(uint32 dwIP, uint16 nUDPPort, bool bIgnorePortOnUniqueIP, bool *pbMultipleIPs = NULL);
	CUpDownClient* GetWaitingClientByIP(uint32 dwIP) const;
	CUpDownClient* GetNextClient(const CUpDownClient *lastclient) const;

	UploadingToClient_Struct* GetUploadingClientStructByClient(const CUpDownClient *pClient) const;

	const CUploadingPtrList& GetUploadListTS(CCriticalSection **outUploadListReadLock);

	void	DeleteAll();
	UINT	GetWaitingPosition(CUpDownClient *client);
	bool	IsClientManagedByUploadQueue(const CUpDownClient *client) const;
	bool	IsClientWaitingForUpload(const CUpDownClient *client) const;
	bool	IsReconnectReserved(const CUpDownClient *client) const;
	bool	IsClientUploadActivating(const CUpDownClient *client) const;
	bool	IsClientUploadActive(const CUpDownClient *client) const;
	bool	HasUploadSlot(const CUpDownClient *client) const;
	bool	HasCollectionUploadSlot(const CUpDownClient *client) const;
	bool	CompleteUploadActivation(CUpDownClient *client);

	uint32	GetSuccessfullUpCount() const					{ return successfullupcount; }
	uint32	GetFailedUpCount() const						{ return failedupcount; }
	uint32	GetAverageUpTime() const;

	CUpDownClient* FindBestClientInQueue();

	CUpDownClientPtrList waitinglist;

protected:
	void		RemoveFromWaitingQueue(POSITION pos, bool updatewindow);
	bool		StartNextUpload(LPCTSTR pszReason);

	static VOID CALLBACK UploadTimer(HWND hWnd, UINT nMsg, UINT_PTR nId, DWORD dwTime) noexcept;

private:
	/** Stable scheduler inputs used for one admission or retention decision. */
	struct UploadSchedulingSnapshot
	{
		INT_PTR	uploadSlots = 0;
		INT_PTR	softMaxUploadSlots = 0;
		INT_PTR	highestFullyActivatedSlots = 0;
		uint32	budgetBytesPerSec = 0;
		uint32	toNetworkDatarate = 0;
		uint32	targetPerSlot = 0;
		bool	hasWaitingClients = false;
		bool	throttlerWantsMoreSlots = false;
	};

	/** Queue-owned phases for one client's upload slot lifecycle. */
	enum class EUploadSlotPhase : uint8
	{
		None,
		Waiting,
		ReservedForReconnect,
		Activating,
		Active,
		Retiring
	};

	struct UploadSlotPhaseState
	{
		EUploadSlotPhase phase = EUploadSlotPhase::None;
		bool collectionSlot = false;
	};

	/** Reasons for ending an active upload slot. */
	enum class EUploadSlotEndReason : uint8
	{
		None,
		CollectionSlotFileSwitched,
		BroadbandSlowSlotRecycle,
		BroadbandSessionTransferLimit,
		BroadbandSessionTimeLimit
	};

	/** Result of evaluating whether an active upload slot should end. */
	struct UploadSlotEndDecision
	{
		bool					shouldEnd = false;
		bool					requeue = false;
		EUploadSlotEndReason	reason = EUploadSlotEndReason::None;
	};

	/** Returns true if the client can immediately take an upload slot. */
	bool	IsClientEligibleForImmediateUpload(const CUpDownClient *client) const;
	EUploadSlotPhase GetUploadSlotPhase(const CUpDownClient *client) const;
	void	SetUploadSlotPhase(CUpDownClient *client, EUploadSlotPhase phase);
	void	SetCollectionUploadSlot(CUpDownClient *client, bool bValue);
	void	SyncLegacyUploadState(CUpDownClient *client);
	void	ClearUploadSlotPhase(CUpDownClient *client);
	void	UpdateReconnectReservation(CUpDownClient *reservedClient);
	/** Adds a client to the waiting queue and performs the required side effects. */
	void	AddClientToWaitingList(CUpDownClient *client);
	/** Activates the specified client into an upload slot. */
	bool	ActivateUploadClient(CUpDownClient *client, LPCTSTR pszReason, bool bRemoveFromWaitingQueue);
	/** Removes an active upload slot and performs all associated side effects. */
	bool	RemoveFromUploadQueue(CUpDownClient *client, LPCTSTR pszReason = NULL, bool updatewindow = true, bool earlyabort = false);
	/** Captures the scheduler inputs used for one admission or retention decision. */
	UploadSchedulingSnapshot CaptureSchedulingSnapshot(bool throttlerWantsMoreSlots) const;
	/** Removes an active upload slot and requeues the client when requested. */
	bool	EndUploadSession(CUpDownClient *client, const UploadSlotEndDecision &decision);
	/** Returns true when the scheduler should start another upload slot now. */
	bool	ShouldOpenUploadSlot(const UploadSchedulingSnapshot &snapshot, bool allowEmptyWaitingQueue, bool addOnNextConnect) const;
	/** Evaluates whether the active upload slot should be ended. */
	UploadSlotEndDecision EvaluateUploadSlotEnd(CUpDownClient *client, const UploadSchedulingSnapshot &snapshot);
	/** Converts an upload-slot end reason into the removal log text. */
	static LPCTSTR GetUploadSlotEndReasonText(EUploadSlotEndReason eReason);
	uint32	GetEffectiveUploadBudgetBytesPerSec() const;
	INT_PTR	GetSoftMaxUploadSlots() const;
	uint32	GetTargetClientDataRateBroadband() const;
	uint32	GetSlowUploadRateThreshold() const;
	bool	ShouldTrackSlowUploadSlots(const UploadSchedulingSnapshot &snapshot) const;
	void	UpdateActiveClientsInfo(ULONGLONG curTick);
	void	RemoveFromWaitingQueueInternal(POSITION pos, bool updatewindow, bool preserveSlotPhase);

	void InsertInUploadingList(CUpDownClient *newclient, bool bNoLocking);
	void InsertInUploadingList(UploadingToClient_Struct *pNewClientUploadStruct, bool bNoLocking);
	float GetAverageCombinedFilePrioAndCredit();

	CUploadingPtrList	uploadinglist;
	// This lock ensures that only the main thread writes the uploading list,
	// other threads need to fetch the lock if they want to read (but are not allowed to write).
	// Don't acquire other locks while having this one in any thread
	// other than UploadDiskIOThread or make sure deadlocks are impossible
	CCriticalSection	m_csUploadListMainThrdWriteOtherThrdsRead;

	// By BadWolf - Accurate Speed Measurement
	CRing<AverageUploadRate> average_ur_hist;
	CRing<ActiveClientsData> activeClients_hist;
	uint32	datarate;		//data rate sent to network (including friends)
	uint32  friendDatarate;	//data rate sent to friends (included in above total)
	// By BadWolf - Accurate Speed Measurement

	UINT_PTR h_timer; //100 ms
	uint32	successfullupcount;
	uint32	failedupcount;
	uint32	totaluploadtime;
	ULONGLONG m_nLastStartUpload;

	ULONGLONG m_dwLastCalculatedAverageCombinedFilePrioAndCredit;
	float	m_fAverageCombinedFilePrioAndCredit;
	INT_PTR	m_iHighestNumberOfFullyActivatedSlotsSinceLastCall;
	INT_PTR	m_MaxActiveClients;
	INT_PTR	m_MaxActiveClientsShortTime;

	uint64	m_sendingBytes;
	uint64	m_average_ur_sum;
	ULONGLONG m_lastCalculatedDataRateTick;

	ULONGLONG m_dwLastResortedUploadSlots;
	bool	m_bStatisticsWaitingListDirty;
	bool	m_bThrottlerWantsMoreSlotsHint;
	std::unordered_map<const CUpDownClient*, UploadSlotPhaseState> m_uploadSlotPhaseStates;
};
