#pragma once

#include "WebServer.h"

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

namespace WebServerJson
{
/**
 * @brief Reports whether the current request target belongs to the REST surface.
 */
bool IsApiRequest(const ThreadData &rData);

/**
 * @brief Handles one authenticated `/api/v1/...` request and writes the JSON response.
 */
void ProcessRequest(const ThreadData &rData);

/**
 * @brief Executes one already-authenticated in-process REST command for a
 * compatibility surface that needs to reuse the native UI command pipeline.
 */
bool ExecuteInternalCommand(const nlohmann::json &rRequest, nlohmann::json &rResult, CStringA &rErrorCode, CString &rErrorMessage);

/**
 * @brief Executes one synchronous REST command dispatch context on the UI
 * thread.
 */
void RunDispatchedCommand(void *pContext);
}
