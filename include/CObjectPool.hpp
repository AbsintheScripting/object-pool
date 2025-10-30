// -----------------------------------------------------------------------------
// CObjectPool.hpp
// A deterministic, header-only, noexcept object pool implementation using std::expected.
// Author: Daniel Contu (Migos) — https://github.com/AbsintheScripting
// -----------------------------------------------------------------------------
#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <new>
#include <vector>

namespace ObjectPool
{
// Error handling with std::expected
/**
 * @brief Lists error states returned by pool operations.
 *
 * - `OUT_OF_RANGE` — index outside pool bounds.
 * - `ALREADY_IN_USE` — attempting to activate an occupied slot.
 * - `ALREADY_UNUSED` — attempting to deactivate an empty slot.
 * - `NOT_IN_USE` — accessing inactive element.
 * - `FULL` — no free slots available.
 */
enum class EPoolError : uint8_t
{
	OUT_OF_RANGE,
	ALREADY_IN_USE,
	NOT_IN_USE,
	ALREADY_UNUSED,
	FULL
};

/** Utility function to convert the error into text, e.g., for logging */
[[nodiscard]]
constexpr const char* ToString(const EPoolError& error) noexcept
{
	switch (error)
	{
	case EPoolError::OUT_OF_RANGE: return "Index out of range";
	case EPoolError::ALREADY_IN_USE: return "Slot already in use";
	case EPoolError::NOT_IN_USE: return "Slot is not in use";
	case EPoolError::ALREADY_UNUSED: return "Slot already unused";
	case EPoolError::FULL: return "Pool is full";
	default: return "Unknown pool error";
	}
}

// Type constraints
template <typename T>
concept pool_object = std::default_initializable<T>;

/**
 * @class CObjectPool
 * @brief Deterministic, fixed-capacity pool for object reuse with explicit lifetime control.
 *
 * `CObjectPool` manages a contiguous, pre-allocated collection of objects with type `T`.
 * Each slot can be marked as *in use* or *free* and (re)constructed in-place without
 * performing any dynamic allocations. This design avoids heap fragmentation and ensures
 * consistent performance in real-time or game-engine scenarios.
 *
 * The class uses `std::expected` to report recoverable errors such as invalid indices
 * or attempts to use an already occupied slot — making the API fully `noexcept` while
 * preserving expressive error semantics.
 *
 * ### Design principles
 * - **Deterministic:** no heap allocations after construction.
 * - **Safe:** type-checked at compile time via concept (`std::default_initializable`).
 * - **Explicit lifetime control:** use/return objects manually (`Use*` / `UnUse`).
 * - **STL-compatible:** provides random access via `operator[]` and iteration
 *   over active elements through `begin()` / `end()`.
 * - **Header-only and noexcept-correct:** zero dependencies beyond the C++ standard library.
 *
 * ### Typical usage
 * ```cpp
 * CObjectPool<CEnemy> enemies(128); // preallocate 128 enemies
 *
 * size_t idx;
 * if (auto result = enemies.UseNext(idx)) {
 *     CEnemy* pEnemy = result.value();
 *     pEnemy->SpawnAt(position);
 * } else {
 *     std::cerr << "Enemy pool full: " << ToString(result.error()) << "\n";
 * }
 *
 * for (auto& enemy : enemies)
 *     enemy.Update();
 *
 * enemies.UnUse(idx); // mark slot free and reset object
 * ```
 *
 * ### Thread safety
 * Not thread-safe. If used across threads, synchronize externally.
 *
 * @tparam T Type stored in the pool. Must satisfy `std::default_initializable`.
 */
template <pool_object T>
class CObjectPool
{
public:
	/**
	 * @class CObjectPool::CIterator
	 * @brief Forward iterator that traverses only *used* elements in the pool.
	 *
	 * This iterator provides STL-compatible traversal over active elements of
	 * a `CObjectPool<T>`. It skips all slots marked as unused, allowing seamless
	 * integration with range-based `for` loops, standard algorithms, and C++23
	 * ranges library operations.
	 *
	 * The iterator satisfies the **ForwardIterator** requirements and supports
	 * comparison, dereference, and increment operations. It is lightweight and
	 * non-owning — it merely references the parent pool and tracks the current index.
	 *
	 * ### Example
	 * ```cpp
	 * CObjectPool<CParticle> pool(100);
	 * size_t id;
	 * (void)pool.UseNext(id); // 0
	 * (void)pool.UseNext(id); // 1
	 *
	 * for (auto& particle : pool)  // implicitly uses CIterator
	 *     particle.Update(dt); // will only update 0 & 1
	 * ```
	 *
	 * ### Design notes
	 * - Skips unused elements automatically on construction and increment.
	 * - Works for both const and non-const contexts.
	 * - Returning by reference allows modification of active objects directly.
	 * - Safe even when pool is empty (begin == end).
	 *
	 * @tparam T Element type stored in the parent `CObjectPool`.
	 */
	class CIterator
	{
	public:
		static constexpr bool B_BEGIN = true;
		static constexpr bool B_END = false;

		// STL conformity
		using iterator_category = std::forward_iterator_tag;
		using value_type = T;
		using difference_type = std::ptrdiff_t;
		using pointer = T*;
		using reference = T&;

		CIterator()
			: currentPos(0),
			  pObject(nullptr),
			  pPool(nullptr)
		{}

		CIterator(CObjectPool* p_pool, const bool b_begin)
			: currentPos(0),
			  pObject(nullptr),
			  pPool(p_pool)
		{
			// Skip to the first in-use object or set to end sentinel
			if (b_begin)
				SkipUnused();
			else
				SkipToEnd();
		}

		// Dereference operator
		reference operator*() const
		{
			return *pObject;
		}

		// Pointer access operator
		pointer operator->() const
		{
			return pObject;
		}

		// Pre-increment
		CIterator& operator++()
		{
			++currentPos;
			SkipUnused();
			return *this;
		}

		// Post-increment
		CIterator operator++(int)
		{
			CIterator temp = *this;
			++(*this);
			return temp;
		}

		// Equality comparison
		bool operator==(const CIterator& other) const
		{
			return pObject == other.pObject;
		}

		// Inequality comparison
		bool operator!=(const CIterator& other) const
		{
			return pObject != other.pObject;
		}

	private:
		size_t currentPos;
		pointer pObject;
		CObjectPool* pPool;

		// Skip unused objects
		void SkipUnused()
		{
			const size_t end = pPool->Size();
			while (currentPos < end && !pPool->IsInUse(currentPos))
			{
				++currentPos;
			}
			pObject = pPool->IsInUse(currentPos) ? (*pPool)[currentPos] : nullptr;
		}

		// End iterator points one past the last element (sentinel)
		void SkipToEnd()
		{
			currentPos = pPool->Size();
			pObject = nullptr;
		}
	};

	using TResult = std::expected<T*, EPoolError>;
	using TResultConst = std::expected<const T*, EPoolError>;
	using TResultVoid = std::expected<void, EPoolError>;

	CObjectPool() = delete;
	/**
	 * @brief Constructs the pool and pre-allocates `size` objects.
	 *
	 * @param size Maximum number of objects managed by the pool.
	 *
	 * With this constructor each element is default-constructed.
	 * No further allocations occur during usage.
	 */
	explicit CObjectPool(size_t size);
	/**
	 * @brief Constructs the pool and pre-allocates `size` objects.
	 *
	 * @param size Maximum number of objects managed by the pool.
	 * @param args  Optional arguments forwarded to `T` constructor for all elements.
	 *
	 * With this constructor each element is constructed using the provided arguments.
	 * No further allocations occur during usage.
	 */
	template <typename... Args>
	explicit CObjectPool(size_t size, Args&&... args);
	~CObjectPool();

	// Prevent assignment and pass-by-value (but may be implemented later)
	CObjectPool(const CObjectPool&) = delete;
	CObjectPool& operator=(const CObjectPool&) = delete;
	// disable move operation (but may be implemented later)
	CObjectPool(const CObjectPool&&) = delete;
	CObjectPool& operator=(const CObjectPool&&) = delete;

	/**
	 * @brief Provides direct, unchecked access to the element at `pos`.
	 *
	 * @param pos Slot index (no bounds checking).
	 * @return Raw pointer to the element.
	 *
	 * Use `Get()` when safety is required; use `operator[]` for high-performance access
	 * when index validity is guaranteed.
	 */
	T* operator[](size_t pos) noexcept;

	/**
	 * @brief Marks a specific slot as *in use* and returns a pointer to the object.
	 *
	 * @param pos Index of the slot to activate.
	 * @return `T*` wrapped in `std::expected` if successful,
	 *              or an error (`OUT_OF_RANGE`, `ALREADY_IN_USE`) otherwise.
	 *
	 * Activating a slot sets its usage flag to true.
	 * Does not reconstruct the object.
	 */
	[[nodiscard]]
	TResult Use(size_t pos) noexcept;
	/**
	 * @brief Finds the next free slot and marks it as *in use*.
	 *
	 * @param[out] found_pos Receives the index of the activated slot,
	 *                       or is not changed if the pool is full.
	 * @return Pointer to the activated object, or `std::unexpected(EPoolError::FULL)`
	 *         if no free slots remain.
	 *
	 * This function performs a linear search from the last inserted objects position
	 * to find the first available position, ensuring fast fill order.
	 */
	[[nodiscard]]
	TResult UseNext(size_t& found_pos) noexcept;
	/**
	 * @brief Finds the next free slot, reconstructs its object, and activates it.
	 *
	 * @param[out] found_pos Receives the index of the activated slot,
	 *                       or is not changed if the pool is full.
	 * @return Pointer to the activated object, or error (`FULL`) if pool has no free slots.
	 *
	 * Equivalent to calling `Replace(index)` followed by `Use(index)`.
	 */
	[[nodiscard]]
	TResult UseNextReplace(size_t& found_pos) noexcept;
	/**
	 * @brief Finds the next free slot, reconstructs its object with new arguments, and activates it.
	 *
	 * @param[out] found_pos Receives the index of the activated slot,
	 *                       or is not changed if the pool is full.
	 * @param args Arguments forwarded to `T`'s constructor for in-place reconstruction.
	 * @return Pointer to the activated object, or error (`FULL`) if pool has no free slots.
	 *
	 * Equivalent to calling `Replace(index, args...)` followed by `Use(index)`.
	 */
	template <typename... Args>
	[[nodiscard]]
	TResult UseNextReplace(size_t& found_pos, Args&&... args) noexcept;
	/**
	 * @brief Returns a pointer to the object at `index` if it is currently in use.
	 *
	 * @param pos Index of the slot to query.
	 * @return Pointer wrapped in `std::expected`, or error (`OUT_OF_RANGE`, `NOT_IN_USE`).
	 *
	 * Safe alternative to `operator[]`, which doesn't check bounds nor usage.
	 */
	[[nodiscard]]
	TResult Get(size_t pos) noexcept;
	/**
	 * @brief Returns a pointer to the object at `index` if it is currently in use.
	 *
	 * @param pos Index of the slot to query.
	 * @return Pointer wrapped in `std::expected`, or error (`OUT_OF_RANGE`, `NOT_IN_USE`).
	 *
	 * Safe alternative to `operator[]`, which doesn't check bounds nor usage.
	 */
	[[nodiscard]]
	TResultConst Get(size_t pos) const noexcept;
	/**
	 * @brief Checks whether the object at `pos` is active.
	 *
	 * @param pos Index of the slot to check.
	 * @return `true` if slot is in use and within range, otherwise `false`.
	 */
	[[nodiscard]]
	bool IsInUse(size_t pos) const noexcept;
	/**
	 * @brief Marks the given slot as *unused* and reconstructs the object.
	 *
	 * @param pos Index to deactivate.
	 * @return Empty `expected` on success, or error (`OUT_OF_RANGE`, `ALREADY_UNUSED`) otherwise.
	 *
	 * Once marked unused, the slot becomes available for subsequent `Use` or `UseNext` calls.
	 */
	TResultVoid UnUse(size_t pos) noexcept;
	/**
	 * @brief Marks the given slot as *unused* and reconstructs the object.
	 *
	 * @param pos Index to deactivate.
	 * @param args  Optional arguments for reinitializing the element via placement new.
	 * @return Empty `expected` on success, or error (`OUT_OF_RANGE`, `ALREADY_UNUSED`) otherwise.
	 *
	 * Once marked unused, the slot becomes available for subsequent `Use` or `UseNext` calls.
	 */
	template <typename... Args>
	TResultVoid UnUse(size_t pos, Args&&... args) noexcept;
	/**
	 * @brief Reconstructs the object at the given index (regardless of usage state).
	 *
	 * @param pos Index of the element to reconstruct.
	 * @return Empty `expected` on success, or `OUT_OF_RANGE` on invalid index.
	 *
	 * This operation marks the element as unused.
	 * It is primarily intended to reset or repopulate the object.
	 */
	[[nodiscard]]
	TResultVoid Replace(size_t pos) noexcept;
	/**
	 * @brief Reconstructs the object at the given index (regardless of usage state).
	 *
	 * @param pos Index of the element to reconstruct.
	 * @param args  Optional arguments for `T`'s constructor.
	 * @return Empty `expected` on success, or `OUT_OF_RANGE` on invalid index.
	 *
	 * This operation marks the element as unused.
	 * It is primarily intended to reset or repopulate the object.
	 */
	template <typename... Args>
	[[nodiscard]]
	TResultVoid Replace(size_t pos, Args&&... args) noexcept;

	/**
	 * @brief Returns begin iterator spanning all *active* elements in the pool.
	 *
	 * Iteration skips unused slots and supports STL algorithms and C++23 ranges.
	 * Example:
	 * ```cpp
	 * for (auto& obj : myPool)
	 *     obj.Update();
	 * ```
	 */
	CIterator begin();
	/** @brief Returns end iterator pointing one past the last element. */
	CIterator end();

	/** @brief Returns the total number of slots in the pool. */
	[[nodiscard]]
	size_t Size() const noexcept;
	/** @brief Returns the number of currently active (used) objects. */
	[[nodiscard]]
	size_t ObjectsInUse() const noexcept;

protected:
	/** @brief Abstract byte object for storing T in the object pool and marking its state. */
	struct CObject
	{
		bool bInUse = false;
		alignas(T) std::byte object[sizeof(T)]{};
	};

	/** @brief updates class member `nextIdx` with the next unused index. */
	void UpdateNextIdx();

	const size_t poolSize;
	size_t nextIdx;
	size_t objectsInUse;
	std::vector<CObject> pool;
};

// implementation

template <pool_object T>
CObjectPool<T>::CObjectPool(const size_t size)
	: poolSize(size),
	  nextIdx(0),
	  objectsInUse(0),
	  pool(std::vector<CObject>(size))
{
	for (size_t pos = 0; pos < poolSize; ++pos)
	{
		// construct value in memory of aligned storage
		// using in place operator new
		::new(&pool[pos].object) T();
	}
}

template <pool_object T>
template <typename... Args>
CObjectPool<T>::CObjectPool(const size_t size, Args&&... args)
	: poolSize(size),
	  nextIdx(0),
	  objectsInUse(0),
	  pool(std::vector<CObject>())
{
	pool.resize(size);
	for (size_t pos = 0; pos < poolSize; ++pos)
	{
		// construct value in memory of aligned storage
		// using in place operator new
		::new(&pool[pos].object) T(std::forward<Args>(args)...);
	}
}

template <pool_object T>
CObjectPool<T>::~CObjectPool()
{
	for (size_t pos = 0; pos < poolSize; ++pos)
		std::destroy_at(std::addressof(pool[pos].object));
}

template <pool_object T>
T* CObjectPool<T>::operator[](const size_t pos) noexcept
{
	return std::launder(reinterpret_cast<T*>(&pool[pos].object));
}

template <pool_object T>
CObjectPool<T>::TResult CObjectPool<T>::Use(const size_t pos) noexcept
{
	if (pos >= poolSize)
		return std::unexpected(EPoolError::OUT_OF_RANGE);

	if (pool[pos].bInUse)
		return std::unexpected(EPoolError::ALREADY_IN_USE);

	pool[pos].bInUse = true;
	UpdateNextIdx();
	objectsInUse++;
	return std::launder(reinterpret_cast<T*>(&pool[pos].object));
}

template <pool_object T>
CObjectPool<T>::TResult CObjectPool<T>::UseNext(size_t& found_pos) noexcept
{
	for (size_t pos = nextIdx, idx = 0; idx < poolSize; ++pos, pos %= poolSize, ++idx)
	{
		if (pool[pos].bInUse)
			continue;

		pool[pos].bInUse = true;
		found_pos = pos;
		UpdateNextIdx();
		objectsInUse++;
		return std::launder(reinterpret_cast<T*>(&pool[pos].object));
	}
	return std::unexpected(EPoolError::FULL);
}

template <pool_object T>
CObjectPool<T>::TResult CObjectPool<T>::UseNextReplace(size_t& found_pos) noexcept
{
	for (size_t pos = nextIdx, idx = 0; idx < poolSize; ++pos, pos %= poolSize, ++idx)
	{
		if (pool[pos].bInUse)
			continue;

		if (auto result = Replace(pos); !result.has_value())
			[[unlikely]] // Replace only returns error out of bounds
			return std::unexpected(result.error());
		pool[pos].bInUse = true;
		found_pos = pos;
		objectsInUse++;
		UpdateNextIdx();
		return std::launder(reinterpret_cast<T*>(&pool[pos].object));
	}
	return std::unexpected(EPoolError::FULL);
}

template <pool_object T>
template <typename... Args>
CObjectPool<T>::TResult CObjectPool<T>::UseNextReplace(size_t& found_pos, Args&&... args) noexcept
{
	for (size_t pos = nextIdx, idx = 0; idx < poolSize; ++pos, pos %= poolSize, ++idx)
	{
		if (pool[pos].bInUse)
			continue;

		if (auto result = Replace(pos, std::forward<Args>(args)...); !result.has_value())
			[[unlikely]] // Replace only returns error out of bounds
			return std::unexpected(result.error());
		pool[pos].bInUse = true;
		found_pos = pos;
		objectsInUse++;
		UpdateNextIdx();
		return std::launder(reinterpret_cast<T*>(&pool[pos].object));
	}
	return std::unexpected(EPoolError::FULL);
}

template <pool_object T>
CObjectPool<T>::TResult CObjectPool<T>::Get(const size_t pos) noexcept
{
	if (pos >= poolSize)
		return std::unexpected(EPoolError::OUT_OF_RANGE);
	if (!pool[pos].bInUse)
		return std::unexpected(EPoolError::NOT_IN_USE);
	return std::launder(reinterpret_cast<T*>(&pool[pos].object));
}

template <pool_object T>
CObjectPool<T>::TResultConst CObjectPool<T>::Get(const size_t pos) const noexcept
{
	if (pos >= poolSize)
		return std::unexpected(EPoolError::OUT_OF_RANGE);
	if (!pool[pos].bInUse)
		return std::unexpected(EPoolError::NOT_IN_USE);
	return std::launder(reinterpret_cast<const T*>(&pool[pos].object));
}

template <pool_object T>
bool CObjectPool<T>::IsInUse(const size_t pos) const noexcept
{
	if (pos >= poolSize)
		return false;
	return pool[pos].bInUse;
}

template <pool_object T>
CObjectPool<T>::TResultVoid CObjectPool<T>::UnUse(const size_t pos) noexcept
{
	if (pos >= poolSize)
		return std::unexpected(EPoolError::OUT_OF_RANGE);
	if (!pool[pos].bInUse)
		return std::unexpected(EPoolError::ALREADY_UNUSED);

	(void)Replace(pos);
	objectsInUse--;
	return {};
}

template <pool_object T>
template <typename... Args>
CObjectPool<T>::TResultVoid CObjectPool<T>::UnUse(const size_t pos, Args&&... args) noexcept
{
	if (pos >= poolSize)
		return std::unexpected(EPoolError::OUT_OF_RANGE);
	if (!pool[pos].bInUse)
		return std::unexpected(EPoolError::ALREADY_UNUSED);

	(void)Replace(pos, std::forward<Args>(args)...);
	objectsInUse--;
	return {};
}

template <pool_object T>
CObjectPool<T>::TResultVoid CObjectPool<T>::Replace(const size_t pos) noexcept
{
	if (pos >= poolSize)
		return std::unexpected(EPoolError::OUT_OF_RANGE);

	std::destroy_at(std::launder(reinterpret_cast<T*>(&pool[pos].object)));
	::new(&pool[pos].object) T();
	pool[pos].bInUse = false;
	return {};
}

template <pool_object T>
template <typename... Args>
CObjectPool<T>::TResultVoid CObjectPool<T>::Replace(const size_t pos, Args&&... args) noexcept
{
	if (pos >= poolSize)
		return std::unexpected(EPoolError::OUT_OF_RANGE);

	std::destroy_at(std::launder(reinterpret_cast<T*>(&pool[pos].object)));
	::new(&pool[pos].object) T(std::forward<Args>(args)...);
	pool[pos].bInUse = false;
	return {};
}

template <pool_object T>
CObjectPool<T>::CIterator CObjectPool<T>::begin()
{
	return CIterator(this, CIterator::B_BEGIN);
}

template <pool_object T>
CObjectPool<T>::CIterator CObjectPool<T>::end()
{
	return CIterator(this, CIterator::B_END);
}

template <pool_object T>
size_t CObjectPool<T>::Size() const noexcept
{
	return poolSize;
}

template <pool_object T>
size_t CObjectPool<T>::ObjectsInUse() const noexcept
{
	return objectsInUse;
}

template <pool_object T>
void CObjectPool<T>::UpdateNextIdx()
{
	for (size_t pos = nextIdx, idx = 0; idx < poolSize; ++pos, pos %= poolSize, ++idx)
	{
		if (pool[pos].bInUse)
			continue;

		nextIdx = pos;
		return;
	}
}
}
