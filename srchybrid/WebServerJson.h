#pragma once

#include "WebServer.h"

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
 * @brief Executes one synchronous REST command dispatch context on the UI
 * thread.
 */
void RunDispatchedCommand(void *pContext);
}
