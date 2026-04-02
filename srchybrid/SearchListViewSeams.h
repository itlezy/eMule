#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace SearchListViewSeams
{
/**
 * @brief Identifies whether a visible projection row represents a parent or child result.
 */
enum class EVisibleRowKind : uint8_t
{
	Parent,
	Child
};

/**
 * @brief Describes one stored search result entry for visible-row seam tests.
 */
struct SStoredRow
{
	size_t uRowId;
	size_t uParentRowId;
	bool bHidden;
	bool bExpanded;
};

/**
 * @brief Describes one visible row emitted by the flattened list projection.
 */
struct SVisibleRow
{
	size_t uRowId;
	EVisibleRowKind eKind;
};

/**
 * @brief Flattens stored parent/child search rows into the visible owner-data order.
 *
 * Hidden parents are omitted entirely. Expanded parents append every matching
 * child row directly after the parent in stored-order.
 */
inline void BuildVisibleRows(const std::vector<SStoredRow> &rStoredRows, std::vector<SVisibleRow> *pVisibleRows)
{
	if (pVisibleRows == NULL)
		return;

	pVisibleRows->clear();
	for (const SStoredRow &rParentRow : rStoredRows) {
		if (rParentRow.uParentRowId != 0 || rParentRow.bHidden)
			continue;

		pVisibleRows->push_back({rParentRow.uRowId, EVisibleRowKind::Parent});
		if (!rParentRow.bExpanded)
			continue;

		for (const SStoredRow &rChildRow : rStoredRows) {
			if (rChildRow.uParentRowId == rParentRow.uRowId)
				pVisibleRows->push_back({rChildRow.uRowId, EVisibleRowKind::Child});
		}
	}
}

/**
 * @brief Finds the visible row index for a stored row id after flattening.
 */
inline ptrdiff_t FindVisibleRowIndex(const std::vector<SVisibleRow> &rVisibleRows, const size_t uRowId)
{
	for (size_t i = 0; i < rVisibleRows.size(); ++i)
		if (rVisibleRows[i].uRowId == uRowId)
			return static_cast<ptrdiff_t>(i);
	return -1;
}

/**
 * @brief Reports whether an owner-data list mutation must be marshalled to the UI thread.
 */
inline bool ShouldMarshalOwnerDataMutation(const unsigned long dwCurrentThreadId, const unsigned long dwUiThreadId)
{
	return dwCurrentThreadId != 0 && dwUiThreadId != 0 && dwCurrentThreadId != dwUiThreadId;
}

/**
 * @brief Coalesces repeated worker refresh requests into a single posted UI wakeup.
 */
inline bool TryQueueCoalescedOwnerDataRefresh(bool &rbRefreshMessagePending)
{
	if (rbRefreshMessagePending)
		return false;

	rbRefreshMessagePending = true;
	return true;
}
}
