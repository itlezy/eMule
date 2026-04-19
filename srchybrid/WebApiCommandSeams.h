#pragma once

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

namespace WebApiCommandSeams
{
using json = nlohmann::json;

enum class ESearchMethod : uint8_t
{
	Invalid,
	Automatic,
	Server,
	Global,
	Kad
};

enum class ESearchFileType : uint8_t
{
	Invalid,
	Any,
	Archive,
	Audio,
	CdImage,
	Image,
	Program,
	Video,
	Document,
	EmuleCollection
};

/**
 * @brief Carries the validated public search/start payload before the UI layer
 * maps it onto the legacy search model.
 */
struct SSearchStartRequest
{
	std::string strQuery;
	ESearchMethod eMethod;
	ESearchFileType eFileType;
	std::string strExtension;
	uint64_t ullMinSize;
	uint64_t ullMaxSize;
	bool bHasMinSize;
	bool bHasMaxSize;

	SSearchStartRequest()
		: eMethod(ESearchMethod::Automatic)
		, eFileType(ESearchFileType::Any)
		, ullMinSize(0)
		, ullMaxSize(0)
		, bHasMinSize(false)
		, bHasMaxSize(false)
	{
	}
};

/**
 * @brief Carries the validated transfer list selector so the UI command path
 * can test argument policy without needing the live download queue.
 */
struct STransfersListRequest
{
	std::string strFilterLower;
	unsigned uCategory;
	bool bHasCategory;

	STransfersListRequest()
		: uCategory(0)
		, bHasCategory(false)
	{
	}
};

/**
 * @brief Carries the bulk mutation selector shared by pause/resume/stop/delete.
 */
struct STransferBulkMutationRequest
{
	std::vector<json> hashes;
	bool bDeleteFiles;

	STransferBulkMutationRequest()
		: bDeleteFiles(false)
	{
	}
};

inline std::string ToLowerAscii(const std::string &rValue)
{
	std::string result(rValue);
	std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch)
	{
		return static_cast<char>(std::tolower(ch));
	});
	return result;
}

inline std::string TrimAsciiWhitespace(const std::string &rValue)
{
	std::string::size_type uBegin = 0;
	while (uBegin < rValue.size() && std::isspace(static_cast<unsigned char>(rValue[uBegin])) != 0)
		++uBegin;

	std::string::size_type uEnd = rValue.size();
	while (uEnd > uBegin && std::isspace(static_cast<unsigned char>(rValue[uEnd - 1])) != 0)
		--uEnd;

	return rValue.substr(uBegin, uEnd - uBegin);
}

/**
 * @brief Parses a non-negative JSON integer regardless of whether the parser
 * stored it as signed or unsigned.
 */
inline bool TryParseNonNegativeUInt64(const json &rValue, uint64_t &ruValue)
{
	if (rValue.is_number_unsigned()) {
		ruValue = rValue.get<uint64_t>();
		return true;
	}
	if (rValue.is_number_integer()) {
		const int64_t iValue = rValue.get<int64_t>();
		if (iValue < 0)
			return false;
		ruValue = static_cast<uint64_t>(iValue);
		return true;
	}
	return false;
}

/**
 * @brief Parses the public search method vocabulary used by search/start.
 */
inline ESearchMethod ParseSearchMethodName(const char *pszMethod)
{
	if (pszMethod == nullptr || pszMethod[0] == '\0')
		return ESearchMethod::Invalid;

	const std::string strMethod(ToLowerAscii(pszMethod));
	if (strMethod == "automatic")
		return ESearchMethod::Automatic;
	if (strMethod == "server")
		return ESearchMethod::Server;
	if (strMethod == "global")
		return ESearchMethod::Global;
	if (strMethod == "kad")
		return ESearchMethod::Kad;
	return ESearchMethod::Invalid;
}

/**
 * @brief Parses the public search type vocabulary used by search/start.
 */
inline ESearchFileType ParseSearchFileTypeName(const char *pszType)
{
	if (pszType == nullptr)
		return ESearchFileType::Invalid;

	const std::string strType(ToLowerAscii(pszType));
	if (strType.empty() || strType == "any")
		return ESearchFileType::Any;
	if (strType == "archive")
		return ESearchFileType::Archive;
	if (strType == "audio")
		return ESearchFileType::Audio;
	if (strType == "cdimage" || strType == "iso")
		return ESearchFileType::CdImage;
	if (strType == "image")
		return ESearchFileType::Image;
	if (strType == "program")
		return ESearchFileType::Program;
	if (strType == "video")
		return ESearchFileType::Video;
	if (strType == "document")
		return ESearchFileType::Document;
	if (strType == "emulecollection")
		return ESearchFileType::EmuleCollection;
	return ESearchFileType::Invalid;
}

/**
 * @brief Validates the search/start payload independently from the live search
 * window so command-contract tests stay deterministic.
 */
inline bool TryParseSearchStartRequest(const json &rParams, SSearchStartRequest &rRequest, std::string &rError)
{
	if (!rParams.contains("query") || !rParams["query"].is_string()) {
		rError = "query must be a string";
		return false;
	}

	rRequest.strQuery = TrimAsciiWhitespace(rParams["query"].get<std::string>());
	if (rRequest.strQuery.empty()) {
		rError = "query must not be empty";
		return false;
	}

	rRequest.eMethod = ESearchMethod::Automatic;
	if (rParams.contains("method")) {
		if (!rParams["method"].is_string()) {
			rError = "method must be a string";
			return false;
		}

		rRequest.eMethod = ParseSearchMethodName(rParams["method"].get_ref<const std::string&>().c_str());
		if (rRequest.eMethod == ESearchMethod::Invalid) {
			rError = "method must be one of automatic, server, global, kad";
			return false;
		}
	}

	rRequest.eFileType = ESearchFileType::Any;
	if (rParams.contains("type")) {
		if (!rParams["type"].is_string()) {
			rError = "type must be a string";
			return false;
		}

		rRequest.eFileType = ParseSearchFileTypeName(rParams["type"].get_ref<const std::string&>().c_str());
		if (rRequest.eFileType == ESearchFileType::Invalid) {
			rError = "type is not supported";
			return false;
		}
	}

	rRequest.strExtension.clear();
	if (rParams.contains("ext")) {
		if (!rParams["ext"].is_string()) {
			rError = "ext must be a string";
			return false;
		}
		rRequest.strExtension = rParams["ext"].get<std::string>();
	}

	rRequest.ullMinSize = 0;
	rRequest.bHasMinSize = false;
	if (rParams.contains("min_size")) {
		if (!TryParseNonNegativeUInt64(rParams["min_size"], rRequest.ullMinSize)) {
			rError = "min_size must be an unsigned number";
			return false;
		}
		rRequest.bHasMinSize = true;
	}

	rRequest.ullMaxSize = 0;
	rRequest.bHasMaxSize = false;
	if (rParams.contains("max_size")) {
		if (!TryParseNonNegativeUInt64(rParams["max_size"], rRequest.ullMaxSize)) {
			rError = "max_size must be an unsigned number";
			return false;
		}
		rRequest.bHasMaxSize = true;
	}

	return true;
}

/**
 * @brief Validates the transfers/list filter payload without touching the live
 * transfer list.
 */
inline bool TryParseTransfersListRequest(const json &rParams, STransfersListRequest &rRequest, std::string &rError)
{
	rRequest.strFilterLower.clear();
	rRequest.uCategory = 0;
	rRequest.bHasCategory = false;

	if (rParams.contains("filter")) {
		if (!rParams["filter"].is_string()) {
			rError = "filter must be a string when provided";
			return false;
		}
		rRequest.strFilterLower = ToLowerAscii(rParams["filter"].get<std::string>());
	}

	if (rParams.contains("category")) {
		uint64_t uCategory = 0;
		if (!TryParseNonNegativeUInt64(rParams["category"], uCategory) || uCategory > UINT_MAX) {
			rError = "category must be an unsigned number";
			return false;
		}
		rRequest.uCategory = static_cast<unsigned>(uCategory);
		rRequest.bHasCategory = true;
	}

	return true;
}

/**
 * @brief Validates the transfers/add payload and trims the public ed2k link
 * before the UI thread starts parsing legacy link objects.
 */
inline bool TryParseTransferAddLink(const json &rParams, std::string &rLink, std::string &rError)
{
	if (!rParams.contains("link") || !rParams["link"].is_string()) {
		rError = "link must be a string";
		return false;
	}

	rLink = TrimAsciiWhitespace(rParams["link"].get<std::string>());
	if (rLink.empty()) {
		rError = "link must not be empty";
		return false;
	}

	return true;
}

/**
 * @brief Validates the shared bulk-mutation payload used by pause/resume/stop
 * and delete before the UI layer resolves hashes to part files.
 */
inline bool TryParseTransferBulkMutationRequest(const json &rParams, STransferBulkMutationRequest &rRequest, std::string &rError)
{
	if (!rParams.contains("hashes") || !rParams["hashes"].is_array()) {
		rError = "hashes must be a string array";
		return false;
	}

	rRequest.hashes.clear();
	for (const json &hashValue : rParams["hashes"])
		rRequest.hashes.push_back(hashValue);
	rRequest.bDeleteFiles = rParams.value("deleteFiles", rParams.value("delete_files", false));
	return true;
}

/**
 * @brief Parses the public decimal search identifier without requiring the live
 * search window.
 */
inline bool TryParseSearchId(const json &rValue, uint32_t &ruSearchID, std::string &rError)
{
	if (!rValue.is_string()) {
		rError = "search_id must be a decimal string";
		return false;
	}

	const std::string strValue = rValue.get<std::string>();
	if (strValue.empty()) {
		rError = "search_id must not be empty";
		return false;
	}

	char *pEnd = nullptr;
	errno = 0;
	const unsigned long uValue = std::strtoul(strValue.c_str(), &pEnd, 10);
	if (errno != 0 || pEnd == nullptr || *pEnd != '\0' || uValue > UINT32_MAX) {
		rError = "search_id must be a valid uint32 decimal string";
		return false;
	}

	ruSearchID = static_cast<uint32_t>(uValue);
	return true;
}
}
