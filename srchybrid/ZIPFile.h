//
// ZIPFile.h
//
// Copyright (c) Shareaza Development Team, 2002-2004.
// This file is part of SHAREAZA (www.shareaza.com)
//
// Shareaza is free software; you can redistribute it
// and/or modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Shareaza is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Shareaza; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#pragma once

class CBuffer;


class CZIPFile
{
// Construction
public:
	explicit CZIPFile(HANDLE hAttach = INVALID_HANDLE_VALUE);
	~CZIPFile();

// File Class
public:
	class File
	{
		friend class CZIPFile;

		inline File() = default;
		CZIPFile *m_pZIP;
	public:
		//CBuffer*	Decompress();
		bool	Extract(LPCTSTR pszFile);
		CString	m_sName;
		uint64	m_nSize;
	protected:
		bool	PrepareToDecompress(LPVOID pStream);
		uint64	m_nLocalOffset;
		uint64	m_nCompressedSize;
		int		m_nCompression;
	};

// Attributes
protected:
	HANDLE	m_hFile;
	File	*m_pFile;
	int		m_nFile;
	bool	m_bAttach;

// Operations
public:
	bool	Open(LPCTSTR pszFile);
	bool	Attach(HANDLE hFile);
	bool	IsOpen() const;
	void	Close();
public:
	int		GetCount() const						{ return m_nFile; } //get the file count
	File*	GetFile(int nFile) const;
	File*	GetFile(LPCTSTR pszFile, BOOL bPartial = FALSE) const;
protected:
	bool	LocateCentralDirectory();
	bool	ParseCentralDirectory(BYTE *pDirectory, DWORD nDirectory);
	bool	SeekToFile(const File *pFile);
};
