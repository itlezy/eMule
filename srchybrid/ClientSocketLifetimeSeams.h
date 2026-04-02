#pragma once

/**
 * @brief Clears a bidirectional client/socket link only when both sides still point at each other.
 *
 * This keeps late teardown paths from accidentally nulling a newer attachment which already replaced
 * one side of the relationship.
 */
template <typename TClient, typename TSocket>
inline void DetachClientSocketPair(TClient *pClient, TSocket *pSocket)
{
	if (pClient != nullptr && pClient->socket == pSocket)
		pClient->socket = nullptr;
	if (pSocket != nullptr && pSocket->client == pClient)
		pSocket->client = nullptr;
}

/**
 * @brief Initializes a newly created socket-side link slot to the detached state before any first attach.
 */
template <typename TSocket>
inline void ResetClientSocketPeer(TSocket *pSocket)
{
	if (pSocket != nullptr)
		pSocket->client = nullptr;
}

/**
 * @brief Rebinds a client/socket pair after detaching any previous peer on either side.
 */
template <typename TClient, typename TSocket>
inline void LinkClientSocketPair(TClient *pClient, TSocket *pSocket)
{
	if (pClient != nullptr && pClient->socket != nullptr && pClient->socket != pSocket)
		DetachClientSocketPair(pClient, pClient->socket);
	if (pSocket != nullptr && pSocket->client != nullptr && pSocket->client != pClient)
		DetachClientSocketPair(pSocket->client, pSocket);

	if (pSocket != nullptr)
		pSocket->client = pClient;
	if (pClient != nullptr)
		pClient->socket = pSocket;
}

/**
 * @brief Reports whether neither side still references the other after teardown.
 */
template <typename TClient, typename TSocket>
inline bool IsClientSocketPairDetached(const TClient *pClient, const TSocket *pSocket)
{
	return (pClient == nullptr || pClient->socket == nullptr)
		&& (pSocket == nullptr || pSocket->client == nullptr);
}
