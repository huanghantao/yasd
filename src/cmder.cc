/**
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "include/common.h"
#include "include/context.h"
#include "include/cmder.h"
#include "include/util.h"
#include "include/global.h"

#include "php/main/php.h"
#include "php/Zend/zend_builtin_functions.h"

BEGIN_EXTERN_C()
#include "php/ext/standard/php_var.h"
END_EXTERN_C()

#include <iostream>

yasd::Cmder *cmder;

namespace yasd {

Cmder::Cmder() {
    register_cmd_handler();
}

Cmder::~Cmder() {}

std::string Cmder::get_next_cmd() {
    std::string tmp;

    std::cout << "> ";
    getline(std::cin, tmp);
    if (tmp == "") {
        return last_cmd;
    }
    last_cmd = tmp;
    return last_cmd;
}

int Cmder::parse_run_cmd() {
    global->is_running = true;

    return NEXT_OPLINE;
}

int Cmder::parse_breakpoint_cmd() {
    int lineno;
    std::string filename;

    auto exploded_cmd = yasd::Util::explode(last_cmd, ' ');

    // breakpoint in current file with lineno
    if (exploded_cmd.size() == 2) {
        filename = yasd::Util::get_executed_filename();
        if (filename == "") {
            filename = global->entry_file;
        }
        lineno = atoi(exploded_cmd[1].c_str());
    } else if (exploded_cmd.size() == 3) {
        filename = exploded_cmd[1];
        lineno = atoi(exploded_cmd[2].c_str());
    } else {
        yasd::Util::printf_info(YASD_ECHO_RED, "use set breakpoint cmd error!");
        return RECV_CMD_AGAIN;
    }

    auto iter = global->breakpoints->find(filename);

    if (iter != global->breakpoints->end()) {
        iter->second.insert(lineno);
    } else {
        std::set<int> lineno_set;
        lineno_set.insert(lineno);
        global->breakpoints->insert(std::make_pair(filename, lineno_set));
    }

    yasd::Util::printf_info(yasd::Color::YASD_ECHO_GREEN, "set breakpoint at %s:%d", filename.c_str(), lineno);

    return RECV_CMD_AGAIN;
}

int Cmder::parse_delete_breakpoint_cmd() {
    int lineno;
    std::string filename;

    auto exploded_cmd = yasd::Util::explode(last_cmd, ' ');

    if (exploded_cmd.size() == 2) {
        filename = yasd::Util::get_executed_filename();
        if (filename == "") {
            filename = global->entry_file;
        }
        lineno = atoi(exploded_cmd[1].c_str());
    } else if (exploded_cmd.size() == 3) {
        filename = exploded_cmd[1];
        lineno = atoi(exploded_cmd[2].c_str());
    } else {
        yasd::Util::printf_info(YASD_ECHO_RED, "use delete breakpoint cmd error!");
        return RECV_CMD_AGAIN;
    }

    auto iter = global->breakpoints->find(filename);

    if (iter != global->breakpoints->end()) {
        iter->second.erase(lineno);
        if (iter->second.empty()) {
            global->breakpoints->erase(iter->first);
        }
        yasd::Util::printf_info(yasd::Color::YASD_ECHO_GREEN, "delete breakpoint at %s:%d", filename.c_str(), lineno);
    } else {
        yasd::Util::printf_info(YASD_ECHO_RED, "breakpoint at %s:%d is not existed!", filename.c_str(), lineno);
    }

    return RECV_CMD_AGAIN;
}

int Cmder::parse_info_cmd() {
    if (global->breakpoints->empty()) {
        yasd::Util::printf_info(YASD_ECHO_RED, "no found breakpoints!");
    }
    for (auto i = global->breakpoints->begin(); i != global->breakpoints->end(); i++) {
        std::cout << "filename: " << i->first << std::endl;
        for (auto j = i->second.begin(); j != i->second.end(); j++) {
            std::cout << *j << ", ";
        }
        std::cout << std::endl;
    }

    return RECV_CMD_AGAIN;
}

int Cmder::parse_step_cmd() {
    global->do_step = true;

    return NEXT_OPLINE;
}

int Cmder::parse_level_cmd() {
    yasd::Context *context = global->get_current_context();

    std::cout << "in coroutine: " << context->cid << ", level: " << context->level
              << ", next level: " << context->next_level << std::endl;

    return RECV_CMD_AGAIN;
}

int Cmder::parse_backtrace_cmd() {
    yasd::Context *context = global->get_current_context();

    yasd::Util::printf_info(
        YASD_ECHO_GREEN, "%s:%d", yasd::Util::get_executed_filename(), yasd::Util::get_executed_file_lineno());

    for (auto iter = context->strace->rbegin(); iter != context->strace->rend(); ++iter) {
        yasd::Util::printf_info(YASD_ECHO_GREEN, "%s:%d", (*iter)->filename.c_str(), (*iter)->lineno);
    }

    return RECV_CMD_AGAIN;
}

int Cmder::parse_next_cmd() {
    yasd::Context *context = global->get_current_context();
    zend_execute_data *frame = EG(current_execute_data);

    int func_line_end = frame->func->op_array.line_end;

    if (frame->opline->lineno == func_line_end) {
        global->do_step = true;
    } else {
        context->next_level = context->level;
        global->do_next = true;
    }

    return NEXT_OPLINE;
}

int Cmder::parse_continue_cmd() {
    return NEXT_OPLINE;
}

int Cmder::parse_quit_cmd() {
    yasd::Util::printf_info(YASD_ECHO_RED, "quit!");
    exit(255);

    return FAILED;
}

int Cmder::parse_print_cmd() {
    yasd::Util::print_var(last_cmd.c_str() + 2, last_cmd.length() - 2);
    global->do_next = true;

    return RECV_CMD_AGAIN;
}

int Cmder::parse_finish_cmd() {
    yasd::Context *context = global->get_current_context();
    // zend_execute_data *frame = EG(current_execute_data);

    // int func_line_end = frame->func->op_array.line_end;

    context->next_level = context->level - 1;
    global->do_finish = true;

    return NEXT_OPLINE;
}

bool Cmder::is_disable_cmd(std::string cmd) {
    // the command in the condition is allowed to execute in non-run state
    if (get_full_name(cmd) != "run" && get_full_name(cmd) != "b" && get_full_name(cmd) != "quit" &&
        get_full_name(cmd) != "info" && get_full_name(cmd) != "delete") {
        return true;
    }

    return false;
}

std::string Cmder::get_full_name(std::string sub_cmd) {
    for (auto &&kv : handlers) {
        if (yasd::Util::is_match(sub_cmd, kv.first)) {
            return kv.first;
        }
    }
    return "unknown cmd";
}

void Cmder::show_welcome_info() {
    yasd::Util::printf_info(YASD_ECHO_GREEN, "[Welcome to yasd, the Swoole debugger]");
    yasd::Util::printf_info(YASD_ECHO_GREEN, "[You can set breakpoint now]");
}

int Cmder::execute_cmd() {
    // yasd::Context *context = global->get_current_context();

    auto exploded_cmd = yasd::Util::explode(last_cmd, ' ');

    if (!global->is_running) {
        if (is_disable_cmd(exploded_cmd[0])) {
            yasd::Util::printf_info(YASD_ECHO_RED, "program is not running!");
            return RECV_CMD_AGAIN;
        }
    }

    auto handler = find_cmd_handler(exploded_cmd[0]);
    if (!handler) {
        return FAILED;
    }

    return handler();
}

void Cmder::register_cmd_handler() {
    handlers.push_back(std::make_pair("run", std::bind(&Cmder::parse_run_cmd, this)));
    handlers.push_back(std::make_pair("b", std::bind(&Cmder::parse_breakpoint_cmd, this)));
    handlers.push_back(std::make_pair("bt", std::bind(&Cmder::parse_backtrace_cmd, this)));
    handlers.push_back(std::make_pair("delete", std::bind(&Cmder::parse_delete_breakpoint_cmd, this)));
    handlers.push_back(std::make_pair("info", std::bind(&Cmder::parse_info_cmd, this)));
    handlers.push_back(std::make_pair("step", std::bind(&Cmder::parse_step_cmd, this)));
    handlers.push_back(std::make_pair("level", std::bind(&Cmder::parse_level_cmd, this)));
    handlers.push_back(std::make_pair("next", std::bind(&Cmder::parse_next_cmd, this)));
    handlers.push_back(std::make_pair("continue", std::bind(&Cmder::parse_continue_cmd, this)));
    handlers.push_back(std::make_pair("quit", std::bind(&Cmder::parse_quit_cmd, this)));
    handlers.push_back(std::make_pair("print", std::bind(&Cmder::parse_print_cmd, this)));
    handlers.push_back(std::make_pair("finish", std::bind(&Cmder::parse_finish_cmd, this)));
}

std::function<int()> Cmder::find_cmd_handler(std::string cmd) {
    for (auto &&kv : handlers) {
        if (kv.first == get_full_name(cmd)) {
            return kv.second;
        }
    }
    return nullptr;
}
}  // namespace yasd
