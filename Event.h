#pragma once
#include <RE/N/NiFloatInterpolator.h>
#include <functional>
#include <stdint.h>
#include <C:\SKSEDev\SKSE_SFF\PCH.h>

#ifdef _MSC_VER
    #undef GetObject
#endif


namespace {

	bool bSSinstalled;

	bool bBlockFeature;
    bool bTriggersTriggered;
    bool bUsingController= false;
    float fHideOffset = -2000;
    bool bPlayerHidden = false;

	bool bLeftHandFull = true;	//sff v1.7.1
    //const char* torchName = "Torch";

	// Properties: (sff v1.7.1: moved from 'plugin.cpp' body)
    float currentCharacterYaw = NULL;  // stores Serana's rot./angle BEFORE opening Inventory menu.
    RE::NiPoint3 prePos;               // stores Serana's pos. BEFORE opening Inventory menu.
    //RE::NiPoint3 cachePos;               // stores Preview Light pos. BEFORE opening Inventory menu.

	// CommonLibSSE, no native 'SetScale()' func.;
	// (code courtesy of Doodlezoid (https://github.com/doodlum/skyrim-actor-scale/blob/main/src/XSEPlugin.cpp))
	void TESObjectREFR_SetScale(RE::TESObjectREFR* a_ref, float a_scale) {
			using func_t = decltype(&TESObjectREFR_SetScale);
			REL::Relocation<func_t> func{REL::RelocationID(19239, 19665)};
			func(a_ref, a_scale);
	}

	bool bGlobalVar(std::string_view myGV) {
			auto currentGV = RE::TESForm::LookupByEditorID<RE::TESGlobal>(myGV);

			if (!currentGV) {
				logger::info("[ERROR]: '{}' Global Var. could not be found...", myGV);
				return false;
			}

			if (currentGV->value == 1) {
				return true;
			}
			return false;
	}

}

class ActivationEventHandler : public RE::BSTEventSink<RE::TESActivateEvent>
{
public:
	using EventResult = RE::BSEventNotifyControl;

	static ActivationEventHandler* GetSingleton() 
	{ 
		static ActivationEventHandler singleton;
		return std::addressof(singleton);
	}
	
	static void Enable() {

		auto ui = RE::ScriptEventSourceHolder::GetSingleton();
		if (ui) {
			ui->AddEventSink(GetSingleton());
			logger::info("ActivationEvent successfully registered.");
		} else {
			logger::info("MenuEvent NOT REGISTERED!");
		}
	}

	static void Disable() { 
		auto ui = RE::ScriptEventSourceHolder::GetSingleton();
		if (ui) {
			ui->RemoveEventSink(GetSingleton());
		}

	}


	//fill in list of possible voids

	virtual EventResult ProcessEvent(const RE::TESActivateEvent* a_event, RE::BSTEventSource<RE::TESActivateEvent>*) override;

private:
	ActivationEventHandler() = default;
	ActivationEventHandler(const ActivationEventHandler&) = delete;
	ActivationEventHandler(ActivationEventHandler&&) = delete;

	inline ~ActivationEventHandler() { Disable(); }

	ActivationEventHandler& operator=(const ActivationEventHandler&) = delete;
	ActivationEventHandler& operator=(ActivationEventHandler&&) = delete;
};

class MenuOpenCloseEventHandler : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
	using EventResult = RE::BSEventNotifyControl;

	static MenuOpenCloseEventHandler* GetSingleton()
	{
		static MenuOpenCloseEventHandler singleton;
		return std::addressof(singleton);
	}

	static void Enable()
	{
		auto ui = RE::UI::GetSingleton();
		if (ui) {
			ui->AddEventSink(GetSingleton());
			logger::info("MenuEvent successfully registered.");
		} else {
			logger::info("MenuEvent NOT REGISTERED!");
		}
	}
	
	static void Disable()
	{
		auto ui = RE::UI::GetSingleton();
		if (ui) {
			ui->RemoveEventSink(GetSingleton());
		}
	}

	void RotateCamera();
	void RotateSerana();
	void ResetCamera();
	void ResetSeranaPos();

	void OnInventoryOpen();
	void OnInventoryClose();

	void MovePlayer(); // if not using SkyrimSoulsRE, use traditional method (physics bugs if not!)
    void ReturnPlayer();

	//void EnableLight();
    //void DisableLight();


	static inline bool bEnable3DManager = false;
	static inline bool bEnableInInventoryMenu = true;
	static inline bool bEnableInContainerMenu = false;
	static inline bool bEnableInBarterMenu = false;
	static inline bool bEnableInMagicMenu = false;
	static inline bool bEnableInTweenMenu = false;
	static inline bool bRotateCamera = false;
	static inline bool bRotatePlayer = true;
	static inline bool bEnableCombat = false;
	static inline bool bEnableFirstPerson = true;
	static inline bool bEnableMoving = true;
	static inline bool bEnableSitting = false;
	static inline bool bEnableAutoMoving = false;
	static inline bool bAltCamSwitchTarget = false;
	static inline float fXOffset = 0.0f;
	static inline float fYOffset = 0.0f;
	static inline float fZOffset = 0.0f;
	static inline float fPitch = 0.2f;
	static inline float fRotation = 0.0f;
	static inline bool bGamepadRotating = true;
	static inline uint32_t iGamepadTurnMethod = 0;

	//static inline float fPlayerHeight;
    static inline RE::TESRace *myRace;
    static inline float originalCamPosX;
    static inline float originalCamPosY;
    static inline float originalCamPosZ;
    static inline float originalCamYaw;
	virtual EventResult ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

private:
	MenuOpenCloseEventHandler() = default;
	MenuOpenCloseEventHandler(const MenuOpenCloseEventHandler&) = delete;
	MenuOpenCloseEventHandler(MenuOpenCloseEventHandler&&) = delete;

	inline ~MenuOpenCloseEventHandler() { Disable(); }

	MenuOpenCloseEventHandler& operator=(const MenuOpenCloseEventHandler&) = delete;
	MenuOpenCloseEventHandler& operator=(MenuOpenCloseEventHandler&&) = delete;

	 bool									m_bDoRadialBlur;
	 bool									m_playerHeadtrackingEnabled;
	 float									m_playerAngleX;
	 float									m_targetZoomOffset;
	 float									m_fOverShoulderPosX;
	 float									m_fOverShoulderPosZ;
	 float									m_fOverShoulderCombatPosX;
	 float									m_fOverShoulderCombatAddY;
	 float									m_fOverShoulderCombatPosZ;
	 float									m_fNewOverShoulderCombatPosX;
	 float									m_fNewOverShoulderCombatAddY;
	 float									m_fNewOverShoulderCombatPosZ;
	 float									m_fAutoVanityModeDelay;
	 float									m_fVanityModeMinDist;
	 float									m_fVanityModeMaxDist;
	 float									m_fMouseWheelZoomSpeed;
	 float									m_worldFOV;
	 RE::NiPoint2							m_freeRotation;
	 RE::NiPoint3							m_posOffsetExpected;
	// RE::NiPointer<RE::NiFloatInterpolator> m_radialBlurStrength;
	 float									m_blurRadius;
};


class InputEventHandler : public RE::BSTEventSink<RE::InputEvent*>
{
	 bool allowRotation;
     bool allowZoom;
    // int iZoomMult = 3;

 public:
	 using EventResult = RE::BSEventNotifyControl;

	 static InputEventHandler* GetSingleton()
	 {
		static InputEventHandler singleton;
		return std::addressof(singleton);
	 }
	 

	 
	 static void Enable()
	 {
		auto ui = RE::BSInputDeviceManager::GetSingleton();
		if (ui) {
			ui->AddEventSink(GetSingleton());
			logger::info("InputEvent successfully registered.");
		} else {
			logger::info("InputEvent NOT REGISTERED!");
		}
	 }

	 static void Disable()
	 {
		auto ui = RE::BSInputDeviceManager::GetSingleton();
		if (ui) {
			ui->RemoveEventSink(GetSingleton());
		}
	 }

	 virtual EventResult ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override;

 private:
	 InputEventHandler() = default;
	 InputEventHandler(const InputEventHandler&) = delete;
	 InputEventHandler(InputEventHandler&&) = delete;

	 inline ~InputEventHandler() { Disable(); }

	 InputEventHandler& operator=(const InputEventHandler&) = delete;
	 InputEventHandler& operator=(InputEventHandler&&) = delete;
};


class DisableArmor3D
{
 public:

	 // called from 'plugin.cpp'
	 static void InstallHook()
	 {
        REL::Relocation<std::uintptr_t> func{REL::VariantID(50884, 51757, 50884)};  // UpdateItem3D
		auto& trampoline = SKSE::GetTrampoline();
		trampoline.write_branch<5>(func.address(), &UpdateItem3D);
		logger::info("Item3D disabler event successfully registered.");
		
	 }

 void UpdateItem3D(RE::InventoryEntryData* a_objDesc)
	 {
        if (!bGlobalVar("SFF_MCM_OutfitPrev")) {
            return;
		}
        
		if (!bGlobalVar("SFF_MCM_DisablePreview3D")) {
            return;
		}
		
		if (!a_objDesc) {
			logger::info("ERROR 01");
			return;
		}

		auto obj = a_objDesc->object;
		if (!obj) {
			logger::info("ERROR 02");
			return;
		}


		auto manager = RE::Inventory3DManager::GetSingleton();

		if (MenuOpenCloseEventHandler::bEnable3DManager) {
			//logger::info("[ITEM 3D] PLAYER IN MENU");
			switch (obj->GetFormType()) {

				case RE::FormType::AlchemyItem:
					manager->UpdateMagic3D(obj, 0);
                    bBlockFeature = true;
					break;

				case RE::FormType::Armor:
					manager->Clear3D();			// 3D item preview should be disabled
                    bBlockFeature = false;		// Player should be able to zoom in camera and rotate Serana
					break;
			

				case RE::FormType::Ammo:
					manager->UpdateMagic3D(obj, 0);
                    bBlockFeature = true;
					break;

				case RE::FormType::Book:
                    bBlockFeature = true;
                    

				case RE::FormType::Scroll:
					manager->UpdateMagic3D(obj, 0);
                    bBlockFeature = true;
					break;

				case RE::FormType::Light:
                    bBlockFeature = true;
                    

				case RE::FormType::Misc:
					manager->UpdateMagic3D(obj, 0);
                    bBlockFeature = true;
					break;

				case RE::FormType::Weapon:
					manager->UpdateMagic3D(obj, 0);
                    bBlockFeature = true;
					break;

				default:
                    manager->UpdateMagic3D(obj, 0);
                    bBlockFeature = false;
					break;

			}

		} else {
			//logger::info("[ITEM 3D] PLAYER not in MENU");
			manager->UpdateMagic3D(obj, 0);
            bBlockFeature = false;
		}
	 }
};


class TriggerBlocker : public RE::MenuControls
{
 public:
	 RE::BSEventNotifyControl ProcessEvent_Hook(RE::InputEvent** a_event, RE::BSTEventSource<RE::InputEvent*>* a_source);

	 static void InstallHook();

	 using ProcessEvent_t = decltype(static_cast<RE::BSEventNotifyControl (RE::MenuControls::*)(RE::InputEvent* const*, RE::BSTEventSource<RE::InputEvent*>*)>(&RE::MenuControls::ProcessEvent));
	 static inline REL::Relocation<ProcessEvent_t> _ProcessEvent;
};
