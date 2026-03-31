# Defects

## Table of Contents

- [Definite Bugs](#definite-bugs)
- [Load-Only Hidden Or Legacy Prefs](#load-only-hidden-or-legacy-prefs)
- [Follow-Up: Purpose And Current Use](#follow-up-purpose-and-current-use)
- [Not Counted As Bugs](#not-counted-as-bugs)

## Preferences Persistence Audit

Date: 2026-03-24

### Definite Bugs

- **`BUG_010`** ~~Upload overhead totals do not round-trip because the saved and loaded INI keys do not match.~~ **[DONE]**
  - The load path now uses the same canonical keys as the save path:
    `UpOverheadTotal` and `UpOverheadTotalPackets`.
  - Effect: upload-overhead byte and packet totals round-trip correctly again.

- **`BUG_011`** ~~Web server allowed IPs are load-only.~~ **[STALE — WebServer removed]**
  - The embedded web server was fully removed (commit `6a1c440`). The `AllowedIPs` preference no longer exists.

### Load-Only Hidden Or Legacy Prefs

These keys are read from `preferences.ini` and used at runtime, but no write-back path was found in `srchybrid/Preferences.cpp`:

- `RestoreLastMainWndDlg`
- `RestoreLastLogPane`
- `FileBufferTimeLimit`
- `DateTimeFormat4Lists`
- `PreviewCopiedArchives`
- `InspectAllFileTypes`
- `PreviewOnIconDblClk`
- `ShowActiveDownloadsBold`
- `UseSystemFontForMainControls`
- `ReBarToolbar`
- `ShowUpDownIconInTaskbar`
- `ShowVerticalHourMarkers`
- `ForceSpeedsToKB`
- `ExtraPreviewWithMenu`
- `KeepUnavailableFixedSharedDirs`
- `PartiallyPurgeOldKnownFiles`
- `AdjustNTFSDaylightFileTime`
- `RearrangeKadSearchKeywords`
- `AICHTrustEveryHash`

These may be stale hidden knobs, migration leftovers, or genuine persistence omissions. They need a follow-up decision:

- restore write-back support
- formally retire them
- or document them as import-only

### Follow-Up: Purpose And Current Use

None of the prefs listed below have direct hits in the `PPg*.cpp` / `PPg*.h` preference-page code, so they are not exposed in the Preferences dialogs.

#### Hidden Prefs Still Used At Runtime

- `RestoreLastMainWndDlg`
  - Purpose: restore the last active main window page on startup.
  - Runtime use: `srchybrid/EmuleDlg.cpp`.

- `RestoreLastLogPane`
  - Purpose: restore the previously selected log pane.
  - Runtime use: `srchybrid/ServerWnd.cpp`.

- `FileBufferTimeLimit`
  - Purpose: maximum age of buffered part-file data before a forced flush.
  - Runtime use: `srchybrid/PartFile.cpp`.

- `DateTimeFormat4Lists`
  - Purpose: separate date/time formatting string for list-view columns.
  - Runtime use: `srchybrid/DownloadListCtrl.cpp`.

- `PreviewCopiedArchives`
  - Purpose: allow preview/archive-recovery logic to work on copied archives.
  - Runtime use: `srchybrid/ArchivePreviewDlg.cpp`, `srchybrid/PartFile.cpp`.

- `InspectAllFileTypes`
  - Purpose: allow file-info inspection/metadata probing for all file types instead of only media-oriented types.
  - Runtime use: `srchybrid/FileInfoDialog.cpp`.

- `PreviewOnIconDblClk`
  - Purpose: double-clicking the icon area in the download list starts preview behavior.
  - Runtime use: `srchybrid/DownloadListCtrl.cpp`.

- `ShowActiveDownloadsBold`
  - Purpose: render active downloads in bold in the download list.
  - Runtime use: `srchybrid/DownloadListCtrl.cpp`.

- `UseSystemFontForMainControls`
  - Purpose: use system UI fonts for major list/window controls.
  - Runtime use: `srchybrid/DownloadListCtrl.cpp`, `srchybrid/MuleListCtrl.cpp`, `srchybrid/ListCtrlX.cpp`, `srchybrid/SharedFilesWnd.cpp`, `srchybrid/StatisticsDlg.cpp`.

- `ReBarToolbar`
  - Purpose: enable the rebar-style toolbar layout.
  - Runtime use: `srchybrid/EmuleDlg.cpp`, `srchybrid/MuleToolBarCtrl.cpp`.

- `ShowUpDownIconInTaskbar`
  - Purpose: show upload/download activity icon state in the taskbar.
  - Runtime use: `srchybrid/EmuleDlg.cpp`.

- `ShowVerticalHourMarkers`
  - Purpose: draw vertical hour markers on the statistics graphs.
  - Runtime use: `srchybrid/OScopeCtrl.cpp`.

- `ForceSpeedsToKB`
  - Purpose: force speed formatting to KiB/s-oriented units instead of byte-level display for small rates.
  - Runtime use: `srchybrid/OtherFunctions.cpp`.

- `ExtraPreviewWithMenu`
  - Purpose: alter where preview actions are surfaced in the download-list UI/menu flow.
  - Runtime use: `srchybrid/DownloadListCtrl.cpp`.

- `KeepUnavailableFixedSharedDirs`
  - Purpose: preserve configured fixed shared directories even when they are temporarily unavailable while loading preferences/shared dirs.
  - Runtime use: startup/shared-dir loading path in `srchybrid/Preferences.cpp`.

- `PartiallyPurgeOldKnownFiles`
  - Purpose: keep the known-files/AICH cleanup logic from fully purging old entries.
  - Runtime use: `srchybrid/AICHSyncThread.cpp`, `srchybrid/KnownFile.cpp`.

- `AdjustNTFSDaylightFileTime`
  - Purpose: compensate NTFS timestamps around daylight-saving switches to avoid false file-change/rehash churn.
  - Runtime use: `srchybrid/OtherFunctions.cpp`, called from `srchybrid/KnownFile.cpp`, `srchybrid/PartFile.cpp`, `srchybrid/SharedFileList.cpp`, `srchybrid/SharedFilesCtrl.cpp`.

- `RearrangeKadSearchKeywords`
  - Purpose: reorder Kad search keywords before issuing Kad searches.
  - Runtime use: `srchybrid/SearchResultsWnd.cpp`.

#### Additional Hidden Load-Only Pref Found During Follow-Up

- `MessageFromValidSourcesOnly`
  - Purpose: restrict chat/message acceptance to clients considered valid enough by the messaging gate.
  - Runtime use: `srchybrid/BaseClient.cpp` via `thePrefs.MsgOnlySecure()`.
  - Status: load-only, not exposed in the Preferences dialogs, and currently missing from the original audit list above.

#### Prefs Which Currently Look Dead Or Import-Only

- `AICHTrustEveryHash`
  - Purpose: intended to relax AICH trust rules.
  - Current status: loaded into `m_bTrustEveryHash`, but no non-`Preferences.*` runtime use was found in this audit.
  - Likely classification: stale hidden knob or unfinished feature.

### Not Counted As Bugs

- `UploadCapacity`, `FileBufferSizePref`, and `QueueSizePref` appear to be backward-compat import keys.
- Session-stat fields and other runtime-only values were not counted as persistence defects.
