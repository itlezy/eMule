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
#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

struct Requested_Block_Struct;
class CUpDownClient;
class CKnownFile;
typedef CTypedPtrList<CPtrList, CUpDownClient*> CUpDownClientPtrList;

struct UploadSession
{
	explicit UploadSession(CUpDownClient *client = NULL)
		: client(client)
		, ioError()
		, disableCompression()
		, slotNumber()
	{
	}
	~UploadSession();

	CUpDownClient										*client;
	CTypedPtrList<CPtrList, Requested_Block_Struct*>	blockRequests;
	CTypedPtrList<CPtrList, Requested_Block_Struct*>	completedBlocks;
	CCriticalSection									blockListsLock; // don't acquire other locks while having this one in any thread other than UploadDiskIOThread or make sure deadlocks are impossible
	bool												ioError;
	bool												disableCompression;
	UINT												slotNumber;
};
typedef std::shared_ptr<UploadSession> UploadSessionPtr;

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
	class CWaitingListCompatView
	{
	public:
		explicit CWaitingListCompatView(const CUploadQueue *owner = NULL) noexcept
			: m_owner(owner)
		{
		}

		POSITION GetHeadPosition() const;
		CUpDownClient* GetNext(POSITION &pos) const;
		INT_PTR GetCount() const;

	private:
		const CUploadQueue *m_owner;
		mutable CCriticalSection m_csThreadSnapshots;
		mutable std::unordered_map<DWORD, std::shared_ptr<const void>> m_threadSnapshots;
	};

	CUploadQueue();
	~CUploadQueue();
	CUploadQueue(const CUploadQueue&) = delete;
	CUploadQueue& operator=(const CUploadQueue&) = delete;

	void	Process();
	void	NoteUploadRequestSeen(CUpDownClient *client) const;
	void	AddClientToQueue(CUpDownClient *client, bool bIgnoreTimelimit = false);
	bool	HandleUploadSlotTeardown(CUpDownClient *client, LPCTSTR pszReason = NULL, bool removeWaiting = false, bool updatewindow = true, bool earlyabort = false);
	bool	IsOnUploadQueue(CUpDownClient *client)	const	{ return IsClientWaitingForUpload(client); }
	bool	IsDownloading(const CUpDownClient *client)	const;

	void	UpdateDatarates();
	uint32	GetDatarate() const								{ return datarate; }
	uint32  GetToNetworkDatarate() const;

	INT_PTR	GetWaitingUserCount() const;
	INT_PTR	GetUploadQueueLength() const;
	INT_PTR	GetActiveUploadsCount()	const					{ return m_MaxActiveClientsShortTime; }
	uint32	GetWaitingUserForFileCount(const CSimpleArray<CObject*> &raFiles, bool bOnlyIfChanged);
	uint32	GetDatarateForFile(const CSimpleArray<CObject*> &raFiles) const;
	uint32	GetTargetClientDataRate(bool bMinDatarate) const;

	CUpDownClient* GetWaitingClientByIP_UDP(uint32 dwIP, uint16 nUDPPort, bool bIgnorePortOnUniqueIP, bool *pbMultipleIPs = NULL);
	CUpDownClient* GetWaitingClientByIP(uint32 dwIP) const;
	void	GetWaitingClientsInRankOrder(std::vector<CUpDownClient*> &clients);
	void	GetActiveUploadClientsInSlotOrder(std::vector<CUpDownClient*> &clients) const;
	int		CompareWaitingClientsByRank(const CUpDownClient *left, const CUpDownClient *right) const;
	float	GetWaitingClientCreditFactor(const CUpDownClient *client) const;

	void	DeleteAll();
	UINT	GetWaitingPosition(const CUpDownClient *client);
	bool	IsClientManagedByUploadQueue(const CUpDownClient *client) const;
	bool	IsClientWaitingForUpload(const CUpDownClient *client) const;
	bool	IsClientUploadActivating(const CUpDownClient *client) const;
	bool	IsClientUploadActive(const CUpDownClient *client) const;
	bool	HasUploadSlot(const CUpDownClient *client) const;
	bool	CompleteUploadActivation(CUpDownClient *client);

	uint32	GetSuccessfullUpCount() const					{ return successfullupcount; }
	uint32	GetFailedUpCount() const						{ return failedupcount; }
	uint32	GetAverageUpTime() const;

	CWaitingListCompatView waitinglist;

protected:
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
		Activating,
		Active,
		Retiring
	};

	/** Reasons for ending an active upload slot. */
	enum class EUploadSlotEndReason : uint8
	{
		None,
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

	struct ClientQueueEntry
	{
		EUploadSlotPhase phase = EUploadSlotPhase::None;
		UploadSessionPtr session;
	};

	struct WaitingUserHashKey
	{
		static constexpr size_t HashBytes = 16;
		std::array<uchar, HashBytes> bytes{};

		bool operator==(const WaitingUserHashKey &other) const noexcept
		{
			return bytes == other.bytes;
		}
	};

	struct WaitingUserHashKeyHasher
	{
		size_t operator()(const WaitingUserHashKey &key) const noexcept
		{
			size_t hash = 1469598103934665603ull;
			for (uchar byte : key.bytes)
				hash = (hash ^ byte) * 1099511628211ull;
			return hash;
		}
	};

	struct WaitingQueueSnapshot
	{
		CUpDownClientPtrList compatibilityMembers;
		std::vector<CUpDownClient*> memberClients;
		std::vector<CUpDownClient*> rankedClients;
		std::unordered_map<const CUpDownClient*, UINT> positionByClient;
	};

	struct ActiveUploadSnapshot
	{
		std::vector<CUpDownClient*> activeClients;
		std::unordered_map<const CUpDownClient*, UINT> slotNumberByClient;
	};

	/** Returns true if the client can immediately take an upload slot. */
	bool	IsClientEligibleForImmediateUpload(const CUpDownClient *client) const;
	const ClientQueueEntry* FindClientQueueEntry(const CUpDownClient *client) const;
	ClientQueueEntry* FindClientQueueEntry(const CUpDownClient *client);
	ClientQueueEntry& EnsureClientQueueEntry(CUpDownClient *client);
	EUploadSlotPhase GetUploadSlotPhase(const CUpDownClient *client) const;
	void	SyncLegacyUploadState(CUpDownClient *client, EUploadSlotPhase phase);
	void	EnterWaitingState(CUpDownClient *client);
	void	BeginActivationState(CUpDownClient *client);
	void	MarkActiveState(CUpDownClient *client);
	void	BeginRetiringState(CUpDownClient *client);
	void	ClearClientQueueEntry(CUpDownClient *client);
	const CKnownFile* GetRequestedUploadFile(const CUpDownClient *client) const;
	bool	ShouldPurgeWaitingClient(const CUpDownClient *client, ULONGLONG curTick) const;
	bool	ShouldRejectLowIdQueueRequest(const CUpDownClient *client) const;
	bool	IsHigherPriorityWaitingClient(const CUpDownClient *candidate, const CUpDownClient *currentBest) const;
	void	PurgeStaleWaitingClients(ULONGLONG curTick);
	static WaitingUserHashKey MakeWaitingUserHashKey(const CUpDownClient *client);
	void	RebuildWaitingIndexes();
	void	CollectDuplicateWaitingCandidates(const CUpDownClient *client, std::vector<CUpDownClient*> &candidates) const;
	void	PublishWaitingSnapshot();
	std::shared_ptr<const WaitingQueueSnapshot> GetWaitingSnapshot() const;
	void	PublishActiveUploadSnapshot();
	std::shared_ptr<const ActiveUploadSnapshot> GetActiveUploadSnapshot() const;
	CUpDownClient* SelectNextWaitingClient();
	CUpDownClient* FindLowestPriorityWaitingClient(const CUpDownClient *excludeClient = NULL);
	bool	PassesQueueAdmissionLimit(const CUpDownClient *client);
	void	TrackExternalQueueRequest(CUpDownClient *client, bool bIgnoreTimelimit) const;
	void	RecordExternalQueueRequestStat(CUpDownClient *client) const;
	bool	HasTooManyQueuedClientsFromSameIP(const CUpDownClient *client, uint16 cSameIP) const;
	bool	RefreshQueuedClient(CUpDownClient *client);
	bool	ResolveExternalQueueConflicts(CUpDownClient *client, uint16 &cSameIP);
	bool	TryAcceptActiveUploadRequest(CUpDownClient *client) const;
	bool	AdmitClientToQueue(CUpDownClient *client, LPCTSTR pszImmediateActivationReason);
	bool	RequeueClientAfterUploadSession(CUpDownClient *client);
	INT_PTR	GetWaitingMemberCount() const					{ return static_cast<INT_PTR>(m_waitingClients.size()); }
	bool	HasWaitingMember(const CUpDownClient *client) const;
	bool	FindWaitingClientIndex(const CUpDownClient *client, size_t &index) const;
	UploadSessionPtr GetUploadSession(const CUpDownClient *client) const;
	std::vector<UploadSessionPtr> GetActiveUploadSessions() const;
	bool	IsCurrentUploadSession(const UploadSessionPtr &session) const;
	bool	AddActiveUploadSession(CUpDownClient *client);
	void	ResetUploadSessionState(const UploadSessionPtr &session);
	bool	RemoveActiveUploadSession(CUpDownClient *client);
	/** Adds a client to the waiting queue and performs the required side effects. */
	void	AddClientToWaitingList(CUpDownClient *client);
	/** Activates the specified client into an upload slot. */
	bool	ActivateUploadClient(CUpDownClient *client, LPCTSTR pszReason, bool bRemoveFromWaitingQueue);
	/** Removes an active upload slot and performs all associated side effects. */
	bool	RemoveActiveUploadSlot(CUpDownClient *client, LPCTSTR pszReason = NULL, bool updatewindow = true, bool earlyabort = false);
	bool	RemoveWaitingClient(CUpDownClient *client, bool updatewindow = true);
	/** Captures the scheduler inputs used for one admission or retention decision. */
	UploadSchedulingSnapshot CaptureSchedulingSnapshot(bool throttlerWantsMoreSlots) const;
	/** Removes an active upload slot and requeues the client when requested. */
	bool	EndUploadSession(CUpDownClient *client, const UploadSlotEndDecision &decision);
	/** Returns true when the scheduler should start another upload slot now. */
	bool	ShouldOpenUploadSlot(const UploadSchedulingSnapshot &snapshot, bool allowEmptyWaitingQueue) const;
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
	bool	RemoveWaitingClientAt(size_t index, bool updatewindow, bool publishSnapshot = true);
	CCriticalSection	m_csWaitingSnapshotRead;
	CCriticalSection	m_csActiveUploadState;

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

	INT_PTR	m_iHighestNumberOfFullyActivatedSlotsSinceLastCall;
	INT_PTR	m_MaxActiveClients;
	INT_PTR	m_MaxActiveClientsShortTime;

	uint64	m_sendingBytes;
	uint64	m_average_ur_sum;
	ULONGLONG m_lastCalculatedDataRateTick;

	bool	m_bStatisticsWaitingListDirty;
	bool	m_bThrottlerWantsMoreSlotsHint;
	std::unordered_map<const CUpDownClient*, ClientQueueEntry> m_clientEntries;
	std::vector<CUpDownClient*> m_waitingClients;
	std::unordered_map<const CUpDownClient*, size_t> m_waitingClientIndexes;
	std::unordered_map<uint32, std::vector<CUpDownClient*>> m_waitingClientsByIP;
	std::unordered_map<uint16, std::vector<CUpDownClient*>> m_waitingClientsByUserPort;
	std::unordered_map<uint16, std::vector<CUpDownClient*>> m_waitingClientsByKadPort;
	std::unordered_map<uint32, std::vector<CUpDownClient*>> m_waitingClientsByHybridId;
	std::unordered_map<WaitingUserHashKey, std::vector<CUpDownClient*>, WaitingUserHashKeyHasher> m_waitingClientsByHash;
	std::shared_ptr<const WaitingQueueSnapshot> m_waitingSnapshot;
	std::vector<CUpDownClient*> m_activeUploadClients;
	std::shared_ptr<const ActiveUploadSnapshot> m_activeUploadSnapshot;

	friend class CUpDownClient;
	friend class CUploadDiskIOThread;
};
