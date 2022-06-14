# eMule - broadband branch
The initial purpose of this project was to provide an eMule repository (including dependencies) that is ready to build and update the dependent libraries when possible. This branch specifically focuses on providing a build that is better suited to address nowadays file sizes and broadband availability. Default hard-coded parameters of eMule were better suite for small-files/slow-connections, leading to very low per-client transfer rates by nowadays standards.
The focus here is to maximise throughput for broadband users, to optimize seeding.
The focus is as well to introduce the least amount of changes to preserve the original quality and stability of the client.

## Building
Please see this repo for build instructions and scripts if you are interested in performing a build [eMule-build](https://github.com/itlezy/eMule-build)
Enjoy and contribute!

## Optimal Settings
Really the one reccomendation would be to set capacity and upload limit, plus a limit of max connections. Other settings as you please.
Be fair about it, the purpose is to maximise seeding, so be generous with your bandwidth.

![2022-06-14 14_05_11-Window](https://user-images.githubusercontent.com/24484050/173573013-6a76d50f-f168-4a81-83c7-888ee3de6b6a.png)

## Summary of changes
The only real difference is fiddling the values of

```c
SESSIONMAXTRANS
SESSIONMAXTIME
MAX_UP_CLIENTS_ALLOWED
UPLOAD_CLIENT_MAXDATARATE
```

to more appropriate values for high-speed connections and large files, and by actually applying the limit of `MAX_UP_CLIENTS_ALLOWED`. As the debate is long, my take on the matter is that it is best to upload at a high-speed to few clients rather than uploading to tenths of clients at ridicolously low speeds.

In future releases these values might be either configurable from setting file or derived by formula based on the stated bandwidth of the user
