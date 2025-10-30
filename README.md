# ObjectPool
**A Modern C++23 Object Pool Implementation (Header-only)**

---

## Overview

ObjectPool is a lightweight, header‑only implementation of the **Object Pool** pattern.
It pre-allocates a fixed number of objects and lets you mark slots as *in use* or *free*,
iterate only over the *used* ones, and (re)construct elements in place when needed.
It follows modern C++ practices and adheres to my [C++23 Code Style Guide](https://gist.github.com/AbsintheScripting/4f2be73c91fc49fc6bc2cefbb2a52895).

This implementation focuses on simplicity, performance, and clarity.

---

## Features

- 🧠 **Templated Design** — Works with any default-constructible type  
- 🚀 **Smart & Fast Iterators** — STL conform forward-iterator skips objects not in use  
- 🧱 **Header-only Library** — Just include `CObjectPool.hpp`  
- 🧩 **`noexcept` Correctness** — Explicit exception guarantees throughout  
- ⚙️ **Deterministic Allocation Pattern** — Fixed preallocation, no dynamic growth at runtime  
- ✅ **Unit Tested** — Includes GoogleTest-based tests in `tests/ObjectPool.cpp`

> 🧵 **Note:** This implementation is **not thread-safe**.  
> If you need concurrency, wrap it with synchronization primitives externally.

---

## Example Usage

```cpp
#include "CObjectPool.hpp"
#include <print>

using namespace ObjectPool;

struct CMyObject
{
	int32_t value = 0;
};

int main()
{
	// Create a pool with 10 pre-allocated objects
	CObjectPool<CMyObject> pool(10);

	// Acquire an object from the pool
	size_t foundIdx;
	auto result = pool.UseNext(foundIdx);
	if (!result.has_value())
	{
		std::println("ERROR: Object pool full: {}", ToString(result.error()));
		return 1;
	}
	// Modify the object
	auto pObject = result.value();
	pObject->value = 42;

	// Iterate over all used objects (in this case only pool[0])
	for (const auto& object : pool)
	{
		std::println("Object value: {}", object.value);
	}

	// UnUse the object
	auto result2 = pool.UnUse(foundIdx);
	if (!result2.has_value())
	{
		// either EPoolError::OUT_OF_RANGE or EPoolError::ALREADY_UNUSED
		std::println("ERROR: Could not unuse the object: {}", ToString(result2.error()));
		return 1;
	}

	return 0;
}
```

---

## Tests & Behavior Reference

The repository includes a comprehensive GoogleTest suite covering:
- Construction (default / with args), bounds & overflow
- `Use`, `UseNext`, `UseNextReplace` semantics and gap filling
- `UnUse` / `Replace` (default and with args) behavior
- Safe access via `Get`, direct access via `operator[]`
- Iterator correctness: skipping gaps, `++it`/post‑increment, `->`/`*`, equality/inequality
- Compatibility with `<algorithm>` & `<ranges>` (`find_if`, `transform`, `for_each`, `all_of`/`any_of`/`none_of`, views + filters)

---

## Building and Testing

The project uses **CMake** and **GoogleTest**.

```bash
# Clone repository
git clone https://github.com/AbsintheScripting/object-pool.git
cd object-pool

# Configure and build
# Make sure you use at least g++-14, if not add -DCMAKE_CXX_COMPILER=g++-14
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Run tests
cd build && ctest --output-on-failure
```

### Dependencies
- **CMake ≥ 3.20**
- **C++23-compatible compiler** (MSVC v145+, GCC 14+, Clang 16+)
- **GoogleTest 1.17.0** (automatically fetched via `FetchContent`)

---

## File Structure

```
CObjectPool/
│
├── include/
│   └── CObjectPool.hpp        # Header-only Object Pool implementation
│
├── tests/
│   └── ObjectPool.cpp         # GoogleTest-based tests
│
└── CMakeLists.txt             # Build + test configuration
```

---

## Code Style

This repository follows the [My C++23 Code Style Guide](https://gist.github.com/AbsintheScripting/4f2be73c91fc49fc6bc2cefbb2a52895),  
which enforces:

- Explicit ownership (`unique_ptr`, RAII)
- Fixed-width integer types (`<cstdint>`)
- Consistent naming (`C` for classes, `b` for booleans, etc.)
- Const- and `noexcept`-correct design
- Clean header separation and modern includes
- Readability over cleverness

> All contributions must conform to this style guide.

---

## License

This project is released under the **MIT License** — see [`LICENSE`](LICENSE).

---

## Author

**Migos (Daniel Contu)**  
Tech Lead / C++ Developer  
[GitHub Profile](https://github.com/AbsintheScripting)  
[Code Style Gist](https://gist.github.com/AbsintheScripting/4f2be73c91fc49fc6bc2cefbb2a52895)
