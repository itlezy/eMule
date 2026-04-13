#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * \brief Composite lookup key for startup reuse of known-file metadata.
 *
 * The key mirrors the legacy filename/date/size tuple used by `FindKnownFile`
 * so callers can replace linear scans without changing lookup semantics.
 */
struct KnownFileLookupKey
{
	std::wstring fileName;
	std::int64_t utcFileDate = -1;
	std::uint64_t fileSize = 0;

	bool operator==(const KnownFileLookupKey &rOther) const noexcept
	{
		return utcFileDate == rOther.utcFileDate
			&& fileSize == rOther.fileSize
			&& fileName == rOther.fileName;
	}
};

/**
 * \brief Hash functor for `KnownFileLookupKey`.
 */
struct KnownFileLookupKeyHash
{
	size_t operator()(const KnownFileLookupKey &rKey) const noexcept
	{
		const size_t hName = std::hash<std::wstring>{}(rKey.fileName);
		const size_t hDate = std::hash<std::int64_t>{}(rKey.utcFileDate);
		const size_t hSize = std::hash<std::uint64_t>{}(rKey.fileSize);
		return hName ^ (hDate + 0x9e3779b9u + (hName << 6) + (hName >> 2)) ^ (hSize + 0x9e3779b9u + (hDate << 6) + (hDate >> 2));
	}
};

/**
 * \brief Builds a lookup key from the legacy filename/date/size tuple.
 */
inline KnownFileLookupKey BuildKnownFileLookupKey(const wchar_t *pszFileName, time_t tUtcFileDate, std::uint64_t ullFileSize)
{
	KnownFileLookupKey key = {};
	key.fileName = (pszFileName != NULL) ? std::wstring(pszFileName) : std::wstring();
	key.utcFileDate = static_cast<std::int64_t>(tUtcFileDate);
	key.fileSize = ullFileSize;
	return key;
}

/**
 * \brief Small collision-preserving index for known-file startup lookups.
 *
 * Multiple values may share the same filename/date/size tuple, so callers must
 * resolve collisions inside the returned bucket.
 */
template <typename TValue>
class TKnownFileLookupIndex
{
public:
	using Bucket = std::vector<TValue>;
	using Map = std::unordered_map<KnownFileLookupKey, Bucket, KnownFileLookupKeyHash>;

	void Clear()
	{
		m_index.clear();
	}

	void Add(const wchar_t *pszFileName, time_t tUtcFileDate, std::uint64_t ullFileSize, const TValue &rValue)
	{
		m_index[BuildKnownFileLookupKey(pszFileName, tUtcFileDate, ullFileSize)].push_back(rValue);
	}

	bool Remove(const wchar_t *pszFileName, time_t tUtcFileDate, std::uint64_t ullFileSize, const TValue &rValue)
	{
		const KnownFileLookupKey key = BuildKnownFileLookupKey(pszFileName, tUtcFileDate, ullFileSize);
		const auto it = m_index.find(key);
		if (it == m_index.end())
			return false;

		Bucket &bucket = it->second;
		for (auto bucketIt = bucket.begin(); bucketIt != bucket.end(); ++bucketIt) {
			if (*bucketIt != rValue)
				continue;
			bucket.erase(bucketIt);
			if (bucket.empty())
				m_index.erase(it);
			return true;
		}
		return false;
	}

	const Bucket* FindBucket(const wchar_t *pszFileName, time_t tUtcFileDate, std::uint64_t ullFileSize) const
	{
		const auto it = m_index.find(BuildKnownFileLookupKey(pszFileName, tUtcFileDate, ullFileSize));
		return (it != m_index.end()) ? &it->second : NULL;
	}

	bool IsEmpty() const noexcept
	{
		return m_index.empty();
	}

private:
	Map m_index;
};
