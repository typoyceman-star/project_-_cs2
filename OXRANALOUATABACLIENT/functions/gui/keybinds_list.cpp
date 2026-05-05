#include "keybinds_list.h"
#include "widgets.h"
#include "../core/globals.h"
#include "imgui.h"

void DrawKeybindsListImGui() {
    if (!g_keybindsListEnabled) return;
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings;
    if (!g_menuOpen) {
        flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
    }
    
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 0.8f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    
    ImGui::SetNextWindowPos(ImVec2(10, 100), ImGuiCond_FirstUseEver);
    ImGui::Begin("KeybindsList", nullptr, flags);
    
    ImGui::TextColored(g_guiColor, "Keybinds");
    ImGui::Separator();
    
    // Aimbot
    if (g_aimbotEnabled) {
        bool active = (GetAsyncKeyState(g_aimbotKey) & 0x8000) != 0;
        ImGui::TextColored(active ? g_guiColor : ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
            "Aimbot [%s]", VkToStringA(g_aimbotKey));
        if (active) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Active");
        }
    }
    
    // Triggerbot
    if (g_triggerEnabled) {
        bool active = (GetAsyncKeyState(g_triggerKey) & 0x8000) != 0;
        ImGui::TextColored(active ? g_guiColor : ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
            "Triggerbot [%s]", VkToStringA(g_triggerKey));
        if (active) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Active");
        }
    }
    
    // Bhop
    if (g_bhopEnabled) {
        bool active = (GetAsyncKeyState(g_bhopKey) & 0x8000) != 0;
        ImGui::TextColored(active ? g_guiColor : ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
            "Bhop [%s]", VkToStringA(g_bhopKey));
        if (active) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Active");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}
