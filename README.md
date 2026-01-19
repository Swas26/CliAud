# CliAud

**CliAud** is a lightweight macOS utility that lets you **instantly switch audio output devices** using a global keyboard shortcut — using a CLI and a small background agent.

It is designed to be:
- fast
- reliable
- persistent across reboots
- transparent (no UI overlays, no hacks)

---

##  Features

- List available audio output devices
- Select which devices to cycle between
- Switch outputs instantly via a global hotkey
- Runs automatically at login
- Clean CLI-first design
- Native CoreAudio implementation

---

## Why CliAud?

macOS does not provide:
- a built-in keyboard shortcut to switch audio outputs
- a simple, scriptable way to manage audio routing

CliAud solves this without menu bar clutter, accessibility hacks, or bloated UI apps.

---

## How It Works

CliAud consists of two components:

### `cliaud` (CLI)
Handles all audio logic:
- device discovery
- device selection
- cycling logic
- configuration persistence

### `cliaud-agent` (Background Agent)
- runs at login
- listens for a global hotkey (**Cmd + Option + 9**)
- triggers `cliaud cycle`

---

##  Installation (Recommended)

### Requirements
- macOS (Apple Silicon)
- Homebrew

### Install via Homebrew
```bash
brew tap Swas26/cliaud
brew install cliaud
```

### Start the background agent (runs at login)
```bash
brew services start cliaud
```

The hotkey is now active.

---

##  Default Hotkey

```
Cmd + Option + 9
```

Each press cycles to the next saved audio output device. (can change this is later verisona)

---

##  Usage

### List available output devices
```bash
cliaud list
```

Example output:
```
[ 0 ]-*DELL 
[ 1 ]  MacBook Speakers
[ 2 ]  AirPods
```

---

### Add devices to the cycle list
```bash
cliaud add 0,1
```

---

### Show saved devices
```bash
cliaud show
```

---

### Cycle output manually
```bash
cliaud cycle
```

(This is what the hotkey triggers.)

---

### Clear saved devices
```bash
cliaud clear
```

---

## Configuration

Configuration is stored at:
```
~/.config/cliaud/config.txt
```

This file contains:
- saved device UIDs
- (future versions) hotkey configuration

Manual editing is usually unnecessary.

---

##  Managing the Background Agent

### Stop the agent
```bash
brew services stop cliaud
```

### Restart the agent
```bash
brew services restart cliaud
```

### Run agent manually (for debugging)
```bash
/opt/homebrew/opt/cliaud/bin/cliaud-agent
```

### Logs
```
/opt/homebrew/var/log/cliaud-agent.err.log
/opt/homebrew/var/log/cliaud-agent.log
```

---

## Architecture (High Level)

```
┌───────────────┐
│  User Hotkey  │  Cmd + Option + 9
└───────┬───────┘
        ↓
┌───────────────────┐
│ cliaud-agent      │  (LaunchAgent)
│ - listens hotkey  │
│ - spawns cliaud   │
└───────┬───────────┘
        ↓
┌───────────────────┐
│ cliaud (CLI)      │
│ - CoreAudio APIs  │
│ - switches output │
└───────────────────┘
```

---

## Uninstall

```bash
brew services stop cliaud
brew uninstall cliaud
brew untap Swas26/cliaud
```
---

## Author

Built by **Swas**  
GitHub: https://github.com/Swas26
