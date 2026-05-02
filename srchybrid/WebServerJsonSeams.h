#pragma once

#include <cerrno>
#include <climits>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

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
 * @brief Copies common collection query parameters into one command payload.
 */
inline void CopyTransferListQueryParams(const std::map<std::string, std::string> &rQuery, json &rParams)
{
	const auto itState = rQuery.find("state");
	if (itState != rQuery.end())
		rParams["filter"] = itState->second;
	uint64_t ullCategory = 0;
	if (TryParseUnsignedQueryValue(rQuery, "categoryId", ullCategory))
		rParams["categoryId"] = ullCategory;
}

/**
 * @brief Copies the bounded log-tail query parameter into one command payload.
 */
inline void CopyLogQueryParams(const std::map<std::string, std::string> &rQuery, json &rParams)
{
	uint64_t ullLimit = 0;
	if (TryParseUnsignedQueryValue(rQuery, "limit", ullLimit))
		rParams["limit"] = ullLimit > INT_MAX ? INT_MAX : static_cast<int>(ullLimit);
}

inline void CopyPagingQueryParams(const std::map<std::string, std::string> &rQuery, json &rParams)
{
	uint64_t ullLimit = 0;
	if (TryParseUnsignedQueryValue(rQuery, "limit", ullLimit))
		rParams["_limit"] = ullLimit > 1000 ? 1000 : static_cast<int>(ullLimit);
	uint64_t ullOffset = 0;
	if (TryParseUnsignedQueryValue(rQuery, "offset", ullOffset))
		rParams["_offset"] = ullOffset > INT_MAX ? INT_MAX : static_cast<int>(ullOffset);
}

/**
 * @brief Parses an endpoint route token in the public "address:port" form.
 */
inline bool TryCopyEndpointToken(const std::string &rValue, json &rParams)
{
	const std::string::size_type uColon = rValue.rfind(':');
	if (uColon == std::string::npos || uColon == 0 || uColon + 1 >= rValue.size())
		return false;

	char *pEnd = NULL;
	errno = 0;
	const unsigned long ulPort = std::strtoul(rValue.substr(uColon + 1).c_str(), &pEnd, 10);
	if (errno != 0 || pEnd == NULL || *pEnd != '\0' || ulPort == 0 || ulPort > 0xFFFFul)
		return false;

	rParams["addr"] = rValue.substr(0, uColon);
	rParams["port"] = static_cast<unsigned>(ulPort);
	return true;
}

/**
 * @brief Copies a public client id token into the legacy selector payload.
 */
inline void CopyClientIdToken(const std::string &rClientId, json &rParams)
{
	if (rClientId.size() == 32)
		rParams["userHash"] = rClientId;
	else {
		json endpoint = json::object();
		if (TryCopyEndpointToken(rClientId, endpoint)) {
			rParams["ip"] = endpoint["addr"];
			rParams["port"] = endpoint["port"];
		}
	}
}

/**
 * @brief Marks list responses that should use the redesigned resource envelope.
 */
inline void RequestItemsEnvelope(json &rParams)
{
	rParams["_items_envelope"] = true;
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

	const std::string strMethodLower(ToLowerAscii(rMethod));
	const bool bGet = strMethodLower == "get";
	const bool bPost = strMethodLower == "post";
	const bool bPatch = strMethodLower == "patch";
	const bool bDelete = strMethodLower == "delete";
	if (!bGet && !bPost && !bPatch && !bDelete) {
		rErrorCode = "INVALID_ARGUMENT";
		rErrorMessage = "only GET, POST, PATCH, and DELETE are supported";
		return false;
	}

	if (route.empty()) {
		rErrorCode = "NOT_FOUND";
		rErrorMessage = "API route not found";
		return false;
	}

	if (route.size() == 1 && route[0] == "app" && bGet) {
		rRoute.strCommand = "app/version";
		return true;
	}
	if (route.size() == 2 && route[0] == "app" && route[1] == "preferences") {
		if (bGet) {
			rRoute.strCommand = "app/preferences/get";
			return true;
		}
		if (bPatch) {
			rRoute.strCommand = "app/preferences/set";
			rRoute.params["prefs"] = body;
			return true;
		}
	}
	if (route.size() == 2 && route[0] == "app" && route[1] == "shutdown" && bPost) {
		rRoute.strCommand = "app/shutdown";
		return true;
	}
	if (route.size() == 1 && route[0] == "status" && bGet) {
		rRoute.strCommand = "status/get";
		return true;
	}
	if (route.size() == 1 && route[0] == "stats" && bGet) {
		rRoute.strCommand = "stats/global";
		return true;
	}
	if (route.size() == 1 && route[0] == "snapshot" && bGet) {
		rRoute.strCommand = "snapshot/get";
		CopyLogQueryParams(query, rRoute.params);
		return true;
	}
	if (route.size() == 1 && route[0] == "categories" && bGet) {
		rRoute.strCommand = "categories/list";
		CopyPagingQueryParams(query, rRoute.params);
		RequestItemsEnvelope(rRoute.params);
		return true;
	}
	if (route.size() == 1 && route[0] == "categories" && bPost) {
		rRoute.strCommand = "categories/create";
		rRoute.params = body;
		return true;
	}
	if (route.size() == 2 && route[0] == "categories" && bGet) {
		rRoute.strCommand = "categories/get";
		rRoute.params["id"] = route[1];
		return true;
	}
	if (route.size() == 2 && route[0] == "categories" && bPatch) {
		rRoute.strCommand = "categories/update";
		rRoute.params = body;
		rRoute.params["id"] = route[1];
		return true;
	}
	if (route.size() == 2 && route[0] == "categories" && bDelete) {
		rRoute.strCommand = "categories/delete";
		rRoute.params["id"] = route[1];
		return true;
	}
	if (route.size() == 1 && route[0] == "transfers" && bGet) {
		rRoute.strCommand = "transfers/list";
		CopyTransferListQueryParams(query, rRoute.params);
		CopyPagingQueryParams(query, rRoute.params);
		RequestItemsEnvelope(rRoute.params);
		return true;
	}
	if (route.size() == 1 && route[0] == "transfers" && bPost) {
		rRoute.strCommand = "transfers/add";
		rRoute.params = body;
		return true;
	}
	if (route.size() == 3 && route[0] == "transfers" && route[1] == "operations" && route[2] == "clear-completed" && bPost) {
		rRoute.strCommand = "transfers/clear_completed";
		rRoute.params = body;
		return true;
	}
	if (route.size() == 2 && route[0] == "transfers" && bGet) {
		rRoute.strCommand = "transfers/get";
		rRoute.params["hash"] = route[1];
		return true;
	}
	if (route.size() == 4 && route[0] == "transfers" && route[2] == "operations" && bPost) {
		const std::string strOperation = route[3];
		if (strOperation == "pause" || strOperation == "resume" || strOperation == "stop") {
			rRoute.strCommand = "transfers/" + strOperation;
			rRoute.params = body;
			rRoute.params["hashes"] = json::array({route[1]});
			return true;
		}
		if (strOperation == "recheck" || strOperation == "preview") {
			rRoute.strCommand = "transfers/" + strOperation;
			rRoute.params = body;
			rRoute.params["hash"] = route[1];
			return true;
		}
		rErrorCode = "NOT_FOUND";
		rErrorMessage = "API route not found";
		return false;
	}
	if (route.size() == 2 && route[0] == "transfers" && bPatch) {
		if (body.contains("priority")) {
			rRoute.strCommand = "transfers/set_priority";
			rRoute.params = body;
			rRoute.params["hash"] = route[1];
			return true;
		}
		if (body.contains("categoryId") || body.contains("categoryName")) {
			rRoute.strCommand = "transfers/set_category";
			rRoute.params = body;
			rRoute.params["hash"] = route[1];
			return true;
		}
		if (body.contains("name")) {
			rRoute.strCommand = "transfers/rename";
			rRoute.params = body;
			rRoute.params["hash"] = route[1];
			return true;
		}
		rErrorCode = "INVALID_ARGUMENT";
		rErrorMessage = "transfer PATCH requires priority, categoryId, categoryName, or name";
		return false;
	}
	if (route.size() == 2 && route[0] == "transfers" && bDelete) {
		rRoute.strCommand = "transfers/delete";
		rRoute.params = body;
		rRoute.params["hashes"] = json::array({route[1]});
		return true;
	}
	if (route.size() == 3 && route[0] == "transfers" && route[2] == "sources" && bGet) {
		rRoute.strCommand = "transfers/sources";
		rRoute.params["hash"] = route[1];
		CopyPagingQueryParams(query, rRoute.params);
		RequestItemsEnvelope(rRoute.params);
		return true;
	}
	if (route.size() == 3 && route[0] == "transfers" && route[2] == "details" && bGet) {
		rRoute.strCommand = "transfers/details";
		rRoute.params["hash"] = route[1];
		return true;
	}
	if (route.size() == 6 && route[0] == "transfers" && route[2] == "sources" && route[4] == "operations" && route[5] == "browse" && bPost) {
		rRoute.strCommand = "transfers/source_browse";
		rRoute.params = body;
		rRoute.params["hash"] = route[1];
		CopyClientIdToken(route[3], rRoute.params);
		return true;
	}
	if (route.size() == 1 && route[0] == "uploads" && bGet) {
		rRoute.strCommand = "uploads/list";
		CopyPagingQueryParams(query, rRoute.params);
		RequestItemsEnvelope(rRoute.params);
		return true;
	}
	if (route.size() == 1 && route[0] == "upload-queue" && bGet) {
		rRoute.strCommand = "uploads/queue";
		CopyPagingQueryParams(query, rRoute.params);
		RequestItemsEnvelope(rRoute.params);
		return true;
	}
	if (route.size() == 2 && route[0] == "uploads" && bDelete) {
		rRoute.strCommand = "uploads/remove";
		rRoute.params = body;
		CopyClientIdToken(route[1], rRoute.params);
		return true;
	}
	if (route.size() == 4 && route[0] == "uploads" && route[2] == "operations" && route[3] == "release-slot" && bPost) {
		rRoute.strCommand = "uploads/release_slot";
		rRoute.params = body;
		CopyClientIdToken(route[1], rRoute.params);
		return true;
	}
	if (route.size() == 4 && route[0] == "uploads" && route[2] == "operations" && bPost) {
		if (route[3] == "add-friend" || route[3] == "remove-friend" || route[3] == "ban" || route[3] == "unban") {
			rRoute.strCommand = "peers/" + route[3];
			rRoute.params = body;
			CopyClientIdToken(route[1], rRoute.params);
			return true;
		}
		if (route[3] == "remove") {
			rRoute.strCommand = "uploads/remove";
			rRoute.params = body;
			CopyClientIdToken(route[1], rRoute.params);
			return true;
		}
		rErrorCode = "NOT_FOUND";
		rErrorMessage = "API route not found";
		return false;
	}
	if (route.size() == 4 && route[0] == "upload-queue" && route[2] == "operations" && route[3] == "release-slot" && bPost) {
		rRoute.strCommand = "uploads/release_slot";
		rRoute.params = body;
		CopyClientIdToken(route[1], rRoute.params);
		return true;
	}
	if (route.size() == 4 && route[0] == "upload-queue" && route[2] == "operations" && bPost) {
		if (route[3] == "add-friend" || route[3] == "remove-friend" || route[3] == "ban" || route[3] == "unban") {
			rRoute.strCommand = "peers/" + route[3];
			rRoute.params = body;
			CopyClientIdToken(route[1], rRoute.params);
			return true;
		}
		if (route[3] == "remove") {
			rRoute.strCommand = "uploads/remove";
			rRoute.params = body;
			CopyClientIdToken(route[1], rRoute.params);
			return true;
		}
		rErrorCode = "NOT_FOUND";
		rErrorMessage = "API route not found";
		return false;
	}
	if (route.size() == 6 && route[0] == "transfers" && route[2] == "sources" && route[4] == "operations" && bPost) {
		if (route[5] == "add-friend" || route[5] == "remove-friend" || route[5] == "ban" || route[5] == "unban") {
			rRoute.strCommand = "peers/" + route[5];
			rRoute.params = body;
			rRoute.params["hash"] = route[1];
			CopyClientIdToken(route[3], rRoute.params);
			return true;
		}
		if (route[5] == "remove") {
			rRoute.strCommand = "peers/remove";
			rRoute.params = body;
			rRoute.params["hash"] = route[1];
			CopyClientIdToken(route[3], rRoute.params);
			return true;
		}
		if (route[5] == "release-slot") {
			rRoute.strCommand = "uploads/release_slot";
			rRoute.params = body;
			CopyClientIdToken(route[3], rRoute.params);
			return true;
		}
		rErrorCode = "NOT_FOUND";
		rErrorMessage = "API route not found";
		return false;
	}
	if (route.size() == 1 && route[0] == "servers" && bGet) {
		rRoute.strCommand = "servers/list";
		CopyPagingQueryParams(query, rRoute.params);
		RequestItemsEnvelope(rRoute.params);
		return true;
	}
	if (route.size() == 1 && route[0] == "servers" && bPost) {
		rRoute.strCommand = "servers/add";
		rRoute.params = body;
		if (body.contains("addr")) {
			rErrorCode = "INVALID_ARGUMENT";
			rErrorMessage = "server create uses address, not addr";
			return false;
		}
		if (body.contains("address"))
			rRoute.params["addr"] = body["address"];
		return true;
	}
	if (route.size() == 3 && route[0] == "servers" && route[1] == "operations" && bPost) {
		if (route[2] == "connect" || route[2] == "disconnect") {
			rRoute.strCommand = "servers/" + route[2];
			rRoute.params = body;
			return true;
		}
		rErrorCode = "NOT_FOUND";
		rErrorMessage = "API route not found";
		return false;
	}
	if (route.size() == 2 && route[0] == "servers" && route[1] == "met-url-imports" && bPost) {
		rRoute.strCommand = "servers/import_met_url";
		rRoute.params = body;
		return true;
	}
	if (route.size() == 4 && route[0] == "servers" && route[2] == "operations" && route[3] == "connect" && bPost) {
		rRoute.strCommand = "servers/connect";
		rRoute.params = body;
		if (!TryCopyEndpointToken(route[1], rRoute.params)) {
			rErrorCode = "INVALID_ARGUMENT";
			rErrorMessage = "server id must use address:port";
			return false;
		}
		return true;
	}
	if (route.size() == 2 && route[0] == "servers" && bGet) {
		rRoute.strCommand = "servers/get";
		if (!TryCopyEndpointToken(route[1], rRoute.params)) {
			rErrorCode = "INVALID_ARGUMENT";
			rErrorMessage = "server id must use address:port";
			return false;
		}
		return true;
	}
	if (route.size() == 2 && route[0] == "servers" && bPatch) {
		rRoute.params = body;
		if (!TryCopyEndpointToken(route[1], rRoute.params)) {
			rErrorCode = "INVALID_ARGUMENT";
			rErrorMessage = "server id must use address:port";
			return false;
		}
		rRoute.strCommand = "servers/update";
		return true;
	}
	if (route.size() == 2 && route[0] == "servers" && bDelete) {
		rRoute.strCommand = "servers/remove";
		rRoute.params = body;
		if (!TryCopyEndpointToken(route[1], rRoute.params)) {
			rErrorCode = "INVALID_ARGUMENT";
			rErrorMessage = "server id must use address:port";
			return false;
		}
		return true;
	}
	if (route.size() == 1 && route[0] == "kad" && bGet) {
		rRoute.strCommand = "kad/status";
		return true;
	}
	if (route.size() == 3 && route[0] == "kad" && route[1] == "operations" && bPost) {
		if (route[2] == "start") {
			rRoute.strCommand = "kad/connect";
			rRoute.params = body;
			return true;
		}
		if (route[2] == "bootstrap") {
			rRoute.strCommand = "kad/bootstrap";
			rRoute.params = body;
			return true;
		}
		if (route[2] == "stop") {
			rRoute.strCommand = "kad/disconnect";
			rRoute.params = body;
			return true;
		}
		if (route[2] == "recheck-firewall") {
			rRoute.strCommand = "kad/recheck_firewall";
			rRoute.params = body;
			return true;
		}
		rErrorCode = "NOT_FOUND";
		rErrorMessage = "API route not found";
		return false;
	}
	if (route.size() == 1 && route[0] == "shared-directories" && bGet) {
		rRoute.strCommand = "shared_directories/get";
		return true;
	}
	if (route.size() == 1 && route[0] == "shared-directories" && bPatch) {
		rRoute.strCommand = "shared_directories/set";
		rRoute.params = body;
		return true;
	}
	if (route.size() == 3 && route[0] == "shared-directories" && route[1] == "operations" && route[2] == "reload" && bPost) {
		rRoute.strCommand = "shared_directories/reload";
		return true;
	}
	if (route.size() == 1 && route[0] == "shared-files" && bGet) {
		rRoute.strCommand = "shared/list";
		CopyPagingQueryParams(query, rRoute.params);
		RequestItemsEnvelope(rRoute.params);
		return true;
	}
	if (route.size() == 1 && route[0] == "shared-files" && bPost) {
		rRoute.strCommand = "shared/add";
		rRoute.params = body;
		return true;
	}
	if (route.size() == 3 && route[0] == "shared-files" && route[1] == "operations" && route[2] == "reload" && bPost) {
		rRoute.strCommand = "shared_directories/reload";
		return true;
	}
	if (route.size() == 2 && route[0] == "shared-files" && bPatch) {
		rRoute.strCommand = "shared/set_rating_comment";
		rRoute.params = body;
		rRoute.params["hash"] = route[1];
		return true;
	}
	if (route.size() == 3 && route[0] == "shared-files" && route[2] == "ed2k-link" && bGet) {
		rRoute.strCommand = "shared/ed2k_link";
		rRoute.params["hash"] = route[1];
		return true;
	}
	if (route.size() == 3 && route[0] == "shared-files" && route[2] == "comments" && bGet) {
		rRoute.strCommand = "shared/comments";
		rRoute.params["hash"] = route[1];
		CopyPagingQueryParams(query, rRoute.params);
		RequestItemsEnvelope(rRoute.params);
		return true;
	}
	if (route.size() == 2 && route[0] == "shared-files" && bGet) {
		rRoute.strCommand = "shared/get";
		rRoute.params["hash"] = route[1];
		return true;
	}
	if (route.size() == 2 && route[0] == "shared-files" && bDelete) {
		rRoute.strCommand = "shared/remove";
		rRoute.params = body;
		rRoute.params["hash"] = route[1];
		return true;
	}
	if (route.size() == 1 && route[0] == "searches" && bPost) {
		rRoute.strCommand = "search/start";
		rRoute.params = body;
		return true;
	}
	if (route.size() == 1 && route[0] == "searches" && bDelete) {
		rRoute.strCommand = "search/clear";
		rRoute.params = body;
		return true;
	}
	if (route.size() == 6 && route[0] == "searches" && route[2] == "results" && route[4] == "operations" && route[5] == "download" && bPost) {
		rRoute.strCommand = "search/download_result";
		rRoute.params = body;
		rRoute.params["searchId"] = route[1];
		rRoute.params["hash"] = route[3];
		return true;
	}
	if (route.size() == 2 && route[0] == "searches" && bGet) {
		rRoute.strCommand = "search/results";
		rRoute.params["searchId"] = route[1];
		return true;
	}
	if (route.size() == 2 && route[0] == "searches" && bDelete) {
		rRoute.strCommand = "search/stop";
		rRoute.params = body;
		rRoute.params["searchId"] = route[1];
		return true;
	}
	if (route.size() == 1 && route[0] == "friends" && bGet) {
		rRoute.strCommand = "friends/list";
		CopyPagingQueryParams(query, rRoute.params);
		RequestItemsEnvelope(rRoute.params);
		return true;
	}
	if (route.size() == 1 && route[0] == "friends" && bPost) {
		rRoute.strCommand = "friends/add";
		rRoute.params = body;
		return true;
	}
	if (route.size() == 2 && route[0] == "friends" && bDelete) {
		rRoute.strCommand = "friends/remove";
		rRoute.params["userHash"] = route[1];
		return true;
	}
	if (route.size() == 1 && route[0] == "logs" && bGet) {
		rRoute.strCommand = "log/get";
		CopyLogQueryParams(query, rRoute.params);
		CopyPagingQueryParams(query, rRoute.params);
		RequestItemsEnvelope(rRoute.params);
		return true;
	}

	rErrorCode = "NOT_FOUND";
	rErrorMessage = "API route not found";
	return false;
}
}
