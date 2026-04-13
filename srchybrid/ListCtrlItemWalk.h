#pragma once

class CMuleListCtrl;

class CListCtrlItemWalk
{
public:
	explicit CListCtrlItemWalk(CMuleListCtrl *pListCtrl)	{ m_pListCtrl = pListCtrl; }

	virtual CObject* GetNextSelectableItem();
	virtual CObject* GetPrevSelectableItem();

	CListCtrl* GetListCtrl() const;

protected:
	virtual	~CListCtrlItemWalk() = default;
	CMuleListCtrl *m_pListCtrl;
};
