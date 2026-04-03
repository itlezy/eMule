#pragma once

#include "SearchParamsPolicy.h"
#include "SafeFile.h"

///////////////////////////////////////////////////////////////////////////////
// ESearchType

//NOTE: The numbers are *equal* to items position in the combo box -> TODO: use item data
enum ESearchType : uint8
{
	SearchTypeAutomatic = 0,
	SearchTypeEd2kServer,
	SearchTypeEd2kGlobal,
	SearchTypeKademlia
};


#define	MAX_SEARCH_EXPRESSION_LEN	512

///////////////////////////////////////////////////////////////////////////////
// SSearchParams

struct SSearchParams
{
	SSearchParams()
		: ullMinSize()
		, ullMaxSize()
		, dwSearchID(_UI32_MAX)
		, uAvailability()
		, uComplete()
		, uiMinBitrate()
		, uiMinLength()
		, eType(static_cast<ESearchType>(SearchParamsPolicy::kDefaultSearchType))
		, bClientSharedFiles()
		, bMatchKeywords()
	{
	}

	explicit SSearchParams(CFileDataIO &rFile)
		: ullMinSize()
		, ullMaxSize()
		, uAvailability()
		, uComplete()
		, uiMinBitrate()
		, uiMinLength()
		, bMatchKeywords()
	{
		dwSearchID = rFile.ReadUInt32();
		/**
		 * @brief Keep the legacy oracle branch aligned with the shared search-method normalization seam.
		 */
		eType = static_cast<ESearchType>(SearchParamsPolicy::NormalizeStoredSearchType(rFile.ReadUInt8()));
		bClientSharedFiles = rFile.ReadUInt8() > 0;
		strSpecialTitle = rFile.ReadString(true);
		strExpression = rFile.ReadString(true);
		strFileType = rFile.ReadString(true);
	}

	void StorePartially(CFileDataIO &rFile) const
	{
		rFile.WriteUInt32(dwSearchID);
		rFile.WriteUInt8(static_cast<uint8>(eType));
		rFile.WriteUInt8(static_cast<uint8>(bClientSharedFiles));
		rFile.WriteString(strSpecialTitle, UTF8strRaw);
		rFile.WriteString(strExpression, UTF8strRaw);
		rFile.WriteString(strFileType, UTF8strRaw);
	}

	CString strSearchTitle;
	CString strExpression;
	CStringW strKeyword;
	CString strBooleanExpr;
	CString strFileType;
	CString strMinSize;
	CString strMaxSize;
	CString strExtension;
	CString strCodec;
	CString strTitle;
	CString strAlbum;
	CString strArtist;
	CString strSpecialTitle;
	uint64 ullMinSize;
	uint64 ullMaxSize;
	uint32 dwSearchID;
	UINT uAvailability;
	UINT uComplete;
	UINT uiMinBitrate;
	UINT uiMinLength;
	ESearchType eType;
	bool bClientSharedFiles;
	bool bMatchKeywords;
};

bool GetSearchPacket(CSafeMemFile &data, SSearchParams *pParams, bool bTargetSupports64Bit, bool *pbPacketUsing64Bit);
