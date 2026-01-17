# felly

*felly* is a collection of small, header-only C++23 libraries extracted from Fred Emmott's C++ projects; the name is a
pun on Meta's [folly](https://github.com/facebook/folly).

My general philosophy is that this should only contain small 'core' components with minimum interdependencies; larger
components should be their own libraries. It should not contain platform-specific helpers.

My current overall goal is to reduce duplication between my own projects.

## What's included

- `felly::guarded_data`: A wrapper for arbitrary data that is guarded by an RAII lock; similar to Rust's mutex
- `felly::moved_flag`: A marker for whether or not an object has been moved. Useful for destructors, e.g.
  `if (moved) return;`
- `felly::non_copyable`: An empty type that is not copyable. Unlike `boost::noncopyable`, it is moveable and comparable
- `felly::numeric_cast`: Range-checked conversion between built-in numeric types, including integral-to-integral,
  floating-point-to-floating-point, and integral-to-from-floating-point conversions. Throws on out of bounds, or when
  attempting to convert NaN to an integral type; NaN is permitted when converting between floating point types.
- `felly::overload`: The trivial implementation of the overload pattern, for invoking `std::visit` on an `std::variant`
  with exhaustiveness checks
- `felly::scope_exit`, `scope_failure`, `scope_success`: RAII helpers for executing code at scope exit unconditionally,
  only when an exception is thrown, or when an exception is *not* thrown. Inspired by TS v3
- `felly::unique_any`: A `unique_ptr`-like class, designed to hold arbitrary types with a 'free'-like function. It is
  also useful for pointers where a non-`nullptr` 'empty' value is used, e.g. `INVALID_HANDLE_VALUE` or `(T*)-1`
- `FELLY_NO_UNIQUE_ADDRESS`: maps to either `[[no_unique_address]]` or `[[msvc::no_unique_address]]`; MSVC recognizes
  `[[no_unique_address]]` but ignores it, to avoid potential ABI issues
- `FELLY_CPLUSPLUS`: maps to either `__cplusplus` or `_MSVC_CLANG`; checks for and deals with CMake's handling of
  `CMAKE_CXX_STANDARD` with `clang-cl` (which passes `-clang:-std=c++23`)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
