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
	: average_ur_hist(512, 512)
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
	, m_uploadSlotPhases()
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

void CUploadQueue::PurgeStaleWaitingClients(ULONGLONG curTick)
{
	for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL;) {
		const POSITION currentPos = pos;
		CUpDownClient *client = waitinglist.GetNext(pos);
		ASSERT(client->GetLastUpRequest());

		if (!ShouldPurgeWaitingClient(client, curTick))
			continue;

		client->ClearWaitStartTime();
		RemoveWaitingClientAt(currentPos, true);
	}
}

void CUploadQueue::BuildRankedWaitingClients(std::vector<CUpDownClient*> &clients) const
{
	clients.clear();
	clients.reserve(static_cast<size_t>(waitinglist.GetCount()));
	for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL;)
		clients.push_back(waitinglist.GetNext(pos));

	std::sort(clients.begin(), clients.end(),
		[this](const CUpDownClient *left, const CUpDownClient *right) {
			return CompareWaitingClientsByRank(left, right) < 0;
		});
}

void CUploadQueue::GetWaitingClientsInRankOrder(std::vector<CUpDownClient*> &clients)
{
	PurgeStaleWaitingClients(::GetTickCount64());
	BuildRankedWaitingClients(clients);
}

CUpDownClient* CUploadQueue::SelectNextWaitingClient()
{
	PurgeStaleWaitingClients(::GetTickCount64());
	std::vector<CUpDownClient*> rankedClients;
	BuildRankedWaitingClients(rankedClients);
	for (CUpDownClient *client : rankedClients) {
		if (IsClientEligibleForImmediateUpload(client))
			return client;
	}
	return NULL;
}

CUpDownClient* CUploadQueue::FindLowestPriorityWaitingClient(const CUpDownClient *excludeClient) const
{
	std::vector<CUpDownClient*> rankedClients;
	BuildRankedWaitingClients(rankedClients);
	for (auto it = rankedClients.rbegin(); it != rankedClients.rend(); ++it) {
		if (*it != excludeClient)
			return *it;
	}
	return NULL;
}

bool CUploadQueue::PassesQueueAdmissionLimit(const CUpDownClient *client) const
{
	const INT_PTR softQueueLimit = thePrefs.GetQueueSize();
	const INT_PTR hardQueueLimit = softQueueLimit + max(softQueueLimit, 800) / 4;
	if (waitinglist.GetCount() >= hardQueueLimit)
		return false;
	if (waitinglist.GetCount() < softQueueLimit)
		return true;

	const CUpDownClient *worstClient = FindLowestPriorityWaitingClient(client);
	return worstClient == NULL || IsHigherPriorityWaitingClient(client, worstClient);
}

bool CUploadQueue::HandleExistingQueueRequest(CUpDownClient *client)
{
	client->SendRankingInfo();
	theApp.emuledlg->transferwnd->GetQueueList()->RefreshClient(client);
	return true;
}

bool CUploadQueue::ResolveDuplicateQueueEntries(CUpDownClient *client, uint16 &cSameIP)
{
	for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		CUpDownClient *cur_client = waitinglist.GetNext(pos);
		if (cur_client == client)
			return HandleExistingQueueRequest(client);

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
				RemoveWaitingClientAt(pos2, true);
				if (!cur_client->socket && cur_client->Disconnected(_T("AddClientToQueue - same userhash 2")))
					delete cur_client;
				return true;
			}
			if (thePrefs.GetVerbose())
				AddDebugLogLine(false, (LPCTSTR)GetResString(IDS_SAMEUSERHASH), client->GetUserName(), cur_client->GetUserName(), cur_client->GetUserName());
			RemoveWaitingClientAt(pos2, true);
			if (!cur_client->socket && cur_client->Disconnected(_T("AddClientToQueue - same userhash 1")))
				delete cur_client;
		} else if (client->GetIP() == cur_client->GetIP()) {
			++cSameIP;
		}
	}
	return false;
}

CUploadQueue::EUploadSlotPhase CUploadQueue::GetUploadSlotPhase(const CUpDownClient *client) const
{
	const auto it = m_uploadSlotPhases.find(client);
	return (it != m_uploadSlotPhases.cend()) ? it->second : EUploadSlotPhase::None;
}

bool CUploadQueue::IsClientManagedByUploadQueue(const CUpDownClient *client) const
{
	return m_uploadSlotPhases.find(client) != m_uploadSlotPhases.cend();
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

void CUploadQueue::SetUploadSlotPhase(CUpDownClient *client, EUploadSlotPhase phase)
{
	if (client == NULL)
		return;

	if (phase == EUploadSlotPhase::None)
		m_uploadSlotPhases.erase(client);
	else
		m_uploadSlotPhases[client] = phase;

	SyncLegacyUploadState(client);
}

void CUploadQueue::SyncLegacyUploadState(CUpDownClient *client)
{
	if (client == NULL)
		return;

	switch (GetUploadSlotPhase(client)) {
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

void CUploadQueue::ClearUploadSlotPhase(CUpDownClient *client)
{
	m_uploadSlotPhases.erase(client);
	SyncLegacyUploadState(client);
}

void CUploadQueue::InsertInUploadingList(CUpDownClient *newclient, bool bNoLocking)
{
	UploadingToClient_Struct *pNewClientUploadStruct = new UploadingToClient_Struct;
	pNewClientUploadStruct->m_pClient = newclient;
	InsertInUploadingList(pNewClientUploadStruct, bNoLocking);
}

void CUploadQueue::InsertInUploadingList(UploadingToClient_Struct *pNewClientUploadStruct, bool bNoLocking)
{
	// Add it last
	theApp.uploadBandwidthThrottler->AddToStandardList(uploadinglist.GetCount(), pNewClientUploadStruct->m_pClient->GetFileUploadSocket());

	if (!bNoLocking)
		m_csUploadListMainThrdWriteOtherThrdsRead.Lock();
	uploadinglist.AddTail(pNewClientUploadStruct);
	if (!bNoLocking)
		m_csUploadListMainThrdWriteOtherThrdsRead.Unlock();

	pNewClientUploadStruct->m_pClient->SetSlotNumber((UINT)uploadinglist.GetCount());
}

bool CUploadQueue::ActivateUploadClient(CUpDownClient *newclient, LPCTSTR pszReason, bool bRemoveFromWaitingQueue)
{
	if (IsDownloading(newclient))
		return false;

	if (bRemoveFromWaitingQueue) {
		POSITION pos = waitinglist.Find(newclient);
		if (pos == NULL)
			return false;
		m_bStatisticsWaitingListDirty = true;
		waitinglist.RemoveAt(pos);
		theApp.emuledlg->transferwnd->GetQueueList()->RemoveClient(newclient);
		theApp.emuledlg->transferwnd->ShowQueueCount(waitinglist.GetCount());
	}

	if (pszReason && thePrefs.GetLogUlDlEvents())
		AddDebugLogLine(false, _T("Adding client to upload list: %s Client: %s"), pszReason, (LPCTSTR)newclient->DbgGetClientInfo());

	// tell the client that we are now ready to upload
	if (!newclient->socket || !newclient->socket->IsConnected() || !newclient->CheckHandshakeFinished()) {
		SetUploadSlotPhase(newclient, EUploadSlotPhase::Activating);
		if (!newclient->TryToConnect(true)) {
			ClearUploadSlotPhase(newclient);
			return false;
		}
	} else {
		if (!CompleteUploadActivation(newclient)) {
			ClearUploadSlotPhase(newclient);
			return false;
		}
	}
	newclient->SetUpStartTime();
	newclient->ResetSessionUp();
	newclient->ClearSlowUploadCooldown();
	newclient->ResetSlowUploadTracking();

	InsertInUploadingList(newclient, false);

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

	SetUploadSlotPhase(client, EUploadSlotPhase::Active);

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

	//if(thePrefs.GetLogUlDlEvents() && theApp.uploadBandwidthThrottler->GetStandardListSize() > uploadinglist.GetCount())
		// debug info, will remove this when I'm done.
	//	AddDebugLogLine(false, _T("UploadQueue: Error! Throttler has more slots than UploadQueue! Throttler: %i UploadQueue: %i Tick: %i"), theApp.uploadBandwidthThrottler->GetStandardListSize(), uploadinglist.GetCount(), ::GetTickCount64());

	tempHighest = min(tempHighest, uploadinglist.GetCount() + 1);
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
	snapshot.uploadSlots = uploadinglist.GetCount();
	snapshot.softMaxUploadSlots = GetSoftMaxUploadSlots();
	snapshot.highestFullyActivatedSlots = m_iHighestNumberOfFullyActivatedSlotsSinceLastCall;
	snapshot.budgetBytesPerSec = GetEffectiveUploadBudgetBytesPerSec();
	snapshot.toNetworkDatarate = GetToNetworkDatarate();
	snapshot.targetPerSlot = GetTargetClientDataRateBroadband();
	snapshot.hasWaitingClients = !waitinglist.IsEmpty();
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
	for (POSITION pos = uploadinglist.GetHeadPosition(); pos != NULL;) {
		// Get the client. Note! Also updates pos as a side effect.
		UploadingToClient_Struct *pCurClientStruct = uploadinglist.GetNext(pos);
		CUpDownClient *cur_client = pCurClientStruct->m_pClient;
		if (thePrefs.m_iDbgHeap >= 2)
			ASSERT_VALID(cur_client);
		//It seems chatting or friend slots can get stuck at times in upload. This needs to be looked into.
		if (cur_client->socket == NULL) {
			HandleUploadSlotTeardown(cur_client, _T("Uploading to client without socket? (CUploadQueue::Process)"));
			if (cur_client->Disconnected(_T("CUploadQueue::Process")))
				delete cur_client;
		} else {
			cur_client->UpdateUploadingStatisticsData();
			if (pCurClientStruct->m_bIOError) {
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
			CSingleLock lockBlockLists(&pCurClientStruct->m_csBlockListsLock, TRUE);
			ASSERT(lockBlockLists.IsLocked());
			// be careful what functions to call while having locks, RemoveFromUploadQueue could,
			// for example, lead to a deadlock here because it tries to get the uploadlist lock,
			// while the IO thread tries to fetch the uploadlist lock and then the blocklist lock
			if (!pCurClientStruct->m_BlockRequests_queue.IsEmpty()) {
				const Requested_Block_Struct *pHeadBlock = pCurClientStruct->m_BlockRequests_queue.GetHead();
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
	CUpDownClient *pMatchingIPClient = NULL;
	uint32 cMatches = 0;
	for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_client = waitinglist.GetNext(pos);
		if (dwIP == cur_client->GetIP() && nUDPPort == cur_client->GetUDPPort())
			return cur_client;
		if (bIgnorePortOnUniqueIP) {
			pMatchingIPClient = cur_client;
			++cMatches;
		}
	}
	if (pbMultipleIPs != NULL)
		*pbMultipleIPs = cMatches > 1;

	if (pMatchingIPClient != NULL && cMatches == 1)
		return pMatchingIPClient;
	return NULL;
}

CUpDownClient* CUploadQueue::GetWaitingClientByIP(uint32 dwIP) const
{
	for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *cur_client = waitinglist.GetNext(pos);
		if (dwIP == cur_client->GetIP())
			return cur_client;
	}
	return NULL;
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
	//This is to keep users from abusing the limits we put on lowID callbacks.
	//1)Check if we are connected to any network and that we are a lowID.
	//  (Although this check shouldn't matter as they wouldn't have found us.
	//  But, maybe I'm missing something, so it's best to check as a precaution.)
	//2)Check if the user is connected to Kad. We do allow all Kad Callbacks.
	//3)Check if the user is in our download list or a friend.
	//  We give these users a special pass as they are helping us.
	//4)Are we connected to a server? If we are, is the user on the same server?
	//  TCP lowID callbacks are also allowed.
	//5)If the queue is very short, allow anyone in as we want to make sure
	//  our upload is always used.
	if (   theApp.IsConnected()
		&& theApp.IsFirewalled()
		&& !client->GetKadPort()
		&& client->GetDownloadState() == DS_NONE
		&& !client->IsFriend()
		&& theApp.serverconnect
		&& !theApp.serverconnect->IsLocalServer(client->GetServerIP(), client->GetServerPort())
		&& GetWaitingUserCount() > 50)
	{
		return;
	}
	client->IncrementAskedCount();
	client->SetLastUpRequest();
	if (!bIgnoreTimelimit)
		client->AddRequestCount(client->GetUploadFileID());
	if (client->IsBanned())
		return;
	uint16 cSameIP = 0;
	if (ResolveDuplicateQueueEntries(client, cSameIP))
		return;
	if (cSameIP >= 3) {
		// do not accept more than 3 clients from the same IP
		if (thePrefs.GetVerbose())
			DEBUG_ONLY(AddDebugLogLine(false, _T("%s's (%s) request to enter the queue was rejected, because of too many clients with the same IP"), client->GetUserName(), (LPCTSTR)ipstr(client->GetConnectIP())));
		return;
	}
	if (theApp.clientlist->GetClientsFromIP(client->GetIP()) >= 3) {
		if (thePrefs.GetVerbose())
			DEBUG_ONLY(AddDebugLogLine(false, _T("%s's (%s) request to enter the queue was rejected, because of too many clients with the same IP (found in TrackedClientsList)"), client->GetUserName(), (LPCTSTR)ipstr(client->GetConnectIP())));
		return;
	}
	// done

	// statistic values
	// TODO: Maybe we should change this to count each request for a file only once and ignore re-asks
	CKnownFile *reqfile = theApp.sharedfiles->GetFileByID((uchar*)client->GetUploadFileID());
	if (reqfile)
		reqfile->statistic.AddRequest();

	ClearUploadSlotPhase(client);

	if (!PassesQueueAdmissionLimit(client))
		return;
	if (IsClientUploadActive(client)) {
		// he's already downloading and probably only wants another file
		if (thePrefs.GetDebugClientTCPLevel() > 0)
			DebugSend("OP_AcceptUploadReq", client);
		Packet *packet = new Packet(OP_ACCEPTUPLOADREQ, 0);
		theStats.AddUpDataOverheadFileRequest(packet->size);
		client->SendPacket(packet);
		return;
	}
	if (client->IsInSlowUploadCooldown()) {
		AddClientToWaitingList(client);
		return;
	}
	const UploadSchedulingSnapshot snapshot = CaptureSchedulingSnapshot(m_bThrottlerWantsMoreSlotsHint);
	if (waitinglist.IsEmpty() && ShouldOpenUploadSlot(snapshot, true)) {
		client->SetWaitStartTime();
		ActivateUploadClient(client, _T("Direct add with empty queue."), false);
	} else {
		AddClientToWaitingList(client);
	}
}

void CUploadQueue::AddClientToWaitingList(CUpDownClient *client)
{
	client->SetWaitStartTime();
	client->SetAskedCount(1);
	SetUploadSlotPhase(client, EUploadSlotPhase::Waiting);
	m_bStatisticsWaitingListDirty = true;
	waitinglist.AddTail(client);
	theApp.emuledlg->transferwnd->GetQueueList()->AddClient(client);
	theApp.emuledlg->transferwnd->ShowQueueCount(waitinglist.GetCount());
	client->SendRankingInfo();
}

bool CUploadQueue::HandleUploadSlotTeardown(CUpDownClient *client, LPCTSTR pszReason, bool removeWaiting, bool updatewindow, bool earlyabort)
{
	const bool removedUpload = RemoveActiveUploadSlot(client, pszReason, updatewindow, earlyabort);
	const bool removedWaiting = removeWaiting ? RemoveWaitingClient(client, updatewindow) : false;
	if (!removedUpload && !removedWaiting && removeWaiting)
		ClearUploadSlotPhase(client);
	return removedUpload || removedWaiting;
}

bool CUploadQueue::RemoveActiveUploadSlot(CUpDownClient *client, LPCTSTR pszReason, bool updatewindow, bool earlyabort)
{
	SetUploadSlotPhase(client, EUploadSlotPhase::Retiring);
	bool result = false;
	uint32 slotCounter = 1;
	for (POSITION pos = uploadinglist.GetHeadPosition(); pos != NULL;) {
		POSITION curPos = pos;
		UploadingToClient_Struct *curClientStruct = uploadinglist.GetNext(pos);
		if (client == curClientStruct->m_pClient) {
			if (updatewindow)
				theApp.emuledlg->transferwnd->GetUploadList()->RemoveClient(client);

			if (thePrefs.GetLogUlDlEvents()) {
				AddDebugLogLine(DLP_DEFAULT, true, _T("Removing client from upload list: %s Client: %s Transferred: %s SessionUp: %s QueueSessionPayload: %s In buffer: %s Req blocks: %i File: %s")
					, pszReason == NULL ? _T("") : pszReason
					, (LPCTSTR)client->DbgGetClientInfo()
					, (LPCTSTR)CastSecondsToHM(client->GetUpStartTimeDelay() / SEC2MS(1))
					, (LPCTSTR)CastItoXBytes(client->GetSessionUp())
					, (LPCTSTR)CastItoXBytes(client->GetQueueSessionPayloadUp())
					, (LPCTSTR)CastItoXBytes(client->GetPayloadInBuffer()), curClientStruct->m_BlockRequests_queue.GetCount()
					, theApp.sharedfiles->GetFileByID(client->GetUploadFileID()) ? (LPCTSTR)theApp.sharedfiles->GetFileByID(client->GetUploadFileID())->GetFileName() : _T(""));
			}
			m_csUploadListMainThrdWriteOtherThrdsRead.Lock();
			uploadinglist.RemoveAt(curPos);
			m_csUploadListMainThrdWriteOtherThrdsRead.Unlock();
			delete curClientStruct; // m_csBlockListsLock.Lock();

			//if (thePrefs.GetLogUlDlEvents() && !theApp.uploadBandwidthThrottler->RemoveFromStandardList(client->socket))
			//	AddDebugLogLine(false, _T("UploadQueue: Didn't find socket to delete. Address: 0x%x"), client->socket);
			theApp.uploadBandwidthThrottler->RemoveFromStandardList(client->socket);

			if (client->GetSessionUp() > 0) {
				++successfullupcount;
				totaluploadtime += client->GetUpStartTimeDelay() / SEC2MS(1);
			} else
				failedupcount += static_cast<uint32>(!earlyabort);

			CKnownFile *requestedFile = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
			if (requestedFile != NULL)
				requestedFile->UpdatePartsInfo();

			theApp.clientlist->AddTrackClient(client); // Keep track of this client
			ClearUploadSlotPhase(client);

			m_iHighestNumberOfFullyActivatedSlotsSinceLastCall = 0;

			result = true;
		} else {
			curClientStruct->m_pClient->SetSlotNumber(slotCounter);
			++slotCounter;
		}
	}
	if (!result)
		ClearUploadSlotPhase(client);
	return result;
}

uint32 CUploadQueue::GetAverageUpTime() const
{
	return successfullupcount ? (totaluploadtime / successfullupcount) : 0;
}

bool CUploadQueue::RemoveWaitingClient(CUpDownClient *client, bool updatewindow)
{
	POSITION pos = waitinglist.Find(client);
	if (pos) {
		RemoveWaitingClientAt(pos, updatewindow);
		return true;
	}
	return false;
}

void CUploadQueue::RemoveWaitingClientAt(POSITION pos, bool updatewindow)
{
	m_bStatisticsWaitingListDirty = true;
	CUpDownClient *todelete = waitinglist.GetAt(pos);
	waitinglist.RemoveAt(pos);
	if (updatewindow) {
		theApp.emuledlg->transferwnd->GetQueueList()->RemoveClient(todelete);
		theApp.emuledlg->transferwnd->ShowQueueCount(waitinglist.GetCount());
	}
	ClearUploadSlotPhase(todelete);
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
		client->SendOutOfPartReqsAndAddToWaitingQueue();

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
	for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL;) {
		CUpDownClient *client = waitinglist.GetNext(pos);
		ClearUploadSlotPhase(client);
	}
	waitinglist.RemoveAll();
	m_csUploadListMainThrdWriteOtherThrdsRead.Lock();
	while (!uploadinglist.IsEmpty()) {
		UploadingToClient_Struct *uploading = uploadinglist.RemoveHead();
		ClearUploadSlotPhase(uploading->m_pClient);
		delete uploading;
	}
	m_csUploadListMainThrdWriteOtherThrdsRead.Unlock();
	m_uploadSlotPhases.clear();
	// PENDING: Remove from UploadBandwidthThrottler as well!
}

UINT CUploadQueue::GetWaitingPosition(const CUpDownClient *client)
{
	if (!IsClientWaitingForUpload(client))
		return 0;
	std::vector<CUpDownClient*> rankedClients;
	GetWaitingClientsInRankOrder(rankedClients);
	for (size_t i = 0; i < rankedClients.size(); ++i) {
		if (rankedClients[i] == client)
			return static_cast<UINT>(i + 1);
	}
	return 0;
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

uint32 CUploadQueue::GetWaitingUserForFileCount(const CSimpleArray<CObject*> &raFiles, bool bOnlyIfChanged)
{
	if (bOnlyIfChanged && !m_bStatisticsWaitingListDirty)
		return _UI32_MAX;

	m_bStatisticsWaitingListDirty = false;
	uint32 nResult = 0;
	for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL;) {
		const CUpDownClient *cur_client = waitinglist.GetNext(pos);
		for (int i = raFiles.GetSize(); --i >= 0;)
			nResult += static_cast<uint32>(md4equ(static_cast<CKnownFile*>(raFiles[i])->GetFileHash(), cur_client->GetUploadFileID()));
	}
	return nResult;
}

uint32 CUploadQueue::GetDatarateForFile(const CSimpleArray<CObject*> &raFiles) const
{
	uint32 nResult = 0;
	for (POSITION pos = uploadinglist.GetHeadPosition(); pos != NULL;) {
		const CUpDownClient *cur_client = uploadinglist.GetNext(pos)->m_pClient;
		for (int i = raFiles.GetSize(); --i >= 0;)
			if (md4equ(static_cast<CKnownFile*>(raFiles[i])->GetFileHash(), cur_client->GetUploadFileID()))
				nResult += cur_client->GetUploadDatarate();
	}
	return nResult;
}

const CUploadingPtrList& CUploadQueue::GetUploadListTS(CCriticalSection **outUploadListReadLock)
{
	ASSERT(*outUploadListReadLock == NULL);
	*outUploadListReadLock = &m_csUploadListMainThrdWriteOtherThrdsRead;
	return uploadinglist;
}

UploadingToClient_Struct* CUploadQueue::GetUploadingClientStructByClient(const CUpDownClient *pClient) const
{
	//TODO: Check if this function is too slow for its usage (esp. when rendering the GUI bars)
	//		if necessary we will have to speed it up with an additional map
	for (POSITION pos = uploadinglist.GetHeadPosition(); pos != NULL;) {
		UploadingToClient_Struct *pCurClientStruct = uploadinglist.GetNext(pos);
		if (pCurClientStruct->m_pClient == pClient)
			return pCurClientStruct;
	}
	return NULL;
}

UploadingToClient_Struct::~UploadingToClient_Struct()
{
	m_pClient->FlushSendBlocks();

	m_csBlockListsLock.Lock();
	while (!m_BlockRequests_queue.IsEmpty())
		delete m_BlockRequests_queue.RemoveHead();

	while (!m_DoneBlocks_list.IsEmpty())
		delete m_DoneBlocks_list.RemoveHead();
	m_csBlockListsLock.Unlock();
}
