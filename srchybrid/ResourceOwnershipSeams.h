#pragma once

#include <memory>
#include <windows.h>

/**
 * @brief Owns a Win32 file handle and closes it automatically on scope exit.
 *
 * The seam keeps file-handle cleanup deterministic across the early-return and exception-heavy
 * hashing paths without changing the surrounding API contracts.
 */
class ScopedHandle
{
public:
	ScopedHandle() noexcept
		: m_hHandle(INVALID_HANDLE_VALUE)
	{
	}

	explicit ScopedHandle(HANDLE hHandle) noexcept
		: m_hHandle(hHandle)
	{
	}

	~ScopedHandle() noexcept
	{
		Reset();
	}

	ScopedHandle(const ScopedHandle&) = delete;
	ScopedHandle& operator=(const ScopedHandle&) = delete;

	ScopedHandle(ScopedHandle&& other) noexcept
		: m_hHandle(other.Release())
	{
	}

	ScopedHandle& operator=(ScopedHandle&& other) noexcept
	{
		if (this != &other)
			Reset(other.Release());
		return *this;
	}

	/**
	 * @brief Returns the currently owned raw handle.
	 */
	HANDLE Get() const noexcept
	{
		return m_hHandle;
	}

	/**
	 * @brief Reports whether the seam currently owns a valid Win32 file handle.
	 */
	bool IsValid() const noexcept
	{
		return m_hHandle != NULL && m_hHandle != INVALID_HANDLE_VALUE;
	}

	/**
	 * @brief Releases ownership without closing the raw handle.
	 */
	HANDLE Release() noexcept
	{
		HANDLE hHandle = m_hHandle;
		m_hHandle = INVALID_HANDLE_VALUE;
		return hHandle;
	}

	/**
	 * @brief Replaces the owned handle after closing the previous one when needed.
	 */
	void Reset(HANDLE hHandle = INVALID_HANDLE_VALUE) noexcept
	{
		if (IsValid())
			::CloseHandle(m_hHandle);
		m_hHandle = hHandle;
	}

private:
	HANDLE m_hHandle;
};

/**
 * @brief Releases a uniquely owned object only when the raw pointer still refers to that instance.
 *
 * This keeps temporary ownership around until the handoff site has definitively accepted the object.
 */
template <typename TObject>
inline void ReleaseOwnedObjectIfMatched(std::unique_ptr<TObject> &pOwnedObject, TObject *pCurrentObject) noexcept
{
	if (pOwnedObject.get() == pCurrentObject)
		static_cast<void>(pOwnedObject.release());
}

/**
 * @brief Appends staged request pointers in-order to the destination pending-block list.
 *
 * The seam keeps the temporary pointer staging container independent from the list ownership model.
 */
template <typename TPendingList, typename TPendingBlock, typename TRequestedBlock>
inline void AppendPendingBlocksFromStage(TPendingList &rPendingList, TRequestedBlock *const *ppBlocks, int nBlockCount)
{
	for (int i = 0; i < nBlockCount; ++i)
		rPendingList.AddTail(new TPendingBlock{ppBlocks[i]});
}
