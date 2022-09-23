#pragma once
namespace uinhibit {
  class InhibitRequestUnsupportedTypeException : std::exception {};
  class InhibitNoResponseException : std::exception {};
  class InhibitNotFoundException : std::exception {};
}
