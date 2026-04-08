#pragma once

#include <cstdint>

inline bool HasMatchingPartFileHashLayout(uint16_t nExpectedMd4Parts, uint16_t nActualMd4Parts, uint16_t nExpectedAichParts, uint16_t nActualAichParts)
{
	return nExpectedMd4Parts == nActualMd4Parts
		&& nExpectedAichParts == nActualAichParts;
}
