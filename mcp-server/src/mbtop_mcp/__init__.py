"""
mbtop MCP Server - Control process tagging and filtering in mbtop

This MCP server allows AI assistants to:
- Tag processes with colors and custom display names
- Filter mbtop to show only tagged processes
- Control the Logs panel (show/hide, select process)
- Dynamically update what's visible in mbtop

mbtop will auto-reload the config within ~2 seconds of changes.
Socket commands provide real-time control of mbtop UI.
"""

from pathlib import Path
from typing import Optional
import json
import socket
import tomlkit
from mcp.server.fastmcp import FastMCP

# Initialize FastMCP server
mcp = FastMCP(
    "mbtop",
    instructions="""
    MCP server for mbtop process monitor. Use these tools to:
    - Tag important processes with colors (Red, Orange, Yellow, Green, Violet, Blue)
    - Give processes meaningful display names
    - Filter mbtop to show only tagged processes
    
    mbtop auto-reloads config changes within ~2 seconds.
    """,
)

# Nord Aurora + Frost color mapping
AURORA_COLORS = {
    "red": "log_fault",
    "orange": "log_error",
    "yellow": "log_info",
    "green": "log_debug_plus",
    "violet": "log_debug",
    "blue": "tag_blue",
}

# Default config path
DEFAULT_CONFIG_PATH = Path.home() / ".config" / "mbtop" / "mbtop.toml"
DEFAULT_SOCKET_PATH = Path.home() / ".config" / "mbtop" / "mbtop.sock"


def get_config_path() -> Path:
    """Get the mbtop config file path."""
    return DEFAULT_CONFIG_PATH


def load_config() -> tomlkit.TOMLDocument:
    """Load the mbtop TOML config file."""
    config_path = get_config_path()
    if not config_path.exists():
        raise FileNotFoundError(f"mbtop config not found at {config_path}")
    return tomlkit.parse(config_path.read_text())


def save_config(config: tomlkit.TOMLDocument) -> None:
    """Save the mbtop TOML config file."""
    config_path = get_config_path()
    config_path.write_text(tomlkit.dumps(config))


def find_process_config(
    config: tomlkit.TOMLDocument, name: str, command: str
) -> tuple[int, Optional[dict]]:
    """
    Find a process config by name AND command (both required).
    Returns (index, config) or (-1, None) if not found.
    """
    processes = config.get("logging", {}).get("processes", [])
    for i, proc in enumerate(processes):
        if proc.get("name") == name and proc.get("command", "") == command:
            return i, proc
    return -1, None


@mcp.tool()
def tag_process(
    name: str,
    command: str,
    color: str = "green",
    display_name: Optional[str] = None,
    log_path: Optional[str] = None,
) -> str:
    """
    Tag a process in mbtop with a color and optional display name.

    Args:
        name: Process name to tag (e.g., "python", "node", "nginx")
        command: Full command line to match (REQUIRED - e.g., "mbtop -p 3", "node server.js")
        color: Color name - one of: red, orange, yellow, green, violet
        display_name: Optional custom name to show instead of process name
        log_path: Optional path to log file for this process

    Returns:
        Success message with the tagged process details
    """
    if not command:
        return "Error: 'command' is required. Use the full command line (e.g., 'mbtop -p 3')"

    color_lower = color.lower()
    if color_lower not in AURORA_COLORS:
        return f"Error: Invalid color '{color}'. Valid colors: {', '.join(AURORA_COLORS.keys())}"

    try:
        config = load_config()
    except FileNotFoundError as e:
        return str(e)

    # Ensure logging.processes exists
    if "logging" not in config:
        config["logging"] = tomlkit.table()
    if "processes" not in config["logging"]:
        config["logging"]["processes"] = tomlkit.aot()

    # Check if process already exists
    idx, existing = find_process_config(config, name, command)

    if existing is not None:
        # Update existing config
        existing["tagged"] = True
        existing["tag_color"] = AURORA_COLORS[color_lower]
        if display_name:
            existing["display_name"] = display_name
        if command:
            existing["command"] = command
        if log_path:
            existing["log_path"] = log_path
    else:
        # Create new process config
        proc = tomlkit.table()
        if display_name:
            proc["display_name"] = display_name
        if log_path:
            proc["log_path"] = log_path
        proc["name"] = name
        proc["command"] = command  # Required
        proc["tag_color"] = AURORA_COLORS[color_lower]
        proc["tagged"] = True
        config["logging"]["processes"].append(proc)

    save_config(config)

    display = display_name or name
    msg = f"Tagged '{name}' as '{display}' with {color} color."
    if log_path:
        msg += f" Log: {log_path}"
    msg += " mbtop will reload automatically."
    return msg


@mcp.tool()
def untag_process(name: str, command: str) -> str:
    """
    Remove tagging from a process (keeps other config like log_path).

    Args:
        name: Process name to untag
        command: Full command line to match (REQUIRED)

    Returns:
        Success or error message
    """
    if not command:
        return "Error: 'command' is required"

    try:
        config = load_config()
    except FileNotFoundError as e:
        return str(e)

    idx, existing = find_process_config(config, name, command)

    if existing is None:
        return f"No config found for process '{name}' with command '{command}'"

    # Remove tagging but keep other config
    existing["tagged"] = False
    existing.pop("tag_color", None)

    save_config(config)
    return f"Removed tagging from '{name}'. mbtop will reload automatically."


@mcp.tool()
def set_process_log(name: str, command: str, log_path: str) -> str:
    """
    Set or update the log file path for a process.

    This configures which application log file mbtop should read when viewing
    logs for this process (when switching to 'application' log source with 'S' key).

    Args:
        name: Process name (e.g., "python", "node", "nginx")
        command: Full command line to match (REQUIRED - e.g., "node server.js")
        log_path: Path to the log file (e.g., "/var/log/myapp.log", "~/logs/app.log")

    Returns:
        Success or error message
    """
    if not command:
        return "Error: 'command' is required"
    if not log_path:
        return "Error: 'log_path' is required"

    try:
        config = load_config()
    except FileNotFoundError as e:
        return str(e)

    # Ensure logging.processes exists
    if "logging" not in config:
        config["logging"] = tomlkit.table()
    if "processes" not in config["logging"]:
        config["logging"]["processes"] = tomlkit.aot()

    idx, existing = find_process_config(config, name, command)

    if existing is not None:
        # Update existing config
        existing["log_path"] = log_path
    else:
        # Create new process config (minimal - just name, command, log_path)
        proc = tomlkit.table()
        proc["name"] = name
        proc["command"] = command
        proc["log_path"] = log_path
        config["logging"]["processes"].append(proc)

    save_config(config)
    return (
        f"Set log path for '{name}' to '{log_path}'. mbtop will reload automatically."
    )


@mcp.tool()
def remove_process_log(name: str, command: str) -> str:
    """
    Remove the log file configuration from a process.

    Args:
        name: Process name
        command: Full command line to match (REQUIRED)

    Returns:
        Success or error message
    """
    if not command:
        return "Error: 'command' is required"

    try:
        config = load_config()
    except FileNotFoundError as e:
        return str(e)

    idx, existing = find_process_config(config, name, command)

    if existing is None:
        return f"No config found for process '{name}' with command '{command}'"

    if "log_path" not in existing:
        return f"Process '{name}' has no log_path configured"

    del existing["log_path"]
    save_config(config)
    return f"Removed log path from '{name}'. mbtop will reload automatically."


@mcp.tool()
def remove_process_config(name: str, command: str) -> str:
    """
    Completely remove a process config from mbtop.

    Args:
        name: Process name to remove
        command: Full command line to match (REQUIRED)

    Returns:
        Success or error message
    """
    if not command:
        return "Error: 'command' is required"

    try:
        config = load_config()
    except FileNotFoundError as e:
        return str(e)

    processes = config.get("logging", {}).get("processes", [])
    idx, existing = find_process_config(config, name, command)

    if existing is None:
        return f"No config found for process '{name}' with command '{command}'"

    del config["logging"]["processes"][idx]

    save_config(config)
    return f"Removed config for '{name}'. mbtop will reload automatically."


@mcp.tool()
def set_filter_tagged(enabled: bool) -> str:
    """
    Enable or disable the tagged process filter in mbtop.
    When enabled, mbtop only shows tagged processes.
    Only affects instances where PROC panel is visible.

    Args:
        enabled: True to show only tagged processes, False to show all

    Returns:
        Success message
    """
    try:
        config = load_config()
    except FileNotFoundError as e:
        return str(e)

    # Ensure process section exists
    if "process" not in config:
        config["process"] = tomlkit.table()

    config["process"]["proc_filter_tagged"] = enabled

    save_config(config)

    status = "enabled" if enabled else "disabled"
    return f"Tagged filter {status}. Only instances showing PROC will be affected."


@mcp.tool()
def list_tagged_processes() -> str:
    """
    List all currently tagged processes in mbtop config.

    Returns:
        Formatted list of tagged processes with their colors and display names
    """
    try:
        config = load_config()
    except FileNotFoundError as e:
        return str(e)

    processes = config.get("logging", {}).get("processes", [])
    tagged = [p for p in processes if p.get("tagged", False)]

    if not tagged:
        return "No tagged processes configured."

    # Reverse lookup for color names
    color_names = {v: k for k, v in AURORA_COLORS.items()}

    lines = ["Tagged processes:"]
    for proc in tagged:
        name = proc.get("name", "?")
        display = proc.get("display_name", name)
        color = color_names.get(proc.get("tag_color", ""), "unknown")
        command = proc.get("command", "")

        line = f"  - {name}"
        if display != name:
            line += f" (display: {display})"
        line += f" [{color}]"
        if command:
            line += f" cmd: {command[:50]}..."
        lines.append(line)

    # Also show filter status
    filter_enabled = config.get("process", {}).get("proc_filter_tagged", False)
    lines.append(f"\nTagged filter: {'enabled' if filter_enabled else 'disabled'}")

    return "\n".join(lines)


@mcp.tool()
def get_aurora_colors() -> str:
    """
    Get the available Nord Aurora colors for process tagging.

    Returns:
        List of available color names
    """
    return """Available colors (Nord Aurora + Frost palette):
  - red     (nord11 - #BF616A) - Alerts, errors, critical
  - orange  (nord12 - #D08770) - Warnings, attention needed
  - yellow  (nord13 - #EBCB8B) - Active work, in progress
  - green   (nord14 - #A3BE8C) - Healthy, OK, success
  - violet  (nord15 - #B48EAD) - Info, background, neutral
  - blue    (nord9  - #81A1C1) - Frost blue, calm, secondary"""


@mcp.tool()
def get_mbtop_status() -> str:
    """
    Get the current mbtop configuration status.

    Returns:
        Summary of mbtop config including tagged filter state and process count
    """
    try:
        config = load_config()
    except FileNotFoundError as e:
        return str(e)

    processes = config.get("logging", {}).get("processes", [])
    tagged_count = len([p for p in processes if p.get("tagged", False)])
    filter_enabled = config.get("process", {}).get("proc_filter_tagged", False)

    return f"""mbtop Status:
  Config path: {get_config_path()}
  Process configs: {len(processes)}
  Tagged processes: {tagged_count}
  Tagged filter: {"enabled" if filter_enabled else "disabled"}"""


# =============================================================================
# Socket-based real-time control tools
# =============================================================================


def send_socket_command(cmd: dict, timeout: float = 2.0, retries: int = 3) -> dict:
    """
    Send a command to mbtop via Unix socket.

    Args:
        cmd: Command dictionary to send as JSON
        timeout: Socket timeout in seconds (used for recv only)
        retries: Number of retry attempts on transient failures

    Returns:
        Response dictionary from mbtop
    """
    import time

    sock_path = DEFAULT_SOCKET_PATH
    if not sock_path.exists():
        return {
            "status": "error",
            "message": f"mbtop socket not found at {sock_path}. Is mbtop running?",
        }

    last_error = None
    for attempt in range(retries):
        try:
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock.setblocking(True)
            sock.connect(str(sock_path))

            # Send command
            data = json.dumps(cmd).encode("utf-8")
            sock.send(data)

            # Shutdown write side to signal EOF (required for server's read() to return)
            sock.shutdown(socket.SHUT_WR)

            # Set timeout for receiving response
            sock.settimeout(timeout)

            # Receive response
            response = sock.recv(4096).decode("utf-8")
            sock.close()

            return json.loads(response)
        except (BrokenPipeError, OSError) as e:
            # Transient error - retry after small delay
            last_error = e
            try:
                sock.close()
            except:
                pass
            if attempt < retries - 1:
                time.sleep(0.2)  # 200ms delay before retry
            continue
        except socket.timeout:
            return {"status": "error", "message": "Timeout waiting for mbtop response"}
        except ConnectionRefusedError:
            return {
                "status": "error",
                "message": "Connection refused. Is mbtop running?",
            }
        except Exception as e:
            return {"status": "error", "message": str(e)}

    # All retries failed
    return {
        "status": "error",
        "message": f"Socket communication failed after {retries} attempts: {last_error}",
    }


@mcp.tool()
def show_logs_panel(
    name: Optional[str] = None,
    command: Optional[str] = None,
    pid: Optional[int] = None,
    position: str = "beside",
) -> str:
    """
    Show the Logs panel in mbtop for a specific process.

    Requires mbtop to be running with PROC panel visible.

    Args:
        name: Process name to show logs for (optional - uses current selection if not specified)
        command: Process command to match (optional)
        pid: Process PID to select (optional)
        position: Panel position - "beside" (right of proc) or "below" (under proc)

    Returns:
        Success or error message
    """
    cmd = {"cmd": "show_logs" if position == "beside" else "show_logs_below"}
    if name:
        cmd["name"] = name
    if command:
        cmd["command"] = command
    if pid:
        cmd["pid"] = pid

    result = send_socket_command(cmd)

    if result.get("status") == "ok":
        return f"Logs panel shown ({position}). mbtop will display logs for the selected process."
    else:
        return f"Error: {result.get('message', 'Unknown error')}"


@mcp.tool()
def hide_logs_panel() -> str:
    """
    Hide the Logs panel in mbtop.

    Returns:
        Success or error message
    """
    result = send_socket_command({"cmd": "hide_logs"})

    if result.get("status") == "ok":
        return "Logs panel hidden."
    else:
        return f"Error: {result.get('message', 'Unknown error')}"


@mcp.tool()
def select_process(
    name: Optional[str] = None, command: Optional[str] = None, pid: Optional[int] = None
) -> str:
    """
    Select a process in mbtop's process list.

    At least one of name, command, or pid must be specified.

    Args:
        name: Process name to select
        command: Process command to match
        pid: Process PID to select

    Returns:
        Success or error message
    """
    if not name and not command and not pid:
        return "Error: At least one of name, command, or pid must be specified"

    cmd = {"cmd": "select_process"}
    if name:
        cmd["name"] = name
    if command:
        cmd["command"] = command
    if pid:
        cmd["pid"] = pid

    result = send_socket_command(cmd)

    if result.get("status") == "ok":
        return "Process selected in mbtop."
    else:
        return f"Error: {result.get('message', 'Unknown error')}"


@mcp.tool()
def activate_process_log(
    name: str, command: Optional[str] = None, pid: Optional[int] = None
) -> str:
    """
    Select a process AND show its logs in one action.

    This combines select_process and show_logs_panel for convenience.

    Args:
        name: Process name to select and show logs for
        command: Process command to match (optional)
        pid: Process PID (optional)

    Returns:
        Success or error message
    """
    cmd = {"cmd": "activate_process_log", "name": name}
    if command:
        cmd["command"] = command
    if pid:
        cmd["pid"] = pid

    result = send_socket_command(cmd)

    if result.get("status") == "ok":
        return f"Process '{name}' selected and Logs panel shown."
    else:
        return f"Error: {result.get('message', 'Unknown error')}"


@mcp.tool()
def set_log_level_filter(level: str = "all") -> str:
    """
    Set the log level filter in the Logs panel.

    Args:
        level: Filter level - one of: "all", "debug", "info", "error", "fault"

    Returns:
        Success or error message
    """
    valid_levels = ["all", "debug", "info", "error", "fault"]
    if level.lower() not in valid_levels:
        return (
            f"Error: Invalid level '{level}'. Valid levels: {', '.join(valid_levels)}"
        )

    result = send_socket_command({"cmd": "set_log_filter", "filter": level.lower()})

    if result.get("status") == "ok":
        return f"Log filter set to '{level}'."
    else:
        return f"Error: {result.get('message', 'Unknown error')}"


@mcp.tool()
def set_log_source(source: str = "system") -> str:
    """
    Set the log source in the Logs panel.

    Args:
        source: Log source - one of: "system" (unified log), "application" (app log file), or "toggle"

    Returns:
        Success or error message
    """
    valid_sources = ["system", "unified", "application", "app", "toggle"]
    if source.lower() not in valid_sources:
        return f"Error: Invalid source '{source}'. Valid sources: system, application, toggle"

    result = send_socket_command({"cmd": "set_log_source", "source": source.lower()})

    if result.get("status") == "ok":
        return f"Log source set to '{source}'."
    else:
        return f"Error: {result.get('message', 'Unknown error')}"


@mcp.tool()
def get_mbtop_live_state() -> str:
    """
    Get the current live state of mbtop via socket.

    This queries mbtop directly for real-time state including:
    - Logs panel visibility and position
    - Currently selected/followed process
    - Filter status

    Returns:
        JSON-formatted state or error message
    """
    result = send_socket_command({"cmd": "get_state"}, timeout=3.0)

    if result.get("status") == "error":
        return f"Error: {result.get('message', 'Unknown error')}"

    # Format the state nicely
    log_source = result.get("logs_source", "system")
    app_log = "available" if result.get("app_log_available") else "not configured"
    return f"""mbtop Live State:
  Logs panel: {"shown" if result.get("logs_shown") else "hidden"}
  Logs position: {"below" if result.get("logs_below") else "beside"}
  Logs source: {log_source} (app log: {app_log})
  Proc panel: {"shown" if result.get("proc_shown") else "hidden"}
  Current process: {result.get("current_name", "none")} (PID: {result.get("current_pid", 0)})
  Followed PID: {result.get("followed_pid", 0)}
  Selected PID: {result.get("selected_pid", 0)}
  Tagged filter: {"enabled" if result.get("filter_tagged") else "disabled"}"""


def main():
    """Run the MCP server."""
    mcp.run()


if __name__ == "__main__":
    main()
