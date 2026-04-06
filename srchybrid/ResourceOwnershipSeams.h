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
 * @brief Owns a GDI object and deletes it on scope exit unless ownership was released.
 */
class ScopedGdiObject
{
public:
	explicit ScopedGdiObject(HGDIOBJ hObject = NULL) noexcept
		: m_hObject(hObject)
	{
	}

	~ScopedGdiObject() noexcept
	{
		if (m_hObject != NULL)
			::DeleteObject(m_hObject);
	}

	ScopedGdiObject(const ScopedGdiObject&) = delete;
	ScopedGdiObject& operator=(const ScopedGdiObject&) = delete;

	HGDIOBJ Get() const noexcept
	{
		return m_hObject;
	}

	HGDIOBJ Release() noexcept
	{
		HGDIOBJ hObject = m_hObject;
		m_hObject = NULL;
		return hObject;
	}

private:
	HGDIOBJ m_hObject;
};

/**
 * @brief Owns a scratch DC and deletes it automatically on scope exit.
 */
class ScopedDc
{
public:
	explicit ScopedDc(HDC hDC = NULL) noexcept
		: m_hDC(hDC)
	{
	}

	~ScopedDc() noexcept
	{
		if (m_hDC != NULL)
			::DeleteDC(m_hDC);
	}

	ScopedDc(const ScopedDc&) = delete;
	ScopedDc& operator=(const ScopedDc&) = delete;

	HDC Get() const noexcept
	{
		return m_hDC;
	}

private:
	HDC m_hDC;
};

/**
 * @brief Restores the previous GDI selection automatically when a DC temporarily selects another object.
 */
class ScopedSelectObject
{
public:
	ScopedSelectObject(HDC hDC, HGDIOBJ hObject) noexcept
		: m_hDC(hDC)
		, m_hPrevious((hDC != NULL && hObject != NULL) ? ::SelectObject(hDC, hObject) : NULL)
	{
	}

	~ScopedSelectObject() noexcept
	{
		if (m_hDC != NULL && m_hPrevious != NULL && m_hPrevious != HGDI_ERROR)
			::SelectObject(m_hDC, m_hPrevious);
	}

	ScopedSelectObject(const ScopedSelectObject&) = delete;
	ScopedSelectObject& operator=(const ScopedSelectObject&) = delete;

	bool IsValid() const noexcept
	{
		return m_hPrevious != NULL && m_hPrevious != HGDI_ERROR;
	}

private:
	HDC m_hDC;
	HGDIOBJ m_hPrevious;
};

/**
 * @brief Returns whether the wrapped Win32 handle currently owns a usable OS handle.
 */
inline bool HasOpenHandle(const ScopedHandle &rHandle) noexcept
{
	return rHandle.IsValid();
}

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
 * @brief Releases ownership when a callee consumed the temporary object and replaced the raw pointer.
 *
 * Some legacy handoff sites delete or otherwise consume the temporary object before returning a different
 * raw pointer to the caller. This seam lets the temporary unique_ptr forget that original address without
 * touching the replacement object.
 */
template <typename TObject>
inline void ReleaseOwnedObjectIfSuperseded(std::unique_ptr<TObject> &pOwnedObject, TObject *pPreviousObject, TObject *pCurrentObject) noexcept
{
	if (pOwnedObject.get() == pPreviousObject && pPreviousObject != pCurrentObject)
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
