// This is heavily based on Phaser (https://github.com/dallison/phaser) and
// Neutron (https://github.com/dallison/neutron).
// Copyright (C) 2025 David Allison.  All Rights Reserved.

#pragma once

namespace sato {

class Message {
public:
  bool IsPopulated() const { return populated_; }
  void SetPopulated(bool populated) { populated_ = populated; }

private:
  bool populated_ = false;
};
}
