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

#pragma once

#include <atomic>
#include <bitset>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <functional>

#include "mbtop_input.hpp"

using std::atomic;
using std::bitset;
using std::string;
using std::vector;
using std::array;

namespace Menu {

	extern atomic<bool> active;
	extern string output;
	extern int signalToSend;
	extern bool redraw;

	//? line, col, height, width
	extern std::unordered_map<string, Input::Mouse_loc> mouse_mappings;

	//* Creates a message box centered on screen
	//? Height of box is determined by size of content vector
	//? Boxtypes: 0 = OK button | 1 = YES and NO with YES selected | 2 = Same as 1 but with NO selected
	//? Strings in content vector is not checked for box width overflow
	class msgBox {
		string box_contents, button_left, button_right;
		int height{};
		int width{};
		int boxtype{};
		int selected{};
		int x{};
		int y{};
	public:
		enum BoxTypes { OK, YES_NO, NO_YES };
		enum msgReturn {
			Invalid,
			Ok_Yes,
			No_Esc,
			Select
		};
		msgBox();
		msgBox(int width, int boxtype, const vector<string>& content, std::string_view title);

		//? Draw and return box as a string
		string operator()();

		//? Process input and returns value from enum Ret
		int input(const string& key);

		//? Clears content vector and private strings
		void clear();
        int getX() const { return x; }
        int getY() const { return y; }
	};

	extern bitset<16> menuMask;

	//* Enum for functions in vector menuFuncs
	enum Menus {
		SizeError,
		SignalChoose,
		SignalSend,
		SignalReturn,
		Options,
		Help,
	    Renice,
		VramAlloc,
		Main
	};

	//* Handles redirection of input for menu functions and handles return codes
	void process(const std::string_view key = "");

	//* Show a menu from enum Menu::Menus
	void show(int menu, int signal=-1);

}

//? New Menu V2 - Redesigned settings dialog with proper UI controls
namespace MenuV2 {

	//? Control types for option rendering
	enum class ControlType {
		Toggle,      // Boolean on/off switch [●]/[○]
		Radio,       // Fixed choice from 2-5 options ◉/○
		Select,      // Dropdown from dynamic list
		Slider,      // Numeric range with visual slider
		Text,        // Free text input
		MultiCheck,  // Multi-select checkboxes ☑/☐ (space-separated values)
		Special      // Custom handling (presets)
	};

	//? Single option definition
	struct OptionDef {
		string key;                  // Config key name
		string label;                // Display label
		string help;                 // Single-line help text
		ControlType control;         // UI control type
		vector<string> choices;      // For Radio (static choices)
		string choices_ref;          // For Select (reference to dynamic list)
		int min_val = 0;             // For Slider
		int max_val = 100;           // For Slider
		int step = 1;                // For Slider
	};

	//? Subcategory grouping options
	struct SubCategory {
		string name;                 // Subcategory header name
		vector<OptionDef> options;   // Options in this subcategory
	};

	//? Main category (tab)
	struct Category {
		string name;                 // "System", "Appearance", etc.
		string icon;                 // Display icon/emoji
		vector<SubCategory> subcats; // Subcategories within
		bool has_subtabs = false;    // For Panels category with CPU/GPU/etc tabs
		vector<string> subtab_names; // Names of sub-tabs if has_subtabs
	};

	//? ==================== Preset Layout System ====================
	//? Clean model with 13 distinct layout combinations
	//?
	//? MEM Type: Controls internal bar orientation (H=horizontal bars, V=vertical bars)
	//?   - If Horizontal: Graph forced to Bar, Disk forced to hidden
	//?   - If Vertical: Graph can be Bar/Meter, Disk can be shown
	//?
	//? NET Position (when MEM visible): Left=under MEM, Right=beside MEM
	//? NET Position (when MEM hidden): Wide only
	//?
	//? PROC Position: Right=compact on right, Wide=fills width at bottom

	//? Memory display type - controls internal bar orientation
	enum class MemType { Horizontal, Vertical };

	//? Network position relative to MEM
	enum class NetPosition { Left, Right, Wide };

	//? Process position
	enum class ProcPosition { Right, Wide };

	//? Preset definition structure - clean model
	struct PresetDef {
		string name = "";

		//? Top panels (always full width, stacked)
		bool cpu_enabled = true;
		bool cpu_bottom = false;          // CPU at bottom of screen instead of top
		bool gpu_enabled = false;
		bool gpu_bottom = false;          // GPU at bottom of screen instead of top
		bool pwr_enabled = false;
		bool pwr_bottom = false;          // PWR at bottom of screen instead of top

		//? MEM panel
		bool mem_enabled = true;
		MemType mem_type = MemType::Vertical;     // H or V - controls bar orientation
		bool mem_graph_meter = false;             // false=Bar, true=Meter (only editable if type=V)
		bool show_disk = true;                    // Show disk section (only editable if type=V)

		//? NET panel
		bool net_enabled = true;
		NetPosition net_position = NetPosition::Right;  // Left/Right/Wide

		//? PROC panel
		bool proc_enabled = true;
		ProcPosition proc_position = ProcPosition::Wide;  // Right/Wide

		//? Graph appearance
		string graph_symbol = "default";

		//? Convert to config string format for shown_boxes
		string toConfigString() const;

		//? Parse from config string format
		static PresetDef fromConfigString(const string& config, const string& name);

		//? Enforce constraints after any change (call after modifying fields)
		void enforceConstraints();
	};

	//? Get all category definitions
	const vector<Category>& getCategories();

	//? Get presets from config
	vector<PresetDef> getPresets();

	//? Save presets to config
	void savePresets(const vector<PresetDef>& presets);

	//? UI Component Renderers - return strings for drawing
	string drawToggle(bool value, bool selected, bool tty_mode = false);
	//? Draw radio buttons with focus tracking: focus_idx specifies which option has keyboard focus (-1 = no focus)
	string drawRadio(const vector<string>& options, int current_idx, int focus_idx, bool tty_mode = false);
	//? Overload for backward compatibility (selected=true highlights current value)
	string drawRadio(const vector<string>& options, int current_idx, bool selected, bool tty_mode = false);
	string drawSlider(int min_val, int max_val, int current, int width, bool selected, bool tty_mode = false);
	string drawSelect(const string& value, int width, bool selected, bool open = false);

	//? Preset preview renderer - ASCII art layout
	string drawPresetPreview(const PresetDef& preset, int width, int height);

	//? Main options menu V2 function
	int optionsMenuV2(const string& key);

	//? Preset editor dialog - parent bounds for responsive layout
	int presetEditor(const string& key, int preset_idx, int parent_x, int parent_y, int parent_w, int parent_h);

	//? Flag to track if theme changed during menu session (for full refresh on exit)
	extern bool theme_changed_pending;

	//? Flag to track if box layout changed during menu session (for full refresh on exit)
	extern bool boxes_changed_pending;

}
