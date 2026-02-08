# SIV (Stable Index Vector)

A header-only C++17 library providing a vector container with stable IDs for accessing elements, even after insertions and deletions. Follows STL naming conventions and mimics `std::vector`'s interface.

## Features

- **Stable IDs**: Objects are accessed via IDs that remain valid regardless of other insertions/deletions
- **Handle System**: Smart handle objects with generation tracking to detect use-after-erase
- **Cache-Friendly**: Data stored contiguously in memory for efficient iteration
- **Custom Allocator Support**: `siv::vector<T, Allocator>` with allocator propagation, like `std::vector`
- **STL-Compatible**: Familiar `std::vector`-like interface (iterators, type aliases, `at()`, `front()`, `back()`, etc.)
- **Header-Only**: Single header file, easy to integrate
- **`-fno-exceptions` Compatible**: Works with both `-fexceptions` and `-fno-exceptions`

## Installation

Simply copy `index_vector.hpp` to your project and include it:

```cpp
#include "index_vector.hpp"
```

## Quick Start

### Basic Usage

```cpp
#include "index_vector.hpp"

struct Entity {
    int x, y;
    std::string name;
};

int main() {
    siv::vector<Entity> entities;

    // Add objects - returns a stable ID
    siv::id_type player = entities.push_back({0, 0, "Player"});
    siv::id_type enemy  = entities.push_back({10, 5, "Enemy"});

    // Access via ID
    entities[player].x = 5;

    // Bounds-checked access
    entities.at(player).y = 10;

    // Erase objects - other IDs remain valid
    entities.erase(enemy);

    // player ID still works!
    printf("%s\n", entities[player].name.c_str());
}
```

### Using Handles

Handles are smart references that detect when their object has been deleted:

```cpp
siv::vector<Entity> entities;
siv::id_type id = entities.push_back({0, 0, "Test"});

// Create a handle
siv::handle<Entity> h = entities.make_handle(id);

// Use like a pointer
h->x = 10;
(*h).y = 20;

// Check validity
if (h.valid()) {
    // Safe to use
}

// After erasing, handle becomes invalid
entities.erase(id);
if (!h) {
    printf("Object was deleted!\n");
}
```

### Iteration

Iterate directly over the contiguous data:

```cpp
// Range-based for loop
for (auto& entity : entities) {
    entity.x += 1;
}

// Reverse iteration
for (auto it = entities.rbegin(); it != entities.rend(); ++it) {
    it->y -= 1;
}

// Direct data pointer access
Entity* ptr = entities.data();
```

### Conditional Removal

```cpp
// Member function
entities.erase_if([](const Entity& e) {
    return e.x < 0;
});

// Free function (C++20 style) - returns number of elements removed
auto removed = siv::erase_if(entities, [](const Entity& e) {
    return e.x < 0;
});
```

### Custom Allocator

Use a custom allocator just like `std::vector`:

```cpp
#include <memory>

// Pool allocator, tracking allocator, etc.
template<typename T>
using my_allocator = std::allocator<T>; // your custom allocator

siv::vector<Entity, my_allocator<Entity>> entities;
siv::id_type id = entities.push_back({0, 0, "Test"});

// Handles carry the allocator type
siv::handle<Entity, my_allocator<Entity>> h = entities.make_handle(id);

// Or construct with an allocator instance
my_allocator<Entity> alloc;
siv::vector<Entity, my_allocator<Entity>> entities2(alloc);
```

## API Reference

### `siv::vector<T, Allocator>`

`Allocator` defaults to `std::allocator<T>`.

#### Member Types

| Type | Definition |
|------|------------|
| `value_type` | `T` |
| `allocator_type` | `Allocator` |
| `size_type` | `std::size_t` |
| `difference_type` | `std::ptrdiff_t` |
| `reference` / `const_reference` | `T&` / `const T&` |
| `pointer` / `const_pointer` | `T*` / `const T*` |
| `iterator` / `const_iterator` | Random access iterators |
| `reverse_iterator` / `const_reverse_iterator` | Reverse iterators |

#### Element Access

| Method | Description |
|--------|-------------|
| `operator[](id)` | Access by ID (no bounds check) |
| `at(id)` | Access by ID (throws `std::out_of_range` or asserts) |
| `front()` / `back()` | First / last element in data order |
| `data()` | Pointer to underlying contiguous storage |

#### Capacity

| Method | Description |
|--------|-------------|
| `empty()` | Check if container is empty |
| `size()` | Number of elements |
| `max_size()` | Maximum possible number of elements |
| `capacity()` | Current allocated capacity |
| `reserve(n)` | Pre-allocate memory |
| `shrink_to_fit()` | Reduce memory to fit current size |
| `get_allocator()` | Returns a copy of the allocator |

#### Modifiers

| Method | Description |
|--------|-------------|
| `push_back(value)` | Copy or move an object, returns stable ID |
| `emplace_back(args...)` | Construct in-place, returns stable ID |
| `pop_back()` | Remove last element in data order |
| `erase(id)` | Remove object by stable ID |
| `erase(handle)` | Remove object referenced by handle |
| `erase_at(idx)` | Remove object by data index |
| `erase_if(pred)` | Remove all elements matching predicate |
| `clear()` | Remove all objects, invalidate all handles |

#### Iterators

| Method | Description |
|--------|-------------|
| `begin()` / `end()` | Forward iterators |
| `cbegin()` / `cend()` | Const forward iterators |
| `rbegin()` / `rend()` | Reverse iterators |
| `crbegin()` / `crend()` | Const reverse iterators |

#### Stable-ID Operations

| Method | Description |
|--------|-------------|
| `make_handle(id)` | Create a validity-tracking handle |
| `make_handle_at(idx)` | Create a handle from a data index |
| `contains(id)` | Check if ID references a live object |
| `is_valid(id, generation)` | Check if ID + generation pair is still valid |
| `generation(id)` | Get current generation counter for an ID |
| `index_of(id)` | Get the current data index for an ID |
| `next_id()` | Peek at the next ID that would be assigned |

### `siv::handle<T, Allocator>`

`Allocator` defaults to `std::allocator<T>`. Must match the allocator of the owning `siv::vector`.

| Method | Description |
|--------|-------------|
| `operator->` / `operator*` | Access underlying object (asserts validity) |
| `valid()` | Check if referenced object still exists |
| `id()` | Get the associated stable ID |
| `generation()` | Get the generation at handle creation time |
| `operator bool()` | Implicit validity check |

### Non-member Functions

| Function | Description |
|----------|-------------|
| `siv::erase_if(vec, pred)` | Remove matching elements, return count removed |
| `operator==`, `!=`, `<`, `<=`, `>`, `>=` | Lexicographic comparison of elements |

### Constants

| Name | Description |
|------|-------------|
| `siv::id_type` | Alias for `uint64_t` |
| `siv::invalid_id` | Sentinel value (`std::numeric_limits<id_type>::max()`) |

## How It Works

- Objects are stored contiguously in a data vector
- An index vector maps stable IDs to current data positions
- On deletion, the last element is swapped into the gap (O(1) erase)
- Generation counters detect use-after-erase scenarios
- Deleted ID slots are recycled on the next insertion

## Requirements

- C++17 or later
- Standard library only

## License

MIT License - see [LICENSE](LICENSE) file.
