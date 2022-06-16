# eMule - broadband branch
The initial purpose of this project was to provide an eMule repository (including dependencies) that is ready to build and update the dependent libraries when possible. This branch specifically focuses on providing a build that is better suited to address nowadays file sizes and broadband availability. Default hard-coded parameters of eMule were better suited for small-files/slow-connections, leading to very low per-client transfer rates by nowadays standards.  
The focus here is to maximise throughput for broadband users, to optimize seeding.  
The focus is as well to introduce the least amount of changes to preserve the original quality and stability of the client.  

## Installation
Just get the zip archive from the [release page](https://github.com/itlezy/eMule/releases/tag/eMule_v0.60d-broadband) and replace your current executable.  
Be sure to make a backup of `%LOCALAPPDATA%\eMule` first, as this is a "beta" build which requires testing, even if the amount of changes are minimal some external dependencies have been bumped up at compiler flags made uniform to optimize the runtime.  

Download the latest Windows x64 release from https://github.com/itlezy/eMule/releases/tag/eMule_v0.60d-broadband

### Optimal Settings
Really the one recommendation would be to set the values of bandwidth capacity and the **upload limit**, plus a limit of max connections if you wish so. Other settings, as you please.  
Be fair about it, the purpose is to **maximise seeding**, so be generous with your bandwidth and set it as much as possible based on your connection.

![2022-06-14 14_05_11-Window](https://user-images.githubusercontent.com/24484050/173573013-6a76d50f-f168-4a81-83c7-888ee3de6b6a.png)

### Configuration
**Max upload slots** are configurable from ini file. Just launch the eMule exe once, close it, and then edit the ini file:

`%LOCALAPPDATA%\eMule\config\preferences.ini`

The key to edit is the following:

`MaxUpClientsAllowed=8`

You can adjust this limit according to your bandwitdh and I/O preferences, suggested ranges are 5, 8, 12, 24, 36  
This setting allows to set a maximum amount of upload slots that will never be surpassed, to reduce I/O contention in both disk and network.  

### IP 2 Country
As some other minor change to the upload list, the IP 2 Country is being added back. At some point it will be updated to latest formats, but for now just google `GeoIPCountryWhois.csv` to download a reasonably recent file and place it in your `%LOCALAPPDATA%\eMule\config`  

## Building
Please see this repo [eMule-build](https://github.com/itlezy/eMule-build) for build instructions and scripts if you are interested in performing a build. This is the broadband branch for features and experimentation, but if you want to start from the base 0.60d here's the [build branch](https://github.com/itlezy/eMule/tree/v0.60d-build) which contains no changes.  
Enjoy and contribute!

## Summary of changes
### opcodes
The one main difference is fiddling the values of  

```c
SESSIONMAXTRANS
SESSIONMAXTIME
MAX_UP_CLIENTS_ALLOWED
UPLOAD_CLIENT_MAXDATARATE
```
  
to more appropriate values for high-speed connections and large files, and by actually applying the limit of `MAX_UP_CLIENTS_ALLOWED`, which can also be configured from ini file.  
As the debate is long, my take on the matter is that it is best to upload at a high-speed to few clients rather than uploading to tenths of clients at ridicolously low speeds. In addition to that it is likely best to let clients download entire files, so `SESSIONMAXTRANS` and `SESSIONMAXTIME` are increased.  

### UploadQueue
Added also a bit of logic to remove from the upload slots clients that have been below a download rate for a certain period of time, so to give more priority to fast downloaders, which should also be fast uploaders to an extent so then they can propagate files quicker if they get it first. The *slower* clients will be able to be back in the slots once the fastest have been served.  

### More fields
Added some more technical fields to the Download and Upload Slots (like progress %), Queue, Shared Files (including a ratio column, similar to BT).  

![2022-06-16 12_08_58-Window](https://user-images.githubusercontent.com/24484050/174048587-d5ee8449-8714-47e9-bd3e-695dcf2c6573.png)
![2022-06-16 12_10_15-Window](https://user-images.githubusercontent.com/24484050/174048689-9c5331da-3875-49be-b90f-61e2645af831.png)
![2022-06-16 12_11_24-Window](https://user-images.githubusercontent.com/24484050/174048775-8f2f56c8-bf71-421e-a532-ebb6dcec3a6e.png)


### Sample Results
This would be a normal upload slots list, in a scenario of 50Mbps capacity

![2022-06-14 14_15_35-Window](https://user-images.githubusercontent.com/24484050/173574898-44543e7e-9fde-484a-9851-fd88fd0286cb.png)

