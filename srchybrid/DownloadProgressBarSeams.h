#pragma once

namespace DownloadProgressBarSeams
{
inline bool HasDrawableExtent(const int iWidth, const int iHeight)
{
	return iWidth > 0 && iHeight > 0;
}

inline bool ShouldIsolateFlatBarDcState(const bool bUseFlatBar)
{
	return bUseFlatBar;
}
}
