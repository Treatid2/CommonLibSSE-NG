#include "catch2/catch_all.hpp"

#include "RE/I/Inventory3DManager.h"
#include "RE/R/RaceSexMenu.h"
#include "RE/S/Sky.h"
#include "RE/T/TESWeather.h"
#include "REL/REL.h"
#include "SKSE/Version.h"

#include <array>
#include <cmath>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <utility>

namespace
{
	template <class T>
	struct EngineStorage
	{
		alignas(T) std::array<std::byte, sizeof(T)> bytes{};
		T* get() { return reinterpret_cast<T*>(bytes.data()); }
	};

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
	EngineStorage<RE::Sky>        skyStorage;
	EngineStorage<RE::TESWeather> weatherStorage;
	auto*                         sky = skyStorage.get();
	auto*                         weather = weatherStorage.get();
	CHECK_FALSE(sky->IsRaining());
	CHECK_FALSE(sky->IsSnowing());
	for (auto flag : { RE::TESWeather::WeatherDataFlag::kRainy, RE::TESWeather::WeatherDataFlag::kSnow }) {
		weather->data.flags = flag;
		for (unsigned value = 0; value <= 255; ++value) {
			const auto byte = static_cast<std::uint8_t>(value);
			weather->data.precipitationBeginFadeIn = byte;
			weather->data.precipitationEndFadeOut = byte;
			const auto threshold = static_cast<float>(value) / 255.0F;
			for (float progress : { 0.0F, 0.25F, 0.5F, 0.75F, 1.0F,
					 threshold, std::nextafter(threshold, -1.0F), std::nextafter(threshold, 2.0F) }) {
				CAPTURE(value, progress);
				sky->currentWeatherPct = progress;
				sky->currentWeather = weather;
				sky->lastWeather = nullptr;
				CHECK(sky->IsRaining() == (progress > threshold && flag == RE::TESWeather::WeatherDataFlag::kRainy));
				CHECK(sky->IsSnowing() == (progress > threshold && flag == RE::TESWeather::WeatherDataFlag::kSnow));
				sky->currentWeather = nullptr;
				sky->lastWeather = weather;
				CHECK(sky->IsRaining() == (progress < threshold && flag == RE::TESWeather::WeatherDataFlag::kRainy));
				CHECK(sky->IsSnowing() == (progress < threshold && flag == RE::TESWeather::WeatherDataFlag::kSnow));
			}
		}
	}
}

TEST_CASE("Weather/Mixed transition flags and direct byte round trips", "[unit][weather-safety]")
{
	EngineStorage<RE::Sky>        skyStorage;
	EngineStorage<RE::TESWeather> currentStorage;
	EngineStorage<RE::TESWeather> previousStorage;
	auto*                         sky = skyStorage.get();
	auto*                         current = currentStorage.get();
	auto*                         previous = previousStorage.get();
	sky->currentWeather = current;
	sky->lastWeather = previous;
	current->data.flags = RE::TESWeather::WeatherDataFlag::kSnow;
	previous->data.flags = RE::TESWeather::WeatherDataFlag::kRainy;
	current->data.precipitationBeginFadeIn = 64;
	previous->data.precipitationEndFadeOut = 192;
	sky->currentWeatherPct = 64.0F / 255.0F;
	CHECK(sky->IsRaining());
	CHECK_FALSE(sky->IsSnowing());
	sky->currentWeatherPct = 0.5F;
	CHECK(sky->IsRaining());
	CHECK(sky->IsSnowing());
	sky->currentWeatherPct = 192.0F / 255.0F;
	CHECK_FALSE(sky->IsRaining());
	CHECK(sky->IsSnowing());
	current->data.flags = RE::TESWeather::WeatherDataFlag::kPleasant;
	previous->data.flags = RE::TESWeather::WeatherDataFlag::kCloudy;
	CHECK_FALSE(sky->IsRaining());
	CHECK_FALSE(sky->IsSnowing());
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
			current->data.*field = static_cast<std::uint8_t>(value);
			CHECK(static_cast<unsigned>(current->data.*field) == value);
			const auto normalized = static_cast<float>(current->data.*field) / 255.0F;
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
		const SKSE::stl::scope_exit              reset{ [] { REL::Module::reset(); } };
		alignas(16) std::array<std::byte, 0x800> storage{};
		auto*                                    menu = reinterpret_cast<RE::RaceSexMenu*>(storage.data());
		auto*                                    inventory = reinterpret_cast<RE::Inventory3DManager*>(storage.data());
		if (REL::Module::IsVR()) {
			CHECK(menu->GetRuntimeData() == nullptr);
			CHECK(std::as_const(*menu).GetRuntimeData() == nullptr);
			REQUIRE(menu->GetVRRuntimeData());
			CHECK(reinterpret_cast<std::byte*>(&menu->GetVRRuntimeData()->sex) == storage.data() + 0x150);
			CHECK(reinterpret_cast<const std::byte*>(&std::as_const(*menu).GetVRRuntimeData()->sliderData) == storage.data() + 0xF8);
			CHECK(inventory->GetRuntimeData() == nullptr);
			CHECK(std::as_const(*inventory).GetRuntimeData() == nullptr);
			REQUIRE(inventory->GetVRRuntimeData());
			CHECK(reinterpret_cast<std::byte*>(inventory->GetVRRuntimeData()) == storage.data() + 0x58);
			CHECK(reinterpret_cast<const std::byte*>(std::as_const(*inventory).GetVRRuntimeData()) == storage.data() + 0x58);
			const std::uint32_t count = 2;
			std::memcpy(storage.data() + 0x258, &count, sizeof(count));
			CHECK(inventory->GetVRRuntimeData()->loadedModels.size() == count);
		} else {
			CHECK(menu->GetVRRuntimeData() == nullptr);
			CHECK(std::as_const(*menu).GetVRRuntimeData() == nullptr);
			CHECK(inventory->GetVRRuntimeData() == nullptr);
			CHECK(std::as_const(*inventory).GetVRRuntimeData() == nullptr);
			REQUIRE(menu->GetRuntimeData());
			CHECK(reinterpret_cast<std::byte*>(&menu->GetRuntimeData()->sliderData) == storage.data() + 0x140);
			CHECK(reinterpret_cast<const std::byte*>(&std::as_const(*menu).GetRuntimeData()->sex) == storage.data() + 0x198);
			CHECK(reinterpret_cast<std::byte*>(inventory->GetRuntimeData()) == storage.data() + inventoryOffset);
			CHECK(reinterpret_cast<const std::byte*>(std::as_const(*inventory).GetRuntimeData()) == storage.data() + inventoryOffset);
		}
	}
}
