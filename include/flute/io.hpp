#pragma once

/// @file io.hpp
/// Reading instances and writing solutions.

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

#include "flute/solver.hpp"
#include "flute/types.hpp"

namespace flute {

/// Upper bound on the number of terminals accepted from an instance file.
inline constexpr std::size_t kMaxTerminals = 10'000'000;

/// Reads the terminals of an instance in SteinLib STP format.
///
/// Only the terminal list (`T <id>` lines) and the coordinate section (`DD <id> <x> <y>`
/// lines) are used. The pins are returned in the order of the coordinate section.
/// @param sourceName name used in error messages
/// @throws ParseError on malformed input, unknown or duplicate terminals, missing
///         coordinates, non-finite numbers, or no terminals at all
std::vector<Point> readStp(std::istream& in, const std::string& sourceName);

/// Reads an STP file.
/// @throws ParseError if the file cannot be opened or is malformed
std::vector<Point> readStpFile(const std::filesystem::path& path);

/// Writes a solution in the plain-text format understood by tools/draw.py:
///
///     # FLUTE result
///     pins <n>
///     edges <m>
///     length <total length>
///     P <x> <y>              (one line per pin)
///     B <x> <y>              (one line per breaking point)
///     E <x1> <y1> <x2> <y2>  (one line per edge)
void writeSolution(std::ostream& out, const std::vector<Point>& pins, const Solution& solution);

/// Writes a solution to a file.
/// @throws Error if the file cannot be written
void writeSolutionFile(const std::filesystem::path& path, const std::vector<Point>& pins,
                       const Solution& solution);

}  // namespace flute
