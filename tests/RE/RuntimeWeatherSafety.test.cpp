#include "catch2/catch_all.hpp"

#include "RE/I/Inventory3DManager.h"
#include "RE/R/RaceSexMenu.h"
#include "RE/S/SkyPrecipitation.h"
#include "RE/T/TESWeather.h"
#include "REL/REL.h"
#include "SKSE/Version.h"

#include <array>
#include <cmath>
#include <tuple>
#include <type_traits>
#include <utility>

namespace
{
	struct RuntimeMarker
	{
		std::uint32_t value{ 0x12345678 };
	};

	template <std::size_t Offset>
	struct MenuHost
	{
		std::array<std::byte, Offset> prefix{};
		RuntimeMarker                 payload;

		SE_ONLY_POINTER_ACCESSOR(RuntimeMarker, GetRuntimeData, RE::RaceSexMenu::FLAT_RUNTIME_DATA_OFFSET);
		VR_ONLY_POINTER_ACCESSOR(RuntimeMarker, GetVRRuntimeData, RE::RaceSexMenu::VR_RUNTIME_DATA_OFFSET);
	};

	template <std::size_t Offset, class Payload = RuntimeMarker>
	struct InventoryHost
	{
		std::array<std::byte, Offset> prefix{};
		Payload                       payload;

		SE_ONLY_POINTER_ACCESSOR(Payload, GetRuntimeData,
			REL::Module::IsAtLeast(SKSE::RUNTIME_SSE_1_6_629) ?
				RE::Inventory3DManager::UPDATED_FLAT_RUNTIME_DATA_OFFSET :
				RE::Inventory3DManager::FLAT_RUNTIME_DATA_OFFSET);
		VR_ONLY_POINTER_ACCESSOR(Payload, GetVRRuntimeData, RE::Inventory3DManager::VR_RUNTIME_DATA_OFFSET);
	};

	template <class Host>
	void CheckRuntimeViews(Host& a_host, bool a_vr)
	{
		if (a_vr) {
			CHECK(a_host.GetRuntimeData() == nullptr);
			CHECK(std::as_const(a_host).GetRuntimeData() == nullptr);
			CHECK(a_host.GetVRRuntimeData() == &a_host.payload);
			CHECK(std::as_const(a_host).GetVRRuntimeData() == &a_host.payload);
		} else {
			CHECK(a_host.GetVRRuntimeData() == nullptr);
			CHECK(std::as_const(a_host).GetVRRuntimeData() == nullptr);
			CHECK(a_host.GetRuntimeData() == &a_host.payload);
			CHECK(std::as_const(a_host).GetRuntimeData() == &a_host.payload);
		}
	}

	template <class Host, class Flat, class VR>
	constexpr bool PointerViewSignatures =
		std::is_same_v<decltype(std::declval<Host&>().GetRuntimeData()), Flat*> &&
		std::is_same_v<decltype(std::declval<const Host&>().GetRuntimeData()), const Flat*> &&
		std::is_same_v<decltype(std::declval<Host&>().GetVRRuntimeData()), VR*> &&
		std::is_same_v<decltype(std::declval<const Host&>().GetVRRuntimeData()), const VR*>&& noexcept(std::declval<Host&>().GetRuntimeData())&& noexcept(std::declval<const Host&>().GetRuntimeData())&& noexcept(std::declval<Host&>().GetVRRuntimeData())&& noexcept(std::declval<const Host&>().GetVRRuntimeData());

	static_assert(PointerViewSignatures<RE::RaceSexMenu, RE::RaceSexMenu::RUNTIME_DATA, RE::RaceSexMenu::VR_RUNTIME_DATA>);
	static_assert(PointerViewSignatures<RE::Inventory3DManager, RE::Inventory3DManager::RUNTIME_DATA, RE::Inventory3DManager::VR_RUNTIME_DATA>);
	static_assert(std::is_unsigned_v<decltype(RE::TESWeather::Data::transDelta)>);
	static_assert(std::is_unsigned_v<decltype(RE::TESWeather::Data::sunGlare)>);
	static_assert(std::is_unsigned_v<decltype(RE::TESWeather::Data::sunDamage)>);
	static_assert(std::is_unsigned_v<decltype(RE::TESWeather::Data::precipitationBeginFadeIn)>);
	static_assert(std::is_unsigned_v<decltype(RE::TESWeather::Data::precipitationEndFadeOut)>);
	static_assert(std::is_unsigned_v<decltype(RE::TESWeather::Data::thunderLightningBeginFadeIn)>);
	static_assert(std::is_unsigned_v<decltype(RE::TESWeather::Data::thunderLightningEndFadeOut)>);
	static_assert(std::is_unsigned_v<decltype(RE::TESWeather::Data::visualEffectBegin)>);
	static_assert(std::is_unsigned_v<decltype(RE::TESWeather::Data::visualEffectEnd)>);
	static_assert(std::is_unsigned_v<decltype(RE::TESWeather::Data::windDirection)>);
	static_assert(std::is_unsigned_v<decltype(RE::TESWeather::Data::windDirectionRange)>);
	static_assert(sizeof(RE::TESWeather::Data) == 0x14);
	static_assert(offsetof(RE::TESWeather::Data, precipitationBeginFadeIn) == 0x06);
	static_assert(offsetof(RE::TESWeather::Data, precipitationEndFadeOut) == 0x07);
	static_assert(offsetof(RE::RaceSexMenu::VR_RUNTIME_DATA, sliderData) == 0xA8);
	static_assert(offsetof(RE::RaceSexMenu::VR_RUNTIME_DATA, sex) == 0x100);
	static_assert(sizeof(RE::LoadedInventoryModelVR) == 0x48);
	static_assert(sizeof(RE::Inventory3DManager::VR_RUNTIME_DATA) == 0x208);
	static_assert(std::is_same_v<decltype(&RE::Inventory3DManager::ToggleItemZoom),
		bool (RE::Inventory3DManager::*)(RE::VR_DEVICE)>);
}

TEST_CASE("Weather/All stored precipitation bytes and strict transition boundaries", "[unit][weather-safety]")
{
	RE::TESWeather::Data weather{};
	using Flag = RE::TESWeather::WeatherDataFlag;
	CHECK_FALSE(RE::detail::HasPrecipitation(nullptr, nullptr, Flag::kRainy, 0.5F));
	CHECK_FALSE(RE::detail::HasPrecipitation(nullptr, nullptr, Flag::kSnow, 0.5F));
	for (auto flag : { RE::TESWeather::WeatherDataFlag::kRainy, RE::TESWeather::WeatherDataFlag::kSnow }) {
		weather.flags = flag;
		for (unsigned value = 0; value <= 255; ++value) {
			const auto byte = static_cast<std::uint8_t>(value);
			weather.precipitationBeginFadeIn = byte;
			weather.precipitationEndFadeOut = byte;
			const auto threshold = static_cast<float>(value) / 255.0F;
			for (float progress : { 0.0F, 0.25F, 0.5F, 0.75F, 1.0F,
					 threshold, std::nextafter(threshold, -1.0F), std::nextafter(threshold, 2.0F) }) {
				CAPTURE(value, progress);
				CHECK(RE::detail::HasPrecipitation(&weather, nullptr, Flag::kRainy, progress) ==
					  (progress > threshold && flag == Flag::kRainy));
				CHECK(RE::detail::HasPrecipitation(&weather, nullptr, Flag::kSnow, progress) ==
					  (progress > threshold && flag == Flag::kSnow));
				CHECK(RE::detail::HasPrecipitation(nullptr, &weather, Flag::kRainy, progress) ==
					  (progress < threshold && flag == Flag::kRainy));
				CHECK(RE::detail::HasPrecipitation(nullptr, &weather, Flag::kSnow, progress) ==
					  (progress < threshold && flag == Flag::kSnow));
			}
		}
	}
}

TEST_CASE("Weather/Mixed transition flags and direct byte round trips", "[unit][weather-safety]")
{
	RE::TESWeather::Data current{};
	RE::TESWeather::Data previous{};
	using Flag = RE::TESWeather::WeatherDataFlag;
	current.flags = Flag::kSnow;
	previous.flags = Flag::kRainy;
	current.precipitationBeginFadeIn = 64;
	previous.precipitationEndFadeOut = 192;
	CHECK(RE::detail::HasPrecipitation(&current, &previous, Flag::kRainy, 64.0F / 255.0F));
	CHECK_FALSE(RE::detail::HasPrecipitation(&current, &previous, Flag::kSnow, 64.0F / 255.0F));
	CHECK(RE::detail::HasPrecipitation(&current, &previous, Flag::kRainy, 0.5F));
	CHECK(RE::detail::HasPrecipitation(&current, &previous, Flag::kSnow, 0.5F));
	CHECK_FALSE(RE::detail::HasPrecipitation(&current, &previous, Flag::kRainy, 192.0F / 255.0F));
	CHECK(RE::detail::HasPrecipitation(&current, &previous, Flag::kSnow, 192.0F / 255.0F));
	current.flags = Flag::kPleasant;
	previous.flags = Flag::kCloudy;
	CHECK_FALSE(RE::detail::HasPrecipitation(&current, &previous, Flag::kRainy, 0.5F));
	CHECK_FALSE(RE::detail::HasPrecipitation(&current, &previous, Flag::kSnow, 0.5F));
	constexpr std::array fields{
		&RE::TESWeather::Data::transDelta, &RE::TESWeather::Data::sunGlare,
		&RE::TESWeather::Data::sunDamage, &RE::TESWeather::Data::precipitationBeginFadeIn,
		&RE::TESWeather::Data::precipitationEndFadeOut, &RE::TESWeather::Data::thunderLightningBeginFadeIn,
		&RE::TESWeather::Data::thunderLightningEndFadeOut, &RE::TESWeather::Data::visualEffectBegin,
		&RE::TESWeather::Data::visualEffectEnd, &RE::TESWeather::Data::windDirection,
		&RE::TESWeather::Data::windDirectionRange
	};
	for (const auto field : fields) {
		for (unsigned value = 0; value <= 255; ++value) {
			current.*field = static_cast<std::uint8_t>(value);
			CHECK(static_cast<unsigned>(current.*field) == value);
			const auto normalized = static_cast<float>(current.*field) / 255.0F;
			CHECK(normalized >= 0.0F);
			CHECK(normalized <= 1.0F);
		}
	}
}

TEST_CASE("Menus/Runtime view rejection and native inventory prefix", "[unit][weather-safety]")
{
	using Runtime = REL::Module::Runtime;
	const std::array cases{
#ifdef ENABLE_SKYRIM_SE
		std::tuple{ SKSE::RUNTIME_SSE_1_5_97, Runtime::SE, 0x58 },
#endif
#ifdef ENABLE_SKYRIM_AE
		std::tuple{ SKSE::RUNTIME_SSE_1_6_353, Runtime::AE, 0x58 },
		std::tuple{ SKSE::RUNTIME_SSE_1_6_629, Runtime::AE, 0x60 },
		std::tuple{ SKSE::RUNTIME_SSE_1_7_99, Runtime::AE, 0x60 },
#endif
#ifdef ENABLE_SKYRIM_VR
		std::tuple{ SKSE::RUNTIME_VR_1_4_15, Runtime::VR, 0x58 },
#endif
	};
	for (const auto& [version, runtime, inventoryOffset] : cases) {
		REQUIRE(REL::Module::mock(version, runtime, L"RuntimeWeatherSafety.exe", 0x1000));
		const SKSE::stl::scope_exit reset{ [] { REL::Module::reset(); } };
		if (REL::Module::IsVR()) {
			using Menu = MenuHost<RE::RaceSexMenu::VR_RUNTIME_DATA_OFFSET>;
			using Inventory = InventoryHost<RE::Inventory3DManager::VR_RUNTIME_DATA_OFFSET,
				RE::Inventory3DManager::VR_RUNTIME_DATA>;
			static_assert(offsetof(Menu, payload) == 0x50);
			static_assert(offsetof(Inventory, payload) == 0x58);
			Menu      menu;
			Inventory inventory;
			CheckRuntimeViews(menu, true);
			CheckRuntimeViews(inventory, true);
			CHECK(inventory.payload.loadedModels.empty());
			CHECK(inventory.payload.loadedModels.size() == 0);
			const auto* countBase = static_cast<const RE::BSTArrayBase*>(&inventory.payload.loadedModels);
			CHECK(reinterpret_cast<const std::byte*>(countBase) ==
				  reinterpret_cast<const std::byte*>(&inventory) + 0x258);
		} else {
			using Menu = MenuHost<RE::RaceSexMenu::FLAT_RUNTIME_DATA_OFFSET>;
			using OldInventory = InventoryHost<RE::Inventory3DManager::FLAT_RUNTIME_DATA_OFFSET>;
			using NewInventory = InventoryHost<RE::Inventory3DManager::UPDATED_FLAT_RUNTIME_DATA_OFFSET>;
			static_assert(offsetof(Menu, payload) == 0x40);
			static_assert(offsetof(OldInventory, payload) == 0x58);
			static_assert(offsetof(NewInventory, payload) == 0x60);
			Menu menu;
			CheckRuntimeViews(menu, false);
			if (inventoryOffset == 0x60) {
				NewInventory inventory;
				CheckRuntimeViews(inventory, false);
			} else {
				OldInventory inventory;
				CheckRuntimeViews(inventory, false);
			}
		}
	}
}
