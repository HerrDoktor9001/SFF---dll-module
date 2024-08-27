#include "Event.h"
#include <spdlog/sinks/basic_file_sink.h>
using namespace std;

namespace logger = SKSE::log;

bool file_exists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

void SetupLog() {
    auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
    auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));
    spdlog::set_default_logger(std::move(loggerPtr));
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::trace);
}

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message) {
    switch (message->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            //
            InputEventHandler::Enable();
            MenuOpenCloseEventHandler::Enable();
            ActivationEventHandler::Enable();

            if (bGlobalVar("SFF_MCM_DisablePreview3D")) {
                DisableArmor3D::InstallHook();
            } else {
                logger::info("[WARNING] SFF Item3D Disabler not enabled. Skipping registration...");
            }
            TriggerBlocker::InstallHook();
            logger::info("*Events registration finished*");
 

            // SFF v1.7.1 - if SkyrimSoulsRE not installed, extra steps necessary to make mod work
           // constexpr auto ACC = L"Data/SKSE/Plugins/AlternateConversationCamera.dll";
           // constexpr auto SPiM = L"Data/SKSE/Plugins/ShowPlayerInMenus.dll";
           
            if (file_exists("Data/SKSE/Plugins/SkyrimSoulsRE.dll")) { 
                bSSinstalled= true;
                logger::info("[INFO] 'SkyrimSoulsRE.dll' found.");
                //logger::info("[WARNING] Be sure to have 'bContainerMenu' set to TRUE to avoid physics issues while using 'Previewer'.");
                logger::info("[WARNING] 'Previewer' feature will not be as smooth when using 'Skyrim Souls' due to alternative implementation requirements.");
            }else{
                bSSinstalled= false;
                logger::info("[INFO] 'SkyrimSoulsRE.dll' not found.");
            }

           // if (file_exists("Data/SKSE/Plugins/AlternateConversationCamera.dll")  && file_exists("Data/SKSE/Plugins/SkyrimSoulsRE.dll")) {
            //    logger::info(
            //        "[INFO] 'AlternateConversationCamera.dll' found.");
           // }
            
            if (file_exists("Data/SKSE/Plugins/ShowPlayerInMenus.dll")) {
                logger::info("[WARNING] 'ShowPlayerInMenus.dll' detected. Please note that SFF's plugin will override its 3D disabler feature.");
            } 
            /* else {
                DisableArmor3D::InstallHook();
            }*/

            break;
    }
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse) {
    SetupLog();

    auto g_messaging =  reinterpret_cast<SKSE::MessagingInterface*>(a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));
    if (!g_messaging) {
        logger::critical("Failed to load messaging interface! This error is fatal: plugin will not load.");
        return false;
    }


    SKSE::Init(a_skse);
    SKSE::AllocTrampoline(1 << 10);

    g_messaging->RegisterListener("SKSE", SKSEMessageHandler);

    logger::info("plugin loaded");

    return true;
}

constexpr auto MATH_PI = 3.14159265358979323846f;
constexpr auto TWO_PI = 6.2831853071795865f;

bool m_thirdForced = false;
bool m_enteredTweenFromFirst = false;
bool m_inMenu = false;
bool m_rotatedInTweenMenu = false;
bool m_shouldDisableAnimCam = false;
bool m_fixCameraZoom = false;
bool m_rotatedPlayer = false;
bool m_inSitState = false;
uint32_t m_camStateId;
float m_playerRotation;
RE::NiPoint2 g_freeRotation;
RE::NiPoint2 m_finalFreeRotation;
float m_finalPlayerAngleX;
float m_finalPlayerAngleZ;
float fRotationAmount = 0.075f;
float fTurnSensitivity = 3.0f;
RE::Setting* fOverShoulderCombatPosX;
RE::Setting* fOverShoulderCombatAddY;
RE::Setting* fOverShoulderCombatPosZ;
RE::Setting* fOverShoulderPosX;
RE::Setting* fOverShoulderPosZ;
RE::Setting* fAutoVanityModeDelay;
RE::Setting* fVanityModeMinDist;
RE::Setting* fVanityModeMaxDist;
RE::Setting* fMouseWheelZoomSpeed;

// TDM Helpers
float NormalRelativeAngle(float a_angle) {
    while (a_angle > MATH_PI) a_angle -= TWO_PI;
    while (a_angle < -MATH_PI) a_angle += TWO_PI;
    return a_angle;

    // return fmod(a_angle, TWO_PI) >= 0 ? (a_angle < PI) ? a_angle : a_angle - TWO_PI : (a_angle >= -PI) ? a_angle :
    // a_angle + TWO_PI;
}

float GetAngle(RE::NiPoint2& a, RE::NiPoint2& b) { return atan2(a.Cross(b), a.Dot(b)); }

RE::NiPoint3 Project(const RE::NiPoint3& A, const RE::NiPoint3& B) {
    return (B * ((A.x * B.x + A.y * B.y + A.z * B.z) / (B.x * B.x + B.y * B.y + B.z * B.z)));
}

RE::NiPoint2 Vec2Rotate(const RE::NiPoint2& vec, float angle) {
    RE::NiPoint2 ret;
    ret.x = vec.x * cos(angle) - vec.y * sin(angle);
    ret.y = vec.x * sin(angle) + vec.y * cos(angle);
    return ret;
}

RE::NiPoint3 GetCameraPos() {
    // auto player = RE::PlayerCharacter::GetSingleton();
    auto player = RE::TESObjectREFR::LookupByID<RE::Actor>(0x02002B6C);
    auto playerCamera = RE::PlayerCamera::GetSingleton();
    RE::NiPoint3 ret;

    if (playerCamera->currentState == playerCamera->cameraStates[RE::CameraStates::kFirstPerson] ||
        playerCamera->currentState == playerCamera->cameraStates[RE::CameraStates::kThirdPerson] ||
        playerCamera->currentState == playerCamera->cameraStates[RE::CameraStates::kMount]) {
        RE::NiNode* root = playerCamera->cameraRoot.get();
        if (root) {
            ret.x = root->world.translate.x;
            ret.y = root->world.translate.y;
            ret.z = root->world.translate.z;
        }
    } else {
        RE::NiPoint3 playerPos = player->GetLookingAtLocation();

        ret.z = playerPos.z;
        ret.x = player->GetPositionX();
        ret.y = player->GetPositionY();
    }

    return ret;
}

// properties
bool bSeranaActivated = false;

// -> -> ACTIVATION
auto ActivationEventHandler::ProcessEvent(const RE::TESActivateEvent* a_event,
                                          RE::BSTEventSource<RE::TESActivateEvent>*) -> EventResult {
    if (!a_event) {
        bSeranaActivated = false;
        return EventResult::kContinue;
    }

    auto PlayerRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(0x00000014);

    auto activatorRef = a_event->actionRef->GetBaseObject();

    if (activatorRef != PlayerRef->GetBaseObject()) {  // if Activator is not Player,
        // logger::info("Activator not Player. Return.");
        return EventResult::kContinue;  // stop code.
    }

    // If everything set (e.g., Player is actual Activator), declare and fill remaining variables
    auto SeranaRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(0x02002b74);  // Serana ref.
    auto activatedRef = a_event->objectActivated->GetBaseObject();            // Ref. of obj. activated by Player

    // Outfit containers
    std::string_view myList[8] = {"SFF_MainOutfit_01_Container", "SFF_MainOutfit_02_Container",
                                  "SFF_MainOutfit_03_Container","SFF_MainOutfit_04_Container","SFF_MainOutfit_05_Container", "SFF_OutfitContainer", "SFF_SleepOutfitContainer", "SFF_SeranaBackpack"};
    const auto* myCont1 = RE::TESForm::LookupByEditorID(myList[0]);
    const auto* myCont2 = RE::TESForm::LookupByEditorID(myList[1]);
    const auto* myCont3 = RE::TESForm::LookupByEditorID(myList[2]);
    const auto* myCont4 = RE::TESForm::LookupByEditorID(myList[3]);
    const auto* myCont5 = RE::TESForm::LookupByEditorID(myList[4]);
    const auto* myCont6 = RE::TESForm::LookupByEditorID(myList[5]);
    const auto* myCont7 = RE::TESForm::LookupByEditorID(myList[6]);

    const auto* myCont8 = RE::TESForm::LookupByEditorID(myList[7]);


    // Other outfit containers must also be added (Sleep, Home, etc.)

    if (activatorRef == PlayerRef->GetBaseObject()) {  // if Player the one activating,
        // logger::info("Player activated something...");

        if (activatedRef == SeranaRef->GetBaseObject()) {  // and Serana the one activated...
                                                           // logger::info("It is Serana! Run code.");

        } else {

            if (activatedRef != myCont1 && activatedRef != myCont2 && activatedRef != myCont3 &&
                activatedRef != myCont4 && activatedRef != myCont5 && activatedRef != myCont6 && activatedRef != myCont7) {  // and activated obj. not one of outfit cont.,

                if (activatedRef == myCont8) {
                
                    //do nothing. Just return code before setting flag to false
                    // this is because opening Backpack resets flag, and opening in sequence another container while still in dialogue means rotation will not be enabled!
                    logger::info("Backpack opened. No need to disable flag...");
                    return EventResult::kContinue;
                }
                
                bSeranaActivated = false;
                return EventResult::kContinue;
            }

            // 
        }

        // SFF v1.7.1 - clear flag so feature can work when activating Serana again
       // if (!bUsingTorch()) {
            bLeftHandFull = false;
       //     logger::info("Serana activated and not using torch; clear flag.");
       // }


        bSeranaActivated = true;  // will only run if activated ref. Serana; or if myCont1, myCont2 or myCont3
    }

    return RE::BSEventNotifyControl::kContinue;
}



/*
// code courtesy of 'Phinix' (http://www.gamesas.com/how-find-location-near-player-but-out-sight-avoid-t261071.html)
// converted from Papyrus to C++;
// adapted to manipulate X axis, instead of Y axis
RE::NiPoint3 GaugeNewPos(float dist) {

   auto player = RE::PlayerCharacter::GetSingleton();
   auto Serana = RE::TESForm::LookupByID<RE::Actor>(0x02002b74);

	float xpos = Serana->GetPositionX();	
	float ypos = Serana->GetPositionY();	
	float zAngle = (player->GetAngleZ()/ 0.01745329252);        // need to convert radian to degree!

    float multiplier = 0.0;
    logger::info("Radian Z: {}", player->GetAngleZ());
    logger::info("Degree Z: {}", zAngle);
    //if (player->GetAngleZ() < MATH_PI) {
    //    multiplier = -1.0;
    //    logger::info("Multiplier set to negative!");
    //} else {
    //    multiplier = 1.0;
   // }

    float xdistance = dist;
    //*multiplier;				

	
	float xdes = 0.0;
    float ydes = 0.0; 
	
	if (zAngle < 90.f) {
        zAngle = zAngle + 90.f;
    } else {
        zAngle = zAngle - 90.f;
    }


	if (zAngle == 0.f) {
        xdes = xpos;
        ydes = ypos + xdistance;
      
        logger::info("[INFO]: NiPoint updated. zAngle: {}", zAngle);
        return RE::NiPoint3(xdes, ydes, player->GetPositionZ());

    } else if (zAngle == 90.f) {
        xdes = xpos;
        ydes = ypos + xdistance;
        
        logger::info("[INFO]: NiPoint updated. zAngle: {}", zAngle);
        return RE::NiPoint3(xdes, ydes, player->GetPositionZ());

    } else if (zAngle == 180.f) {
        xdes = xpos - xdistance;
        ydes = ypos;
        
        logger::info("[INFO]: NiPoint updated. zAngle: {}", zAngle);
        return RE::NiPoint3(xdes, ydes, player->GetPositionZ());

    }

    else if (zAngle == 270.f) {
        xdes = xpos;
        ydes = ypos - xdistance;

        logger::info("[INFO]: NiPoint updated. zAngle: {}", zAngle);
        return RE::NiPoint3(xdes, ydes, player->GetPositionZ());

    }

    else if (zAngle <= 89.999f) {
        float xoffset = xdistance * std::cos(zAngle);
        float yoffset = xdistance * std::sin(zAngle);
        xdes = xpos + xoffset;
        ydes = ypos + yoffset;
 
        logger::info("[INFO]: NiPoint updated. zAngle: {}", zAngle);
        return RE::NiPoint3(xdes, ydes, player->GetPositionZ());
    }

    else if (zAngle <= 179.999f) {
        float xoffset = xdistance * std::sin(zAngle - 90);
        float yoffset = xdistance * std::cos(zAngle - 90);
        xdes = xpos + xoffset;
        ydes = ypos - yoffset;
 
        logger::info("[INFO]: NiPoint updated. zAngle: {}", zAngle);
        return RE::NiPoint3(xdes, ydes, player->GetPositionZ());
    }

    else if (zAngle <= 269.999f) {
        float xoffset = xdistance * std::cos(zAngle - 180);
        float yoffset = xdistance * std::sin(zAngle - 180);
        xdes = xpos - xoffset;
        ydes = ypos - yoffset;

        logger::info("[INFO]: NiPoint updated. zAngle: {}", zAngle);
        return RE::NiPoint3(xdes, ydes, player->GetPositionZ());
    }
		
	else if (zAngle <= 359.999f) {
    
		float xoffset = xdistance * std::sin(zAngle - 270);
        float yoffset = xdistance * std::cos(zAngle - 270);
        xdes = xpos - xoffset;
        ydes = ypos + yoffset;

        logger::info("[INFO]: NiPoint updated. zAngle: {}", zAngle);
        return RE::NiPoint3(xdes, ydes, player->GetPositionZ());

    } else {
        logger::info("[ERROR]: NiPoint update failed!");
        return RE::NiPoint3(player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());
        
    }
}
*/

// ->  -> INPUT
auto InputEventHandler::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*)
    -> EventResult {
    // logger::info("*InputEvent test. Obviously working.*");
    if (!a_event) {
        logger::info("*InputEvent returned...*");
        return EventResult::kContinue;
    }

    auto camera = RE::PlayerCamera::GetSingleton();
    if (camera) {
        auto& state = camera->currentState;
        if (state) {
            if (!m_inMenu) {
                m_camStateId = state->id;
            }
        }
    }

    auto Serana = RE::TESForm::LookupByID<RE::Actor>(0x02002b74);
    //auto player = RE::PlayerCharacter::GetSingleton();


    if (!Serana || !Serana->Is3DLoaded()) {
        return EventResult::kContinue;
    }

    auto ui = RE::UI::GetSingleton();

    if (!ui || !m_inMenu) {
        return EventResult::kContinue;
    }

    for (auto event = *a_event; event; event = event->next) {
        auto inputDevice = event->GetDevice();

        switch (event->GetEventType()) {
            case RE::INPUT_EVENT_TYPE::kButton: {
                auto buttonEvent = event->AsButtonEvent();

                if (!buttonEvent || !buttonEvent->IsHeld()) {
                    allowRotation = false;
                    allowZoom = false;
                    continue;
                }

                switch (inputDevice) {
                    case RE::INPUT_DEVICE::kGamepad: {
                    
                        const auto gamepadButton = static_cast<ControllerButton>(buttonEvent->GetIDCode());
                        
                        bUsingController = true;    // if Controller detected as input, to block trigger equip
                        
                        // enables controller triggers to zoom in/out
                        if (gamepadButton == 9) {
                            if (const auto playerCamera = RE::PlayerCamera::GetSingleton()) {
                                
                                if (bGlobalVar("SFF_MCM_EnablePreviewZoom")) {
                                    camera->worldFOV += 5 * fRotationAmount;
                                }

                                // enables controller triggers to free-move Serana
                                if (bGlobalVar("SFF_MCM_EnablePreviewMove") && camera->worldFOV <= 95.f) {
                                   
                                    RE::NiPoint3 newSeranaPos = RE::NiPoint3(
                                        Serana->GetPositionX() - ( (5 * fRotationAmount) * std::sin(Serana->GetAngleZ() ) ),
                                        Serana->GetPositionY() - ( (5 * fRotationAmount) * std::cos(Serana->GetAngleZ() ) ),
                                        Serana->GetPositionZ());
                                   
                                    Serana->SetPosition(newSeranaPos, false); 
                                    Serana->Update3DPosition(true);
                                }
                            }
                            continue;
                        
                        } else if (gamepadButton == 10) {
                            if (const auto playerCamera = RE::PlayerCamera::GetSingleton()) {
                                
                                if (bGlobalVar("SFF_MCM_EnablePreviewZoom")) {
                                    camera->worldFOV -= 5 * fRotationAmount;
                                }

                                // enables controller triggers to free-move Serana
                                if (bGlobalVar("SFF_MCM_EnablePreviewMove") && camera->worldFOV <= 95.f) {
                                     
                                    RE::NiPoint3 newSeranaPos = RE::NiPoint3(
                                        Serana->GetPositionX() + ( (5 * fRotationAmount) * std::sin(Serana->GetAngleZ() ) ),
                                        Serana->GetPositionY() + ( (5 * fRotationAmount) * std::cos(Serana->GetAngleZ() ) ),
                                        Serana->GetPositionZ());
                                   
                                    Serana->SetPosition(newSeranaPos, false); 
                                    Serana->Update3DPosition(true);
                                }
                            }
                            continue;
                        }
                    } break;

                    case RE::INPUT_DEVICE::kMouse:

                        bUsingController = false;   // if not using Controller, disable trigger equip block (as it also blocks mouse-click equip)

                        if (const auto mouseButton = static_cast<MouseButton>(buttonEvent->GetIDCode());
                            mouseButton == (257 - 0x100)) {
                            allowRotation = true;
                            continue;

                        } else if (mouseButton == 0){   // left mouse button
                            if (!bBlockFeature)         // should only be true if previewing armour items; any other items, zooming disabled
                                allowZoom = true;   
                            
                            continue;
                        }                       

                        break;
                }
            }
                continue;

            case RE::INPUT_EVENT_TYPE::kMouseMove: {
                
                if (allowRotation) {
                    auto mouseEvent = reinterpret_cast<RE::MouseMoveEvent*>(event->AsIDEvent());
                    if (abs(mouseEvent->mouseInputX) < fTurnSensitivity) continue;
                    if (const auto playerCamera = RE::PlayerCamera::GetSingleton()) {
                        auto thirdPersonState = static_cast<RE::ThirdPersonState*>(playerCamera->currentState.get());

                      
                        Serana->SetRotationZ(Serana->data.angle.z + ((mouseEvent->mouseInputX > 0 ? -1 : 1) * fRotationAmount));
                        thirdPersonState->freeRotation.x -= ((mouseEvent->mouseInputX > 0 ? -1 : 1) * fRotationAmount);
                        Serana->Update3DPosition(true);
     
                    }
                }
                if (allowZoom) {
                    auto mouseEvent = reinterpret_cast<RE::MouseMoveEvent*>(event->AsIDEvent());
                    if (abs(mouseEvent->mouseInputX) < fTurnSensitivity) continue;
                    if (const auto playerCamera = RE::PlayerCamera::GetSingleton()) {
                        
                        if (bGlobalVar("SFF_MCM_EnablePreviewZoom")) {
                            camera->worldFOV = camera->worldFOV + ((mouseEvent->mouseInputX > 0 ? -1 : 1) * (fRotationAmount * 5));
                        }
                        
                        if (bGlobalVar("SFF_MCM_EnablePreviewMove")) {
                            RE::NiPoint3 newSeranaPos = RE::NiPoint3(
                                Serana->GetPositionX() - ( ((5 * (fRotationAmount*2)) * (mouseEvent->mouseInputX > 0 ? -1 : 1)) * std::sin(Serana->GetAngleZ()) ),
                                Serana->GetPositionY() - ( ((5 * (fRotationAmount*2)) * (mouseEvent->mouseInputX > 0 ? -1 : 1)) * std::cos(Serana->GetAngleZ()) ),
                                Serana->GetPositionZ());

                            Serana->SetPosition(newSeranaPos, false);
                            Serana->Update3DPosition(true);
                        }

                    }

                }
            }
                continue;
            case RE::INPUT_EVENT_TYPE::kThumbstick: {
                //  disable animcam before moving in menu
                if (camera && m_shouldDisableAnimCam && !ui->GameIsPaused()) {
                    auto idEvent = static_cast<RE::ButtonEvent*>(event);
                    auto userEvents = RE::UserEvents::GetSingleton();
                    if (idEvent->userEvent == userEvents->move) {
                        auto thirdPersonState = static_cast<RE::ThirdPersonState*>(camera->currentState.get());

                        thirdPersonState->toggleAnimCam = false;
         
                        // turning off AnimCam messes with zoom for some reason, so modify it again
                        if (m_fixCameraZoom) {
                            fVanityModeMinDist->data.f -= (17.0f + (66.6f * MenuOpenCloseEventHandler::fPitch));
                            fVanityModeMaxDist->data.f -= (17.0f + (66.6f * MenuOpenCloseEventHandler::fPitch));
                            m_fixCameraZoom = false;
                        }

                        m_shouldDisableAnimCam = false;
                    }
                }
                
                // if Player using controller, rotating thumbsticks AND previewing armour item,
                // enable rotation.
                if (MenuOpenCloseEventHandler::iGamepadTurnMethod == 0 && MenuOpenCloseEventHandler::bGamepadRotating && !bBlockFeature) {
                    auto stickEvent = reinterpret_cast<RE::ThumbstickEvent*>(event->AsIDEvent());
                    auto playerCamera = RE::PlayerCamera::GetSingleton();

                    if (stickEvent && playerCamera && stickEvent->IsRight()) {
                        auto thirdPersonState = static_cast<RE::ThirdPersonState*>(playerCamera->currentState.get());
                        if (abs(stickEvent->yValue) > (2 * fTurnSensitivity)) continue;
                        
                        
                        Serana->SetRotationZ(Serana->data.angle.z + ((stickEvent->xValue > 0 ? -1 : 1) * fRotationAmount));
                        thirdPersonState->freeRotation.x -= ((stickEvent->xValue > 0 ? -1 : 1) * fRotationAmount);
                        Serana->Update3DPosition(true);

                    }

                }
                
            } break;
        }
    }

    return EventResult::kContinue;
}


// [PILLAR FUNCTION]
auto MenuOpenCloseEventHandler::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) -> EventResult {
    
    if (!bSeranaActivated) {
        MenuOpenCloseEventHandler::bEnable3DManager = false;    // no need to disable 3D item preview if Serana not found!
        return EventResult::kContinue;
    }
    
    //auto Serana = RE::TESForm::LookupByID<RE::Actor>(0x02002b74);
    auto SFF_MCM_EnablePreviewer = RE::TESForm::LookupByEditorID<RE::TESGlobal>("SFF_MCM_OutfitPrev");

    if (SFF_MCM_EnablePreviewer && SFF_MCM_EnablePreviewer->value > 0) {
        //logger::info("[INFO]: Outfit Preview feature toggled ON!");
    } else {
        logger::info("[INFO][WARNING]: Outfit Preview feature toggled OFF!");
        MenuOpenCloseEventHandler::bEnable3DManager = false;  // no need to disable 3D item preview if Serana not found!
        return EventResult::kContinue;
    }

 //   if (bUsingTorch()) {
 //       logger::info("Serana has something equipped on Left Hand. Temporarilly skip Previewer feature...");
 //       MenuOpenCloseEventHandler::bEnable3DManager = false;  
 //       bLeftHandFull = true;
 //       return EventResult::kContinue;
 //   } else {
 //       logger::info("Serana Left Hand empty! Proceed 1.");
 //       if (bLeftHandFull) {
 //           logger::info("Serana Left Hand empty, but flag not cleared. Abort...");
 //           return EventResult::kContinue;
 //       }
 //   }

    auto Serana = RE::TESForm::LookupByID<RE::Actor>(0x02002b74);

    RE::TESForm* handItemL = Serana->GetEquippedObject(true);  // sff v1.7.1 
    RE::FormID torchID = 120044;

    //auto itemID = handItemL->GetFormID();
   // auto itemName = handItemL->GetName();
    RE::FormID itemID;
    if (handItemL) 
        itemID = handItemL->GetFormID();

    if (bSSinstalled && handItemL && itemID && itemID == torchID) {
                
        logger::info("Serana has torch equipped on Left Hand. Temporarilly skip Previewer feature...");
        MenuOpenCloseEventHandler::bEnable3DManager = false;
        bLeftHandFull = true;
        return EventResult::kContinue;
        
    } else {

        if (bLeftHandFull) {    //1st failsafe mech.
            logger::info("Serana not using torch, but flag not cleared. Abort...");
            return EventResult::kContinue;
        }
    }

    auto uiStr = RE::InterfaceStrings::GetSingleton();
    if (uiStr) {
        auto& name = a_event->menuName;

        if (name == uiStr->containerMenu) {

            if (a_event->opening) {
                m_inMenu = true;
                OnInventoryOpen();
                MenuOpenCloseEventHandler::bEnable3DManager = true;

            } else if (!a_event->opening) {
                if (!bLeftHandFull) { //2nd fail-safe mech.
                    OnInventoryClose();
                }
                MenuOpenCloseEventHandler::bEnable3DManager = false;
            }

        }
    }
    return EventResult::kContinue;
}

// [MENU *OPEN* MENU]
void MenuOpenCloseEventHandler::OnInventoryOpen() {

    auto camera = RE::PlayerCamera::GetSingleton();

    if (!camera) {
        return;
    }

    auto Serana = RE::TESForm::LookupByID<RE::Actor>(0x02002b74);

    if (Serana) {
        m_playerRotation = Serana->data.angle.z;

        RotateSerana();    // [1st] get Serana to Player's pos. & rot.
        RotateCamera();    // [2nd] rotate camera to face Serana
        if (!bSSinstalled) {
            MovePlayer();   //[3rd] move player if SkyrimSouls not installed (to avoid physiscs issues). Canna move while menu unpaused, else Player char. dies.
        }
    }
}


// [*OPEN*] [1st]: Serana rotation
void MenuOpenCloseEventHandler::RotateSerana() {
    auto Serana = RE::TESForm::LookupByID<RE::Actor>(0x02002b74);
    auto myPlayer = RE::PlayerCharacter::GetSingleton();  // actual Player character.

    if (!Serana) {
        // logger::info("Serana not found as Actor");
        return;
    }
    
    // cache position & rot.
    currentCharacterYaw =
        Serana->data.angle.z;        // Serana's cur. Z angle. Use this when exiting menu to restore Serana's angle
    prePos = Serana->GetPosition();  // Serana's cur. pos. Use this when exiting menu to restore pos.

    // SERANA position properties
    auto seranaPos = Serana->GetPosition();
    RE::NiPoint3 cameraPos = GetCameraPos();

    // PLAYER rotation properties
    float curCameraYaw = myPlayer->data.angle.z;          // Player angle and camera angle *should* be the same (?)
    RE::NiPoint3 curPlayerPos = myPlayer->GetPosition();  // Grabs actual Player pos., so we can set Serana to it

    Serana->SetRotationZ(curCameraYaw);        // sets Serana's Z angle to same as Player's
    Serana->SetPosition(curPlayerPos, false);  // sets Serana's Pos. to same as Player's

    m_rotatedPlayer = true;

    Serana->Update3DPosition(true);

    // SFF v1.7.1 - minimise Player char.: if not minimised, Player character will interfere with camera/rotation
    if (bSSinstalled) {
        TESObjectREFR_SetScale(myPlayer, 0.0);  // minimise Player char. to hide Player and avoid physics obstruction
    }
}


// [*OPEN*] [2nd]: Camera rotation
void MenuOpenCloseEventHandler::RotateCamera() {
    auto camera = RE::PlayerCamera::GetSingleton();
    auto ini = RE::INISettingCollection::GetSingleton();
    auto Serana = RE::TESForm::LookupByID<RE::Actor>(0x02002b74);
    auto myPlayer = RE::PlayerCharacter::GetSingleton();  // actual Player character.

    auto thirdState = (RE::ThirdPersonState*)camera->cameraStates[RE::CameraState::kThirdPerson].get();
    auto mod = RE::TESForm::LookupByID<RE::TESImageSpaceModifier>(0x000434BB);  // Image Space Adapter - ISMTween

    // collect original values for later
    m_playerAngleX = Serana->data.angle.x;  // unused?
    m_freeRotation = thirdState->freeRotation;
    g_freeRotation = m_freeRotation;
    m_posOffsetExpected = thirdState->posOffsetExpected;
    m_blurRadius = mod->blurRadius->floatValue;
    
    // sff v1.7.1 - store camera position & rotation so we can retun to it later
    originalCamPosX = camera->pos.x;
    originalCamPosY = camera->pos.y;
    originalCamPosZ = camera->pos.z;
    originalCamYaw = camera->yaw;

    if (m_camStateId == RE::CameraState::kFirstPerson) {
        m_thirdForced = true;
        camera->SetState(thirdState);
    } 

    camera->cameraTarget = Serana;  // v1.6.0 ~ center camera on Serana before rotating it (so camera changes relative to Serana)

    // set over the shoulder camera values for when player has weapon drawn and unpaused menu(s) in order to prevent
    // camera from snapping
    fOverShoulderCombatPosX = ini->GetSetting("fOverShoulderCombatPosX:Camera");
    fOverShoulderCombatAddY = ini->GetSetting("fOverShoulderCombatAddY:Camera");
    fOverShoulderCombatPosZ = ini->GetSetting("fOverShoulderCombatPosZ:Camera");
    fAutoVanityModeDelay = ini->GetSetting("fAutoVanityModeDelay:Camera");
    fOverShoulderPosX = ini->GetSetting("fOverShoulderPosX:Camera");
    fOverShoulderPosZ = ini->GetSetting("fOverShoulderPosZ:Camera");
    fVanityModeMinDist = ini->GetSetting("fVanityModeMinDist:Camera");
    fVanityModeMaxDist = ini->GetSetting("fVanityModeMaxDist:Camera");
    fMouseWheelZoomSpeed = ini->GetSetting("fMouseWheelZoomSpeed:Camera");
    m_fOverShoulderCombatPosX = fOverShoulderCombatPosX->GetFloat();
    m_fOverShoulderCombatAddY = fOverShoulderCombatAddY->GetFloat();
    m_fOverShoulderCombatPosZ = fOverShoulderCombatPosZ->GetFloat();
    m_fOverShoulderPosX = fOverShoulderPosX->GetFloat();
    m_fOverShoulderPosZ = fOverShoulderPosZ->GetFloat();
    m_fAutoVanityModeDelay = fAutoVanityModeDelay->GetFloat();
    m_fVanityModeMinDist = fVanityModeMinDist->GetFloat();
    m_fVanityModeMaxDist = fVanityModeMaxDist->GetFloat();
    m_fMouseWheelZoomSpeed = fMouseWheelZoomSpeed->GetFloat();
    m_worldFOV = camera->worldFOV;
    Serana->GetGraphVariableBool("IsNPC", m_playerHeadtrackingEnabled);

    // disable blur before opening menu so character is not obscured
    mod->radialBlur.strength = 0;
    mod->blurRadius->floatValue = 0;

    // temporarily disable headtracking, if enabled
    Serana->SetGraphVariableBool("IsNPC", false);
    myPlayer->SetGraphVariableBool("IsNPC", false); 

    // toggle anim cam which unshackles camera and lets it move in front of player with their weapon drawn, necessary if
    // not using TDM
    thirdState->toggleAnimCam = true;
    thirdState->freeRotationEnabled = true;

    fAutoVanityModeDelay->data.f = 10800.0f;  // 3 hours

    // make these values externally modifiable in the future
    m_fNewOverShoulderCombatPosX = -fXOffset - 75.0f;       // right-left (- to +)
   // if (bLeftHandFull) {
   //     m_fNewOverShoulderCombatAddY = -20.5f;
   // } else {
        m_fNewOverShoulderCombatAddY = -10.5f;  // back-front (- to +) (originally 0.f)
   // }
    m_fNewOverShoulderCombatPosZ = fZOffset - 50.0f;

    thirdState->freeRotation.x = MATH_PI + fRotation - 0.5f;

    // account for camera freeRotation settings getting pushed into player's pitch (x) values when weapon drawn
    thirdState->freeRotation.y = 25.0f;

    fOverShoulderCombatPosX->data.f = m_fNewOverShoulderCombatPosX;
    fOverShoulderCombatAddY->data.f = m_fNewOverShoulderCombatAddY;
    fOverShoulderCombatPosZ->data.f = m_fNewOverShoulderCombatPosZ;

    fOverShoulderPosX->data.f = m_fNewOverShoulderCombatPosX;
    fOverShoulderPosZ->data.f = m_fNewOverShoulderCombatPosZ;

    fVanityModeMinDist->data.f = 155.0f - fYOffset;
    fVanityModeMaxDist->data.f = 155.0f - fYOffset;

    // Skyrim Souls RE has the potential to mess with camera distance in menus after exiting, so when we restore it
    // later on, make the transition suitably instant
    fMouseWheelZoomSpeed->data.f = 10000.0f;

    // make final camera distance not depend on camera pitch (looking down at player)
   // thirdState->pitchZoomOffset = 0.1f;

    thirdState->posOffsetExpected = thirdState->posOffsetActual =
        RE::NiPoint3(m_fNewOverShoulderCombatPosX, m_fNewOverShoulderCombatAddY, m_fNewOverShoulderCombatPosZ);
    
    camera->worldFOV = 90.f;
    
    camera->Update();
    camera->ToggleFreeCameraMode(false);
}

// [*OPEN*] [3rd]: Move Player
void MenuOpenCloseEventHandler::MovePlayer() {
    auto myPlayer = RE::PlayerCharacter::GetSingleton();  // actual Player character.

    auto myPosX = myPlayer->GetPositionX();
    auto myPosY = myPlayer->GetPositionY();
    auto myPosZ = myPlayer->GetPositionZ();

    RE::NiPoint3 newPos = RE::NiPoint3(myPosX + fHideOffset, myPosY + fHideOffset, myPosZ + fHideOffset);

    myPlayer->SetPosition(newPos, false);
    myPlayer->Update3DPosition(true);
    bPlayerHidden = true;
}

// [*OPEN*] [4th]: Enable Lights
// canna get ref. from an item not loaded in same cell as Player, it seems. 
// no chance of getting our Light ref. then, as it resides in another cell.
//void MenuOpenCloseEventHandler::EnableLight() {
//    auto SFF_MCM_EnablePreviewLight = RE::TESForm::LookupByEditorID<RE::TESGlobal>("SFF_MCM_EnablePreviewLight");
   // auto PreviewLight = RE::TESForm::LookupByEditorID<RE::TESObjectREFR>("SFF_Previewer_MagicLight");
//    auto PreviewLight = RE::TESForm::LookupByID<RE::TESObjectREFR>(4376880);
//    auto Serana = RE::TESForm::LookupByID<RE::Actor>(0x02002b74);
//
//    if (SFF_MCM_EnablePreviewLight) {
//        logger::info("[INFO] Global found. Its value is: {}", SFF_MCM_EnablePreviewLight->value);
//    } else {
//        logger::info("[ERROR] Global not found...");  
//        return;
//    }
//
//    if (!PreviewLight) {
//        logger::info("LIGHT NOT FOUND!");
//        return;
//    }
//
//    if (SFF_MCM_EnablePreviewLight->value == 1) {
//       cachePos = PreviewLight->GetPosition();
//
//        PreviewLight->SetPosition(35.f * std::sin(Serana->GetAngleZ()), 35.f * std::cos(Serana->GetAngleZ()), (Serana->GetHeight() - 1));
//    }
//}

// [MENU *CLOSE* MENU]
void MenuOpenCloseEventHandler::OnInventoryClose() {

    m_inMenu = false;

    // 3rd failsafe mech.
    if (bLeftHandFull) {
        logger::info("Serana torch flag not yet cleared. Abort...");
        return;
    }

    ResetSeranaPos();
    ResetCamera();
    if (!bSSinstalled) {
        ReturnPlayer();  //[3rd] if SkyrimSouls not installed (to avoid physiscs issues)
    }
}

// [*CLOSE*] [1st]: reset Serana pos.
void MenuOpenCloseEventHandler::ResetSeranaPos() {
    auto Serana = RE::TESForm::LookupByID<RE::Actor>(0x02002B74);
    auto myPlayer = RE::PlayerCharacter::GetSingleton();
    if (bSSinstalled) {

        TESObjectREFR_SetScale(myPlayer, 1.0);
    }

    if (currentCharacterYaw) {
        Serana->SetRotationZ(currentCharacterYaw);  // restores Serana's original rotation (pre menu)
        Serana->SetPosition(prePos, false);         // restores Serana's original pos. (pre menu)
        Serana->Update3DPosition(true);             // update her position so she actually appears in the given coordinates
        currentCharacterYaw = NULL;
    }

}

// [*CLOSE*] [2nd]: reset camera
void MenuOpenCloseEventHandler::ResetCamera() {
    auto player = RE::TESForm::LookupByID<RE::Actor>(0x02002b74);
    auto myPlayer = RE::PlayerCharacter::GetSingleton();

    auto camera = RE::PlayerCamera::GetSingleton();
    auto thirdState = (RE::ThirdPersonState*)camera->cameraStates[RE::CameraState::kThirdPerson].get();
    auto mod = RE::TESForm::LookupByID<RE::TESImageSpaceModifier>(0x000434BB);

    // restore original values
    player->data.angle.x = m_playerAngleX;
    player->data.angle.z = m_playerRotation;
    fAutoVanityModeDelay->data.f = m_fAutoVanityModeDelay;
    thirdState->toggleAnimCam = false;
    thirdState->targetZoomOffset = m_targetZoomOffset;
    thirdState->freeRotation = m_freeRotation;
    fVanityModeMinDist->data.f = m_fVanityModeMinDist;
    fVanityModeMaxDist->data.f = m_fVanityModeMaxDist;
    camera->worldFOV = m_worldFOV;
    thirdState->posOffsetExpected = thirdState->posOffsetActual = m_posOffsetExpected;
    fOverShoulderCombatPosX->data.f = m_fOverShoulderCombatPosX;
    fOverShoulderCombatAddY->data.f = m_fOverShoulderCombatAddY;
    fOverShoulderCombatPosZ->data.f = m_fOverShoulderCombatPosZ;
    fOverShoulderPosX->data.f = m_fOverShoulderPosX;
    fOverShoulderPosZ->data.f = m_fOverShoulderPosZ;
    mod->blurRadius->floatValue = m_blurRadius;

    // SFF v1.7.1 - return camera to correct pos.
    camera->pos.x = originalCamPosX;
    camera->pos.y = originalCamPosY;
    camera->pos.z = originalCamPosZ;
    camera->yaw = originalCamYaw;

    player->SetGraphVariableBool("IsNPC", m_playerHeadtrackingEnabled);
    myPlayer->SetGraphVariableBool("IsNPC", m_playerHeadtrackingEnabled);

    if (m_thirdForced) {
        const auto firstPersonState =
            static_cast<RE::FirstPersonState*>(camera->cameraStates[RE::CameraState::kFirstPerson].get());
        camera->SetState(firstPersonState);
    }
    m_thirdForced = false;

    camera->cameraTarget = myPlayer;  // v1.6.0 ~ center camera back on Player

    if (camera->IsInFreeCameraMode()) {
        camera->ToggleFreeCameraMode(false);
    }
    camera->Update();

    // camera->Update() function uses this value, so restore it after we've updated the camera
    fMouseWheelZoomSpeed->data.f = m_fMouseWheelZoomSpeed;

    m_rotatedPlayer = false;
    m_inSitState = false;
}

// [*CLOSE*] [3rd]: return Player
void MenuOpenCloseEventHandler::ReturnPlayer() {
    if (bPlayerHidden) {
        auto myPlayer = RE::PlayerCharacter::GetSingleton();  // actual Player character.

        auto myPosX = myPlayer->GetPositionX();
        auto myPosY = myPlayer->GetPositionY();
        auto myPosZ = myPlayer->GetPositionZ();

        RE::NiPoint3 newPos = RE::NiPoint3(myPosX - fHideOffset, myPosY - fHideOffset, myPosZ - fHideOffset);

        myPlayer->SetPosition(newPos, false);
        myPlayer->Update3DPosition(true);
        bPlayerHidden = false;
    }
}

//void MenuOpenCloseEventHandler::DisableLight() {
//    auto SFF_MCM_EnablePreviewLight = RE::TESForm::LookupByEditorID<RE::TESGlobal>("SFF_MCM_EnablePreviewLight");
   // auto PreviewLight = RE::TESForm::LookupByID<RE::TESObjectREFR>(0x04376881);
//    auto PreviewLight = RE::TESForm::LookupByEditorID<RE::TESObjectREFR>("SFF_Previewer_MagicLight");
    //auto PreviewLight = RE::TESForm::LookupByID<RE::TESObjectREFR>(4376881);
//
//    if (SFF_MCM_EnablePreviewLight) {
        //logger::info("[INFO] Global found. Its value is: {}", SFF_MCM_EnablePreviewLight->value);
//    } else {
//        logger::info("[ERROR] Global not found...");
//        return;
//    }
//
//    if (!PreviewLight) return;
//
//    if (SFF_MCM_EnablePreviewLight->value == 1) {
        //cachePos = PreviewLight->GetPosition();
//        
 //       PreviewLight->SetPosition(cachePos);  // returns Preview Light to its original position (CommonLibSSE has no 'Enable()' function). 
//    }
//}


RE::BSEventNotifyControl TriggerBlocker::ProcessEvent_Hook(RE::InputEvent** a_event, RE::BSTEventSource<RE::InputEvent*>* a_source) {
    
    // Disable item equipping when zooming in or out with controller
    if (a_event && *a_event) {
        
        if (!bUsingController) {
            // SFF v1.7.1 ~ if Player not using controller, no need to run this code, as it can cause mouse click to stop registering
            return _ProcessEvent(this, a_event, a_source);
        }
        
        for (RE::InputEvent* evn = *a_event; evn; evn = evn->next) {
            if (evn && evn->HasIDCode()) {
                
                RE::UserEvents* userEvents = RE::UserEvents::GetSingleton();
                RE::IDEvent* idEvent = static_cast<RE::ButtonEvent*>(evn);

                // only disable triggers if Serana has been activated by Player
                if (bSeranaActivated && idEvent) {
                    if (idEvent->userEvent == userEvents->rightEquip || idEvent->userEvent == userEvents->leftEquip)
                        idEvent->userEvent = "";
                }
            }
        }
    }

    return _ProcessEvent(this, a_event, a_source);
}

void TriggerBlocker::InstallHook() {
    REL::Relocation<std::uintptr_t> MenuControlsVtbl{RE::VTABLE_MenuControls[0]};
    _ProcessEvent = MenuControlsVtbl.write_vfunc(0x1, &ProcessEvent_Hook);
    logger::info("MenuControl hook registered");
}
