#pragma once

#include "KadSupport.h"
#include "PartFilePersistenceSeams.h"

namespace Kademlia
{
	/**
	 * @brief Describes a parsed `nodes.dat` snapshot without mutating the live routing table.
	 */
	struct NodesDatFileInfo
	{
		NodesDatFileInfo();

		uint32 m_uUsableContacts;
		bool m_bBootstrapOnly;
	};

	/**
	 * @brief Parses a `nodes.dat` candidate and counts usable contacts.
	 */
	bool InspectNodesDatFile(LPCTSTR pszFilename, NodesDatFileInfo &rInfo);

	/**
	 * @brief Atomically replaces the persisted `nodes.dat` with a validated candidate file.
	 */
	bool ReplaceNodesDatFile(LPCTSTR pszSourceFilename, LPCTSTR pszTargetFilename);

	/**
	 * @brief Atomically replaces the persisted `nodes.dat` through injectable filesystem operations.
	 */
	bool ReplaceNodesDatFileWithOps(LPCTSTR pszSourceFilename, LPCTSTR pszTargetFilename, DWORD *pdwLastError, const PartFilePersistenceSeams::FileSystemOps &rOps);

	/**
	 * @brief Validates and promotes a prepared `nodes.dat` candidate.
	 */
	bool InstallPreparedNodesDatFile(LPCTSTR pszSourceFilename, LPCTSTR pszTargetFilename, DWORD *pdwLastError = NULL);
}
