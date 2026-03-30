//this file is part of eMule
//Copyright (C)2024-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
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

#ifndef ASSERT
#include <cassert>
#define ASSERT(expression) assert(expression)
#endif

typedef struct {
	uint64	datalen;
	DWORD	timestamp;
} TransferredData;

template<class TYPE> class CRing
{
	UINT_PTR m_nCount;		//the number of added items
	UINT_PTR m_nIncrement;	//increase capacity by this number of items
	UINT_PTR m_nSize;		//buffer capacity
	TYPE *m_pData;			//the buffer
	UINT_PTR m_nHead;		//the oldest item (to be extracted first)

	/** Reallocates the backing buffer while preserving logical item order. */
	void SetBuffer(UINT_PTR nSize);
	/** Returns the backing-buffer slot for a logical ring index. */
	UINT_PTR GetPhysicalIndex(UINT_PTR index) const;
public:
	explicit CRing(UINT_PTR nSize = 128, UINT_PTR nIncrement = 128); //zero values default to 128
	~CRing()								{ delete[] m_pData; }
	const TYPE& operator [](UINT_PTR index) const	{ ASSERT(index < m_nCount); return m_pData[GetPhysicalIndex(index)]; }

	void AddTail(const TYPE &newElement);
	UINT_PTR Capacity() const				{ return m_nSize; }
	UINT_PTR Count() const					{ return m_nCount; }
	const TYPE& Head() const				{ ASSERT(m_nCount > 0); return m_pData[m_nHead]; }
	const TYPE& Tail() const				{ ASSERT(m_nCount > 0); return m_pData[GetPhysicalIndex(m_nCount - 1)]; }
	bool IsEmpty() const					{ return !m_nCount; }
	void RemoveAll();
	void RemoveHead();
	void SetCapacity(UINT_PTR nSize);
	void SetIncrement(UINT_PTR nIncrement)	{ m_nIncrement = nIncrement ? nIncrement : 128; }
};

template<class TYPE>
CRing<TYPE>::CRing(UINT_PTR nSize, UINT_PTR nIncrement)
	: m_nCount()
	, m_nIncrement(nIncrement ? nIncrement : 128)
	, m_nSize(nSize ? nSize : 128)
	, m_pData()
	, m_nHead()
{
	SetBuffer(m_nSize);
}

template<class TYPE>
UINT_PTR CRing<TYPE>::GetPhysicalIndex(UINT_PTR index) const
{
	return (m_nHead + index) % m_nSize;
}

template<class TYPE>
void CRing<TYPE>::AddTail(const TYPE &newElement)
{
	if (m_nCount >= m_nSize)
		SetCapacity(m_nSize + m_nIncrement);
	m_pData[GetPhysicalIndex(m_nCount)] = newElement;
	++m_nCount;
}

template<class TYPE>
void CRing<TYPE>::RemoveAll()
{
	m_nCount = 0;
	m_nHead = 0;
}

template<class TYPE>
void CRing<TYPE>::RemoveHead()
{
	if (m_nCount) {
		--m_nCount;
		if (m_nCount == 0)
			m_nHead = 0;
		else
			m_nHead = (m_nHead + 1) % m_nSize;
	}
}

template<class TYPE>
void CRing<TYPE>::SetBuffer(UINT_PTR nSize)
{
	TYPE *dst = new TYPE[nSize];
	for (UINT_PTR index = 0; index < m_nCount; ++index)
		dst[index] = m_pData[GetPhysicalIndex(index)];
	delete[] m_pData;
	m_nSize = nSize;
	m_pData = dst;
	m_nHead = 0;
}

template<class TYPE>
void CRing<TYPE>::SetCapacity(UINT_PTR nSize)
{
	if (nSize > m_nSize)
		SetBuffer(nSize);
}
