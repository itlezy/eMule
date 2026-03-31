//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

class CPartFile;

/**
 * Carries one UTF-8 JSON command line from the pipe worker thread into the UI
 * thread and returns the serialized UTF-8 response line.
 */
struct SPipeApiCommandContext
{
	CStringA strRequestLine;
	CStringA strResponseLine;
};

/**
 * Hosts the local named-pipe bridge that feeds the standalone remote API
 * process.
 */
class CPipeApiServer
{
public:
	CPipeApiServer();
	~CPipeApiServer();

	CPipeApiServer(const CPipeApiServer&) = delete;
	CPipeApiServer& operator=(const CPipeApiServer&) = delete;

	void Start();
	void Stop();

	bool IsConnected() const							{ return m_bConnected.load(); }

	LRESULT OnHandleCommand(WPARAM wParam, LPARAM lParam);
	void NotifyStatsUpdated(bool bForce = false);
	void NotifyDownloadAdded(const CPartFile *pPartFile);
	void NotifyDownloadRemoved(const CPartFile *pPartFile);
	void NotifyDownloadCompleted(const CPartFile *pPartFile, bool bSucceeded);
	void NotifyDownloadUpdated(const CPartFile *pPartFile);

private:
	void RunWorker();
	bool ProcessClient(HANDLE hPipe);
	bool ReadNextLine(HANDLE hPipe, CStringA &rLine);
	void DispatchCommandLine(const CStringA &rLine, CStringA &rResponseLine);
	bool WriteUtf8Line(HANDLE hPipe, const CStringA &rSerializedLine);
	bool WriteCurrentPipeLine(const CStringA &rSerializedLine);
	void DisconnectPipe();
	void WakePendingConnect() const;

	std::thread m_worker;
	std::atomic_bool m_bStopRequested;
	std::atomic_bool m_bConnected;
	std::mutex m_pipeMutex;
	std::mutex m_writeMutex;
	std::string m_strReadBuffer;
	HANDLE m_hPipe;
	DWORD m_dwLastStatsEventTick;
};

extern CPipeApiServer thePipeApiServer;
