# Removing the IRC Module from eMule

This document provides a complete, ordered guide to removing the built-in IRC client from the eMule codebase. The IRC module comprises ~5,300 lines across 14 source files, 2 icon resources, 2 dialog templates, ~60 string resources, ~25 preference keys, and integration points in 9+ non-IRC files.

---

## Table of Contents

1. [Overview of IRC Files](#1-overview-of-irc-files)
2. [Step 1: Remove from Project File](#step-1-remove-from-project-file-emulevcxproj)
3. [Step 2: Delete IRC Source Files and Icons](#step-2-delete-irc-source-files-and-icons)
4. [Step 3: Gut EmuleDlg (Main Window)](#step-3-gut-emuledlg-main-window)
5. [Step 4: Remove Toolbar Button](#step-4-remove-toolbar-button)
6. [Step 5: Remove Preferences Page](#step-5-remove-preferences-page)
7. [Step 6: Remove Preference Variables](#step-6-remove-preference-variables)
8. [Step 7: Remove IRC Menu/Context-Menu Entries](#step-7-remove-irc-menucontext-menu-entries)
9. [Step 8: Remove Dialog and Control Resources](#step-8-remove-dialog-and-control-resources)
10. [Step 9: Remove String Resources](#step-9-remove-string-resources)
11. [Step 10: Remove Menu Command IDs](#step-10-remove-menu-command-ids)
12. [Step 11: Remove Help IDs and Constants](#step-11-remove-help-ids-and-constants)
13. [Step 12: Miscellaneous Cleanup](#step-12-miscellaneous-cleanup)
14. [Verification Checklist](#verification-checklist)

---

## 1. Overview of IRC Files

### Core IRC source files (delete all)

| File | Lines | Purpose |
|------|-------|---------|
| `IrcMain.cpp` / `.h` | ~1607 | IRC protocol handler, command parser |
| `IrcSocket.cpp` / `.h` | ~230 | Async socket for IRC server connection |
| `IrcWnd.cpp` / `.h` | ~1422 | Main IRC window (CIrcWnd : CDialog) |
| `IrcChannelTabCtrl.cpp` / `.h` | ~829 | Channel tab control, Channel/Nick structs |
| `IrcChannelListCtrl.cpp` / `.h` | ~274 | Server channel-list control |
| `IrcNickListCtrl.cpp` / `.h` | ~490 | Nick list control with context menu |
| `PPgIRC.cpp` / `.h` | ~437 | IRC preferences property page |

### Icon resources (delete both)

- `res/IRC.ico`
- `res/IRCClipboard.ico`

---

## Step 1: Remove from Project File (`emule.vcxproj`)

Remove these `<ClCompile>` entries:

```xml
<ClCompile Include="IrcChannelListCtrl.cpp" />
<ClCompile Include="IrcChannelTabCtrl.cpp" />
<ClCompile Include="IrcMain.cpp" />
<ClCompile Include="IrcNickListCtrl.cpp" />
<ClCompile Include="IrcSocket.cpp" />
<ClCompile Include="IrcWnd.cpp" />
<ClCompile Include="PPgIRC.cpp" />
```

Remove these `<ClInclude>` entries:

```xml
<ClInclude Include="IrcChannelListCtrl.h" />
<ClInclude Include="IrcChannelTabCtrl.h" />
<ClInclude Include="IrcMain.h" />
<ClInclude Include="IrcNickListCtrl.h" />
<ClInclude Include="IrcSocket.h" />
<ClInclude Include="IrcWnd.h" />
<ClInclude Include="PPgIRC.h" />
```

If a `.vcxproj.filters` file exists, remove the corresponding entries there too.

---

## Step 2: Delete IRC Source Files and Icons

Delete these 16 files from `srchybrid/`:

```
IrcMain.cpp          IrcMain.h
IrcSocket.cpp        IrcSocket.h
IrcWnd.cpp           IrcWnd.h
IrcChannelTabCtrl.cpp   IrcChannelTabCtrl.h
IrcChannelListCtrl.cpp  IrcChannelListCtrl.h
IrcNickListCtrl.cpp     IrcNickListCtrl.h
PPgIRC.cpp           PPgIRC.h
res/IRC.ico
res/IRCClipboard.ico
```

---

## Step 3: Gut EmuleDlg (Main Window)

**`EmuleDlg.h`:**
- Remove `class CIrcWnd;` forward declaration
- Remove member `CIrcWnd *ircwnd;`

**`EmuleDlg.cpp`:**
- Remove `#include "IrcWnd.h"`
- Remove `ircwnd = new CIrcWnd;` (in constructor / `OnInitDialog`)
- Remove `delete ircwnd;` (in destructor / `OnClose`)
- Remove `DialogCreateIndirect(ircwnd, IDD_IRC);` (in dialog creation)
- Remove the `case IDD_IRC:` block that sets `activate = ircwnd;`
- Remove `ircwnd,` from the dialog pointer array
- Remove `case MP_HM_IRC: SetActiveDialog(ircwnd);` handler
- Remove `ircwnd->UpdateFonts(...)` call
- Remove the menu entry: `menu.AppendMenu(MF_STRING, MP_HM_IRC, GetResString(IDS_IRC), _T("IRC"));`
- Remove all toolbar-related mappings for `TBBTN_IRC` / `ircwnd`:
  - `m_mapTbarCmdToIcon[TBBTN_IRC] = _T("IRC");`
  - Any `case` or `if` blocks comparing `pWnd == ircwnd` or returning `ircwnd`
- Remove or adjust `thePrefs.SetLastMainWndDlgID(IDD_IRC)` logic (ensure it cannot save/restore `IDD_IRC` as last active window)

> **Important:** After removing `ircwnd` from the dialog array, verify that array indices used elsewhere (e.g., toolbar button-to-window mapping) are adjusted. Typically, IRC is index 7 in a 0-based array of main windows.

---

## Step 4: Remove Toolbar Button

**`MuleToolBarCtrl.h`:**
- Remove `#define TBBTN_IRC` (or its equivalent in the button ID enum)

**`MuleToolBarCtrl.cpp`:**
- Remove `#include "IrcWnd.h"`
- Remove `theApp.emuledlg->ircwnd,` from the toolbar window-pointer array
- Remove `IDS_IRC,` from the toolbar button string-ID array
- Adjust the total button count constant if one exists

> **Important:** The toolbar arrays are positional. Removing an entry shifts all subsequent indices. Double-check that the remaining entries still align correctly across all parallel arrays (button IDs, string IDs, window pointers, icon mappings).

---

## Step 5: Remove Preferences Page

**`PreferencesDlg.h`:**
- Remove `#include "PPgIRC.h"`
- Remove member `CPPgIRC m_wndIRC;`

**`PreferencesDlg.cpp`:**
- Remove `m_wndIRC.m_psp.dwFlags &= ~PSH_HASHELP;`
- Remove `CTreePropSheet::SetPageIcon(&m_wndIRC, _T("IRC"));`
- Remove `AddPage(&m_wndIRC);`
- Remove `m_wndIRC.Localize();`
- Remove `IDS_IRC` from any page-title arrays used for tree navigation

---

## Step 6: Remove Preference Variables

**`Preferences.h`** -- remove all of these members and their getter/setter inlines:

```
// Member variables
UINT    splitterbarPositionIRC;
CString m_strIRCServer;
CString m_strIRCNick;
CString m_strIRCChannelFilter;
bool    m_bIRCAddTimeStamp;
bool    m_bIRCUseChannelFilter;
UINT    m_uIRCChannelUserFilter;
CString m_strIRCPerformString;
bool    m_bIRCUsePerform;
bool    m_bIRCGetChannelsOnConnect;
bool    m_bIRCAcceptLinks;
bool    m_bIRCAcceptLinksFriendsOnly;
bool    m_bIRCPlaySoundEvents;
bool    m_bIRCIgnoreMiscMessages;
bool    m_bIRCIgnoreJoinMessages;
bool    m_bIRCIgnorePartMessages;
bool    m_bIRCIgnoreQuitMessages;
bool    m_bIRCIgnorePingPongMessages;
bool    m_bIRCIgnoreEmuleAddFriendMsgs;
bool    m_bIRCAllowEmuleAddFriend;
bool    m_bIRCIgnoreEmuleSendLinkMsgs;
bool    m_bIRCJoinHelpChannel;
bool    m_bIRCEnableSmileys;
bool    m_bIRCEnableUTF8;
```

Getter/setter methods to remove (~30 methods):
```
GetSplitterbarPositionIRC() / SetSplitterbarPositionIRC()
GetIRCNick() / SetIRCNick()
GetIRCServer()
GetIRCAddTimeStamp()
GetIRCUseChannelFilter()
GetIRCChannelFilter()
GetIRCChannelUserFilter()
GetIRCUsePerform()
GetIRCPerformString()
GetIRCJoinHelpChannel()
GetIRCGetChannelsOnConnect()
GetIRCPlaySoundEvents()
GetIRCIgnoreMiscMessages()
GetIRCIgnoreJoinMessages()
GetIRCIgnorePartMessages()
GetIRCIgnoreQuitMessages()
GetIRCIgnorePingPongMessages()
GetIRCIgnoreEmuleAddFriendMsgs()
GetIRCIgnoreEmuleSendLinkMsgs()
GetIRCAllowEmuleAddFriend()
GetIRCAcceptLinks()
GetIRCAcceptLinksFriendsOnly()
GetIRCEnableSmileys()
GetIRCEnableUTF8()
```

**`Preferences.cpp`** -- remove:

- All static initializations for the above variables (~lines 384-424 area)
- All `ini.WriteXxx(...)` calls for IRC keys (~lines 1780-1803 area):
  ```
  SplitterbarPositionIRC, DefaultIRCServerNew, IRCNick,
  IRCAddTimestamp, IRCFilterName, IRCFilterUser, IRCUseFilter,
  IRCPerformString, IRCUsePerform, IRCListOnConnect,
  IRCAcceptLink, IRCAcceptLinkFriends, IRCSoundEvents,
  IRCIgnoreMiscMessages, IRCIgnoreJoinMessages,
  IRCIgnorePartMessages, IRCIgnoreQuitMessages,
  IRCIgnorePingPongMessages, IRCIgnoreEmuleAddFriendMsgs,
  IRCAllowEmuleAddFriend, IRCIgnoreEmuleSendLinkMsgs,
  IRCHelpChannel, IRCEnableSmileys, IRCEnableUTF8
  ```
- All `ini.GetXxx(...)` calls that load these keys (~lines 2284-2309 area)

> **Note:** Existing user INI files will harmlessly contain leftover IRC keys -- the CIni reader will simply skip unknown keys. No migration code is needed.

---

## Step 7: Remove IRC Menu/Context-Menu Entries

### SharedFilesCtrl.cpp

- Remove `#include "IrcWnd.h"`
- Remove `m_SharedFilesMenu.AppendMenu(MF_STRING, Irc_SetSendLink, ...)` (~line 1335)
- Remove the `EnableMenuItem(Irc_SetSendLink, ...)` call (~line 793)
- Remove the `case Irc_SetSendLink:` handler that calls `ircwnd->SetSendFileString(...)` (~line 832)

### ServerListCtrl.cpp

- Remove `#include "IrcWnd.h"`
- Remove the `case Irc_SetSendLink:` handler (~line 465-470)

### ChatSelector.cpp

- Remove any reference to `IDS_IRC_ADDTOFRIENDLIST` (~line 609) -- check if this string resource is shared or can be replaced with a non-IRC equivalent

---

## Step 8: Remove Dialog and Control Resources

**`emule.rc`:**

Remove these two entire dialog templates:

```
IDD_IRC DIALOGEX ...
    // ~12 lines defining the IRC main window layout
END

IDD_PPG_IRC DIALOGEX ...
    // ~20 lines defining the IRC preferences page layout
END
```

Remove these two icon resource lines:

```
IRC                     ICON    "res\\IRC.ico"
IRCCLIPBOARD            ICON    "res\\IRCClipboard.ico"
```

**`Resource.h`:**

Remove these dialog IDs:
```cpp
#define IDD_IRC                         2303
#define IDD_PPG_IRC                     2501
```

Remove these control IDs (only if not shared with non-IRC dialogs -- verify each):
```cpp
#define IDC_NICKLIST                    2006
#define IDC_TAB2                        2004
#define IDC_SERVERCHANNELLIST           2005
#define IDC_BN_IRCCONNECT               2459
#define IDC_CLOSECHAT                   2071
#define IDC_INPUTWINDOW                 2007
#define IDC_CHATSEND                    2433
#define IDC_TEXT_FORMAT                  2003
#define IDC_IRC_SERVER_FRM              2516
#define IDC_IRC_SERVER_BOX              2008
#define IDC_IRC_NICK_FRM                2518
#define IDC_IRC_NICK_BOX                2016
#define IDC_IRC_FILTER_FRM              2520
#define IDC_IRC_USECHANFILTER           2329
#define IDC_IRC_NAME_TEXT               2521
#define IDC_IRC_NAME_BOX                2011
#define IDC_IRC_MINUSER_TEXT            2523
#define IDC_IRC_MINUSER_BOX             2108
#define IDC_IRC_PERFORM_FRM             2524
#define IDC_IRC_USEPERFORM              2130
#define IDC_IRC_PERFORM_BOX             2038
#define IDC_IRC_MISC_FRM                2375
#define IDC_MISC_IRC                    2819
#define IDC_SPLITTER_IRC                2858
#define IDC_SPLITTER_IRC_CHANNEL        2851
```

> **Caution:** Some `IDC_` values like `IDC_NICKLIST`, `IDC_TAB2`, `IDC_CLOSECHAT`, `IDC_INPUTWINDOW`, `IDC_CHATSEND`, `IDC_TEXT_FORMAT` may be used by other dialogs (e.g., the Friends/Messages window shares a similar layout). **Grep each ID before deleting** to confirm it is IRC-only. IDs prefixed with `IDC_IRC_` are safe to remove unconditionally.

---

## Step 9: Remove String Resources

Remove all IRC-only `IDS_IRC*` string resources from both `Resource.h` (the `#define` lines) and `emule.rc` (the `STRINGTABLE` entries).

Keep these four shared IDs for existing non-IRC consumers in `WebServer.cpp` and `ChatSelector.cpp`:

```
IDS_IRC_CONNECT
IDS_IRC_DISCONNECT
IDS_IRC_PERFORM
IDS_IRC_ADDTOFRIENDLIST
```

Remove the remaining IDs from this list:

```
IDS_IRC                 360     "&IRC"
IDS_IRC_PRIVATECHANSTART 583
IDS_IRC_SLAPMSG         584
IDS_IRC_PRIVMESSAGE     585
IDS_IRC_KICK            589
IDS_IRC_SLAP            590
IDS_IRC_JOIN            591
IDS_IRC_CHANNELLIST     592
IDS_IRC_HASJOINED       595
IDS_IRC_HASPARTED       596
IDS_IRC_WASKICKEDBY     597
IDS_IRC_HASQUIT         599
IDS_IRC_NOWKNOWNAS      600
IDS_IRC_SETSMODE        601
IDS_IRC_NOTSUPPORTED    602
IDS_IRC_CHANNEL         603
IDS_IRC_NICK            604
IDS_IRC_SEND            605
IDS_IRC_NAME            606
IDS_IRC_SLAPMSGSEND     607
IDS_IRC_ADDTIMESTAMP    608
IDS_IRC_USEFILTER       609
IDS_IRC_USEPERFORM      611
IDS_IRC_LOADCHANNELLISTONCON 613
IDS_IRC_ADDASFRIEND     614
IDS_IRC_SENDLINK        620
IDS_IRC_RECIEVEDLINK    621
IDS_IRC_ACCEPTLINKS     622
IDS_IRC_ADDLINKTOIRC    619
IDS_IRC_IGNOREJOINMESSAGE 546
IDS_IRC_IGNOREPARTMESSAGE 548
IDS_IRC_IGNOREQUITMESSAGE 565
IDS_IRC_SOUNDEVENTS     567
IDS_IRC_IGNOREINFOMESSAGE 644
IDS_IRC_IGNOREPINGPONGMESSAGE 1596
IDS_IRC_ENABLEUTF8      1597
IDS_IRC_IGNOREMISCMESSAGE 1232
IDS_IRC_EMULEPROTO_IGNORESENDLINK 760
IDS_IRC_EMULEPROTO_IGNOREADDFRIEND 679
IDS_IRC_EMULEPROTO_IGNOREINFOMESSAGE 729
IDS_IRC_EMULEPROTO_ALLOWADDFRIEND 1270
IDS_IRC_HELPCHANNEL     1099
IDS_IRC_HELPCHANNELPERFORM 1100
IDS_IRC_ACCEPTLINKSFRIENDS 1101
IDS_IRC_NOSFS           1230
IDS_IRC_NEWNICK         1352
IDS_IRC_NEWNICKDESC     1353
IDS_IRC_BAN             1599
```

> **Note:** `IDS_IRC_ADDTOFRIENDLIST` is still used by `ChatSelector.cpp`, and `IDS_IRC_CONNECT`, `IDS_IRC_DISCONNECT`, and `IDS_IRC_PERFORM` are still used by `WebServer.cpp`. Keep those four IDs until they are renamed to neutral resource names in a separate pass.

---

## Step 10: Remove Menu Command IDs

**`MenuCmds.h`:**

Remove these defines:

```cpp
#define MP_HM_IRC           10215
#define Irc_Join             10240
#define Irc_Close            10241
#define Irc_Priv             10242
#define Irc_AddFriend        10243
#define Irc_SendLink         10244
#define Irc_SetSendLink      10245
#define Irc_Kick             10246
#define Irc_Ban              10247
#define Irc_KB               10248
#define Irc_Slap             10249
#define Irc_OpCommands       10250   // reserves IDs 10250-10299
#define Irc_ChanCommands     10300   // reserves IDs 10300-10399
```

---

## Step 11: Remove Help IDs and Constants

**`HelpIDs.h`:**

```cpp
#define eMule_FAQ_IRC_Chat              85
#define eMule_FAQ_Preferences_IRC       102
#define eMule_FAQ_GUI_IRC               206
```

**`Opcodes.h`:**

```cpp
#define MAX_IRC_MSG_LEN                 450
```

---

## Step 12: Miscellaneous Cleanup

### PPgGeneral.cpp
- Remove `#include "IrcWnd.h"`
- Remove `theApp.emuledlg->ircwnd->Localize();` call (~line 215)

### Language / localization DLLs
- If the project ships separate language resource DLLs, the same `IDD_IRC`, `IDD_PPG_IRC`, and `IDS_IRC*` entries must be removed from each language `.rc` file. Grep across `srchybrid/langs/` or equivalent.

### WebServer (if applicable)
- Check `WebServer.cpp`/`WebInterface` templates for IRC references. In the current tree, the web UI still reuses `IDS_IRC_CONNECT`, `IDS_IRC_DISCONNECT`, and `IDS_IRC_PERFORM` as generic labels, so those string IDs must stay for now.

### INI file keys
- No code changes needed. Leftover `[eMule]` section keys (`DefaultIRCServerNew`, `IRCNick`, etc.) in user INI files are harmlessly ignored when no code reads them.

### emule.rc icon index
- After removing the two icon entries, verify no code loads these icons by name string (`_T("IRC")`, `_T("IRCCLIPBOARD")`). The toolbar icon mapping `m_mapTbarCmdToIcon[TBBTN_IRC] = _T("IRC")` should already be removed in Step 3.

---

## Verification Checklist

After completing all steps:

- [ ] **Compile** -- the project must build with zero errors and zero IRC-related warnings
- [ ] **Grep verification** -- run these and confirm zero hits:
  ```
  grep -ri "IrcWnd\|IrcMain\|IrcSocket\|IrcChannel\|IrcNick\|PPgIRC" srchybrid/ --include="*.cpp" --include="*.h" --include="*.rc"
  grep -ri "TBBTN_IRC\|IDD_IRC\|IDD_PPG_IRC\|MP_HM_IRC\|Irc_Join\|Irc_Close\|Irc_Priv" srchybrid/ --include="*.cpp" --include="*.h"
  grep -ri "IDS_IRC" srchybrid/ --include="*.cpp" --include="*.h" --include="*.rc"
  grep -ri "m_strIRC\|m_bIRC\|m_uIRC\|GetIRC\|SetIRC" srchybrid/ --include="*.cpp" --include="*.h"
  ```
- [ ] **Shared string exception** -- the `IDS_IRC` grep is allowed to report only:
  - `IDS_IRC_CONNECT`
  - `IDS_IRC_DISCONNECT`
  - `IDS_IRC_PERFORM`
  - `IDS_IRC_ADDTOFRIENDLIST`
- [ ] **Run the application** -- verify:
  - Main window opens without the IRC tab/button
  - Toolbar renders correctly (no gaps, no shifted icons)
  - Preferences dialog opens without the IRC page
  - Right-click shared files -- no "Send link to IRC" option
  - Right-click server list -- no IRC-related option
  - Saving and loading preferences works without errors
- [ ] **File count** -- confirm 14 `.cpp`/`.h` files and 2 `.ico` files are deleted (16 total)

---

## Estimated Impact

| Metric | Count |
|--------|-------|
| Source files removed | 14 (.cpp/.h) |
| Resource files removed | 2 (.ico) |
| Lines of code removed | ~5,300 |
| Non-IRC files modified | ~12 |
| Preference keys removed | 24 |
| String resources removed | ~50 |
| Menu command IDs removed | 13 |
| Dialog templates removed | 2 |
