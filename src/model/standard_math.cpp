// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/model/standard_math.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace thrystr::model {
namespace {

constexpr double kFunctionValueClamp = 8.0;

double clamp_finite(double value, double limit = kFunctionValueClamp) {
    if (!std::isfinite(value)) {
        return value < 0.0 ? -limit : limit;
    }
    return std::clamp(value, -limit, limit);
}

double nonzero(double value) {
    if (std::abs(value) < 1.0e-9) {
        return value < 0.0 ? -1.0e-9 : 1.0e-9;
    }
    return value;
}

double approximate_riemann_zeta(double s) {
    s = std::clamp(s, 1.05, 12.0);
    double sum = 0.0;
    for (int n = 1; n <= 96; ++n) {
        sum += std::pow(static_cast<double>(n), -s);
    }
    return sum;
}

double bessel_j0(double value) {
#if defined(__cpp_lib_math_special_functions)
    return std::cyl_bessel_j(0.0, value);
#else
    const double x = std::abs(value);
    if (x > 12.0) {
        return std::sqrt(2.0 / (std::numbers::pi * x)) * std::cos(x - std::numbers::pi / 4.0);
    }
    double term = 1.0;
    double sum = 1.0;
    for (int k = 1; k <= 24; ++k) {
        term *= -0.25 * x * x / static_cast<double>(k * k);
        sum += term;
    }
    return sum;
#endif
}

} // namespace

StandardFunction normalize_standard_function(std::uint32_t raw) {
    if (raw >= static_cast<std::uint32_t>(StandardFunction::Count)) {
        return StandardFunction::Sine;
    }
    return static_cast<StandardFunction>(raw);
}

std::string_view standard_function_name(StandardFunction function) {
    switch (function) {
    case StandardFunction::Sine:
        return "sine";
    case StandardFunction::Cosine:
        return "cosine";
    case StandardFunction::Tangent:
        return "tangent";
    case StandardFunction::Cosecant:
        return "cosecant";
    case StandardFunction::Secant:
        return "secant";
    case StandardFunction::Cotangent:
        return "cotangent";
    case StandardFunction::ArcSine:
        return "arcsine";
    case StandardFunction::ArcCosine:
        return "arccosine";
    case StandardFunction::ArcTangent:
        return "arctangent";
    case StandardFunction::ArcCosecant:
        return "arccosecant";
    case StandardFunction::ArcSecant:
        return "arcsecant";
    case StandardFunction::ArcCotangent:
        return "arccotangent";
    case StandardFunction::HyperbolicSine:
        return "hyperbolic sine";
    case StandardFunction::HyperbolicCosine:
        return "hyperbolic cosine";
    case StandardFunction::HyperbolicTangent:
        return "hyperbolic tangent";
    case StandardFunction::HyperbolicCosecant:
        return "hyperbolic cosecant";
    case StandardFunction::HyperbolicSecant:
        return "hyperbolic secant";
    case StandardFunction::HyperbolicCotangent:
        return "hyperbolic cotangent";
    case StandardFunction::InverseHyperbolicSine:
        return "inverse hyperbolic sine";
    case StandardFunction::InverseHyperbolicCosine:
        return "inverse hyperbolic cosine";
    case StandardFunction::InverseHyperbolicTangent:
        return "inverse hyperbolic tangent";
    case StandardFunction::InverseHyperbolicCosecant:
        return "inverse hyperbolic cosecant";
    case StandardFunction::InverseHyperbolicSecant:
        return "inverse hyperbolic secant";
    case StandardFunction::InverseHyperbolicCotangent:
        return "inverse hyperbolic cotangent";
    case StandardFunction::NaturalExponential:
        return "natural exponential";
    case StandardFunction::GeneralExponential:
        return "general exponential";
    case StandardFunction::NaturalLog:
        return "natural log";
    case StandardFunction::CommonLog:
        return "common log";
    case StandardFunction::BinaryLog:
        return "binary log";
    case StandardFunction::Square:
        return "square";
    case StandardFunction::Cube:
        return "cube";
    case StandardFunction::SquareRoot:
        return "square root";
    case StandardFunction::CubeRoot:
        return "cube root";
    case StandardFunction::Reciprocal:
        return "reciprocal";
    case StandardFunction::AbsoluteValue:
        return "absolute value";
    case StandardFunction::Sign:
        return "sign";
    case StandardFunction::Floor:
        return "floor";
    case StandardFunction::Ceiling:
        return "ceiling";
    case StandardFunction::Round:
        return "round";
    case StandardFunction::FractionalPart:
        return "fractional part";
    case StandardFunction::Heaviside:
        return "heaviside";
    case StandardFunction::Gamma:
        return "gamma";
    case StandardFunction::ErrorFunction:
        return "error function";
    case StandardFunction::RiemannZeta:
        return "riemann zeta";
    case StandardFunction::BesselJ0:
        return "bessel j0";
    case StandardFunction::Count:
        break;
    }
    return "sine";
}

double standard_function_value(StandardFunction function, double unit_x) {
    const double theta = 2.0 * std::numbers::pi * unit_x;
    const double sin_theta = std::sin(theta);
    const double bounded = std::clamp(sin_theta, -0.999999, 0.999999);
    const double limited_unit = std::clamp(unit_x, -6.0, 6.0);
    const double hyperbolic_unit = std::clamp(unit_x, -3.0, 3.0);
    const double positive = std::abs(unit_x) + 1.0e-6;
    const double positive_one = std::abs(unit_x) + 1.0;
    const double inverse_domain =
        std::copysign(1.0 + std::abs(sin_theta), sin_theta == 0.0 ? 1.0 : sin_theta);

    switch (function) {
    case StandardFunction::Sine:
        return std::sin(theta);
    case StandardFunction::Cosine:
        return std::cos(theta);
    case StandardFunction::Tangent:
        return std::tan(theta);
    case StandardFunction::Cosecant:
        return 1.0 / nonzero(std::sin(theta));
    case StandardFunction::Secant:
        return 1.0 / nonzero(std::cos(theta));
    case StandardFunction::Cotangent:
        return 1.0 / nonzero(std::tan(theta));
    case StandardFunction::ArcSine:
        return std::asin(bounded);
    case StandardFunction::ArcCosine:
        return std::acos(bounded);
    case StandardFunction::ArcTangent:
        return std::atan(unit_x);
    case StandardFunction::ArcCosecant:
        return std::asin(1.0 / inverse_domain);
    case StandardFunction::ArcSecant:
        return std::acos(1.0 / inverse_domain);
    case StandardFunction::ArcCotangent:
        return std::atan(1.0 / nonzero(unit_x));
    case StandardFunction::HyperbolicSine:
        return std::sinh(hyperbolic_unit);
    case StandardFunction::HyperbolicCosine:
        return std::cosh(hyperbolic_unit);
    case StandardFunction::HyperbolicTangent:
        return std::tanh(hyperbolic_unit);
    case StandardFunction::HyperbolicCosecant:
        return 1.0 / nonzero(std::sinh(hyperbolic_unit));
    case StandardFunction::HyperbolicSecant:
        return 1.0 / nonzero(std::cosh(hyperbolic_unit));
    case StandardFunction::HyperbolicCotangent:
        return 1.0 / nonzero(std::tanh(hyperbolic_unit));
    case StandardFunction::InverseHyperbolicSine:
        return std::asinh(unit_x);
    case StandardFunction::InverseHyperbolicCosine:
        return std::acosh(positive_one);
    case StandardFunction::InverseHyperbolicTangent:
        return std::atanh(bounded);
    case StandardFunction::InverseHyperbolicCosecant:
        return std::asinh(1.0 / nonzero(unit_x));
    case StandardFunction::InverseHyperbolicSecant: {
        const double domain = std::clamp(0.05 + 0.95 * std::abs(sin_theta), 0.000001, 1.0);
        return std::acosh(1.0 / domain);
    }
    case StandardFunction::InverseHyperbolicCotangent: {
        const double domain = std::copysign(1.0 + std::abs(unit_x), unit_x == 0.0 ? 1.0 : unit_x);
        return std::atanh(1.0 / domain);
    }
    case StandardFunction::NaturalExponential:
        return std::exp(limited_unit);
    case StandardFunction::GeneralExponential:
        return std::pow(2.0, std::clamp(unit_x, -8.0, 8.0));
    case StandardFunction::NaturalLog:
        return std::log(positive);
    case StandardFunction::CommonLog:
        return std::log10(positive);
    case StandardFunction::BinaryLog:
        return std::log2(positive);
    case StandardFunction::Square:
        return unit_x * unit_x;
    case StandardFunction::Cube:
        return unit_x * unit_x * unit_x;
    case StandardFunction::SquareRoot:
        return std::sqrt(positive);
    case StandardFunction::CubeRoot:
        return std::cbrt(unit_x);
    case StandardFunction::Reciprocal:
        return 1.0 / nonzero(unit_x);
    case StandardFunction::AbsoluteValue:
        return std::abs(unit_x);
    case StandardFunction::Sign:
        return unit_x > 0.0 ? 1.0 : (unit_x < 0.0 ? -1.0 : 0.0);
    case StandardFunction::Floor:
        return std::floor(unit_x);
    case StandardFunction::Ceiling:
        return std::ceil(unit_x);
    case StandardFunction::Round:
        return std::round(unit_x);
    case StandardFunction::FractionalPart:
        return unit_x - std::floor(unit_x);
    case StandardFunction::Heaviside:
        return unit_x >= 0.0 ? 1.0 : 0.0;
    case StandardFunction::Gamma:
        return std::tgamma(std::clamp(positive, 0.1, 6.0));
    case StandardFunction::ErrorFunction:
        return std::erf(unit_x);
    case StandardFunction::RiemannZeta:
        return approximate_riemann_zeta(std::abs(unit_x) + 1.05);
    case StandardFunction::BesselJ0:
        return bessel_j0(theta);
    case StandardFunction::Count:
        break;
    }
    return std::sin(theta);
}

double rotated_standard_function_value(StandardFunction function, double unit_x,
                                       double rotation_degrees) {
    const double raw_y = clamp_finite(standard_function_value(function, unit_x));
    if (std::abs(rotation_degrees) <= 1.0e-9) {
        return raw_y;
    }

    const double radians = rotation_degrees * std::numbers::pi / 180.0;
    return clamp_finite(unit_x * std::sin(radians) + raw_y * std::cos(radians));
}

} // namespace thrystr::model
