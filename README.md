# musicd

Standalone native music daemon for LoliAPP.

## Language

- English: this file
- Chinese: `README.zh-CN.md`

## Goals

- Keep playback alive across pages
- Keep playback alive after the miniapp exits
- Own queue, progress clock, lyric timeline, and auto-next
- Expose a small IPC surface for the Falcon frontend

## Current layout

- `src/main.cpp`: daemon entrypoint
- `src/music_daemon.cpp`: daemon bootstrap and lifecycle
- `src/playback_engine.cpp`: `ffmpeg | aplay` playback pipeline
- `src/audio_output_manager.cpp`: ALSA / BlueALSA output scan
- `include/musicd/music_daemon.h`: top-level daemon interface
- `bin/musicctl`: local socket control helper

## Working features

- Background daemon over Unix socket: `/tmp/musicd.sock`
- Playback pipeline: `ffmpeg` decode, `aplay` output
- Output detection: Bluetooth A2DP first, otherwise ALSA/default
- Output hot-change handling: auto pause on device change
- Queue with auto-next
- Control commands: `PLAY`, `ENQUEUE`, `PAUSE`, `RESUME`, `STOP`, `NEXT`, `LIST_OUTPUTS`, `SET_OUTPUT`, `GET_STATE`, `QUIT`

## Quick start

```sh
cd /home/skysight/musicd
./build.sh
./build/s6/musicd
```

Another shell:

```sh
/home/skysight/musicd/bin/musicctl state
/home/skysight/musicd/bin/musicctl outputs
/home/skysight/musicd/bin/musicctl play 'https://example.com/test.mp3'
```

## Cross compile

For `s6`:

```sh
export CROSS_TOOLCHAIN_PREFIX="$HOME/toolchain/arm-unknown-linux-gnueabihf/bin/arm-unknown-linux-gnueabihf-"
cd /home/skysight/musicd
./build.sh
```

Or let the wrapper infer it:

```sh
cd /home/skysight/musicd
MUSICD_DEVICE=s6 ./build.sh
```

For `a6`:

```sh
cd /home/skysight/musicd
MUSICD_DEVICE=a6 ./build.sh
```

## Next steps

1. Add progress event push instead of polling-only state reads
2. Add lyric parsing / current line sync
3. Add persistent queue / resume recovery

## License

This project is licensed under the GNU General Public License v3.0 or later (`GPL-3.0-or-later`).
See `LICENSE` for details.
