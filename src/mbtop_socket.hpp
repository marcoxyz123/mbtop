/* Copyright 2024 mbtop contributors

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
#include <functional>
#include <string>
#include <thread>

namespace Socket {

	//? Socket path: ~/.config/mbtop/mbtop.sock
	extern std::string socket_path;

	//? Is the socket server running?
	extern std::atomic<bool> running;

	//? Flag to signal main loop that a command needs processing
	extern std::atomic<bool> command_pending;

	//? Pending command type
	enum class CommandType {
		None,
		ShowLogs,           //? Show logs panel (beside mode)
		ShowLogsBelow,      //? Show logs panel (below mode)
		HideLogs,           //? Hide logs panel
		SelectProcess,      //? Select a process in the list
		ActivateProcessLog, //? Select process AND show its log
		SetLogFilter,       //? Set log filter text
		SetLogSource,       //? Set log source (system/application)
		GetState,           //? Query current state (returns JSON)
	};

	//? Current pending command
	extern std::atomic<CommandType> pending_command;

	//? Command parameters (set before command_pending = true)
	struct CommandParams {
		std::string process_name;
		std::string process_command;
		std::string filter_text;
		std::string source_text;  //? "system" or "application"
		int pid = 0;
	};
	extern CommandParams params;

	//? Response for GetState command (set by main thread)
	extern std::string state_response;
	extern std::atomic<bool> response_ready;

	//? Initialize and start the socket server
	void start();

	//? Stop the socket server and clean up
	void stop();

	//? Process pending command (called from main loop)
	//? Returns true if a command was processed
	bool process_pending_command();

	//? Get the socket path
	std::string get_socket_path();

}
