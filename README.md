# eMule - broadband branch
The initial purpose of this project was to provide an eMule repository (including dependencies) that is ready to build and update the dependent libraries when possible. This branch specifically focuses on providing a build that is better suited to address nowadays file sizes and broadband availability. Default hard-coded parameters of eMule were better suited for small-files/slow-connections, leading to very low per-client transfer rates by nowadays standards.  
The focus here is to maximise throughput for broadband users, to optimize seeding.  
The focus is as well to introduce the least amount of changes to preserve the original quality and stability of the client.  

## Installation
Just get che zip from the release and replace your current executable.  
Be sure to make a backup of `%LOCALAPPDATA%\eMule` first, as this is a "beta" build which requires testing, even if the amount of changes are minimal some external dependencies have been bumped up at compiler flags made uniform to optimize the runtime.  

Download the latest Windows x64 release from https://github.com/itlezy/eMule/releases/tag/eMule_v0.60d-broadband

### Optimal Settings
Really the one reccomendation would be to set capacity and **upload limit**, plus a limit of max connections. Other settings, as you please.  
Be fair about it, the purpose is to **maximise seeding**, so be generous with your bandwidth.

![2022-06-14 14_05_11-Window](https://user-images.githubusercontent.com/24484050/173573013-6a76d50f-f168-4a81-83c7-888ee3de6b6a.png)

### Configuration
Max upload slots are configurable from ini file. Just launch the exe once, close it, and then edit the ini file:

`%LOCALAPPDATA%\eMule\config\preferences.ini`

The key to edit is the following:

`MaxUpClientsAllowed=8`

You can adjust this limit according to your bandwitdh and I/O preferences, suggested ranges are 5, 8, 12, 24, 36  
This setting allows to set a maximum amount of upload slots that will never be surpassed, to reduce I/O contention in both disk and network.  

### IP 2 Country
As some other minor change to the upload list, the IP 2 Country is being added back. At some point it will be updated to latest formats, but for now just google `GeoIPCountryWhois.csv` to download a reasonably recent file and place it in your `%LOCALAPPDATA%\eMule\config`  

## Building
Please see this repo for build instructions and scripts if you are interested in performing a build [eMule-build](https://github.com/itlezy/eMule-build)
Enjoy and contribute!

## Summary of changes
### opcodes
The one real difference is fiddling the values of  

```c
SESSIONMAXTRANS
SESSIONMAXTIME
MAX_UP_CLIENTS_ALLOWED
UPLOAD_CLIENT_MAXDATARATE
```
  
to more appropriate values for high-speed connections and large files, and by actually applying the limit of `MAX_UP_CLIENTS_ALLOWED`.  
As the debate is long, my take on the matter is that it is best to upload at a high-speed to few clients rather than uploading to tenths of clients at ridicolously low speeds. In addition to that it is likely best to let clients download entire files, so `SESSIONMAXTRANS` is increased.  

### UploadQueue
Added also a bit of logic to remove from the upload slots clients that have been below a download rate for a certain period of time, so to give more priority to fast downloaders, which should also be fast uploaders to an extent so then they can propagate files quicker if they get it first. The *slower* clients will be able to be back in the slots once the fastest have been served.  

### Sample Results
This would be a normal upload slots list

![2022-06-14 14_15_35-Window](https://user-images.githubusercontent.com/24484050/173574898-44543e7e-9fde-484a-9851-fd88fd0286cb.png)

