//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.

#pragma once

namespace HelperThreadLaunchSeams
{
	enum class IocpShutdownAction
	{
		NoOp,
		WaitOnly,
		SignalAndWait
	};

	/**
	 * @brief Reports whether a helper-thread launch returned a live thread object.
	 */
	inline bool DidStartThread(const void *pThread)
	{
		return pThread != nullptr;
	}

	/**
	 * @brief Classifies shutdown for workers that are controlled through an IOCP.
	 */
	inline IocpShutdownAction ClassifyIocpShutdown(bool bThreadStarted, bool bPortReady)
	{
		if (!bThreadStarted)
			return IocpShutdownAction::NoOp;
		return bPortReady ? IocpShutdownAction::SignalAndWait : IocpShutdownAction::WaitOnly;
	}

	/**
	 * @brief Reports whether an IOCP helper can accept wake or file work.
	 */
	inline bool CanPostIocpWork(bool bThreadStarted, bool bStopRequested, bool bPortReady, bool bWorkerRunning)
	{
		return bThreadStarted && !bStopRequested && bPortReady && bWorkerRunning;
	}

	/**
	 * @brief Reports whether an event-driven helper should be waited during shutdown.
	 */
	inline bool ShouldWaitForEventThreadShutdown(bool bThreadStarted)
	{
		return bThreadStarted;
	}
}
