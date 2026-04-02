#pragma once

#include <cstddef>

struct ClientCreditsChallengeLayout
{
	size_t nMessageLength;
	size_t nChallengeIpLength;
};

constexpr size_t kClientCreditsMaxPublicKeySize = 80u;

/**
 * @brief Builds the bounded secure-ident challenge layout from the caller-provided key length.
 */
inline bool TryBuildClientCreditsChallengeLayout(const size_t nKeyLength, const bool bIncludeChallengeIp, ClientCreditsChallengeLayout &layout)
{
	if (nKeyLength > kClientCreditsMaxPublicKeySize)
		return false;

	layout.nChallengeIpLength = bIncludeChallengeIp ? 5u : 0u;
	layout.nMessageLength = nKeyLength + 4u + layout.nChallengeIpLength;
	return layout.nMessageLength <= kClientCreditsMaxPublicKeySize + 9u;
}

/**
 * @brief Reports whether the caller-provided signature buffer can hold the encoded signature bytes.
 */
inline bool CanStoreClientCreditsSignature(const size_t nSignatureLength, const size_t nMaxSize)
{
	return nSignatureLength <= nMaxSize;
}

/**
 * @brief Applies the existing secure-ident failure transition without re-deriving it at each callsite.
 */
template <typename TState>
inline TState GetClientCreditsStateAfterVerifyFailure(const TState currentState, const TState neededState, const TState failedState)
{
	return currentState == neededState ? failedState : currentState;
}
