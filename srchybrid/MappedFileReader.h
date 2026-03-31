#pragma once

#include <Windows.h>
#include <cstddef>

/**
 * @brief Consumes contiguous byte spans from a mapped file range.
 */
class IMappedFileRangeVisitor
{
public:
	virtual ~IMappedFileRangeVisitor() = default;

	/**
	 * @brief Processes the next contiguous span from the requested file range.
	 */
	virtual void OnMappedFileBytes(const BYTE *pBytes, size_t nByteCount) = 0;
};

/**
 * @brief Walks a file range through allocation-granularity-aligned mapping windows.
 */
bool VisitMappedFileRange(HANDLE hFile, ULONGLONG nOffset, ULONGLONG nLength, IMappedFileRangeVisitor &rVisitor, DWORD *pdwError = NULL);
