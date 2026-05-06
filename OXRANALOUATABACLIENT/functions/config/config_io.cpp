#include "config_io.h"
#include "paths.h"
#include "../core/globals.h"
#include "imgui.h"

void SaveConfig()
{
    std::string dllDir;
    if (!GetDllDirA(dllDir)) return;
    std::string configPath = MakePathA(dllDir, "config.ini");
    char buf[64];
    
    // Binds
    wsprintfA(buf, "%d", g_aimbotKey); WritePrivateProfileStringA("Binds", "AimbotKey", buf, configPath.c_str());
    wsprintfA(buf, "%d", g_triggerKey); WritePrivateProfileStringA("Binds", "TriggerKey", buf, configPath.c_str());
    wsprintfA(buf, "%d", g_whKey); WritePrivateProfileStringA("Binds", "EspKey", buf, configPath.c_str());
    wsprintfA(buf, "%d", g_bonesKey); WritePrivateProfileStringA("Binds", "BonesKey", buf, configPath.c_str());
    wsprintfA(buf, "%d", g_bhopKey); WritePrivateProfileStringA("Binds", "BhopKey", buf, configPath.c_str());
    
    // ESP Floats
    sprintf_s(buf, sizeof(buf), "%.1f", g_boxPadding); WritePrivateProfileStringA("ESP", "BoxPadding", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_boxThickness); WritePrivateProfileStringA("ESP", "BoxThickness", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_hpBarWidth); WritePrivateProfileStringA("ESP", "HPBarWidth", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_hpBarOffset); WritePrivateProfileStringA("ESP", "HPBarOffset", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_nameOffsetY); WritePrivateProfileStringA("ESP", "NameOffsetY", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_gunOffsetY); WritePrivateProfileStringA("ESP", "GunOffsetY", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_gunOffsetX); WritePrivateProfileStringA("ESP", "GunOffsetX", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_damageTextSize); WritePrivateProfileStringA("ESP", "DamageTextSize", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_damageTextLifetime); WritePrivateProfileStringA("ESP", "DamageTextLifetime", buf, configPath.c_str());
    
    // Aimbot Floats & Settings
    sprintf_s(buf, sizeof(buf), "%.1f", g_aimbotFov); WritePrivateProfileStringA("Combat", "AimbotFov", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_aimbotSmooth); WritePrivateProfileStringA("Combat", "AimbotSmooth", buf, configPath.c_str());
    WritePrivateProfileStringA("Combat", "AimbotTeamCheck", g_aimbotTeamCheck ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Combat", "AimbotVisCheck", g_aimbotVisCheck ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Combat", "AimbotAutoFire", g_aimbotAutoFire ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Combat", "AimbotDrawFov", g_aimbotDrawFov ? "1" : "0", configPath.c_str());

    // Booleans Visuals
    WritePrivateProfileStringA("Visuals", "NameEsp", g_nameEspEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "GunEsp", g_gunEspEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "HpBar", g_hpBarEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "DynamicBoxColor", g_dynamicBoxColor ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "HitSound", g_hitSoundEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "DamageIndicators", g_damageIndicatorsEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "WhEnabled", g_whEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "BonesEnabled", g_bonesEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "ChamsEnabled", g_chamsEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "SpectatorList", g_spectatorListEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "BombEsp", g_bombEspEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "CustomCrosshair", g_customCrosshair ? "1" : "0", configPath.c_str());
    
    // Дополнительные функции Visuals
    WritePrivateProfileStringA("Visuals", "Snaplines", g_snaplinesEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "Distance", g_distanceEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "RadarHack", g_radarHackEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "OofArrows", g_oofArrowsEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "KeybindsList", g_keybindsListEnabled ? "1" : "0", configPath.c_str());
    
    // Misc
    WritePrivateProfileStringA("Misc", "SkinWarningShown", g_skinWarningShown ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Misc", "AutoStrafe", g_autoStrafeEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Misc", "AntiCapture", g_antiCaptureEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Combat", "AimbotEnabled", g_aimbotEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Combat", "TriggerEnabled", g_triggerEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Misc", "BhopEnabled", g_bhopEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Misc", "AntiFlash", g_antiFlashEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Misc", "NoSmoke", g_noSmokeEnabled ? "1" : "0", configPath.c_str());
    // NoRecoil/NoSpread сохранение отключено - крашит при инжекте в главном меню
    // WritePrivateProfileStringA("Misc", "NoRecoil", g_noRecoilEnabled ? "1" : "0", configPath.c_str());
    // WritePrivateProfileStringA("Misc", "NoSpread", g_noSpreadEnabled ? "1" : "0", configPath.c_str());
    
    // Сохранение значений слайдеров (ползунков)
    sprintf_s(buf, sizeof(buf), "%.0f", g_oofArrowsRadius); WritePrivateProfileStringA("Visuals", "OofRadius", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_oofArrowsSize); WritePrivateProfileStringA("Visuals", "OofSize", buf, configPath.c_str());
    
    // Сохранение цвета меню (R, G, B, A)
    sprintf_s(buf, sizeof(buf), "%.2f,%.2f,%.2f,%.2f", g_guiColor.x, g_guiColor.y, g_guiColor.z, g_guiColor.w);
    WritePrivateProfileStringA("Menu", "Color", buf, configPath.c_str());

    // Skin Changer (per-weapon: paintKit, wear, seed, name)
    WritePrivateProfileStringA("SkinChanger", "Enabled", g_skinChangerEnabled ? "1" : "0", configPath.c_str());
    {
        char keyBuf[64];
        for (const auto& kv : g_skinConfig) {
            const int def = kv.first;
            const WeaponSkinCfg& cfg = kv.second;
            sprintf_s(keyBuf, sizeof(keyBuf), "W%d_PaintKit", def); wsprintfA(buf, "%d", cfg.paintKit); WritePrivateProfileStringA("SkinChanger", keyBuf, buf, configPath.c_str());
            sprintf_s(keyBuf, sizeof(keyBuf), "W%d_Wear", def);     sprintf_s(buf, sizeof(buf), "%.4f", cfg.wear); WritePrivateProfileStringA("SkinChanger", keyBuf, buf, configPath.c_str());
            sprintf_s(keyBuf, sizeof(keyBuf), "W%d_Seed", def);     wsprintfA(buf, "%d", cfg.seed); WritePrivateProfileStringA("SkinChanger", keyBuf, buf, configPath.c_str());
            sprintf_s(keyBuf, sizeof(keyBuf), "W%d_Name", def);     WritePrivateProfileStringA("SkinChanger", keyBuf, cfg.customName, configPath.c_str());
        }
    }

    // Knife Changer
    WritePrivateProfileStringA("KnifeChanger", "Enabled", g_knifeEnabled ? "1" : "0", configPath.c_str());
    wsprintfA(buf, "%d", g_knifeDefIndex); WritePrivateProfileStringA("KnifeChanger", "DefIndex", buf, configPath.c_str());
    wsprintfA(buf, "%d", g_knifePaintKit); WritePrivateProfileStringA("KnifeChanger", "PaintKit", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.4f", g_knifeWear); WritePrivateProfileStringA("KnifeChanger", "Wear", buf, configPath.c_str());
    wsprintfA(buf, "%d", g_knifeSeed); WritePrivateProfileStringA("KnifeChanger", "Seed", buf, configPath.c_str());
    WritePrivateProfileStringA("KnifeChanger", "Name", g_knifeName, configPath.c_str());
}

void LoadConfig()
{
    std::string dllDir;
    if (!GetDllDirA(dllDir)) return;
    std::string configPath = MakePathA(dllDir, "config.ini");
    char buf[64];
    
    // Binds
    g_aimbotKey = GetPrivateProfileIntA("Binds", "AimbotKey", g_aimbotKey, configPath.c_str());
    g_triggerKey = GetPrivateProfileIntA("Binds", "TriggerKey", g_triggerKey, configPath.c_str());
    g_whKey = GetPrivateProfileIntA("Binds", "EspKey", g_whKey, configPath.c_str());
    g_bonesKey = GetPrivateProfileIntA("Binds", "BonesKey", g_bonesKey, configPath.c_str());
    g_bhopKey = GetPrivateProfileIntA("Binds", "BhopKey", g_bhopKey, configPath.c_str());
    
    // ESP Floats
    GetPrivateProfileStringA("ESP", "BoxPadding", "1.5", buf, sizeof(buf), configPath.c_str()); g_boxPadding = (float)atof(buf);
    GetPrivateProfileStringA("ESP", "BoxThickness", "2.0", buf, sizeof(buf), configPath.c_str()); g_boxThickness = (float)atof(buf);
    GetPrivateProfileStringA("ESP", "HPBarWidth", "4.0", buf, sizeof(buf), configPath.c_str()); g_hpBarWidth = (float)atof(buf);
    GetPrivateProfileStringA("ESP", "HPBarOffset", "3.0", buf, sizeof(buf), configPath.c_str()); g_hpBarOffset = (float)atof(buf);
    GetPrivateProfileStringA("ESP", "NameOffsetY", "14.0", buf, sizeof(buf), configPath.c_str()); g_nameOffsetY = (float)atof(buf);
    GetPrivateProfileStringA("ESP", "GunOffsetY", "2.0", buf, sizeof(buf), configPath.c_str()); g_gunOffsetY = (float)atof(buf);
    GetPrivateProfileStringA("ESP", "GunOffsetX", "0.0", buf, sizeof(buf), configPath.c_str()); g_gunOffsetX = (float)atof(buf);
    GetPrivateProfileStringA("ESP", "DamageTextSize", "20.0", buf, sizeof(buf), configPath.c_str()); g_damageTextSize = (float)atof(buf);
    GetPrivateProfileStringA("ESP", "DamageTextLifetime", "4.0", buf, sizeof(buf), configPath.c_str()); g_damageTextLifetime = (float)atof(buf);

    // Aimbot Floats & Settings
    GetPrivateProfileStringA("Combat", "AimbotFov", "5.0", buf, sizeof(buf), configPath.c_str()); g_aimbotFov = (float)atof(buf);
    GetPrivateProfileStringA("Combat", "AimbotSmooth", "3.0", buf, sizeof(buf), configPath.c_str()); g_aimbotSmooth = (float)atof(buf);
    g_aimbotTeamCheck = GetPrivateProfileIntA("Combat", "AimbotTeamCheck", g_aimbotTeamCheck ? 1 : 0, configPath.c_str()) != 0;
    g_aimbotVisCheck = GetPrivateProfileIntA("Combat", "AimbotVisCheck", g_aimbotVisCheck ? 1 : 0, configPath.c_str()) != 0;
    g_aimbotAutoFire = GetPrivateProfileIntA("Combat", "AimbotAutoFire", g_aimbotAutoFire ? 1 : 0, configPath.c_str()) != 0;
    g_aimbotDrawFov = GetPrivateProfileIntA("Combat", "AimbotDrawFov", g_aimbotDrawFov ? 1 : 0, configPath.c_str()) != 0;

    // Booleans Visuals
    g_nameEspEnabled = GetPrivateProfileIntA("Visuals", "NameEsp", g_nameEspEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_gunEspEnabled = GetPrivateProfileIntA("Visuals", "GunEsp", g_gunEspEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_hpBarEnabled = GetPrivateProfileIntA("Visuals", "HpBar", g_hpBarEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_dynamicBoxColor = GetPrivateProfileIntA("Visuals", "DynamicBoxColor", g_dynamicBoxColor ? 1 : 0, configPath.c_str()) != 0;
    g_hitSoundEnabled = GetPrivateProfileIntA("Visuals", "HitSound", g_hitSoundEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_damageIndicatorsEnabled = GetPrivateProfileIntA("Visuals", "DamageIndicators", g_damageIndicatorsEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_whEnabled = GetPrivateProfileIntA("Visuals", "WhEnabled", g_whEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_bonesEnabled = GetPrivateProfileIntA("Visuals", "BonesEnabled", g_bonesEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_chamsEnabled = GetPrivateProfileIntA("Visuals", "ChamsEnabled", g_chamsEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_spectatorListEnabled = GetPrivateProfileIntA("Visuals", "SpectatorList", g_spectatorListEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_bombEspEnabled = GetPrivateProfileIntA("Visuals", "BombEsp", g_bombEspEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_customCrosshair = GetPrivateProfileIntA("Visuals", "CustomCrosshair", g_customCrosshair ? 1 : 0, configPath.c_str()) != 0;
    
    // Дополнительные функции Visuals
    g_snaplinesEnabled = GetPrivateProfileIntA("Visuals", "Snaplines", g_snaplinesEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_distanceEnabled = GetPrivateProfileIntA("Visuals", "Distance", g_distanceEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_radarHackEnabled = GetPrivateProfileIntA("Visuals", "RadarHack", g_radarHackEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_oofArrowsEnabled = GetPrivateProfileIntA("Visuals", "OofArrows", g_oofArrowsEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_keybindsListEnabled = GetPrivateProfileIntA("Visuals", "KeybindsList", g_keybindsListEnabled ? 1 : 0, configPath.c_str()) != 0;
    
    // Misc
    g_skinWarningShown = GetPrivateProfileIntA("Misc", "SkinWarningShown", g_skinWarningShown ? 1 : 0, configPath.c_str()) != 0;
    g_autoStrafeEnabled = GetPrivateProfileIntA("Misc", "AutoStrafe", g_autoStrafeEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_antiCaptureEnabled = GetPrivateProfileIntA("Misc", "AntiCapture", g_antiCaptureEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_aimbotEnabled = GetPrivateProfileIntA("Combat", "AimbotEnabled", g_aimbotEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_triggerEnabled = GetPrivateProfileIntA("Combat", "TriggerEnabled", g_triggerEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_bhopEnabled = GetPrivateProfileIntA("Misc", "BhopEnabled", g_bhopEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_antiFlashEnabled = GetPrivateProfileIntA("Misc", "AntiFlash", g_antiFlashEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_noSmokeEnabled = GetPrivateProfileIntA("Misc", "NoSmoke", g_noSmokeEnabled ? 1 : 0, configPath.c_str()) != 0;
    // NoRecoil/NoSpread загрузка отключена - крашит при инжекте в главном меню
    // g_noRecoilEnabled = GetPrivateProfileIntA("Misc", "NoRecoil", g_noRecoilEnabled ? 1 : 0, configPath.c_str()) != 0;
    // g_noSpreadEnabled = GetPrivateProfileIntA("Misc", "NoSpread", g_noSpreadEnabled ? 1 : 0, configPath.c_str()) != 0;
    
    // Загрузка значений слайдеров
    GetPrivateProfileStringA("Visuals", "OofRadius", "150.0", buf, sizeof(buf), configPath.c_str()); g_oofArrowsRadius = (float)atof(buf);
    GetPrivateProfileStringA("Visuals", "OofSize", "15.0", buf, sizeof(buf), configPath.c_str()); g_oofArrowsSize = (float)atof(buf);
    
    // Загрузка цвета меню
    GetPrivateProfileStringA("Menu", "Color", "", buf, sizeof(buf), configPath.c_str());
    if (buf[0] != '\0') {
        float r, g, b, a;
        if (sscanf_s(buf, "%f,%f,%f,%f", &r, &g, &b, &a) == 4) {
            g_guiColor = ImVec4(r, g, b, a);
            // ApplyClientStyle(); // ИСПРАВЛЕНО: Убрано. Контекст ImGui тут еще не создан!
        }
    }

    // Skin Changer
    g_skinChangerEnabled = GetPrivateProfileIntA("SkinChanger", "Enabled", g_skinChangerEnabled ? 1 : 0, configPath.c_str()) != 0;
    {
        char keyBuf[64];
        for (auto& kv : g_skinConfig) {
            const int def = kv.first;
            WeaponSkinCfg& cfg = kv.second;
            sprintf_s(keyBuf, sizeof(keyBuf), "W%d_PaintKit", def);
            cfg.paintKit = GetPrivateProfileIntA("SkinChanger", keyBuf, cfg.paintKit, configPath.c_str());
            sprintf_s(keyBuf, sizeof(keyBuf), "W%d_Wear", def);
            char wearStr[32]; sprintf_s(wearStr, sizeof(wearStr), "%.4f", cfg.wear);
            GetPrivateProfileStringA("SkinChanger", keyBuf, wearStr, buf, sizeof(buf), configPath.c_str());
            cfg.wear = (float)atof(buf);
            sprintf_s(keyBuf, sizeof(keyBuf), "W%d_Seed", def);
            cfg.seed = GetPrivateProfileIntA("SkinChanger", keyBuf, cfg.seed, configPath.c_str());
            sprintf_s(keyBuf, sizeof(keyBuf), "W%d_Name", def);
            GetPrivateProfileStringA("SkinChanger", keyBuf, "", cfg.customName, (DWORD)sizeof(cfg.customName), configPath.c_str());
        }
    }

    // Knife Changer
    g_knifeEnabled = GetPrivateProfileIntA("KnifeChanger", "Enabled", g_knifeEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_knifeDefIndex = GetPrivateProfileIntA("KnifeChanger", "DefIndex", g_knifeDefIndex, configPath.c_str());
    g_knifePaintKit = GetPrivateProfileIntA("KnifeChanger", "PaintKit", g_knifePaintKit, configPath.c_str());
    {
        char wearStr[32]; sprintf_s(wearStr, sizeof(wearStr), "%.4f", g_knifeWear);
        GetPrivateProfileStringA("KnifeChanger", "Wear", wearStr, buf, sizeof(buf), configPath.c_str());
        g_knifeWear = (float)atof(buf);
    }
    g_knifeSeed = GetPrivateProfileIntA("KnifeChanger", "Seed", g_knifeSeed, configPath.c_str());
    GetPrivateProfileStringA("KnifeChanger", "Name", "", g_knifeName, (DWORD)sizeof(g_knifeName), configPath.c_str());
}
