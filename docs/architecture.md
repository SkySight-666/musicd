# musicd Architecture Draft

## Core modules

- `MusicDaemon`: top-level lifecycle and composition root
- `IpcServer`: frontend-facing command/event transport
- `QueueManager`: queue mutation and auto-next policy
- `PlaybackEngine`: ffmpeg-backed playback core (next)
- `LyricEngine`: LRC parsing and current line sync (next)
- `StateStore`: persist queue, track, position, and settings (next)

## Process model

The daemon should become the single source of truth.
Frontend pages can attach and detach without affecting playback continuity.

## Near-term implementation order

1. Unix domain socket command server
2. JSON message schema
3. Playback engine process wrapper around ffmpeg
4. Periodic progress updates + current lyric line
5. State persistence and restart recovery
