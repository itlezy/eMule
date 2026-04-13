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
#include "stdafx.h"
#include "ShareableFile.h"
#include "emule.h"
#include "otherfunctions.h"
#include "PathHelpers.h"

IMPLEMENT_DYNAMIC(CShareableFile, CAbstractFile)

CShareableFile::CShareableFile()
	: m_verifiedFileType(FILETYPE_UNKNOWN)
{
}

CString CShareableFile::GetInfoSummary(bool bNoFormatCommands) const
{
	CString strFolder(PathHelpers::TrimTrailingSeparator(GetPath()));

	CString strType(GetFileTypeDisplayStr());
	if (strType.IsEmpty())
		strType += _T('-');

	CString info(GetFileName());
	info.AppendFormat(_T("\n")
		_T("%s %s\n")
		_T("%s\n")
		_T("%s: %s\n")
		_T("%s: %s")
		, (LPCTSTR)GetResString(IDS_FD_SIZE), (LPCTSTR)CastItoXBytes((uint64)GetFileSize())
		, bNoFormatCommands ? _T("") : _T("<br_head>")
		, (LPCTSTR)GetResString(IDS_TYPE), (LPCTSTR)strType
		, (LPCTSTR)GetResString(IDS_FOLDER), (LPCTSTR)strFolder
	);
	return info;
}
