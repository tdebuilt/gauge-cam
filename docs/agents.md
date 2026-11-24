# Driving a gauge with an AI agent

[Français](agents.fr.md) · [Back to the README](../README.md)

An AI agent that can run commands (Claude Code, Codex and the like) can read the gauge, explain a
wrong reading and tune the dial for you. It uses the same [HTTP API](api.md) as the interface.

## Give the agent what it needs

1. **Network**: the agent runs on a computer that reaches the device, for example
   `http://192.168.1.50`.
2. **Address and password** in two environment variables, never in the conversation:

   ```sh
   export GAUGE_CAM_URL=http://192.168.1.50
   export GAUGE_CAM_PASSWORD='the interface password'   # empty when none is set
   ```

3. **The skill**: copy [`skills/gauge-cam/`](../skills/gauge-cam/SKILL.md) into the skills folder of
   your agent, for example `~/.claude/skills/gauge-cam/` for Claude Code or `~/.codex/skills/gauge-cam/`
   for Codex. It tells the agent how to call the API without showing the password, and the rules
   below.

Without the skill, point the agent at `GET /api/openapi.json`: the device describes every route and
setting itself, with the same rules in its introduction.

## The rules the agent follows

| Step | Call | Why |
|---|---|---|
| Look | `GET /api/status`, `/api/config`, `/api/photo.jpg`, `/api/log` | Understand before changing anything |
| Try | `POST /api/test` with draft settings | Reads the last photo, stores nothing |
| Ask | before and after shown to you | Nothing is saved without your approval |
| Save | `POST /api/config` with the changed keys | The answer lists what changed |
| Check | `POST /api/read` | New photos, real reading |
| Undo | `POST /api/config/undo` | Back to the settings before the change |

The agent never touches Wi-Fi, passwords or MQTT unless you ask, and asks before an update or a
restart.

## What you can ask

- "What is the oil level, and when was the last good reading?"
- "The level jumped this afternoon: why?" The agent reads the log (`Settings changed` lines, restarts,
  refused readings) and compares the photos.
- "Check the needle centre and the graduations on today's photo." The agent places them on the photo,
  tries them, and shows you the difference before saving.

## Safety

- Undo works for the last change; a second undo redoes it. Keep a copy of the settings too
  (*Settings → System → Export*): a reflash erases them.
- The interface password protects the whole API. Give the agent its own computer account, not
  yours, if it runs unattended.
