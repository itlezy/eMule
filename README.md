# eMule - broadband branch

The initial purpose of this project was to provide an eMule repository
(including dependencies) that is ready to build and update the dependent
libraries when possible.

This development branch specifically focuses on providing a build that is
better suited to address nowadays file sizes and broadband availability.
Default hard-coded parameters of eMule were better suited for
small-files/slow-connections, leading to very low per-client transfer rates by
nowadays standards.

The focus here is to maximise throughput for broadband users, to **optimize
seeding**. The main feature that was added was the capability of limiting the
upload slots to a certain number, while ensuring to make full usage of the
upload bandwidth.

This is a seeder-oriented branch, designed to seed back to the ED2K network, to
allow a personalized upload strategy based on the nature of your shared library
and your IT setup.

The focus is as well to introduce the least amount of changes to preserve the
original quality and stability of the client. Please read the guide below to
understand the configuration parameters and the capabilities.

This branch keeps the broadband upload controller and session controls, and
reapplies a small set of useful ratio/cooldown columns, but does not carry over
the wider UI/queue experiments from `v0.60d-dev`. For the code
level rationale and the current `v0.72a-broadband-dev` design notes, please see
[`BROADBAND.md`](docs/BROADBAND.md).

## Installation

### eMule

Reccomended to install the latest eMule Community version, but any `0.50+`
should be fine as well.

Get the latest eMule Community edition from here:
https://github.com/irwir/eMule/releases

### Broadband Edition

To install eMule broadband, build the executable from this branch and replace
your current eMule executable.

Be sure to make a backup of `%LOCALAPPDATA%\eMule` first, as this is still a
"beta" build which requires testing. Even if the amount of changes is kept
deliberately low, this branch changes the upload controller behavior and some
legacy hard-coded defaults.

### Optimal Settings

Really the one recommendation would be to set the values of bandwidth capacity
and the **upload limit**, plus a limit of max connections if you wish so. Other
settings, as you please.

Be fair about it, the purpose is to **maximise seeding**, so be generous with
your bandwidth and set it as much as possible based on your connection and I/O
preferences.

![2022-06-14 14_05_11-Window](https://user-images.githubusercontent.com/24484050/173573013-6a76d50f-f168-4a81-83c7-888ee3de6b6a.png)

### Upload Slots Settings

**Max upload slots** are now configurable from **Preferences > Tweaks > Broadband**.

If you prefer editing the ini file directly, the underlying key is still:

`BBMaxUpClientsAllowed=12`

You can adjust this limit according to your bandwitdh and I/O preferences,
suggested ranges are `6`, `8`, `12`, `16`, `24`, and so on.

This setting acts as the normal amount of upload slots the controller will aim
for. In edge cases the controller may briefly exceed it by a small amount if
the upload pipe is still underfilled, but the goal is to avoid the old runaway
slot growth.

If you are seeding from multiple disk drives or SSD drives, then you can bump
up the upload slots as you deem fit.

### Broadband Settings

These settings are now available in **Preferences > Tweaks > Broadband** and
still map directly to the same `preferences.ini` keys.

|Setting|Default|Description|
|---|---|---|
|`BBMaxUpClientsAllowed`|12|Upper target of concurrent uploads in steady state.|
|`BBBoostLowRatioFiles`|0|Enable a score bonus for files whose all-time uploaded-bytes-to-file-size ratio is below this threshold. `0` disables the feature.|
|`BBBoostLowRatioFilesBy`|0|Additive queue-score bonus applied when `BBBoostLowRatioFiles` matches. `0` disables the effect.|
|`BBDeboostLowIDs`|0|If set above `1`, divide the score of actual LowID clients by this value. `0` or `1` disable the penalty.|
|`BBSessionMaxTrans`|68719476736|Values above `100` indicate how much data in bytes is allowed for a client to download in a single session. Default is `64 GiB`, matching the current broadband opcode value.|
|`BBSessionMaxTrans`|1-100|Values in the range of `1` to `100` indicate how much data in percentage of the size of the file being uploaded is allowed for a client to download in a single session. In example, set to `33` to allow roughly a third of the file size to be uploaded in a single session.|
|`BBSessionMaxTrans`|0|Disables transfer-based session rotation.|
|`BBSessionMaxTime`|10800000|Indicates how much time (in ms) is allowed for a client to download in a single session, default is `3 hrs`.|
|`BBSessionMaxTime`|0|Disables time-based session rotation.|

Please note that the rest of the broadband behavior is now derived from the
current upload budget and slot target rather than being controlled by the wider
set of hidden settings that existed on `v0.60d-dev`.

The Tweaks page presents the session transfer limit in a friendlier way:

- `Disabled`
- `Percent of file size`
- `Absolute limit (MiB)`

and presents the session time limit in **minutes** instead of raw milliseconds.

The useful behavioral pieces that were kept are:

- limit the normal upload slots to a sensible amount
- make full usage of the upload bandwidth
- recycle persistently weak uploaders instead of opening many more slots
- let strict seeders boost low-ratio files and penalize LowID clients in queue score
- allow configurable rotation by transfer amount or time

Your best take to fully understand the logic is to **review the code itself**:

- old branch reference: https://github.com/itlezy/eMule/commits/v0.60d-dev
- current branch design notes: [`BROADBAND.md`](docs/BROADBAND.md)

We have not much time to test, so be sensible.

### Ratio Columns

This branch also reintroduces a small part of the old broadband UI in a cleaned
up form:

- `All-Time Ratio`
- `Session Ratio`
- `Cooldown` on upload and queue lists

The ratio columns are file-level metrics:

- `All-Time Ratio` = all-time uploaded bytes for the file divided by the file size
- `Session Ratio` = current-session uploaded bytes for the file divided by the file size

These columns are available in:

- Shared Files
- Uploading
- On Queue

### Example Settings

#### More aggressive

```ini
BBMaxUpClientsAllowed=6
BBBoostLowRatioFiles=0.5
BBBoostLowRatioFilesBy=50
BBDeboostLowIDs=4
BBSessionMaxTrans=33
BBSessionMaxTime=7200000
```

#### More Relaxed

```ini
BBMaxUpClientsAllowed=12
BBSessionMaxTrans=68719476736
BBSessionMaxTime=10800000
```

#### Control over Max Trans and Max Time

Bear in mind that you can adjust the max trans and the max time, so to decide
the best upload strategy for you, by rotating clients every x MiB, every x
seconds, or every x percent of file.

```ini
BBSessionMaxTrans=268435456
BBSessionMaxTime=10800000
```

Or as mentioned above, you can set the max trans to a percentage of the file
being uploaded, like in example a third:

```ini
BBSessionMaxTrans=33
```

#### Strict seeder bias

If you want to prioritize files you have uploaded the least and push LowIDs back
in the queue, you can enable the score policy knobs below:

```ini
BBBoostLowRatioFiles=0.5
BBBoostLowRatioFilesBy=50
BBDeboostLowIDs=4
```

With the example above:

- files whose all-time upload ratio is below `0.5` get a `+50` score bonus
- actual LowID clients have their queue score divided by `4`

## Get an High ID

As you might know, eMule servers assign you a Low or an High ID based on the
fact you are able to receive inbound connections. So how to get an High ID?
There are a number of guides to help you with this, but let me summarize few
steps.

Getting an High ID is important for a number of reasons and to improve your
overall download/upload experience.

Ensure you got the UPnP option active in eMule's connection settings, this
should work in most scenarios.

![2022-09-19 09_10_51-Window](https://user-images.githubusercontent.com/24484050/190966375-c8a2839c-67ec-44e7-9eb3-39a392de176e.png)

Some users might be behind network infrastructure that does not support it, so
a very good option would be to get a VPN service that supports port mapping.
Some do support UPnP, do a google search _vpn with port forwarding_. This has
the benefit to help you with privacy.

### Windows Firewall Helper

For Windows 10 and Windows 11, this repository also ships a manual helper to
create a Windows Defender Firewall allow rule for `emule.exe`. Use the `.cmd`
launcher so the script always runs through Windows built-in `powershell.exe`:

```powershell
.\scripts\network-firewall-opener.cmd -ExePath .\srchybrid\x64\Debug\emule.exe
```

To remove that managed rule again:

```powershell
.\scripts\network-firewall-opener.cmd -Remove
```

Run the script as administrator. If `-ExePath` is omitted, it searches the
repository for `emule.exe` and uses the default build output when it finds one.
It creates one inbound app rule named `eMule` for all firewall profiles.

![2022-09-19 09_02_57-Window](https://user-images.githubusercontent.com/24484050/190966620-94fd4903-9358-4891-8f5c-f75dc93bb5f3.png)

Once you are setup you can check the port forwarding status with
[UPnP Wizard](https://www.xldevelopment.net/upnpwiz.php), to ensure the ports
are correctly setup.

Then you can verify online if you are able to receive inbound connections on one
of these websites:

- https://www.yougetsignal.com/tools/open-ports/
- https://portchecker.co/check

## Building

Please see the companion build workspace one level up from this repo for build
instructions and scripts if you are interested in performing a build.

This is the broadband branch for features and experimentation, but the build is
validated through the parent workspace scripts rather than by building this repo
standalone.

Enjoy and contribute!

## Summary of changes

### opcodes

The one main difference is to allow more appropriate values for high-speed
connections and large files in:

```c
SESSIONMAXTRANS
SESSIONMAXTIME
MAX_UP_CLIENTS_ALLOWED
UPLOAD_CLIENT_MAXDATARATE
```

The current values on this branch are:

- `SESSIONMAXTRANS = 64 GiB`
- `SESSIONMAXTIME = 3 hours`
- `UPLOAD_CLIENT_MAXDATARATE = 1 MiB/s`
- `MAX_UP_CLIENTS_ALLOWED = 50`

As the debate is long, my take on the matter is that it is best to upload at a
high speed to few clients rather than uploading to tenths of clients at
ridicolously low speeds. In addition to that it is likely best to let clients
download entire files or meaningful parts of them, so `SESSIONMAXTRANS` and
`SESSIONMAXTIME` are increased.

Some have argued that these values were marked as _do not change_ in the
opcodes file, but please consider that this software was literally designed with
small files and slow connections in mind. The sole purpose of this branch is to
seed back to the ED2K network with settings that cope better with nowadays
large files and broadband links.

### UploadQueue

With the philosophy of keeping changes to a minimum:

- Added logic to keep the upload slots near a configurable broadband target
  instead of allowing the old controller to keep growing them.
- Added logic to remove from the upload slots clients that have been below a
  reasonable download rate for a certain period of time, so to give more
  priority to fast downloaders, which should also be fast uploaders to an extent
  so then they can propagate files quicker if they get it first.
- The *slower* clients will be able to be back in the slots once the faster
  have been served again, but not immediately, thanks to a short cooldown after
  slow-slot eviction.
- Added configurable session rotation by transfer amount or session time.
- Added support for `BBSessionMaxTrans=1..100` as a percentage of the currently
  uploaded file size.
- When publishing shared files to a server, files with a lower all-time upload
  ratio are preferred within the same upload-priority bucket.

### What has been dropped from `v0.60d-dev`

This branch intentionally does **not** carry over:

- the extra hidden broadband knobs for queue score boosting/deboosting
- the auto-friend management logic
- the wider upload/download/shared/queue UI field additions beyond the
  ratio/cooldown columns

The focus here is to keep the broadband upload controller isolated and
maintainable on top of `v0.72a`.

### For Linux

For Linux and other platforms, or Windows as well, please check friend project
https://github.com/mercu01/amule
