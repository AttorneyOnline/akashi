# akashi <img src="https://github.com/AttorneyOnline/akashi/blob/master/akashi/resource/icon/256.png" width=30 height=30>
A C++ server for Attorney Online 2<br><br>
![Code Format and Build](https://github.com/AttorneyOnline/akashi/actions/workflows/main.yml/badge.svg?event=push) [![Maintenance](https://img.shields.io/badge/Maintained%3F-yes-green.svg)](https://GitHub.com/AttorneyOnline/akashi/graphs/commit-activity) [![Linux](https://svgshare.com/i/Zhy.svg)](https://svgshare.com/i/Zhy.svg) [![Windows](https://svgshare.com/i/ZhY.svg)](https://svgshare.com/i/ZhY.svg)<br>

# Where to download
You can find the latest stable release on our [release page.](https://github.com/AttorneyOnline/akashi/releases)<br>
Nightly CI builds can be found at [Github Actions](https://github.com/AttorneyOnline/akashi/actions)<br>

# Support
Akashi has a maintained [Wiki](https://github.com/AttorneyOnline/akashi/wiki) for setup and configuration.<br><br>
For more support join the official Attorney Online 2 Discord!<br>
[![AttorneyOnline](https://discordapp.com/api/guilds/278529040497770496/widget.png?style=banner2)](https://discord.gg/wWvQ3pw)

# Build Instructions
If you are unable to use either CI or release builds, you can compile akashi yourself.<br>
Requires Qt >= 5.10, and Qt websockets

**Ubuntu 20.04/22.04** - Ubuntu 18.04 or older are not supported.
```
   sudo apt install build-essential qt5-default libqt5websockets5-dev
   git clone https://github.com/AttorneyOnline/akashi
   cd akashi
   qmake && make
```

# Contributors
![GitHub Contributors Image](https://contrib.rocks/image?repo=AttorneyOnline/akashi)
