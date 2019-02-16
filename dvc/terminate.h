#pragma once

namespace dvc {

void terminate_handler();

void log_stacktrace();

void log_current_exception();

void install_terminate_handler();

void install_segfault_handler();

}  // namespace dvc
