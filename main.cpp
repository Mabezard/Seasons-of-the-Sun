#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <cmath>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <string>
#include <array>
#include <vector>

namespace SeasonsOfTheSun {

    constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;
    constexpr float RAD_TO_DEG = 180.0f / 3.14159265358979323846f;
	
	int g_loadedPresetIndex = 0;
    float g_loadedLatitude = 53.0f;
    bool g_loadedInvertSouthern = false;

    int GetDefaultPresetIndex(RE::StaticFunctionTag*) {
        return g_loadedPresetIndex;
    }

    float GetDefaultLatitude(RE::StaticFunctionTag*) {
        return g_loadedLatitude;
    }

    // ==============================================================================
    // 1. LOGGER INITIALIZATION
    // ==============================================================================

    void InitializeLogging() {
        auto path = SKSE::log::log_directory();
        if (!path) {
            return;
        }

        *path /= "SeasonsOfTheSun.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));

        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    }

    // ==============================================================================
    // 2. COMMUNITY SHADERS JSON WRITER
    // ==============================================================================

    void WriteCommunityShadersOverride(float a_angle) {
        std::string jsonPath = "Data/SKSE/Plugins/CommunityShaders/Overrides/SeasonsOfTheSun_SkySync.json";
        
        try {
            std::filesystem::create_directories("Data/SKSE/Plugins/CommunityShaders/Overrides");
            
            std::ofstream outFile(jsonPath, std::ios::out | std::ios::trunc);
            if (outFile.is_open()) {
                outFile << "{\n";
                outFile << "  \"CustomAngle\": " << a_angle << ",\n";
               // outFile << "  \"MinShadowElevation\": 1.0,\n"; 
                outFile << "  \"SunPath\": 3,\n";
                outFile << "  \"Enabled\": true,\n";
                outFile << "  \"UseAlternateSunPath\": true\n";
                outFile << "}\n";
                outFile.close();
                SKSE::log::info("Updated CS Override JSON at '{}' (CustomAngle: {})", jsonPath, a_angle);
            } else {
                SKSE::log::error("Failed to open JSON override file at '{}'", jsonPath);
            }
        } catch (const std::exception& e) {
            SKSE::log::error("Exception in WriteCommunityShadersOverride: {}", e.what());
        }
    }

    // ==============================================================================
    // 3. TESCLIMATE MEMORY OVERRIDE & VAMPIRE PATCHING
    // ==============================================================================

    void UpdateSkyrimClimateTiming(float a_latitude, float a_declination, [[maybe_unused]] float a_transitionHours = 1.5f) {
        try {
            float astronomicalLat = (a_latitude > 0.0f) ? -a_latitude : a_latitude;

            float latRad  = astronomicalLat * DEG_TO_RAD;
            float declRad = a_declination * DEG_TO_RAD;

            constexpr float h0_refraction_rad = -0.833f * DEG_TO_RAD;

            float cosH0 = (std::sin(h0_refraction_rad) - std::sin(latRad) * std::sin(declRad)) / 
                          (std::cos(latRad) * std::cos(declRad));

            float h0;
            if (cosH0 >= 1.0f) {
                h0 = 15.0f;  // Polar Night fallback
            } else if (cosH0 <= -1.0f) {
                h0 = 165.0f; // Midnight Sun fallback
            } else {
                h0 = std::acos(cosH0) * RAD_TO_DEG;
            }

            float halfDayHours = h0 / 15.0f;

            float visualSunriseHour = 12.0f - halfDayHours;
            float visualSunsetHour  = 12.0f + halfDayHours;

            float sunriseBeginHour = visualSunriseHour - 1.0f;
            float sunriseEndHour   = visualSunriseHour + 1.5f;
            float sunsetBeginHour  = visualSunsetHour  - 1.5f;
            float sunsetEndHour    = visualSunsetHour  + 1.0f;

            uint8_t srBeginByte = static_cast<uint8_t>(std::clamp(sunriseBeginHour * 6.0f, 0.0f, 143.0f));
            uint8_t srEndByte   = static_cast<uint8_t>(std::clamp(sunriseEndHour   * 6.0f, 0.0f, 143.0f));
            uint8_t ssBeginByte = static_cast<uint8_t>(std::clamp(sunsetBeginHour  * 6.0f, 0.0f, 143.0f));
            uint8_t ssEndByte   = static_cast<uint8_t>(std::clamp(sunsetEndHour    * 6.0f, 0.0f, 143.0f));

            auto* climate = RE::TESForm::LookupByID<RE::TESClimate>(0x00000812);
            if (climate) {
                climate->timing.sunrise.begin = srBeginByte;
                climate->timing.sunrise.end   = srEndByte;
                climate->timing.sunset.begin  = ssBeginByte;
                climate->timing.sunset.end    = ssEndByte;

                SKSE::log::info("Updated TESClimate [00000812] -> Visual SR: {:.2f}h (SR Begin Byte {}), Visual SS: {:.2f}h (SS End Byte {})", 
                    visualSunriseHour, srBeginByte, visualSunsetHour, ssEndByte);
            } else {
                SKSE::log::warn("TESClimate record [00000812] not found in memory!");
            }
        } catch (const std::exception& e) {
            SKSE::log::error("Exception in UpdateSkyrimClimateTiming: {}", e.what());
        }
    }
    
    void UpdateVampireSunlightConditions(float visualSunriseHour, float visualSunsetHour) {
        constexpr std::array<RE::FormID, 4> VAMPIRE_SPELL_IDS = {
            0x000C1E8B, // MAG_VampireWeaknessSun01
            0x000ED09A, // MAG_VampireWeaknessSun02
            0x000ED09B, // MAG_VampireWeaknessSun03
            0x000ED09C  // MAG_VampireWeaknessSun04
        };

        static std::vector<float*> morningConditionTargets;
        static std::vector<float*> eveningConditionTargets;
        static bool initializedTargets = false;

        if (!initializedTargets) {
            for (auto formID : VAMPIRE_SPELL_IDS) {
                auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(formID);
                if (!spell) continue;

                auto discoverConditions = [](RE::TESCondition& condition) {
                    for (auto* cond = condition.head; cond; cond = cond->next) {
                        float& compVal = cond->data.comparisonValue.f;
                        if (compVal >= 4.5f && compVal <= 5.5f) {
                            morningConditionTargets.push_back(&compVal);
                        } else if (compVal >= 18.5f && compVal <= 19.5f) {
                            eveningConditionTargets.push_back(&compVal);
                        }
                    }
                };

                for (auto* effect : spell->effects) {
                    if (effect) {
                        discoverConditions(effect->conditions);
                        if (effect->baseEffect) {
                            discoverConditions(effect->baseEffect->conditions);
                        }
                    }
                }
            }

            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (dataHandler) {
                auto* lordSpell = dataHandler->LookupForm<RE::SpellItem>(0x012D15, "Dawnguard.esm");
                if (lordSpell) {
                    for (auto* effect : lordSpell->effects) {
                        if (effect) {
                            for (auto* cond = effect->conditions.head; cond; cond = cond->next) {
                                float& compVal = cond->data.comparisonValue.f;
                                if (compVal >= 4.5f && compVal <= 5.5f) morningConditionTargets.push_back(&compVal);
                                else if (compVal >= 18.5f && compVal <= 19.5f) eveningConditionTargets.push_back(&compVal);
                            }
                            if (effect->baseEffect) {
                                for (auto* cond = effect->baseEffect->conditions.head; cond; cond = cond->next) {
                                    float& compVal = cond->data.comparisonValue.f;
                                    if (compVal >= 4.5f && compVal <= 5.5f) morningConditionTargets.push_back(&compVal);
                                    else if (compVal >= 18.5f && compVal <= 19.5f) eveningConditionTargets.push_back(&compVal);
                                }
                            }
                        }
                    }
                }
            }

            initializedTargets = true;
            SKSE::log::info("Discovered {} morning targets and {} evening targets for vampire weakness.", 
                morningConditionTargets.size(), eveningConditionTargets.size());
        }

        for (float* target : morningConditionTargets) {
            if (target) *target = visualSunriseHour;
        }

        for (float* target : eveningConditionTargets) {
            if (target) *target = visualSunsetHour;
        }

        SKSE::log::info("Updated Vampire Conditions -> Morning ({:.2f}h), Evening ({:.2f}h)", 
            visualSunriseHour, visualSunsetHour);
    }

    // ==============================================================================
    // 4. PAPYRUS BINDING HANDLER
    // ==============================================================================
	

    float GetCalculatedSunAngle(RE::StaticFunctionTag*, float a_latitude, int a_dayOfYear, int a_totalDays, bool a_invertSouthern) {
        float activeLat = a_latitude;
        float phaseOffset = (a_totalDays == 365) ? 56.0f : 17.0f;
        float adjustedDay = static_cast<float>(a_dayOfYear) + phaseOffset;

        if (a_invertSouthern) {
            adjustedDay += (static_cast<float>(a_totalDays) * 0.5f);
        }

        float radianDay = (2.0f * 3.14159265358979323846f * adjustedDay) / static_cast<float>(a_totalDays);
        float declination = 23.44f * std::cos(radianDay);
        float finalAngle = activeLat + declination;

        // 1. Sync CS Override JSON
        WriteCommunityShadersOverride(finalAngle);

        // 2. Compute Visual Sunrise/Sunset hours for Climate & Vampires
        float astronomicalLat = (a_latitude > 0.0f) ? -a_latitude : a_latitude;
        float latRad  = astronomicalLat * DEG_TO_RAD;
        float declRad = declination * DEG_TO_RAD;
        constexpr float h0_refraction_rad = -0.833f * DEG_TO_RAD;

        float cosH0 = (std::sin(h0_refraction_rad) - std::sin(latRad) * std::sin(declRad)) / 
                      (std::cos(latRad) * std::cos(declRad));
        float h0;
        if (cosH0 >= 1.0f) {
            h0 = 15.0f;
        } else if (cosH0 <= -1.0f) {
            h0 = 165.0f;
        } else {
            h0 = std::acos(cosH0) * RAD_TO_DEG;
        }
        float halfDayHours = h0 / 15.0f;
        float visualSunriseHour = 12.0f - halfDayHours;
        float visualSunsetHour  = 12.0f + halfDayHours;

        // 3. Mutate TESClimate Memory Timings
        UpdateSkyrimClimateTiming(a_latitude, declination, 1.5f);

        // 4. Mutate Vampire Sunlight Weakness Conditions in Memory
        UpdateVampireSunlightConditions(visualSunriseHour, visualSunsetHour);
		
        
        SKSE::log::info("Calculated Sun Angle [Lat: {}, Day: {}/{}] -> Result: {} deg (SR: {:.2f}h, SS: {:.2f}h)",
            a_latitude, a_dayOfYear, a_totalDays, finalAngle, visualSunriseHour, visualSunsetHour);

        return finalAngle;
    }

    bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm) {
        a_vm->RegisterFunction("GetCalculatedSunAngle", "SeasonsOfTheSunNative", GetCalculatedSunAngle);
        return true;
    }
}

// ==============================================================================
// SKSE PLUGIN ENTRY POINT
// ==============================================================================

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse) {
    SeasonsOfTheSun::InitializeLogging();

    SKSE::Init(a_skse);
    SKSE::log::info("SeasonsOfTheSun C++ Plugin initialized.");

    auto messaging = SKSE::GetMessagingInterface();
    if (messaging) {
        messaging->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
            if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
                SKSE::GetPapyrusInterface()->Register(SeasonsOfTheSun::RegisterPapyrusFunctions);
                SKSE::log::info("Successfully registered SeasonsOfTheSunNative Papyrus bindings.");
            }
        });
    }

    return true;
}