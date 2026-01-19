# felly

*felly* is a collection of small, header-only C++23 libraries extracted from Fred Emmott's C++ projects; the name is a
pun on Meta's [folly](https://github.com/facebook/folly).

My general philosophy is that this should only contain small 'core' components with minimum interdependencies; larger
components should be their own libraries. It should not contain platform-specific helpers.

My current overall goal is to reduce duplication between my own projects.

This library currently targets C++23, and is tested on:

- **Windows**: VS 2022 cl 19.44, clang-cl 19.1.5
- **Linux**: G++ 14.2.0, clang++ 18.1.3
- **macOS**: Apple Clang 17.0.0

I expect this library to continue to target modern compilers; once it's past v0.x, I plan to increase the major version when increasing the required C++ standard or reducing compiler support, except when dropping platforms that have reached their upstream end-of-life.

## Using this library

`vcpkg` is recommended (see below); otherwise, make the `include/` subdirectory of this repository available to your build system. That's it.

`felly` is available in [my vcpkg registry](https://github.com/fredemmott/vcpkg-registry); add `felly` to your `vcpkg.json` as usual, and add the registry to your `vcpkg-configuration.json`, e.g.:

```json
{
  "registries": [
    {
      "kind": "git",
      "repository": "https://github.com/fredemmott/vcpkg-registry",
      "reference": "master",
      "baseline": "REGISTRY_COMMIT_HASH_GOES_HERE",
      "packages": [
        "felly"
      ]
    }
  ]
}
```

## Components

- [felly::guarded_data](#fellyguarded_data): Monitor-pattern/totally not a Rust mutex
- [felly::moved_flag](#fellymoved_flag): Marker to simplify destructors of moveable objects
- [felly::non_copyable](#fellynon_copyable): Supertype or member to ban copying while allowing moving
- [felly::numeric_cast](#fellynumeric_cast): Cast between numeric types (including integral types ↔ floating point types) with bounds and other error checks 
- [felly::overload](#fellyoverload): Helper for `std::visit()` on `std::variant` with compiler exhaustiveness checks
- [felly::scope_exit, scope_fail, scope_success](#fellyscope_exit-scope_fail-scope_success): RAII helpers for executing code when the current scope ends
- [felly::unique_any](#fellyunique_any): Like `std::unique_ptr`, but for any type, or pointers with invalid values other than `nullptr`
- [felly::unique_ptr](#fellyunique_ptr): Specialization of `unique_any`, adding pointer-specific features
- [FELLY_CPLUSPLUS](#felly_cplusplus): Like `__cplusplus`, but works around Microsoft decisions and clang-cl quirks to give consistently correct results
- [FELLY_NO_UNIQUE_ADDRESS](#felly_no_unique_address): Like `[[no_unique_address]]`, but uses `[[msvc::no_unique_address]]` where available to work around Microsoft's decision to make `[[no_unique_address]]` a no-op to preserve ABI compatibility

---

### felly::guarded_data

**Overview**

A wrapper that encapsulates data and a mutex, ensuring the data can only be accessed while the mutex is held. This provides a pattern similar to Rust's `std::sync::Mutex`, preventing accidental access without the mutex.

**Example**

```cpp
#include <felly/guarded_data.hpp>
#include <string>

felly::guarded_data<std::string> protected_str("Hello");

{
    auto lock = protected_str.lock();
    lock->append(" World"); // Access via operator->
} // Mutex automatically released
```

**Common Edge Cases/Problems**

* **Manual Unlocking**: Supports a `.unlock()` method on the guard to release the mutex early.
* **Const Correctness**: Provides `lock() const` which returns a guard for `const T&`, preventing mutation of shared data in read-only contexts.
* **Deadlocks**: Standard mutex rules apply; nesting multiple `guarded_data` locks requires careful ordering.

**Differences with Alternatives**

* **vs std::mutex**: `std::mutex` is decoupled from the data; `guarded_data` enforces the association so you cannot accidentally access the data without locking.

---

### felly::moved_flag

**Overview**

A simple boolean wrapper that automatically sets itself to `true` when the containing object is moved, simplifying destructor logic.

**Example**
```cpp
#include <felly/moved_flag.hpp>

struct MyResource {
    felly::moved_flag moved;
    ~MyResource() {
        if (moved) return;
        // Perform cleanup
        // ...
    }
    // ...
};
```

---

### felly::non_copyable

**Overview**
A base class or member that disables copy constructors and assignments while explicitly enabling move semantics and modern comparisons.

**Example**
```cpp
#include <felly/non_copyable.hpp>

struct MyType : felly::non_copyable {
    // MyType is moveable, but cannot be copied
};
```

**Differences with Alternatives**
* **vs boost::noncopyable**: Boost's version often disables moves; `felly::non_copyable` is move-friendly, and also supports `operator<=>`.

---

### felly::numeric_cast

**Overview**
A suite of range-checked conversion functions that throw a `numeric_cast_range_error` if a conversion would result in data loss or is mathematically undefined (like NaN to int).

**Example**
```cpp
#include <felly/numeric_cast.hpp>

try {
    int64_t big = 10000000000;
    int32_t small = felly::numeric_cast<int32_t>(big); // Throws!
} catch (const felly::numeric_cast_range_error& e) {
    // Handle error
}
```

**Common Edge Cases/Problems**
* **Floating-point to/from Integral**: Not limited to integral-to-integral or floating-point-to-floating-point
* **NaN Handling**: Specifically throws when converting `NaN` to integers, but allows `NaN` when casting between float types.
* **Precision Loss**: Includes specialized logic to detect if a large floating point value exceeds the exact representable range of a target integer.

**Differences with Alternatives**
* **vs static_cast**: `static_cast` silently truncates or wraps; `numeric_cast` ensures data integrity.
* **vs boost::numeric_cast**: Updated for C++23 concepts and handles floating-point/integral crossovers with higher precision.

---

### felly::overload

**Overview**
A helper for the "overload pattern," allowing you to pass multiple lambdas to `std::visit` to handle different types in a `std::variant`.

**Example**

```cpp
#include <felly/overload.hpp>
#include <variant>

std::variant<int, float> v = 10;
std::visit(felly::overload {
    [](int i) { /* ... */ },
    [](float f) { /* ... */ }
}, v);
```

**Common Edge Cases/Problems**
* **Exhaustiveness**: Using this with `std::visit` ensures you get a compiler error if a variant type is not handled.

**Differences with Alternatives**
* **vs Custom Struct**: Replaces the manual boilerplate of creating a struct that inherits from multiple lambdas.

---

### felly::scope_exit, scope_fail, scope_success

**Overview**
RAII guards that execute a callback when the current scope exits. Variants allow execution always, only on exception (`scope_fail`), or only on success (`scope_success`).

**Example**
```cpp
#include <felly/scope_exit.hpp>

void process() {
    auto* ptr = malloc(1024);
    felly::scope_exit cleanup([&]{ free(ptr); });
    
    felly::scope_fail on_error([&]{ std::print("Operation failed!"); });
}
```

**Common Edge Cases/Problems**
* **Double Execution**: The `.release()` method allows you to cancel the callback (e.g., if ownership is transferred).

---

### felly::unique_any

**Overview**

A resource management container similar to `unique_ptr`, but designed to handle arbitrary types (like `int` handles) and custom "invalid" sentinel values.

**Example**

```cpp
#include <felly/unique_any.hpp>
#include <unistd.h>

// Guarding a file descriptor where -1 is the invalid value
using unique_fd = felly::unique_any<
    const int, 
    &close,
    [](const int fd){ return fd >= 0; } // is_valid function (optional)
>;

unique_fd my_fd(open("test.txt", O_RDONLY));
```

`unique_fd` is defined as `const int` here to allow replacing it via `reset()`, but not allow mutating it via a `get()` reference.

**Common Edge Cases/Problems**
* **'Special' pointers**: Perfect for Win32 `HANDLE`s where `INVALID_HANDLE_VALUE` is possible
* **Non-pointer values**: Suitable for Unix FDs or WinSock sockets
* **Const promotion**: Supports moving a non-const handle into a const-handle container.
* **Storage overhead**: Uses a specialized pointer storage optimization to avoid `std::optional` overhead when the underlying type is a pointer.
* **consistent handling for opaque types which vary by platform**: for example, `locale_t` is a pointer on *some* platforms, and a value on others
* **By-address cleanup of values**: Some APIs require you to create `foo_t foo;`, and pass `&foo` to a 'cleanup' function

**Differences with Alternatives**
* Ownership can be released via `disown()`, which returns the (moved) underlying value; this is equivalent to `std::unique_ptr`'s `release()`. I've renamed it due to repeatedly coming across code that leaks by accidentally calling `release()` (disown) when `reset()` (delete/cleanup) was intended.
* **vs std::unique_ptr**: `unique_ptr` is strictly for pointers and uses `nullptr` as the only empty state. `unique_any` is generic for any type and any "invalid" predicate. `felly::unique_any` also takes destructors as a value, rather than as a type.
* **vs most other `unique_any`**: most of these can only hold *values*, or require that invalid values are compile-time constants. `-1` is a common invalid value, but is an invalid compile-time value for any pointer type

**Notes**

If you are using `unique_any` with a pointer type, you can use [`felly::unique_ptr`](#fellyunique_ptr) instead to gain a default constructor (initializing to the empty state), and support for `std::out_ptr` and `std::inout_ptr`.

`unique_any` has no default constructor, as the expected behavior is unclear for `unique_any<T>` vs `unique_any<T*>`:

- option A: both default-construct the parameter, i.e. `unique_any<T>` is initialized to `T {}` and `unique_any<T*>` is initialized to `nullptr`
- option B: both are *empty*

Instead:

- use `unique_any<T> { std::nullopt }` for an empty state; `unique_any<T> { nullptr }` is also supported for pointer types
- use `unique_any<T> { std::in_place }` for a default-initialized *value*. This is not supported for pointer types.

---

### felly::unique_ptr

**Overview**: Specialization of `unique_any`, adding a default constructor (initializing to empty/`nullptr`), and support for `std::out_ptr` and `std::inout_ptr`. This is especially useful for interopation with C APIs, including the Win32 APIs.

**Example**:

```cpp
#include <felly/unique_ptr.hpp>

felly::unique_ptr<foo_t, &foo_close> x;
foo_open(std::out_ptr(x));
```

**Common Edge Cases/Problems**
* **'Special' pointers**: Perfect for Win32 `HANDLE`s where `INVALID_HANDLE_VALUE` is possible

**Differences with Alternatives**
* Ownership can be released via `disown()`, which returns the (moved) underlying value; this is equivalent to `std::unique_ptr`'s `release()`. I've renamed it due to repeatedly coming across code that leaks by accidentally calling `release()` (disown) when `reset()` (delete/cleanup) was intended.
* **vs std::unique_ptr**: `felly::unique_ptr` takes destroy functions as a value, rather than as a type.
* **vs felly::unique_any**: while `felly::unique_ptr` is better for pointer types, it is *only* usable for pointer types; `felly::unique_any` gives you a consistent API for both pointer and non-pointer types.
---

### Macros and Versioning

#### FELLY_NO_UNIQUE_ADDRESS
* **Include**: `#include <felly/no_unique_address.hpp>`
* **Overview**: Provides a cross-platform way to use `[[no_unique_address]]`.
* **Problem**: MSVC recognizes the standard attribute but ignores it for ABI compatibility; this macro uses `[[msvc::no_unique_address]]` on MSVC to ensure space optimization actually occurs.

#### FELLY_CPLUSPLUS
* **Include**: `#include <felly/version.hpp>`
* **Overview**: A reliable version of the `__cplusplus` macro.
* **Problem**: Fixes cases where MSVC reports `199711L` or where `clang-cl` and CMake interact to report the wrong standard version.

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
