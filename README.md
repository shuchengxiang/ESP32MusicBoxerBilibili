# ESP32MusicBoxerBilibili
通过输入关键字自动播放bilibili音乐的ESP32播放器，通过连接蓝牙播放

## 实现简介
### 服务端
服务端主要作用为与设备端进行交互，通过修改配置来改变设备端的行为，比如调节音量、播放歌曲、暂停等；同时服务端实现了通过关键字或bvid搜索B站第一条视频来播放音频的方法，获取B站视频链接后通过ffmpeg进行解码再编码成aac格式推流，推流后由设备端接收后播放
### 设备端
设备端为ESP32开发板，由C++进行编写，IDE使用VSCode+platformio,所有的依赖都存在于platformio.ini中，注意arduino版本为2.0.5.4，已在platformio.ini中标出，其他IDE注意框架版本，因为高版本往往会导致问题；设备端通过UDP连接与服务端进行交互，通过不断轮询的方式来确认服务端是否下达了新命名，并且做出相应的行为，具体细节可查看代码

## 使用方法
### 硬件环境
1. 环境准备为一台服务器（个人电脑或其他服务器设备）
2. 一个ESP32开发板
3. 一副蓝牙耳机或蓝牙音箱
4. wifi环境
### 软件环境
1. ffmpeg安装在服务器上，并可以使用命令行ffmpeg直接调用
2. 服务器有python3环境，测试环境为3.10
### 使用方法
1. 修改设备端代码（src/main.cpp）中的wifi名称、密码
2. 修改设备端代码（src/UDPMessageController.cpp）中的SVR_ADDRESS字段，将IP改为自己的服务器地址
3. 将设备端代码编译、烧录至esp32开发板
4. 将蓝牙设备打开搜索状态靠近esp32开发板，开发板会自动连接最近的蓝牙设备并记录
5. 修改代码server/bilibili_operate.py中的HEADERS.cookie字段，可以参照此种方式获取https://github.com/SocialSisterYi/bilibili-API-collect/blob/master/docs/search/search_request.md，直接用浏览器访问 https://bilibili.com,然后打开开发者工具，直接把那段很长的cookie字段放入其中即可
6. 启动服务端，在服务端目录运行python server/server.py
7. 设备端正常连接服务端之后，会根据设备id在server/data目录下生成一个json文件，在服务器端直接修改这个文件保存即可在设备端生效，字段描述如下：
    1. "volume": 设备音量，填写一个数字
    2. "paused": 是否暂停1为暂停， 0为播放
    3. "music_url": 暂时无用,
    4. "server_url": 局域网中的服务端IP端口，如http://192.168.137.1:8180,
    5. "bvid": B站Bvid号，填写后播放Bvid对应的视频音频,
    6. "keyword": 搜索关键字，填写后自动播放B站搜索结果的第一条结果"洛天依 歌行四方"

## 参考
本程序主要参考https://github.com/noolua/BLPlayer 项目，进行了部分裁剪与修改，原项目有更多的功能可以参考，大家可以移步至此项目查看更多功能，感谢大神的贡献。其余也参考了BLPlayer项目中的参考项目，在下方再次列出：
1. https://github.com/SocialSisterYi/bilibili-API-collect
2. https://github.com/pschatzmann/ESP32-A2DP
3. https://github.com/earlephilhower/ESP8266Audio

## 未来的样子
本项目最初的目的是实现一个语音控制播放音乐的“B站小音箱”，现在由于有其他的事情只能暂时停止，所以将项目提前发布出来记录，便于以后继续。本项目实现了搜索播放部分，其余只需接入麦克风和语音识别接口就可以了，接入麦克风的部分也已经调通（还未接入语音识别API），可参看另一个项目，如果读者有兴趣实现可以继续研究一下

## 许可证
* GPL-3.0 license