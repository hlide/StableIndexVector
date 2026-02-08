#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>


namespace siv
{
    /// Stable identifier type. Maps to an object through the index indirection layer.
    using id_type = uint64_t;

    inline constexpr id_type invalid_id = std::numeric_limits<id_type>::max();

    template<typename T, typename Allocator = std::allocator<T>>
    class vector;

    /** A standalone smart reference to an object managed by a siv::vector.
     *  Tracks validity via a generation counter to detect use-after-erase.
     *
     * @tparam T The type of the referenced object
     * @tparam Allocator The allocator type used by the owning vector
     */
    template<typename T, typename Allocator = std::allocator<T>>
    class handle
    {
    public:
        handle() = default;

        T* operator->()
        {
            assert(valid() && "Dereferencing invalid handle");
            return &(*m_vector)[m_id];
        }

        const T* operator->() const
        {
            assert(valid() && "Dereferencing invalid handle");
            return &(*m_vector)[m_id];
        }

        T& operator*()
        {
            assert(valid() && "Dereferencing invalid handle");
            return (*m_vector)[m_id];
        }

        const T& operator*() const
        {
            assert(valid() && "Dereferencing invalid handle");
            return (*m_vector)[m_id];
        }

        [[nodiscard]]
        id_type id() const noexcept
        {
            return m_id;
        }

        [[nodiscard]]
        id_type generation() const noexcept
        {
            return m_generation;
        }

        explicit operator bool() const noexcept
        {
            return valid();
        }

        [[nodiscard]]
        bool valid() const noexcept
        {
            return m_vector && m_vector->is_valid(m_id, m_generation);
        }

    private:
        handle(id_type id, id_type generation, vector<T, Allocator>* vec)
            : m_id{id}
            , m_generation{generation}
            , m_vector{vec}
        {}

        id_type                m_id         = 0;
        id_type                m_generation = 0;
        vector<T, Allocator>*  m_vector     = nullptr;

        friend class vector<T, Allocator>;
    };

    /** A vector providing stable IDs for element access.
     *  IDs remain valid across insertions and deletions of other elements.
     *  Data is stored contiguously for cache-friendly iteration.
     *
     * @tparam T The element type. Must be move-constructible and move-assignable.
     * @tparam Allocator The allocator type. Defaults to std::allocator<T>.
     */
    template<typename T, typename Allocator>
    class vector
    {
        struct metadata
        {
            id_type rid        = 0;
            id_type generation = 0;
        };

        using metadata_allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<metadata>;
        using index_allocator_type    = typename std::allocator_traits<Allocator>::template rebind_alloc<id_type>;

    public:
        // -- Member types (std::vector compatible) --

        using value_type             = T;
        using allocator_type         = Allocator;
        using size_type              = std::size_t;
        using difference_type        = std::ptrdiff_t;
        using reference              = T&;
        using const_reference        = const T&;
        using pointer                = T*;
        using const_pointer          = const T*;
        using iterator               = typename std::vector<T, Allocator>::iterator;
        using const_iterator         = typename std::vector<T, Allocator>::const_iterator;
        using reverse_iterator       = typename std::vector<T, Allocator>::reverse_iterator;
        using const_reverse_iterator = typename std::vector<T, Allocator>::const_reverse_iterator;

        // -- Constructors / assignment --

        vector() = default;

        explicit vector(const Allocator& alloc)
            : m_data(alloc)
            , m_metadata(metadata_allocator_type(alloc))
            , m_indexes(index_allocator_type(alloc))
        {}

        /// Non-copyable and non-movable to prevent dangling handle pointers
        vector(const vector&) = delete;
        vector& operator=(const vector&) = delete;
        vector(vector&&) = delete;
        vector& operator=(vector&&) = delete;

        // -- Element access --

        /** Bounds-checked access by ID.
         *  @throws std::out_of_range if exceptions are enabled, otherwise asserts
         */
        reference at(id_type id)
        {
            check_at(id);
            return m_data[m_indexes[id]];
        }

        const_reference at(id_type id) const
        {
            check_at(id);
            return m_data[m_indexes[id]];
        }

        /// Access element by stable ID (no bounds checking)
        reference operator[](id_type id)
        {
            return m_data[m_indexes[id]];
        }

        const_reference operator[](id_type id) const
        {
            return m_data[m_indexes[id]];
        }

        reference front()
        {
            return m_data.front();
        }

        const_reference front() const
        {
            return m_data.front();
        }

        reference back()
        {
            return m_data.back();
        }

        const_reference back() const
        {
            return m_data.back();
        }

        pointer data() noexcept
        {
            return m_data.data();
        }

        const_pointer data() const noexcept
        {
            return m_data.data();
        }

        // -- Iterators --

        iterator       begin()        noexcept { return m_data.begin();   }
        iterator       end()          noexcept { return m_data.end();     }
        const_iterator begin()  const noexcept { return m_data.begin();   }
        const_iterator end()    const noexcept { return m_data.end();     }
        const_iterator cbegin() const noexcept { return m_data.cbegin();  }
        const_iterator cend()   const noexcept { return m_data.cend();    }

        reverse_iterator       rbegin()        noexcept { return m_data.rbegin();  }
        reverse_iterator       rend()          noexcept { return m_data.rend();    }
        const_reverse_iterator rbegin()  const noexcept { return m_data.rbegin();  }
        const_reverse_iterator rend()    const noexcept { return m_data.rend();    }
        const_reverse_iterator crbegin() const noexcept { return m_data.crbegin(); }
        const_reverse_iterator crend()   const noexcept { return m_data.crend();   }

        // -- Capacity --

        [[nodiscard]] bool      empty()    const noexcept { return m_data.empty();    }
        [[nodiscard]] size_type size()     const noexcept { return m_data.size();     }
        [[nodiscard]] size_type max_size() const noexcept { return m_data.max_size(); }
        [[nodiscard]] size_type capacity() const noexcept { return m_data.capacity(); }

        void reserve(size_type new_cap)
        {
            m_data.reserve(new_cap);
            m_metadata.reserve(new_cap);
            m_indexes.reserve(new_cap);
        }

        /// Shrinks the data vector. Index/metadata vectors are not shrunk (needed for ID recycling).
        void shrink_to_fit()
        {
            m_data.shrink_to_fit();
        }

        /// Returns a copy of the allocator
        [[nodiscard]]
        allocator_type get_allocator() const noexcept
        {
            return m_data.get_allocator();
        }

        // -- Modifiers --

        /// Removes all elements and invalidates all existing handles
        void clear()
        {
            m_data.clear();
            for (auto& m : m_metadata) {
                ++m.generation;
            }
        }

        /** Copies the provided object at the end of the vector
         *  @return The stable ID to retrieve the object
         */
        [[nodiscard]]
        id_type push_back(const T& value)
        {
            const id_type id = get_free_slot();
            m_data.push_back(value);
            return id;
        }

        /** Moves the provided object at the end of the vector
         *  @return The stable ID to retrieve the object
         */
        [[nodiscard]]
        id_type push_back(T&& value)
        {
            const id_type id = get_free_slot();
            m_data.push_back(std::move(value));
            return id;
        }

        /** Constructs an element in-place at the end of the vector
         *  @return The stable ID to retrieve the object
         */
        template<typename... Args>
        [[nodiscard]]
        id_type emplace_back(Args&&... args)
        {
            const id_type id = get_free_slot();
            m_data.emplace_back(std::forward<Args>(args)...);
            return id;
        }

        /// Removes the last element in data order
        void pop_back()
        {
            assert(!empty() && "pop_back on empty vector");
            erase_at(m_data.size() - 1);
        }

        /** Removes the object referenced by the provided stable ID
         *  @param id The stable ID of the object to remove
         */
        void erase(id_type id)
        {
            assert(id < m_indexes.size() && "ID out of range");
            assert(m_indexes[id] < m_data.size() && "Object already erased or ID invalid");
            const id_type data_idx      = m_indexes[id];
            const id_type last_data_idx = m_data.size() - 1;
            const id_type last_id       = m_metadata[last_data_idx].rid;
            ++m_metadata[data_idx].generation;
            std::swap(m_data[data_idx], m_data[last_data_idx]);
            std::swap(m_metadata[data_idx], m_metadata[last_data_idx]);
            std::swap(m_indexes[id], m_indexes[last_id]);
            m_data.pop_back();
        }

        /** Removes the object referenced by the handle
         *  @param h A handle to the object to remove
         */
        void erase(const handle<T, Allocator>& h)
        {
            assert(h.m_vector == this && "Handle does not belong to this vector");
            assert(h.valid() && "Handle references an erased object");
            erase(h.id());
        }

        /** Removes the object at the given data index
         *  @param idx Position in the contiguous data array
         */
        void erase_at(size_type idx)
        {
            assert(idx < m_data.size() && "Index out of range");
            erase(m_metadata[idx].rid);
        }

        /** Removes all elements matching the predicate (C++20-style member)
         *  @param predicate Unary predicate returning true for elements to remove
         */
        template<typename Pred>
        void erase_if(Pred&& predicate)
        {
            for (size_type i{0}; i < m_data.size();) {
                if (predicate(m_data[i])) {
                    erase_at(i);
                } else {
                    ++i;
                }
            }
        }

        // -- Stable-ID specific operations --

        /** Returns the current data index for the given ID
         *  @param id The stable ID
         */
        [[nodiscard]]
        size_type index_of(id_type id) const
        {
            assert(id < m_indexes.size() && "ID out of range");
            return m_indexes[id];
        }

        /** Creates a handle pointing to the given stable ID
         *  @param id The stable ID of a live object
         */
        handle<T, Allocator> make_handle(id_type id)
        {
            assert(id < m_indexes.size() && m_indexes[id] < m_data.size());
            return {id, m_metadata[m_indexes[id]].generation, this};
        }

        /** Creates a handle from a data index
         *  @param idx Position in the contiguous data array
         */
        handle<T, Allocator> make_handle_at(size_type idx)
        {
            assert(idx < size());
            return {m_metadata[idx].rid, m_metadata[idx].generation, this};
        }

        /** Checks if an ID + generation pair still references a live object.
         *  Used internally by handle::valid().
         */
        [[nodiscard]]
        bool is_valid(id_type id, id_type generation) const noexcept
        {
            if (id >= m_indexes.size() || m_indexes[id] >= m_metadata.size()) {
                return false;
            }
            return generation == m_metadata[m_indexes[id]].generation;
        }

        /// Returns the generation counter for the given ID
        [[nodiscard]]
        id_type generation(id_type id) const
        {
            assert(id < m_indexes.size() && "ID out of range");
            return m_metadata[m_indexes[id]].generation;
        }

        /// Returns the ID that would be assigned to the next inserted element
        [[nodiscard]]
        id_type next_id() const
        {
            if (m_metadata.size() > m_data.size()) {
                return m_metadata[m_data.size()].rid;
            }
            return m_data.size();
        }

        /// Checks whether the ID references a currently live object
        [[nodiscard]]
        bool contains(id_type id) const noexcept
        {
            return id < m_indexes.size() && m_indexes[id] < m_data.size();
        }

    private:
        void check_at(id_type id) const
        {
            if (id >= m_indexes.size() || m_indexes[id] >= m_data.size()) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
                throw std::out_of_range("siv::vector::at: invalid id");
#else
                assert(false && "siv::vector::at: invalid id");
#endif
            }
        }

        id_type get_free_slot()
        {
            const id_type id = get_free_id();
            m_indexes[id] = m_data.size();
            return id;
        }

        id_type get_free_id()
        {
            if (m_metadata.size() > m_data.size()) {
                ++m_metadata[m_data.size()].generation;
                return m_metadata[m_data.size()].rid;
            }
            const id_type new_id = m_data.size();
            // Reserve both before modifying either to prevent desync on allocation failure
            m_indexes.reserve(m_indexes.size() + 1);
            m_metadata.reserve(m_metadata.size() + 1);
            // After successful reserves, push_back on trivial types cannot throw
            m_metadata.push_back({new_id, 0});
            m_indexes.push_back(new_id);
            return new_id;
        }

        std::vector<T, Allocator>                      m_data;
        std::vector<metadata, metadata_allocator_type>  m_metadata;
        std::vector<id_type, index_allocator_type>      m_indexes;
    };

    // -- Non-member functions --

    /// Erases all elements matching the predicate (C++20-style free function)
    /// @return The number of elements removed
    template<typename T, typename Allocator, typename Pred>
    typename vector<T, Allocator>::size_type erase_if(vector<T, Allocator>& v, Pred predicate)
    {
        const auto old_size = v.size();
        v.erase_if(std::move(predicate));
        return old_size - v.size();
    }

    /// @note Comparisons operate on elements in data-order (internal storage order),
    /// which may differ from insertion order after deletions (swap-to-back).
    template<typename T, typename Allocator>
    bool operator==(const vector<T, Allocator>& lhs, const vector<T, Allocator>& rhs)
    {
        return lhs.size() == rhs.size()
            && std::equal(lhs.begin(), lhs.end(), rhs.begin());
    }

    template<typename T, typename Allocator>
    bool operator!=(const vector<T, Allocator>& lhs, const vector<T, Allocator>& rhs)
    {
        return !(lhs == rhs);
    }

    template<typename T, typename Allocator>
    bool operator<(const vector<T, Allocator>& lhs, const vector<T, Allocator>& rhs)
    {
        return std::lexicographical_compare(lhs.begin(), lhs.end(),
                                            rhs.begin(), rhs.end());
    }

    template<typename T, typename Allocator>
    bool operator<=(const vector<T, Allocator>& lhs, const vector<T, Allocator>& rhs)
    {
        return !(rhs < lhs);
    }

    template<typename T, typename Allocator>
    bool operator>(const vector<T, Allocator>& lhs, const vector<T, Allocator>& rhs)
    {
        return rhs < lhs;
    }

    template<typename T, typename Allocator>
    bool operator>=(const vector<T, Allocator>& lhs, const vector<T, Allocator>& rhs)
    {
        return !(lhs < rhs);
    }
}
