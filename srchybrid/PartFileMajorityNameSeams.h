#pragma once

#include <atlstr.h>
#include <cstdint>
#include <vector>

namespace PartFileMajorityNameSeams
{
struct MajorityNameSelection
{
	bool HasCandidate = false;
	CString Name;
	UINT CandidateVotes = 0;
	UINT TotalVotes = 0;
	UINT RequiredPercent = 51;
	UINT MinimumVotes = 0;
};

/**
 * @brief Normalizes the required source-name agreement percentage for majority filename decisions.
 */
inline UINT NormalizeRequiredPercent(UINT percent)
{
	if (percent < 1)
		return 1;
	if (percent > 100)
		return 100;
	return percent;
}

/**
 * @brief Returns whether a candidate has enough source-name agreement to be applied.
 */
inline bool HasRequiredAgreement(UINT candidateVotes, UINT totalVotes, UINT requiredPercent)
{
	if (candidateVotes == 0 || totalVotes == 0 || candidateVotes > totalVotes)
		return false;
	return static_cast<uint64_t>(candidateVotes) * 100u >= static_cast<uint64_t>(NormalizeRequiredPercent(requiredPercent)) * totalVotes;
}

/**
 * @brief Selects the unique majority filename candidate from normalized source-provided names.
 */
inline MajorityNameSelection SelectMajorityName(const std::vector<CString> &sourceNames, UINT minimumVotes, UINT requiredPercent)
{
	struct Bucket
	{
		CString Name;
		UINT Votes = 0;
	};

	std::vector<Bucket> buckets;
	UINT totalVotes = 0;
	for (const CString &sourceName : sourceNames) {
		CString name(sourceName);
		name.Trim();
		if (name.IsEmpty())
			continue;

		++totalVotes;
		bool found = false;
		for (Bucket &bucket : buckets) {
			if (bucket.Name.CompareNoCase(name) == 0) {
				++bucket.Votes;
				found = true;
				break;
			}
		}
		if (!found) {
			Bucket bucket;
			bucket.Name = name;
			bucket.Votes = 1;
			buckets.push_back(bucket);
		}
	}

	MajorityNameSelection selection;
	selection.TotalVotes = totalVotes;
	selection.RequiredPercent = NormalizeRequiredPercent(requiredPercent);
	selection.MinimumVotes = minimumVotes;

	bool tiedForFirst = false;
	for (const Bucket &bucket : buckets) {
		if (bucket.Votes > selection.CandidateVotes) {
			selection.Name = bucket.Name;
			selection.CandidateVotes = bucket.Votes;
			tiedForFirst = false;
		} else if (bucket.Votes == selection.CandidateVotes) {
			tiedForFirst = true;
		}
	}

	selection.HasCandidate = !tiedForFirst
		&& selection.CandidateVotes >= minimumVotes
		&& HasRequiredAgreement(selection.CandidateVotes, selection.TotalVotes, selection.RequiredPercent);
	if (!selection.HasCandidate)
		selection.Name.Empty();
	return selection;
}
}
