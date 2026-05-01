#pragma once

#include <atlstr.h>

#include "types.h"
#include "Version.h"

enum FriendLinkTransitionAction
{
	friendLinkTransitionNone,
	friendLinkTransitionUserhashFailed,
	friendLinkTransitionUnlink
};

struct FriendLinkSnapshot
{
	bool bHasFriend;
	bool bHasUserhash;
	bool bTryingToConnect;
	bool bHashMatchesClient;
	bool bEndpointMatchesClient;
};

/**
 * @brief Classifies how the current linked friend should react to the latest user-hash snapshot.
 */
inline FriendLinkTransitionAction ClassifyFriendLinkTransition(const FriendLinkSnapshot &rSnapshot)
{
	if (!rSnapshot.bHasFriend || !rSnapshot.bHasUserhash || rSnapshot.bHashMatchesClient)
		return friendLinkTransitionNone;

	return rSnapshot.bTryingToConnect ? friendLinkTransitionUserhashFailed : friendLinkTransitionUnlink;
}

/**
 * @brief Reports whether the current friend snapshot requires a fresh search in the friend list.
 */
inline bool ShouldSearchReplacementFriend(const FriendLinkSnapshot &rSnapshot)
{
	return !rSnapshot.bHasFriend || rSnapshot.bHasUserhash || !rSnapshot.bEndpointMatchesClient;
}

struct BuddyHelloSnapshot
{
	bool bShouldAdvertise;
	uint32 dwBuddyIP;
	uint16 nBuddyPort;
};

/**
 * @brief Returns the peer-visible mod identity advertised in eMule hello tags.
 */
inline CString GetAdvertisedClientModIdentity()
{
	return MOD_CLIENT_MOD_VERSION_TEXT;
}

/**
 * @brief Formats a client software label with its optional mod identity.
 */
inline CString BuildFullClientSoftVersionDisplay(const CString &strClientSoftware, const CString &strClientModVersion)
{
	if (strClientModVersion.IsEmpty())
		return strClientSoftware;

	CString strDisplay;
	strDisplay.Format(_T("%s [%s]"), (LPCTSTR)strClientSoftware, (LPCTSTR)strClientModVersion);
	return strDisplay;
}

/**
 * @brief Builds the one-shot buddy payload snapshot used while serializing hello tags.
 */
inline BuddyHelloSnapshot BuildBuddyHelloSnapshot(bool bIsFirewalled, bool bHasBuddy, uint32 dwBuddyIP, uint16 nBuddyPort)
{
	BuddyHelloSnapshot snapshot = {};
	snapshot.bShouldAdvertise = bIsFirewalled && bHasBuddy;
	snapshot.dwBuddyIP = dwBuddyIP;
	snapshot.nBuddyPort = nBuddyPort;
	return snapshot;
}

/**
 * @brief Computes the hello tag count once the buddy advertisement decision is known.
 */
inline uint32 GetHelloTagCount(const BuddyHelloSnapshot &rBuddySnapshot)
{
	return 7u + (rBuddySnapshot.bShouldAdvertise ? 2u : 0u);
}
