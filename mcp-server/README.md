# mbtop MCP Server

MCP (Model Context Protocol) server for controlling [mbtop](https://github.com/marcoxyz123/mbtop) process monitor. Allows AI assistants to tag, color, and filter processes in real-time.

## Features

- **Tag processes** with Nord Aurora colors (Red, Orange, Yellow, Green, Violet, Blue)
- **Custom display names** - show meaningful names instead of process names
- **Filter view** - show only tagged processes
- **Auto-reload** - mbtop picks up changes within ~2 seconds

## Installation

### Using uv (recommended)

```bash
cd mcp-server
uv sync
```

### Using pip

```bash
cd mcp-server
pip install -e .
```

## Configuration

### Claude Desktop

Add to `~/Library/Application Support/Claude/claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "mbtop": {
      "command": "uv",
      "args": ["--directory", "/path/to/mbtop/mcp-server", "run", "mbtop-mcp"]
    }
  }
}
```

### Claude Code

Add to your Claude Code MCP settings:

```json
{
  "mbtop": {
    "command": "uv",
    "args": ["--directory", "/path/to/mbtop/mcp-server", "run", "mbtop-mcp"]
  }
}
```

## Available Tools

### `tag_process`
Tag a process with a color and optional display name.

```
tag_process(
    name: str,           # Process name (e.g., "python", "node")
    color: str,          # Color: red, orange, yellow, green, violet
    display_name?: str,  # Optional custom display name
    command?: str        # Optional command line to match specific instance
)
```

### `untag_process`
Remove tagging from a process (keeps other config like log_path).

```
untag_process(name: str, command?: str)
```

### `remove_process_config`
Completely remove a process config from mbtop.

```
remove_process_config(name: str, command?: str)
```

### `set_filter_tagged`
Enable/disable the tagged process filter.

```
set_filter_tagged(enabled: bool)
```

### `list_tagged_processes`
List all currently tagged processes.

### `get_aurora_colors`
Get available color names and their hex values.

### `get_mbtop_status`
Get mbtop config status (tagged count, filter state).

## Colors (Nord Aurora)

| Name | Theme Key | Hex | Use Case |
|------|-----------|-----|----------|
| red | log_fault | #BF616A | Alerts, errors, critical |
| orange | log_error | #D08770 | Warnings, attention needed |
| yellow | log_info | #EBCB8B | Active work, in progress |
| green | log_debug_plus | #A3BE8C | Healthy, OK, success |
| violet | log_debug | #B48EAD | Info, background, neutral |

## Example Usage

When helping debug a web application:

```
1. tag_process("nginx", "green", "PROXY")
2. tag_process("node", "yellow", "API")
3. tag_process("postgres", "violet", "DATABASE")
4. set_filter_tagged(true)
```

mbtop will now show only these 3 processes with their custom names and colors.

## Requirements

- mbtop v1.7.0+ with TOML config and MCP support
- Python 3.10+
- mcp SDK
- tomlkit

## License

Same as mbtop - Apache 2.0
