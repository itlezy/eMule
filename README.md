# eMule - broadband branch
The initial purpose of this project was to provide an eMule repository (including dependencies) that is ready to build and update the dependent libraries when possible. This branch specifically focuses on providing a build that is better suited to address nowadays file sizes and broadband availability. Default hard-coded parameters of eMule were better suite for small-files/slow-connections, leading to very low per-client transfer rates by nowadays standards.
The focus here is to maximise throughput for broadband users, to optimize seeding.
The focus is as well to introduce the least amount of changes to preserve the original quality and stability of the client.

## Building
Please see this repo for build instructions and scripts if you are interested in performing a build [eMule-build](https://github.com/itlezy/eMule-build)
Enjoy and contribute!

## Optimal Settings
Really the one reccomendation would be to set capacity and upload limit, plus a limit of max connections. Other settings as you please.
![2022-06-14 13_57_10-Window](https://user-images.githubusercontent.com/24484050/173571775-0685e1e4-92af-4718-952e-ec27facdc0c7.png)

## Summary of changes
The only real difference is fiddling the values of

```c
SESSIONMAXTRANS
SESSIONMAXTIME
MAX_UP_CLIENTS_ALLOWED
UPLOAD_CLIENT_MAXDATARATE
```

to more appropriate values for high-speed connections and large files. In future releases these values might be either configurable from setting file or derived by formula based on the stated bandwidth of the user
