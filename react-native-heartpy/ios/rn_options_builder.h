// Central options validation and (future) JS builders for RN bridges
#pragma once

#include <string>
#include "heartpy_core.h"
#include <jsi/jsi.h>

// Returns false if options are invalid. On failure, sets err_code (stable code)
// and err_msg (short reason). On success, err_code/msg are untouched.
// This function performs validation only (no mutation). Clamping/snap logic is
// left to call sites or underlying core behavior. This preserves current P0 behavior.
extern "C" bool hp_validate_options(double fs,
                                     const heartpy::Options& opt,
                                     const char** err_code,
                                     std::string* err_msg);

// Build Options from a JSI object (subset used by streaming). On error, sets code/msg.
heartpy::Options hp_build_options_from_jsi(facebook::jsi::Runtime& rt,
                                           const facebook::jsi::Object& opts,
                                           const char** err_code,
                                           std::string* err_msg);
