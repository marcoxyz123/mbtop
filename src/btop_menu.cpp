/* Copyright 2021 Aristocratos (jakob@qvantnet.com)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

indent = tab
tab-size = 4
*/

#include "btop_menu.hpp"

#include "btop_config.hpp"
#include "btop_draw.hpp"
#include "btop_log.hpp"
#include "btop_shared.hpp"
#include "btop_theme.hpp"
#include "btop_tools.hpp"

#include <errno.h>
#include <signal.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <utility>

#include <fmt/format.h>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include "osx/apple_silicon_gpu.hpp"
#endif

using std::array;
using std::ceil;
using std::max;
using std::min;
using std::ref;
using std::views::iota;

using namespace Tools;

namespace fs = std::filesystem;

namespace Menu {

   atomic<bool> active (false);
   string bg;
   bool redraw{true};
   int currentMenu = -1;
   msgBox messageBox;
   int signalToSend{};
   int signalKillRet{};

   const array<string, 32> P_Signals = {
	   "0",
#ifdef __linux__
#if defined(__hppa__)
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
		"SIGTRAP", "SIGABRT", "SIGSTKFLT", "SIGFPE",
		"SIGKILL", "SIGBUS", "SIGSEGV", "SIGXCPU",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGUSR1",
		"SIGUSR2", "SIGCHLD", "SIGPWR", "SIGVTALRM",
		"SIGPROF", "SIGIO", "SIGWINCH", "SIGSTOP",
		"SIGTSTP", "SIGCONT", "SIGTTIN", "SIGTTOU",
		"SIGURG", "SIGXFSZ", "SIGSYS"
#elif defined(__mips__)
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
		"SIGTRAP", "SIGABRT", "SIGEMT", "SIGFPE",
		"SIGKILL", "SIGBUS", "SIGSEGV", "SIGSYS",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGUSR1",
		"SIGUSR2", "SIGCHLD", "SIGPWR", "SIGWINCH",
		"SIGURG", "SIGIO", "SIGSTOP", "SIGTSTP",
		"SIGCONT", "SIGTTIN", "SIGTTOU", "SIGVTALRM",
		"SIGPROF", "SIGXCPU", "SIGXFSZ"
#elif defined(__alpha__)
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
		"SIGTRAP", "SIGABRT", "SIGEMT", "SIGFPE",
		"SIGKILL", "SIGBUS", "SIGSEGV", "SIGSYS",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGURG",
		"SIGSTOP", "SIGTSTP", "SIGCONT", "SIGCHLD",
		"SIGTTIN", "SIGTTOU", "SIGIO", "SIGXCPU",
		"SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH",
		"SIGPWR", "SIGUSR1", "SIGUSR2"
#elif defined (__sparc__)
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
		"SIGTRAP", "SIGABRT", "SIGEMT", "SIGFPE",
		"SIGKILL", "SIGBUS", "SIGSEGV", "SIGSYS",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGURG",
		"SIGSTOP", "SIGTSTP", "SIGCONT", "SIGCHLD",
		"SIGTTIN", "SIGTTOU", "SIGIO", "SIGXCPU",
		"SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH",
		"SIGLOST", "SIGUSR1", "SIGUSR2"
#else
		"SIGHUP", "SIGINT",	"SIGQUIT",	"SIGILL",
		"SIGTRAP", "SIGABRT", "SIGBUS", "SIGFPE",
		"SIGKILL", "SIGUSR1", "SIGSEGV", "SIGUSR2",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGSTKFLT",
		"SIGCHLD", "SIGCONT", "SIGSTOP", "SIGTSTP",
		"SIGTTIN", "SIGTTOU", "SIGURG", "SIGXCPU",
		"SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH",
		"SIGIO", "SIGPWR", "SIGSYS"
#endif
#elif defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__APPLE__)
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
		"SIGTRAP", "SIGABRT", "SIGEMT", "SIGFPE",
		"SIGKILL", "SIGBUS", "SIGSEGV", "SIGSYS",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGURG",
		"SIGSTOP", "SIGTSTP", "SIGCONT", "SIGCHLD",
		"SIGTTIN", "SIGTTOU", "SIGIO", "SIGXCPU",
		"SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH",
		"SIGINFO", "SIGUSR1", "SIGUSR2"
#else
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL",
		"SIGTRAP", "SIGABRT", "7", "SIGFPE",
		"SIGKILL", "10", "SIGSEGV", "12",
		"SIGPIPE", "SIGALRM", "SIGTERM", "16",
		"17", "18", "19", "20",
		"21", "22", "23", "24",
		"25", "26", "27", "28",
		"29", "30", "31"
#endif
	};

  std::unordered_map<string, Input::Mouse_loc> mouse_mappings;

   const array<array<string, 3>, 3> menu_normal = {
		array<string, 3>{
			"┌─┐┌─┐┌┬┐┌┬┐┬┌┐┌┌─┐┌─┐",
			"└─┐├┤  │  │ │││││ ┬└─┐",
			"└─┘└─┘ ┴  ┴ ┴┘└┘└─┘└─┘"
		},
		{
			"┬ ┬┌─┐┬  ┌─┐",
			"├─┤├┤ │  ├─┘",
			"┴ ┴└─┘┴─┘┴  "
		},
		{
			"┌─┐ ┬ ┬ ┬┌┬┐",
			"│─┼┐│ │ │ │ ",
			"└─┘└└─┘ ┴ ┴ "
		}
	};

	const array<array<string, 3>, 3> menu_selected = {
		array<string, 3>{
			"╔═╗╔═╗╔╦╗╔╦╗╦╔╗╔╔═╗╔═╗",
			"╚═╗╠╣  ║  ║ ║║║║║ ╦╚═╗",
			"╚═╝╚═╝ ╩  ╩ ╩╝╚╝╚═╝╚═╝"
		},
		{
			"╦ ╦╔═╗╦  ╔═╗",
			"╠═╣╠╣ ║  ╠═╝",
			"╩ ╩╚═╝╩═╝╩  "
		},
		{
			"╔═╗ ╦ ╦ ╦╔╦╗ ",
			"║═╬╗║ ║ ║ ║  ",
			"╚═╝╚╚═╝ ╩ ╩  "
		}
	};

	const array<int, 3> menu_width = {22, 12, 12};

	const vector<array<string, 2>> help_text = {
		{"Mouse 1", "Clicks buttons and selects in process list."},
		{"Mouse scroll", "Scrolls any scrollable list/text under cursor."},
		{"Esc, m", "Toggles main menu."},
		{"p", "Cycle view presets forwards."},
		{"shift + p", "Cycle view presets backwards."},
		{"1", "Toggle CPU box."},
		{"2", "Toggle MEM box."},
		{"3", "Toggle NET box."},
		{"4", "Toggle PROC box."},
		{"5", "Toggle GPU box."},
		{"6", "Toggle ANE split view (Apple Silicon)."},
		{"7", "Toggle Power panel (Apple Silicon)."},
		{"A", "GPU memory allocation menu (Apple Silicon)."},
		{"d", "Toggle disks view in MEM box."},
		{"F2, o", "Shows options."},
		{"F1, ?, h", "Shows this window."},
		{"ctrl + z", "Sleep program and put in background."},
		{"ctrl + r", "Reloads config file from disk."},
		{"q, ctrl + c", "Quits program."},
		{"+, -", "Add/Subtract 100ms to/from update timer."},
		{"Up, Down", "Select in process list."},
		{"Enter", "Show detailed information for selected process."},
		{"Spacebar", "Expand/collapse the selected process in tree view."},
		{"C", "Expand/collapse the selected process' children."},
		{"Pg Up, Pg Down", "Jump 1 page in process list."},
		{"Home, End", "Jump to first or last page in process list."},
		{"Left, Right", "Select previous/next sorting column."},
		{"b, n", "Select previous/next network device."},
		{"i", "Toggle disks io mode with big graphs."},
		{"Tab", "Cycle memory/disk selection mode."},
		{"M, B", "Toggle meter/braille display mode in MEM box."},
		{"T", "Toggle memory item visibility mode."},
		{"S", "Toggle swap item visibility mode."},
		{"V", "Toggle VRAM item visibility mode (Apple Silicon)."},
		{"z", "Toggle totals reset for current network device"},
		{"a", "Toggle auto scaling for the network graphs."},
		{"y", "Toggle synced scaling mode for network graphs."},
		{"f, /", "To enter a process filter. Start with ! for regex."},
		{"F", "Follow selected process."},
		{"u", "Pause process list."},
		{"delete", "Clear any entered filter."},
		{"c", "Toggle per-core cpu usage of processes."},
		{"r", "Reverse sorting order in processes box."},
		{"e", "Toggle processes tree view."},
		{"%", "Toggles memory display mode in processes box."},
		{"Selected +, -", "Expand/collapse the selected process in tree view."},
		{"Selected t", "Terminate selected process with SIGTERM - 15."},
		{"Selected k", "Kill selected process with SIGKILL - 9."},
		{"Selected s", "Select or enter signal to send to process."},
		{"Selected N", "Select new nice value for selected process."},
		{"", " "},
		{"", "VIM KEYS (options): h,j,k,l,g,G for navigation."},
		{"", "Help 'h' becomes 'H', kill 'k' becomes 'K'."},
		{"", " "},
		{"", "For bug reporting and project updates, visit:"},
		{"", "https://github.com/aristocratos/btop"},
	};

	const vector<vector<vector<string>>> categories = {
		{
			{"color_theme",
				"Set color theme.",
				"",
				"Choose from all theme files in (usually)",
				"\"/usr/[local/]share/btop/themes\" and",
				"\"~/.config/btop/themes\".",
				"",
				"\"Default\" for builtin default theme.",
				"\"TTY\" for builtin 16-color theme.",
				"",
				"For theme updates see:",
				"https://github.com/aristocratos/btop"},
			{"theme_background",
				"If the theme set background should be shown.",
				"",
				"Set to False if you want terminal background",
				"transparency."},
			{"truecolor",
				"Sets if 24-bit truecolor should be used.",
				"",
				"Will convert 24-bit colors to 256 color",
				"(6x6x6 color cube) if False.",
				"",
				"Set to False if your terminal doesn't have",
				"truecolor support and can't convert to",
				"256-color."},
			{"force_tty",
				"TTY mode.",
				"",
				"Set to true to force tty mode regardless",
				"if a real tty has been detected or not.",
				"",
				"Will force 16-color mode and TTY theme,",
				"set all graph symbols to \"tty\" and swap",
				"out other non tty friendly symbols."},
			{"vim_keys",
				"Enable vim keys.",
				"Set to True to enable \"h,j,k,l\" keys for",
				"directional control in lists.",
				"",
				"Conflicting keys for",
				"h (help) and k (kill)",
				"is accessible while holding shift."},
			{"disable_mouse",
				"Disable all mouse events."},
			{"presets",
				"Define presets for the layout of the boxes.",
				"",
				"Preset 0 is always all boxes shown with",
				"default settings.",
				"Max 9 presets.",
				"",
				"Format: \"box_name:P:G,box_name:P:G\"",
				"P=(0 or 1) for alternate positions.",
				"G=graph symbol to use for box.",
				"",
				"Use whitespace \" \" as separator between",
				"different presets.",
				"",
				"Example:",
				"\"mem:0:tty,proc:1:default cpu:0:braille\""},
			{"shown_boxes",
				"Manually set which boxes to show.",
				"",
				"Available values are \"cpu pwr mem net proc\".",
			#ifdef GPU_SUPPORT
				"Or \"gpu0\" through \"gpu5\" for GPU boxes.",
			#endif
				"Separate values with whitespace.",
				"",
				"Toggle between presets with key \"p\"."},
			{"update_ms",
				"Update time in milliseconds.",
				"",
				"Recommended 2000 ms or above for better",
				"sample times for graphs.",
				"",
				"Min value: 100 ms",
				"Max value: 86400000 ms = 24 hours."},
			{"rounded_corners",
				"Rounded corners on boxes.",
				"",
				"True or False",
				"",
				"Is always False if TTY mode is ON."},
			{"terminal_sync",
				"Output synchronization.",
				"",
				"Use terminal synchronized output sequences",
				"to reduce flickering on supported terminals.",
				"",
				"True or False."},
			{"graph_symbol",
				"Default symbols to use for graph creation.",
				"",
				"\"braille\", \"block\" or \"tty\".",
				"",
				"\"braille\" offers the highest resolution but",
				"might not be included in all fonts.",
				"",
				"\"block\" has half the resolution of braille",
				"but uses more common characters.",
				"",
				"\"tty\" uses only 3 different symbols but will",
				"work with most fonts.",
				"",
				"Note that \"tty\" only has half the horizontal",
				"resolution of the other two,",
				"so will show a shorter historical view."},
			{"clock_format",
				"Clock format using strftime syntax.",
				"(Displayed left of update interval)",
				"",
				"Empty string to disable clock.",
				"",
				"Examples of strftime formats:",
				"\"%X\" = locale HH:MM:SS",
				"\"%H:%M:%S\" = 24h format",
				"\"%I:%M:%S %p\" = 12h with AM/PM",
				"\"%d\" = day, \"%m\" = month, \"%y\" = year"},
			{"clock_12h",
				"Use 12-hour clock format.",
				"",
				"Shows time as HH:MM:SS AM/PM",
				"instead of 24-hour format.",
				"",
				"Overrides clock_format when enabled.",
				"",
				"True or False."},
			{"show_hostname",
				"Show hostname in CPU box header.",
				"(Displayed in center position)",
				"",
				"Strips \".local\" suffix but keeps",
				"full FQDN (e.g., host.domain.com).",
				"",
				"True or False."},
			{"show_uptime_header",
				"Show system uptime in header.",
				"(Displayed after preset button)",
				"",
				"Shows uptime in d:h:m:s format.",
				"",
				"True or False."},
			{"show_username_header",
				"Show current username in header.",
				"(Displayed after uptime)",
				"",
				"True or False."},
			{"preset_names",
				"Custom names for presets 1-9.",
				"",
				"Names separated by whitespace.",
				"Displayed after preset number.",
				"",
				"Example:",
				"\"Standard LLM Processes Power\"",
				"",
				"Leave empty to use default numbers."},
			{"base_10_sizes",
				"Use base 10 for bits and bytes sizes.",
				"",
				"Uses KB = 1000 instead of KiB = 1024,",
				"MB = 1000KB instead of MiB = 1024KiB,",
				"and so on.",
				"",
				"True or False."},
			{"background_update",
				"Update main ui when menus are showing.",
				"",
				"True or False.",
				"",
				"Set this to false if the menus is flickering",
				"too much for a comfortable experience."},
			{"show_battery",
				"Show battery stats.",
				"(Only visible if cpu box is enabled!)",
				"",
				"Show battery stats in the top right corner",
				"if a battery is present."},
			{"selected_battery",
				"Select battery.",
				"",
				"Which battery to use if multiple are present.",
				"Can be both batteries and UPS.",
				"",
				"\"Auto\" for auto detection."},
			{"show_battery_watts",
				"Show battery power.",
				"",
				"Show discharge power when discharging.",
				"Show charging power when charging."},
			{"log_level",
				"Set loglevel for error.log",
				"",
				"\"ERROR\", \"WARNING\", \"INFO\" and \"DEBUG\".",
				"",
				"The level set includes all lower levels,",
				"i.e. \"DEBUG\" will show all logging info."},
			{"save_config_on_exit",
				"Save config on exit.",
				"",
				"Automatically save current settings to",
				"config file on exit.",
				"",
				"When this is toggled from True to False",
				"a save is immediately triggered.",
				"This way a manual save can be done by",
				"toggling this setting on and off again."}
		},
		{
			{"cpu_bottom",
				"Cpu box location.",
				"",
				"Show cpu box at bottom of screen instead",
				"of top."},
			{"graph_symbol_cpu",
				"Graph symbol to use for graphs in cpu box.",
				"",
				"\"default\", \"braille\", \"block\" or \"tty\".",
				"",
				"\"default\" for the general default symbol.",},
			{"cpu_graph_upper",
				"Cpu upper graph.",
				"",
				"Sets the CPU/GPU stat shown in upper half of",
				"the CPU graph.",
				"",
				"CPU:",
				"\"total\" = Total cpu usage. (Auto)",
				"\"user\" = User mode cpu usage.",
				"\"system\" = Kernel mode cpu usage.",
				"+ more depending on kernel.",
		#ifdef GPU_SUPPORT
				"",
				"GPU:",
				"\"gpu-totals\" = GPU usage split by device.",
				"\"gpu-vram-totals\" = VRAM usage split by GPU.",
				"\"gpu-pwr-totals\" = Power usage split by GPU.",
				"\"gpu-average\" = Avg usage of all GPUs.",
				"\"gpu-vram-total\" = VRAM usage of all GPUs.",
				"\"gpu-pwr-total\" = Power usage of all GPUs.",
				"Not all stats are supported on all devices."
		#endif
				},
			{"cpu_graph_lower",
				"Cpu lower graph.",
				"",
				"Sets the CPU/GPU stat shown in lower half of",
				"the CPU graph.",
				"",
				"CPU:",
				"\"total\" = Total cpu usage.",
				"\"user\" = User mode cpu usage.",
				"\"system\" = Kernel mode cpu usage.",
				"+ more depending on kernel.",
		#ifdef GPU_SUPPORT
				"",
				"GPU:",
				"\"gpu-totals\" = GPU usage split/device. (Auto)",
				"\"gpu-vram-totals\" = VRAM usage split by GPU.",
				"\"gpu-pwr-totals\" = Power usage split by GPU.",
				"\"gpu-average\" = Avg usage of all GPUs.",
				"\"gpu-vram-total\" = VRAM usage of all GPUs.",
				"\"gpu-pwr-total\" = Power usage of all GPUs.",
				"Not all stats are supported on all devices."
		#endif
				},
			{"cpu_invert_lower",
					"Toggles orientation of the lower CPU graph.",
					"",
					"True or False."},
			{"cpu_single_graph",
					"Completely disable the lower CPU graph.",
					"",
					"Shows only upper CPU graph and resizes it",
					"to fit to box height.",
					"",
					"True or False."},
		#ifdef GPU_SUPPORT
			{"show_gpu_info",
					"Show gpu info in cpu box.",
					"",
					"Toggles gpu stats in cpu box and the",
					"gpu graph (if \"cpu_graph_lower\" is set to",
					"\"Auto\").",
					"",
					"\"Auto\" to show when no gpu box is shown.",
					"\"On\" to always show.",
					"\"Off\" to never show."},
		#endif
			{"check_temp",
				"Enable cpu temperature reporting.",
				"",
				"True or False."},
			{"cpu_sensor",
				"Cpu temperature sensor.",
				"",
				"Select the sensor that corresponds to",
				"your cpu temperature.",
				"",
				"Set to \"Auto\" for auto detection."},
			{"show_coretemp",
				"Show temperatures for cpu cores.",
				"",
				"Only works if check_temp is True and",
				"the system is reporting core temps."},
			{"cpu_core_map",
				"Custom mapping between core and coretemp.",
				"",
				"Can be needed on certain cpus to get correct",
				"temperature for correct core.",
				"",
				"Use lm-sensors or similar to see which cores",
				"are reporting temperatures on your machine.",
				"",
				"Format: \"X:Y\"",
				"X=core with wrong temp.",
				"Y=core with correct temp.",
				"Use space as separator between multiple",
				"entries.",
				"",
				"Example: \"4:0 5:1 6:3\""},
			{"temp_scale",
				"Which temperature scale to use.",
				"",
				"Celsius, default scale.",
				"",
				"Fahrenheit, the american one.",
				"",
				"Kelvin, 0 = absolute zero, 1 degree change",
				"equals 1 degree change in Celsius.",
				"",
				"Rankine, 0 = absolute zero, 1 degree change",
				"equals 1 degree change in Fahrenheit."},
			{"show_cpu_freq",
				"Show CPU frequency.",
				"",
				"Can cause slowdowns on systems with many",
				"cores and certain kernel versions."},
		#ifdef __linux__
			{"freq_mode",
				"How the CPU frequency will be displayed.",
				"",
				"First, get the frequency from the first",
				"core.",
				"",
				"Range, show the lowest and the highest",
				"frequency.",
				"",
				"Lowest, the lowest frequency.",
				"",
				"Highest, the highest frequency.",
				"",
				"Average, sum and divide."},
		#endif
			{"custom_cpu_name",
				"Custom cpu model name in cpu percentage box.",
				"",
				"Empty string to disable."},
			{"show_uptime",
				"Shows the system uptime in the CPU box.",
				"",
				"Can also be shown in the clock by using",
				"\"/uptime\" in the formatting.",
				"",
				"True or False."},
			{"show_cpu_watts",
				"Shows the CPU power consumption in watts.",
				"",
				"Requires running `make setcap` or",
				"`make setuid` or running with sudo.",
				"",
				"True or False."},
		},
	#ifdef GPU_SUPPORT
		{
			{"nvml_measure_pcie_speeds",
				"Measure PCIe throughput on NVIDIA cards.",
				"",
				"May impact performance on certain cards.",
				"",
				"True or False."},
			{"rsmi_measure_pcie_speeds",
				"Measure PCIe throughput on AMD cards.",
				"",
				"May impact performance on certain cards.",
				"",
				"True or False."},
			{"graph_symbol_gpu",
				"Graph symbol to use for graphs in gpu box.",
				"",
				"\"default\", \"braille\", \"block\" or \"tty\".",
				"",
				"\"default\" for the general default symbol.",},
			{"gpu_mirror_graph",
				"Horizontally mirror the GPU graph.",
				"",
				"True or False."},
			{"shown_gpus",
				"Manually set which gpu vendors to show.",
				"",
				"Available values are",
				"\"nvidia\", \"amd\", and \"intel\".",
				"Separate values with whitespace.",
				"",
				"A restart is required to apply changes."},
			{"custom_gpu_name0",
				"Custom gpu0 model name in gpu stats box.",
				"",
				"Empty string to disable."},
			{"custom_gpu_name1",
				"Custom gpu1 model name in gpu stats box.",
				"",
				"Empty string to disable."},
			{"custom_gpu_name2",
				"Custom gpu2 model name in gpu stats box.",
				"",
				"Empty string to disable."},
			{"custom_gpu_name3",
				"Custom gpu3 model name in gpu stats box.",
				"",
				"Empty string to disable."},
			{"custom_gpu_name4",
				"Custom gpu4 model name in gpu stats box.",
				"",
				"Empty string to disable."},
			{"custom_gpu_name5",
				"Custom gpu5 model name in gpu stats box.",
				"",
				"Empty string to disable."},
		},
	#endif
		{
			{"mem_below_net",
				"Mem box location.",
				"",
				"Show mem box below net box instead of above."},
			{"net_beside_mem",
				"Net box beside mem.",
				"",
				"Show net box beside mem box instead of",
				"above/below. Pressing '3' cycles:",
				"hidden -> stacked -> beside.",
				"",
				"True or False."},
			{"graph_symbol_mem",
				"Graph symbol to use for graphs in mem box.",
				"",
				"\"default\", \"braille\", \"block\" or \"tty\".",
				"",
				"\"default\" for the general default symbol.",},
			{"mem_graphs",
				"Show graphs for memory values.",
				"",
				"True or False."},
			{"show_disks",
				"Split memory box to also show disks.",
				"",
				"True or False."},
			{"show_io_stat",
				"Toggle IO activity graphs.",
				"",
				"Show small IO graphs that for disk activity",
				"(disk busy time) when not in IO mode.",
				"",
				"True or False."},
			{"io_mode",
				"Toggles io mode for disks.",
				"",
				"Shows big graphs for disk read/write speeds",
				"instead of used/free percentage meters.",
				"",
				"True or False."},
			{"io_graph_combined",
				"Toggle combined read and write graphs.",
				"",
				"Only has effect if \"io mode\" is True.",
				"",
				"True or False."},
			{"io_graph_speeds",
				"Set top speeds for the io graphs.",
				"",
				"Manually set which speed in MiB/s that",
				"equals 100 percent in the io graphs.",
				"(100 MiB/s by default).",
				"",
				"Format: \"device:speed\" separate disks with",
				"whitespace \" \".",
				"",
				"Example: \"/dev/sda:100, /dev/sdb:20\"."},
			{"show_swap",
				"If swap memory should be shown in memory box.",
				"",
				"True or False."},
			{"swap_disk",
				"Show swap as a disk.",
				"",
				"Ignores show_swap value above.",
				"Inserts itself after first disk."},
			{"only_physical",
				"Filter out non physical disks.",
				"",
				"Set this to False to include network disks,",
				"RAM disks and similar.",
				"",
				"True or False."},
			{"show_network_drives",
				"(macOS) Show network drives in disks list.",
				"",
				"Includes NFS, SMB, and AFP network shares.",
				"The protocol type will be shown",
				"in parentheses after the disk name.",
				"",
				"True or False."},
			{"use_fstab",
				"(Linux) Read disks list from /etc/fstab.",
				"",
				"This also disables only_physical.",
				"",
				"True or False."},
			{"zfs_hide_datasets",
				"(Linux) Hide ZFS datasets in disks list.",
				"",
				"Setting this to True will hide all datasets,",
				"and only show ZFS pools.",
				"",
				"(IO stats will be calculated per-pool)",
				"",
				"True or False."},
			{"disk_free_priv",
				"(Linux) Type of available disk space.",
				"",
				"Set to true to show how much disk space is",
				"available for privileged users.",
				"",
				"Set to false to show available for normal",
				"users."},
			{"disks_filter",
				"Optional filter for shown disks.",
				"",
				"Should be full path of a mountpoint.",
				"Separate multiple values with",
				"whitespace \" \".",
				"",
				"Only disks matching the filter will be shown.",
				"Prepend \033[3mexclude=\033[23m to only show disks ",
				"not matching the filter.",
				"",
				"Examples:",
				"/boot /home/user",
				"exclude=/boot /home/user"},
			{"zfs_arc_cached",
				"(Linux) Count ZFS ARC as cached memory.",
				"",
				"Add ZFS ARC used to cached memory and",
				"ZFS ARC available to available memory.",
				"These are otherwise reported by the Linux",
				"kernel as used memory.",
				"",
				"True or False."},
			{"mem_show_used",
				"Show 'Used' memory item.",
				"",
				"Toggle visibility of the Used memory",
				"bar in the Total section.",
				"Can also toggle with Shift+T in mem box."},
			{"mem_show_available",
				"Show 'Available' memory item.",
				"",
				"Toggle visibility of the Available",
				"memory bar in the Total section.",
				"Can also toggle with Shift+T in mem box."},
			{"mem_show_cached",
				"Show 'Cached' memory item.",
				"",
				"Toggle visibility of the Cached memory",
				"bar in the Total section.",
				"Can also toggle with Shift+T in mem box."},
			{"mem_show_free",
				"Show 'Free' memory item.",
				"",
				"Toggle visibility of the Free memory",
				"bar in the Total section.",
				"Can also toggle with Shift+T in mem box."},
			{"swap_show_used",
				"Show 'Swap Used' item.",
				"",
				"Toggle visibility of the Swap Used",
				"bar in the Swap section.",
				"Can also toggle with Shift+S in mem box."},
			{"swap_show_free",
				"Show 'Swap Free' item.",
				"",
				"Toggle visibility of the Swap Free",
				"bar in the Swap section.",
				"Can also toggle with Shift+S in mem box."},
			{"vram_show_used",
				"Show 'VRAM Used' item.",
				"",
				"Toggle visibility of the VRAM Used",
				"bar in the VRAM section.",
				"Can also toggle with Shift+V in mem box."},
			{"vram_show_free",
				"Show 'VRAM Free' item.",
				"",
				"Toggle visibility of the VRAM Free",
				"bar in the VRAM section.",
				"Can also toggle with Shift+V in mem box."},
		},
		{
			{"graph_symbol_net",
				"Graph symbol to use for graphs in net box.",
				"",
				"\"default\", \"braille\", \"block\" or \"tty\".",
				"",
				"\"default\" for the general default symbol.",},
			{"swap_upload_download",
				"Swap the positions of the upload and download",
				"graphs.",
				"",
				"This allows for a more \"intuitive\" view",
				"with download being down, on the bottom."},
			{"net_download",
				"Fixed network graph download value.",
				"",
				"Value in Mebibits, default \"100\".",
				"",
				"Can be toggled with auto button."},
			{"net_upload",
				"Fixed network graph upload value.",
				"",
				"Value in Mebibits, default \"100\".",
				"",
				"Can be toggled with auto button."},
			{"net_auto",
				"Start in network graphs auto rescaling mode.",
				"",
				"Ignores any values set above at start and",
				"rescales down to 10Kibibytes at the lowest.",
				"",
				"True or False."},
			{"net_sync",
				"Network scale sync.",
				"",
				"Syncs the scaling for download and upload to",
				"whichever currently has the highest scale.",
				"",
				"True or False."},
			{"net_iface",
				"Network Interface.",
				"",
				"Manually set the starting Network Interface.",
				"",
				"Will otherwise automatically choose the NIC",
				"with the highest total download since boot."},
		    {"base_10_bitrate",
			    "Base 10 bitrate",
			    "",
			    "True:  Use SI prefixes for bitrates",
			    "       (1000Kbps = 1Mbps)",
			    "False: Use binary prefixes for bitrates",
			    "       (1024Kibps = 1Mibps)",
			    "Auto:  Use the General -> Base 10 Sizes",
			    "       setting for bitrates",
			    "",
			    "True, False, or Auto",},
		},
		{
			{"proc_left",
				"Proc box location.",
				"",
				"Show proc box on left side of screen",
				"instead of right."},
			{"proc_full_width",
				"Proc full width (side-by-side mode).",
				"",
				"When net_beside_mem is active, show proc",
				"panel full width under both mem+net.",
				"Pressing '4' cycles: hidden -> under",
				"net -> full width.",
				"",
				"True or False."},
			{"graph_symbol_proc",
				"Graph symbol to use for graphs in proc box.",
				"",
				"\"default\", \"braille\", \"block\" or \"tty\".",
				"",
				"\"default\" for the general default symbol.",},
			{"proc_sorting",
				"Processes sorting option.",
				"",
				"Possible values:",
				"\"pid\", \"program\", \"arguments\", \"threads\",",
				"\"user\", \"memory\", \"cpu lazy\" and",
				"\"cpu direct\".",
				"",
				"\"cpu lazy\" updates top process over time.",
				"\"cpu direct\" updates top process",
				"directly."},
			{"proc_reversed",
				"Reverse processes sorting order.",
				"",
				"True or False."},
			{"proc_tree",
				"Processes tree view.",
				"",
				"Set true to show processes grouped by",
				"parents with lines drawn between parent",
				"and child process."},
			{"proc_aggregate",
				"Aggregate child's resources in parent.",
				"",
				"In tree-view, include all child resources",
				"with the parent even while expanded."},
			{"proc_colors",
				"Enable colors in process view.",
				"",
				"True or False."},
			{"proc_gradient",
				"Enable process view gradient fade.",
				"",
				"Fades from top or current selection.",
				"Max fade value is equal to current themes",
				"\"inactive_fg\" color value."},
			{"proc_per_core",
				"Process usage per core.",
				"",
				"If process cpu usage should be of the core",
				"it's running on or usage of the total",
				"available cpu power.",
				"",
				"If true and process is multithreaded",
				"cpu usage can reach over 100%."},
			{"proc_mem_bytes",
				"Show memory as bytes in process list.",
				" ",
				"Will show percentage of total memory",
				"if False."},
			{"keep_dead_proc_usage",
				"Cpu and Mem usage for dead processes",
				"",
				"Set true if process should preserve the cpu",
				"and memory usage of when it died while",
				"paused."},
			{"proc_cpu_graphs",
				"Show cpu graph for each process.",
				"",
				"True or False"},
			{"proc_gpu_graphs",
				"Show gpu graph for each process.",
				"",
				"Only visible when GPU column is enabled.",
				"",
				"True or False"},
			{"proc_filter_kernel",
				"(Linux) Filter kernel processes from output.",
				"",
				"Set to 'True' to filter out internal",
				"processes started by the Linux kernel."},
			{"proc_follow_detailed",
				"Follow selected process with detailed view",
				"",
				"If set to 'True' then when opening the",
				"detailed view, the process will be",
				"followed in the list. Pressing enter",
				"again will close the detailed view",
				"and stop following the process."},
		}
	};

	msgBox::msgBox() {}
	msgBox::msgBox(int width, int boxtype, const vector<string>& content, const std::string_view title)
	: width(width), boxtype(boxtype) {
		auto tty_mode = Config::getB("tty_mode");
		auto rounded = Config::getB("rounded_corners");
		const auto& right_up = (tty_mode or not rounded ? Symbols::right_up : Symbols::round_right_up);
		const auto& left_up = (tty_mode or not rounded ? Symbols::left_up : Symbols::round_left_up);
		const auto& right_down = (tty_mode or not rounded ? Symbols::right_down : Symbols::round_right_down);
		const auto& left_down = (tty_mode or not rounded ? Symbols::left_down : Symbols::round_left_down);
		height = content.size() + 7;
		x = Term::width / 2 - width / 2;
		y = Term::height/2 - height/2;
		if (boxtype == 2) selected = 1;


		button_left = left_up + Symbols::h_line * 6 + Mv::l(7) + Mv::d(2) + left_down + Symbols::h_line * 6 + Mv::l(7) + Mv::u(1) + Symbols::v_line;
		button_right = Symbols::v_line + Mv::l(7) + Mv::u(1) + Symbols::h_line * 6 + right_up + Mv::l(7) + Mv::d(2) + Symbols::h_line * 6 + right_down + Mv::u(2);

		box_contents = Draw::createBox(x, y, width, height, Theme::c("hi_fg"), true, title) + Mv::d(1);
		for (const auto& line : content) {
			box_contents += Mv::save + Mv::r(max((size_t)0, (width / 2) - (Fx::uncolor(line).size() / 2) - 1)) + line + Mv::restore + Mv::d(1);
		}
	}

	string msgBox::operator()() {
		string out;
		int pos = width / 2 - (boxtype == 0 ? 6 : 14);
		const auto first_color = (selected == 0 ? Theme::c("hi_fg") : Theme::c("div_line"));
		out = Mv::d(1) + Mv::r(pos) + Fx::b + first_color + button_left + (selected == 0 ? Theme::c("title") : Theme::c("main_fg") + Fx::ub)
			+ (boxtype == 0 ? "    Ok    " : "    Yes    ") + first_color + button_right;
		mouse_mappings["button1"] = Input::Mouse_loc{y + height - 4, x + pos + 1, 3, 12 + (boxtype > 0 ? 1 : 0)};
		if (boxtype > 0) {
			const auto second_color = (selected == 1 ? Theme::c("hi_fg") : Theme::c("div_line"));
			out += Mv::r(2) + second_color + button_left + (selected == 1 ? Theme::c("title") : Theme::c("main_fg") + Fx::ub)
				+ "    No    " + second_color + button_right;
			mouse_mappings["button2"] = Input::Mouse_loc{y + height - 4, x + pos + 15 + (boxtype > 0 ? 1 : 0), 3, 12};
		}
		return box_contents + out + Fx::reset;
	}

	//? Process input
	int msgBox::input(const string& key) {
		if (key.empty()) return Invalid;

		if (is_in(key, "escape", "backspace", "q") or key == "button2") {
			return No_Esc;
		}
		else if (key == "button1" or (boxtype == 0 and str_to_upper(key) == "O")) {
			return Ok_Yes;
		}
		else if (is_in(key, "enter", "space")) {
			return selected + 1;
		}
		else if (boxtype == 0) {
			return Invalid;
		}
		else if (str_to_upper(key) == "Y") {
			return Ok_Yes;
		}
		else if (str_to_upper(key) == "N") {
			return No_Esc;
		}
		else if (is_in(key, "right", "tab")) {
			if (++selected > 1) selected = 0;
			return Select;
		}
		else if (is_in(key, "left", "shift_tab")) {
			if (--selected < 0) selected = 1;
			return Select;
		}

		return Invalid;
	}

	void msgBox::clear() {
		box_contents.clear();
		box_contents.shrink_to_fit();
		button_left.clear();
		button_left.shrink_to_fit();
		button_right.clear();
		button_right.shrink_to_fit();
		mouse_mappings.erase("button1");
		mouse_mappings.erase("button2");
	}

	enum menuReturnCodes {
		NoChange,
		Changed,
		Closed,
		Switch
	};

	static int signalChoose(const string& key) {
		auto s_pid = (Config::getB("show_detailed") and Config::getI("selected_pid") == 0 ? Config::getI("detailed_pid") : Config::getI("selected_pid"));
		static int x{};
		static int y{};
		static int selected_signal = -1;

		if (bg.empty()) selected_signal = -1;
		auto& out = Global::overlay;
		int retval = Changed;

		if (redraw) {
			x = Term::width/2 - 40;
			y = Term::height/2 - 9;
			bg = Draw::createBox(x + 2, y, 78, 19, Theme::c("hi_fg"), true, "signals");
			bg += Mv::to(y+2, x+3) + Theme::c("title") + Fx::b + cjust("Send signal to PID " + to_string(s_pid) + " ("
				+ uresize((s_pid == Config::getI("detailed_pid") ? Proc::detailed.entry.name : Config::getS("selected_name")), 30) + ")", 76);
		}
		else if (is_in(key, "escape", "q")) {
			return Closed;
		}
		else if (key.starts_with("button_")) {
			int new_select = (key.length() > 7) ? stoi_safe(key.substr(7), -1) : -1;
			if (new_select >= 0 and new_select == selected_signal)
				goto ChooseEntering;
			else if (new_select >= 0)
				selected_signal = new_select;
		}
		else if (is_in(key, "enter", "space") and selected_signal >= 0) {
			ChooseEntering:
			signalKillRet = 0;
			if (s_pid < 1) {
				signalKillRet = ESRCH;
				menuMask.set(SignalReturn);
			}
			else if (kill(s_pid, selected_signal) != 0) {
				signalKillRet = errno;
				menuMask.set(SignalReturn);
			}
			return Closed;
		}
		else if (key.size() == 1 and isdigit(key.at(0)) and selected_signal < 10) {
			string combined = (selected_signal < 1 ? key : to_string(selected_signal) + key);
			selected_signal = std::min(stoi_safe(combined, selected_signal), 64);
		}
		else if (key == "backspace" and selected_signal != -1) {
			selected_signal = (selected_signal < 10 ? -1 : selected_signal / 10);
		}
		else if (is_in(key, "up", "k") and selected_signal != 16) {
			if (selected_signal == 1) selected_signal = 31;
			else if (selected_signal < 6) selected_signal += 25;
			else {
				bool offset = (selected_signal > 16);
				selected_signal -= 5;
				if (selected_signal <= 16 and offset) selected_signal--;
			}
		}
		else if (is_in(key, "down", "j")) {
			if (selected_signal == 31) selected_signal = 1;
			else if (selected_signal < 1 or selected_signal == 16) selected_signal = 1;
			else if (selected_signal > 26) selected_signal -= 25;
			else {
				bool offset = (selected_signal < 16);
				selected_signal += 5;
				if (selected_signal >= 16 and offset) selected_signal++;
				if (selected_signal > 31) selected_signal = 31;
			}
		}
		else if (is_in(key, "left", "h") and selected_signal > 0 and selected_signal != 16) {
			if (--selected_signal < 1) selected_signal = 31;
			else if (selected_signal == 16) selected_signal--;
		}
		else if (is_in(key, "right", "l") and selected_signal <= 31 and selected_signal != 16) {
			if (++selected_signal > 31) selected_signal = 1;
			else if (selected_signal == 16) selected_signal++;
		}
		else {
			retval = NoChange;
		}

		if (retval == Changed) {
			int cy = y+4, cx = x+4;
			out = bg + Mv::to(cy++, x+3) + Theme::c("main_fg") + Fx::ub
				+ rjust("Enter signal number: ", 48) + Theme::c("hi_fg") + (selected_signal >= 0 ? to_string(selected_signal) : "") + Theme::c("main_fg") + Fx::bl + "█" + Fx::ubl;

			auto sig_str = to_string(selected_signal);
			for (int count = 0, i = 0; const auto& sig : P_Signals) {
				if (count == 0 or count == 16) { count++; continue; }
				if (i++ % 5 == 0) { ++cy; cx = x+4; }
				out += Mv::to(cy, cx);
				if (count == selected_signal) out += Theme::c("selected_bg") + Theme::c("selected_fg") + Fx::b + ljust(to_string(count), 3) + ljust('(' + sig + ')', 12) + Fx::reset;
				else out += Theme::c("hi_fg") + ljust(to_string(count), 3) + Theme::c("main_fg") + ljust('(' + sig + ')', 12);
				if (redraw) mouse_mappings["button_" + to_string(count)] = {cy, cx, 1, 15};
				count++;
				cx += 15;
			}

			cy++;
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust( "↑ ↓ ← →", 33, true) + Theme::c("main_fg") + Fx::ub + " | To choose signal.";
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust("0-9", 33) + Theme::c("main_fg") + Fx::ub + " | Enter manually.";
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust("ENTER", 33) + Theme::c("main_fg") + Fx::ub + " | To send signal.";
			mouse_mappings["enter"] = {cy, x, 1, 73};
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust("ESC or \"q\"", 33) + Theme::c("main_fg") + Fx::ub + " | To abort.";
			mouse_mappings["escape"] = {cy, x, 1, 73};

			out += Fx::reset;
		}

		return (redraw ? Changed : retval);
	}

	static int sizeError(const string& key) {
		if (redraw) {
			vector<string> cont_vec {
				Fx::b + Theme::g("used")[100] + "Error:" + Theme::c("main_fg") + Fx::ub,
				"Terminal size too small to" + Fx::reset,
				"display menu or box!" + Fx::reset };

			messageBox = Menu::msgBox{45, 0, cont_vec, "error"};
			Global::overlay = messageBox();
		}

		auto ret = messageBox.input(key);
		if (ret == msgBox::Ok_Yes or ret == msgBox::No_Esc) {
			messageBox.clear();
			return Closed;
		}
		else if (redraw) {
			return Changed;
		}
		return NoChange;
	}

	static int signalSend(const string& key) {
		auto s_pid = (Config::getB("show_detailed") and Config::getI("selected_pid") == 0 ? Config::getI("detailed_pid") : Config::getI("selected_pid"));
		if (s_pid == 0) return Closed;
		if (redraw) {
			atomic_wait(Runner::active);
			auto& p_name = (s_pid == Config::getI("detailed_pid") ? Proc::detailed.entry.name : Config::getS("selected_name"));
			vector<string> cont_vec = {
				Fx::b + Theme::c("main_fg") + "Send signal: " + Fx::ub + Theme::c("hi_fg") + to_string(signalToSend)
				+ (signalToSend > 0 and signalToSend <= 32 ? Theme::c("main_fg") + " (" + P_Signals.at(signalToSend) + ')' : ""),

				Fx::b + Theme::c("main_fg") + "To PID: " + Fx::ub + Theme::c("hi_fg") + to_string(s_pid) + Theme::c("main_fg") + " ("
				+ uresize(p_name, 16) + ')' + Fx::reset,
			};
			messageBox = Menu::msgBox{50, 1, cont_vec, (signalToSend > 1 and signalToSend <= 32 and signalToSend != 17 ? P_Signals.at(signalToSend) : "signal")};
			Global::overlay = messageBox();
		}
		auto ret = messageBox.input(key);
		if (ret == msgBox::Ok_Yes) {
			signalKillRet = 0;
			if (kill(s_pid, signalToSend) != 0) {
				signalKillRet = errno;
				menuMask.set(SignalReturn);
			}
			messageBox.clear();
			return Closed;
		}
		else if (ret == msgBox::No_Esc) {
			messageBox.clear();
			return Closed;
		}
		else if (ret == msgBox::Select) {
			Global::overlay = messageBox();
			return Changed;
		}
		else if (redraw) {
			return Changed;
		}
		return NoChange;
	}

	static int signalReturn(const string& key) {
		if (redraw) {
			vector<string> cont_vec;
			cont_vec.push_back(Fx::b + Theme::g("used")[100] + "Failure:" + Theme::c("main_fg") + Fx::ub);
			if (signalKillRet == EINVAL) {
				cont_vec.push_back("Unsupported signal!" + Fx::reset);
			}
			else if (signalKillRet == EPERM) {
				cont_vec.push_back("Insufficient permissions to send signal!" + Fx::reset);
			}
			else if (signalKillRet == ESRCH) {
				cont_vec.push_back("Process not found!" + Fx::reset);
			}
			else {
				cont_vec.push_back("Unknown error! (errno: " + to_string(signalKillRet) + ')' + Fx::reset);
			}

			messageBox = Menu::msgBox{50, 0, cont_vec, "error"};
			Global::overlay = messageBox();
		}

		auto ret = messageBox.input(key);
		if (ret == msgBox::Ok_Yes or ret == msgBox::No_Esc) {
			messageBox.clear();
			return Closed;
		}
		else if (redraw) {
			return Changed;
		}
		return NoChange;
	}

	static int mainMenu(const string& key) {
		enum MenuItems { Options, Help, Quit };
		static int y{};
		static int selected{};
		static vector<string> colors_selected;
		static vector<string> colors_normal;
		auto tty_mode = Config::getB("tty_mode");
		if (bg.empty()) selected = 0;
		int retval = Changed;

		if (redraw) {
			y = Term::height/2 - 10;
			bg = Draw::banner_gen(y, 0, true);
			if (not tty_mode) {
				colors_selected = {
					Theme::hex_to_color(Global::Banner_src.at(2).at(1)),  //? Use TOP colors (cool), skip 2 leaf lines
					Theme::hex_to_color(Global::Banner_src.at(4).at(1)),
					Theme::hex_to_color(Global::Banner_src.at(6).at(1))
				};
				colors_normal = {
					Theme::hex_to_color("#CC"),
					Theme::hex_to_color("#AA"),
					Theme::hex_to_color("#80")
				};
			}
		}
		else if (is_in(key, "escape", "q", "m", "mouse_click")) {
			return Closed;
		}
		else if (key.starts_with("button_")) {
			if (int new_select = key.back() - '0'; new_select == selected)
				goto MainEntering;
			else
				selected = new_select;
		}
		else if (is_in(key, "enter", "space")) {
			MainEntering:
			switch (selected) {
				case Options:
					menuMask.set(Menus::Options);
					currentMenu = Menus::Options;
					return Switch;
				case Help:
					menuMask.set(Menus::Help);
					currentMenu = Menus::Help;
					return Switch;
				case Quit:
					clean_quit(0);
			}
		}
		else if (is_in(key, "down", "tab", "mouse_scroll_down", "j")) {
			if (++selected > 2) selected = 0;
		}
		else if (is_in(key, "up", "shift_tab", "mouse_scroll_up", "k")) {
			if (--selected < 0) selected = 2;
		}
		else {
			retval = NoChange;
		}

		if (retval == Changed) {
			auto& out = Global::overlay;
			out = bg + Fx::reset + Fx::b;
			auto cy = y + 8;  // One line spacing after banner
			for (const auto& i : iota(0, 3)) {
				if (tty_mode) out += (i == selected ? Theme::c("hi_fg") : Theme::c("main_fg"));
				const auto& menu = (not tty_mode and i == selected ? menu_selected[i] : menu_normal[i]);
				const auto& colors = (i == selected ? colors_selected : colors_normal);
				if (redraw) mouse_mappings["button_" + to_string(i)] = {cy, Term::width/2 - menu_width[i]/2, 3, menu_width[i]};
				for (int ic = 0; const auto& line : menu) {
					out += Mv::to(cy++, Term::width/2 - menu_width[i]/2) + (tty_mode ? "" : colors[ic++]) + line;
				}
			}
			//? Add version below QUIT
			auto version_str = "v" + Global::Version;
			out += Mv::to(cy + 1, Term::width/2 - (int)version_str.size()/2)
				+ Theme::c("main_fg") + Fx::b + Fx::i + version_str;
			out += Fx::reset;
		}

		return (redraw ? Changed : retval);
	}

static int optionsMenu(const string& key) {
 		enum Predispositions { isBool, isInt, isString, is2D, isBrowsable, isEditable};
		static int y{};
		static int x{};
		static int height{};
		static int page{};
		static int pages{};
		static int selected{};
		static int select_max{};
		static int item_height{};
		static int selected_cat{};
		static int max_items{};
		static int last_sel{};
		static bool editing{};
		static Draw::TextEdit editor;
		static string warnings;
		static bitset<8> selPred;
		static const std::unordered_map<string, std::reference_wrapper<const vector<string>>> optionsList = {
			{"color_theme", std::cref(Theme::themes)},
			{"log_level", std::cref(Logger::log_levels)},
			{"temp_scale", std::cref(Config::temp_scales)},
		#ifdef __linux__
			{"freq_mode", std::cref(Config::freq_modes)},
		#endif
			{"proc_sorting", std::cref(Proc::sort_vector)},
			{"graph_symbol", std::cref(Config::valid_graph_symbols)},
			{"graph_symbol_cpu", std::cref(Config::valid_graph_symbols_def)},
			{"graph_symbol_mem", std::cref(Config::valid_graph_symbols_def)},
			{"graph_symbol_net", std::cref(Config::valid_graph_symbols_def)},
			{"graph_symbol_proc", std::cref(Config::valid_graph_symbols_def)},
			{"cpu_graph_upper", std::cref(Cpu::available_fields)},
			{"cpu_graph_lower", std::cref(Cpu::available_fields)},
			{"cpu_sensor", std::cref(Cpu::available_sensors)},
			{"selected_battery", std::cref(Config::available_batteries)},
	        {"base_10_bitrate", std::cref(Config::base_10_bitrate_values)},
		#ifdef GPU_SUPPORT
			{"show_gpu_info", std::cref(Config::show_gpu_values)},
			{"graph_symbol_gpu", std::cref(Config::valid_graph_symbols_def)},
		#endif
		};
		auto tty_mode = Config::getB("tty_mode");
		auto vim_keys = Config::getB("vim_keys");
		if (max_items == 0) {
			for (const auto& cat : categories) {
				if ((int)cat.size() > max_items) max_items = cat.size();
			}
		}
		if (bg.empty()) {
			page = selected = selected_cat = last_sel = 0;
			redraw = true;
			Theme::updateThemes();
		}
		int retval = Changed;
		bool recollect{};
		bool screen_redraw{};
		bool theme_refresh{};

		//? Draw background if needed else process input
		if (redraw) {
			mouse_mappings.clear();
			selPred.reset();
			y = max(1, Term::height/2 - 3 - max_items);
			x = Term::width/2 - 39;
			height = min(Term::height - 7, max_items * 2 + 4);
			if (height % 2 != 0) height--;
			bg 	= Draw::banner_gen(y, 0, true)
				+ Draw::createBox(x, y + 6, 78, height, Theme::c("hi_fg"), true, fmt::format("{}tab{}", Theme::c("hi_fg"), Theme::c("main_fg")) + Symbols::right)
				+ Mv::to(y+8, x) + Theme::c("hi_fg") + Symbols::div_left + Theme::c("div_line") + Symbols::h_line * 29
				+ Symbols::div_up + Symbols::h_line * (78 - 32) + Theme::c("hi_fg") + Symbols::div_right
				+ Mv::to(y+6+height - 1, x+30) + Symbols::div_down + Theme::c("div_line");
			for (const auto& i : iota(0, height - 4)) {
				bg += Mv::to(y+9 + i, x + 30) + Symbols::v_line;
			}
		}
		else if (not warnings.empty() and not key.empty()) {
			auto ret = messageBox.input(key);
			if (ret == msgBox::msgReturn::Ok_Yes or ret == msgBox::msgReturn::No_Esc) {
				warnings.clear();
				messageBox.clear();
			}
		}
		else if (editing and not key.empty()) {
			if (is_in(key, "escape", "mouse_click")) {
				editor.clear();
				editing = false;
			}
			else if (key == "enter") {
				const auto& option = categories[selected_cat][item_height * page + selected][0];
				if (selPred.test(isString) and Config::stringValid(option, editor.text)) {
					Config::set(option, editor.text);
					if (option == "custom_cpu_name" or option.starts_with("custom_gpu_name"))
						screen_redraw = true;
					else if (is_in(option, "shown_boxes", "presets")) {
						screen_redraw = true;
						Config::current_preset = -1;
					}
					else if (option == "clock_format") {
						Draw::update_clock(true);
						screen_redraw = true;
					}
					else if (option == "preset_names") {
						screen_redraw = true;
					}
					else if (option == "cpu_core_map") {
						atomic_wait(Runner::active);
						Cpu::core_mapping = Cpu::get_core_mapping();
					}
				}
				else if (selPred.test(isInt) and Config::intValid(option, editor.text)) {
					Config::set(option, stoi_safe(editor.text));
				}
				else
					warnings = Config::validError;

				editor.clear();
				editing = false;
			}
			else if (not editor.command(key))
				retval = NoChange;
		}
		else if (key == "mouse_click") {
			const auto [mouse_x, mouse_y] = Input::mouse_pos;
			if (mouse_x < x or mouse_x > x + 80 or mouse_y < y + 6 or mouse_y > y + 6 + height) {
				return Closed;
			}
			else if (mouse_x < x + 30 and mouse_y > y + 8) {
				auto m_select = ceil((double)(mouse_y - y - 8) / 2) - 1;
				if (selected != m_select)
					selected = m_select;
				else if (selPred.test(isEditable))
					goto mouseEnter;
				else retval = NoChange;
			}
		}
		else if (is_in(key, "enter", "e", "E") and selPred.test(isEditable)) {
			mouseEnter:
			const auto& option = categories[selected_cat][item_height * page + selected][0];
			editor = Draw::TextEdit{Config::getAsString(option), selPred.test(isInt)};
			editing = true;
			mouse_mappings.clear();
		}
		else if (is_in(key, "escape", "q", "o", "backspace")) {
			return Closed;
		}
		else if (is_in(key, "down", "mouse_scroll_down") or (vim_keys and key == "j")) {
			if (++selected > select_max or selected >= item_height) {
				if (page < pages - 1) page++;
				else if (pages > 1) page = 0;
				selected = 0;
			}
		}
		else if (is_in(key, "up", "mouse_scroll_up") or (vim_keys and key == "k")) {
			if (--selected < 0) {
				if (page > 0) page--;
				else if (pages > 1) page = pages - 1;

				selected = item_height - 1;
			}
		}
		else if (pages > 1 and key == "page_down") {
			if (++page >= pages) page = 0;
			selected = 0;
			last_sel = -1;
		}
		else if (pages > 1 and key == "page_up") {
			if (--page < 0) page = pages - 1;
			selected = 0;
			last_sel = -1;
		}
		else if (key == "tab") {
			if (++selected_cat >= (int)categories.size()) selected_cat = 0;
			page = selected = 0;
		}
		else if (key == "shift_tab") {
			if (--selected_cat < 0) selected_cat = (int)categories.size() - 1;
			page = selected = 0;
		}
#ifdef GPU_SUPPORT
		else if (is_in(key, "1", "2", "3", "4", "5", "6") or key.starts_with("select_cat_")) {
#else
		else if (is_in(key, "1", "2", "3", "4", "5") or key.starts_with("select_cat_")) {
#endif
		selected_cat = key.back() - '0' - 1;
			page = selected = 0;
		}
		else if (is_in(key, "left", "right") or (vim_keys and is_in(key, "h", "l"))) {
			const auto& option = categories[selected_cat][item_height * page + selected][0];
			if (selPred.test(isInt)) {
				const int mod = (option == "update_ms" ? 100 : 1);
				long value = Config::getI(option);
				if (key == "right" or (vim_keys and key == "l")) value += mod;
				else value -= mod;

				if (Config::intValid(option, to_string(value)))
					Config::set(option, static_cast<int>(value));
				else {
					warnings = Config::validError;
				}
			}
			else if (selPred.test(isBool)) {
				Config::flip(option);
				screen_redraw = true;

				// Special handling for options that need additional action.
				if (option == "truecolor") {
					theme_refresh = true;
					Config::flip("lowcolor");
				}
				else if (option == "force_tty") {
					theme_refresh = true;
					Config::flip("tty_mode");
				}
				else if (is_in(option, "rounded_corners", "theme_background"))
					theme_refresh = true;
				else if (option == "background_update") {
					Runner::pause_output = false;
				}
				else if (option == "base_10_sizes") {
					recollect = true;
				}
				else if (option == "save_config_on_exit" and not Config::getB("save_config_on_exit")) {
					const bool old_write_new = Config::write_new;
					Config::write_new = true;
					Config::write();
					Config::write_new = old_write_new;
				}
				else if (option == "disable_mouse") {
					const auto is_mouse_enabled = !Config::getB("disable_mouse");
					std::cout << (is_mouse_enabled ? Term::mouse_on : Term::mouse_off) << std::flush;
				}
				else if (option == "clock_12h") {
					Draw::update_clock(true);
				}
				else if (option == "show_hostname") {
					Draw::update_hostname(true);
				}
				else if (is_in(option, "show_uptime_header", "show_username_header")) {
					// These trigger a full redraw which is already set above
				}
			}
			else if (selPred.test(isBrowsable)) {
				auto& optList = optionsList.at(option).get();
				int i = v_index(optList, Config::getS(option));

				if ((key == "right" or (vim_keys and key == "l")) and ++i >= (int)optList.size()) i = 0;
				else if ((key == "left" or (vim_keys and key == "h")) and --i < 0) i = optList.size() - 1;
				Config::set(option, optList.at(i));

				if (option == "color_theme")
					theme_refresh = true;
				else if (option == "log_level") {
					Logger::set_log_level(optList.at(i));
					Logger::info("Logger set to {}", optList.at(i));
				}
				else if (option == "base_10_bitrate") {
				    recollect = true;
				}
				else if (is_in(option, "proc_sorting", "cpu_sensor", "show_gpu_info") or option.starts_with("graph_symbol") or option.starts_with("cpu_graph_"))
					screen_redraw = true;
			}
			else
				retval = NoChange;
		}
		else {
			retval = NoChange;
		}

		//? Draw the menu
		if (retval == Changed) {
			Config::unlock();
			auto& out = Global::overlay;
			out = bg;
			item_height = min((int)categories[selected_cat].size(), (int)floor((double)(height - 4) / 2));
			pages = ceil((double)categories[selected_cat].size() / item_height);
			if (page > pages - 1) page = pages - 1;
			select_max = min(item_height - 1, (int)categories[selected_cat].size() - 1 - item_height * page);
			if (selected > select_max) {
				selected = select_max;
			}

			//? Get variable properties for currently selected option
			if (selPred.none() or last_sel != (selected_cat << 8) + selected) {
				selPred.reset();
				last_sel = (selected_cat << 8) + selected;
				const auto& selOption = categories[selected_cat][item_height * page + selected][0];
				if (Config::ints.contains(selOption))
					selPred.set(isInt);
				else if (Config::bools.contains(selOption))
					selPred.set(isBool);
				else
					selPred.set(isString);

				if (not selPred.test(isString))
					selPred.set(is2D);
				else if (optionsList.contains(selOption)) {
					selPred.set(isBrowsable);
				}
				if (not selPred.test(isBrowsable) and (selPred.test(isString) or selPred.test(isInt)))
					selPred.set(isEditable);
			}

			//? Category buttons
			out += Mv::to(y+7, x+4);
		#ifdef GPU_SUPPORT
			for (int i = 0; const auto& m : {"general", "cpu", "gpu", "mem", "net", "proc"}) {
		#else
			for (int i = 0; const auto& m : {"general", "cpu", "mem", "net", "proc"}) {
		#endif
				out += Fx::b + (i == selected_cat
						? Theme::c("hi_fg") + '[' + Theme::c("title") + m + Theme::c("hi_fg") + ']'
						: Theme::c("hi_fg") + to_string(i + 1) + Theme::c("title") + m + ' ')
				#ifdef GPU_SUPPORT
					+ Mv::r(7);
				#else
					+ Mv::r(10);
				#endif

#if !defined(GPU_SUPPORT)
				constexpr static auto option_menu_tab_width = 15;
#else
				constexpr static auto option_menu_tab_width = 12;
#endif
				if (const auto button_name = fmt::format("select_cat_{}", i + 1); not editing and not mouse_mappings.contains(button_name)) {
					mouse_mappings[button_name] = {
						.line = y + 6,
						.col = x + 2 + (option_menu_tab_width * i),
						.height = 3,
						.width = option_menu_tab_width,
					};
				}
				i++;
			}
			if (pages > 1) {
				out += Mv::to(y+6 + height - 1, x+2) + Theme::c("hi_fg") + Symbols::title_left_down + Fx::b + Symbols::up + Theme::c("title") + " page "
					+ to_string(page+1) + '/' + to_string(pages) + ' ' + Theme::c("hi_fg") + Symbols::down + Fx::ub + Symbols::title_right_down;
			}
			//? Option name and value
			auto cy = y+9;
			for (int c = 0, i = max(0, item_height * page); c++ < item_height and i < (int)categories[selected_cat].size(); i++) {
				const auto& option = categories[selected_cat][i][0];
				const auto& value = (option == "color_theme" ? fs::path(Config::getS("color_theme")).stem().string() : Config::getAsString(option));

				out += Mv::to(cy++, x + 1) + (c-1 == selected ? Theme::c("selected_bg") + Theme::c("selected_fg") : Theme::c("title"))
					+ Fx::b + cjust(capitalize(s_replace(option, "_", " "))
						+ (c-1 == selected and selPred.test(isBrowsable)
							? ' ' + to_string(v_index(optionsList.at(option).get(), (option == "color_theme" ? Config::getS("color_theme") : value)) + 1) + '/' + to_string(optionsList.at(option).get().size())
							: ""), 29);
				out	+= Mv::to(cy++, x + 1) + (c-1 == selected ? "" : Theme::c("main_fg")) + Fx::ub + "  "
					+ (c-1 == selected and editing ? cjust(editor(24), 34, true) : cjust(value, 25, true)) + "  ";

				if (c-1 == selected) {
					if (not editing and (selPred.test(is2D) or selPred.test(isBrowsable))) {
						out += Fx::b + Mv::to(cy-1, x+2) + Symbols::left + Mv::to(cy-1, x+28) + Symbols::right;
						mouse_mappings["left"] = {cy-2, x, 2, 5};
						mouse_mappings["right"] = {cy-2, x+25, 2, 5};
					}
					if (selPred.test(isEditable)) {
						out += Fx::b + Mv::to(cy-1, x+28 - (not editing and selPred.test(isInt) ? 2 : 0)) + (tty_mode ? "E" : Symbols::enter);
					}
					//? Description of selected option
					out += Fx::reset + Theme::c("title") + Fx::b;
					for (int cyy = y+7; const auto& desc : categories[selected_cat][i]) {
						if (cyy++ == y+7) continue;
						else if (cyy == y+10) out += Theme::c("main_fg") + Fx::ub;
						else if (cyy > y + height + 4) break;
						out += Mv::to(cyy, x+32) + desc;
					}
				}
			}

			if (not warnings.empty()) {
				messageBox = msgBox{min(78, (int)ulen(warnings) + 10), msgBox::BoxTypes::OK, {uresize(warnings, 74)}, "warning"};
				out += messageBox();
			}

			out += Fx::reset;
		}

		if (theme_refresh) {
			Theme::setTheme();
			Draw::banner_gen(0, 0, false, true);
			screen_redraw = true;
			redraw = true;
			optionsMenu("");
		}
		if (screen_redraw) {
			auto overlay_bkp = std::move(Global::overlay);
			auto clock_bkp = std::move(Global::clock);
			Draw::calcSizes();
			Global::overlay = std::move(overlay_bkp);
			Global::clock = std::move(clock_bkp);
			recollect = true;
		}
		if (recollect) {
			Runner::run("all", false, true);
			retval = NoChange;
		}

		return (redraw ? Changed : retval);
	}

	static int helpMenu(const string& key) {
		static int y{};
		static int x{};
		static int height{};
		static int page{};
		static int pages{};

		if (bg.empty()) page = 0;
		int retval = Changed;

		if (redraw) {
			y = max(1, Term::height/2 - 4 - (int)(help_text.size() / 2));
			x = Term::width/2 - 39;
			height = min(Term::height - 6, (int)help_text.size() + 3);
			pages = ceil((double)help_text.size() / (height - 3));
			page = 0;
			bg = Draw::banner_gen(y, 0, true);
			bg += Draw::createBox(x, y + 6, 78, height, Theme::c("hi_fg"), true, "help");
		}
		else if (is_in(key, "escape", "q", "h", "backspace", "space", "enter", "mouse_click")) {
			return Closed;
		}
		else if (pages > 1 and is_in(key, "down", "j", "page_down", "tab", "mouse_scroll_down")) {
			if (++page >= pages) page = 0;
		}
		else if (pages > 1 and is_in(key, "up", "k", "page_up", "shift_tab", "mouse_scroll_up")) {
			if (--page < 0) page = pages - 1;
		}
		else {
			retval = NoChange;
		}


		if (retval == Changed) {
			auto& out = Global::overlay;
			out = bg;
			if (pages > 1) {
				out += Mv::to(y+height+6, x + 2) + Theme::c("hi_fg") + Symbols::title_left_down + Fx::b + Symbols::up + Theme::c("title") + " page "
					+ to_string(page+1) + '/' + to_string(pages) + ' ' + Theme::c("hi_fg") + Symbols::down + Fx::ub + Symbols::title_right_down;
			}
			auto cy = y+7;
			out += Mv::to(cy++, x + 1) + Theme::c("title") + Fx::b + cjust("Key:", 20) + "Description:";
			for (int c = 0, i = max(0, (height - 3) * page); c++ < height - 3 and i < (int)help_text.size(); i++) {
				out += Mv::to(cy++, x + 1) + Theme::c("hi_fg") + Fx::b + cjust(help_text[i][0], 20)
					+ Theme::c("main_fg") + Fx::ub + help_text[i][1];
			}
			out += Fx::reset;
		}


		return (redraw ? Changed : retval);
	}

	static int reniceMenu(const string& key) {
		auto s_pid = (Config::getB("show_detailed") and Config::getI("selected_pid") == 0 ? Config::getI("detailed_pid") : Config::getI("selected_pid"));
		static int x{};
		static int y{};
		static int selected_nice = 0;
		static string nice_edit;

		if (bg.empty()) {
			selected_nice = 0;
			nice_edit.clear();
		}
		auto& out = Global::overlay;
		int retval = Changed;

		if (redraw) {
			x = Term::width/2 - 25;
			y = Term::height/2 - 6;
			bg = Draw::createBox(x + 2, y, 50, 13, Theme::c("hi_fg"), true, "renice");
			bg += Mv::to(y+2, x+3) + Theme::c("title") + Fx::b + cjust("Renice PID " + to_string(s_pid) + " ("
				+ uresize((s_pid == Config::getI("detailed_pid") ? Proc::detailed.entry.name : Config::getS("selected_name")), 15) + ")", 48);
		}
		else if (is_in(key, "escape", "q")) {
			return Closed;
		}
		else if (is_in(key, "enter", "space")) {
			if (s_pid > 0) {
				if (not nice_edit.empty()) {
					selected_nice = stoi_safe(nice_edit, 0);
				}
				if (not Proc::set_priority(s_pid, selected_nice)) {
					// TODO: show error message
				}
			}
			return Closed;
		}
		else if (key.size() == 1 and (isdigit(key.at(0)) or (key.at(0) == '-' and nice_edit.empty()))) {
			nice_edit += key;
		}
		else if (key == "backspace" and not nice_edit.empty()) {
			nice_edit.pop_back();
		}
		else if (is_in(key, "up", "k")) {
			if (++selected_nice > 19) selected_nice = -20;
			nice_edit.clear();
		}
		else if (is_in(key, "down", "j")) {
			if (--selected_nice < -20) selected_nice = 19;
			nice_edit.clear();
		}
		else if (is_in(key, "left", "h")) {
			if ((selected_nice -= 5) < -20) selected_nice += 40;
			nice_edit.clear();
		}
		else if (is_in(key, "right", "l")) {
			if ((selected_nice += 5) > 19) selected_nice -= 40;
			nice_edit.clear();
		}
		else {
			retval = NoChange;
		}

		if (retval == Changed) {
			int cy = y+4;
			if (not nice_edit.empty()) {
				selected_nice = stoi_safe(nice_edit, 0);
			}
			out = bg + Mv::to(cy++, x+3) + Theme::c("main_fg") + Fx::ub
				+ rjust("Enter nice value: ", 30) + Theme::c("hi_fg") + (nice_edit.empty() ? to_string(selected_nice) : nice_edit) + Theme::c("main_fg") + Fx::bl + "█" + Fx::ubl;

			cy++;
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust( "↑ ↓", 20, true) + Theme::c("main_fg") + Fx::ub + " | To change value.";
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust( "← →", 20, true) + Theme::c("main_fg") + Fx::ub + " | To change value by 5.";
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust("0-9", 20) + Theme::c("main_fg") + Fx::ub + " | Enter manually.";
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust("ENTER", 20) + Theme::c("main_fg") + Fx::ub + " | To set nice value.";
			out += Mv::to(++cy, x+3) + Fx::b + Theme::c("hi_fg") + rjust("ESC or 'q'", 20) + Theme::c("main_fg") + Fx::ub + " | To abort.";

			out += Fx::reset;
		}

		return (redraw ? Changed : retval);
	}

#if defined(__APPLE__) && __MAC_OS_X_VERSION_MIN_REQUIRED > 101504
	static int vramAllocMenu(const string& key) {
		static int x{};
		static int y{};
		static int selected_option = 0;
		static string custom_value;
		static vector<std::pair<string, long long>> options;  // {label, value_mb}

		if (bg.empty()) {
			selected_option = 0;
			custom_value.clear();

			//? Calculate options based on total RAM
			long long total_ram = Gpu::get_total_ram();
			long long total_ram_mb = total_ram / (1024LL * 1024);

			options.clear();
			options.push_back({"Default (system managed)", 0});

			//? Generate options at 66%, 75%, 83%, 92% of RAM
			vector<int> percentages = {66, 75, 83, 92};
			for (int pct : percentages) {
				long long value_mb = (total_ram_mb * pct) / 100;
				long long value_gb = value_mb / 1024;
				string label = fmt::format("{} GiB ({}%)", value_gb, pct);
				options.push_back({label, value_mb});
			}
			options.push_back({"Custom...", -1});
		}

		auto& out = Global::overlay;
		int retval = Changed;

		if (redraw) {
			int box_width = 40;
			int box_height = static_cast<int>(options.size()) + 8;
			x = Term::width/2 - box_width/2;
			y = Term::height/2 - box_height/2;
			bg = Draw::createBox(x, y, box_width, box_height, Theme::c("hi_fg"), true, "GPU Memory Allocation");

			//? Show current limit
			int64_t current_limit_mb = 0;
			size_t size = sizeof(current_limit_mb);
			sysctlbyname("iogpu.wired_limit_mb", &current_limit_mb, &size, nullptr, 0);
			string current_str = (current_limit_mb > 0)
				? fmt::format("{:.1f} GiB", static_cast<double>(current_limit_mb) / 1024.0)
				: "System default";
			bg += Mv::to(y+2, x+2) + Theme::c("title") + Fx::b + cjust("Current: " + current_str, box_width - 4) + Fx::ub;
		}
		else if (is_in(key, "escape", "q")) {
			return Closed;
		}
		else if (is_in(key, "enter", "space")) {
			if (selected_option >= 0 and selected_option < static_cast<int>(options.size())) {
				long long value_mb = options[selected_option].second;

				if (value_mb == -1) {
					//? Custom option - parse input
					if (not custom_value.empty()) {
						value_mb = stoi_safe(custom_value, 0) * 1024;  // Convert GB to MB
					} else {
						return (redraw ? Changed : NoChange);  // Stay in menu if no value entered
					}
				}

				//? Execute the allocation change
				Logger::debug("Calling set_gpu_memory_limit({})", value_mb);
				bool result = Gpu::set_gpu_memory_limit(value_mb);
				Logger::debug("set_gpu_memory_limit returned: {}", result);
				//? Note: Don't force GPU refresh - let it happen naturally to avoid race conditions
			}
			Logger::debug("Returning Closed from vramAllocMenu");
			return Closed;
		}
		else if (key.size() == 1 and isdigit(key.at(0))) {
			//? Allow number input for custom value
			if (options[selected_option].second == -1) {
				if (custom_value.size() < 3) custom_value += key;  // Max 3 digits (999 GB)
			}
		}
		else if (key == "backspace") {
			if (not custom_value.empty()) custom_value.pop_back();
		}
		else if (is_in(key, "up", "k")) {
			if (--selected_option < 0) selected_option = static_cast<int>(options.size()) - 1;
			custom_value.clear();
		}
		else if (is_in(key, "down", "j")) {
			if (++selected_option >= static_cast<int>(options.size())) selected_option = 0;
			custom_value.clear();
		}
		else {
			retval = NoChange;
		}

		if (retval == Changed) {
			int cy = y + 4;
			out = bg;

			//? Draw options
			for (size_t i = 0; i < options.size(); ++i) {
				bool is_selected = (static_cast<int>(i) == selected_option);
				string prefix = is_selected ? Theme::c("hi_fg") + Fx::b + "> " : "  ";
				string suffix = is_selected ? Fx::ub + Theme::c("main_fg") : "";

				if (options[i].second == -1 and is_selected) {
					//? Custom option with input field
					string input_display = custom_value.empty() ? "_" : custom_value + " GiB" + Fx::bl + "█" + Fx::ubl;
					out += Mv::to(cy++, x+2) + prefix + "Custom: " + input_display + suffix;
				} else {
					out += Mv::to(cy++, x+2) + prefix + options[i].first + suffix;
				}
			}

			cy++;
			out += Mv::to(++cy, x+2) + Theme::c("hi_fg") + Fx::b + "↑↓" + Fx::ub + Theme::c("main_fg") + " Navigate  "
				+ Theme::c("hi_fg") + Fx::b + "Enter" + Fx::ub + Theme::c("main_fg") + " Apply  "
				+ Theme::c("hi_fg") + Fx::b + "Esc" + Fx::ub + Theme::c("main_fg") + " Cancel";

			out += Fx::reset;
		}

		return (redraw ? Changed : retval);
	}
#else
	//? Stub for non-Apple platforms
	static int vramAllocMenu(const string&) {
		return Closed;
	}
#endif

	//* Wrapper to call the new MenuV2 options menu
	//? Set to 1 to use the new MenuV2 options menu instead of the old one
	#define USE_MENUV2_OPTIONS 1

	#ifdef USE_MENUV2_OPTIONS
	//? Forward declaration for MenuV2::optionsMenuV2 (defined later in this file)
	int optionsMenuV2_wrapper(const string& key);
	#endif

	static int optionsMenuWrapper(const string& key) {
		#ifdef USE_MENUV2_OPTIONS
		return optionsMenuV2_wrapper(key);
		#else
		return optionsMenu(key);
		#endif
	}

	//* Add menus here and update enum Menus in header
	const auto menuFunc = vector{
		ref(sizeError),
		ref(signalChoose),
		ref(signalSend),
		ref(signalReturn),
		ref(optionsMenuWrapper),
		ref(helpMenu),
		ref(reniceMenu),
		ref(vramAllocMenu),
		ref(mainMenu),
	};
	bitset<16> menuMask;

	void process(const std::string_view key) {
		if (menuMask.none()) {
			Menu::active = false;
			Global::overlay.clear();
			Global::overlay.shrink_to_fit();
			Runner::pause_output = false;
			bg.clear();
			bg.shrink_to_fit();
			currentMenu = -1;
			// If theme or boxes changed during menu session, apply changes and trigger full refresh
			// Aggressive clear to prevent display corruption from cached colors
			if (MenuV2::theme_changed_pending or MenuV2::boxes_changed_pending) {
				std::cout << Term::clear << std::flush;  // Clear screen completely first
				if (MenuV2::theme_changed_pending) {
					Theme::setTheme();         // Load the new theme colors
				}
				Draw::calcSizes();             // Recalculate all box sizes and clear caches
				Global::resized = true;        // Trigger full refresh in main loop
				MenuV2::theme_changed_pending = false;
				MenuV2::boxes_changed_pending = false;
			}
			else {
				Runner::run("all", true, true);
			}
			mouse_mappings.clear();
			return;
		}

		if (currentMenu < 0 or not menuMask.test(currentMenu)) {
			Menu::active = true;
			redraw = true;
			if (((menuMask.test(Main) or menuMask.test(Options) or menuMask.test(Help) or menuMask.test(SignalChoose))
			and (Term::width < 80 or Term::height < 24))
			or (Term::width < 50 or Term::height < 20)) {
				menuMask.reset();
				menuMask.set(SizeError);
			}

			for (const auto& i : iota(0, (int)menuMask.size())) {
				if (menuMask.test(i)) currentMenu = i;
			}

		}

		auto retCode = menuFunc.at(currentMenu)(key.data());
		if (retCode == Closed) {
			menuMask.reset(currentMenu);
			mouse_mappings.clear();
			bg.clear();
			Runner::pause_output = false;
			process();
		}
		else if (redraw) {
			redraw = false;
			Runner::run("all", true, true);
		}
		else if (retCode == Changed)
			Runner::run("overlay");
		else if (retCode == Switch) {
			Runner::pause_output = false;
			bg.clear();
			redraw = true;
			mouse_mappings.clear();
			process();
		}
	}

	void show(int menu, int signal) {
		menuMask.set(menu);
		signalToSend = signal;
		process();
	}
}

//? ====================== MenuV2 Implementation ======================
namespace MenuV2 {
	using namespace Tools;
	using namespace std;

	//? Flag to track theme change during menu session - triggers full refresh on exit
	bool theme_changed_pending = false;

	//? Flag to track box layout change during menu session - triggers full refresh on exit
	bool boxes_changed_pending = false;

	//? ==================== Clock Format Display Helper ====================
	//? Translates strftime codes to human-readable examples
	string clockFormatToReadable(const string& fmt) {
		static const std::unordered_map<string, string> clock_display = {
			{"%X", "14:30:45 (locale)"},
			{"%H:%M", "14:30"},
			{"%H:%M:%S", "14:30:45"},
			{"%I:%M %p", "2:30 PM"},
			{"%I:%M:%S %p", "2:30:45 PM"},
			{"", "(disabled)"}
		};
		auto it = clock_display.find(fmt);
		return (it != clock_display.end()) ? it->second : fmt;
	}

	//? ==================== Box Toggle Helpers ====================
	//? Maps pseudo-config keys to box names in shown_boxes
	static const std::unordered_map<string, string> box_key_to_name = {
		{"box_show_cpu", "cpu"},
		{"box_show_gpu", "gpu0"},
		{"box_show_pwr", "pwr"},
		{"box_show_mem", "mem"},
		{"box_show_net", "net"},
		{"box_show_proc", "proc"}
	};

	//? Check if a box is enabled in shown_boxes
	bool isBoxEnabled(const string& key) {
		auto it = box_key_to_name.find(key);
		if (it == box_key_to_name.end()) return false;
		const string& box_name = it->second;
		const string& shown = Config::getS("shown_boxes");
		return shown.find(box_name) != string::npos;
	}

	//? Toggle a box in shown_boxes
	void toggleBox(const string& key) {
		auto it = box_key_to_name.find(key);
		if (it == box_key_to_name.end()) return;
		const string& box_name = it->second;
		string shown = Config::getS("shown_boxes");

		size_t pos = shown.find(box_name);
		if (pos != string::npos) {
			// Remove the box - handle leading/trailing spaces
			size_t end = pos + box_name.length();
			// Remove trailing space if exists
			if (end < shown.length() && shown[end] == ' ') end++;
			// Or remove leading space if at end
			else if (pos > 0 && shown[pos-1] == ' ') pos--;
			shown.erase(pos, end - pos);
		} else {
			// Add the box at appropriate position (maintain order: cpu pwr gpu0 mem net proc)
			const vector<string> order = {"cpu", "pwr", "gpu0", "mem", "net", "proc"};
			vector<string> current_boxes;
			for (const auto& b : ssplit(shown)) {
				current_boxes.push_back(b);
			}
			current_boxes.push_back(box_name);
			// Sort by order
			std::sort(current_boxes.begin(), current_boxes.end(), [&order](const string& a, const string& b) {
				auto pos_a = std::find(order.begin(), order.end(), a);
				auto pos_b = std::find(order.begin(), order.end(), b);
				return pos_a < pos_b;
			});
			shown.clear();
			for (size_t i = 0; i < current_boxes.size(); i++) {
				if (i > 0) shown += " ";
				shown += current_boxes[i];
			}
		}
		Config::set("shown_boxes", shown);
		boxes_changed_pending = true;
		Config::current_preset = -1;
	}

	//? Check if key is a box toggle pseudo-config
	bool isBoxToggleKey(const string& key) {
		return box_key_to_name.find(key) != box_key_to_name.end();
	}

	//? ==================== UI Component Renderers ====================

	string drawToggle(bool value, bool selected, bool tty_mode) {
		const string on_char = tty_mode ? "[x]" : "[" + Theme::c("proc_misc") + "●" + Theme::c("main_fg") + "]";
		const string off_char = tty_mode ? "[ ]" : "[" + Theme::c("inactive_fg") + " " + Theme::c("main_fg") + "]";

		string result;
		if (selected) {
			result = Theme::c("selected_bg") + Theme::c("selected_fg");
		}
		result += (value ? on_char : off_char);
		if (selected) {
			result += Fx::reset;
		}
		return result;
	}

	string drawRadio(const vector<string>& options, int current_idx, bool selected, bool tty_mode) {
		string result;
		const string sel_char = tty_mode ? "(*)" : Theme::c("proc_misc") + "●" + Theme::c("main_fg");
		const string unsel_char = tty_mode ? "( )" : Theme::c("inactive_fg") + "○" + Theme::c("main_fg");

		for (size_t i = 0; i < options.size(); i++) {
			if (i > 0) result += "  ";
			bool is_current = (static_cast<int>(i) == current_idx);

			if (selected and is_current) {
				result += Theme::c("selected_bg") + Theme::c("selected_fg");
			}
			result += (is_current ? sel_char : unsel_char) + " " + options[i];
			if (selected and is_current) {
				result += Fx::reset;
			}
		}
		return result;
	}

	string drawSlider(int min_val, int max_val, int current, int width, bool selected, bool tty_mode) {
		if (width < 10) width = 10;

		float ratio = static_cast<float>(current - min_val) / static_cast<float>(max_val - min_val);
		int pos = static_cast<int>(ratio * (width - 2));
		if (pos < 0) pos = 0;
		if (pos > width - 2) pos = width - 2;

		string bar;
		const string track_char = tty_mode ? "-" : "─";
		const string handle_char = tty_mode ? "O" : "●";

		bar += "[";
		for (int i = 0; i < width - 2; i++) {
			if (i == pos) {
				bar += Theme::c("proc_misc") + handle_char + Theme::c("main_fg");
			} else if (i < pos) {
				bar += Theme::c("proc_misc") + track_char + Theme::c("main_fg");
			} else {
				bar += Theme::c("inactive_fg") + track_char + Theme::c("main_fg");
			}
		}
		bar += "]";

		string result;
		if (selected) {
			result = Theme::c("selected_bg") + Theme::c("selected_fg");
		}
		result += bar + " " + to_string(current);
		if (selected) {
			result += Fx::reset;
		}
		return result;
	}

	string drawSelect(const string& value, int width, bool selected, bool open) {
		string result;
		string display_val = value;
		if (static_cast<int>(display_val.size()) > width - 4) {
			display_val = display_val.substr(0, width - 7) + "...";
		}

		if (selected) {
			result = Theme::c("selected_bg") + Theme::c("selected_fg");
		}

		result += "[ " + ljust(display_val, width - 4) + (open ? "▲" : "▼") + "]";

		if (selected) {
			result += Fx::reset;
		}
		return result;
	}

	//? ==================== Preset Preview Renderer ====================

	string drawPresetPreview(const PresetDef& preset, int width, int height) {
		if (width < 20 or height < 8) return "Preview too small";

		vector<string> lines(height, string(width, ' '));

		auto drawBox = [&](int x, int y, int w, int h, const string& label) {
			if (y < 0 or y >= height or x < 0 or x >= width) return;
			// Top border
			if (y < height) {
				for (int i = x; i < x + w and i < width; i++) {
					lines[y][i] = (i == x) ? '+' : (i == x + w - 1) ? '+' : '-';
				}
			}
			// Bottom border
			if (y + h - 1 < height) {
				for (int i = x; i < x + w and i < width; i++) {
					lines[y + h - 1][i] = (i == x) ? '+' : (i == x + w - 1) ? '+' : '-';
				}
			}
			// Sides and content
			for (int row = y + 1; row < y + h - 1 and row < height; row++) {
				if (x < width) lines[row][x] = '|';
				if (x + w - 1 < width) lines[row][x + w - 1] = '|';
			}
			// Center label
			if (y + h / 2 < height and not label.empty()) {
				int label_start = x + (w - static_cast<int>(label.size())) / 2;
				for (size_t i = 0; i < label.size() and label_start + static_cast<int>(i) < width; i++) {
					if (label_start + static_cast<int>(i) >= 0) {
						lines[y + h / 2][label_start + i] = label[i];
					}
				}
			}
		};

		int y_pos = 0;

		// CPU always at top if enabled
		if (preset.cpu_enabled) {
			drawBox(0, y_pos, width, 3, "CPU");
			y_pos += 3;
		}

		// GPU under CPU
		if (preset.gpu_enabled) {
			drawBox(0, y_pos, width, 3, "GPU");
			y_pos += 3;
		}

		// PWR under GPU
		if (preset.pwr_enabled) {
			drawBox(0, y_pos, width, 3, "PWR");
			y_pos += 3;
		}

		int remaining_height = height - y_pos;
		if (remaining_height < 4) remaining_height = 4;

		// Memory, Network, Processes layout
		bool has_mem = (preset.mem_layout != MemLayout::Hidden);
		bool has_net = (preset.net_layout != NetLayout::Hidden);
		bool has_proc = (preset.proc_layout != ProcLayout::Hidden);

		if (preset.mem_layout == MemLayout::Horizontal and preset.net_layout == NetLayout::Horizontal) {
			// MEM H | NET H side by side, PROC H below
			int mem_net_height = has_proc ? remaining_height / 2 : remaining_height;
			int half_width = width / 2;

			if (has_mem) {
				string mem_label = (preset.disk_mode == DiskMode::Hidden) ? "MEM H" : "MEM+DSK";
				drawBox(0, y_pos, half_width, mem_net_height, mem_label);
			}
			if (has_net) {
				drawBox(half_width, y_pos, width - half_width, mem_net_height, "NET H");
			}

			if (has_proc and preset.proc_layout == ProcLayout::Horizontal) {
				drawBox(0, y_pos + mem_net_height, width, remaining_height - mem_net_height, "PROC H");
			}
		}
		else if (preset.mem_layout == MemLayout::Vertical) {
			// Vertical layout: MEM V on left, potentially NET V + PROC V on right
			int left_width = width / 3;
			int right_width = width - left_width;

			string mem_label = (preset.disk_mode == DiskMode::Hidden) ? "MEM V" : "MEM V+D";
			drawBox(0, y_pos, left_width, remaining_height, mem_label);

			if (preset.net_layout == NetLayout::Vertical and preset.proc_layout == ProcLayout::Vertical) {
				int net_height = remaining_height / 2;
				drawBox(left_width, y_pos, right_width, net_height, "NET V");
				drawBox(left_width, y_pos + net_height, right_width, remaining_height - net_height, "PROC V");
			}
			else if (preset.proc_layout == ProcLayout::Vertical) {
				drawBox(left_width, y_pos, right_width, remaining_height, "PROC V");
			}
			else if (preset.proc_layout == ProcLayout::Horizontal) {
				int proc_height = remaining_height / 3;
				drawBox(left_width, y_pos, right_width, remaining_height - proc_height, has_net ? "NET" : "");
				drawBox(0, y_pos + remaining_height - proc_height, width, proc_height, "PROC H");
			}
		}
		else {
			// Simple fallback
			if (has_proc) {
				drawBox(0, y_pos, width, remaining_height, "PROC");
			}
		}

		// Join lines
		string result;
		for (const auto& line : lines) {
			result += line + "\n";
		}
		if (not result.empty()) result.pop_back(); // Remove trailing newline
		return result;
	}

	//? ==================== Category Definitions ====================

	const vector<Category>& getCategories() {
		static vector<Category> cats;

		if (cats.empty()) {
			// Category 1: System
			cats.push_back({
				"System", "",
				{
					{"Update", {
						{"update_ms", "Update Interval", "Update time in milliseconds (100-86400000)", ControlType::Slider, {}, "", 100, 10000, 100},
						{"background_update", "Background Update", "Update UI when menus are showing", ControlType::Toggle, {}, "", 0, 0, 0},
						{"terminal_sync", "Terminal Sync", "Use synchronized output to reduce flickering", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Input", {
						{"vim_keys", "Vim Keys (hjkl)", "Enable h,j,k,l for directional control", ControlType::Toggle, {}, "", 0, 0, 0},
						{"disable_mouse", "Disable Mouse", "Disable all mouse events", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Units", {
						{"base_10_sizes", "Base 10 Sizes", "Use KB=1000 instead of KiB=1024", ControlType::Toggle, {}, "", 0, 0, 0},
						{"base_10_bitrate", "Bitrate Units", "How to display network bitrates", ControlType::Radio, {"Auto", "True", "False"}, "", 0, 0, 0},
					}},
					{"Logging", {
						{"log_level", "Log Level", "Logging verbosity for error.log", ControlType::Radio, {"DISABLED", "ERROR", "WARNING", "INFO", "DEBUG"}, "", 0, 0, 0},
					}},
					{"Config", {
						{"save_config_on_exit", "Save on Exit", "Auto-save settings when exiting", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Advanced", {
						{"cpu_core_map", "CPU Core Map", "Override CPU core detection (comma-separated)", ControlType::Text, {}, "", 0, 0, 0},
					#ifdef __linux__
						{"freq_mode", "Frequency Mode", "CPU frequency display mode", ControlType::Radio, {"first", "range", "lowest", "highest", "average"}, "", 0, 0, 0},
					#endif
					}},
					{"Compatibility", {
						{"lowcolor", "Low Color Mode", "Use 256-color mode instead of truecolor", ControlType::Toggle, {}, "", 0, 0, 0},
						{"tty_mode", "TTY Mode", "Force TTY compatible output (16 colors)", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
				},
				false, {}
			});

			// Category 2: Appearance
			cats.push_back({
				"Appearance", "",
				{
					{"Theme", {
						{"color_theme", "Color Theme", "Select color theme from themes folder", ControlType::Select, {}, "color_theme", 0, 0, 0},
						{"theme_background", "Theme Background", "Show theme background color", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Colors", {
						{"truecolor", "True Color (24-bit)", "Use 24-bit colors (disable for 256-color terminals)", ControlType::Toggle, {}, "", 0, 0, 0},
						{"force_tty", "Force TTY Mode", "Force 16-color TTY mode", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Borders", {
						{"rounded_corners", "Rounded Corners", "Use rounded corners on boxes", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Graph Style", {
						{"graph_symbol", "Default Graph Symbol", "Symbol style for all graphs", ControlType::Radio, {"braille", "block", "tty"}, "", 0, 0, 0},
					}},
					{"Title Bar", {
						{"clock_format", "Clock Format", "Time display style in header", ControlType::Select, {}, "clock_format", 0, 0, 0},
						{"show_hostname", "Show Hostname", "Display hostname in header (centered)", ControlType::Toggle, {}, "", 0, 0, 0},
						{"show_uptime_header", "Show Uptime", "Display uptime in header", ControlType::Toggle, {}, "", 0, 0, 0},
						{"show_username_header", "Show Username", "Display username in header", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Battery", {
						{"show_battery", "Show Battery", "Display battery status if present", ControlType::Toggle, {}, "", 0, 0, 0},
						{"selected_battery", "Battery", "Which battery to monitor", ControlType::Select, {}, "selected_battery", 0, 0, 0},
						{"show_battery_watts", "Show Battery Power", "Display charge/discharge power", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Layout", {
						{"box_show_cpu", "CPU", "Show CPU panel", ControlType::Toggle, {}, "", 0, 0, 0},
						{"box_show_gpu", "GPU", "Show GPU panel", ControlType::Toggle, {}, "", 0, 0, 0},
						{"box_show_pwr", "PWR", "Show Power panel", ControlType::Toggle, {}, "", 0, 0, 0},
						{"box_show_mem", "MEM", "Show Memory panel", ControlType::Toggle, {}, "", 0, 0, 0},
						{"box_show_net", "NET", "Show Network panel", ControlType::Toggle, {}, "", 0, 0, 0},
						{"box_show_proc", "PROC", "Show Processes panel", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
				},
				false, {}
			});

			// Category 3: Panels (with sub-tabs)
			cats.push_back({
				"Panels", "",
				{
					//? ==================== CPU Sub-tab ====================
					{"CPU | Display", {
						{"cpu_bottom", "CPU at Bottom", "Show CPU box at bottom of screen", ControlType::Toggle, {}, "", 0, 0, 0},
						{"show_uptime", "Show Uptime", "Display uptime in CPU box", ControlType::Toggle, {}, "", 0, 0, 0},
						{"show_cpu_freq", "Show Frequency", "Display CPU frequency", ControlType::Toggle, {}, "", 0, 0, 0},
						{"show_cpu_watts", "Show Power", "Display CPU power consumption", ControlType::Toggle, {}, "", 0, 0, 0},
						{"custom_cpu_name", "Custom CPU Name", "Override CPU model name (empty to disable)", ControlType::Text, {}, "", 0, 0, 0},
					}},
					{"CPU | Temperature", {
						{"check_temp", "Show Temperature", "Enable CPU temperature display", ControlType::Toggle, {}, "", 0, 0, 0},
						{"show_coretemp", "Show Core Temps", "Display per-core temperatures", ControlType::Toggle, {}, "", 0, 0, 0},
						{"temp_scale", "Temperature Scale", "Temperature unit", ControlType::Radio, {"celsius", "fahrenheit", "kelvin", "rankine"}, "", 0, 0, 0},
						{"cpu_sensor", "Temperature Sensor", "Select CPU temperature sensor", ControlType::Select, {}, "cpu_sensor", 0, 0, 0},
					}},
					{"CPU | Graphs", {
						{"graph_symbol_cpu", "Graph Symbol", "Symbol for CPU graphs", ControlType::Radio, {"default", "braille", "block", "tty"}, "", 0, 0, 0},
						{"cpu_graph_upper", "Upper Graph", "Stat for upper half of CPU graph", ControlType::Select, {}, "cpu_graph_upper", 0, 0, 0},
						{"cpu_graph_lower", "Lower Graph", "Stat for lower half of CPU graph", ControlType::Select, {}, "cpu_graph_lower", 0, 0, 0},
						{"cpu_single_graph", "Single Graph", "Disable lower graph (upper only)", ControlType::Toggle, {}, "", 0, 0, 0},
						{"cpu_invert_lower", "Invert Lower", "Flip orientation of lower graph", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
				#ifdef GPU_SUPPORT
					{"CPU | GPU Info", {
						{"show_gpu_info", "Show GPU Info", "Display GPU stats in CPU box", ControlType::Radio, {"Auto", "On", "Off"}, "", 0, 0, 0},
					}},

					//? ==================== GPU Sub-tab ====================
					{"GPU | Display", {
						{"gpu_mirror_graph", "Mirror Graph", "Mirror GPU graph horizontally", ControlType::Toggle, {}, "", 0, 0, 0},
						{"nvml_measure_pcie_speeds", "Measure PCIe Speeds", "Enable PCIe bandwidth measurement (NVIDIA)", ControlType::Toggle, {}, "", 0, 0, 0},
						{"shown_gpus", "Shown GPUs", "Which GPU vendors to show (nvidia amd intel)", ControlType::Text, {}, "", 0, 0, 0},
					}},
					{"GPU | Graphs", {
						{"graph_symbol_gpu", "Graph Symbol", "Symbol for GPU graphs", ControlType::Radio, {"default", "braille", "block", "tty"}, "", 0, 0, 0},
					}},
				#endif

					//? ==================== Memory Sub-tab ====================
					{"Mem | Display", {
						{"mem_below_net", "Memory Below Net", "Place memory box below network box", ControlType::Toggle, {}, "", 0, 0, 0},
						{"net_beside_mem", "Net Beside Mem", "Show network box beside memory box", ControlType::Toggle, {}, "", 0, 0, 0},
						{"mem_graphs", "Show Mem Graphs", "Display memory usage graphs", ControlType::Toggle, {}, "", 0, 0, 0},
						{"mem_horizontal", "Horizontal Layout", "Show memory graphs side by side", ControlType::Toggle, {}, "", 0, 0, 0},
						{"mem_bar_mode", "Bar Mode", "Use meter bars instead of graphs", ControlType::Toggle, {}, "", 0, 0, 0},
						{"show_swap", "Show Swap", "Display swap memory info", ControlType::Toggle, {}, "", 0, 0, 0},
						{"swap_disk", "Swap as Disk", "Show swap as disk entry", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Mem | Items", {
						{"mem_show_used", "Show Used", "Display used memory", ControlType::Toggle, {}, "", 0, 0, 0},
						{"mem_show_available", "Show Available", "Display available memory", ControlType::Toggle, {}, "", 0, 0, 0},
						{"mem_show_cached", "Show Cached", "Display cached memory", ControlType::Toggle, {}, "", 0, 0, 0},
						{"mem_show_free", "Show Free", "Display free memory", ControlType::Toggle, {}, "", 0, 0, 0},
						{"swap_show_used", "Swap Used", "Display swap used", ControlType::Toggle, {}, "", 0, 0, 0},
						{"swap_show_free", "Swap Free", "Display swap free", ControlType::Toggle, {}, "", 0, 0, 0},
						{"vram_show_used", "VRAM Used", "Display VRAM used", ControlType::Toggle, {}, "", 0, 0, 0},
						{"vram_show_free", "VRAM Free", "Display VRAM free", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Mem | Graphs", {
						{"graph_symbol_mem", "Graph Symbol", "Symbol for memory graphs", ControlType::Radio, {"default", "braille", "block", "tty"}, "", 0, 0, 0},
					}},
					{"Mem | ZFS", {
						{"zfs_arc_cached", "ZFS ARC as Cache", "Count ZFS ARC as cached memory", ControlType::Toggle, {}, "", 0, 0, 0},
						{"zfs_hide_datasets", "Hide ZFS Datasets", "Hide individual ZFS datasets from disk list", ControlType::Toggle, {}, "", 0, 0, 0},
					}},

					//? ==================== Disk Sub-tab ====================
					{"Disk | Display", {
						{"show_disks", "Show Disks", "Enable disk usage display", ControlType::Toggle, {}, "", 0, 0, 0},
						{"only_physical", "Only Physical", "Hide virtual/loop devices", ControlType::Toggle, {}, "", 0, 0, 0},
						{"show_network_drives", "Network Drives", "Show NFS/SMB/AFP network drives", ControlType::Toggle, {}, "", 0, 0, 0},
						{"use_fstab", "Use fstab", "Use fstab for disk detection", ControlType::Toggle, {}, "", 0, 0, 0},
						{"disk_free_priv", "Free as Privileged", "Calculate free space as privileged user", ControlType::Toggle, {}, "", 0, 0, 0},
						{"disks_filter", "Disks Filter", "Select disk to show (empty=all)", ControlType::Select, {}, "disks_filter", 0, 0, 0},
					}},
					{"Disk | I/O", {
						{"show_io_stat", "Show I/O Stats", "Display disk I/O statistics", ControlType::Toggle, {}, "", 0, 0, 0},
						{"io_mode", "I/O Mode", "Toggle detailed I/O display mode", ControlType::Toggle, {}, "", 0, 0, 0},
						{"io_graph_combined", "Combined I/O Graph", "Combine read/write into single graph", ControlType::Toggle, {}, "", 0, 0, 0},
						{"io_graph_speeds", "Graph Speeds", "I/O graph speed reference (comma-separated)", ControlType::Text, {}, "", 0, 0, 0},
					}},

					//? ==================== Network Sub-tab ====================
					{"Net | Display", {
						{"net_auto", "Auto Select", "Auto-select primary network interface", ControlType::Toggle, {}, "", 0, 0, 0},
						{"net_iface", "Interface", "Select network interface to display", ControlType::Select, {}, "net_iface", 0, 0, 0},
						{"net_sync", "Sync Scales", "Synchronize upload/download graph scales", ControlType::Toggle, {}, "", 0, 0, 0},
						{"swap_upload_download", "Swap Up/Down", "Swap upload and download positions", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Net | Reference", {
						{"net_download", "Download Ref", "Download speed reference value (Mebibits)", ControlType::Slider, {}, "", 1, 10000, 10},
						{"net_upload", "Upload Ref", "Upload speed reference value (Mebibits)", ControlType::Slider, {}, "", 1, 10000, 10},
					}},
					{"Net | Graphs", {
						{"graph_symbol_net", "Graph Symbol", "Symbol for network graphs", ControlType::Radio, {"default", "braille", "block", "tty"}, "", 0, 0, 0},
					}},

					//? ==================== Processes Sub-tab ====================
					{"Proc | Display", {
						{"proc_left", "Processes Left", "Place processes box on left side", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_full_width", "Full Width", "Show proc panel full width (with net_beside_mem)", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_tree", "Tree View", "Show process tree hierarchy", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_colors", "Enable Colors", "Enable colored process info", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_gradient", "Gradient", "Enable gradient colors in process list", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_per_core", "Per Core", "Show per-core CPU usage", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_mem_bytes", "Memory as Bytes", "Show memory usage in bytes", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_show_cmd", "Show Command", "Show command column", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Proc | Behavior", {
						{"proc_aggregate", "Aggregate", "Aggregate child process stats in tree view", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_info_smaps", "Use smaps", "Use smaps for accurate memory (slower)", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_filter_kernel", "Filter Kernel", "Filter out kernel processes (Linux)", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_follow_detailed", "Follow Detailed", "Follow selected process in detailed view", ControlType::Toggle, {}, "", 0, 0, 0},
						{"keep_dead_proc_usage", "Keep Dead Usage", "Preserve CPU/mem usage for dead processes", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Proc | Sorting", {
						{"proc_sorting", "Default Sort", "Default process sorting column", ControlType::Select, {}, "proc_sorting", 0, 0, 0},
						{"proc_reversed", "Reverse Sort", "Reverse sort order", ControlType::Toggle, {}, "", 0, 0, 0},
					}},
					{"Proc | Graphs", {
						{"proc_cpu_graphs", "CPU Graphs", "Show mini CPU graphs in process list", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_gpu", "GPU Column", "Show GPU usage column (Apple Silicon)", ControlType::Toggle, {}, "", 0, 0, 0},
						{"proc_gpu_graphs", "GPU Graphs", "Show mini GPU graphs in process list", ControlType::Toggle, {}, "", 0, 0, 0},
						{"graph_symbol_proc", "Graph Symbol", "Symbol for process graphs", ControlType::Radio, {"default", "braille", "block", "tty"}, "", 0, 0, 0},
					}},
				},
				true, {"CPU", "GPU", "PWR", "Memory", "Network", "Processes"}
			});

			// Category 4: Presets
			cats.push_back({
				"Presets", "",
				{
					{"Presets", {
						// Presets use special handling - not regular options
					}},
				},
				false, {}
			});
		}

		return cats;
	}

	//? ==================== Preset Conversion ====================

	string PresetDef::toConfigString() const {
		string result;

		// Build shown_boxes style string based on layout
		vector<string> boxes;
		if (cpu_enabled) boxes.push_back("cpu:0:" + graph_symbol);
		if (gpu_enabled) boxes.push_back("gpu0:0:" + graph_symbol);
		if (pwr_enabled) boxes.push_back("pwr:0:" + graph_symbol);

		if (mem_layout != MemLayout::Hidden) {
			int pos = (mem_layout == MemLayout::Vertical) ? 0 : 0;
			boxes.push_back("mem:" + to_string(pos) + ":" + graph_symbol);
		}
		if (net_layout != NetLayout::Hidden) {
			int pos = (net_layout == NetLayout::Vertical) ? 0 : 0;
			boxes.push_back("net:" + to_string(pos) + ":" + graph_symbol);
		}
		if (proc_layout != ProcLayout::Hidden) {
			int pos = (proc_layout == ProcLayout::Vertical) ? 1 : 0;
			boxes.push_back("proc:" + to_string(pos) + ":" + graph_symbol);
		}

		for (size_t i = 0; i < boxes.size(); i++) {
			if (i > 0) result += ",";
			result += boxes[i];
		}

		return result;
	}

	PresetDef PresetDef::fromConfigString(const string& config, const string& preset_name) {
		PresetDef preset;
		preset.name = preset_name;
		preset.cpu_enabled = false;
		preset.gpu_enabled = false;
		preset.pwr_enabled = false;
		preset.mem_layout = MemLayout::Hidden;
		preset.net_layout = NetLayout::Hidden;
		preset.proc_layout = ProcLayout::Hidden;

		auto parts = ssplit(config, ',');
		for (const auto& part : parts) {
			auto box_parts = ssplit(part, ':');
			if (box_parts.empty()) continue;

			string box_name = box_parts[0];
			int position = (box_parts.size() > 1) ? stoi_safe(box_parts[1], 0) : 0;
			string symbol = (box_parts.size() > 2) ? string(box_parts[2]) : "default";

			if (preset.graph_symbol == "default" and symbol != "default") {
				preset.graph_symbol = symbol;
			}

			if (box_name == "cpu") {
				preset.cpu_enabled = true;
			}
			else if (box_name.starts_with("gpu")) {
				preset.gpu_enabled = true;
			}
			else if (box_name == "pwr") {
				preset.pwr_enabled = true;
			}
			else if (box_name == "mem") {
				preset.mem_layout = (position == 1) ? MemLayout::Vertical : MemLayout::Horizontal;
			}
			else if (box_name == "net") {
				preset.net_layout = (position == 1) ? NetLayout::Vertical : NetLayout::Horizontal;
			}
			else if (box_name == "proc") {
				preset.proc_layout = (position == 1) ? ProcLayout::Vertical : ProcLayout::Horizontal;
			}
		}

		return preset;
	}

	vector<PresetDef> getPresets() {
		vector<PresetDef> presets;
		presets.resize(9); // Presets 1-9

		// Get preset names
		auto preset_names_str = Config::getS("preset_names");
		auto names = ssplit(preset_names_str);

		// Get presets config string
		auto presets_str = Config::getS("presets");
		auto preset_configs = ssplit(presets_str);

		for (int i = 0; i < 9; i++) {
			string name = (i < static_cast<int>(names.size())) ? string(names[i]) : "";
			string config = (i < static_cast<int>(preset_configs.size())) ? string(preset_configs[i]) : "";

			if (config.empty()) {
				presets[i].name = name;
			} else {
				presets[i] = PresetDef::fromConfigString(config, name);
			}
		}

		return presets;
	}

	void savePresets(const vector<PresetDef>& presets) {
		string names_str;
		string configs_str;

		for (size_t i = 0; i < presets.size() and i < 9; i++) {
			if (i > 0) {
				names_str += " ";
				configs_str += " ";
			}
			names_str += presets[i].name.empty() ? ("Preset" + to_string(i + 1)) : presets[i].name;
			configs_str += presets[i].toConfigString();
		}

		Config::set("preset_names", names_str);
		Config::set("presets", configs_str);
	}

	//? ==================== Dynamic Choices Helper ====================

	//? Get dynamic choices for Select controls based on choices_ref
	vector<string> getDynamicChoices(const string& choices_ref) {
		vector<string> choices;

		if (choices_ref == "net_iface") {
			// Return available network interfaces
			choices.push_back("");  // Empty = auto-select
			for (const auto& iface : Net::interfaces) {
				choices.push_back(iface);
			}
		}
		else if (choices_ref == "disks_filter") {
			// Return available disk mountpoints for filtering
			// Get the last collected memory data to access disks
			choices.push_back("");  // Empty = show all disks
			auto mem = Mem::collect(true);  // no_update = true to get cached data
			for (const auto& mount : mem.disks_order) {
				choices.push_back(mount);
			}
		}
		else if (choices_ref == "color_theme") {
			// Return available themes - Theme::themes is a vector<string>
			for (const auto& theme : Theme::themes) {
				choices.push_back(theme);
			}
		}
		else if (choices_ref == "proc_sorting") {
			// Fixed choices for process sorting
			choices = {"pid", "program", "arguments", "threads", "user", "memory", "cpu lazy", "cpu direct"};
		}
		else if (choices_ref == "cpu_sensor") {
			// Return available CPU temperature sensors
			choices.push_back("");  // Empty = auto-select
			for (const auto& sensor : Cpu::available_sensors) {
				choices.push_back(sensor);
			}
		}
		else if (choices_ref == "cpu_graph_upper" or choices_ref == "cpu_graph_lower") {
			// Fixed choices for CPU graph stats
			choices = {"total", "user", "system", "iowait", "irq", "steal", "guest"};
		}
		else if (choices_ref == "selected_battery") {
			// Return available batteries
			choices.push_back("Auto");
			// Add detected batteries if available
		}
		else if (choices_ref == "clock_format") {
			// Common clock format choices
			choices = {"%X", "%H:%M", "%H:%M:%S", "%I:%M %p", "%I:%M:%S %p", ""};
		}

		return choices;
	}

	//? ==================== Main Options Menu V2 ====================

	int optionsMenuV2(const string& key) {
		// This is a placeholder - full implementation requires substantial UI work
		// For now, redirect to the classic menu
		// TODO: Implement full V2 menu with tabs, proper controls, preset editor

		// Static state variables
		static int selected_cat = 0;
		static int selected_subcat = 0;
		static int selected_option = 0;
		static int scroll_offset = 0;
		static bool editing = false;
		static Draw::TextEdit editor;
		static bool dropdown_open = false;
		static int dropdown_selection = 0;
		static vector<string> dropdown_choices;
		static string dropdown_key;            //? Track which option key opened the dropdown
		static bool theme_refresh = false;

		const auto& cats = getCategories();
		auto tty_mode = Config::getB("tty_mode");
		auto vim_keys = Config::getB("vim_keys");

		// Dialog dimensions - responsive height (min 30, max 60), always centered
		const int dialog_width = 76;
		const int min_dialog_height = 30;
		const int max_dialog_height = 60;
		const int banner_height = 9;      // MBTOP banner is 9 rows
		const int settings_height = 3;    // SETTINGS text is 3 rows
		const int banner_spacing = 0;     // Space between banner and SETTINGS
		const int dialog_spacing = 0;     // Space between SETTINGS and dialog

		// Calculate what we can show based on available space
		// Priority: Dialog (min 30) > SETTINGS text > MBTOP banner
		const int total_with_all = banner_height + banner_spacing + settings_height + dialog_spacing + min_dialog_height + 2;
		const int total_with_banner_only = banner_height + dialog_spacing + min_dialog_height + 2;

		bool show_banner = (Term::height >= total_with_banner_only);
		bool show_settings_text = (Term::height >= total_with_all);

		// Calculate dialog height based on available space
		int available_for_dialog;
		if (show_settings_text) {
			available_for_dialog = Term::height - banner_height - banner_spacing - settings_height - dialog_spacing - 2;
		} else if (show_banner) {
			available_for_dialog = Term::height - banner_height - dialog_spacing - 2;
		} else {
			available_for_dialog = Term::height - 4;
		}
		const int dialog_height = max(min_dialog_height, min(available_for_dialog, max_dialog_height));

		// Calculate positions - everything centered
		const int x = (Term::width - dialog_width) / 2;
		int y;
		if (show_settings_text) {
			// Banner at top, SETTINGS below, dialog below that
			y = banner_height + banner_spacing + settings_height + dialog_spacing + 1;
		} else if (show_banner) {
			// Banner at top, dialog below
			y = banner_height + dialog_spacing + 1;
		} else {
			// Just dialog, centered
			y = max(1, (Term::height - dialog_height) / 2);
		}

		auto& out = Global::overlay;
		int retval = Menu::Changed;

		// Lambda to rebuild Menu::bg with current theme colors
		// This is called on initial draw AND after theme changes
		auto rebuildMenuBg = [&]() {
			Menu::mouse_mappings.clear();
			Menu::bg.clear();

			// Draw MBTOP banner if there's room
			if (show_banner) {
				Menu::bg = Draw::banner_gen(1, 0, true);
			}

			// Draw SETTINGS text if there's room
			if (show_settings_text) {
				const array<string, 3> settings_text = {
					"╔═╗╔═╗╔╦╗╔╦╗╦╔╗╔╔═╗╔═╗",
					"╚═╗╠╣  ║  ║ ║║║║║ ╦╚═╗",
					"╚═╝╚═╝ ╩  ╩ ╩╝╚╝╚═╝╚═╝"
				};
				const int settings_width = 22;
				int settings_y = banner_height + banner_spacing;
				int settings_x = Term::width / 2 - settings_width / 2;

				// Draw blank mask around SETTINGS text
				int settings_mask_x = max(1, settings_x - 1);
				int settings_mask_y = max(1, settings_y - 1);
				int settings_mask_w = min(settings_width + 2, Term::width - settings_mask_x + 1);
				int settings_mask_h = settings_height + 2;
				string settings_blank(settings_mask_w, ' ');
				for (int row = 0; row < settings_mask_h; row++) {
					Menu::bg += Mv::to(settings_mask_y + row, settings_mask_x) + Theme::c("main_bg") + settings_blank;
				}

				// Use gradient colors from banner
				vector<string> colors = {
					Theme::hex_to_color(Global::Banner_src.at(2).at(1)),
					Theme::hex_to_color(Global::Banner_src.at(4).at(1)),
					Theme::hex_to_color(Global::Banner_src.at(6).at(1))
				};
				for (int i = 0; i < 3; i++) {
					Menu::bg += Mv::to(settings_y + i, settings_x) + Fx::b + colors[i] + settings_text[i];
				}
				Menu::bg += Fx::ub;
			}

			// Draw blank mask around the dialog box (1 char padding)
			const int mask_padding = 1;
			int mask_x = max(1, x - mask_padding);
			int mask_y = max(1, y - mask_padding);
			int mask_width = min(dialog_width + mask_padding * 2, Term::width - mask_x + 1);
			int mask_height = min(dialog_height + mask_padding * 2, Term::height - mask_y + 1);
			string blank_line(mask_width, ' ');
			for (int row = 0; row < mask_height; row++) {
				Menu::bg += Mv::to(mask_y + row, mask_x) + Theme::c("main_bg") + blank_line;
			}

			// Draw dialog background
			Menu::bg += Draw::createBox(x, y, dialog_width, dialog_height, Theme::c("hi_fg"), true, "MBTOP Settings");

			// Mouse mappings for tabs (visual drawing is done dynamically)
			for (size_t i = 0; i < cats.size(); i++) {
				Menu::mouse_mappings["tab_" + to_string(i)] = {
					y + 1, x + 2 + static_cast<int>(i) * 14, 3, 13
				};
			}

			// Double-line separator under categories
			const string double_h_line = "═";
			Menu::bg += Mv::to(y + 3, x) + Theme::c("hi_fg") + "╞"
				+ Theme::c("div_line");
			for (int i = 0; i < dialog_width - 2; i++) Menu::bg += double_h_line;
			Menu::bg += Theme::c("hi_fg") + "╡";
		};

		// Handle theme change - DON'T apply during menu, only mark for full app refresh on exit
		if (theme_refresh) {
			theme_refresh = false;
			theme_changed_pending = true;         // Mark for full refresh when menu closes
		}

		if (Menu::redraw) {
			rebuildMenuBg();
		}
		else if (dropdown_open) {
			// Handle dropdown selection modal
			if (is_in(key, "escape", "q", "backspace")) {
				dropdown_open = false;
				dropdown_choices.clear();
			}
			else if (key == "enter" or key == "space") {
				// Apply selection
				const auto& cat = cats[selected_cat];
				if (selected_subcat < static_cast<int>(cat.subcats.size())) {
					const auto& subcat = cat.subcats[selected_subcat];
					if (selected_option < static_cast<int>(subcat.options.size())) {
						const auto& opt = subcat.options[selected_option];
						if (dropdown_selection < static_cast<int>(dropdown_choices.size())) {
							Config::set(opt.key, dropdown_choices[dropdown_selection]);
							// Trigger theme refresh for theme-related options
							if (opt.key == "color_theme") {
								theme_refresh = true;
							}
							// Sync clock_12h config when clock_format changes
							else if (opt.key == "clock_format") {
								const string& fmt = dropdown_choices[dropdown_selection];
								// 12-hour formats contain %I (hour) or %p (AM/PM)
								bool is_12h = (fmt.find("%I") != string::npos or fmt.find("%p") != string::npos);
								Config::set("clock_12h", is_12h);
							}
							Menu::redraw = true;  // Redraw to show new selection
						}
					}
				}
				dropdown_open = false;
				dropdown_choices.clear();
			}
			else if (is_in(key, "down", "j") or (vim_keys and key == "j")) {
				if (dropdown_selection < static_cast<int>(dropdown_choices.size()) - 1) {
					dropdown_selection++;
					Menu::redraw = true;
				}
			}
			else if (is_in(key, "up", "k") or (vim_keys and key == "k")) {
				if (dropdown_selection > 0) {
					dropdown_selection--;
					Menu::redraw = true;
				}
			}
			else {
				retval = Menu::NoChange;
			}
		}
		else if (editing) {
			// Handle text editing
			if (is_in(key, "escape", "mouse_click")) {
				editor.clear();
				editing = false;
			}
			else if (key == "enter") {
				// Apply edited value
				const auto& cat = cats[selected_cat];
				if (selected_subcat < static_cast<int>(cat.subcats.size())) {
					const auto& subcat = cat.subcats[selected_subcat];
					if (selected_option < static_cast<int>(subcat.options.size())) {
						const auto& opt = subcat.options[selected_option];
						if (opt.control == ControlType::Text or opt.control == ControlType::Slider) {
							if (Config::stringValid(opt.key, editor.text)) {
								Config::set(opt.key, editor.text);
							}
						}
					}
				}
				editor.clear();
				editing = false;
			}
			else if (not editor.command(key)) {
				retval = Menu::NoChange;
			}
		}
		else {
			// Helper lambda to get current option
			auto getCurrentOption = [&]() -> const OptionDef* {
				const auto& cat = cats[selected_cat];
				if (selected_subcat < static_cast<int>(cat.subcats.size())) {
					const auto& subcat = cat.subcats[selected_subcat];
					if (selected_option < static_cast<int>(subcat.options.size())) {
						return &subcat.options[selected_option];
					}
				}
				return nullptr;
			};

			// Navigation handling
			if (is_in(key, "escape", "q", "backspace")) {
				// Just return - full app theme refresh happens when entire menu system closes
				return Menu::Closed;
			}
			else if (key == "tab") {
				selected_cat = (selected_cat + 1) % static_cast<int>(cats.size());
				selected_subcat = 0;
				selected_option = 0;
				scroll_offset = 0;
				Menu::redraw = true;
			}
			else if (key == "shift_tab") {
				selected_cat = (selected_cat - 1 + static_cast<int>(cats.size())) % static_cast<int>(cats.size());
				selected_subcat = 0;
				selected_option = 0;
				scroll_offset = 0;
				Menu::redraw = true;
			}
			else if (is_in(key, "right", "l") or (vim_keys and key == "l")) {
				// Right arrow: change Radio/Select/Slider value or switch tab if no option selected
				const auto* opt = getCurrentOption();
				if (opt) {
					if (opt->control == ControlType::Radio and not opt->choices.empty()) {
						// Find current value index and cycle to next
						string current = Config::getAsString(opt->key);
						int idx = 0;
						for (size_t i = 0; i < opt->choices.size(); i++) {
							if (opt->choices[i] == current) {
								idx = static_cast<int>(i);
								break;
							}
						}
						idx = (idx + 1) % static_cast<int>(opt->choices.size());
						Config::set(opt->key, opt->choices[idx]);
					}
					else if (opt->control == ControlType::Select and not opt->choices_ref.empty()) {
						// Get dynamic choices and cycle to next
						auto choices = getDynamicChoices(opt->choices_ref);
						if (not choices.empty()) {
							string current = Config::getAsString(opt->key);
							int idx = 0;
							for (size_t i = 0; i < choices.size(); i++) {
								if (choices[i] == current) {
									idx = static_cast<int>(i);
									break;
								}
							}
							idx = (idx + 1) % static_cast<int>(choices.size());
							Config::set(opt->key, choices[idx]);
							// Trigger theme refresh for color_theme
							if (opt->key == "color_theme") {
								theme_refresh = true;
							}
						}
					}
					else if (opt->control == ControlType::Slider) {
						int current = Config::getI(opt->key);
						int new_val = min(current + opt->step, opt->max_val);
						Config::set(opt->key, new_val);
					}
					else if (opt->control == ControlType::Toggle) {
						if (isBoxToggleKey(opt->key)) {
							toggleBox(opt->key);
						} else {
							Config::flip(opt->key);
							// Trigger theme refresh for theme-related options
							if (is_in(opt->key, "truecolor", "force_tty", "theme_background", "rounded_corners")) {
								theme_refresh = true;
								if (opt->key == "truecolor") Config::flip("lowcolor");
								else if (opt->key == "force_tty") Config::flip("tty_mode");
							}
						}
					}
				}
			}
			else if (is_in(key, "left", "h") or (vim_keys and key == "h")) {
				// Left arrow: change Radio/Select/Slider value or switch tab if no option selected
				const auto* opt = getCurrentOption();
				if (opt) {
					if (opt->control == ControlType::Radio and not opt->choices.empty()) {
						// Find current value index and cycle to previous
						string current = Config::getAsString(opt->key);
						int idx = 0;
						for (size_t i = 0; i < opt->choices.size(); i++) {
							if (opt->choices[i] == current) {
								idx = static_cast<int>(i);
								break;
							}
						}
						idx = (idx - 1 + static_cast<int>(opt->choices.size())) % static_cast<int>(opt->choices.size());
						Config::set(opt->key, opt->choices[idx]);
					}
					else if (opt->control == ControlType::Select and not opt->choices_ref.empty()) {
						// Get dynamic choices and cycle to previous
						auto choices = getDynamicChoices(opt->choices_ref);
						if (not choices.empty()) {
							string current = Config::getAsString(opt->key);
							int idx = 0;
							for (size_t i = 0; i < choices.size(); i++) {
								if (choices[i] == current) {
									idx = static_cast<int>(i);
									break;
								}
							}
							idx = (idx - 1 + static_cast<int>(choices.size())) % static_cast<int>(choices.size());
							Config::set(opt->key, choices[idx]);
							// Trigger theme refresh for color_theme
							if (opt->key == "color_theme") {
								theme_refresh = true;
							}
						}
					}
					else if (opt->control == ControlType::Slider) {
						int current = Config::getI(opt->key);
						int new_val = max(current - opt->step, opt->min_val);
						Config::set(opt->key, new_val);
					}
					else if (opt->control == ControlType::Toggle) {
						if (isBoxToggleKey(opt->key)) {
							toggleBox(opt->key);
						} else {
							Config::flip(opt->key);
							// Trigger theme refresh for theme-related options
							if (is_in(opt->key, "truecolor", "force_tty", "theme_background", "rounded_corners")) {
								theme_refresh = true;
								if (opt->key == "truecolor") Config::flip("lowcolor");
								else if (opt->key == "force_tty") Config::flip("tty_mode");
							}
						}
					}
				}
			}
			else if (is_in(key, "down", "j") or (vim_keys and key == "j")) {
				const auto& cat = cats[selected_cat];
				if (not cat.subcats.empty()) {
					int total_options = 0;
					for (const auto& sc : cat.subcats) {
						total_options += static_cast<int>(sc.options.size());
					}
					if (total_options > 0) {
						// Move to next option, wrapping subcategories
						selected_option++;
						const auto& subcat = cat.subcats[selected_subcat];
						if (selected_option >= static_cast<int>(subcat.options.size())) {
							selected_option = 0;
							selected_subcat = (selected_subcat + 1) % static_cast<int>(cat.subcats.size());
						}
					}
				}
			}
			else if (is_in(key, "up", "k") or (vim_keys and key == "k")) {
				const auto& cat = cats[selected_cat];
				if (not cat.subcats.empty()) {
					selected_option--;
					if (selected_option < 0) {
						selected_subcat = (selected_subcat - 1 + static_cast<int>(cat.subcats.size())) % static_cast<int>(cat.subcats.size());
						const auto& subcat = cat.subcats[selected_subcat];
						selected_option = max(0, static_cast<int>(subcat.options.size()) - 1);
					}
				}
			}
			else if (is_in(key, "enter", "space")) {
				const auto* opt = getCurrentOption();
				if (opt) {
					if (opt->control == ControlType::Toggle) {
						if (isBoxToggleKey(opt->key)) {
							toggleBox(opt->key);
						} else {
							Config::flip(opt->key);
							// Trigger theme refresh for theme-related options
							if (is_in(opt->key, "truecolor", "force_tty", "theme_background", "rounded_corners")) {
								theme_refresh = true;
								if (opt->key == "truecolor") Config::flip("lowcolor");
								else if (opt->key == "force_tty") Config::flip("tty_mode");
							}
						}
					}
					else if (opt->control == ControlType::Text) {
						editor = Draw::TextEdit{Config::getAsString(opt->key), false};
						editing = true;
					}
					else if (opt->control == ControlType::Radio and not opt->choices.empty()) {
						// Cycle to next option on enter
						string current = Config::getAsString(opt->key);
						int idx = 0;
						for (size_t i = 0; i < opt->choices.size(); i++) {
							if (opt->choices[i] == current) {
								idx = static_cast<int>(i);
								break;
							}
						}
						idx = (idx + 1) % static_cast<int>(opt->choices.size());
						Config::set(opt->key, opt->choices[idx]);
					}
					else if (opt->control == ControlType::Select and not opt->choices_ref.empty()) {
						// Open dropdown modal for Select
						dropdown_choices = getDynamicChoices(opt->choices_ref);
						if (not dropdown_choices.empty()) {
							string current = Config::getAsString(opt->key);
							dropdown_selection = 0;
							for (size_t i = 0; i < dropdown_choices.size(); i++) {
								if (dropdown_choices[i] == current) {
									dropdown_selection = static_cast<int>(i);
									break;
								}
							}
							dropdown_key = opt->key;  //? Track which option opened the dropdown
							dropdown_open = true;
						}
					}
				}
			}
			else if (key.starts_with("tab_")) {
				int tab_idx = key.back() - '0';
				if (tab_idx >= 0 and tab_idx < static_cast<int>(cats.size())) {
					selected_cat = tab_idx;
					selected_subcat = 0;
					selected_option = 0;
					scroll_offset = 0;
					Menu::redraw = true;
				}
			}
			else {
				retval = Menu::NoChange;
			}
		}

		// Check theme_refresh AGAIN after key handling (in case user just changed theme)
		// DON'T apply during menu - only mark for full app refresh on exit
		if (theme_refresh) {
			theme_refresh = false;
			theme_changed_pending = true;         // Mark for full refresh when menu closes
		}

		// Draw content
		if (retval == Menu::Changed or Menu::redraw) {
			out = Menu::bg;

			// Draw tab bar dynamically (so highlighting updates on category change)
			out += Mv::to(y + 2, x + 2);
			for (size_t i = 0; i < cats.size(); i++) {
				bool is_sel = (static_cast<int>(i) == selected_cat);
				string tab_text = cats[i].icon.empty() ? cats[i].name : cats[i].icon + " " + cats[i].name;
				out += (is_sel ? Theme::c("hi_fg") + Fx::b + "[" + Theme::c("title") : Theme::c("inactive_fg") + " ")
					+ tab_text
					+ (is_sel ? Theme::c("hi_fg") + "]" + Fx::ub : " ");
				out += "  ";
			}

			const auto& cat = cats[selected_cat];
			int content_y = y + 5;  // One line space after category tabs separator
			int content_x = x + 2;
			int content_width = dialog_width - 4;
			int max_lines = dialog_height - 10;  // Reserve space for help section at bottom

			// First pass: calculate total lines and find selected item's line position
			int total_lines = 0;
			int selected_line = 0;
			int selected_header_line = 0;  // Track header line for selected subcategory
			for (size_t sc_idx = 0; sc_idx < cat.subcats.size(); sc_idx++) {
				const auto& subcat = cat.subcats[sc_idx];

				// Track header line for selected subcategory
				if (static_cast<int>(sc_idx) == selected_subcat) {
					selected_header_line = total_lines;
				}

				total_lines++;  // Subcategory header
				total_lines++;  // Spacing after header

				for (size_t opt_idx = 0; opt_idx < subcat.options.size(); opt_idx++) {
					if (static_cast<int>(sc_idx) == selected_subcat and
						static_cast<int>(opt_idx) == selected_option) {
						selected_line = total_lines;
					}
					total_lines++;
				}
				total_lines++;  // Spacing between subcategories
			}

			// Adjust scroll_offset to keep selection visible (including its header)
			if (selected_header_line < scroll_offset) {
				// Scroll up to show header when selected item's header is above view
				scroll_offset = selected_header_line;
			}
			else if (selected_line >= scroll_offset + max_lines) {
				scroll_offset = selected_line - max_lines + 1;
			}
			// Clamp scroll_offset to valid range
			scroll_offset = max(0, min(scroll_offset, max(0, total_lines - max_lines)));

			bool can_scroll_up = scroll_offset > 0;
			bool can_scroll_down = (scroll_offset + max_lines) < total_lines;

			int current_line = 0;
			int visible_line = 0;

			// Render subcategories and options (with scroll offset)
			for (size_t sc_idx = 0; sc_idx < cat.subcats.size() and visible_line < max_lines; sc_idx++) {
				const auto& subcat = cat.subcats[sc_idx];

				// Subcategory header
				if (current_line >= scroll_offset and visible_line < max_lines) {
					string line_fill;
					int fill_len = max(0, content_width - static_cast<int>(subcat.name.size()) - 4);
					for (int i = 0; i < fill_len; i++) line_fill += Symbols::h_line;
					out += Mv::to(content_y + visible_line, content_x)
						+ Theme::c("title") + Fx::b + Symbols::h_line + " " + subcat.name + " "
						+ line_fill
						+ Fx::ub;
					visible_line++;
				}
				current_line++;

				// Spacing after subcategory header
				if (current_line >= scroll_offset and visible_line < max_lines) {
					visible_line++;
				}
				current_line++;

				// Options in subcategory
				for (size_t opt_idx = 0; opt_idx < subcat.options.size() and visible_line < max_lines; opt_idx++) {
					if (current_line >= scroll_offset and visible_line < max_lines) {
						const auto& opt = subcat.options[opt_idx];
						bool is_selected = (static_cast<int>(sc_idx) == selected_subcat and
											static_cast<int>(opt_idx) == selected_option);

						string line;
						if (is_selected) {
							line += Theme::c("selected_bg") + Theme::c("selected_fg");
						} else {
							line += Theme::c("main_fg");
						}

						// Option label (left side)
						string label = ljust(opt.label, 25);

						// Option value (right side)
						string value_str;
						switch (opt.control) {
							case ControlType::Toggle: {
								bool val = isBoxToggleKey(opt.key) ? isBoxEnabled(opt.key) : Config::getB(opt.key);
								value_str = drawToggle(val, is_selected, tty_mode);
								break;
							}
							case ControlType::Radio: {
								int current_idx = 0;
								string current_val = Config::getS(opt.key);
								for (size_t i = 0; i < opt.choices.size(); i++) {
									if (opt.choices[i] == current_val) {
										current_idx = static_cast<int>(i);
										break;
									}
								}
								value_str = drawRadio(opt.choices, current_idx, is_selected, tty_mode);
								break;
							}
							case ControlType::Slider: {
								int current_val = Config::getI(opt.key);
								value_str = drawSlider(opt.min_val, opt.max_val, current_val, 20, is_selected, tty_mode);
								break;
							}
							case ControlType::Select: {
								string full_val = Config::getS(opt.key);
								string display_val = full_val;
								// For color_theme, show just the theme name without path
								if (opt.key == "color_theme") {
									display_val = fs::path(display_val).stem().string();
								}
								// For clock_format, show human-readable example
								else if (opt.key == "clock_format") {
									display_val = clockFormatToReadable(full_val);
								}
								// For empty values, show placeholder
								if (display_val.empty()) {
									display_val = "(auto)";
								}
								value_str = drawSelect(display_val, 16, is_selected, dropdown_open and is_selected);
								// Add color preview for theme selection
								if (opt.key == "color_theme") {
									value_str += " " + Theme::previewColors(full_val);
								}
								break;
							}
							case ControlType::Text:
								if (editing and is_selected) {
									value_str = "[" + editor(18) + "]";
								} else {
									string text_val = Config::getS(opt.key);
									if (text_val.empty()) text_val = "(empty)";
									value_str = "[" + ljust(text_val, 18) + "]";
								}
								break;
							default:
								value_str = Config::getAsString(opt.key);
								break;
						}

						line += label + value_str;
						if (is_selected) {
							line += Fx::reset;
						}

						out += Mv::to(content_y + visible_line, content_x) + line;
						visible_line++;
					}
					current_line++;
				}

				// Spacing between subcategories
				if (current_line >= scroll_offset and visible_line < max_lines) {
					visible_line++;
				}
				current_line++;
			}

			// Separator line above help section with scroll indicators
			out += Mv::to(y + dialog_height - 5, x) + Theme::c("hi_fg") + Symbols::div_left
				+ Theme::c("div_line");
			for (int i = 0; i < dialog_width - 2; i++) out += Symbols::h_line;
			out += Theme::c("hi_fg") + Symbols::div_right;

			// Add scroll indicators on the right side of the separator
			if (can_scroll_up or can_scroll_down) {
				string scroll_indicator;
				if (can_scroll_up) scroll_indicator += "↑";
				if (can_scroll_down) scroll_indicator += "↓";
				int indicator_x = x + dialog_width - 3 - static_cast<int>(scroll_indicator.size());
				out += Mv::to(y + dialog_height - 5, indicator_x) + Theme::c("hi_fg") + scroll_indicator;
			}

			// Help text (centered) with spacing above and below
			if (selected_subcat < static_cast<int>(cat.subcats.size())) {
				const auto& subcat = cat.subcats[selected_subcat];
				if (selected_option < static_cast<int>(subcat.options.size())) {
					const auto& opt = subcat.options[selected_option];
					string help_text = opt.help;
					// Truncate if too long for dialog width
					int max_help_len = content_width;
					if (static_cast<int>(help_text.size()) > max_help_len) {
						help_text = help_text.substr(0, max_help_len - 3) + "...";
					}
					// Center the help text
					int help_x = x + (dialog_width - static_cast<int>(help_text.size())) / 2;
					out += Mv::to(y + dialog_height - 3, help_x)
						+ Theme::c("inactive_fg") + help_text;
				}
			}

			// Navigation hints at very bottom
			out += Mv::to(y + dialog_height - 1, x + 2)
				+ Theme::c("hi_fg") + "Tab" + Theme::c("main_fg") + ":Category  "
				+ Theme::c("hi_fg") + "↑↓" + Theme::c("main_fg") + ":Navigate  "
				+ Theme::c("hi_fg") + "Enter" + Theme::c("main_fg") + ":Toggle/Edit  "
				+ Theme::c("hi_fg") + "Esc" + Theme::c("main_fg") + ":Close";

			// Draw dropdown modal if open
			if (dropdown_open and not dropdown_choices.empty()) {
				const int dropdown_max_height = min(15, static_cast<int>(dropdown_choices.size()) + 2);
				const bool is_theme_dropdown = (dropdown_key == "color_theme");
				//? Wider dropdown for themes: name (25) + preview (23) + padding (6) = 54
				const int dropdown_width = is_theme_dropdown ? 58 : 40;
				const int dropdown_x = x + (dialog_width - dropdown_width) / 2;
				const int dropdown_y = y + (dialog_height - dropdown_max_height) / 2;

				// Draw dropdown background box
				out += Draw::createBox(dropdown_x, dropdown_y, dropdown_width, dropdown_max_height,
					Theme::c("hi_fg"), true, is_theme_dropdown ? "Select Theme" : "Select Option");

				// Calculate visible range for scrolling - keep cursor centered
				const int visible_items = dropdown_max_height - 2;
				const int total_items = static_cast<int>(dropdown_choices.size());
				const int half_visible = visible_items / 2;
				int scroll_start = 0;

				// Center-focused scrolling: selection stays in middle of visible area
				if (dropdown_selection > half_visible) {
					scroll_start = dropdown_selection - half_visible;
				}
				// Don't scroll past the end - cursor moves down when near end of list
				if (scroll_start + visible_items > total_items) {
					scroll_start = std::max(0, total_items - visible_items);
				}

				// Draw dropdown items
				const bool is_clock_dropdown = (dropdown_key == "clock_format");
				for (int i = 0; i < visible_items and (scroll_start + i) < static_cast<int>(dropdown_choices.size()); i++) {
					int item_idx = scroll_start + i;
					string item_full_path = dropdown_choices[item_idx];  //? Keep full path for theme preview
					string item_display = item_full_path;

					// For color_theme, show just the theme name
					if (item_display.find('/') != string::npos) {
						item_display = fs::path(item_display).stem().string();
					}
					// For clock_format, show human-readable examples
					else if (is_clock_dropdown) {
						item_display = clockFormatToReadable(item_full_path);
					}

					// Empty value shows as "(auto)" or "(none)"
					if (item_display.empty()) {
						item_display = "(auto)";
					}

					// For theme dropdown, reserve space for color preview (8×2 chars + 7 spaces + 1 leading = 24)
					const int preview_width = is_theme_dropdown ? 24 : 0;
					const int max_name_width = dropdown_width - 6 - preview_width;

					// Truncate if too long
					if (static_cast<int>(item_display.size()) > max_name_width) {
						item_display = item_display.substr(0, max_name_width - 3) + "...";
					}

					bool is_selected_item = (item_idx == dropdown_selection);
					out += Mv::to(dropdown_y + 1 + i, dropdown_x + 2);

					if (is_selected_item) {
						out += Theme::c("hi_fg") + Fx::b + "> " + ljust(item_display, max_name_width);
					} else {
						out += Theme::c("main_fg") + "  " + ljust(item_display, max_name_width);
					}

					// Add color preview for theme dropdown
					if (is_theme_dropdown) {
						out += " " + Theme::previewColors(item_full_path);
					}

					if (is_selected_item) {
						out += Fx::ub;
					}
				}

				// Show scroll indicators if needed
				if (scroll_start > 0) {
					out += Mv::to(dropdown_y, dropdown_x + dropdown_width - 4) + Theme::c("hi_fg") + "↑";
				}
				if (scroll_start + visible_items < static_cast<int>(dropdown_choices.size())) {
					out += Mv::to(dropdown_y + dropdown_max_height - 1, dropdown_x + dropdown_width - 4) + Theme::c("hi_fg") + "↓";
				}
			}

			out += Fx::reset;
		}

		return (Menu::redraw ? Menu::Changed : retval);
	}

	//? ==================== Preset Editor ====================

	int presetEditor(const string& key, int preset_idx) {
		// Static state for the editor
		static PresetDef current_preset;
		static int selected_field = 0;
		static bool initialized = false;
		static Draw::TextEdit name_editor;
		static bool editing_name = false;

		auto tty_mode = Config::getB("tty_mode");

		// Initialize preset from config on first call
		if (not initialized) {
			auto presets = getPresets();
			if (preset_idx >= 0 and preset_idx < static_cast<int>(presets.size())) {
				current_preset = presets[preset_idx];
			} else {
				// New preset with defaults
				current_preset = PresetDef{};
				current_preset.name = "New Preset";
			}
			initialized = true;
			Menu::redraw = true;
		}

		// Dialog dimensions
		const int dialog_width = 70;
		const int dialog_height = 22;
		const int x = (Term::width - dialog_width) / 2;
		const int y = (Term::height - dialog_height) / 2;

		auto& out = Global::overlay;
		int retval = Menu::Changed;

		// Field definitions for navigation
		enum Fields { Name, CPU, GPU, PWR, MEM_Layout, MEM_Disk, NET, PROC, GraphSymbol, Save, Cancel };
		const int total_fields = 11;

		// Key handling
		if (editing_name) {
			if (is_in(key, "escape", "enter")) {
				if (key == "enter") {
					current_preset.name = name_editor.text;
				}
				name_editor.clear();
				editing_name = false;
			}
			else {
				name_editor.command(key);
			}
		}
		else {
			if (is_in(key, "escape", "q")) {
				initialized = false;
				return Menu::Closed;
			}
			else if (is_in(key, "down", "j", "tab")) {
				selected_field = (selected_field + 1) % total_fields;
			}
			else if (is_in(key, "up", "k", "shift_tab")) {
				selected_field = (selected_field - 1 + total_fields) % total_fields;
			}
			else if (is_in(key, "enter", "space")) {
				switch (selected_field) {
					case Name:
						name_editor = Draw::TextEdit{current_preset.name, false};
						editing_name = true;
						break;
					case CPU: current_preset.cpu_enabled = not current_preset.cpu_enabled; break;
					case GPU: current_preset.gpu_enabled = not current_preset.gpu_enabled; break;
					case PWR: current_preset.pwr_enabled = not current_preset.pwr_enabled; break;
					case MEM_Layout:
						current_preset.mem_layout = static_cast<MemLayout>(
							(static_cast<int>(current_preset.mem_layout) + 1) % 3);
						break;
					case MEM_Disk:
						current_preset.disk_mode = static_cast<DiskMode>(
							(static_cast<int>(current_preset.disk_mode) + 1) % 3);
						break;
					case NET:
						current_preset.net_layout = static_cast<NetLayout>(
							(static_cast<int>(current_preset.net_layout) + 1) % 3);
						break;
					case PROC:
						current_preset.proc_layout = static_cast<ProcLayout>(
							(static_cast<int>(current_preset.proc_layout) + 1) % 3);
						break;
					case GraphSymbol: {
						vector<string> symbols = {"default", "braille", "block", "tty"};
						int idx = 0;
						for (size_t i = 0; i < symbols.size(); i++) {
							if (symbols[i] == current_preset.graph_symbol) {
								idx = static_cast<int>(i);
								break;
							}
						}
						idx = (idx + 1) % static_cast<int>(symbols.size());
						current_preset.graph_symbol = symbols[idx];
						break;
					}
					case Save: {
						// Save the preset
						auto presets = getPresets();
						if (preset_idx >= 0 and preset_idx < static_cast<int>(presets.size())) {
							presets[preset_idx] = current_preset;
						} else {
							presets.push_back(current_preset);
						}
						savePresets(presets);
						initialized = false;
						return Menu::Closed;
					}
					case Cancel:
						initialized = false;
						return Menu::Closed;
				}
			}
			else if (is_in(key, "left", "h")) {
				// Cycle backward for applicable fields
				switch (selected_field) {
					case MEM_Layout:
						current_preset.mem_layout = static_cast<MemLayout>(
							(static_cast<int>(current_preset.mem_layout) + 2) % 3);
						break;
					case MEM_Disk:
						current_preset.disk_mode = static_cast<DiskMode>(
							(static_cast<int>(current_preset.disk_mode) + 2) % 3);
						break;
					case NET:
						current_preset.net_layout = static_cast<NetLayout>(
							(static_cast<int>(current_preset.net_layout) + 2) % 3);
						break;
					case PROC:
						current_preset.proc_layout = static_cast<ProcLayout>(
							(static_cast<int>(current_preset.proc_layout) + 2) % 3);
						break;
				}
			}
			else if (is_in(key, "right", "l")) {
				// Cycle forward for applicable fields (same as enter/space)
				switch (selected_field) {
					case CPU: current_preset.cpu_enabled = not current_preset.cpu_enabled; break;
					case GPU: current_preset.gpu_enabled = not current_preset.gpu_enabled; break;
					case PWR: current_preset.pwr_enabled = not current_preset.pwr_enabled; break;
					case MEM_Layout:
						current_preset.mem_layout = static_cast<MemLayout>(
							(static_cast<int>(current_preset.mem_layout) + 1) % 3);
						break;
					case MEM_Disk:
						current_preset.disk_mode = static_cast<DiskMode>(
							(static_cast<int>(current_preset.disk_mode) + 1) % 3);
						break;
					case NET:
						current_preset.net_layout = static_cast<NetLayout>(
							(static_cast<int>(current_preset.net_layout) + 1) % 3);
						break;
					case PROC:
						current_preset.proc_layout = static_cast<ProcLayout>(
							(static_cast<int>(current_preset.proc_layout) + 1) % 3);
						break;
				}
			}
			else {
				retval = Menu::NoChange;
			}
		}

		// Draw the dialog
		if (retval == Menu::Changed or Menu::redraw) {
			// Create dialog box
			out = Draw::createBox(x, y, dialog_width, dialog_height, Theme::c("hi_fg"), true, "Preset Editor");

			int row = y + 2;
			const int label_x = x + 3;
			const int value_x = x + 20;

			// Helper for rendering a field
			auto renderField = [&](int field_id, const string& label, const string& value) {
				bool is_sel = (selected_field == field_id);
				out += Mv::to(row, label_x) + Theme::c("main_fg") + label + ":";
				out += Mv::to(row, value_x);
				if (is_sel) out += Theme::c("selected_bg") + Theme::c("selected_fg");
				out += value;
				if (is_sel) out += Fx::reset;
				row++;
			};

			// Name field
			if (editing_name) {
				out += Mv::to(row, label_x) + Theme::c("main_fg") + "Name:";
				out += Mv::to(row, value_x) + Theme::c("selected_bg") + name_editor(20) + Fx::reset;
			} else {
				renderField(Name, "Name", current_preset.name);
			}
			row++;

			// Panel toggles
			out += Mv::to(row, label_x) + Theme::c("title") + Fx::b + "─ Panels " + string(20, '-') + Fx::ub;
			row++;
			renderField(CPU, "CPU", drawToggle(current_preset.cpu_enabled, selected_field == CPU, tty_mode));
			renderField(GPU, "GPU", drawToggle(current_preset.gpu_enabled, selected_field == GPU, tty_mode));
			renderField(PWR, "Power", drawToggle(current_preset.pwr_enabled, selected_field == PWR, tty_mode));
			row++;

			// Memory layout
			vector<string> mem_opts = {"Hidden", "Horizontal", "Vertical"};
			renderField(MEM_Layout, "Memory", drawRadio(mem_opts, static_cast<int>(current_preset.mem_layout), selected_field == MEM_Layout, tty_mode));

			// Disk mode
			vector<string> disk_opts = {"Hidden", "Bar/Meter", "Graph"};
			renderField(MEM_Disk, "  Disk Mode", drawRadio(disk_opts, static_cast<int>(current_preset.disk_mode), selected_field == MEM_Disk, tty_mode));
			row++;

			// Network layout
			vector<string> net_opts = {"Hidden", "Horizontal", "Vertical"};
			renderField(NET, "Network", drawRadio(net_opts, static_cast<int>(current_preset.net_layout), selected_field == NET, tty_mode));

			// Process layout
			vector<string> proc_opts = {"Hidden", "Horizontal", "Vertical"};
			renderField(PROC, "Processes", drawRadio(proc_opts, static_cast<int>(current_preset.proc_layout), selected_field == PROC, tty_mode));
			row++;

			// Graph symbol
			vector<string> sym_opts = {"default", "braille", "block", "tty"};
			int sym_idx = 0;
			for (size_t i = 0; i < sym_opts.size(); i++) {
				if (sym_opts[i] == current_preset.graph_symbol) {
					sym_idx = static_cast<int>(i);
					break;
				}
			}
			renderField(GraphSymbol, "Graph Symbol", drawRadio(sym_opts, sym_idx, selected_field == GraphSymbol, tty_mode));
			row += 2;

			// Buttons
			out += Mv::to(row, label_x);
			bool save_sel = (selected_field == Save);
			bool cancel_sel = (selected_field == Cancel);
			out += (save_sel ? Theme::c("selected_bg") + Theme::c("selected_fg") : Theme::c("hi_fg"))
				+ "[ Save ]" + Fx::reset + "   "
				+ (cancel_sel ? Theme::c("selected_bg") + Theme::c("selected_fg") : Theme::c("inactive_fg"))
				+ "[ Cancel ]" + Fx::reset;

			// Draw preview on the right side
			const int preview_x = x + 40;
			const int preview_y = y + 3;
			const int preview_w = 26;
			const int preview_h = 15;

			out += Mv::to(preview_y - 1, preview_x) + Theme::c("title") + "Preview:";
			out += drawPresetPreview(current_preset, preview_w, preview_h);

			// Position the preview
			string preview = drawPresetPreview(current_preset, preview_w, preview_h);
			// Split preview into lines and position them
			int py = preview_y;
			string line;
			for (char c : preview) {
				if (c == '\n') {
					out += Mv::to(py++, preview_x) + line;
					line.clear();
				} else {
					line += c;
				}
			}
			if (not line.empty()) {
				out += Mv::to(py, preview_x) + line;
			}
		}

		return (Menu::redraw ? Menu::Changed : retval);
	}

} // namespace MenuV2

//? Wrapper function in Menu namespace to call MenuV2::optionsMenuV2
namespace Menu {
	int optionsMenuV2_wrapper(const string& key) {
		return MenuV2::optionsMenuV2(key);
	}
}
