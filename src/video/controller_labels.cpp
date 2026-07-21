#include "video/controller_labels.hpp"

#include <SDL3/SDL.h>

namespace tetris::video {

settings::ControllerLabels detect_controller_labels() {
    settings::ControllerLabels detected = settings::ControllerLabels::nintendo;
    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
    for (int index = 0; index < count; ++index) {
        SDL_Gamepad* gamepad = SDL_GetGamepadFromID(gamepads[index]);
        if (gamepad == nullptr)
            continue;
        const SDL_GamepadType type = SDL_GetGamepadType(gamepad);
        bool recognized = true;
        switch (type) {
        case SDL_GAMEPAD_TYPE_XBOX360:
        case SDL_GAMEPAD_TYPE_XBOXONE:
            detected = settings::ControllerLabels::xbox;
            break;
        case SDL_GAMEPAD_TYPE_PS3:
        case SDL_GAMEPAD_TYPE_PS4:
        case SDL_GAMEPAD_TYPE_PS5:
            detected = settings::ControllerLabels::playstation;
            break;
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        case SDL_GAMEPAD_TYPE_GAMECUBE:
            detected = settings::ControllerLabels::nintendo;
            break;
        case SDL_GAMEPAD_TYPE_UNKNOWN:
        case SDL_GAMEPAD_TYPE_STANDARD:
        case SDL_GAMEPAD_TYPE_COUNT:
            recognized = false;
            break;
        }
        if (recognized)
            break;
    }
    SDL_free(gamepads);
    return detected;
}

settings::ControllerLabels resolve_controller_labels(settings::ControllerLabels selected,
                                                       settings::ControllerLabels detected) {
    return selected == settings::ControllerLabels::automatic ? detected : selected;
}

const char* face_button_label(settings::ControllerLabels style, FaceButton button) {
    if (style == settings::ControllerLabels::automatic)
        style = settings::ControllerLabels::nintendo;
    if (style == settings::ControllerLabels::xbox) {
        switch (button) {
        case FaceButton::south: return "A";
        case FaceButton::east: return "B";
        case FaceButton::west: return "X";
        case FaceButton::north: return "Y";
        }
    }
    if (style == settings::ControllerLabels::playstation) {
        switch (button) {
        case FaceButton::south: return "CROSS";
        case FaceButton::east: return "CIRCLE";
        case FaceButton::west: return "SQUARE";
        case FaceButton::north: return "TRIANGLE";
        }
    }
    switch (button) {
    case FaceButton::south: return "B";
    case FaceButton::east: return "A";
    case FaceButton::west: return "Y";
    case FaceButton::north: return "X";
    }
    return "";
}

std::string controller_prompt(settings::ControllerLabels style, FaceButton button,
                              std::string_view action) {
    std::string prompt = face_button_label(style, button);
    prompt += ' ';
    prompt += action;
    return prompt;
}

} // namespace tetris::video
