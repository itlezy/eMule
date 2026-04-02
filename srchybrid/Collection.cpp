//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( devs@emule-project.net / https://www.emule-project.net )
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
#include "StdAfx.h"
#include "collection.h"
#include "KnownFile.h"
#include "CollectionFile.h"
#include "SafeFile.h"
#include "Packets.h"
#include "Preferences.h"
#include "SharedFilelist.h"
#include "emule.h"
#include "Log.h"
#include "md5sum.h"
#include "CollectionSeams.h"
#include <memory>
#include <vector>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define COLLECTION_FILE_VERSION1_INITIAL		0x01
#define COLLECTION_FILE_VERSION2_LARGEFILES		0x02

CCollection::CCollection()
	: m_bTextFormat()
	, m_nKeySize()
	, m_pabyCollectionAuthorKey()
{
	m_CollectionFilesMap.InitHashTable(1031);
	m_sCollectionName.Format(_T("New Collection-%u"), ::GetTickCount());
}

CCollection::CCollection(const CCollection *pCollection)
	: m_sCollectionName(pCollection->m_sCollectionName)
	, m_bTextFormat(pCollection->m_bTextFormat)
{
	if (pCollection->m_pabyCollectionAuthorKey != NULL) {
		m_nKeySize = pCollection->m_nKeySize;
		m_pabyCollectionAuthorKey = new BYTE[m_nKeySize];
		memcpy(m_pabyCollectionAuthorKey, pCollection->m_pabyCollectionAuthorKey, m_nKeySize);
		m_sCollectionAuthorName = pCollection->m_sCollectionAuthorName;
	} else {
		m_nKeySize = 0;
		m_pabyCollectionAuthorKey = NULL;
	}

	m_CollectionFilesMap.InitHashTable(1031);
	for (const CCollectionFilesMap::CPair *pair = pCollection->m_CollectionFilesMap.PGetFirstAssoc(); pair != NULL; pair = pCollection->m_CollectionFilesMap.PGetNextAssoc(pair))
		AddFileToCollection(pair->value, true);
}

CCollection::~CCollection()
{
	delete[] m_pabyCollectionAuthorKey;
	CSKey key;
	for (POSITION pos = m_CollectionFilesMap.GetStartPosition(); pos != NULL;) {
		CCollectionFile *pCollectionFile;
		m_CollectionFilesMap.GetNextAssoc(pos, key, pCollectionFile);
		delete pCollectionFile;
	}
}

CCollectionFile* CCollection::AddFileToCollection(CAbstractFile *pAbstractFile, bool bCreateClone)
{
	CSKey key(pAbstractFile->GetFileHash());
	CCollectionFile *pCollectionFile;
	if (m_CollectionFilesMap.Lookup(key, pCollectionFile)) {
		ASSERT(0);
		return pCollectionFile;
	}

	if (bCreateClone)
		pCollectionFile = new CCollectionFile(pAbstractFile);
	else if (pAbstractFile->IsKindOf(RUNTIME_CLASS(CCollectionFile)))
		pCollectionFile = static_cast<CCollectionFile*>(pAbstractFile);
	else
		pCollectionFile = NULL;

	if (pCollectionFile)
		m_CollectionFilesMap[key] = pCollectionFile;

	return pCollectionFile;
}

void CCollection::RemoveFileFromCollection(const CAbstractFile *pAbstractFile)
{
	CSKey key(pAbstractFile->GetFileHash());
	CCollectionFile *pCollectionFile;
	if (m_CollectionFilesMap.Lookup(key, pCollectionFile)) {
		m_CollectionFilesMap.RemoveKey(key);
		delete pCollectionFile;
	} else
		ASSERT(0);
}

void CCollection::SetCollectionAuthorKey(const byte *abyCollectionAuthorKey, uint32 nSize)
{
	delete[] m_pabyCollectionAuthorKey;
	m_pabyCollectionAuthorKey = NULL;
	m_nKeySize = 0;
	if (abyCollectionAuthorKey != NULL) {
		m_pabyCollectionAuthorKey = new BYTE[nSize];
		memcpy(m_pabyCollectionAuthorKey, abyCollectionAuthorKey, nSize);
		m_nKeySize = nSize;
	}
}

bool CCollection::InitCollectionFromFile(const CString &sFilePath, const CString &sFileName)
{
	CSafeFile data;
	if (!data.Open(sFilePath, CFile::modeRead | CFile::shareDenyWrite | CFile::typeBinary))
		return false;
	bool bCollectionLoaded = false;
	try {
		uint32 nVersion = data.ReadUInt32();
		if (nVersion == COLLECTION_FILE_VERSION1_INITIAL || nVersion == COLLECTION_FILE_VERSION2_LARGEFILES) {
			for (uint32 headerTagCount = data.ReadUInt32(); headerTagCount > 0; --headerTagCount) {
				CTag tag(data, true);
				switch (tag.GetNameID()) {
				case FT_FILENAME:
					if (tag.IsStr())
						m_sCollectionName = tag.GetStr();
					break;
				case FT_COLLECTIONAUTHOR:
					if (tag.IsStr())
						m_sCollectionAuthorName = tag.GetStr();
					break;
				case FT_COLLECTIONAUTHORKEY:
					if (tag.IsBlob())
						SetCollectionAuthorKey(tag.GetBlob(), tag.GetBlobSize());
				}
			}
			for (uint32 fileCount = data.ReadUInt32(); fileCount > 0; --fileCount)
				try {
					/** Keep malformed individual collection entries from aborting the whole import. */
					std::unique_ptr<CCollectionFile> pCollectionFile(new CCollectionFile(data));
					if (AddFileToCollection(pCollectionFile.get(), false) != NULL)
						pCollectionFile.release();
				} catch (CException *ex) {
					ex->Delete();
					ASSERT(0);
					if (!ShouldContinueAfterCollectionEntryFailure())
						return false;
				}

			bCollectionLoaded = true;
		}
		if (m_pabyCollectionAuthorKey != NULL) {
			bool bResult = false;
			if (data.GetLength() > data.GetPosition()) {
				using namespace CryptoPP;

				CollectionSignatureLayout layout = {};
				if (!TryBuildCollectionSignatureLayout(data.GetLength(), data.GetPosition(), layout))
					return false;
				data.SeekToBegin();
				std::vector<BYTE> abyMessage(layout.nMessageLength);
				VERIFY(data.Read(abyMessage.data(), layout.nMessageLength) == layout.nMessageLength);

				StringSource ss_Pubkey(m_pabyCollectionAuthorKey, m_nKeySize, true, 0);
				RSASSA_PKCS1v15_SHA_Verifier pubkey(ss_Pubkey);

				std::vector<BYTE> abySignature(layout.nSignatureLength);
				VERIFY(data.Read(abySignature.data(), layout.nSignatureLength) == layout.nSignatureLength);

				bResult = pubkey.VerifyMessage(abyMessage.data(), layout.nMessageLength, abySignature.data(), layout.nSignatureLength);
			}
			if (!bResult) {
				DebugLogWarning(_T("Collection %s: Verification of public key failed!"), (LPCTSTR)m_sCollectionName);
				delete[] m_pabyCollectionAuthorKey;
				m_pabyCollectionAuthorKey = NULL;
				m_nKeySize = 0;
				m_sCollectionAuthorName.Empty();
			} else
				DebugLog(_T("Collection %s: Public key verified"), (LPCTSTR)m_sCollectionName);

		} else
			m_sCollectionAuthorName.Empty();
		data.Close();
	} catch (CException *ex) {
		ex->Delete();
		return false;
	} catch (const CryptoPP::Exception &e) {
		AddDebugLogLine(false, _T("Failed to load collection %s: %hs"), (LPCTSTR)sFileName, e.what());
		return false;
	} catch (const std::exception &e) {
		AddDebugLogLine(false, _T("Failed to load collection %s: %hs"), (LPCTSTR)sFileName, e.what());
		return false;
	}

	if (!bCollectionLoaded) {
		CStdioFile data1;
		if (data1.Open(sFilePath, CFile::modeRead | CFile::shareDenyWrite | CFile::typeText)) {
			try {
				CString sLink;
				while (data1.ReadString(sLink)) {
					//Ignore all lines that start with #.
					//These lines can be used for future features.
					if (sLink.Find(_T('#')) != 0) {
						try {
							/** Keep malformed individual text entries from aborting the whole import. */
							std::unique_ptr<CCollectionFile> pCollectionFile(new CCollectionFile());
							if (pCollectionFile->InitFromLink(sLink) && AddFileToCollection(pCollectionFile.get(), false) != NULL)
								pCollectionFile.release();
						} catch (CException *ex) {
							ex->Delete();
							ASSERT(0);
							if (!ShouldContinueAfterCollectionEntryFailure())
								return false;
						}
					}
				}
				data1.Close();
				//No collection name tag; use file name without extension
				int iLen = sFileName.GetLength();
				if (HasCollectionExtention(sFileName))
					iLen -= _countof(COLLECTION_FILEEXTENSION) - 1;
				m_sCollectionName = sFileName.Left(iLen);
				m_bTextFormat = true;
				return true;
			} catch (CException *ex) {
				ex->Delete();
			} catch (const std::exception &e) {
				AddDebugLogLine(false, _T("Failed to load text collection %s: %hs"), (LPCTSTR)sFileName, e.what());
			}
		}
	}
	return bCollectionLoaded;
}

void CCollection::WriteToFileAddShared(CryptoPP::RSASSA_PKCS1v15_SHA_Signer *pSignKey)
{
	using namespace CryptoPP;

	CString sFilePath(thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR));
	sFilePath.AppendFormat(_T("%s%s"), (LPCTSTR)m_sCollectionName, COLLECTION_FILEEXTENSION);

	if (m_bTextFormat) {
		CStdioFile data;
		if (data.Open(sFilePath, CFile::modeCreate | CFile::modeWrite | CFile::shareDenyWrite | CFile::typeText)) {
			try {
				for (const CCollectionFilesMap::CPair *pair = m_CollectionFilesMap.PGetFirstAssoc(); pair != NULL; pair = m_CollectionFilesMap.PGetNextAssoc(pair))
					if (pair->value)
						data.WriteString(pair->value->GetED2kLink() + _T('\n'));

				data.Close();
			} catch (CFileException *ex) {
				ex->Delete();
				return;
			} catch (...) {
				ASSERT(0);
				return;
			}
		}
	} else {
		CSafeFile data;
		if (data.Open(sFilePath, CFile::modeCreate | CFile::modeReadWrite | CFile::shareDenyWrite | CFile::typeBinary)) {
			try {
				//Version
				// check first if we have any large files in the map - write use lowest version possible
				uint32 dwVersion = COLLECTION_FILE_VERSION1_INITIAL;
				for (const CCollectionFilesMap::CPair *pair = m_CollectionFilesMap.PGetFirstAssoc(); pair != NULL; pair = m_CollectionFilesMap.PGetNextAssoc(pair))
					if (pair->value->IsLargeFile()) {
						dwVersion = COLLECTION_FILE_VERSION2_LARGEFILES;
						break;
					}

				data.WriteUInt32(dwVersion);
				//NumberHeaderTags
				data.WriteUInt32(m_pabyCollectionAuthorKey ? 3 : 1);

				CTag collectionName(FT_FILENAME, m_sCollectionName);
				collectionName.WriteTagToFile(data, UTF8strRaw);

				if (m_pabyCollectionAuthorKey != NULL) {
					CTag collectionAuthor(FT_COLLECTIONAUTHOR, m_sCollectionAuthorName);
					collectionAuthor.WriteTagToFile(data, UTF8strRaw);

					CTag collectionAuthorKey(FT_COLLECTIONAUTHORKEY, m_nKeySize, m_pabyCollectionAuthorKey);
					collectionAuthorKey.WriteTagToFile(data, UTF8strRaw);
				}

				//Total Files
				data.WriteUInt32((uint32)m_CollectionFilesMap.GetCount());

				for (const CCollectionFilesMap::CPair *pair = m_CollectionFilesMap.PGetFirstAssoc(); pair != NULL; pair = m_CollectionFilesMap.PGetNextAssoc(pair))
					pair->value->WriteCollectionInfo(data);

				if (pSignKey != NULL) {
					uint32 nPos = 0;
					if (!TryConvertCollectionSerializedLength(data.GetPosition(), nPos)) {
						ASSERT(0);
						return;
					}
					data.SeekToBegin();
					std::vector<BYTE> abyMessage(nPos);
					VERIFY(data.Read(abyMessage.data(), nPos) == nPos);

					SecByteBlock sbbSignature(pSignKey->SignatureLength());
					AutoSeededRandomPool rng;
					pSignKey->SignMessage(rng, abyMessage.data(), nPos, sbbSignature.begin());
					std::vector<BYTE> abySignature(sbbSignature.size());
					ArraySink asink(abySignature.data(), abySignature.size());
					asink.Put(sbbSignature.begin(), sbbSignature.size());
					data.Write(abySignature.data(), (UINT)asink.TotalPutLength());
				}
				data.Close();
			} catch (CFileException *ex) {
				ex->Delete();
				return;
			} catch (const CryptoPP::Exception &e) {
				AddDebugLogLine(false, _T("Failed to write collection %s: %hs"), (LPCTSTR)m_sCollectionName, e.what());
				return;
			} catch (const std::exception &e) {
				AddDebugLogLine(false, _T("Failed to write collection %s: %hs"), (LPCTSTR)m_sCollectionName, e.what());
				return;
			}
		}
	}

	theApp.sharedfiles->AddFileFromNewlyCreatedCollection(sFilePath);
}

bool CCollection::HasCollectionExtention(const CString &sFileName)
{
	return ExtensionIs(sFileName, COLLECTION_FILEEXTENSION);
}

CString CCollection::GetCollectionAuthorKeyString()
{
	return m_pabyCollectionAuthorKey ? EncodeBase16(m_pabyCollectionAuthorKey, m_nKeySize) : CString();
}

CString CCollection::GetAuthorKeyHashString() const
{
	if (m_pabyCollectionAuthorKey == NULL)
		return CString();
	MD5Sum md5(m_pabyCollectionAuthorKey, m_nKeySize);
	return md5.GetHashString().MakeUpper();
}
