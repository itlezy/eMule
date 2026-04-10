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
#include "UploadQueue.h"
#include "Packets.h"
#include "PartFile.h"
#include "ListenSocket.h"
#include "Exceptions.h"
#include "Scheduler.h"
#include "PerfLog.h"
#include "UploadBandwidthThrottler.h"
#include "ClientList.h"
#include "DownloadQueue.h"
#include "FriendList.h"
#include "Statistics.h"
#include "UpDownClient.h"
#include "SharedFileList.h"
#include "KnownFileList.h"
#include "ServerConnect.h"
#include "ClientCredits.h"
#include "ServerList.h"
#include "WebServer.h"
#include "emuledlg.h"
#include "ServerWnd.h"
#include "TransferDlg.h"
#include "StatisticsDlg.h"
#include "Kademlia/Kademlia/Kademlia.h"
#include "Kademlia/Kademlia/Prefs.h"
#include "Log.h"
#include "collection.h"
#include <algorithm>
#include <unordered_set>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


static uint32 i1sec, i2sec, i5sec, i60sec;
static UINT s_uSaveStatistics = 0;
static uint32 igraph, istats;

#define HIGHSPEED_UPLOADRATE_START	(500*1024)
#define HIGHSPEED_UPLOADRATE_END	(300*1024)

namespace
{
	uint64 ResolveBBSessionTransferLimitBytes(const CKnownFile *pUploadingFile)
	{
		switch (thePrefs.GetBBSessionTransferMode()) {
		case BBSTM_PERCENT_OF_FILE:
			if (pUploadingFile == NULL)
				return 0;
			return ((uint64)pUploadingFile->GetFileSize() * thePrefs.GetBBSessionTransferValue() + 99u) / 100u;
		case BBSTM_ABSOLUTE_MIB:
			return (uint64)thePrefs.GetBBSessionTransferValue() * 1024ui64 * 1024ui64;
		default:
			return 0;
		}
	}
}


CUploadQueue::CUploadQueue()
	: waitinglist(this)
	, average_ur_hist(512, 512)
	, activeClients_hist(512, 512)
	, datarate()
	, friendDatarate()
	, successfullupcount()
	, failedupcount()
	, totaluploadtime()
	, m_nLastStartUpload()
	, m_iHighestNumberOfFullyActivatedSlotsSinceLastCall()
	, m_MaxActiveClients()
	, m_MaxActiveClientsShortTime()
	, m_average_ur_sum()
	, m_lastCalculatedDataRateTick()
	, m_bStatisticsWaitingListDirty(true)
	, m_bThrottlerWantsMoreSlotsHint()
	, m_clientEntries()
	, m_waitingClients()
	, m_waitingSnapshot(std::make_shared<WaitingQueueSnapshot>())
	, m_activeUploadClients()
	, m_activeUploadSnapshot(std::make_shared<ActiveUploadSnapshot>())
{
	i1sec = i2sec = i5sec = i60sec = 0;
	VERIFY((h_timer = ::SetTimer(NULL, 0, SEC2MS(1)/10, UploadTimer)) != 0);
	if (thePrefs.GetVerbose() && !h_timer)
		AddDebugLogLine(true, _T("Failed to create 'upload queue' timer - %s"), (LPCTSTR)GetErrorMessage(::GetLastError()));
}

CUploadQueue::~CUploadQueue()
{
	if (h_timer)
		::KillTimer(0, h_timer);
}

/**
 * Find the highest ranking client in the waiting queue, and return it.
 *
 * Low id client are ranked as lowest possible, unless they are currently connected.
 * Returns whether a waiting client can take an upload slot immediately.
 */
bool CUploadQueue::IsClientEligibleForImmediateUpload(const CUpDownClient *client) const
{
	return !client->IsInSlowUploadCooldown()
		&& (!client->HasLowID() || (client->socket && client->socket->IsConnected()));
}

const CKnownFile* CUploadQueue::GetRequestedUploadFile(const CUpDownClient *client) const
{
	return client ? theApp.sharedfiles->GetFileByID(client->GetUploadFileID()) : NULL;
}

bool CUploadQueue::ShouldPurgeWaitingClient(const CUpDownClient *client, ULONGLONG curTick) const
{
	return client == NULL
		|| curTick >= client->GetLastUpRequest() + MAX_PURGEQUEUETIME
		|| GetRequestedUploadFile(client) == NULL;
}

bool CUploadQueue::ShouldRejectLowIdQueueRequest(const CUpDownClient *client) const
{
	return theApp.IsConnected()
		&& theApp.IsFirewalled()
		&& !client->GetKadPort()
		&& client->GetDownloadState() == DS_NONE
		&& !client->IsFriend()
		&& theApp.serverconnect
		&& !theApp.serverconnect->IsLocalServer(client->GetServerIP(), client->GetServerPort())
		&& GetWaitingUserCount() > 50;
}

CUploadQueue::WaitingUserHashKey CUploadQueue::MakeWaitingUserHashKey(const CUpDownClient *client)
{
	WaitingUserHashKey key;
	if (client != NULL && client->HasValidHash())
		md4cpy(key.bytes.data(), client->GetUserHash());
	return key;
}

float CUploadQueue::GetWaitingClientCreditFactor(const CUpDownClient *client) const
{
	if (!thePrefs.UseCreditSystem() || client == NULL || client->Credits() == NULL)
		return 1.0f;
	return client->Credits()->GetScoreRatio(client->GetIP());
}

int CUploadQueue::CompareWaitingClientsByRank(const CUpDownClient *left, const CUpDownClient *right) const
{
	if (left == right)
		return 0;
	if (left == NULL)
		return 1;
	if (right == NULL)
		return -1;

	const CKnownFile *pLeftFile = GetRequestedUploadFile(left);
	const CKnownFile *pRightFile = GetRequestedUploadFile(right);
	if (pLeftFile == NULL)
		return 1;
	if (pRightFile == NULL)
		return -1;

	const float fLeftRatio = pLeftFile->GetAllTimeUploadRatio();
	const float fRightRatio = pRightFile->GetAllTimeUploadRatio();
	if (fLeftRatio < fRightRatio)
		return -1;
	if (fLeftRatio > fRightRatio)
		return 1;

	if (left->GetWaitStartTime() < right->GetWaitStartTime())
		return -1;
	if (left->GetWaitStartTime() > right->GetWaitStartTime())
		return 1;

	if (left->GetFriendSlot() != right->GetFriendSlot())
		return left->GetFriendSlot() ? -1 : 1;

	const float fLeftCredit = GetWaitingClientCreditFactor(left);
	const float fRightCredit = GetWaitingClientCreditFactor(right);
	if (fLeftCredit > fRightCredit)
		return -1;
	if (fLeftCredit < fRightCredit)
		return 1;

	if (left->GetFilePrioAsNumber() > right->GetFilePrioAsNumber())
		return -1;
	if (left->GetFilePrioAsNumber() < right->GetFilePrioAsNumber())
		return 1;

	return (left < right) ? -1 : 1;
}

bool CUploadQueue::IsHigherPriorityWaitingClient(const CUpDownClient *candidate, const CUpDownClient *currentBest) const
{
	return CompareWaitingClientsByRank(candidate, currentBest) < 0;
}

void CUploadQueue::PublishWaitingSnapshot()
{
	std::shared_ptr<WaitingQueueSnapshot> snapshot = std::make_shared<WaitingQueueSnapshot>();
	snapshot->memberClients = m_waitingClients;
	snapshot->rankedClients = m_waitingClients;
	std::sort(snapshot->rankedClients.begin(), snapshot->rankedClients.end(),
		[this](const CUpDownClient *left, const CUpDownClient *right) {
			return CompareWaitingClientsByRank(left, right) < 0;
		});

	snapshot->positionByClient.reserve(snapshot->rankedClients.size());
	for (size_t i = 0; i < snapshot->rankedClients.size(); ++i)
		snapshot->positionByClient[snapshot->rankedClients[i]] = static_cast<UINT>(i + 1);

	for (CUpDownClient *client : snapshot->memberClients) {
		snapshot->compatibilityMembers.AddTail(client);
		if (client == NULL)
			continue;
		if (client->GetIP() != 0)
			snapshot->clientsByIP[client->GetIP()].push_back(client);
		if (client->GetIP() != 0 && client->GetUDPPort() != 0)
			snapshot->clientsByUdpEndpoint[WaitingUdpEndpointKey{client->GetIP(), client->GetUDPPort()}] = client;
	}

	CSingleLock lock(&m_csWaitingSnapshotRead, TRUE);
	m_waitingSnapshot = snapshot;
}

void CUploadQueue::RebuildWaitingIndexes()
{
	m_waitingClientIndexes.clear();
	m_waitingClientsByIP.clear();
	m_waitingClientsByUserPort.clear();
	m_waitingClientsByKadPort.clear();
	m_waitingClientsByHybridId.clear();
	m_waitingClientsByHash.clear();

	m_waitingClientIndexes.reserve(m_waitingClients.size());
	for (size_t index = 0; index < m_waitingClients.size(); ++index) {
		CUpDownClient *client = m_waitingClients[index];
		if (client == NULL)
			continue;

		m_waitingClientIndexes[client] = index;

		if (client->GetIP() != 0)
			m_waitingClientsByIP[client->GetIP()].push_back(client);
		if (client->GetUserPort() != 0)
			m_waitingClientsByUserPort[client->GetUserPort()].push_back(client);
		if (client->GetKadPort() != 0)
			m_waitingClientsByKadPort[client->GetKadPort()].push_back(client);
		if (client->GetUserIDHybrid() != 0)
			m_waitingClientsByHybridId[client->GetUserIDHybrid()].push_back(client);
		if (client->HasValidHash())
			m_waitingClientsByHash[MakeWaitingUserHashKey(client)].push_back(client);
	}
}

void CUploadQueue::CollectDuplicateWaitingCandidates(const CUpDownClient *client, std::vector<CUpDownClient*> &candidates) const
{
	candidates.clear();
	if (client == NULL)
		return;

	std::unordered_set<CUpDownClient*> seen;
	seen.reserve(m_waitingClients.size());

	const auto appendBucket = [&candidates, &seen](const std::vector<CUpDownClient*> &bucket) {
		for (CUpDownClient *candidate : bucket) {
			if (candidate != NULL && seen.insert(candidate).second)
				candidates.push_back(candidate);
		}
	};

	if (client->HasValidHash()) {
		const auto hashIt = m_waitingClientsByHash.find(MakeWaitingUserHashKey(client));
		if (hashIt != m_waitingClientsByHash.cend())
			appendBucket(hashIt->second);
	}
	if (client->GetIP() != 0) {
		const auto ipIt = m_waitingClientsByIP.find(client->GetIP());
		if (ipIt != m_waitingClientsByIP.cend())
			appendBucket(ipIt->second);
	}
	if (client->GetUserPort() != 0) {
		const auto portIt = m_waitingClientsByUserPort.find(client->GetUserPort());
		if (portIt != m_waitingClientsByUserPort.cend())
			appendBucket(portIt->second);
	}
	if (client->GetKadPort() != 0) {
		const auto kadIt = m_waitingClientsByKadPort.find(client->GetKadPort());
		if (kadIt != m_waitingClientsByKadPort.cend())
			appendBucket(kadIt->second);
	}
	if (client->GetUserIDHybrid() != 0) {
		const auto hybridIt = m_waitingClientsByHybridId.find(client->GetUserIDHybrid());
		if (hybridIt != m_waitingClientsByHybridId.cend())
			appendBucket(hybridIt->second);
	}
}

std::shared_ptr<const CUploadQueue::WaitingQueueSnapshot> CUploadQueue::GetWaitingSnapshot() const
{
	CSingleLock lock(const_cast<CCriticalSection*>(&m_csWaitingSnapshotRead), TRUE);
	return m_waitingSnapshot;
}

void CUploadQueue::PublishActiveUploadSnapshot()
{
	std::shared_ptr<ActiveUploadSnapshot> snapshot = std::make_shared<ActiveUploadSnapshot>();
	CSingleLock lock(&m_csActiveUploadState, TRUE);
	snapshot->activeClients = m_activeUploadClients;
	snapshot->slotNumberByClient.reserve(snapshot->activeClients.size());
	for (size_t i = 0; i < snapshot->activeClients.size(); ++i)
		snapshot->slotNumberByClient[snapshot->activeClients[i]] = static_cast<UINT>(i + 1);
	m_activeUploadSnapshot = snapshot;
}

std::shared_ptr<const CUploadQueue::ActiveUploadSnapshot> CUploadQueue::GetActiveUploadSnapshot() const
{
	CSingleLock lock(const_cast<CCriticalSection*>(&m_csActiveUploadState), TRUE);
	return m_activeUploadSnapshot;
}

POSITION CUploadQueue::CWaitingListCompatView::GetHeadPosition() const
{
	const std::shared_ptr<const WaitingQueueSnapshot> snapshot = (m_owner != NULL) ? m_owner->GetWaitingSnapshot() : std::shared_ptr<const WaitingQueueSnapshot>();
	CSingleLock lock(&m_csThreadSnapshots, TRUE);
	if (snapshot != NULL)
		m_threadSnapshots[::GetCurrentThreadId()] = std::static_pointer_cast<const void>(snapshot);
	else
		m_threadSnapshots.erase(::GetCurrentThreadId());
	return (snapshot != NULL) ? snapshot->compatibilityMembers.GetHeadPosition() : NULL;
}

CUpDownClient* CUploadQueue::CWaitingListCompatView::GetNext(POSITION &pos) const
{
	std::shared_ptr<const WaitingQueueSnapshot> snapshot;
	{
		CSingleLock lock(&m_csThreadSnapshots, TRUE);
		const auto it = m_threadSnapshots.find(::GetCurrentThreadId());
		if (it != m_threadSnapshots.cend())
			snapshot = std::static_pointer_cast<const WaitingQueueSnapshot>(it->second);
	}
	if (snapshot == NULL)
		snapshot = (m_owner != NULL) ? m_owner->GetWaitingSnapshot() : std::shared_ptr<const WaitingQueueSnapshot>();
	if (snapshot == NULL)
		return NULL;

	CUpDownClient *client = snapshot->compatibilityMembers.GetNext(pos);
	if (pos == NULL) {
		CSingleLock lock(&m_csThreadSnapshots, TRUE);
		m_threadSnapshots.erase(::GetCurrentThreadId());
	}
	return client;
}

INT_PTR CUploadQueue::CWaitingListCompatView::GetCount() const
{
	const std::shared_ptr<const WaitingQueueSnapshot> snapshot = (m_owner != NULL) ? m_owner->GetWaitingSnapshot() : std::shared_ptr<const WaitingQueueSnapshot>();
	return (snapshot != NULL) ? static_cast<INT_PTR>(snapshot->memberClients.size()) : 0;
}

void CUploadQueue::PurgeStaleWaitingClients(ULONGLONG curTick)
{
	bool removedAny = false;
	for (size_t index = 0; index < m_waitingClients.size();) {
		CUpDownClient *client = m_waitingClients[index];
		ASSERT(client->GetLastUpRequest());

		if (!ShouldPurgeWaitingClient(client, curTick)) {
			++index;
			continue;
		}

		client->ClearWaitStartTime();
		removedAny |= RemoveWaitingClientAt(index, true, false);
	}

	if (removedAny)
		PublishWaitingSnapshot();
}

void CUploadQueue::GetWaitingClientsInRankOrder(std::vector<CUpDownClient*> &clients)
{
	const std::shared_ptr<const WaitingQueueSnapshot> snapshot = GetWaitingSnapshot();
	clients = (snapshot != NULL) ? snapshot->rankedClients : std::vector<CUpDownClient*>();
}

CUpDownClient* CUploadQueue::SelectNextWaitingClient()
{
	const std::shared_ptr<const WaitingQueueSnapshot> snapshot = GetWaitingSnapshot();
	if (snapshot == NULL)
		return NULL;
	for (CUpDownClient *client : snapshot->rankedClients) {
		if (IsClientEligibleForImmediateUpload(client))
			return client;
	}
	return NULL;
}

CUpDownClient* CUploadQueue::FindLowestPriorityWaitingClient(const CUpDownClient *excludeClient)
{
	const std::shared_ptr<const WaitingQueueSnapshot> snapshot = GetWaitingSnapshot();
	if (snapshot == NULL)
		return NULL;
	for (auto it = snapshot->rankedClients.rbegin(); it != snapshot->rankedClients.rend(); ++it) {
		if (*it != excludeClient)
			return *it;
	}
	return NULL;
}

bool CUploadQueue::PassesQueueAdmissionLimit(const CUpDownClient *client)
{
	const INT_PTR softQueueLimit = thePrefs.GetQueueSize();
	const INT_PTR hardQueueLimit = softQueueLimit + max(softQueueLimit, 800) / 4;
	if (GetWaitingMemberCount() >= hardQueueLimit)
		return false;
	if (GetWaitingMemberCount() < softQueueLimit)
		return true;

	const CUpDownClient *worstClient = FindLowestPriorityWaitingClient(client);
	return worstClient == NULL || IsHigherPriorityWaitingClient(client, worstClient);
}

void CUploadQueue::NoteUploadRequestSeen(CUpDownClient *client) const
{
	if (client != NULL && !client->GetWaitStartTime())
		client->SetWaitStartTime();
}

void CUploadQueue::TrackExternalQueueRequest(CUpDownClient *client, bool bIgnoreTimelimit) const
{
	NoteUploadRequestSeen(client);
	client->IncrementAskedCount();
	client->SetLastUpRequest();
	if (!bIgnoreTimelimit)
		client->AddRequestCount(client->GetUploadFileID());
}

void CUploadQueue::RecordExternalQueueRequestStat(CUpDownClient *client) const
{
	// TODO: Maybe we should change this to count each request for a file only once and ignore re-asks
	CKnownFile *reqfile = theApp.sharedfiles->GetFileByID((uchar*)client->GetUploadFileID());
	if (reqfile != NULL)
		reqfile->statistic.AddRequest();
}

bool CUploadQueue::HasTooManyQueuedClientsFromSameIP(const CUpDownClient *client, uint16 cSameIP) const
{
	if (cSameIP >= 3) {
		if (thePrefs.GetVerbose())
			DEBUG_ONLY(AddDebugLogLine(false, _T("%s's (%s) request to enter the queue was rejected, because of too many clients with the same IP"), client->GetUserName(), (LPCTSTR)ipstr(client->GetConnectIP())));
		return true;
	}

	if (theApp.clientlist->GetClientsFromIP(client->GetIP()) >= 3) {
		if (thePrefs.GetVerbose())
			DEBUG_ONLY(AddDebugLogLine(false, _T("%s's (%s) request to enter the queue was rejected, because of too many clients with the same IP (found in TrackedClientsList)"), client->GetUserName(), (LPCTSTR)ipstr(client->GetConnectIP())));
		return true;
	}

	return false;
}

bool CUploadQueue::RefreshQueuedClient(CUpDownClient *client)
{
	RebuildWaitingIndexes();
	PublishWaitingSnapshot();
	client->SendRankingInfo();
	theApp.emuledlg->transferwnd->GetQueueList()->RefreshClient(client);
	return true;
}

bool CUploadQueue::ResolveExternalQueueConflicts(CUpDownClient *client, uint16 &cSameIP)
{
	if (HasWaitingMember(client))
		return RefreshQueuedClient(client);

	std::vector<CUpDownClient*> candidates;
	CollectDuplicateWaitingCandidates(client, candidates);
	for (CUpDownClient *cur_client : candidates) {
		if (!HasWaitingMember(cur_client))
			continue;
		if (cur_client == client)
			return RefreshQueuedClient(client);

		if (client->Compare(cur_client)) {
			theApp.clientlist->AddTrackClient(client);
			if (cur_client->credits != NULL && cur_client->credits->GetCurrentIdentState(cur_client->GetIP()) == IS_IDENTIFIED) {
				if (thePrefs.GetVerbose())
					AddDebugLogLine(false, (LPCTSTR)GetResString(IDS_SAMEUSERHASH), client->GetUserName(), cur_client->GetUserName(), client->GetUserName());
				return true;
			}
			if (client->credits == NULL || client->credits->GetCurrentIdentState(client->GetIP()) != IS_IDENTIFIED) {
				if (thePrefs.GetVerbose())
					AddDebugLogLine(false, (LPCTSTR)GetResString(IDS_SAMEUSERHASH), client->GetUserName(), cur_client->GetUserName(), _T("Both"));
				RemoveWaitingClient(cur_client, true);
				if (!cur_client->socket && cur_client->Disconnected(_T("AddClientToQueue - same userhash 2")))
					delete cur_client;
				return true;
			}
			if (thePrefs.GetVerbose())
				AddDebugLogLine(false, (LPCTSTR)GetResString(IDS_SAMEUSERHASH), client->GetUserName(), cur_client->GetUserName(), cur_client->GetUserName());
			RemoveWaitingClient(cur_client, true);
			if (!cur_client->socket && cur_client->Disconnected(_T("AddClientToQueue - same userhash 1")))
				delete cur_client;
		}
	}

	if (client->GetIP() != 0) {
		const auto ipIt = m_waitingClientsByIP.find(client->GetIP());
		if (ipIt != m_waitingClientsByIP.cend())
			cSameIP = static_cast<uint16>(ipIt->second.size());
	}
	return false;
}

bool CUploadQueue::TryAcceptActiveUploadRequest(CUpDownClient *client) const
{
	if (!IsDownloading(client))
		return false;

	if (thePrefs.GetDebugClientTCPLevel() > 0)
		DebugSend("OP_AcceptUploadReq", client);
	Packet *packet = new Packet(OP_ACCEPTUPLOADREQ, 0);
	theStats.AddUpDataOverheadFileRequest(packet->size);
	client->SendPacket(packet);
	return true;
}

bool CUploadQueue::AdmitClientToQueue(CUpDownClient *client, LPCTSTR pszImmediateActivationReason)
{
	PurgeStaleWaitingClients(::GetTickCount64());

	if (TryAcceptActiveUploadRequest(client))
		return true;

	ClearClientQueueEntry(client);

	if (!PassesQueueAdmissionLimit(client))
		return false;
	if (client->IsInSlowUploadCooldown()) {
		AddClientToWaitingList(client);
		return true;
	}

	const UploadSchedulingSnapshot snapshot = CaptureSchedulingSnapshot(m_bThrottlerWantsMoreSlotsHint);
	if (GetWaitingMemberCount() == 0 && ShouldOpenUploadSlot(snapshot, true))
		return ActivateUploadClient(client, pszImmediateActivationReason, false);

	AddClientToWaitingList(client);
	return true;
}

bool CUploadQueue::RequeueClientAfterUploadSession(CUpDownClient *client)
{
	if (client == NULL)
		return false;

	if (thePrefs.GetDebugClientTCPLevel() > 0)
		DebugSend("OP_OutOfPartReqs", client);
	Packet *pPacket = new Packet(OP_OUTOFPARTREQS, 0);
	theStats.AddUpDataOverheadFileRequest(pPacket->size);
	client->SendPacket(pPacket);
	client->m_fSentOutOfPartReqs = 1;
	client->SetWaitStartTime();
	return AdmitClientToQueue(client, _T("Requeued after upload slot rotation."));
}

const CUploadQueue::ClientQueueEntry* CUploadQueue::FindClientQueueEntry(const CUpDownClient *client) const
{
	const auto it = m_clientEntries.find(client);
	return (it != m_clientEntries.cend()) ? &it->second : NULL;
}

CUploadQueue::ClientQueueEntry* CUploadQueue::FindClientQueueEntry(const CUpDownClient *client)
{
	const auto it = m_clientEntries.find(client);
	return (it != m_clientEntries.cend()) ? &it->second : NULL;
}

CUploadQueue::ClientQueueEntry& CUploadQueue::EnsureClientQueueEntry(CUpDownClient *client)
{
	return m_clientEntries[client];
}

CUploadQueue::EUploadSlotPhase CUploadQueue::GetUploadSlotPhase(const CUpDownClient *client) const
{
	CSingleLock lock(const_cast<CCriticalSection*>(&m_csActiveUploadState), TRUE);
	const ClientQueueEntry *entry = FindClientQueueEntry(client);
	return (entry != NULL) ? entry->phase : EUploadSlotPhase::None;
}

bool CUploadQueue::IsClientManagedByUploadQueue(const CUpDownClient *client) const
{
	CSingleLock lock(const_cast<CCriticalSection*>(&m_csActiveUploadState), TRUE);
	return FindClientQueueEntry(client) != NULL;
}

bool CUploadQueue::IsClientWaitingForUpload(const CUpDownClient *client) const
{
	return GetUploadSlotPhase(client) == EUploadSlotPhase::Waiting;
}

bool CUploadQueue::IsClientUploadActivating(const CUpDownClient *client) const
{
	return GetUploadSlotPhase(client) == EUploadSlotPhase::Activating;
}

bool CUploadQueue::IsClientUploadActive(const CUpDownClient *client) const
{
	return GetUploadSlotPhase(client) == EUploadSlotPhase::Active;
}

bool CUploadQueue::HasUploadSlot(const CUpDownClient *client) const
{
	return inSet(GetUploadSlotPhase(client), EUploadSlotPhase::Activating, EUploadSlotPhase::Active, EUploadSlotPhase::Retiring);
}

void CUploadQueue::SyncLegacyUploadState(CUpDownClient *client, EUploadSlotPhase phase)
{
	if (client == NULL)
		return;

	switch (phase) {
	case EUploadSlotPhase::Waiting:
		client->SetUploadState(US_ONUPLOADQUEUE);
		break;
	case EUploadSlotPhase::Activating:
		client->SetUploadState(US_CONNECTING);
		break;
	case EUploadSlotPhase::Active:
	case EUploadSlotPhase::Retiring:
		client->SetUploadState(US_UPLOADING);
		break;
	case EUploadSlotPhase::None:
	default:
		client->SetUploadState(US_NONE);
		break;
	}
}

void CUploadQueue::EnterWaitingState(CUpDownClient *client)
{
	if (client == NULL)
		return;

	CSingleLock lock(&m_csActiveUploadState, TRUE);
	ClientQueueEntry &entry = EnsureClientQueueEntry(client);
	entry.phase = EUploadSlotPhase::Waiting;
	entry.session.reset();
	SyncLegacyUploadState(client, entry.phase);
}

void CUploadQueue::BeginActivationState(CUpDownClient *client)
{
	if (client == NULL)
		return;

	CSingleLock lock(&m_csActiveUploadState, TRUE);
	ClientQueueEntry &entry = EnsureClientQueueEntry(client);
	entry.phase = EUploadSlotPhase::Activating;
	SyncLegacyUploadState(client, entry.phase);
}

void CUploadQueue::MarkActiveState(CUpDownClient *client)
{
	if (client == NULL)
		return;

	CSingleLock lock(&m_csActiveUploadState, TRUE);
	ClientQueueEntry &entry = EnsureClientQueueEntry(client);
	entry.phase = EUploadSlotPhase::Active;
	SyncLegacyUploadState(client, entry.phase);
}

void CUploadQueue::BeginRetiringState(CUpDownClient *client)
{
	if (client == NULL)
		return;

	CSingleLock lock(&m_csActiveUploadState, TRUE);
	ClientQueueEntry &entry = EnsureClientQueueEntry(client);
	entry.phase = EUploadSlotPhase::Retiring;
	SyncLegacyUploadState(client, entry.phase);
}

void CUploadQueue::ClearClientQueueEntry(CUpDownClient *client)
{
	if (client == NULL)
		return;

	CSingleLock lock(&m_csActiveUploadState, TRUE);
	m_clientEntries.erase(client);
	SyncLegacyUploadState(client, EUploadSlotPhase::None);
}

bool CUploadQueue::HasWaitingMember(const CUpDownClient *client) const
{
	CSingleLock lock(const_cast<CCriticalSection*>(&m_csActiveUploadState), TRUE);
	const ClientQueueEntry *entry = FindClientQueueEntry(client);
	return entry != NULL
		&& entry->phase == EUploadSlotPhase::Waiting
		&& m_waitingClientIndexes.find(client) != m_waitingClientIndexes.cend();
}

bool CUploadQueue::FindWaitingClientIndex(const CUpDownClient *client, size_t &index) const
{
	const auto it = m_waitingClientIndexes.find(client);
	if (it == m_waitingClientIndexes.cend())
		return false;
	index = it->second;
	return true;
}

UploadSessionPtr CUploadQueue::GetUploadSession(const CUpDownClient *client) const
{
	CSingleLock lock(const_cast<CCriticalSection*>(&m_csActiveUploadState), TRUE);
	const ClientQueueEntry *entry = FindClientQueueEntry(client);
	return (entry != NULL) ? entry->session : UploadSessionPtr();
}

std::vector<UploadSessionPtr> CUploadQueue::GetActiveUploadSessions() const
{
	std::vector<UploadSessionPtr> sessions;
	CSingleLock lock(const_cast<CCriticalSection*>(&m_csActiveUploadState), TRUE);
	sessions.reserve(m_activeUploadClients.size());
	for (CUpDownClient *client : m_activeUploadClients) {
		const ClientQueueEntry *entry = FindClientQueueEntry(client);
		if (entry != NULL && entry->session != NULL)
			sessions.push_back(entry->session);
	}
	return sessions;
}

bool CUploadQueue::IsCurrentUploadSession(const UploadSessionPtr &session) const
{
	if (session == NULL || session->client == NULL)
		return false;
	return GetUploadSession(session->client) == session;
}

bool CUploadQueue::HasUploadSessionIoError(const UploadSessionPtr &session) const
{
	if (session == NULL)
		return true;
	CSingleLock lockBlockLists(&session->blockListsLock, TRUE);
	return session->ioError;
}

bool CUploadQueue::IsDownloading(const CUpDownClient *client) const
{
	return GetUploadSession(client) != NULL;
}

INT_PTR CUploadQueue::GetUploadQueueLength() const
{
	const std::shared_ptr<const ActiveUploadSnapshot> snapshot = GetActiveUploadSnapshot();
	return (snapshot != NULL) ? static_cast<INT_PTR>(snapshot->activeClients.size()) : 0;
}

void CUploadQueue::GetActiveUploadClientsInSlotOrder(std::vector<CUpDownClient*> &clients) const
{
	const std::shared_ptr<const ActiveUploadSnapshot> snapshot = GetActiveUploadSnapshot();
	clients = (snapshot != NULL) ? snapshot->activeClients : std::vector<CUpDownClient*>();
}

UINT CUploadQueue::GetActiveUploadSlotNumber(const CUpDownClient *client) const
{
	if (client == NULL)
		return 0;
	const std::shared_ptr<const ActiveUploadSnapshot> snapshot = GetActiveUploadSnapshot();
	if (snapshot == NULL)
		return 0;
	const auto it = snapshot->slotNumberByClient.find(client);
	return (it != snapshot->slotNumberByClient.cend()) ? it->second : 0;
}

bool CUploadQueue::TryGetActiveUploadVisualState(const CUpDownClient *client, ActiveUploadVisualState &state) const
{
	state = ActiveUploadVisualState();
	if (client == NULL)
		return false;

	state.slotNumber = GetActiveUploadSlotNumber(client);

	state.isActivating = IsClientUploadActivating(client);
	state.isActive = IsClientUploadActive(client);

	const UploadSessionPtr session = GetUploadSession(client);
	if (session == NULL)
		return state.slotNumber != 0 || state.isActivating || state.isActive;

	CSingleLock lockBlockLists(&session->blockListsLock, TRUE);
	if (!session->blockRequests.IsEmpty()) {
		const Requested_Block_Struct *block = session->blockRequests.GetHead();
		if (block != NULL) {
			const uint64 start = (block->StartOffset / PARTSIZE) * PARTSIZE;
			state.pendingRanges.push_back(ActiveUploadRange{start, start + PARTSIZE});
		}
	}
	if (!session->completedBlocks.IsEmpty()) {
		const Requested_Block_Struct *block = session->completedBlocks.GetHead();
		if (block != NULL) {
			const uint64 start = (block->StartOffset / PARTSIZE) * PARTSIZE;
			state.pendingRanges.push_back(ActiveUploadRange{start, start + PARTSIZE});
		}
		for (POSITION pos = session->completedBlocks.GetHeadPosition(); pos != NULL;) {
			block = session->completedBlocks.GetNext(pos);
			if (block != NULL)
				state.completedRanges.push_back(ActiveUploadRange{block->StartOffset, block->EndOffset + 1});
		}
	}
	return true;
}

bool CUploadQueue::EnqueueUploadRequestBlock(CUpDownClient *client, Requested_Block_Struct *reqblock, INT_PTR *pQueueCount)
{
	if (pQueueCount != NULL)
		*pQueueCount = 0;
	if (client == NULL || reqblock == NULL)
		return false;

	CKnownFile *srcfile = theApp.sharedfiles->GetFileByID(reqblock->FileID);
	const UploadSessionPtr session = GetUploadSession(client);
	if (session == NULL) {
		if (srcfile != NULL)
			DebugLogError(_T("AddReqBlock: Uploading client not found in Uploadlist, %s, %s"), (LPCTSTR)client->DbgGetClientInfo(), (LPCTSTR)srcfile->GetFileName());
		delete reqblock;
		return false;
	}

	CSingleLock lockBlockLists(&session->blockListsLock, TRUE);
	if (!lockBlockLists.IsLocked()) {
		ASSERT(0);
		delete reqblock;
		return false;
	}

	if (session->ioError) {
		if (srcfile != NULL)
			DebugLogWarning(_T("AddReqBlock: Uploading client has pending IO Error, %s, %s"), (LPCTSTR)client->DbgGetClientInfo(), (LPCTSTR)srcfile->GetFileName());
		delete reqblock;
		return false;
	}

	for (POSITION pos = session->completedBlocks.GetHeadPosition(); pos != NULL;) {
		const Requested_Block_Struct *cur_reqblock = session->completedBlocks.GetNext(pos);
		if (reqblock->StartOffset == cur_reqblock->StartOffset
			&& reqblock->EndOffset == cur_reqblock->EndOffset
			&& md4equ(reqblock->FileID, cur_reqblock->FileID))
		{
			delete reqblock;
			return false;
		}
	}
	for (POSITION pos = session->blockRequests.GetHeadPosition(); pos != NULL;) {
		const Requested_Block_Struct *cur_reqblock = session->blockRequests.GetNext(pos);
		if (reqblock->StartOffset == cur_reqblock->StartOffset
			&& reqblock->EndOffset == cur_reqblock->EndOffset
			&& md4equ(reqblock->FileID, cur_reqblock->FileID))
		{
			delete reqblock;
			return false;
		}
	}

	session->blockRequests.AddTail(reqblock);
	if (pQueueCount != NULL)
		*pQueueCount = session->blockRequests.GetCount();
	return true;
}

bool CUploadQueue::TryPeekNextUploadBlockRead(const UploadSessionPtr &session, uint64 currentPayload, uint32 bufferLimit, Requested_Block_Struct &block, uint64 &addedPayloadQueueSession) const
{
	if (session == NULL || session->client == NULL)
		return false;

	CSingleLock lockBlockLists(&session->blockListsLock, TRUE);
	if (!lockBlockLists.IsLocked() || session->ioError || session->blockRequests.IsEmpty())
		return false;

	addedPayloadQueueSession = session->client->GetQueueSessionUploadAdded();
	if (addedPayloadQueueSession > currentPayload && addedPayloadQueueSession - currentPayload >= bufferLimit)
		return false;

	const Requested_Block_Struct *currentblock = session->blockRequests.GetHead();
	if (currentblock == NULL)
		return false;

	block = *currentblock;
	return true;
}

bool CUploadQueue::PromotePendingUploadBlockRead(const UploadSessionPtr &session, const Requested_Block_Struct &expectedBlock, uint64 addedPayloadQueueSession)
{
	if (session == NULL || session->client == NULL)
		return false;

	CSingleLock lockBlockLists(&session->blockListsLock, TRUE);
	if (!lockBlockLists.IsLocked() || session->ioError || session->blockRequests.IsEmpty())
		return false;

	Requested_Block_Struct *currentblock = session->blockRequests.GetHead();
	if (currentblock == NULL
		|| currentblock->StartOffset != expectedBlock.StartOffset
		|| currentblock->EndOffset != expectedBlock.EndOffset
		|| !md4equ(currentblock->FileID, expectedBlock.FileID))
	{
		return false;
	}

	session->client->SetQueueSessionUploadAdded(addedPayloadQueueSession);
	session->completedBlocks.AddHead(session->blockRequests.RemoveHead());
	return true;
}

void CUploadQueue::MarkUploadSessionIoError(const UploadSessionPtr &session)
{
	if (session == NULL)
		return;
	CSingleLock lockBlockLists(&session->blockListsLock, TRUE);
	session->ioError = true;
}

bool CUploadQueue::AddActiveUploadSession(CUpDownClient *client)
{
	if (client == NULL)
		return false;

	theApp.uploadBandwidthThrottler->AddToStandardList(GetUploadQueueLength(), client->GetFileUploadSocket());

	{
		CSingleLock lock(&m_csActiveUploadState, TRUE);
		ClientQueueEntry &entry = EnsureClientQueueEntry(client);
		if (entry.session != NULL)
			return false;
		const UploadSessionPtr session = std::make_shared<UploadSession>(client);
		m_activeUploadClients.push_back(client);
		entry.session = session;
	}
	PublishActiveUploadSnapshot();
	return true;
}

void CUploadQueue::ResetUploadSessionState(const UploadSessionPtr &session)
{
	if (session == NULL)
		return;

	session->client->FlushSendBlocks();

	CSingleLock lockBlockLists(&session->blockListsLock, TRUE);
	while (!session->blockRequests.IsEmpty())
		delete session->blockRequests.RemoveHead();
	while (!session->completedBlocks.IsEmpty())
		delete session->completedBlocks.RemoveHead();
	session->ioError = false;
	session->disableCompression = false;
}

bool CUploadQueue::RemoveActiveUploadSession(CUpDownClient *client)
{
	UploadSessionPtr session;
	{
		CSingleLock lock(&m_csActiveUploadState, TRUE);
		ClientQueueEntry *entry = FindClientQueueEntry(client);
		if (entry == NULL || entry->session == NULL)
			return false;

		session = entry->session;
		entry->session.reset();

		const auto activeIt = std::find(m_activeUploadClients.begin(), m_activeUploadClients.end(), client);
		if (activeIt != m_activeUploadClients.end())
			m_activeUploadClients.erase(activeIt);
	}

	PublishActiveUploadSnapshot();
	ResetUploadSessionState(session);
	theApp.uploadBandwidthThrottler->RemoveFromStandardList(client->socket);
	return true;
}

bool CUploadQueue::ActivateUploadClient(CUpDownClient *newclient, LPCTSTR pszReason, bool bRemoveFromWaitingQueue)
{
	if (IsDownloading(newclient))
		return false;

	if (bRemoveFromWaitingQueue) {
		size_t index = 0;
		if (!FindWaitingClientIndex(newclient, index))
			return false;
		if (!RemoveWaitingClientAt(index, true))
			return false;
	}

	if (pszReason && thePrefs.GetLogUlDlEvents())
		AddDebugLogLine(false, _T("Adding client to upload list: %s Client: %s"), pszReason, (LPCTSTR)newclient->DbgGetClientInfo());

	// tell the client that we are now ready to upload
	if (!newclient->socket || !newclient->socket->IsConnected() || !newclient->CheckHandshakeFinished()) {
		BeginActivationState(newclient);
		if (!newclient->TryToConnect(true)) {
			ClearClientQueueEntry(newclient);
			return false;
		}
	} else {
		if (!CompleteUploadActivation(newclient)) {
			ClearClientQueueEntry(newclient);
			return false;
		}
	}
	newclient->SetUpStartTime();
	newclient->ResetSessionUp();
	newclient->ClearSlowUploadCooldown();
	newclient->ResetSlowUploadTracking();

	if (!AddActiveUploadSession(newclient)) {
		ClearClientQueueEntry(newclient);
		return false;
	}

	m_nLastStartUpload = ::GetTickCount64();

	// statistic
	CKnownFile *reqfile = theApp.sharedfiles->GetFileByID((uchar*)newclient->GetUploadFileID());
	if (reqfile)
		reqfile->statistic.AddAccepted();

	theApp.emuledlg->transferwnd->GetUploadList()->AddClient(newclient);

	return true;
}

bool CUploadQueue::StartNextUpload(LPCTSTR pszReason)
{
	CUpDownClient *newclient = SelectNextWaitingClient();
	if (newclient == NULL)
		return false;

	return ActivateUploadClient(newclient, pszReason, true);
}

bool CUploadQueue::CompleteUploadActivation(CUpDownClient *client)
{
	if (client == NULL || client->socket == NULL || !client->socket->IsConnected() || !client->CheckHandshakeFinished())
		return false;

	MarkActiveState(client);

	if (thePrefs.GetDebugClientTCPLevel() > 0)
		DebugSend("OP_AcceptUploadReq", client);
	Packet *packet = new Packet(OP_ACCEPTUPLOADREQ, 0);
	theStats.AddUpDataOverheadFileRequest(packet->size);
	client->SendPacket(packet);
	return true;
}

void CUploadQueue::UpdateActiveClientsInfo(ULONGLONG curTick)
{
	// Save number of active clients for statistics
	INT_PTR tempHighest = theApp.uploadBandwidthThrottler->GetHighestNumberOfFullyActivatedSlotsSinceLastCallAndReset();

	//if(thePrefs.GetLogUlDlEvents() && theApp.uploadBandwidthThrottler->GetStandardListSize() > GetUploadQueueLength())
		// debug info, will remove this when I'm done.
	//	AddDebugLogLine(false, _T("UploadQueue: Error! Throttler has more slots than UploadQueue! Throttler: %i UploadQueue: %i Tick: %i"), theApp.uploadBandwidthThrottler->GetStandardListSize(), GetUploadQueueLength(), ::GetTickCount64());

	tempHighest = min(tempHighest, GetUploadQueueLength() + 1);
	m_iHighestNumberOfFullyActivatedSlotsSinceLastCall = tempHighest;

	// save some data about the number of fully active clients
	INT_PTR tempMaxRemoved = 0;
	while (!activeClients_hist.IsEmpty() && curTick >= activeClients_hist.Head().timestamp + SEC2MS(20)) {
		tempMaxRemoved = max(tempMaxRemoved, activeClients_hist.Head().slots);
		activeClients_hist.RemoveHead();
	}

	activeClients_hist.AddTail(ActiveClientsData{tempHighest, curTick});

	if (activeClients_hist.Count() <= 1)
		m_MaxActiveClients = m_MaxActiveClientsShortTime = m_iHighestNumberOfFullyActivatedSlotsSinceLastCall;
	else {
		INT_PTR tempMax, tempMaxShortTime;
		tempMax = tempMaxShortTime = m_iHighestNumberOfFullyActivatedSlotsSinceLastCall;
		for (UINT_PTR ix = activeClients_hist.Count(); ix-- > 0;) {
			const ActiveClientsData &d = activeClients_hist[ix];
			if (curTick >= d.timestamp + SEC2MS(10) && (tempMaxRemoved <= tempMax || tempMaxRemoved < m_MaxActiveClients))
				break;
			if (d.slots > tempMax)
				tempMax = d.slots;
			if (d.slots > tempMaxShortTime && curTick < d.timestamp + SEC2MS(10))
				tempMaxShortTime = d.slots;
		}
		if (tempMaxRemoved >= m_MaxActiveClients || tempMax > m_MaxActiveClients)
			m_MaxActiveClients = tempMax;
		m_MaxActiveClientsShortTime = tempMaxShortTime;
	}
}

CUploadQueue::UploadSchedulingSnapshot CUploadQueue::CaptureSchedulingSnapshot(bool throttlerWantsMoreSlots) const
{
	UploadSchedulingSnapshot snapshot;
	snapshot.uploadSlots = GetUploadQueueLength();
	snapshot.softMaxUploadSlots = GetSoftMaxUploadSlots();
	snapshot.highestFullyActivatedSlots = m_iHighestNumberOfFullyActivatedSlotsSinceLastCall;
	snapshot.budgetBytesPerSec = GetEffectiveUploadBudgetBytesPerSec();
	snapshot.toNetworkDatarate = GetToNetworkDatarate();
	snapshot.targetPerSlot = GetTargetClientDataRateBroadband();
	snapshot.hasWaitingClients = GetWaitingMemberCount() > 0;
	snapshot.throttlerWantsMoreSlots = throttlerWantsMoreSlots;
	return snapshot;
}

/**
 * Maintenance method for the uploading slots. It adds and removes clients to the
 * uploading list. It also makes sure that all the uploading slots' Sockets
 * always have enough packets in their queues, etc.
 *
 * This method is called approximately once every 100 milliseconds.
 */
void CUploadQueue::Process()
{
	const ULONGLONG curTick = ::GetTickCount64();
	m_bThrottlerWantsMoreSlotsHint = theApp.uploadBandwidthThrottler->GetNeedsMoreBandwidthSlotsSinceLastCallAndReset();
	UpdateActiveClientsInfo(curTick);

	const UploadSchedulingSnapshot snapshot = CaptureSchedulingSnapshot(m_bThrottlerWantsMoreSlotsHint);
	if (ShouldOpenUploadSlot(snapshot, false))
		// There's not enough open uploads. Open another one.
		StartNextUpload(_T("Not enough open upload slots for the current speed"));

	// The loop that feeds the upload slots with data.
	for (const UploadSessionPtr &session : GetActiveUploadSessions()) {
		CUpDownClient *cur_client = session->client;
		if (thePrefs.m_iDbgHeap >= 2)
			ASSERT_VALID(cur_client);
		//It seems chatting or friend slots can get stuck at times in upload. This needs to be looked into.
		if (cur_client->socket == NULL) {
			HandleUploadSlotTeardown(cur_client, _T("Uploading to client without socket? (CUploadQueue::Process)"));
			if (cur_client->Disconnected(_T("CUploadQueue::Process")))
				delete cur_client;
		} else {
			cur_client->UpdateUploadingStatisticsData();
			if (HasUploadSessionIoError(session)) {
				HandleUploadSlotTeardown(cur_client, _T("IO/Other Error while creating data packet (see earlier log entries)"), false, true);
				continue;
			}
			const UploadSlotEndDecision endDecision = EvaluateUploadSlotEnd(cur_client, snapshot);
			if (endDecision.shouldEnd) {
				EndUploadSession(cur_client, endDecision);
				continue;
			}
			// Increase the sockets buffer for fast uploads (was in UpdateUploadingStatisticsData()).
			// This should be done in the throttling thread, but the throttler
			// does not have access to the client's download rate
			if (cur_client->GetUploadDatarate() > 512 * 1024) {
				CEMSocket *sock = cur_client->GetFileUploadSocket();
				if (sock)
					sock->UseBigSendBuffer();
			}

			// check if the file id of the topmost block request matches the current upload file, otherwise
			// the IO thread will wait for us (only for this client of course) to fix it for cross-thread sync reasons
			CSingleLock lockBlockLists(&session->blockListsLock, TRUE);
			ASSERT(lockBlockLists.IsLocked());
			// be careful what functions to call while having locks, RemoveFromUploadQueue could,
			// for example, lead to a deadlock here because it can remove the upload session,
			// while the IO thread is enumerating sessions and then taking the blocklist lock
			if (!session->blockRequests.IsEmpty()) {
				const Requested_Block_Struct *pHeadBlock = session->blockRequests.GetHead();
				if (!md4equ(pHeadBlock->FileID, cur_client->GetUploadFileID())) {
					uchar aucNewID[MDX_DIGEST_SIZE];
					md4cpy(aucNewID, pHeadBlock->FileID);

					lockBlockLists.Unlock();

					CKnownFile *pCurrentUploadFile = theApp.sharedfiles->GetFileByID(aucNewID);
					if (pCurrentUploadFile != NULL)
						cur_client->SetUploadFileID(pCurrentUploadFile);
					else
						HandleUploadSlotTeardown(cur_client, _T("Requested FileID in block request not found in shared files"), false, true);
				}
			}
		}
	}

	// Save used bandwidth for speed calculations
	(void)theApp.uploadBandwidthThrottler->GetNumberOfSentBytesOverheadSinceLastCallAndReset(); //reset only
	uint64 sentBytes = theApp.uploadBandwidthThrottler->GetNumberOfSentBytesSinceLastCallAndReset();
	m_average_ur_sum += sentBytes;

	average_ur_hist.AddTail(AverageUploadRate{sentBytes, theStats.sessionSentBytesToFriend, curTick});
	// keep no more than 30 secs of data
	while (average_ur_hist.Count() > 3 && curTick >= average_ur_hist.Head().timestamp + SEC2MS(30)) {
		m_average_ur_sum -= average_ur_hist.Head().upBytes;
		average_ur_hist.RemoveHead();
	}
};

uint32 CUploadQueue::GetTargetClientDataRate(bool bMinDatarate) const
{
	const uint32 uBroadbandTarget = GetTargetClientDataRateBroadband();
	if (uBroadbandTarget > 0)
		return bMinDatarate ? (std::max<uint32>)(3 * 1024, uBroadbandTarget * 3 / 4) : uBroadbandTarget;

	uint32 nOpenSlots = (uint32)GetUploadQueueLength();
	// 3 slots or less - 3KiB/s
	// 4 slots or more - linear growth by 1 KiB/s steps, cap off at UPLOAD_CLIENT_MAXDATARATE
	uint32 nResult;
	if (nOpenSlots <= 3)
		nResult = 3 * 1024;
	else
		nResult = min(UPLOAD_CLIENT_MAXDATARATE, nOpenSlots * 1024);

	return bMinDatarate ? nResult * 3 / 4 : nResult;
}

bool CUploadQueue::ShouldOpenUploadSlot(const UploadSchedulingSnapshot &snapshot, bool allowEmptyWaitingQueue) const
{
	if (!allowEmptyWaitingQueue && !snapshot.hasWaitingClients)
		return false;

	INT_PTR curUploadSlots = snapshot.uploadSlots;

	if (curUploadSlots < MIN_UP_CLIENTS_ALLOWED)
		return true;

	if (::GetTickCount64() < m_nLastStartUpload + SEC2MS(1) && datarate < 102400)
		return false;

	if (curUploadSlots < max(MIN_UP_CLIENTS_ALLOWED, 4))
		return true;
	if (curUploadSlots >= MAX_UP_CLIENTS_ALLOWED)
		return false;
	if (curUploadSlots < snapshot.softMaxUploadSlots)
		return true;
	if (curUploadSlots > snapshot.softMaxUploadSlots)
		return false;
	if (snapshot.budgetBytesPerSec == 0)
		return false;
	if (snapshot.toNetworkDatarate + (std::max<uint32>)(snapshot.targetPerSlot / 3, 1024u) >= snapshot.budgetBytesPerSec)
		return false;
	if (!snapshot.throttlerWantsMoreSlots)
		return false;
	return snapshot.highestFullyActivatedSlots > snapshot.uploadSlots;
}

uint32 CUploadQueue::GetEffectiveUploadBudgetBytesPerSec() const
{
	const uint32 uConfiguredUploadKiB = thePrefs.GetMaxUpload();
	if (uConfiguredUploadKiB == UNLIMITED)
		return 0;
	return uConfiguredUploadKiB * 1024u;
}

INT_PTR CUploadQueue::GetSoftMaxUploadSlots() const
{
	return (std::max<INT_PTR>)(MIN_UP_CLIENTS_ALLOWED, thePrefs.GetBBMaxUploadClientsAllowed());
}

uint32 CUploadQueue::GetTargetClientDataRateBroadband() const
{
	const INT_PTR iSoftMaxSlots = GetSoftMaxUploadSlots();
	if (iSoftMaxSlots <= 0)
		return 0;

	const uint32 uBudgetBytesPerSec = GetEffectiveUploadBudgetBytesPerSec();
	if (uBudgetBytesPerSec == 0)
		return 0;

	return (std::max<uint32>)(3 * 1024, uBudgetBytesPerSec / static_cast<uint32>(iSoftMaxSlots));
}

uint32 CUploadQueue::GetSlowUploadRateThreshold() const
{
	const uint32 uTargetPerSlot = GetTargetClientDataRateBroadband();
	if (uTargetPerSlot == 0)
		return 3 * 1024;

	const float fFactor = (std::max)(0.05f, thePrefs.GetBBSlowUploadThresholdFactor());
	return (std::max<uint32>)(1024u, static_cast<uint32>(uTargetPerSlot * fFactor));
}

bool CUploadQueue::ShouldTrackSlowUploadSlots(const UploadSchedulingSnapshot &snapshot) const
{
	if (!snapshot.hasWaitingClients)
		return false;
	if (snapshot.uploadSlots < snapshot.softMaxUploadSlots)
		return false;
	if (snapshot.budgetBytesPerSec == 0)
		return false;
	return snapshot.toNetworkDatarate + (std::max<uint32>)(snapshot.targetPerSlot / 3, 1024u) < snapshot.budgetBytesPerSec;
}

CUpDownClient* CUploadQueue::GetWaitingClientByIP_UDP(uint32 dwIP, uint16 nUDPPort, bool bIgnorePortOnUniqueIP, bool *pbMultipleIPs)
{
	const std::shared_ptr<const WaitingQueueSnapshot> snapshot = GetWaitingSnapshot();
	if (snapshot == NULL) {
		if (pbMultipleIPs != NULL)
			*pbMultipleIPs = false;
		return NULL;
	}

	const auto endpointIt = snapshot->clientsByUdpEndpoint.find(WaitingUdpEndpointKey{dwIP, nUDPPort});
	if (endpointIt != snapshot->clientsByUdpEndpoint.cend())
		return endpointIt->second;

	const auto ipIt = snapshot->clientsByIP.find(dwIP);
	const uint32 cMatches = (ipIt != snapshot->clientsByIP.cend()) ? static_cast<uint32>(ipIt->second.size()) : 0;
	if (pbMultipleIPs != NULL)
		*pbMultipleIPs = cMatches > 1;

	if (bIgnorePortOnUniqueIP && ipIt != snapshot->clientsByIP.cend() && cMatches == 1)
		return ipIt->second.front();
	return NULL;
}

CUpDownClient* CUploadQueue::GetWaitingClientByIP(uint32 dwIP) const
{
	const std::shared_ptr<const WaitingQueueSnapshot> snapshot = GetWaitingSnapshot();
	if (snapshot == NULL)
		return NULL;
	const auto it = snapshot->clientsByIP.find(dwIP);
	return (it != snapshot->clientsByIP.cend() && !it->second.empty()) ? it->second.front() : NULL;
}

/**
 * Add a client to the waiting queue for uploads.
 *
 * @param client address of the client that should be added to the waiting queue
 *
 * @param bIgnoreTimelimit don't check time limit to possibly ban the client.
 */
void CUploadQueue::AddClientToQueue(CUpDownClient *client, bool bIgnoreTimelimit)
{
	if (ShouldRejectLowIdQueueRequest(client))
		return;

	TrackExternalQueueRequest(client, bIgnoreTimelimit);
	if (client->IsBanned())
		return;
	uint16 cSameIP = 0;
	if (ResolveExternalQueueConflicts(client, cSameIP))
		return;

	if (HasTooManyQueuedClientsFromSameIP(client, cSameIP))
		return;

	RecordExternalQueueRequestStat(client);
	(void)AdmitClientToQueue(client, _T("Direct add with empty queue."));
}

void CUploadQueue::AddClientToWaitingList(CUpDownClient *client)
{
	client->SetWaitStartTime();
	client->SetAskedCount(1);
	EnterWaitingState(client);
	m_bStatisticsWaitingListDirty = true;
	m_waitingClients.push_back(client);
	RebuildWaitingIndexes();
	PublishWaitingSnapshot();
	theApp.emuledlg->transferwnd->GetQueueList()->AddClient(client);
	theApp.emuledlg->transferwnd->ShowQueueCount(GetWaitingUserCount());
	client->SendRankingInfo();
}

bool CUploadQueue::HandleUploadSlotTeardown(CUpDownClient *client, LPCTSTR pszReason, bool removeWaiting, bool updatewindow, bool earlyabort)
{
	const bool removedUpload = RemoveActiveUploadSlot(client, pszReason, updatewindow, earlyabort);
	const bool removedWaiting = removeWaiting ? RemoveWaitingClient(client, updatewindow) : false;
	if (!removedUpload && !removedWaiting && removeWaiting)
		ClearClientQueueEntry(client);
	return removedUpload || removedWaiting;
}

bool CUploadQueue::RemoveActiveUploadSlot(CUpDownClient *client, LPCTSTR pszReason, bool updatewindow, bool earlyabort)
{
	BeginRetiringState(client);
	const UploadSessionPtr session = GetUploadSession(client);
	if (session == NULL) {
		ClearClientQueueEntry(client);
		return false;
	}

	if (updatewindow)
		theApp.emuledlg->transferwnd->GetUploadList()->RemoveClient(client);

	int pendingBlocks = 0;
	{
		CSingleLock lockBlockLists(&session->blockListsLock, TRUE);
		pendingBlocks = static_cast<int>(session->blockRequests.GetCount());
	}

	if (thePrefs.GetLogUlDlEvents()) {
		AddDebugLogLine(DLP_DEFAULT, true, _T("Removing client from upload list: %s Client: %s Transferred: %s SessionUp: %s QueueSessionPayload: %s In buffer: %s Req blocks: %i File: %s")
			, pszReason == NULL ? _T("") : pszReason
			, (LPCTSTR)client->DbgGetClientInfo()
			, (LPCTSTR)CastSecondsToHM(client->GetUpStartTimeDelay() / SEC2MS(1))
			, (LPCTSTR)CastItoXBytes(client->GetSessionUp())
			, (LPCTSTR)CastItoXBytes(client->GetQueueSessionPayloadUp())
			, (LPCTSTR)CastItoXBytes(client->GetPayloadInBuffer()), pendingBlocks
			, theApp.sharedfiles->GetFileByID(client->GetUploadFileID()) ? (LPCTSTR)theApp.sharedfiles->GetFileByID(client->GetUploadFileID())->GetFileName() : _T(""));
	}

	if (!RemoveActiveUploadSession(client)) {
		ClearClientQueueEntry(client);
		return false;
	}

	if (client->GetSessionUp() > 0) {
		++successfullupcount;
		totaluploadtime += client->GetUpStartTimeDelay() / SEC2MS(1);
	} else
		failedupcount += static_cast<uint32>(!earlyabort);

	CKnownFile *requestedFile = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
	if (requestedFile != NULL)
		requestedFile->UpdatePartsInfo();

	theApp.clientlist->AddTrackClient(client); // Keep track of this client
	ClearClientQueueEntry(client);
	m_iHighestNumberOfFullyActivatedSlotsSinceLastCall = 0;
	return true;
}

uint32 CUploadQueue::GetAverageUpTime() const
{
	return successfullupcount ? (totaluploadtime / successfullupcount) : 0;
}

bool CUploadQueue::RemoveWaitingClient(CUpDownClient *client, bool updatewindow)
{
	size_t index = 0;
	return FindWaitingClientIndex(client, index) ? RemoveWaitingClientAt(index, updatewindow) : false;
}

bool CUploadQueue::RemoveWaitingClientAt(size_t index, bool updatewindow, bool publishSnapshot)
{
	if (index >= m_waitingClients.size())
		return false;

	m_bStatisticsWaitingListDirty = true;
	CUpDownClient *todelete = m_waitingClients[index];
	m_waitingClients.erase(m_waitingClients.begin() + static_cast<std::ptrdiff_t>(index));
	RebuildWaitingIndexes();
	if (publishSnapshot)
		PublishWaitingSnapshot();
	if (updatewindow) {
		theApp.emuledlg->transferwnd->GetQueueList()->RemoveClient(todelete);
		theApp.emuledlg->transferwnd->ShowQueueCount(GetWaitingUserCount());
	}
	ClearClientQueueEntry(todelete);
	return true;
}

LPCTSTR CUploadQueue::GetUploadSlotEndReasonText(EUploadSlotEndReason eReason)
{
	switch (eReason) {
	case EUploadSlotEndReason::BroadbandSlowSlotRecycle:
		return _T("Broadband slow slot recycle");
	case EUploadSlotEndReason::BroadbandSessionTransferLimit:
		return _T("Broadband session transfer limit");
	case EUploadSlotEndReason::BroadbandSessionTimeLimit:
		return _T("Broadband session time limit");
	default:
		return _T("Completed transfer");
	}
}

bool CUploadQueue::EndUploadSession(CUpDownClient *client, const UploadSlotEndDecision &decision)
{
	if (!decision.shouldEnd)
		return false;

	if (!RemoveActiveUploadSlot(client, GetUploadSlotEndReasonText(decision.reason), true))
		return false;

	if (decision.requeue)
		return RequeueClientAfterUploadSession(client);

	return true;
}

CUploadQueue::UploadSlotEndDecision CUploadQueue::EvaluateUploadSlotEnd(CUpDownClient *client, const UploadSchedulingSnapshot &snapshot)
{
	UploadSlotEndDecision decision;

	//If we have nobody in the queue, do NOT remove the current uploads.
	//This will save some bandwidth and some unneeded swapping from upload/queue/upload.
	if (!snapshot.hasWaitingClients)
		return decision;

	const bool bShouldOpenReplacementSlot = ShouldOpenUploadSlot(snapshot, false);

	if (ShouldTrackSlowUploadSlots(snapshot)) {
		client->UpdateSlowUploadTracking(::GetTickCount64(), GetSlowUploadRateThreshold());
		if (client->ShouldRecycleSlowUpload(SEC2MS(thePrefs.GetBBSlowUploadGraceSeconds()), SEC2MS(thePrefs.GetBBZeroRateGraceSeconds()))) {
			client->SetSlowUploadCooldownUntil(::GetTickCount64() + SEC2MS(thePrefs.GetBBSlowUploadCooldownSeconds()));
			if (thePrefs.GetLogUlDlEvents()) {
				if (client->GetAccumulatedZeroUploadMs() >= SEC2MS(thePrefs.GetBBZeroRateGraceSeconds()))
					AddDebugLogLine(DLP_LOW, false, _T("%s: Upload slot recycled due to zero upload during broadband underfill."), client->GetUserName());
				else
					AddDebugLogLine(DLP_LOW, false, _T("%s: Upload slot recycled due to slow upload during broadband underfill."), client->GetUserName());
			}
			client->ResetSlowUploadTracking();
			decision.shouldEnd = true;
			decision.requeue = true;
			decision.reason = EUploadSlotEndReason::BroadbandSlowSlotRecycle;
			return decision;
		}
	} else {
		client->ResetSlowUploadTracking();
	}

	const CKnownFile *pUploadingFile = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
	const uint64 uSessionTransferLimit = ResolveBBSessionTransferLimitBytes(pUploadingFile);
	if (uSessionTransferLimit > 0) {
		// Allow the client to download a specified amount per session; but keep going if no one needs this slot.
		if (client->GetQueueSessionPayloadUp() > uSessionTransferLimit && bShouldOpenReplacementSlot) {
			if (thePrefs.GetLogUlDlEvents())
				AddDebugLogLine(DLP_DEFAULT, false, _T("%s: Upload session ended due to broadband transfer limit (%s)"), client->GetUserName(), (LPCTSTR)CastItoXBytes(uSessionTransferLimit));
			decision.shouldEnd = true;
			decision.requeue = true;
			decision.reason = EUploadSlotEndReason::BroadbandSessionTransferLimit;
			return decision;
		}
	}

	const UINT uSessionTimeLimitSeconds = thePrefs.GetBBSessionTimeLimitSeconds();
	if (uSessionTimeLimitSeconds > 0 && client->GetUpStartTimeDelay() > SEC2MS(uSessionTimeLimitSeconds) && bShouldOpenReplacementSlot) {
		if (thePrefs.GetLogUlDlEvents())
			AddDebugLogLine(DLP_LOW, false, _T("%s: Upload session ended due to broadband time limit %s."), client->GetUserName(), (LPCTSTR)CastSecondsToHM(uSessionTimeLimitSeconds));
		decision.shouldEnd = true;
		decision.requeue = true;
		decision.reason = EUploadSlotEndReason::BroadbandSessionTimeLimit;
		return decision;
	}

	return decision;
}

void CUploadQueue::DeleteAll()
{
	for (CUpDownClient *client : m_waitingClients) {
		ClearClientQueueEntry(client);
	}
	m_waitingClients.clear();
	RebuildWaitingIndexes();
	PublishWaitingSnapshot();

	{
		CSingleLock lock(&m_csActiveUploadState, TRUE);
		for (const auto &entry : m_clientEntries) {
			if (entry.second.session != NULL)
				ResetUploadSessionState(entry.second.session);
		}
		m_activeUploadClients.clear();
		m_clientEntries.clear();
	}
	PublishActiveUploadSnapshot();
	// PENDING: Remove from UploadBandwidthThrottler as well!
}

UINT CUploadQueue::GetWaitingPosition(const CUpDownClient *client)
{
	if (!IsClientWaitingForUpload(client))
		return 0;
	const std::shared_ptr<const WaitingQueueSnapshot> snapshot = GetWaitingSnapshot();
	if (snapshot == NULL)
		return 0;
	const auto it = snapshot->positionByClient.find(client);
	return (it != snapshot->positionByClient.cend()) ? it->second : 0;
}

VOID CALLBACK CUploadQueue::UploadTimer(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR /*idEvent*/, DWORD /*dwTime*/) noexcept
{
	// NOTE: Always handle all type of MFC exceptions in TimerProcs - otherwise we'll get mem leaks
	try {
		// Barry - Don't do anything if the app is shutting down - can cause unhandled exceptions
		if (theApp.IsClosing())
			return;

		// Elandal:ThreadSafeLogging -->
		// other threads may have queued up log lines. This prints them.
		theApp.HandleDebugLogQueue();
		theApp.HandleLogQueue();
		// Elandal: ThreadSafeLogging <--

		theApp.uploadqueue->Process();
		theApp.downloadqueue->Process();
		if (thePrefs.ShowOverhead()) {
			theStats.CompUpDatarateOverhead();
			theStats.CompDownDatarateOverhead();
		}

		// one second
		if (++i1sec >= 10) {
			i1sec = 0;

			// try to use different time intervals here to avoid disk I/O congestion by saving all files at once
			theApp.clientcredits->Process();	// 13 minutes
			theApp.serverlist->Process();		// 17 minutes
			theApp.knownfiles->Process();		// 11 minutes
			theApp.friendlist->Process();		// 19 minutes
			theApp.clientlist->Process();
			theApp.sharedfiles->Process();
			if (Kademlia::CKademlia::IsRunning()) {
				Kademlia::CKademlia::Process();
				if (Kademlia::CKademlia::GetPrefs()->HasLostConnection()) {
					Kademlia::CKademlia::Stop();
					theApp.emuledlg->ShowConnectionState();
				}
			}
			if (theApp.serverconnect->IsConnecting() && !theApp.serverconnect->IsSingleConnect())
				theApp.serverconnect->TryAnotherConnectionRequest();

			theApp.listensocket->UpdateConnectionsStatus();
			if (thePrefs.WatchClipboard4ED2KLinks()) {
				// TODO: Remove this from here. This has to be done with a clipboard chain
				// and *not* with a timer!!
				theApp.SearchClipboard();
			}

			if (theApp.serverconnect->IsConnecting())
				theApp.serverconnect->CheckForTimeout();

			// 2 seconds
			if (++i2sec >= 2) {
				i2sec = 0;

				// Update connection stats...
				theStats.UpdateConnectionStats(theApp.uploadqueue->GetDatarate() / 1024.0f, theApp.downloadqueue->GetDatarate() / 1024.0f);

#ifdef HAVE_WIN7_SDK_H
				if (thePrefs.IsWin7TaskbarGoodiesEnabled())
					theApp.emuledlg->UpdateStatusBarProgress();
#endif
			}

			// display graphs
			if (thePrefs.GetTrafficOMeterInterval() > 0 && ++igraph >= (uint32)thePrefs.GetTrafficOMeterInterval()) {
				igraph = 0;
				theApp.emuledlg->statisticswnd->SetCurrentRate(theApp.uploadqueue->GetDatarate() / 1024.0f, theApp.downloadqueue->GetDatarate() / 1024.0f);
			}
			if (theApp.emuledlg->activewnd == theApp.emuledlg->statisticswnd
				&& theApp.emuledlg->IsWindowVisible()
				&& thePrefs.GetStatsInterval() > 0	// display is on
				&& ++istats >= (uint32)thePrefs.GetStatsInterval())
			{
				istats = 0;
				theApp.emuledlg->statisticswnd->ShowStatistics();
			}

			theApp.uploadqueue->UpdateDatarates();

			//save rates every second
			theStats.RecordRate();

			if (theApp.emuledlg->IsTrayIconToFlash())
				theApp.emuledlg->ShowTransferRate(true);

			// *** 5 seconds **********************************************
			if (++i5sec >= 5) {
				i5sec = 0;
#ifdef _DEBUG
				if (thePrefs.m_iDbgHeap > 0 && !AfxCheckMemory())
					AfxDebugBreak();
#endif
				theApp.listensocket->Process();
				theApp.OnlineSig(); // Added By Bouc7
				if (!theApp.emuledlg->IsTrayIconToFlash())
					theApp.emuledlg->ShowTransferRate();

				// update cat-titles with downloads info only when needed
				if (thePrefs.ShowCatTabInfos()
					&& theApp.emuledlg->activewnd == theApp.emuledlg->transferwnd
					&& theApp.emuledlg->IsWindowVisible())
				{
					theApp.emuledlg->transferwnd->UpdateCatTabTitles(false);
				}

				if (thePrefs.IsSchedulerEnabled())
					theApp.scheduler->Check();

				theApp.emuledlg->transferwnd->UpdateListCount(CTransferWnd::wnd2Uploading, -1);
			}

			// *** 60 seconds *********************************************
			if (++i60sec >= 60) {
				i60sec = 0;

				if (thePrefs.GetWSIsEnabled())
					theApp.webserver->UpdateSessionCount();

				theApp.serverconnect->KeepConnectionAlive();

				if (thePrefs.GetPreventStandby())
					theApp.ResetStandByIdleTimer(); // Reset Windows idle standby timer if necessary
			}

			if (++s_uSaveStatistics >= thePrefs.GetStatsSaveInterval()) {
				s_uSaveStatistics = 0;
				thePrefs.SaveStats();
			}
		}

		// need more accuracy here; do not rely on the 'i5sec' and 'i60sec' helpers.
		thePerfLog.LogSamples();
	}
	CATCH_DFLT_EXCEPTIONS(_T("CUploadQueue::UploadTimer"))
}

void CUploadQueue::UpdateDatarates()
{
	// Calculate average data rate
	const ULONGLONG curTick = ::GetTickCount64();
	if (curTick < m_lastCalculatedDataRateTick + (SEC2MS(1) / 2))
		return;
	m_lastCalculatedDataRateTick = curTick;
	if (average_ur_hist.Count() > 1 && average_ur_hist.Tail().timestamp > average_ur_hist.Head().timestamp) {
		ULONGLONG duration = average_ur_hist.Tail().timestamp - average_ur_hist.Head().timestamp;
		datarate = (uint32)(SEC2MS(m_average_ur_sum - average_ur_hist.Head().upBytes) / duration);
		friendDatarate = (uint32)(SEC2MS(average_ur_hist.Tail().upFriendBytes - average_ur_hist.Head().upFriendBytes) / duration);
	}
}

uint32 CUploadQueue::GetToNetworkDatarate() const
{
	return (datarate > friendDatarate) ? datarate - friendDatarate : 0;
}

INT_PTR CUploadQueue::GetWaitingUserCount() const
{
	const std::shared_ptr<const WaitingQueueSnapshot> snapshot = GetWaitingSnapshot();
	return (snapshot != NULL) ? static_cast<INT_PTR>(snapshot->memberClients.size()) : 0;
}

uint32 CUploadQueue::GetWaitingUserForFileCount(const CSimpleArray<CObject*> &raFiles, bool bOnlyIfChanged)
{
	if (bOnlyIfChanged && !m_bStatisticsWaitingListDirty)
		return _UI32_MAX;

	m_bStatisticsWaitingListDirty = false;
	uint32 nResult = 0;
	const std::shared_ptr<const WaitingQueueSnapshot> snapshot = GetWaitingSnapshot();
	if (snapshot == NULL)
		return 0;
	for (const CUpDownClient *cur_client : snapshot->memberClients) {
		for (int i = raFiles.GetSize(); --i >= 0;)
			nResult += static_cast<uint32>(md4equ(static_cast<CKnownFile*>(raFiles[i])->GetFileHash(), cur_client->GetUploadFileID()));
	}
	return nResult;
}

uint32 CUploadQueue::GetDatarateForFile(const CSimpleArray<CObject*> &raFiles) const
{
	uint32 nResult = 0;
	const std::shared_ptr<const ActiveUploadSnapshot> snapshot = GetActiveUploadSnapshot();
	if (snapshot == NULL)
		return 0;
	for (const CUpDownClient *cur_client : snapshot->activeClients) {
		for (int i = raFiles.GetSize(); --i >= 0;)
			if (md4equ(static_cast<CKnownFile*>(raFiles[i])->GetFileHash(), cur_client->GetUploadFileID()))
				nResult += cur_client->GetUploadDatarate();
	}
	return nResult;
}

UploadSession::~UploadSession()
{
	blockListsLock.Lock();
	while (!blockRequests.IsEmpty())
		delete blockRequests.RemoveHead();

	while (!completedBlocks.IsEmpty())
		delete completedBlocks.RemoveHead();
	blockListsLock.Unlock();
}
