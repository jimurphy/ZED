#pragma once

#include <array>
#include <cmath>

namespace zed
{
inline constexpr const char* filterConfigurationID = "filterConfiguration";

// These explicit indices are the parameter and automation contract.
enum class FilterConfiguration : int
{
    svfLP = 0,
    svfHP = 1,
    svfBP = 2,
    svfBR = 3,
    sallenKeyLP = 4,
    sallenKeyHP = 5,
    transistorLadderLP = 6,
    diodeLadderLP = 7
};

enum class FilterModel { svf = 0, sallenKey = 1, transistorLadder = 2, diodeLadder = 3 };
// Response values also match the existing filter display's mode values.
enum class FilterResponse { lp = 1, hp = 2, bp = 3, br = 4 };

inline constexpr std::array<const char*, 8> configurationNames {
    "SVF: LP", "SVF: HP", "SVF: BP", "SVF: BR",
    "Sallen-Key: LP", "Sallen-Key: HP", "Transistor ladder: LP", "Diode ladder: LP"
};

struct FilterSelection
{
    FilterModel model;
    FilterResponse response;
};

inline constexpr std::array<FilterSelection, 8> configurations {{
    { FilterModel::svf, FilterResponse::lp },
    { FilterModel::svf, FilterResponse::hp },
    { FilterModel::svf, FilterResponse::bp },
    { FilterModel::svf, FilterResponse::br },
    { FilterModel::sallenKey, FilterResponse::lp },
    { FilterModel::sallenKey, FilterResponse::hp },
    { FilterModel::transistorLadder, FilterResponse::lp },
    { FilterModel::diodeLadder, FilterResponse::lp }
}};

inline FilterConfiguration configurationFromRaw(float value) noexcept
{
    // Reject NaN, infinities, out-of-range and fractional values before casting.
    // A malformed value uses the startup configuration without repairing state.
    if (!std::isfinite(value) || value < 0.0f || value > 7.0f || std::floor(value) != value)
        return FilterConfiguration::svfLP;
    return static_cast<FilterConfiguration>(static_cast<int>(value));
}

inline constexpr FilterSelection selectionFor(FilterConfiguration configuration) noexcept
{
    const auto index = static_cast<unsigned>(configuration);
    return configurations[index < configurations.size() ? index : 0];
}

inline constexpr bool responseAvailable(FilterModel model, FilterResponse response) noexcept
{
    for (const auto selection : configurations)
        if (selection.model == model && selection.response == response)
            return true;
    return false;
}

inline constexpr FilterConfiguration configurationFor(FilterModel model, FilterResponse response) noexcept
{
    if (!responseAvailable(model, response))
        response = FilterResponse::lp;
    for (unsigned index = 0; index < configurations.size(); ++index)
        if (configurations[index].model == model && configurations[index].response == response)
            return static_cast<FilterConfiguration>(index);
    return FilterConfiguration::svfLP;
}

inline constexpr FilterConfiguration selectModel(FilterConfiguration current, FilterModel model) noexcept
{
    return configurationFor(model, selectionFor(current).response);
}
}
