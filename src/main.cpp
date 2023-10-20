#include "Settings.h"

namespace Handler
{
	RE::CameraState lastDrawCameraState;
	RE::CameraState lastAimCameraState;

	float lastZoomOffset;

	namespace detail
	{
		RE::CameraState QCameraState()
		{
			return *stl::adjust_pointer<RE::CameraState>(RE::PlayerCamera::GetSingleton()->currentState, 0x50);
		}

		bool QInThirdPerson()
		{
			return QCameraState() == RE::CameraState::kThirdPerson;
		}

		bool QInFirstPerson()
		{
			return QCameraState() == RE::CameraState::kFirstPerson;
		}
	}

	void TrySetFirstPerson(RE::CameraState& a_lastCameraState)
	{
		a_lastCameraState = detail::QCameraState();
	    if (detail::QInThirdPerson()) {
		    RE::PlayerCamera::GetSingleton()->ForceFirstPerson();
		}
	}

	void TrySetThirdPerson(const RE::CameraState a_lastCameraState)
	{
		if (detail::QInFirstPerson() && (!Settings::GetSingleton()->rememberLastCameraState || a_lastCameraState == RE::CameraState::kThirdPerson)) {
			RE::PlayerCamera::GetSingleton()->ForceThirdPerson();
		}
	}

	struct WeaponDraw
	{
		static bool thunk(std::uintptr_t a_handler, RE::Actor* a_actor, const RE::BSFixedString& a_tag)
		{
			const auto result = func(a_handler, a_actor, a_tag);
			if (a_actor->IsPlayerRef()) {
				TrySetFirstPerson(lastDrawCameraState);
			}
			return result;
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline std::uintptr_t                   address{ REL::Relocation<std::uintptr_t>(REL::ID(155299), 0x2C).address() };
	};

	struct EnterIronSights
	{
		static bool thunk(RE::PlayerCamera* a_camera, std::uint32_t a_state)
		{
			const auto result = func(a_camera, a_state);
			if (!result) {
				TrySetFirstPerson(lastAimCameraState);
			}
			return result;
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline std::uintptr_t                   address{ REL::Relocation<std::uintptr_t>(REL::ID(153910), 0x128).address() };  // -_-
	};

	struct WeaponSheathe
	{
		static bool thunk(std::uintptr_t a_handler, RE::Actor* a_actor, const RE::BSFixedString& a_tag)
		{
			const auto result = func(a_handler, a_actor, a_tag);
			if (a_actor->IsPlayerRef()) {
				TrySetThirdPerson(lastDrawCameraState);
			}
			return result;
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline std::size_t                      idx{ 0x1 };
	};

	struct ExitIronSights
	{
		static bool thunk(RE::PlayerCamera* a_camera, std::uint32_t a_state)
		{
			const auto result = func(a_camera, a_state);
			if (!result) {
				TrySetThirdPerson(lastAimCameraState);
			}
			return result;
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline std::uintptr_t                   address{ REL::Relocation<std::uintptr_t>(REL::ID(153910), 0x231).address() };  // -_-
	};

	void Install()
	{
	    const auto settings = Settings::GetSingleton();

		settings->LoadSettings();

		if (settings->switchOnDraw) {
			// avoid weaponDraw firing twice
		    stl::write_thunk_call<WeaponDraw>();
		    stl::write_vfunc<WeaponSheathe>(RE::VTABLE::WeaponSheatheHandler[0]);

			logger::info("Hooked WeaponDraw/Sheathe");
		}

		if (settings->switchOnAim) {
			// if (!playerCamera->QCameraState(0x10)
			//	Push/PopIronSightMode

			stl::write_thunk_call<EnterIronSights>();
			stl::write_thunk_call<ExitIronSights>();

			logger::info("Hooked Enter/ExitIronSights");
		}
	}
}

void MessageCallback(SFSE::MessagingInterface::Message* a_msg) noexcept
{
	switch (a_msg->type) {
	case SFSE::MessagingInterface::kPostLoad:
		Handler::Install();
		break;
	default:
		break;
	}
}

DLLEXPORT constinit auto SFSEPlugin_Version = []() noexcept {
	SFSE::PluginVersionData data{};

	data.PluginVersion(Version::MAJOR);
	data.PluginName(Version::PROJECT);
	data.AuthorName("powerofthree");
	data.UsesSigScanning(false);
	data.UsesAddressLibrary(true);
	data.HasNoStructUse(false);
	data.IsLayoutDependent(true);
	data.CompatibleVersions({ SFSE::RUNTIME_LATEST });

	return data;
}();

DLLEXPORT bool SFSEAPI SFSEPlugin_Load(const SFSE::LoadInterface* a_sfse)
{
	SFSE::Init(a_sfse);

	SFSE::GetMessagingInterface()->RegisterListener(MessageCallback);

	return true;
}
