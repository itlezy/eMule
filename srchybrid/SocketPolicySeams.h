#pragma once

#include <winsock2.h>

/** Returns whether a disconnect should mark the client as a dead source. */
inline bool ShouldMarkClientAsDeadSource(bool bWasConnecting, bool bHasDownloadError)
{
	return bWasConnecting || bHasDownloadError;
}

/** Returns whether a verbose outgoing client-connect failure should be logged. */
inline bool ShouldLogVerboseClientConnectError(int nErrorCode)
{
	return nErrorCode != WSAECONNREFUSED && nErrorCode != WSAETIMEDOUT;
}
