# musicd IPC Draft

## Direction

Frontend becomes a thin client.
The daemon owns playback state and survives page/app exit.

## Command examples

- `player.get_state`
- `player.play`
- `player.pause`
- `player.seek`
- `queue.replace`
- `queue.enqueue`
- `queue.next`
- `lyric.get_current`

## Event examples

- `player.state_changed`
- `player.progress`
- `queue.changed`
- `track.changed`
- `lyric.line_changed`
- `player.error`

## Notes

- Transport candidate: Unix domain socket
- Payload candidate: JSON first, binary later if needed
- The daemon should persist queue and current playback snapshot
