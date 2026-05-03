#pragma once

#include "WebServer.h"

/**
 * @brief Runtime web-server entry points for the eMule BB *arr compatibility bridge.
 */
namespace WebServerArrCompat
{
/**
 * @brief Reports whether the request belongs to the eMule BB *arr compatibility surface.
 */
bool IsCompatRequest(const ThreadData &rData);

/**
 * @brief Handles one authenticated Torznab-compatible Prowlarr request.
 */
void ProcessRequest(const ThreadData &rData);
}
