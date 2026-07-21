#pragma once

#include "settings.hpp"

#include <string>
#include <string_view>

namespace tetris::video {

enum class FaceButton {
    south,
    east,
    west,
    north,
};

settings::ControllerLabels detect_controller_labels();
settings::ControllerLabels resolve_controller_labels(settings::ControllerLabels selected,
                                                       settings::ControllerLabels detected);
const char* face_button_label(settings::ControllerLabels style, FaceButton button);
std::string controller_prompt(settings::ControllerLabels style, FaceButton button,
                              std::string_view action);

} // namespace tetris::video
