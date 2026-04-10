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
		, queuedPayloadBytes()
		, ioError()
	{
	}
	~UploadSession();

	CUpDownClient										*client;
	CTypedPtrList<CPtrList, Requested_Block_Struct*>	blockRequests;
	CTypedPtrList<CPtrList, Requested_Block_Struct*>	completedBlocks;
	CCriticalSection									blockListsLock; // don't acquire other locks while having this one in any thread other than UploadDiskIOThread or make sure deadlocks are impossible
	uint64												queuedPayloadBytes;
	bool												ioError;
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
	struct ActiveUploadRange
	{
		uint64 startOffset = 0;
		uint64 endOffset = 0;
	};

	struct ActiveUploadVisualState
	{
		UINT slotNumber = 0;
		uint64 payloadInBuffer = 0;
		bool isActivating = false;
		bool isActive = false;
		std::vector<ActiveUploadRange> pendingRanges;
		std::vector<ActiveUploadRange> completedRanges;
	};

	struct WaitingUdpEndpointKey
	{
		uint32 ip = 0;
		uint16 udpPort = 0;

		bool operator==(const WaitingUdpEndpointKey &other) const noexcept
		{
			return ip == other.ip && udpPort == other.udpPort;
		}
	};

	struct WaitingUdpEndpointKeyHasher
	{
		size_t operator()(const WaitingUdpEndpointKey &key) const noexcept
		{
			return (static_cast<size_t>(key.ip) << 16) ^ static_cast<size_t>(key.udpPort);
		}
	};

	struct WaitingQueueSnapshot
	{
		INT_PTR GetCount() const								{ return static_cast<INT_PTR>(memberClients.size()); }
		const std::vector<CUpDownClient*>& GetRankedClients() const	{ return rankedClients; }
		UINT GetPosition(const CUpDownClient *client) const;
		CUpDownClient* FindByIP(uint32 ip) const;
		CUpDownClient* FindByUdpEndpoint(uint32 ip, uint16 udpPort, bool ignorePortOnUniqueIP, bool *multipleIPs = NULL) const;

	private:
		friend class CUploadQueue;
		std::vector<CUpDownClient*> memberClients;
		std::vector<CUpDownClient*> rankedClients;
		std::unordered_map<const CUpDownClient*, UINT> positionByClient;
		std::unordered_map<uint32, std::vector<CUpDownClient*>> clientsByIP;
		std::unordered_map<WaitingUdpEndpointKey, CUpDownClient*, WaitingUdpEndpointKeyHasher> clientsByUdpEndpoint;
	};

	struct ActiveUploadSnapshot
	{
		const std::vector<CUpDownClient*>& GetActiveClients() const	{ return activeClients; }
		const ActiveUploadVisualState* FindVisualState(const CUpDownClient *client) const;

	private:
		friend class CUploadQueue;
		std::vector<CUpDownClient*> activeClients;
		std::unordered_map<const CUpDownClient*, ActiveUploadVisualState> visualStateByClient;
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

	std::shared_ptr<const WaitingQueueSnapshot> GetWaitingSnapshot() const;
	std::shared_ptr<const ActiveUploadSnapshot> GetActiveUploadSnapshot() const;
	bool	EnqueueUploadRequestBlock(CUpDownClient *client, Requested_Block_Struct *reqblock, INT_PTR *pQueueCount = NULL);
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

	/** Decision for a new queue admission after conflict checks have completed. */
	enum class EUploadAdmissionAction : uint8
	{
		Reject,
		QueueWaiting,
		ActivateImmediately
	};

	struct UploadAdmissionDecision
	{
		EUploadAdmissionAction action = EUploadAdmissionAction::Reject;
	};

	struct UploadSlotOpenDecision
	{
		bool	shouldOpen = false;
		LPCTSTR reason = NULL;
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

	/** Returns true if the client can immediately take an upload slot. */
	bool	IsClientEligibleForImmediateUpload(const CUpDownClient *client) const;
	const ClientQueueEntry* FindClientQueueEntry(const CUpDownClient *client) const;
	ClientQueueEntry* FindClientQueueEntry(const CUpDownClient *client);
	ClientQueueEntry& EnsureClientQueueEntry(CUpDownClient *client);
	EUploadSlotPhase GetUploadSlotPhase(const CUpDownClient *client) const;
	void	SyncLegacyUploadState(CUpDownClient *client, EUploadSlotPhase phase);
	bool	QueueWaitingClientState(CUpDownClient *client);
	bool	BeginClientActivationState(CUpDownClient *client, bool removeFromWaitingQueue);
	bool	MarkClientUploadActiveState(CUpDownClient *client);
	bool	BeginClientRetiringState(CUpDownClient *client);
	void	ClearClientQueueState(CUpDownClient *client);
	const CKnownFile* GetRequestedUploadFile(const CUpDownClient *client) const;
	bool	ShouldPurgeWaitingClient(const CUpDownClient *client, ULONGLONG curTick) const;
	bool	ShouldRejectLowIdQueueRequest(const CUpDownClient *client) const;
	bool	IsHigherPriorityWaitingClient(const CUpDownClient *candidate, const CUpDownClient *currentBest) const;
	void	PurgeStaleWaitingClients(ULONGLONG curTick);
	static WaitingUserHashKey MakeWaitingUserHashKey(const CUpDownClient *client);
	void	RebuildWaitingIndexes();
	void	CollectDuplicateWaitingCandidates(const CUpDownClient *client, std::vector<CUpDownClient*> &candidates) const;
	void	PublishWaitingSnapshot();
	void	PublishActiveUploadSnapshot();
	CUpDownClient* SelectNextWaitingClient();
	CUpDownClient* FindLowestPriorityWaitingClient(const CUpDownClient *excludeClient = NULL) const;
	bool	PassesQueueAdmissionLimit(const CUpDownClient *client) const;
	void	TrackExternalQueueRequest(CUpDownClient *client, bool bIgnoreTimelimit) const;
	void	RecordExternalQueueRequestStat(CUpDownClient *client) const;
	uint16	GetQueuedSameIPCount(const CUpDownClient *client) const;
	bool	HasTooManyQueuedClientsFromSameIP(const CUpDownClient *client) const;
	bool	RefreshQueuedClient(CUpDownClient *client);
	void	DropQueuedDuplicateClient(CUpDownClient *client, LPCTSTR disconnectReason);
	bool	ResolveExternalQueueConflicts(CUpDownClient *client);
	bool	TryAcceptActiveUploadRequest(CUpDownClient *client) const;
	UploadAdmissionDecision EvaluateQueueAdmission(const CUpDownClient *client, const UploadSchedulingSnapshot &snapshot) const;
	bool	ExecuteQueueAdmission(CUpDownClient *client, const UploadAdmissionDecision &decision, LPCTSTR pszImmediateActivationReason);
	bool	AdmitClientToQueue(CUpDownClient *client, LPCTSTR pszImmediateActivationReason);
	bool	RequeueClientAfterUploadSession(CUpDownClient *client);
	INT_PTR	GetWaitingMemberCount() const;
	bool	HasWaitingMember(const CUpDownClient *client) const;
	bool	FindWaitingClientIndex(const CUpDownClient *client, size_t &index) const;
	UploadSessionPtr GetUploadSession(const CUpDownClient *client) const;
	std::vector<UploadSessionPtr> GetActiveUploadSessions() const;
	bool	IsCurrentUploadSession(const UploadSessionPtr &session) const;
	bool	HasUploadSessionIoError(const UploadSessionPtr &session) const;
	bool	TryPeekNextUploadBlockRead(const UploadSessionPtr &session, uint64 currentPayload, uint32 bufferLimit, Requested_Block_Struct &block, uint64 &addedPayloadQueueSession) const;
	bool	PromotePendingUploadBlockRead(const UploadSessionPtr &session, const Requested_Block_Struct &expectedBlock, uint64 addedPayloadQueueSession);
	void	MarkUploadSessionIoError(const UploadSessionPtr &session);
	bool	AttachActiveUploadSession(CUpDownClient *client);
	void	ResetUploadSessionState(const UploadSessionPtr &session);
	bool	DetachActiveUploadSession(CUpDownClient *client, UploadSessionPtr &session);
	void	RefreshWaitingQueueUiCount() const;
	void	NotifyWaitingClientAdded(CUpDownClient *client) const;
	void	NotifyWaitingClientRefreshed(CUpDownClient *client) const;
	void	NotifyWaitingClientRemoved(CUpDownClient *client) const;
	void	NotifyActiveUploadAdded(CUpDownClient *client) const;
	void	NotifyActiveUploadRemoved(CUpDownClient *client) const;
	void	SendAcceptUploadRequestPacket(CUpDownClient *client) const;
	void	SendOutOfPartReqsPacket(CUpDownClient *client) const;
	void	BeginUploadSessionBookkeeping(CUpDownClient *client) const;
	void	FinalizeUploadSessionBookkeeping(CUpDownClient *client, bool earlyabort);
	bool	ValidateActiveUploadHeadBlock(CUpDownClient *client, const UploadSessionPtr &session);
	void	MaybeUseBigSendBuffer(CUpDownClient *client) const;
	void	ProcessActiveUploadSession(const UploadSessionPtr &session, const UploadSchedulingSnapshot &snapshot);
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
	/** Returns a slot-opening decision for one scheduling tick. */
	UploadSlotOpenDecision EvaluateUploadSlotOpening(const UploadSchedulingSnapshot &snapshot, bool allowEmptyWaitingQueue) const;
	/** Returns true when the scheduler should start another upload slot now. */
	bool	ShouldOpenUploadSlot(const UploadSchedulingSnapshot &snapshot, bool allowEmptyWaitingQueue) const;
	/** Evaluates whether the active upload slot should be ended. */
	UploadSlotEndDecision EvaluateUploadSlotEnd(CUpDownClient *client, const UploadSchedulingSnapshot &snapshot);
	/** Computes active-slot retention policy without mutating queue state. */
	UploadSlotEndDecision EvaluateUploadSlotEndPolicy(const CUpDownClient *client, const UploadSchedulingSnapshot &snapshot) const;
	void	UpdateUploadSlotEndTracking(CUpDownClient *client, const UploadSchedulingSnapshot &snapshot);
	void	ApplyUploadSlotEndDecisionEffects(CUpDownClient *client, const UploadSlotEndDecision &decision);
	void	ApplySlowUploadRecycleEffects(CUpDownClient *client);
	void	LogUploadSlotTransferLimitEnd(CUpDownClient *client) const;
	void	LogUploadSlotTimeLimitEnd(CUpDownClient *client) const;
	/** Converts an upload-slot end reason into the removal log text. */
	static LPCTSTR GetUploadSlotEndReasonText(EUploadSlotEndReason eReason);
	uint32	GetEffectiveUploadBudgetBytesPerSec() const;
	INT_PTR	GetSoftMaxUploadSlots() const;
	uint32	GetTargetClientDataRateBroadband() const;
	uint32	GetSlowUploadRateThreshold() const;
	bool	ShouldTrackSlowUploadSlots(const UploadSchedulingSnapshot &snapshot) const;
	void	UpdateActiveClientsInfo(ULONGLONG curTick);
	bool	RemoveWaitingClientAt(size_t index, bool updatewindow, bool publishSnapshots = true);
	void	MarkWaitingSnapshotDirty()						{ m_waitingSnapshotDirty = true; }
	void	MarkActiveSnapshotDirty()						{ m_activeUploadSnapshotDirty = true; }
	void	FlushDirtySnapshots();
	CCriticalSection	m_csWaitingSnapshotRead;	// Reader-side waiting snapshot publication lock.
	CCriticalSection	m_csActiveSnapshotRead;	// Reader-side active snapshot publication lock.
	CCriticalSection	m_csQueueState;			// Mutable queue-state lock. Protects queue-owned membership, entries, and active-session ownership.

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
	bool	m_waitingSnapshotDirty;
	std::vector<CUpDownClient*> m_activeUploadClients;
	std::shared_ptr<const ActiveUploadSnapshot> m_activeUploadSnapshot;
	bool	m_activeUploadSnapshotDirty;

	friend class CUpDownClient;
	friend class CUploadDiskIOThread;
};
