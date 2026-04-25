#pragma once

class CPerfLog
{
public:
	// those values have to be specified in 'preferences.ini' -> hardcode them
	enum ELogMode : uint8
	{
		None = 0,
		OneSample = 1,
		AllSamples = 2
	};
	// those values have to be specified in 'preferences.ini' -> hardcode them
	enum ELogFileFormat : uint8
	{
		CSV = 0,
		MRTG = 1
	};

	CPerfLog();

	void Startup();
	void Shutdown();
	void LogSamples();
	/**
	 * Reloads the hidden PerfLog settings from preferences.ini.
	 */
	void ReloadSettings();
	/**
	 * Persists the hidden PerfLog mode and applies it immediately.
	 */
	void SetEnabled(bool bEnable);
	/**
	 * Persists the PerfLog output settings and applies them immediately.
	 */
	void SetSettings(bool bEnable, int iFileFormat, const CString &strFilePath, UINT uIntervalMinutes);
	/**
	 * Returns whether PerfLog is currently active.
	 */
	bool IsEnabled() const;
	static int GetConfiguredFileFormat();
	static CString GetConfiguredFilePath();
	static UINT GetConfiguredIntervalMinutes();

protected:
	DWORD	m_dwInterval;
	ULONGLONG m_dwLastSampled;
	CString	m_strFilePath;
	CString	m_strMRTGDataFilePath;
	CString	m_strMRTGOverheadFilePath;
	uint64	m_nLastSessionSentBytes;
	uint64	m_nLastSessionRecvBytes;
	uint64	m_nLastDnOH;
	uint64	m_nLastUpOH;
	ELogMode m_eMode;
	ELogFileFormat m_eFileFormat;

	bool m_bInitialized;

	/**
	 * Loads the hidden PerfLog settings from preferences.ini into runtime state.
	 */
	void LoadSettings();
	/**
	 * Resets the sampling baseline so logging starts from the current session counters.
	 */
	void ResetSampleBaseline();
	void WriteSamples(UINT nCurDn, UINT nCurUp, UINT nCurDnOH, UINT nCurUpOH);
};

extern CPerfLog thePerfLog;
