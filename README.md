# Build and run on linux
1. Set NDK path in build.sh.
2. Plug your Android phone.
3. call ./build.sh
4. See log by `adb logcat *:S EGL-DMABUF-Test:V`
![](./images/logs-screenshot.png)

# Run on windows
1. `cd win_scripts`
2. `.\run.bat`

# Results on some platforms.
| Android            | CPU   | GPU           | EGL version | libdmabufheap.so | EGL Error code |
| ------------------ | ----- | ------------- | ----------- | ---------------- | -------------- |
| 12(AOSP on Oriole) | 888   | Mali-G78 MP20 | 1.4         | ✔️               | ✔️             |
| 9                  | 845   | Adreno 630    | 1.4         | ❌                |                |
| 13                 | 865   | Adreno 650    | 1.5         | ✔️               | 0x300C         |
| 13                 | 888   | Mali-g78 MP10 | 1.5         | ✔️               | 0x300C         |
| 13                 | 8Gen1 | 730           | 1.5         | ✔️               | 0x300C         |
| 14                 | 8Gen3 | 750           | 1.5         | ✔️               | 0x300C         |