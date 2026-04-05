#pragma once

#include <atomic>
#include <Windows.h>

/**
 * @brief Raises a cross-thread LONG flag to a non-zero value.
 */
inline void SetAtomicLongFlag(std::atomic<LONG> &rFlag, LONG nValue = 1)
{
	rFlag.store(nValue);
}

/**
 * @brief Clears a cross-thread LONG flag back to zero.
 */
inline void ClearAtomicLongFlag(std::atomic<LONG> &rFlag)
{
	rFlag.store(0);
}

/**
 * @brief Reports whether a cross-thread LONG flag is currently non-zero.
 */
inline bool IsAtomicLongFlagSet(const std::atomic<LONG> &rFlag)
{
	return rFlag.load() != 0;
}

/**
 * @brief Consumes a cross-thread LONG flag and resets it to zero in one atomic operation.
 */
inline bool ConsumeAtomicLongFlag(std::atomic<LONG> &rFlag)
{
	return rFlag.exchange(0) != 0;
}
