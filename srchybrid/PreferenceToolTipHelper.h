#pragma once

#include <map>

/**
 * @brief Small helper for property-page control tooltips in the Preferences dialog.
 *
 * The helper owns the tooltip window and the backing tooltip strings so pages can
 * register explanatory help text without keeping separate string storage alive.
 */
class CPreferenceToolTipHelper
{
public:
	/**
	 * @brief Creates the tooltip window for the given property page if needed.
	 */
	bool Init(CWnd *pOwner)
	{
		if (m_toolTip.GetSafeHwnd() != NULL)
			return true;
		m_registeredIds.clear();
		if (pOwner == NULL || !m_toolTip.Create(pOwner, TTS_ALWAYSTIP | TTS_NOPREFIX))
			return false;
		m_toolTip.SetMaxTipWidth(420);
		m_toolTip.Activate(TRUE);
		return true;
	}

	/**
	 * @brief Adds or updates the tooltip text for a dialog control.
	 */
	void SetTool(CWnd *pOwner, UINT nCtrlId, const CString &text)
	{
		if (pOwner == NULL || m_toolTip.GetSafeHwnd() == NULL)
			return;

		CWnd *pCtrl = pOwner->GetDlgItem(nCtrlId);
		if (pCtrl == NULL || pCtrl->GetSafeHwnd() == NULL)
			return;

		m_textById[nCtrlId] = text;
		CString &storedText = m_textById[nCtrlId];
		if (m_registeredIds.find(nCtrlId) == m_registeredIds.end()) {
			m_toolTip.AddTool(pCtrl, storedText);
			m_registeredIds[nCtrlId] = true;
		} else
			m_toolTip.UpdateTipText(storedText, pCtrl);
	}

	/**
	 * @brief Relays mouse messages so the tooltip control can track hover state.
	 */
	void RelayEvent(MSG *pMsg)
	{
		if (m_toolTip.GetSafeHwnd() != NULL && pMsg != NULL)
			m_toolTip.RelayEvent(pMsg);
	}

private:
	CToolTipCtrl m_toolTip;
	std::map<UINT, CString> m_textById;
	std::map<UINT, bool> m_registeredIds;
};
