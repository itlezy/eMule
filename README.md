# eMule - broadband branch
The initial purpose of this project was to provide an eMule repository (including dependencies) that is ready to build and update the dependent libraries when possible.  
This development branch specifically focuses on providing a build that is better suited to address nowadays file sizes and broadband availability. Default hard-coded parameters of eMule were better suited for small-files/slow-connections, leading to very low per-client transfer rates by nowadays standards.  
The focus here is to maximise throughput for broadband users, to **optimize seeding**. This is a seeder mod, designed to seed back to the E2DK network.  
The focus is as well to introduce the least amount of changes to preserve the original quality and stability of the client.  

## Installation
### eMule
Reccomended to install the latest eMule Community version, but any 0.50+ should be fine as well. https://github.com/irwir/eMule/releases  

### Broadband Edition
Just get the zip archive from the [release page](https://github.com/itlezy/eMule/releases/tag/eMule_v0.60d-broadband) and replace your current eMule executable.  
Be sure to make a backup of `%LOCALAPPDATA%\eMule` first, as this is a "beta" build which requires testing, even if the amount of changes are minimal some external dependencies have been bumped up at compiler flags made uniform to optimize the runtime.  

**Download** the latest Windows x64 release from https://github.com/itlezy/eMule/releases/tag/eMule_v0.60d-broadband  

### Optimal Settings
Really the one recommendation would be to set the values of bandwidth capacity and the **upload limit**, plus a limit of max connections if you wish so. Other settings, as you please.  
Be fair about it, the purpose is to **maximise seeding**, so be generous with your bandwidth and set it as much as possible based on your connection.

![2022-06-14 14_05_11-Window](https://user-images.githubusercontent.com/24484050/173573013-6a76d50f-f168-4a81-83c7-888ee3de6b6a.png)

### Upload Slots Settings
**Max upload slots** are configurable from ini file. Just launch the eMule exe once, close it, and then edit the ini file:

Run notepad `%LOCALAPPDATA%\eMule\config\preferences.ini`

The key to edit is the following:

`BBMaxUpClientsAllowed=8`

You can adjust this limit according to your bandwitdh and I/O preferences, suggested ranges are 5, 8, 12, 24, 36, .. up to you  
This setting allows to set a maximum amount of upload slots that will never be surpassed, to reduce I/O contention in both disk and network. If you are seeding from multiple disk drives or SSD drives, then you can bump up the upload slots as you deem fit.  

### Broadband Settings
Please find below all preferences.ini settings.  
|Setting|Default|Description|
|---|---|---|
|`BBMaxUpClientsAllowed`|5|Upper limit of concurrent uploads|
|`BBMaxUploadTargetFillPerc`|75|Given the max upload speed (which we reccomend to set!), indicates the target % to fill. Below that overall upload speed target, the slow client logic will take place and slow clients will be deprioritized from the upload slots. Slow clients will be deprioritized only when there are clients in the waiting list|
|`BBSlowRateTolerancePerc`|133|Given the max up clients allowed, will identify slow clients based on the formula `BBSlowRateTolerancePerc / 100.0f * (1 + BBMaxUpClientsAllowed)` You can monitor the slowness of a client by the _caught slow_ column in the upload list|
|`BBSlowDownloaderSampleDepth`|4|Indicates how many samples are taken in account to mark a client as "slow downloader". This provides a temporal depth to mark slow clients and remove them from the upload slots. Suggested values are between 2 (aggressive) and 12 (more relaxed)|
|`BBSessionMaxTrans`|68719476736|Indicates how much data in bytes is allowed for a client to download in a single session. Adjust based on the files you plan to share, default is 64Gb|
|`BBSessionMaxTime`|10800000|Indicates how much time is allowed for a client to download in a single session, default is 3hrs|
|`BBUploadClientMaxDataRate`|1048576|Indicates the target max data rate used in a number of calculations done by the upload throttler and it is also used to mark slow clients when an upload limit is not set. Suggested values are between 256k and 1Mb|
|`BBBoostLowRatioFiles`|2|Indicates the ratio threshold below which files are prioritized in the queue by adding `BBBoostLowRatioFilesBy=400`|
|`BBBoostFilesSmallerThan`|16|Speaks for itself (in Mb)|
|`BBDeboostLowIDs`|3|Deboost LowID clients in the queue by this factor|
|`BBDeboostHighRatioFiles`|3|Deboost files higher than this ratio by a factor of the ratio itself|

Your best take to fully understand the logic is to **review the [code itself](https://github.com/itlezy/eMule/commits/v0.60d-dev)** `git diff origin/v0.60d-build origin/v0.60d-dev`  We have not much time to test, so be sensible  

## Get an High ID
As you might know, eMule servers assign you a Low or an High ID based on the fact you are able to receive inbound connections. So how to get an High ID? There are a number of guides to help you with this, but let me summarize few steps. Getting an High ID is important for a number of reasons and to improve your overall download/upload experience.  
\
Ensure you got the UPnP option active in eMule's connectiong settings, this should work in most scenarios.  
![2022-09-19 09_10_51-Window](https://user-images.githubusercontent.com/24484050/190966375-c8a2839c-67ec-44e7-9eb3-39a392de176e.png)
\
\
Some users might be behind network infrastructure that does not support it, so a very good option would be to get a VPN service that supports port mapping. Some do support UPnP, do a google search _vpn with port forwarding_. This has the benefit to help you with privacy.  
![2022-09-19 09_02_57-Window](https://user-images.githubusercontent.com/24484050/190966620-94fd4903-9358-4891-8f5c-f75dc93bb5f3.png)
\
\
Once you are setup you can check the port forwarding status with [UPnP Wizard](https://www.xldevelopment.net/upnpwiz.php), to ensure the ports are correctly setup.  
Then you can verify online if you are able to receive inbound connections on one of these websites https://www.yougetsignal.com/tools/open-ports/ or https://portchecker.co/check  

## IP 2 Country
As some other minor change to the upload list, the IP 2 Country is being added back. At some point it will be updated to latest formats, but for now just google `GeoIPCountryWhois.csv` to download a reasonably recent file and place it in your `%LOCALAPPDATA%\eMule\config`  

## Building
Please see this repo [eMule-build](https://github.com/itlezy/eMule-build) for build instructions and scripts if you are interested in performing a build. This is the broadband branch for features and experimentation, but if you want to start from the base 0.60d here's the [build branch](https://github.com/itlezy/eMule/tree/v0.60d-build) which contains no changes.  
Enjoy and contribute!

## Summary of changes
### opcodes
The one main difference is allow to fiddle with the values of  

```c
SESSIONMAXTRANS
SESSIONMAXTIME
MAX_UP_CLIENTS_ALLOWED
UPLOAD_CLIENT_MAXDATARATE
```
  
to more appropriate values for high-speed connections and large files, and by actually applying the limit of `MAX_UP_CLIENTS_ALLOWED`, which can also be configured from ini file.  
\
As the debate is long, my take on the matter is that it is best to upload at a high-speed to few clients rather than uploading to tenths of clients at ridicolously low speeds. In addition to that it is likely best to let clients download entire files, so `SESSIONMAXTRANS` and `SESSIONMAXTIME` are increased.  
\
Some have argued that these values were marked as _do not change_ in the opcodes file, but please consider that this software was literally designed with 3Mb average files and 56k connections in mind, running on 100MHz computers. The sole purpose of this mod is to seed back to the E2K network, which has been slowly fading very likely because the clients are not correctly set to cope with nowadays large files.  

### UploadQueue
With the philosophy of keeping changes to a minimum:
- Added a bit of logic to remove from the upload slots clients that have been below a download rate for a certain period of time, so to give more priority to fast downloaders, which should also be fast uploaders to an extent so then they can propagate files quicker if they get it first. The *slower* clients will be able to be back in the slots once the fastest have been served
- Added a "ratio" display in upload slot, upload queue and shared files, so to provide evidence on the seeding ratio of files. Low ratio files from the queue will be "bumped" to an higher score, to spread quicker  

### More stuff
Added some more technical fields to the Download and Upload Slots (like progress %), Queue, Shared Files (including a ratio column, similar to BT). Upload compression has been disabled, which logic was not applicable to nowadays extensions, thanks for the idea to https://github.com/mercu01/amule  

#### Downloads
![2022-06-16 12_08_58-Window](https://user-images.githubusercontent.com/24484050/174048587-d5ee8449-8714-47e9-bd3e-695dcf2c6573.png)

#### Uploads
Few more fields have been added for who likes to monitor how their seeding status is going. Check out the context menu as well.  
![2022-09-18 19_31_28-Window](https://user-images.githubusercontent.com/24484050/190920794-f86db4ab-20fc-4b31-bdeb-5c1f38d95e66.png)


#### Shared Files
![2022-06-16 12_11_24-Window](https://user-images.githubusercontent.com/24484050/174048775-8f2f56c8-bf71-421e-a532-ebb6dcec3a6e.png)

#### Queue
![2022-06-16 12_17_22-Window](https://user-images.githubusercontent.com/24484050/174049352-d1b664b5-f952-421f-b720-e4d5ab8b3d42.png)


### Sample Results
This would be a normal upload slots list, in a scenario of 50Mbps capacity. So rather than having tenths of uploads, you can get few uploads at top speed.  

![2022-06-16 12_30_07-Window](https://user-images.githubusercontent.com/24484050/174051485-6e3e2cb6-5a11-4930-bcf4-0e4c49f4d71f.png)

### All Settings (suggested)
![2022-09-18 19_44_23-Window](https://user-images.githubusercontent.com/24484050/190921214-bc859198-3552-436b-8d08-49a212dbfb76.png)
\
![2022-09-18 19_43_57-Window](https://user-images.githubusercontent.com/24484050/190921216-1c7d3a0e-732b-41d7-9c94-4345e99bcc94.png)
\
![2022-09-18 19_45_58-Window](https://user-images.githubusercontent.com/24484050/190921217-35313e76-a1c5-4bfd-a0eb-83924b88dfa9.png)
\
![2022-09-18 19_45_21-Window](https://user-images.githubusercontent.com/24484050/190921218-b07edb40-d85d-4170-827b-e754da82cb8b.png)
\
![2022-09-18 19_44_56-Window](https://user-images.githubusercontent.com/24484050/190921219-5343b7c5-09ac-49d1-b64a-d3ff17443072.png)
