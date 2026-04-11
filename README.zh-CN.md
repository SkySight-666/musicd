# musicd

`musicd` 是为 LoliAPP 设计的独立原生音乐守护进程。

## 目标

- 页面切换后继续播放
- 小程序退出后继续播放
- 后端统一维护队列、进度时钟、自动下一首
- 通过 IPC 提供稳定控制接口给前端

## 当前目录

- `src/main.cpp`: 进程入口
- `src/music_daemon.cpp`: 主循环与命令分发
- `src/playback_engine.cpp`: `ffmpeg | aplay` 播放链路
- `src/audio_output_manager.cpp`: ALSA / BlueALSA 输出扫描
- `include/musicd/music_daemon.h`: 守护进程接口
- `bin/musicctl`: 控制脚本（构建后自动复制到 `build/<device>/musicctl`）

## 已实现能力

- Unix Socket 后台服务：`/tmp/musicd.sock`
- 音频播放：`ffmpeg` 解码，`aplay` 输出
- 输出检测：蓝牙 A2DP 优先，缺失时回退 ALSA/default
- 输出变更保护：检测到声卡/蓝牙变化自动暂停
- 播放队列 + 自动下一首
- 命令接口：
  - `PLAY`
  - `ENQUEUE`
  - `PAUSE`
  - `RESUME`
  - `STOP`
  - `NEXT`
  - `LIST_OUTPUTS`
  - `SET_OUTPUT`
  - `GET_STATE`
  - `QUIT`

## 快速开始

```sh
cd /home/skysight/musicd
./build.sh
./build/s6/musicd
```

另开一个终端：

```sh
/home/skysight/musicd/build/s6/musicctl state
/home/skysight/musicd/build/s6/musicctl outputs
/home/skysight/musicd/build/s6/musicctl play 'https://example.com/test.mp3'
```

## 交叉编译

`s6`:

```sh
export CROSS_TOOLCHAIN_PREFIX="$HOME/toolchain/arm-unknown-linux-gnueabihf/bin/arm-unknown-linux-gnueabihf-"
cd /home/skysight/musicd
./build.sh
```

或让脚本自动推断：

```sh
cd /home/skysight/musicd
MUSICD_DEVICE=s6 ./build.sh
```

`a6`:

```sh
cd /home/skysight/musicd
MUSICD_DEVICE=a6 ./build.sh
```

## 后续计划

1. 增加进度事件推送，减少纯轮询开销
2. 接入歌词解析与逐行同步
3. 增加队列持久化与重启恢复

## 开源协议

本项目采用 GNU General Public License v3.0 or later（`GPL-3.0-or-later`）。
详见 `LICENSE` 文件。
