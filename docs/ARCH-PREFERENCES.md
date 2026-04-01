# Preferences Reference

This file documents the preferences and preference-like INI keys used by the current `v0.72a-broadband-dev` branch.

It is based on:

- `srchybrid/Preferences.cpp`
- `srchybrid/Preferences.h`
- the `PPg*.cpp` preference pages
- [DEFECTS.md](/C:/prj/p2p/eMulebb/eMule/DEFECTS.md)

## Table of Contents

- [Scope](#scope)
- [Legend](#legend)
- [UI-Exposed Preferences](#ui-exposed-preferences)
  - [General](#general) — [Connection And Throughput](#connection-and-throughput) — [Server / eD2k / Kad](#server--ed2k--kad) — [Directories, Files, And Download Behavior](#directories-files-and-download-behavior) — [Display / UI / Toolbar](#display--ui--toolbar) — [Security, Privacy, And Obfuscation](#security-privacy-and-obfuscation) — [Messaging / Chat](#messaging--chat) — [Notifications](#notifications) — [UPnP](#upnp) — [Broadband Branch Controls](#broadband-branch-controls)
- [Hidden Runtime Preferences](#hidden-runtime-preferences)
- [Hidden Or Legacy Keys Which Look Stale](#hidden-or-legacy-keys-which-look-stale-transitional-or-import-only)
- [UI Defaults Which Are Not First-Class CPreferences Keys](#ui-defaults-which-are-not-first-class-cpreferences-keys)
- [Technical Notes For Non-Obvious Settings](#technical-notes-for-non-obvious-settings)
  - [Throughput And Connection Control](#throughput-and-connection-control) — [Server / Kad Behavior](#server--kad-behavior) — [File I/O And Download Mechanics](#file-io-and-download-mechanics) — [Security / Messaging / Obfuscation](#security--messaging--obfuscation) — [UI / Rendering Internals](#ui--rendering-internals) — [Broadband Branch Controls](#broadband-branch-controls-1) — [Hidden Maintenance / Legacy Behavior Knobs](#hidden-maintenance--legacy-behavior-knobs)
- [Counters, Statistics, And State Stored In The Same INI](#counters-statistics-and-state-stored-in-the-same-ini)
- [TODO](#todo)
- [Practical Reading Guide](#practical-reading-guide)

## Scope

This document separates four different things which all live in `preferences.ini`:

1. real user-facing preferences
2. hidden/internal runtime preferences
3. compatibility or import-only keys
4. counters, statistics, and UI layout state

The first two groups are the actual behavior knobs.
The last two groups are documented because they still live in the same file, but they are not normal end-user settings.

## Legend

| Column | Meaning |
| --- | --- |
| `INI key` | The exact key name used in the INI file. |
| `Section` | INI section. If omitted in code, it is the main `eMule` section. |
| `Mode` | `RW` = read and written, `R` = read only, `W` = write only. |
| `UI` | Whether the setting is exposed in the Preferences dialogs. |
| `Default` | Default if the key is missing, based on current code. |
| `Explanation` | Practical purpose and effect. |

## UI-Exposed Preferences

### General

| INI key | Section | Mode | UI | Default | Explanation |
| --- | --- | --- | --- | --- | --- |
| `Nick` | `eMule` | `RW` | Yes | `DEFAULT_NICK` | The user nickname shown to peers. Saved as UTF-8. |
| `Language` | `eMule` | `RW` | Yes | Current OS / app default | UI language identifier. Changing it reloads most windows. |
| `BringToFront` | `eMule` | `RW` | Yes | `false` | Bring the main window to the foreground on relevant events. |
| `OnlineSignature` | `eMule` | `RW` | Yes | `false` | Enables the online signature/status output used by external tools or web pages. |
| `StartupMinimized` | `eMule` | `RW` | Yes | `false` | Start the main window minimized. This is persisted correctly. |
| `AutoStart` | `eMule` | `RW` | Yes | `false` | Start eMule automatically with Windows. |
| `PreventStandby` | `eMule` | `RW` | Yes | OS-dependent / `false` on old systems | Prevent system standby while the app is active. |

### Connection And Throughput

| INI key | Section | Mode | UI | Default | Explanation |
| --- | --- | --- | --- | --- | --- |
| `MaxUpload` | `eMule` | `RW` | Yes | `50000` | Upload speed limit in KiB/s for this branch. |
| `MaxDownload` | `eMule` | `RW` | Yes | `200000` | Download speed limit in KiB/s for this branch. |
| `DownloadCapacity` | `eMule` | `RW` | Yes | `200000` | Download line-capacity hint in KiB/s. Used for graphs and throughput logic. |
| `UploadCapacityNew` | `eMule` | `RW` | Yes | `50000` | Upload line-capacity hint in KiB/s. Important for the broadband controller. |
| `MaxConnections` | `eMule` | `RW` | Yes | existing app default | Hard cap for total connections. |
| `MaxHalfConnections` | `eMule` | `RW` | Yes | existing app default | Cap for half-open TCP connections. |
| `MaxConnectionsPerFiveSeconds` | `eMule` | `RW` | Yes | existing app default | Burst limiter for outbound connection attempts. |
| `Port` | `eMule` | `RW` | Yes | existing app default | Main TCP listening port. |
| `UDPPort` | `eMule` | `RW` | Yes | existing app default | Main UDP listening port. |
| `ServerUDPPort` | `eMule` | `RW` | Yes | existing app default | UDP port used for server communication. |
| `BindInterface` | `eMule` | `RW` | Yes | empty | Preferred network interface for P2P sockets. Empty means no interface restriction. |
| `BindInterfaceName` | `eMule` | `RW` | Yes | empty | Stored display name for the selected P2P bind interface, mainly to keep the UI readable if the adapter is currently missing. |
| `BindAddr` | `eMule` | `RW` | Yes | empty | Optional IPv4 address for P2P sockets. Empty means all addresses on the selected interface, or all interfaces when no interface is selected. |
| `ConditionalTCPAccept` | `eMule` | `RW` | Yes | existing app default | Controls conditional TCP accept behavior. This is an advanced network-side knob. |
| `ConnectionTimeout` | `eMule` | `RW` | Advanced tree | `30` seconds | Default TCP peer-connection timeout used by `EMSocket` and related connect/disconnect paths. |
| `DownloadTimeout` | `eMule` | `RW` | Advanced tree | `75` seconds | Inactivity timeout for receiving download payload blocks from a peer. |
| `UDPReceiveBufferSize` | `eMule` | `RW` | Advanced tree | `512 * 1024` | UDP receive socket buffer size in bytes. Exposed in Tweaks as KiB. |
| `BigSendBufferSize` | `eMule` | `RW` | Advanced tree | `512 * 1024` | Configured large TCP send buffer size in bytes for upload sockets. Exposed in Tweaks as KiB. |
| `UploadClientDataRate` | `eMule` | `RW` | Advanced tree | `8 * 1024 * 1024` | Ceiling for the heuristic per-client upload target used by the broadband slot controller. Exposed in Tweaks as KiB/s. |

### Server / eD2k / Kad

| INI key | Section | Mode | UI | Default | Explanation |
| --- | --- | --- | --- | --- | --- |
| `Reconnect` | `eMule` | `RW` | Yes | existing app default | Automatically reconnect to a server after disconnects. |
| `Serverlist` | `eMule` | `RW` | Yes | `false` | Auto-update the server list on startup. You asked to keep this default off. |
| `AddServersFromServer` | `eMule` | `RW` | Yes | `true` | Update the server list using server-provided data when connecting to a server. |
| `AddServersFromClient` | `eMule` | `RW` | Yes | existing app default | Accept server addresses from clients. |
| `Autoconnect` | `eMule` | `RW` | Yes | `false` | Automatically connect on startup. You asked to keep this default off because first launch can be messy otherwise. |
| `AutoConnectStaticOnly` | `eMule` | `RW` | Yes | `false` | Restrict auto-connect attempts to static servers only. |
| `DeadServerRetry` | `eMule` | `RW` | Yes | `5` | Number of failed tries after which a server is treated as dead and removed. |
| `SafeServerConnect` | `eMule` | `RW` | Yes | existing app default | More cautious server-connect strategy. |
| `ServerKeepAliveTimeout` | `eMule` | `RW` | Advanced tree | existing app default | Timeout used to keep the server connection alive. |
| `YourHostname` | `eMule` | `RW` | Yes | empty | Optional hostname override displayed/used by the app. |
| `NetworkED2K` | `eMule` | `RW` | Yes | `true` | Enable the eD2k network layer. |
| `NetworkKademlia` | `eMule` | `RW` | Yes | `true` | Enable the Kad network layer. |

### Directories, Files, And Download Behavior

| INI key | Section | Mode | UI | Default | Explanation |
| --- | --- | --- | --- | --- | --- |
| `IncomingDir` | `eMule` | `RW` | Yes | app default path | Directory used for completed downloads. |
| `TempDir` | `eMule` | `RW` | Yes | app default path | Primary temp directory for `.part` files. |
| `TempDirs` | `eMule` | `RW` | Yes | empty | Additional temp directories, stored as a `|`-separated list. |
| `MaxSourcesPerFile` | `eMule` | `RW` | Yes | `600` | Hard cap for sources tracked per file. |
| `AddNewFilesPaused` | `eMule` | `RW` | Yes | `false` | Add new downloads in paused state instead of starting immediately. |
| `PreviewPrio` | `eMule` | `RW` | Yes | `true` | Try to download preview chunks first. This is the setting you just changed to default on. |
| `AllocateFullFile` | `eMule` | `RW` | Yes | `false` | Preallocate the full output file size ahead of download progress. |
| `SparsePartFiles` | `eMule` | `RW` | Yes | `false` | Use sparse part-file allocation where supported. |
| `CommitFiles` | `eMule` | `RW` | Yes | `1` | Controls file commit behavior. Legacy low-level file I/O policy. |
| `CheckDiskspace` | `eMule` | `RW` | Yes | `false` | Enforce minimum free-disk-space checks. |
| `MinFreeDiskSpace` | `eMule` | `RW` | Yes | `5 * 1024 * 1024 * 1024` | Minimum free disk space threshold. Tweaks now edits this in GB. |
| `AutoArchivePreviewStart` | `eMule` | `RW` | Yes | `true` | Automatically start archive preview extraction. |
| `ExtractMetaData` | `eMule` | `RW` | Yes | `1` | Metadata extraction mode. |
| `ResolveSharedShellLinks` | `eMule` | `RW` | Yes | `false` | Resolve shell links in shared directories. |
| `ShowSharedFilesDetails` | `eMule` | `RW` | Yes | `true` | Show the shared-files details area. |
| `AutoShowLookups` | `eMule` | `RW` | Yes | `true` | Auto-show Kad lookups in the UI. |
| `RemoveFilesToBin` | `eMule` | `RW` | Yes | `true` | Send removed files to the recycle bin instead of deleting directly. |
| `RememberCancelledFiles` | `eMule` | `RW` | Yes | `true` | Keep memory of cancelled files. |
| `RememberDownloadedFiles` | `eMule` | `RW` | Yes | `true` | Keep memory of already-downloaded files. |
| `AutoClearCompleted` | `eMule` | `RW` | Yes | `false` | Automatically clear finished downloads from the list. |
| `FileBufferSize` | `eMule` | `RW` | Yes | `2 * 1024 * 1024` | Global file buffer size in bytes. Exposed via the Tweaks slider. |
| `QueueSize` | `eMule` | `RW` | Yes | derived from app default | Queue size cap. |

### Display / UI / Toolbar

| INI key | Section | Mode | UI | Default | Explanation |
| --- | --- | --- | --- | --- | --- |
| `ShowRatesOnTitle` | `eMule` | `RW` | Yes | `true` | Show current transfer rates in the window title. |
| `ShowExtControls` | `eMule` | `RW` | Yes | `true` | Show extended controls. You asked for this default on. |
| `ShowDwlPercentage` | `eMule` | `RW` | Yes | `false` | Show extra download percentage information. |
| `IndicateRatings` | `eMule` | `RW` | Yes | `true` | Show file ratings in UI where supported. |
| `ToolTipDelay` | `eMule` | `RW` | Yes | existing app default | Delay before tooltips appear. |
| `ToolbarSetting` | `eMule` | `RW` | Yes | toolbar default | Toolbar layout/configuration string. |
| `ToolbarBitmap` | `eMule` | `RW` | Yes | empty | Custom toolbar bitmap path. |
| `ToolbarBitmapFolder` | `eMule` | `RW` | Yes | empty | Folder containing toolbar bitmaps. |
| `ToolbarLabels` | `eMule` | `RW` | Yes | toolbar default | Label mode for toolbar buttons. |
| `ToolbarIconSize` | `eMule` | `RW` | Yes | `32` | Toolbar icon size in pixels. |
| `WinaTransToolbar` | `eMule` | `RW` | Yes | `true` | Toolbar transparency integration on supported Windows versions. |
| `ShowDownloadToolbar` | `eMule` | `RW` | Yes | `true` | Show the download toolbar. |
| `SkinProfile` | `eMule` | `RW` | Yes | empty | Selected skin profile. |
| `SkinProfileDir` | `eMule` | `RW` | Yes | empty | Selected skin directory. |
| `DateTimeFormat` | `eMule` | `RW` | Yes | `%A, %c` | Main date/time format string. |
| `DateTimeFormat4Log` | `eMule` | `RW` | Yes | `%c` | Date/time format string used in logs. |
| `TransferDoubleClick` | `eMule` | `RW` | Yes | existing app default | Double-click action in transfer lists. |
| `ShowOverhead` | `eMule` | `RW` | Yes | `false` | Show protocol overhead in transfer display. |
| `ShowInfoOnCatTabs` | `eMule` | `RW` | Yes | `false` | Show info on category tabs. |
| `ShowCopyEd2kLinkCmd` | `eMule` | `RW` | Yes | `false` in older code path | Show the copy-ed2k-link context command. |

### Security, Privacy, And Obfuscation

| INI key | Section | Mode | UI | Default | Explanation |
| --- | --- | --- | --- | --- | --- |
| `FilterServersByIP` | `eMule` | `RW` | Yes | existing app default | Filter servers using the IP filter rules. |
| `FilterLevel` | `eMule` | `RW` | Yes | `127` | IP filter aggressiveness level. |
| `FilterBadIPs` | `eMule` | `RW` | Yes | existing app default | Filter LAN/bad IPs more generally. |
| `SecureIdent` | `eMule` | `RW` | Yes | `true` | Enable secure identification support. |
| `CryptLayerRequested` | `eMule` | `RW` | Yes | `true` | Request obfuscation when talking to peers. |
| `CryptLayerRequired` | `eMule` | `RW` | Yes | `false` | Require obfuscation. |
| `CryptLayerSupported` | `eMule` | `RW` | Yes | `true` | Advertise obfuscation support. |
| `EnableSearchResultSpamFilter` | `eMule` | `RW` | Yes | `true` | Filter obviously spammy search results. |
| `CheckFileOpen` | `eMule` | `RW` | Yes | `true` | Check for open/locked files before file operations. |
| `SeeShare` | `eMule` | `RW` | Yes | existing app default | Controls who can see your shared files. |
| `AdvancedSpamFilter` | `eMule` | `RW` | Yes | `true` | Enables the more advanced spam filter path. |

### Messaging / Chat

| INI key | Section | Mode | UI | Default | Explanation |
| --- | --- | --- | --- | --- | --- |
| `MessagesFromFriendsOnly` | `eMule` | `RW` | Yes | `false` | Restrict incoming messages to friends only. |
| `MessageUseCaptchas` | `eMule` | `RW` | Yes | `true` | Require captchas for chat where the anti-spam path is active. |
| `MessageEnableSmileys` | `eMule` | `RW` | Yes | `true` | Enable smiley rendering in messages. |
| `MessageFilter` | `eMule` | `RW` | Yes | built-in filter string | Text filter applied to incoming messages. |
| `CommentFilter` | `eMule` | `RW` | Yes | built-in filter string | Text filter applied to comments. |

### Notifications

| INI key | Section | Mode | UI | Default | Explanation |
| --- | --- | --- | --- | --- | --- |
| `NotifierConfiguration` | `eMule` | `RW` | Yes | notifier config path | Path to notifier configuration. |
| `NotifyOnDownload` | `eMule` | `RW` | Yes | `false` unless enabled by user | Notify on completed downloads. |
| `NotifyOnNewDownload` | `eMule` | `RW` | Yes | `false` unless enabled by user | Notify when a new download is added. |
| `NotifyOnChat` | `eMule` | `RW` | Yes | `false` unless enabled by user | Notify on incoming chat. |
| `NotifyOnLog` | `eMule` | `RW` | Yes | `false` unless enabled by user | Notify on log output. |
| `NotifyOnImportantError` | `eMule` | `RW` | Yes | `false` unless enabled by user | Notify on important errors. |
| `NotifierPopEveryChatMessage` | `eMule` | `RW` | Yes | `false` | Pop every chat message, not just first/important ones. |
| `NotifierUseSound` | `eMule` | `RW` | Yes | `ntfstNoSound` | Select notifier sound mode. |
| `NotifierSoundPath` | `eMule` | `RW` | Yes | empty | Custom sound file path. |

### UPnP

| INI key | Section | Mode | UI | Default | Explanation |
| --- | --- | --- | --- | --- | --- |
| `EnableUPnP` | `UPnP` | `RW` | Yes | `true` | Enable UPnP. You asked for this default on, including wizard flow. |
| `CloseUPnPOnExit` | `eMule` | `RW` | Yes | `true` | Remove UPnP mappings on exit. |
| `SkipWANIPSetup` | `eMule` | `RW` | Yes | `false` | Skip WAN IP setup path. |
| `SkipWANPPPSetup` | `eMule` | `RW` | Yes | `false` | Skip WAN PPP setup path. |
| `LastWorkingImplementation` | `eMule` | `RW` | No | `1` | Tracks which UPnP backend last worked. Internal, but persisted. |

### Broadband Branch Controls

| INI key | Section | Mode | UI | Default | Explanation |
| --- | --- | --- | --- | --- | --- |
| `BBMaxUpClientsAllowed` | `eMule` | `RW` | Yes | `12` | Broadband steady-state upload-slot target. This is the main broadband slot-count knob. |
| `BBSessionMaxTrans` | `eMule` | `RW` | Yes | `SESSIONMAXTRANS` from `Opcodes.h` | Upload session transfer limit. `0` disables, `1..100` means percent of current file size, `>100` means absolute byte count. |
| `BBSessionMaxTime` | `eMule` | `RW` | Yes | `SESSIONMAXTIME` from `Opcodes.h` | Upload session time limit in milliseconds. `0` disables. |
| `BBBoostLowRatioFiles` | `eMule` | `RW` | Yes | `0` | Ratio threshold below which files are considered low-ratio and may get score boosting. |
| `BBBoostLowRatioFilesBy` | `eMule` | `RW` | Yes | `0` | Additive score bonus applied when the low-ratio threshold matches. |
| `BBDeboostLowIDs` | `eMule` | `RW` | Yes | `0` | Divisor applied to actual LowID clients in queue scoring. |

## Hidden Runtime Preferences

These settings are active and meaningful. They are now exposed in the Advanced tree on the Tweaks page, but they are still not exposed on the normal dedicated preference pages.

| INI key | Section | Mode | UI | Default | Explanation |
| --- | --- | --- | --- | --- | --- |
| `RestoreLastMainWndDlg` | `eMule` | `RW` | Advanced tree | `false` | Restore the last active main tab on startup. Used in `EmuleDlg.cpp`. |
| `RestoreLastLogPane` | `eMule` | `RW` | Advanced tree | `false` | Restore the selected log pane. Used in `ServerWnd.cpp`. |
| `FileBufferTimeLimit` | `eMule` | `RW` | Advanced tree | `120` seconds | Maximum age of buffered part-file data before forced flush. Used in `PartFile.cpp`. |
| `DateTimeFormat4Lists` | `eMule` | `RW` | Advanced tree | `%c` | Separate date/time format string for list controls. Used in `DownloadListCtrl.cpp`. |
| `PreviewCopiedArchives` | `eMule` | `RW` | Advanced tree | `true` | Allow preview/archive-recovery logic on copied archives. Used in archive preview and part-file logic. |
| `InspectAllFileTypes` | `eMule` | `RW` | Advanced tree | `0` | Force metadata inspection on all file types, not just the obvious media types. |
| `PreviewOnIconDblClk` | `eMule` | `RW` | Advanced tree | `false` | Use icon double-click as preview action in the download list. |
| `ShowActiveDownloadsBold` | `eMule` | `RW` | Advanced tree | `false` | Render active downloads in bold. |
| `UseSystemFontForMainControls` | `eMule` | `RW` | Advanced tree | `false` | Use system fonts for major controls and list views. |
| `ReBarToolbar` | `eMule` | `RW` | Advanced tree | `true` | Enable rebar-based toolbar layout. |
| `ShowUpDownIconInTaskbar` | `eMule` | `RW` | Advanced tree | `false` | Show upload/download state in taskbar icon handling. |
| `ShowVerticalHourMarkers` | `eMule` | `RW` | Advanced tree | `true` | Draw vertical hour markers on statistics graphs. |
| `ForceSpeedsToKB` | `eMule` | `RW` | Advanced tree | `false` | Force speed-formatting helpers to prefer KB-based units. |
| `ExtraPreviewWithMenu` | `eMule` | `RW` | Advanced tree | `false` | Changes where preview actions appear in the download-list UI/menu flow. |
| `KeepUnavailableFixedSharedDirs` | `eMule` | `RW` | Advanced tree | `false` | Keep fixed shared dirs even when currently unavailable during startup/shared-dir loading. |
| `PartiallyPurgeOldKnownFiles` | `eMule` | `RW` | Advanced tree | `true` | Makes known-file/AICH cleanup less aggressive. |
| `AdjustNTFSDaylightFileTime` | `eMule` | `RW` | Advanced tree | `false` | Adjust NTFS timestamps around daylight-saving boundaries to avoid false file-change/rehash churn. |
| `RearrangeKadSearchKeywords` | `eMule` | `RW` | Advanced tree | `true` | Reorder Kad search keywords before issuing the search. |
| `MessageFromValidSourcesOnly` | `eMule` | `RW` | Advanced tree | `true` | Hidden message acceptance gate used by `BaseClient.cpp`. Restricts messages to sources considered valid enough by that path. |

## Hidden Or Legacy Keys Which Look Stale, Transitional, Or Import-Only

| INI key | Section | Mode | Default | Explanation |
| --- | --- | --- | --- | --- |
| `FileBufferSizePref` | `eMule` | `R` | old compatibility import | Old file-buffer key. Current code reads it only as migration input, then uses `FileBufferSize`. |
| `QueueSizePref` | `eMule` | `R` | old compatibility import | Old queue-size key. Current code reads it only as migration input, then uses `QueueSize`. |
| `UploadCapacity` | `eMule` | `R` | legacy compatibility | Older upload-capacity key superseded by `UploadCapacityNew`. |
| `UserSortedServerList` | `eMule` | `R` | legacy compatibility | Legacy server-list ordering state. |
| `AICHTrustEveryHash` | `eMule` | `R` | `false` | Loaded into preferences, but no confirmed non-`Preferences.*` runtime use was found during the audit. Likely stale or unfinished. |

## UI Defaults Which Are Not First-Class `CPreferences` Keys

These values are currently defaulted directly in UI code or autocomplete history behavior, not stored as first-class `CPreferences` members.

| Setting | Current default | Where it is implemented | Explanation |
| --- | --- | --- | --- |
| Server.met URL | `http://upd.emule-security.org/server.met` | `ServerWnd.cpp` | Default text shown in the server.met URL edit when empty. |
| Nodes.dat URL | `http://upd.emule-security.org/nodes.dat` | `KademliaWnd.cpp` | Default text shown in the Kad bootstrap URL edit when empty. |
| Kad bootstrap mode | `Load nodes from URL` | `KademliaWnd.cpp` | Default radio selection in the Kad tab. |
| IP filter URL | `http://upd.emule-security.org/ipfilter.zip` | `PPgSecurity.cpp` | Default text shown in the IP filter URL edit when empty. |

## Technical Notes For Non-Obvious Settings

This section explains what the more technical settings actually do in runtime terms.

### Throughput And Connection Control

| Setting | What it actually does |
| --- | --- |
| `DownloadCapacity` | This is not a live throttle by itself. It is the app's configured estimate of available downstream capacity. Code uses it for graph scaling, throughput heuristics, and any logic that wants a line-capacity hint instead of the current live rate. |
| `UploadCapacityNew` | This is the configured upstream-capacity hint. On this branch it feeds the broadband upload controller together with the active upload limit when slot targeting and per-slot throughput goals are derived. |
| `MaxUpload` | This is the actual upload limit presented to the rest of the app. The broadband controller treats it as one bound on the effective upload budget, so it directly constrains slot targeting and per-slot throughput goals. |
| `MaxDownload` | This is the active download speed limit. Unlike capacity, this is the operative cap used when download throttling logic is active. |
| `MaxConnections` | Hard cap on total tracked/open connections. It is a blunt resource guard, not a prioritization tool. |
| `MaxHalfConnections` | Caps not-yet-fully-open TCP connections. This mainly affects connect burst behavior and old Windows TCP stack sensitivity. |
| `MaxConnectionsPerFiveSeconds` | Burst limiter for new outbound connections. It smooths connection churn and protects both the local stack and remote peers from aggressive connect storms. |
| `ConditionalTCPAccept` | A lower-level network acceptance policy. It affects when the app accepts inbound TCP work under load instead of being a simple UI convenience setting. |
| `ConnectionTimeout` | Sets the default timeout budget for peer TCP sockets. The listen-socket and download paths extend this base timeout in specific states, but this is now the shared baseline. |
| `DownloadTimeout` | Controls how long a download peer may stay silent between completed payload blocks before the transfer is cancelled and put back on queue. |
| `UploadClientDataRate` | Caps the heuristic target rate for one upload slot. It does not replace the global upload throttle; it bounds the per-slot target derived by the broadband upload controller. |

### Server / Kad Behavior

| Setting | What it actually does |
| --- | --- |
| `Serverlist` | Controls whether startup will try to refresh the server list automatically. This is only the startup refresh behavior, not the broader “accept new servers from other places” policy. |
| `AddServersFromServer` | Lets connected servers feed additional server entries into the local server list. In practice this is the “update server list when connecting to a server” behavior. |
| `AddServersFromClient` | Lets clients contribute server addresses. This broadens discovery but also expands the trust surface. |
| `Autoconnect` | If enabled, the app starts trying to connect automatically at startup. It affects startup sequencing, which is why you reverted its default to `false`. |
| `AutoConnectStaticOnly` | Restricts auto-connect attempts to static servers. It narrows the candidate set rather than changing connection mechanics. |
| `DeadServerRetry` | Defines how many failed contacts a server can accumulate before being treated as dead in server-list maintenance paths. |
| `SafeServerConnect` | Chooses a more conservative connect strategy rather than simply “connect faster”. It reduces churn and poor target selection during automatic connect flows. |
| `NetworkED2K` / `NetworkKademlia` | These are top-level network enables. Disabling one removes that network's activity paths rather than merely hiding UI. |

### File I/O And Download Mechanics

| Setting | What it actually does |
| --- | --- |
| `FileBufferSize` | Caps how much data a part file keeps buffered before flushing. Larger values reduce write frequency and can improve sequential write behavior, but increase memory use and delay flushes. |
| `FileBufferTimeLimit` | Hidden companion to `FileBufferSize`. Even if the buffer does not fill, `PartFile.cpp` forces a flush once buffered data gets older than this threshold. |
| `UDPReceiveBufferSize` | Controls the UDP socket receive buffer size. Larger fixed values reduce packet-drop risk during bursts at the cost of some kernel buffer memory. |
| `BigSendBufferSize` | Controls the larger TCP send buffer target for upload sockets. Larger values can help keep fast upload slots fed without relying on tiny legacy buffers. |
| `QueueSize` | Caps the upload waiting queue size. It does not directly control upload slots; it limits how many clients can remain queued for service. |
| `PreviewPrio` | Biases downloads toward preview-relevant chunks first. It changes chunk-request preference, not just UI sorting. |
| `AllocateFullFile` | Causes destination files to be pre-sized up front. This can reduce fragmentation but increases upfront disk work and space reservation. |
| `SparsePartFiles` | Lets the filesystem avoid materializing all zero regions immediately. Useful on supported filesystems, but behavior depends on platform/filesystem support. |
| `CommitFiles` | Controls how aggressively the app commits file data to stable storage. It is a legacy durability/performance tradeoff rather than a user-facing convenience option. |
| `CheckDiskspace` / `MinFreeDiskSpace` | These gate download/write behavior when free space gets too low. They are operational safety checks, not UI-only warnings. |
| `PreviewCopiedArchives` | Hidden runtime switch used by archive preview/recovery paths. It determines whether copied archives are eligible for preview recovery behavior. |
| `InspectAllFileTypes` | Hidden runtime switch for metadata inspection. When enabled, file-info probing extends beyond the usual media-centric file types. |
| `PreviewOnIconDblClk` | Changes input behavior in the download list: double-clicking the icon zone becomes a preview action. |
| `ExtraPreviewWithMenu` | Hidden UI-flow modifier that changes how preview actions are exposed in the transfer list/menu logic. |

### Security / Messaging / Obfuscation

| Setting | What it actually does |
| --- | --- |
| `SecureIdent` | Enables the secure-identification system used to validate peer identity claims. This affects trust/credit-related flows, not encryption. |
| `CryptLayerRequested` | Advertises that the client would like obfuscated/encrypted peer traffic when the other side supports it. |
| `CryptLayerRequired` | Refuses plain connections where obfuscation is expected. This is much stronger than merely “prefer obfuscation”. |
| `CryptLayerSupported` | Announces protocol support for obfuscation. Disabling it removes that capability entirely. |
| `AdvancedSpamFilter` | Enables additional filtering logic for spam-like content beyond the simpler legacy path. |
| `MessageUseCaptchas` | Requires chat captcha checks where the message anti-spam path is active. It affects acceptance flow, not just presentation. |
| `MessageFromValidSourcesOnly` | Hidden message gate. In `BaseClient.cpp`, it restricts which peers can send messages through that path. Despite the name, it is not exposed in the dialogs today. |

### UI / Rendering Internals

| Setting | What it actually does |
| --- | --- |
| `UseSystemFontForMainControls` | Hidden rendering choice used by multiple list and window classes. It changes which font objects are applied to major controls. |
| `ReBarToolbar` | Hidden toolbar layout selector. It changes whether the app uses the rebar-style toolbar host instead of just recoloring an existing toolbar. |
| `ShowUpDownIconInTaskbar` | Hidden taskbar integration flag. It controls whether upload/download state feeds taskbar icon handling. |
| `ShowVerticalHourMarkers` | Hidden graph-rendering option. It affects how the statistics graph paints time markers. |
| `ForceSpeedsToKB` | Hidden formatting override. It changes unit-selection logic in the formatting helpers, not the underlying rate calculations. |
| `DateTimeFormat4Lists` | Hidden date formatting string used specifically by list controls, distinct from the main UI and log formats. |

### Broadband Branch Controls

| Setting | What it actually does |
| --- | --- |
| `BBMaxUpClientsAllowed` | Sets the broadband branch's normal steady-state upload slot target. The queue/throttler logic tries to stabilize around this value instead of the old “many 25 KiB/s slots” model. |
| `BBSessionMaxTrans` | Controls when a productive upload slot should be rotated out based on delivered payload. `0` disables transfer-based rotation; `1..100` interprets the number as a percent of the current upload file size; values above `100` are treated as absolute byte limits. |
| `BBSessionMaxTime` | Time-based rotation limit for upload sessions. It complements `BBSessionMaxTrans` so healthy slots can still rotate after long service times. |
| `BBBoostLowRatioFiles` | Threshold for deciding that a shared file is under-seeded enough to deserve queue-score preference. The actual ratio metric is file transferred bytes divided by file size. |
| `BBBoostLowRatioFilesBy` | Additive queue-score bonus applied when the low-ratio threshold matches. This affects who gets the next productive slot, not how many slots exist. |
| `BBDeboostLowIDs` | Queue-score divisor applied to actual LowID clients. This is deliberately harsh seeder policy: LowIDs are not banned, but they are deprioritized. |

### Hidden Maintenance / Legacy Behavior Knobs

| Setting | What it actually does |
| --- | --- |
| `KeepUnavailableFixedSharedDirs` | Hidden startup/shared-dir loading behavior. It decides whether fixed shared directories survive config load even when currently inaccessible. |
| `PartiallyPurgeOldKnownFiles` | Hidden known-file retention policy. It makes old known-file/AICH cleanup less aggressive, which can preserve more history at the cost of more stale entries. |
| `AdjustNTFSDaylightFileTime` | Hidden timestamp workaround. It compensates for NTFS daylight-saving timestamp shifts to avoid accidental “file changed” detections and rehashes. |
| `RearrangeKadSearchKeywords` | Hidden Kad search optimization. It changes the keyword order presented to Kad search logic, which can alter search matching behavior. |
| `RestoreLastMainWndDlg` / `RestoreLastLogPane` | Hidden session-restore flags for page/pane selection on startup. |

## Counters, Statistics, And State Stored In The Same INI

These are persisted, but they are not really "preferences". They are state, telemetry, or historical counters.

### Transfer Totals And Session Counters

| Key family | Explanation |
| --- | --- |
| `TotalDownloadedBytes`, `TotalUploadedBytes` | Cumulative transfer totals across sessions. |
| `DownSuccessfulSessions`, `DownFailedSessions`, `DownAvgTime`, `DownCompletedFiles`, `DownSessionCompletedFiles` | Download session counters and aggregates. |
| `UpSuccessfulSessions`, `UpFailedSessions`, `UpAvgTime` | Upload session counters and aggregates. |
| `LostFromCorruption`, `SavedFromCompression`, `PartsSavedByICH` | Recovery/integrity savings counters. |

### Protocol / Client-Family / Port-Family Data Totals

| Key family | Explanation |
| --- | --- |
| `DownData_*`, `UpData_*` | Data totals grouped by client family or source family. |
| `DownDataPort_*`, `UpDataPort_*` | Data totals grouped by port family. |
| `UpData_File`, `UpData_Partfile` | Upload totals grouped by file type/source of upload. |

### Overhead Counters

| Key family | Explanation |
| --- | --- |
| `DownOverhead*` | Download-side protocol overhead counters. |
| `UpOverhead*` | Upload-side protocol overhead counters. |

### Connection And Server Statistics

| Key family | Explanation |
| --- | --- |
| `Conn*` | Connection runtime totals, peak values, average rates, reconnect counts, and durations. |
| `SrvrsMost*` | High-water marks for server count, users online, and files available. |
| `SharedMostFilesShared`, `SharedLargest*` | High-water marks related to sharing size and file counts. |

### UI Layout And Window State

| Key family | Explanation |
| --- | --- |
| `SplitterbarPosition*` | Saved splitter positions for main panes. |
| `TransferWnd1`, `TransferWnd2` | Transfer window layout state. |
| `LastMainWndDlgID`, `LastLogPaneID` | Remembered last selected pages/panes. |
| `Toolbar*`, `Skin*` | Toolbar and skin configuration state. |
| `HyperTextFont`, `LogTextFont`, `Log*Color` | Saved fonts and colors. |
| `StatColor*`, `statsExpandedTreeItems`, `statsConnectionsGraphRatio`, `HasCustomTaskIconColor` | Statistics window presentation state. |

### Category Records

Categories are saved under their own numbered sections rather than as one flat preference block.

| Key | Explanation |
| --- | --- |
| `Count` in `General` | Number of user categories beyond the default one. |
| `Title`, `Incoming`, `Comment` | Category metadata. |
| `Color` | Category color. |
| `a4afPriority` | Category A4AF priority. |
| `AutoCat`, `Autocat`, `RegularExpression`, `AutoCatAsRegularExpression` | Auto-categorization rules. |
| `Filter`, `FilterNegator` | Category filter behavior. |
| `downloadInAlphabeticalOrder` | Per-category alphabetical ordering option. |
| `Care4All` | Category-wide "care for all" behavior flag. |

## TODO

The unresolved issues from [DEFECTS.md](/C:/prj/p2p/eMulebb/eMule/DEFECTS.md) that matter most for the preference system are:

| Item | Why it matters | Suggested follow-up |
| --- | --- | --- |
| `AllowedIPs` load-only behavior | Web allowed-IP list is not written back. | Add save support or remove the feature explicitly. |
| Hidden runtime prefs listed above | They are now exposed in the Advanced tree and written back, but they still need selective runtime verification because several are niche or internal. | Verify each edited setting in the affected subsystem before treating the UI as fully validated. |
| `AICHTrustEveryHash` | Likely stale hidden knob. | Confirm intent and then either wire it back up or remove it. |

## Practical Reading Guide

If you only care about real end-user settings, read:

1. `UI-Exposed Preferences`
2. `Broadband Branch Controls`
3. `UI Defaults Which Are Not First-Class CPreferences Keys`

If you are auditing weird legacy behavior, also read:

1. `Hidden Runtime Preferences`
2. `Hidden Or Legacy Keys Which Look Stale, Transitional, Or Import-Only`
3. `TODO`
