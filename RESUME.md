# Resume

## Last chunk completed
- Exposed the live hidden runtime preferences from `docs/PREFERENCES.md` in the Advanced tree on the Tweaks page.
- Added Advanced-tree groups for startup/session, hidden file/preview behavior, hidden display/rendering behavior, and hidden security/messaging/Kad behavior.
- Wired the new Tweaks controls into `CPPgTweaks` load, validation, apply, and localization flow.
- Added `preferences.ini` save support for the hidden runtime keys which were previously load-only.
- Updated `docs/PREFERENCES.md` so the hidden runtime section now reflects `RW` behavior and Advanced-tree exposure.

## Current behavior
- Hidden runtime preferences now have editable controls in `Preferences > Tweaks`.
- The following keys now round-trip through `preferences.ini`:
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
  - `PreferRestrictedOverUser`
  - `PartiallyPurgeOldKnownFiles`
  - `AdjustNTFSDaylightFileTime`
  - `RearrangeKadSearchKeywords`
  - `MessageFromValidSourcesOnly`

## Next chunk
- Build with `..\23-build-emule-debug-incremental.cmd` and fix any compile/resource errors from the new Tweaks tree additions.
- Runtime-verify the new Advanced-tree groups and confirm labels fit, tree expansion feels reasonable, and edits persist across restart.
- Verify the most behavior-sensitive hidden runtime settings:
  - `DateTimeFormat4Lists`
  - `UseSystemFontForMainControls`
  - `ReBarToolbar`
  - `ShowUpDownIconInTaskbar`
  - `KeepUnavailableFixedSharedDirs`
  - `PreferRestrictedOverUser`
  - `MessageFromValidSourcesOnly`
- After preference validation, return to the Kad modernization chunk centered on `CSafeKad2` and `CFastKad`.
