# felly

*felly* is a collection of small, header-only C++23 libraries extracted from Fred Emmott's C++ projects; the name is a pun on Meta's [folly](https://github.com/facebook/folly).

My general philosophy is that this should only contain small 'core' components with minimum interdependencies; larger components should be their own libraries. It should not contain platform-specifc helpers.

My current goal is to reduce duplication between my own projects; it is not suitable for broader use.

## What's included

- `felly::guarded_data`: A wrapper for arbitrary data that is guarded by an RAII lock; similar to Rust's mutex
- `felly::moved_flag`: A marker for whether or not an object has been moved. Useful for destructors, e.g. `if (moved) return;`
- `felly::scope_exit`, `scope_failure`, `scope_success`: RAII helpers for executing code at scope exit unconditionally, only when an exception is thrown, or when an exception is *not* thrown. Inspired by TS v3
- `felly::unique_any`: A `unique_ptr`-like class, designed to hold arbitrary types with a 'free'-like function. It is also useful for pointers where a non-`nullptr` 'empty' value is used, e.g. `INVALID_HANDLE_VALUE` or `(T*)-1`

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
