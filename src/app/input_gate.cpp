#include "input_gate.hpp"

namespace noto3d::app {

bool viewportGateOpen(bool viewportHovered,
                      bool anyItemHovered,
                      bool anyItemActive,
                      bool globalModalCapture) noexcept {
    return viewportHovered && !anyItemHovered && !anyItemActive && !globalModalCapture;
}

}  // namespace noto3d::app
