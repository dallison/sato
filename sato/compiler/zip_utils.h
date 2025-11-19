// This is heavily based on Phaser (https://github.com/dallison/phaser) and
// Neutron (https://github.com/dallison/neutron).
// Copyright (C) 2025 David Allison.  All Rights Reserved.

#pragma once

#include "absl/status/status.h"
#include "zip.h"
#include <string>

namespace sato {

absl::Status AddFileToZip(zip_t *zip, const std::string &full_message_name,
                          const std::string &content);

} // namespace sato