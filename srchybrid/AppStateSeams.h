#pragma once

#include <cstdint>

enum AppState : uint8_t
{
	APP_STATE_STARTING = 0,	// initialization phase
	APP_STATE_RUNNING,
	APP_STATE_ASKCLOSE,		// exit dialog is on screen
	APP_STATE_SHUTTINGDOWN,
	APP_STATE_DONE			// shutdown has completed
};

/**
 * @brief Reports whether the current application state should still be treated as running.
 */
inline bool IsAppStateRunning(const AppState eAppState)
{
	return eAppState == APP_STATE_RUNNING || eAppState == APP_STATE_ASKCLOSE;
}

/**
 * @brief Reports whether the current application state has entered shutdown or finished closing.
 */
inline bool IsAppStateClosing(const AppState eAppState)
{
	return eAppState == APP_STATE_SHUTTINGDOWN || eAppState == APP_STATE_DONE;
}
