#pragma once

#include <cerrno>
#include <climits>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

namespace WebServerJsonSeams
{
using json = nlohmann::json;

/**
 * @brief Carries one parsed REST route command together with the normalized
 * request parameters that feed the existing UI-command handler.
 */
struct SApiRoute
{
	std::string strCommand;
	json params;

	SApiRoute()
		: params(json::object())
	{
	}
};

inline std::string ToLowerAscii(const std::string &rValue)
{
	std::string result(rValue);
	for (char &rCh : result)
		rCh = static_cast<char>(std::tolower(static_cast<unsigned char>(rCh)));
	return result;
}

inline int HexNibble(const char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'a' && ch <= 'f')
		return 10 + (ch - 'a');
	if (ch >= 'A' && ch <= 'F')
		return 10 + (ch - 'A');
	return -1;
}

/**
 * @brief Decodes one URL-encoded UTF-8 token for REST path and query parsing.
 */
inline std::string UrlDecodeUtf8(const std::string &rValue)
{
	std::string decoded;
	decoded.reserve(rValue.size());
	for (size_t i = 0; i < rValue.size(); ++i) {
		const char ch = rValue[i];
		if (ch == '+' ) {
			decoded.push_back(' ');
			continue;
		}

		if (ch == '%' && i + 2 < rValue.size()) {
			const int high = HexNibble(rValue[i + 1]);
			const int low = HexNibble(rValue[i + 2]);
			if (high >= 0 && low >= 0) {
				decoded.push_back(static_cast<char>((high << 4) | low));
				i += 2;
				continue;
			}
		}

		decoded.push_back(ch);
	}
	return decoded;
}

/**
 * @brief Removes the query suffix from one request target.
 */
inline std::string GetRequestPath(const std::string &rRequestTarget)
{
	const std::string::size_type uQuery = rRequestTarget.find('?');
	return uQuery == std::string::npos ? rRequestTarget : rRequestTarget.substr(0, uQuery);
}

/**
 * @brief Reports whether one request target belongs to the in-process REST
 * surface.
 */
inline bool IsApiRequestTarget(const std::string &rRequestTarget)
{
	const std::string strPathLower(ToLowerAscii(GetRequestPath(rRequestTarget)));
	return strPathLower == "/api/v1" || strPathLower.rfind("/api/v1/", 0) == 0;
}

inline std::map<std::string, std::string> ParseQueryString(const std::string &rRequestTarget)
{
	std::map<std::string, std::string> query;
	const std::string::size_type uQuery = rRequestTarget.find('?');
	if (uQuery == std::string::npos || uQuery + 1 >= rRequestTarget.size())
		return query;

	size_t uPos = uQuery + 1;
	while (uPos <= rRequestTarget.size()) {
		const std::string::size_type uAmp = rRequestTarget.find('&', uPos);
		const std::string token = rRequestTarget.substr(
			uPos,
			uAmp == std::string::npos ? std::string::npos : (uAmp - uPos));
		if (!token.empty()) {
			const std::string::size_type uEquals = token.find('=');
			const std::string strName = UrlDecodeUtf8(token.substr(0, uEquals));
			const std::string strValue = uEquals == std::string::npos ? std::string() : UrlDecodeUtf8(token.substr(uEquals + 1));
			query[strName] = strValue;
		}

		if (uAmp == std::string::npos)
			break;
		uPos = uAmp + 1;
	}

	return query;
}

inline std::vector<std::string> SplitPathSegments(const std::string &rPath)
{
	std::vector<std::string> segments;
	size_t uPos = 0;
	while (uPos <= rPath.size()) {
		const std::string::size_type uSlash = rPath.find('/', uPos);
		const std::string token = rPath.substr(
			uPos,
			uSlash == std::string::npos ? std::string::npos : (uSlash - uPos));
		if (!token.empty())
			segments.push_back(UrlDecodeUtf8(token));

		if (uSlash == std::string::npos)
			break;
		uPos = uSlash + 1;
	}

	return segments;
}

inline bool TryParseUnsignedQueryValue(const std::map<std::string, std::string> &rQuery, const char *pszName, uint64_t &ruValue)
{
	const auto it = rQuery.find(pszName);
	if (it == rQuery.end())
		return false;

	char *pEnd = NULL;
	errno = 0;
	const unsigned long long ullValue = std::strtoull(it->second.c_str(), &pEnd, 10);
	if (errno != 0 || pEnd == NULL || *pEnd != '\0')
		return false;

	ruValue = static_cast<uint64_t>(ullValue);
	return true;
}

/**
 * @brief Parses one JSON request body and reports the stable REST error text
 * when parsing fails.
 */
inline bool TryParseRequestBody(const std::string &rRequestBody, json &rBody, std::string &rErrorMessage)
{
	rBody = json::object();
	if (rRequestBody.empty())
		return true;

	try {
		rBody = json::parse(rRequestBody);
		return true;
	} catch (const json::exception &rJsonError) {
		rErrorMessage = "invalid JSON body: ";
		rErrorMessage += rJsonError.what();
		return false;
	}
}

/**
 * @brief Maps the stable REST error codes onto HTTP status codes.
 */
inline int GetHttpStatusForError(const std::string &rCode)
{
	if (rCode == "INVALID_ARGUMENT")
		return 400;
	if (rCode == "UNAUTHORIZED")
		return 401;
	if (rCode == "NOT_FOUND")
		return 404;
	if (rCode == "INVALID_STATE")
		return 409;
	if (rCode == "EMULE_UNAVAILABLE")
		return 503;
	return 500;
}

/**
 * @brief Builds one normalized REST route command from the raw HTTP method,
 * target, and body.
 */
inline bool TryBuildRoute(
	const std::string &rMethod,
	const std::string &rRequestTarget,
	const std::string &rRequestBody,
	SApiRoute &rRoute,
	std::string &rErrorCode,
	std::string &rErrorMessage)
{
	rRoute.params = json::object();
	rErrorCode.clear();
	rErrorMessage.clear();

	const std::string strPath(GetRequestPath(rRequestTarget));
	const std::vector<std::string> segments = SplitPathSegments(strPath);
	if (segments.size() < 2 || ToLowerAscii(segments[0]) != "api" || ToLowerAscii(segments[1]) != "v1") {
		rErrorCode = "NOT_FOUND";
		rErrorMessage = "API route not found";
		return false;
	}

	const std::vector<std::string> route(segments.begin() + 2, segments.end());
	const std::map<std::string, std::string> query = ParseQueryString(rRequestTarget);
	json body;
	if (!TryParseRequestBody(rRequestBody, body, rErrorMessage)) {
		rErrorCode = "INVALID_ARGUMENT";
		return false;
	}
	if (!body.is_object()) {
		if (!rRequestBody.empty()) {
			rErrorCode = "INVALID_ARGUMENT";
			rErrorMessage = "JSON body must be an object";
			return false;
		}
		body = json::object();
	}

	const std::string strMethodUpper(ToLowerAscii(rMethod));
	const bool bGet = strMethodUpper == "get";
	const bool bPost = strMethodUpper == "post";
	if (!bGet && !bPost) {
		rErrorCode = "INVALID_ARGUMENT";
		rErrorMessage = "only GET and POST are supported";
		return false;
	}

	if (route.size() == 2 && route[0] == "app" && route[1] == "version" && bGet) {
		rRoute.strCommand = "app/version";
		return true;
	}
	if (route.size() == 2 && route[0] == "app" && route[1] == "preferences") {
		if (bGet) {
			rRoute.strCommand = "app/preferences/get";
			return true;
		}
		if (bPost) {
			rRoute.strCommand = "app/preferences/set";
			rRoute.params["prefs"] = body;
			return true;
		}
	}
	if (route.size() == 2 && route[0] == "app" && route[1] == "shutdown" && bPost) {
		rRoute.strCommand = "app/shutdown";
		return true;
	}
	if (route.size() == 2 && route[0] == "stats" && route[1] == "global" && bGet) {
		rRoute.strCommand = "stats/global";
		return true;
	}
	if (route.size() == 1 && route[0] == "transfers" && bGet) {
		rRoute.strCommand = "transfers/list";
		const auto itFilter = query.find("filter");
		if (itFilter != query.end())
			rRoute.params["filter"] = itFilter->second;
		uint64_t ullCategory = 0;
		if (TryParseUnsignedQueryValue(query, "category", ullCategory))
			rRoute.params["category"] = ullCategory;
		return true;
	}
	if (route.size() == 2 && route[0] == "transfers" && route[1] == "add" && bPost) {
		rRoute.strCommand = "transfers/add";
		rRoute.params = body;
		return true;
	}
	if (route.size() == 2 && route[0] == "transfers"
		&& (route[1] == "pause" || route[1] == "resume" || route[1] == "stop" || route[1] == "delete")
		&& bPost)
	{
		rRoute.strCommand = "transfers/" + route[1];
		rRoute.params = body;
		return true;
	}
	if (route.size() == 2 && route[0] == "transfers" && bGet) {
		rRoute.strCommand = "transfers/get";
		rRoute.params["hash"] = route[1];
		return true;
	}
	if (route.size() == 3 && route[0] == "transfers" && route[2] == "sources" && bGet) {
		rRoute.strCommand = "transfers/sources";
		rRoute.params["hash"] = route[1];
		return true;
	}
	if (route.size() == 3 && route[0] == "transfers" && route[2] == "recheck" && bPost) {
		rRoute.strCommand = "transfers/recheck";
		rRoute.params["hash"] = route[1];
		return true;
	}
	if (route.size() == 3 && route[0] == "transfers" && route[2] == "priority" && bPost) {
		rRoute.strCommand = "transfers/set_priority";
		rRoute.params = body;
		rRoute.params["hash"] = route[1];
		return true;
	}
	if (route.size() == 3 && route[0] == "transfers" && route[2] == "category" && bPost) {
		rRoute.strCommand = "transfers/set_category";
		rRoute.params = body;
		rRoute.params["hash"] = route[1];
		return true;
	}
	if (route.size() == 2 && route[0] == "uploads" && route[1] == "list" && bGet) {
		rRoute.strCommand = "uploads/list";
		return true;
	}
	if (route.size() == 2 && route[0] == "uploads" && route[1] == "queue" && bGet) {
		rRoute.strCommand = "uploads/queue";
		return true;
	}
	if (route.size() == 2 && route[0] == "uploads" && (route[1] == "remove" || route[1] == "release_slot") && bPost) {
		rRoute.strCommand = "uploads/" + route[1];
		rRoute.params = body;
		return true;
	}
	if (route.size() == 2 && route[0] == "servers" && (route[1] == "list" || route[1] == "status") && bGet) {
		rRoute.strCommand = "servers/" + route[1];
		return true;
	}
	if (route.size() == 2 && route[0] == "servers" && (route[1] == "connect" || route[1] == "disconnect" || route[1] == "add" || route[1] == "remove") && bPost) {
		rRoute.strCommand = "servers/" + route[1];
		rRoute.params = body;
		return true;
	}
	if (route.size() == 2 && route[0] == "kad" && route[1] == "status" && bGet) {
		rRoute.strCommand = "kad/status";
		return true;
	}
	if (route.size() == 2 && route[0] == "kad" && (route[1] == "connect" || route[1] == "disconnect" || route[1] == "recheck_firewall") && bPost) {
		rRoute.strCommand = "kad/" + route[1];
		return true;
	}
	if (route.size() == 2 && route[0] == "shared" && route[1] == "list" && bGet) {
		rRoute.strCommand = "shared/list";
		return true;
	}
	if (route.size() == 2 && route[0] == "shared" && route[1] == "add" && bPost) {
		rRoute.strCommand = "shared/add";
		rRoute.params = body;
		return true;
	}
	if (route.size() == 2 && route[0] == "shared" && route[1] == "remove" && bPost) {
		rRoute.strCommand = "shared/remove";
		rRoute.params = body;
		return true;
	}
	if (route.size() == 2 && route[0] == "shared" && bGet) {
		rRoute.strCommand = "shared/get";
		rRoute.params["hash"] = route[1];
		return true;
	}
	if (route.size() == 2 && route[0] == "search" && route[1] == "start" && bPost) {
		rRoute.strCommand = "search/start";
		rRoute.params = body;
		return true;
	}
	if (route.size() == 2 && route[0] == "search" && route[1] == "results" && bGet) {
		rRoute.strCommand = "search/results";
		const auto it = query.find("search_id");
		if (it != query.end())
			rRoute.params["search_id"] = it->second;
		return true;
	}
	if (route.size() == 2 && route[0] == "search" && route[1] == "stop" && bPost) {
		rRoute.strCommand = "search/stop";
		rRoute.params = body;
		return true;
	}
	if (route.size() == 1 && route[0] == "log" && bGet) {
		rRoute.strCommand = "log/get";
		uint64_t ullLimit = 0;
		if (TryParseUnsignedQueryValue(query, "limit", ullLimit))
			rRoute.params["limit"] = ullLimit > INT_MAX ? INT_MAX : static_cast<int>(ullLimit);
		return true;
	}
	if (route.size() == 2 && route[0] == "log" && route[1] == "get" && bGet) {
		rRoute.strCommand = "log/get";
		uint64_t ullLimit = 0;
		if (TryParseUnsignedQueryValue(query, "limit", ullLimit))
			rRoute.params["limit"] = ullLimit > INT_MAX ? INT_MAX : static_cast<int>(ullLimit);
		return true;
	}

	rErrorCode = "NOT_FOUND";
	rErrorMessage = "API route not found";
	return false;
}
}
