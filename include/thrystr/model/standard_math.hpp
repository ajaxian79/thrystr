// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace thrystr::model {

enum class StandardFunction : std::uint32_t {
    Sine,
    Cosine,
    Tangent,
    Cosecant,
    Secant,
    Cotangent,
    ArcSine,
    ArcCosine,
    ArcTangent,
    ArcCosecant,
    ArcSecant,
    ArcCotangent,
    HyperbolicSine,
    HyperbolicCosine,
    HyperbolicTangent,
    HyperbolicCosecant,
    HyperbolicSecant,
    HyperbolicCotangent,
    InverseHyperbolicSine,
    InverseHyperbolicCosine,
    InverseHyperbolicTangent,
    InverseHyperbolicCosecant,
    InverseHyperbolicSecant,
    InverseHyperbolicCotangent,
    NaturalExponential,
    GeneralExponential,
    NaturalLog,
    CommonLog,
    BinaryLog,
    Square,
    Cube,
    SquareRoot,
    CubeRoot,
    Reciprocal,
    AbsoluteValue,
    Sign,
    Floor,
    Ceiling,
    Round,
    FractionalPart,
    Heaviside,
    Gamma,
    ErrorFunction,
    RiemannZeta,
    BesselJ0,
    Count,
};

constexpr std::array<StandardFunction, static_cast<std::size_t>(StandardFunction::Count)>
    kStandardFunctions = {
        StandardFunction::Sine,
        StandardFunction::Cosine,
        StandardFunction::Tangent,
        StandardFunction::Cosecant,
        StandardFunction::Secant,
        StandardFunction::Cotangent,
        StandardFunction::ArcSine,
        StandardFunction::ArcCosine,
        StandardFunction::ArcTangent,
        StandardFunction::ArcCosecant,
        StandardFunction::ArcSecant,
        StandardFunction::ArcCotangent,
        StandardFunction::HyperbolicSine,
        StandardFunction::HyperbolicCosine,
        StandardFunction::HyperbolicTangent,
        StandardFunction::HyperbolicCosecant,
        StandardFunction::HyperbolicSecant,
        StandardFunction::HyperbolicCotangent,
        StandardFunction::InverseHyperbolicSine,
        StandardFunction::InverseHyperbolicCosine,
        StandardFunction::InverseHyperbolicTangent,
        StandardFunction::InverseHyperbolicCosecant,
        StandardFunction::InverseHyperbolicSecant,
        StandardFunction::InverseHyperbolicCotangent,
        StandardFunction::NaturalExponential,
        StandardFunction::GeneralExponential,
        StandardFunction::NaturalLog,
        StandardFunction::CommonLog,
        StandardFunction::BinaryLog,
        StandardFunction::Square,
        StandardFunction::Cube,
        StandardFunction::SquareRoot,
        StandardFunction::CubeRoot,
        StandardFunction::Reciprocal,
        StandardFunction::AbsoluteValue,
        StandardFunction::Sign,
        StandardFunction::Floor,
        StandardFunction::Ceiling,
        StandardFunction::Round,
        StandardFunction::FractionalPart,
        StandardFunction::Heaviside,
        StandardFunction::Gamma,
        StandardFunction::ErrorFunction,
        StandardFunction::RiemannZeta,
        StandardFunction::BesselJ0,
};

StandardFunction normalize_standard_function(std::uint32_t raw);
std::string_view standard_function_name(StandardFunction function);
double standard_function_value(StandardFunction function, double unit_x);
double rotated_standard_function_value(StandardFunction function, double unit_x,
                                       double rotation_degrees);

} // namespace thrystr::model
