#pragma once

/// @file error.hpp
/// Exception types thrown by the library.

#include <cstddef>
#include <stdexcept>
#include <string>

namespace flute {

/// Base class of all errors that are caused by invalid external data or I/O.
/// Programming errors (violated preconditions) throw std::invalid_argument or
/// std::logic_error instead.
class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// An instance file is malformed.
class ParseError : public Error {
public:
    ParseError(const std::string& source, std::size_t line, const std::string& message)
        : Error(source + ":" + std::to_string(line) + ": " + message) {}
    ParseError(const std::string& source, const std::string& message)
        : Error(source + ": " + message) {}
};

/// A lookup-table file is malformed, truncated or cannot be written.
class LookupTableError : public Error {
public:
    using Error::Error;
};

}  // namespace flute
