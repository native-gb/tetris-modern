#include "controls_menu.hpp"

#include "controls.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <array>

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

namespace tetris {
namespace {

struct ActionRow {
    Action action;
    const char* label;
};

constexpr std::array actions = {
    ActionRow{Action::left, "Left"},
    ActionRow{Action::right, "Right"},
    ActionRow{Action::up, "Up / menu up"},
    ActionRow{Action::down, "Down / soft drop"},
    ActionRow{Action::confirm, "Confirm"},
    ActionRow{Action::back, "Back"},
    ActionRow{Action::rotate_counterclockwise, "Rotate counterclockwise"},
    ActionRow{Action::rotate_clockwise, "Rotate clockwise"},
    ActionRow{Action::hard_drop, "Hard / sonic drop"},
    ActionRow{Action::hold, "Hold piece"},
    ActionRow{Action::restart, "Quick restart"},
    ActionRow{Action::start, "Start / pause"},
    ActionRow{Action::select, "Select"},
    ActionRow{Action::quit, "Quit"},
};

int assigned_player(const GubsyLobbyState& lobby, int device_id) {
    for (int player = 0; player < static_cast<int>(lobby.local_players.size()); ++player) {
        const GubsyLobbyPlayer& local = lobby.local_players[static_cast<std::size_t>(player)];
        const bool assigned = std::ranges::any_of(
            local.devices, [device_id](GubsyLobbyDeviceAssignment device) {
                return device.type == InputSourceType::Gamepad && device.device_id == device_id;
            });
        if (assigned)
            return player;
    }
    return -1;
}

int draw_browser_gamepads() {
#ifdef __EMSCRIPTEN__
    if (emscripten_sample_gamepad_data() != EMSCRIPTEN_RESULT_SUCCESS) {
        ImGui::Text("Browser Gamepad API: unavailable");
        return -1;
    }

    int connected = 0;
    const int slots = emscripten_get_num_gamepads();
    for (int index = 0; index < slots; ++index) {
        EmscriptenGamepadEvent gamepad{};
        if (emscripten_get_gamepad_status(index, &gamepad) != EMSCRIPTEN_RESULT_SUCCESS ||
            !gamepad.connected) {
            continue;
        }
        ++connected;
        ImGui::BulletText("Browser %d: %s", gamepad.index, gamepad.id);
        ImGui::Indent();
        ImGui::TextDisabled("mapping %s, %d buttons, %d axes",
                            gamepad.mapping[0] ? gamepad.mapping : "none", gamepad.numButtons,
                            gamepad.numAxes);
        ImGui::Unindent();
    }
    ImGui::Text("Browser Gamepad API: %d connected", connected);
    return connected;
#else
    return -1;
#endif
}

void draw_input_diagnostics(GubsyRuntime& runtime) {
    ImGui::SeparatorText("Detection diagnostics");
    const int browser_count = draw_browser_gamepads();

    int joystick_count = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&joystick_count);
    int mapped_count = 0;
    for (int index = 0; index < joystick_count; ++index) {
        const SDL_JoystickID id = joysticks[index];
        const bool mapped = SDL_IsGamepad(id);
        mapped_count += mapped ? 1 : 0;
        const char* name = SDL_GetJoystickNameForID(id);
        ImGui::BulletText("SDL %u: %s%s", id, name && *name ? name : "Unknown joystick",
                          mapped ? "" : " (not mapped as a gamepad)");
    }
    SDL_free(joysticks);

    const int opened_count = static_cast<int>(gubsy_get_gamepads(runtime).size());
    ImGui::Text("SDL joysticks: %d | mapped gamepads: %d | Gubsy opened: %d", joystick_count,
                mapped_count, opened_count);

    if (browser_count > 0 && joystick_count == 0) {
        ImGui::TextColored(ImVec4(1.0F, 0.75F, 0.25F, 1.0F),
                           "Browser sees the controller, but SDL has not discovered it. Rescan.");
    } else if (joystick_count > 0 && mapped_count == 0) {
        ImGui::TextColored(ImVec4(1.0F, 0.45F, 0.3F, 1.0F),
                           "SDL sees a joystick, but has no standard gamepad mapping for it.");
    } else if (mapped_count > opened_count) {
        ImGui::TextColored(ImVec4(1.0F, 0.45F, 0.3F, 1.0F),
                           "SDL recognizes a gamepad that Gubsy could not open.");
    }
}

void draw_controllers(GubsyRuntime& runtime, std::string& status) {
    if (ImGui::Button("Rescan controllers")) {
        gubsy_refresh_gamepads(runtime);
        assign_unclaimed_gamepads(runtime);
        status = gubsy_get_gamepads(runtime).empty() ? "No controllers detected"
                                                     : "Controller scan complete";
    }

#ifdef __EMSCRIPTEN__
    ImGui::TextWrapped("Chrome may hide a connected controller until you press a button on it. "
                       "Press a controller button, then choose Rescan controllers.");
#else
    ImGui::TextWrapped("Connected controllers are detected automatically. Rescan if a device was "
                       "connected before the game opened but does not appear here.");
#endif

    draw_input_diagnostics(runtime);

    const std::vector<GubsyGamepad> gamepads = gubsy_get_gamepads(runtime);
    if (gamepads.empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("No controllers detected.");
        return;
    }

    for (const GubsyGamepad& gamepad : gamepads) {
        ImGui::PushID(gamepad.device_id);
        ImGui::SeparatorText(gamepad.name.c_str());
        ImGui::TextDisabled("SDL device %d", gamepad.device_id);
        const int selected = assigned_player(gubsy_get_lobby_state(runtime), gamepad.device_id);
        if (ImGui::RadioButton("Player 1", selected == 0)) {
            status = set_gamepad_player(runtime, gamepad.device_id, 0)
                         ? "Controller assigned to Player 1"
                         : "Could not assign controller";
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Player 2", selected == 1)) {
            status = set_gamepad_player(runtime, gamepad.device_id, 1)
                         ? "Controller assigned to Player 2"
                         : "Could not assign controller";
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Unassigned", selected < 0)) {
            status = set_gamepad_player(runtime, gamepad.device_id, -1)
                         ? "Controller unassigned"
                         : "Could not unassign controller";
        }
        ImGui::PopID();
    }
}

bool save(GubsyRuntime& runtime, const BindsProfile& profile, std::string& status) {
    if (!gubsy_replace_binds_profile(runtime, profile)) {
        status = "Could not save bindings";
        return false;
    }
    status = "Bindings saved";
    return true;
}

void choice_heading(int code) {
    if (code == static_cast<int>(GubsyButton::KB_A))
        ImGui::TextDisabled("Keyboard");
    else if (code == static_cast<int>(GubsyButton::MOUSE_LEFT))
        ImGui::TextDisabled("Mouse");
    else if (code == static_cast<int>(GubsyButton::GP_A))
        ImGui::TextDisabled("Controller");
}

bool draw_binding_combo(GubsyRuntime& runtime, const BindsProfile& profile, int binding_index,
                        int action, const char* id, std::string& status) {
    const std::vector<ginput::ButtonBind>& bindings = profile.button_binds();
    const bool adding = binding_index < 0;
    const int current = adding ? -1
                               : bindings[static_cast<std::size_t>(binding_index)].device_button;
    const std::string preview =
        adding ? "Add binding..." : binds_input_label(BindsActionType::Button, current);
    ImGui::SetNextItemWidth(270.0F);
    if (!ImGui::BeginCombo(id, preview.c_str()))
        return false;

    bool changed = false;
    for (const InputChoice choice : binds_input_choices(BindsActionType::Button)) {
        choice_heading(choice.code);
        const bool selected = choice.code == current;
        if (ImGui::Selectable(choice.label, selected)) {
            BindsProfile updated = profile;
            if (!replace_bind_at(updated, BindsActionType::Button, binding_index, choice.code,
                                 action)) {
                status = "That binding already exists";
            } else {
                changed = save(runtime, updated, status);
            }
        }
        if (selected)
            ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
    return changed;
}

bool draw_axis_combo(GubsyRuntime& runtime, const BindsProfile& profile, int binding_index,
                     AnalogAction action, const char* id, std::string& status) {
    const std::vector<ginput::Axis1DBind>& bindings = profile.axis_1d_binds();
    const bool adding = binding_index < 0;
    const int current =
        adding ? -1 : bindings[static_cast<std::size_t>(binding_index)].device_axis;
    const std::string preview =
        adding ? "Add trigger..." : binds_input_label(BindsActionType::Analog1D, current);
    ImGui::SetNextItemWidth(270.0F);
    if (!ImGui::BeginCombo(id, preview.c_str()))
        return false;

    bool changed = false;
    for (const InputChoice choice : binds_input_choices(BindsActionType::Analog1D)) {
        const bool selected = choice.code == current;
        if (ImGui::Selectable(choice.label, selected)) {
            BindsProfile updated = profile;
            if (!replace_bind_at(updated, BindsActionType::Analog1D, binding_index, choice.code,
                                 static_cast<int>(action))) {
                status = "That binding already exists";
            } else {
                changed = save(runtime, updated, status);
            }
        }
        if (selected)
            ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
    return changed;
}

bool draw_action(GubsyRuntime& runtime, const BindsProfile& profile, ActionRow row,
                 std::string& status) {
    const int action = static_cast<int>(row.action);
    ImGui::SeparatorText(row.label);
    bool found = false;
    for (std::size_t index = 0; index < profile.button_binds().size(); ++index) {
        if (profile.button_binds()[index].action != action)
            continue;
        found = true;
        ImGui::PushID(static_cast<int>(index));
        const bool changed =
            draw_binding_combo(runtime, profile, static_cast<int>(index), action, "##binding",
                               status);
        ImGui::SameLine();
        const bool remove = ImGui::SmallButton("Remove");
        ImGui::PopID();
        if (changed)
            return true;
        if (remove) {
            BindsProfile updated = profile;
            if (remove_bind_at(updated, BindsActionType::Button, static_cast<int>(index)))
                return save(runtime, updated, status);
            status = "Could not remove binding";
        }
    }

    if (!found)
        ImGui::TextDisabled("Unbound");
    ImGui::PushID(action);
    const bool changed =
        draw_binding_combo(runtime, profile, -1, action, "##add-binding", status);
    ImGui::PopID();
    return changed;
}

bool draw_drop_axes(GubsyRuntime& runtime, const BindsProfile& profile, std::string& status) {
    constexpr AnalogAction action = AnalogAction::drop;
    ImGui::SeparatorText("Trigger hard / sonic drop");
    bool found = false;
    for (std::size_t index = 0; index < profile.axis_1d_binds().size(); ++index) {
        if (profile.axis_1d_binds()[index].axis_1d != static_cast<int>(action))
            continue;
        found = true;
        // Axis and button bindings use separate vectors whose numeric indices
        // can overlap. Give axis rows their own namespace before using the
        // vector index so repeated Remove buttons never share an ImGui ID.
        ImGui::PushID("drop-axis-row");
        ImGui::PushID(static_cast<int>(index));
        const bool changed =
            draw_axis_combo(runtime, profile, static_cast<int>(index), action, "##axis", status);
        ImGui::SameLine();
        const bool remove = ImGui::SmallButton("Remove");
        ImGui::PopID();
        ImGui::PopID();
        if (changed)
            return true;
        if (remove) {
            BindsProfile updated = profile;
            if (remove_bind_at(updated, BindsActionType::Analog1D, static_cast<int>(index)))
                return save(runtime, updated, status);
            status = "Could not remove binding";
        }
    }

    if (!found)
        ImGui::TextDisabled("Unbound");
    ImGui::PushID("drop-axis");
    const bool changed = draw_axis_combo(runtime, profile, -1, action, "##add-axis", status);
    ImGui::PopID();
    return changed;
}

void draw_player(GubsyRuntime& runtime, int player, std::string& status) {
    const GubsyLobbyState& lobby = gubsy_get_lobby_state(runtime);
    if (player >= static_cast<int>(lobby.local_players.size())) {
        ImGui::TextUnformatted("Player input profile is unavailable.");
        return;
    }

    const int profile_id = lobby.local_players[static_cast<std::size_t>(player)].binds_profile_id;
    const BindsProfile* source = gubsy_find_binds_profile(runtime, profile_id);
    if (source == nullptr) {
        ImGui::TextUnformatted("Player bindings are unavailable.");
        return;
    }
    const BindsProfile profile = *source;

    ImGui::TextUnformatted("Changes apply immediately and persist between runs.");
    if (ImGui::Button("Reset to defaults")) {
        status = reset_controls(runtime, player) ? "Default bindings restored"
                                                 : "Could not restore default bindings";
        return;
    }
    ImGui::SameLine();
    if (ImGui::Button("Swap rotation directions")) {
        status = swap_rotation_controls(runtime, player)
                     ? "Clockwise and counterclockwise bindings swapped"
                     : "Could not swap rotation bindings";
        return;
    }
    for (const ActionRow row : actions) {
        if (draw_action(runtime, profile, row, status))
            return;
        if (row.action == Action::hard_drop && draw_drop_axes(runtime, profile, status))
            return;
    }
}

} // namespace

void ControlsMenu::open() {
    visible = true;
    select_player_one = true;
}

void ControlsMenu::toggle() {
    visible = !visible;
    if (visible)
        select_player_one = true;
}

bool ControlsMenu::is_open() const {
    return visible;
}

bool ControlsMenu::chord_pressed(const Buttons& buttons) {
    const bool chord_down = buttons.start && buttons.select;
    const bool pressed = chord_down && !chord_was_down;
    chord_was_down = chord_down;
    return pressed;
}

void ControlsMenu::draw(GubsyRuntime& runtime, bool arranged_with_tools) {
    if (!visible)
        return;

    if (arranged_with_tools) {
        constexpr float tools_and_gap = 343.0F;
        constexpr float column_gap = 12.0F;
        constexpr float window_top = 36.0F;
        constexpr float window_margin = 16.0F;
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float available_columns =
            std::max(2.0F, display.x - tools_and_gap - column_gap - window_margin);
        const float available_width = available_columns * 0.5F;
        const float controls_x = tools_and_gap + available_width + column_gap;
        const float available_height = std::max(1.0F, display.y - window_top - window_margin);
        const float minimum_width = std::min(360.0F, available_width);
        const float minimum_height = std::min(240.0F, available_height);
        ImGui::SetNextWindowPos(ImVec2(controls_x, window_top), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(available_width, available_height),
                                 ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(minimum_width, minimum_height),
            ImVec2(available_width, available_height));
    } else {
        ImGui::SetNextWindowSize(ImVec2(520.0F, 640.0F), ImGuiCond_FirstUseEver);
    }
    if (ImGui::Begin("Controls", &visible)) {
        if (!status.empty())
            ImGui::TextDisabled("%s", status.c_str());
        if (ImGui::BeginTabBar("Players")) {
            if (ImGui::BeginTabItem("Controllers")) {
                draw_controllers(runtime, status);
                ImGui::EndTabItem();
            }
            const ImGuiTabItemFlags player_one_flags =
                select_player_one ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
            if (ImGui::BeginTabItem("Player 1", nullptr, player_one_flags)) {
                draw_player(runtime, 0, status);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Player 2")) {
                draw_player(runtime, 1, status);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
            select_player_one = false;
        }
    }
    ImGui::End();
}

} // namespace tetris
