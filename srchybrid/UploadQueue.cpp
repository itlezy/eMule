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
#include "LastCommonRouteFinder.h"
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
constexpr DWORD UPLOAD_OVERFLOW_GRACE_TIME = SEC2MS(2);
constexpr DWORD UPLOAD_SLOW_EVICT_TIME = SEC2MS(15);
constexpr DWORD UPLOAD_ZERO_RATE_EVICT_TIME = SEC2MS(3);
// Modern fixed floor for the legacy no-budget upload-slot heuristic.
// 32 KiB/s corresponds to a 256 kbit/s upstream assumption for a minimally viable slot.
constexpr uint32 MIN_FALLBACK_UPLOAD_CLIENT_DATARATE = 32u * 1024u;

uint64 ResolveBBSessionMaxTransferLimit(uint64 configuredLimit, const CKnownFile *pUploadingFile)
{
	if (configuredLimit == 0)
		return 0;
	if (configuredLimit <= 100) {
		if (pUploadingFile == NULL)
			return 0;
		// Percent mode is based on the full file size and rounded up so non-zero values stay meaningful.
		return max(1ui64, ((uint64)pUploadingFile->GetFileSize() * configuredLimit + 99ui64) / 100ui64);
	}
	return configuredLimit;
}
}

struct CUploadQueue::BroadbandControlState
{
	bool	hasBudget;
	uint32	effectiveBudgetBytesPerSec;
	uint32	softMaxSlots;
	uint32	targetPerSlotBytesPerSec;
	uint32	underfillHeadroomBytesPerSec;
	uint32	overflowAllowance;
	uint32	slowRateThresholdBytesPerSec;
	bool	isUnderfilled;
};

CUploadQueue::CUploadQueue()
	: average_ur_hist(512, 512)
	, activeClients_hist(512, 512)
	, datarate()
	, friendDatarate()
	, successfullupcount()
	, failedupcount()
	, totaluploadtime()
	, m_nLastStartUpload()
	, m_dwLastCalculatedAverageCombinedFilePrioAndCredit()
	, m_fAverageCombinedFilePrioAndCredit()
	, m_iHighestNumberOfFullyActivatedSlotsSinceLastCall()
	, m_MaxActiveClients()
	, m_MaxActiveClientsShortTime()
	, m_average_ur_sum()
	, m_lastCalculatedDataRateTick()
	, m_dwUnderfillStartTick()
	, m_bNeedMoreBandwidthSlots()
	, m_dwLastResortedUploadSlots()
	, m_bStatisticsWaitingListDirty(true)
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
 * A low id client that is not connected, but would have been ranked highest if it
 * had been connected, gets a flag set. This flag means that the client should be
 * allowed to get an upload slot immediately once it connects.
 *
 * @return address of the highest ranking client.
 */
CUpDownClient* CUploadQueue::FindBestClientInQueue()
{
	uint32 bestscore = 0;
	uint32 bestlowscore = 0;
	CUpDownClient *newclient = NULL;
	CUpDownClient *lowclient = NULL;
	const DWORD curTick = ::GetTickCount();

	for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		CUpDownClient *cur_client = waitinglist.GetNext(pos);
		//While we are going through this list. Lets check if a client appears to have left the network.
		ASSERT(cur_client->GetLastUpRequest());
		if ((curTick >= cur_client->GetLastUpRequest() + MAX_PURGEQUEUETIME) || !theApp.sharedfiles->GetFileByID(cur_client->GetUploadFileID())) {
			//This client has either not been seen in a long time, or we no longer share the file he wanted any more.
			cur_client->ClearWaitStartTime();
			RemoveFromWaitingQueue(pos2, true);
		} else {
			// finished clearing
			uint32 cur_score = cur_client->GetScore(false);

			if (cur_score > bestscore) {
				// cur_client is more worthy than current best client that is ready to go (connected).
				if (!cur_client->HasLowID() || (cur_client->socket && cur_client->socket->IsConnected())) {
					// this client is a HighID or a lowID client that is ready to go (connected)
					// and it is more worthy
					bestscore = cur_score;
					newclient = cur_client;
				} else if (!cur_client->m_bAddNextConnect) {
					// this client is a lowID client that is not ready to go (not connected)

					// now that we know this client is not ready to go, compare it to the best not ready client
					// the best not ready client may be better than the best ready client, so we need to check
					// against that client
					if (cur_score > bestlowscore) {
						// it is more worthy, keep it
						bestlowscore = cur_score;
						lowclient = cur_client;
					}
				}
			}
		}
	}

	if (lowclient && bestlowscore > bestscore)
		lowclient->m_bAddNextConnect = true;

	return newclient;
}

void CUploadQueue::InsertInUploadingList(CUpDownClient *newclient, bool bNoLocking)
{
	UploadingToClient_Struct *pNewClientUploadStruct = new UploadingToClient_Struct;
	pNewClientUploadStruct->m_pClient = newclient;
	InsertInUploadingList(pNewClientUploadStruct, bNoLocking);
}

void CUploadQueue::InsertInUploadingList(UploadingToClient_Struct *pNewClientUploadStruct, bool bNoLocking)
{
	//Lets make sure any client that is added to the list has this flag reset!
	pNewClientUploadStruct->m_pClient->m_bAddNextConnect = false;
	// Add it last
	theApp.uploadBandwidthThrottler->AddToStandardList(uploadinglist.GetCount(), pNewClientUploadStruct->m_pClient->GetFileUploadSocket());

	if (!bNoLocking)
		m_csUploadListMainThrdWriteOtherThrdsRead.Lock();
	uploadinglist.AddTail(pNewClientUploadStruct);
	if (!bNoLocking)
		m_csUploadListMainThrdWriteOtherThrdsRead.Unlock();

	pNewClientUploadStruct->m_pClient->SetSlotNumber((UINT)uploadinglist.GetCount());
}

bool CUploadQueue::AddUpNextClient(LPCTSTR pszReason, CUpDownClient *directadd)
{
	CUpDownClient *newclient = directadd;
	// select next client or use given client
	if (newclient == NULL) {
		newclient = FindBestClientInQueue();
		if (newclient == NULL)
			return false;
	}
	RemoveFromWaitingQueue(newclient, true);
	theApp.emuledlg->transferwnd->ShowQueueCount(waitinglist.GetCount());

	if (IsDownloading(newclient))
		return false;

	if (pszReason && thePrefs.GetLogUlDlEvents())
		AddDebugLogLine(false, _T("Adding client to upload list: %s Client: %s"), pszReason, (LPCTSTR)newclient->DbgGetClientInfo());

	newclient->ResetSlowUploadTracking();
	newclient->ClearSlowUploadCooldown();

	if (newclient->HasCollectionUploadSlot() && directadd == NULL) {
		ASSERT(0);
		newclient->SetCollectionUploadSlot(false);
	}

	// tell the client that we are now ready to upload
	if (!newclient->socket || !newclient->socket->IsConnected() || !newclient->CheckHandshakeFinished()) {
		newclient->SetUploadState(US_CONNECTING);
		if (!newclient->TryToConnect(true))
			return false;
	} else {
		if (thePrefs.GetDebugClientTCPLevel() > 0)
			DebugSend("OP_AcceptUploadReq", newclient);
		Packet *packet = new Packet(OP_ACCEPTUPLOADREQ, 0);
		theStats.AddUpDataOverheadFileRequest(packet->size);
		newclient->SendPacket(packet);
		newclient->SetUploadState(US_UPLOADING);
	}
	newclient->SetUpStartTime();
	newclient->ResetSessionUp();

	InsertInUploadingList(newclient, false);

	m_nLastStartUpload = ::GetTickCount();

	// statistic
	CKnownFile *reqfile = theApp.sharedfiles->GetFileByID((uchar*)newclient->GetUploadFileID());
	if (reqfile)
		reqfile->statistic.AddAccepted();

	theApp.emuledlg->transferwnd->GetUploadList()->AddClient(newclient);

	return true;
}

void CUploadQueue::UpdateActiveClientsInfo(DWORD curTick)
{
	// Save number of active clients for statistics
	INT_PTR tempHighest = theApp.uploadBandwidthThrottler->GetHighestNumberOfFullyActivatedSlotsSinceLastCallAndReset();
	m_bNeedMoreBandwidthSlots = theApp.uploadBandwidthThrottler->GetNeedsMoreBandwidthSlotsSinceLastCallAndReset();

	//if(thePrefs.GetLogUlDlEvents() && theApp.uploadBandwidthThrottler->GetStandardListSize() > uploadinglist.GetCount())
		// debug info, will remove this when I'm done.
	//	AddDebugLogLine(false, _T("UploadQueue: Error! Throttler has more slots than UploadQueue! Throttler: %i UploadQueue: %i Tick: %i"), theApp.uploadBandwidthThrottler->GetStandardListSize(), uploadinglist.GetCount(), ::GetTickCount());

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

// Only treat the upload budget as valid when it comes from an explicit capacity or live USS allowance.
bool CUploadQueue::HasEffectiveUploadBudget() const
{
	if (thePrefs.GetMaxGraphUploadRate(false) != UNLIMITED)
		return true;
	return thePrefs.IsDynUpEnabled() && theApp.lastCommonRouteFinder->GetUpload() > 0;
}

// Base all broadband slot decisions on the effective upload budget: capacity, user limit and USS allowance.
uint32 CUploadQueue::GetEffectiveUploadBudget() const
{
	uint32 budget = thePrefs.GetMaxGraphUploadRate(true);
	const uint32 maxUpload = thePrefs.GetMaxUpload();
	if (maxUpload != UNLIMITED)
		budget = min(budget, maxUpload);
	if (thePrefs.IsDynUpEnabled())
		budget = min(budget, theApp.lastCommonRouteFinder->GetUpload() / 1024u);
	return max(1u, budget);
}

uint32 CUploadQueue::GetSoftMaxUploadSlots() const
{
	return max((uint32)MIN_UP_CLIENTS_ALLOWED, min((uint32)MAX_UP_CLIENTS_ALLOWED, thePrefs.GetMaxUpClientsAllowed()));
}

// Derive the full broadband controller state from the slot target and the current budget.
CUploadQueue::BroadbandControlState CUploadQueue::GetBroadbandControlState() const
{
	BroadbandControlState state = {};
	state.softMaxSlots = GetSoftMaxUploadSlots();
	state.hasBudget = HasEffectiveUploadBudget();
	if (!state.hasBudget)
		return state;

	state.effectiveBudgetBytesPerSec = GetEffectiveUploadBudget() * 1024u;
	state.targetPerSlotBytesPerSec = max(3u * 1024u, state.effectiveBudgetBytesPerSec / max(1u, state.softMaxSlots));
	state.underfillHeadroomBytesPerSec = max(state.targetPerSlotBytesPerSec / 2u, state.effectiveBudgetBytesPerSec / 20u);
	state.overflowAllowance = max(1u, (state.softMaxSlots + 5u) / 6u);
	state.slowRateThresholdBytesPerSec = max(3u * 1024u, state.targetPerSlotBytesPerSec / 3u);
	state.isUnderfilled = (uint64)datarate + state.underfillHeadroomBytesPerSec < state.effectiveBudgetBytesPerSec;
	return state;
}

// Allow only a small overflow above the steady-state slot target, and only after sustained underfill.
bool CUploadQueue::AllowTemporaryUploadOverflow() const
{
	return GetBroadbandControlState().hasBudget
		&& m_dwUnderfillStartTick != 0
		&& ::GetTickCount() >= m_dwUnderfillStartTick + UPLOAD_OVERFLOW_GRACE_TIME;
}

uint32 CUploadQueue::GetUploadSlotLimit() const
{
	const BroadbandControlState state = GetBroadbandControlState();
	uint32 slotLimit = state.softMaxSlots;
	if (AllowTemporaryUploadOverflow())
		slotLimit = min((uint32)MAX_UP_CLIENTS_ALLOWED, slotLimit + state.overflowAllowance);
	return slotLimit;
}

uint32 CUploadQueue::GetSlowUploadRateThreshold() const
{
	const BroadbandControlState state = GetBroadbandControlState();
	return state.hasBudget ? state.slowRateThresholdBytesPerSec : max(3u * 1024u, GetTargetClientDataRate(false) / 3u);
}

// Slow-slot tracking is only meaningful when we are already at the target slot count and still underfilling the line.
bool CUploadQueue::ShouldTrackSlowUploadSlots() const
{
	const BroadbandControlState state = GetBroadbandControlState();
	return state.hasBudget
		&& !waitinglist.IsEmpty()
		&& uploadinglist.GetCount() >= (INT_PTR)state.softMaxSlots
		&& state.isUnderfilled;
}

// Keep a short underfill timer so the throttler can ask for at most a couple of temporary overflow slots.
void CUploadQueue::UpdateSlotCapacityState(DWORD curTick)
{
	if (ShouldTrackSlowUploadSlots()
		&& m_bNeedMoreBandwidthSlots
		&& uploadinglist.GetCount() < (INT_PTR)MAX_UP_CLIENTS_ALLOWED)
	{
		if (m_dwUnderfillStartTick == 0)
			m_dwUnderfillStartTick = curTick;
	}
	else
		m_dwUnderfillStartTick = 0;
}

bool CUploadQueue::ShouldOpenMoreUploadSlots(INT_PTR curUploadSlots) const
{
	if (curUploadSlots < max(MIN_UP_CLIENTS_ALLOWED, 4))
		return true;
	if (curUploadSlots >= MAX_UP_CLIENTS_ALLOWED)
		return false;

	const BroadbandControlState state = GetBroadbandControlState();
	if (!state.hasBudget)
		return (uint32)curUploadSlots < GetUploadSlotLimit();

	// Broadband mode grows slots only while there is still measurable budget headroom.
	return state.isUnderfilled && (uint32)curUploadSlots < GetUploadSlotLimit();
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
	const DWORD curTick = ::GetTickCount();
	UpdateActiveClientsInfo(curTick);
	UpdateSlotCapacityState(curTick);

	if (ForceNewClient())
		// There's not enough open uploads. Open another one.
		AddUpNextClient(_T("Not enough open upload slots for the current speed"));

	// The loop that feeds the upload slots with data.
	for (POSITION pos = uploadinglist.GetHeadPosition(); pos != NULL;) {
		// Get the client. Note! Also updates pos as a side effect.
		UploadingToClient_Struct *pCurClientStruct = uploadinglist.GetNext(pos);
		CUpDownClient *cur_client = pCurClientStruct->m_pClient;
		if (thePrefs.m_iDbgHeap >= 2)
			ASSERT_VALID(cur_client);
		//It seems chatting or friend slots can get stuck at times in upload. This needs to be looked into.
		if (cur_client->socket == NULL) {
			RemoveFromUploadQueue(cur_client, _T("Uploading to client without socket? (CUploadQueue::Process)"));
			if (cur_client->Disconnected(_T("CUploadQueue::Process")))
				delete cur_client;
		} else {
			cur_client->UpdateUploadingStatisticsData();
			if (pCurClientStruct->m_bIOError) {
				RemoveFromUploadQueue(cur_client, _T("IO/Other Error while creating data packet (see earlier log entries)"), true);
				continue;
			}
			if (CheckForTimeOver(cur_client)) {
				RemoveFromUploadQueue(cur_client, _T("Completed transfer"), true);
				cur_client->SendOutOfPartReqsAndAddToWaitingQueue();
				continue;
			}
			// Scale the large send-buffer trigger from the current slot target instead of a fixed legacy rate.
			const uint32 bigSendBufferThreshold = max(32u * 1024u, GetTargetClientDataRate(false) / 2u);
			if (cur_client->GetUploadDatarate() >= bigSendBufferThreshold) {
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
						RemoveFromUploadQueue(cur_client, _T("Requested FileID in block request not found in shared files"), true);
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

bool CUploadQueue::AcceptNewClient(bool addOnNextConnect) const
{
	INT_PTR curUploadSlots = uploadinglist.GetCount();

	//We allow ONE extra slot to be created to accommodate lowID users.
	//This is because we skip these users when it was actually their turn
	//to get an upload slot.
	curUploadSlots -= static_cast<INT_PTR>(addOnNextConnect && curUploadSlots > 0);

	return AcceptNewClient(curUploadSlots);
}

// check if we can allow a new client to start downloading from us
bool CUploadQueue::AcceptNewClient(INT_PTR curUploadSlots) const
{
	return ShouldOpenMoreUploadSlots(curUploadSlots);
}

/**
 * Return the expected upload rate for one healthy upload slot.
 *
 * This is a heuristic reference value for upload-queue decisions, not the actual
 * socket throttle. The throttler still enforces the real upload budget separately.
 *
 * Current consumers use this value to decide:
 * - when an upload slot is too slow and should be replaced
 * - when to enable the large TCP send buffer
 * - how aggressively upload data should be prefetched from disk
 *
 * There are two paths:
 * - broadband-budget path: derive the slot target from the current effective upload budget
 * - legacy fallback path: keep a simple slot-count-based estimate when no budget exists
 */
uint32 CUploadQueue::GetTargetClientDataRate(bool bMinDatarate) const
{
	if (!HasEffectiveUploadBudget()) {
		// Legacy fallback path used only when no explicit upload budget is available.
		// Keep the old "1 KiB/s per open slot" shape for compatibility with the surrounding
		// queue heuristics, but clamp it to a modern fixed floor and ceiling so the queue
		// no longer assumes dial-up-era upload slot speeds.
		uint32 nOpenSlots = (uint32)GetUploadQueueLength();
		uint32 nResult = max(MIN_FALLBACK_UPLOAD_CLIENT_DATARATE, nOpenSlots * 1024u);
		// The cap is still a heuristic ceiling only. It prevents the fallback path from
		// extrapolating unrealistic per-slot targets when the slot count becomes very large.
		nResult = min(UPLOAD_CLIENT_MAXDATARATE, nResult);
		return bMinDatarate ? nResult * 3 / 4 : nResult;
	}

	const BroadbandControlState state = GetBroadbandControlState();
	const uint32 nResult = state.targetPerSlotBytesPerSec;
	return bMinDatarate ? nResult * 3 / 4 : nResult;
}

bool CUploadQueue::ForceNewClient(bool allowEmptyWaitingQueue)
{
	if (!allowEmptyWaitingQueue && waitinglist.IsEmpty())
		return false;

	INT_PTR curUploadSlots = uploadinglist.GetCount();
	if (curUploadSlots < MIN_UP_CLIENTS_ALLOWED)
		return true;

	if (::GetTickCount() < m_nLastStartUpload + SEC2MS(1) && datarate < 102400)
		return false;

	if (!theApp.lastCommonRouteFinder->AcceptNewClient()) // UploadSpeedSense can veto a new slot if USS enabled
		return false;

	return ShouldOpenMoreUploadSlots(curUploadSlots);
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
	// check for duplicates
	for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		CUpDownClient *cur_client = waitinglist.GetNext(pos);
		if (cur_client == client) {
			if (client->m_bAddNextConnect && !client->IsInSlowUploadCooldown() && AcceptNewClient(client->m_bAddNextConnect)) {
				//Special care is given to lowID clients that missed their upload slot
				//due to the saving bandwidth on callbacks.
				if (thePrefs.GetLogUlDlEvents())
					AddDebugLogLine(true, _T("Adding ****lowid when reconnecting. Client: %s"), (LPCTSTR)client->DbgGetClientInfo());
				client->m_bAddNextConnect = false;
				RemoveFromWaitingQueue(client, true);
				// statistic values // TODO: Maybe we should change this to count each request for a file only once and ignore re-asks
				CKnownFile *reqfile = theApp.sharedfiles->GetFileByID((uchar*)client->GetUploadFileID());
				if (reqfile)
					reqfile->statistic.AddRequest();
				AddUpNextClient(_T("Adding ****lowid when reconnecting."), client);
			} else {
				client->SendRankingInfo();
				theApp.emuledlg->transferwnd->GetQueueList()->RefreshClient(client);
			}
			return;
		}
		if (client->Compare(cur_client)) {
			theApp.clientlist->AddTrackClient(client); // in any case keep track of this client

			// another client with same ip:port or hash
			// this happens only in rare cases, because same userhash / ip:ports are assigned to the right client on connecting in most cases
			if (cur_client->credits != NULL && cur_client->credits->GetCurrentIdentState(cur_client->GetIP()) == IS_IDENTIFIED) {
				//cur_client has a valid secure hash, don't remove him
				if (thePrefs.GetVerbose())
					AddDebugLogLine(false, (LPCTSTR)GetResString(IDS_SAMEUSERHASH), client->GetUserName(), cur_client->GetUserName(), client->GetUserName());
				return;
			}
			if (client->credits == NULL || client->credits->GetCurrentIdentState(client->GetIP()) != IS_IDENTIFIED) {
				// remove both since we do not know who the bad one is
				if (thePrefs.GetVerbose())
					AddDebugLogLine(false, (LPCTSTR)GetResString(IDS_SAMEUSERHASH), client->GetUserName(), cur_client->GetUserName(), _T("Both"));
				RemoveFromWaitingQueue(pos2, true);
				if (!cur_client->socket && cur_client->Disconnected(_T("AddClientToQueue - same userhash 2")))
					delete cur_client;
				return;
			}
			//client has a valid secure hash, add him and remove the other one
			if (thePrefs.GetVerbose())
				AddDebugLogLine(false, (LPCTSTR)GetResString(IDS_SAMEUSERHASH), client->GetUserName(), cur_client->GetUserName(), cur_client->GetUserName());
			RemoveFromWaitingQueue(pos2, true);
			if (!cur_client->socket && cur_client->Disconnected(_T("AddClientToQueue - same userhash 1")))
				delete cur_client;
		} else if (client->GetIP() == cur_client->GetIP()) {
			// same IP, different port, different userhash
			++cSameIP;
		}
	}
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

	// emule collection will bypass the queue
	if (reqfile != NULL && CCollection::HasCollectionExtention(reqfile->GetFileName()) && reqfile->GetFileSize() < (uint64)MAXPRIORITYCOLL_SIZE
		&& !client->IsDownloading() && client->socket != NULL && client->socket->IsConnected())
	{
		client->SetCollectionUploadSlot(true);
		RemoveFromWaitingQueue(client, true);
		AddUpNextClient(_T("Collection Priority Slot"), client);
		return;
	}

	client->SetCollectionUploadSlot(false);

	// cap the list
	// the queue limit in prefs is only a soft limit. Hard limit is higher up to 25% to accept
	// powershare clients and other high ranking clients after soft limit has been reached
	INT_PTR softQueueLimit = thePrefs.GetQueueSize();
	INT_PTR hardQueueLimit = softQueueLimit + max(softQueueLimit, 800) / 4;

	// if soft queue limit has been reached, only let in high ranking clients
	if (waitinglist.GetCount() >= hardQueueLimit
		|| (waitinglist.GetCount() >= softQueueLimit // soft queue limit is reached
			&& (!client->IsFriend() || !client->GetFriendSlot()) // client is not a friend with friend slot
			&& client->GetCombinedFilePrioAndCredit() < GetAverageCombinedFilePrioAndCredit() // and client has lower credits/wants lower prio file than average client in queue
		   )
	   )
	{
		// block client from getting on queue
		return;
	}
	if (client->IsDownloading()) {
		// he's already downloading and probably only wants another file
		if (thePrefs.GetDebugClientTCPLevel() > 0)
			DebugSend("OP_AcceptUploadReq", client);
		Packet *packet = new Packet(OP_ACCEPTUPLOADREQ, 0);
		theStats.AddUpDataOverheadFileRequest(packet->size);
		client->SendPacket(packet);
		return;
	}
	if (waitinglist.IsEmpty() && !client->IsInSlowUploadCooldown() && ForceNewClient(true)) {
		client->SetWaitStartTime();
		AddUpNextClient(_T("Direct add with empty queue."), client);
	} else {
		m_bStatisticsWaitingListDirty = true;
		waitinglist.AddTail(client);
		client->SetUploadState(US_ONUPLOADQUEUE);
		theApp.emuledlg->transferwnd->GetQueueList()->AddClient(client, true);
		theApp.emuledlg->transferwnd->ShowQueueCount(waitinglist.GetCount());
		client->SendRankingInfo();
	}
}

float CUploadQueue::GetAverageCombinedFilePrioAndCredit()
{
	const DWORD curTick = ::GetTickCount();

	if (curTick >= m_dwLastCalculatedAverageCombinedFilePrioAndCredit + SEC2MS(5)) {
		m_dwLastCalculatedAverageCombinedFilePrioAndCredit = curTick;

		// TODO: is there a risk of overflow? I don't think so...
		float sum = 0;
		for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL;)
			sum += waitinglist.GetNext(pos)->GetCombinedFilePrioAndCredit();

		m_fAverageCombinedFilePrioAndCredit = sum / waitinglist.GetCount();
	}

	return m_fAverageCombinedFilePrioAndCredit;
}

bool CUploadQueue::RemoveFromUploadQueue(CUpDownClient *client, LPCTSTR pszReason, bool updatewindow, bool earlyabort)
{
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
			client->m_bAddNextConnect = false;

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
			client->SetUploadState(US_NONE);
			client->SetCollectionUploadSlot(false);

			m_iHighestNumberOfFullyActivatedSlotsSinceLastCall = 0;

			result = true;
		} else {
			curClientStruct->m_pClient->SetSlotNumber(slotCounter);
			++slotCounter;
		}
	}
	return result;
}

uint32 CUploadQueue::GetAverageUpTime() const
{
	return successfullupcount ? (totaluploadtime / successfullupcount) : 0;
}

bool CUploadQueue::RemoveFromWaitingQueue(CUpDownClient *client, bool updatewindow)
{
	POSITION pos = waitinglist.Find(client);
	if (pos) {
		RemoveFromWaitingQueue(pos, updatewindow);
		return true;
	}
	return false;
}

void CUploadQueue::RemoveFromWaitingQueue(POSITION pos, bool updatewindow)
{
	m_bStatisticsWaitingListDirty = true;
	CUpDownClient *todelete = waitinglist.GetAt(pos);
	waitinglist.RemoveAt(pos);
	if (updatewindow) {
		theApp.emuledlg->transferwnd->GetQueueList()->RemoveClient(todelete);
		theApp.emuledlg->transferwnd->ShowQueueCount(waitinglist.GetCount());
	}
	todelete->m_bAddNextConnect = false;
	todelete->SetUploadState(US_NONE);
}

bool CUploadQueue::CheckForTimeOver(CUpDownClient *client)
{
	//If we have nobody in the queue, do NOT remove the current uploads.
	//This will save some bandwidth and some unneeded swapping from upload/queue/upload.
	if (waitinglist.IsEmpty() || client->GetFriendSlot())
		return false;

	const CKnownFile *pUploadingFile = theApp.sharedfiles->GetFileByID(client->requpfileid);

	if (client->HasCollectionUploadSlot()) {
		if (pUploadingFile == NULL)
			return true;
		if (CCollection::HasCollectionExtention(pUploadingFile->GetFileName()) && pUploadingFile->GetFileSize() < (uint64)MAXPRIORITYCOLL_SIZE)
			return false;
		if (thePrefs.GetLogUlDlEvents())
			AddDebugLogLine(DLP_HIGH, false, _T("%s: Upload session ended - client with Collection Slot tried to request blocks from another file"), client->GetUserName());
		return true;
	}

	// Prefer replacing a slot that has been bad for a while over growing the upload list indefinitely.
	if (ShouldTrackSlowUploadSlots()) {
		if (client->GetZeroUploadAccumulatedMs() >= UPLOAD_ZERO_RATE_EVICT_TIME) {
			client->SetSlowUploadCooldown(::GetTickCount() + UPLOAD_SLOW_EVICT_TIME);
			if (thePrefs.GetLogUlDlEvents())
				AddDebugLogLine(DLP_DEFAULT, false, _T("%s: Upload session ended due to zero upload rate for %s."), client->GetUserName(), (LPCTSTR)CastSecondsToHM(UPLOAD_ZERO_RATE_EVICT_TIME / SEC2MS(1)));
			return true;
		}
		if (client->GetSlowUploadAccumulatedMs() >= UPLOAD_SLOW_EVICT_TIME) {
			client->SetSlowUploadCooldown(::GetTickCount() + UPLOAD_SLOW_EVICT_TIME);
			if (thePrefs.GetLogUlDlEvents())
				AddDebugLogLine(DLP_DEFAULT, false, _T("%s: Upload session ended due to being too slow for %s. Rate: %s"), client->GetUserName(), (LPCTSTR)CastSecondsToHM(UPLOAD_SLOW_EVICT_TIME / SEC2MS(1)), (LPCTSTR)CastItoXBytes(client->GetUploadDatarate()));
			return true;
		}
	}

	// Broadband session limits override the stock SESSIONMAX* rotation defaults on this branch.
	const uint64 configuredSessionMaxTrans = thePrefs.GetBBSessionMaxTrans();
	const uint64 resolvedSessionMaxTrans = ResolveBBSessionMaxTransferLimit(configuredSessionMaxTrans, pUploadingFile);
	if (resolvedSessionMaxTrans > 0 && client->GetQueueSessionPayloadUp() > resolvedSessionMaxTrans && !ForceNewClient()) {
		if (thePrefs.GetLogUlDlEvents()) {
			if (configuredSessionMaxTrans <= 100)
				AddDebugLogLine(DLP_DEFAULT, false, _T("%s: Upload session ended due to BBSessionMaxTrans=%I64u%% (%s)."), client->GetUserName(), configuredSessionMaxTrans, (LPCTSTR)CastItoXBytes(resolvedSessionMaxTrans));
			else
				AddDebugLogLine(DLP_DEFAULT, false, _T("%s: Upload session ended due to BBSessionMaxTrans (%s)."), client->GetUserName(), (LPCTSTR)CastItoXBytes(resolvedSessionMaxTrans));
		}
		return true;
	}
	if (thePrefs.GetBBSessionMaxTime() > 0 && (uint64)client->GetUpStartTimeDelay() > thePrefs.GetBBSessionMaxTime() && !ForceNewClient()) {
		if (thePrefs.GetLogUlDlEvents())
			AddDebugLogLine(DLP_LOW, false, _T("%s: Upload session ended due to BBSessionMaxTime %s."), client->GetUserName(), (LPCTSTR)CastSecondsToHM((time_t)(thePrefs.GetBBSessionMaxTime() / SEC2MS(1))));
		return true;
	}

	return false;
}

void CUploadQueue::DeleteAll()
{
	waitinglist.RemoveAll();
	m_csUploadListMainThrdWriteOtherThrdsRead.Lock();
	while (!uploadinglist.IsEmpty())
		delete uploadinglist.RemoveHead();
	m_csUploadListMainThrdWriteOtherThrdsRead.Unlock();
	// PENDING: Remove from UploadBandwidthThrottler as well!
}

UINT CUploadQueue::GetWaitingPosition(CUpDownClient *client)
{
	if (!IsOnUploadQueue(client))
		return 0;
	UINT rank = 1;
	UINT myscore = client->GetScore(false);
	for (POSITION pos = waitinglist.GetHeadPosition(); pos != NULL;)
		rank += static_cast<UINT>(waitinglist.GetNext(pos)->GetScore(false) > myscore);

	return rank;
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

		// ZZ:UploadSpeedSense -->
		struct CurrentParamStruct cur;
		cur.dPingTolerance = (thePrefs.GetDynUpPingTolerance() > 100) ? ((thePrefs.GetDynUpPingTolerance() - 100) / 100.0) : 0;
		cur.uCurUpload = theApp.uploadqueue->GetDatarate();
		cur.uMinUpload = thePrefs.GetMinUpload();
		cur.uMaxUpload = (thePrefs.GetMaxUpload() ? thePrefs.GetMaxUpload() : thePrefs.GetMaxGraphUploadRate(false));
		cur.uPingToleranceMilliseconds = thePrefs.GetDynUpPingToleranceMilliseconds();
		cur.uGoingUpDivider = thePrefs.GetDynUpGoingUpDivider();
		cur.uGoingDownDivider = thePrefs.GetDynUpGoingDownDivider();
		cur.uNumberOfPingsForAverage = thePrefs.GetDynUpNumberOfPings();
		cur.uLowestInitialPingAllowed = 20; // PENDING: Hard coded min pLowestPingAllowed
		cur.bUseMillisecondPingTolerance = thePrefs.IsDynUpUseMillisecondPingTolerance();
		cur.bEnabled = thePrefs.IsDynUpEnabled();

		if (theApp.lastCommonRouteFinder->SetPrefs(cur))
			theApp.emuledlg->SetStatusBarPartsSize();
		// ZZ:UploadSpeedSense <--

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

			// ZZ:UploadSpeedSense -->
			theApp.emuledlg->ShowPing();

			bool gotEnoughHosts = theApp.clientlist->GiveClientsForTraceRoute();
			if (!gotEnoughHosts)
				theApp.serverlist->GiveServersForTraceRoute();

			// ZZ:UploadSpeedSense <--

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

				thePrefs.EstimateMaxUploadCap(theApp.uploadqueue->GetDatarate() / 1024);

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

CUpDownClient* CUploadQueue::GetNextClient(const CUpDownClient *lastclient) const
{
	if (waitinglist.IsEmpty())
		return NULL;
	if (!lastclient)
		return waitinglist.GetHead();
	POSITION pos = waitinglist.Find(const_cast<CUpDownClient*>(lastclient));
	if (!pos) {
		TRACE("Error: CUploadQueue::GetNextClient");
		return waitinglist.GetHead();
	}
	waitinglist.GetNext(pos);
	return pos ? waitinglist.GetAt(pos) : NULL;
}

void CUploadQueue::UpdateDatarates()
{
	// Calculate average data rate
	const DWORD curTick = ::GetTickCount();
	if (curTick < m_lastCalculatedDataRateTick + (SEC2MS(1) / 2))
		return;
	m_lastCalculatedDataRateTick = curTick;
	if (average_ur_hist.Count() > 1 && average_ur_hist.Tail().timestamp > average_ur_hist.Head().timestamp) {
		DWORD duration = average_ur_hist.Tail().timestamp - average_ur_hist.Head().timestamp;
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
