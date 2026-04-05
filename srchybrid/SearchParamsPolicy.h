#pragma once

#include <cstdint>

namespace SearchParamsPolicy
{
constexpr uint8_t kDefaultSearchType = 1u;
constexpr uint8_t kMaxSupportedSearchType = 3u;

/**
 * @brief Maps persisted search-method ids onto the currently supported search methods.
 *
 * Older profiles may still contain the removed ContentDB search method. Those
 * stale ids now fall back to the standard server search method instead of
 * reaching dead UI branches.
 */
inline uint8_t NormalizeStoredSearchType(const uint8_t uStoredSearchType)
{
	return uStoredSearchType <= kMaxSupportedSearchType ? uStoredSearchType : kDefaultSearchType;
}
}
