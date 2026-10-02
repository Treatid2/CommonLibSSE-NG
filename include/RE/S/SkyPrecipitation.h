#pragma once

#include "RE/T/TESWeather.h"

namespace RE::detail
{
	/// Evaluates strict normalized fade thresholds for independently nullable weather data.
	[[nodiscard]] inline bool HasPrecipitation(const TESWeather::Data* a_current,
		const TESWeather::Data* a_previous, TESWeather::WeatherDataFlag a_flag, float a_progress) noexcept
	{
		return (a_current && a_current->flags.any(a_flag) &&
				   static_cast<float>(a_current->precipitationBeginFadeIn) / 255.0F < a_progress) ||
		       (a_previous && a_previous->flags.any(a_flag) &&
				   static_cast<float>(a_previous->precipitationEndFadeOut) / 255.0F > a_progress);
	}
}
