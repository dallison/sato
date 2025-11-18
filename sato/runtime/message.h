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
