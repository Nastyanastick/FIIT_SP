#include <not_implemented.h>
#include <cstddef>
#include "../include/allocator_buddies_system.h"
#include <cstring>
#include <algorithm>
#include <new>
#include <stdexcept>

std::byte* allocator_buddies_system::bytes(void* p) noexcept
{
    return reinterpret_cast<std::byte*>(p);
}

const std::byte* allocator_buddies_system::bytes(const void* p) noexcept
{
    return reinterpret_cast<const std::byte*>(p);
}

allocator_buddies_system::allocator_header* allocator_buddies_system::header() noexcept
{
    return reinterpret_cast<allocator_header*>(_trusted_memory);
}

const allocator_buddies_system::allocator_header* allocator_buddies_system::header() const noexcept
{
    return reinterpret_cast<const allocator_header*>(_trusted_memory);
}

std::pmr::memory_resource*& allocator_buddies_system::parent_allocator_ref() noexcept
{
    return header()->parent_allocator;
}

const std::pmr::memory_resource* allocator_buddies_system::parent_allocator_ref() const noexcept
{
    return header()->parent_allocator;
}

allocator_with_fit_mode::fit_mode& allocator_buddies_system::fit_mode_ref() noexcept
{
    return header()->mode;
}

const allocator_with_fit_mode::fit_mode& allocator_buddies_system::fit_mode_ref() const noexcept
{
    return header()->mode;
}

unsigned char& allocator_buddies_system::space_power_ref() noexcept
{
    return header()->space_power;
}

const unsigned char& allocator_buddies_system::space_power_ref() const noexcept
{
    return header()->space_power;
}

std::mutex& allocator_buddies_system::mutex_ref() noexcept
{
    return header()->mtx;
}

const std::mutex& allocator_buddies_system::mutex_ref() const noexcept
{
    return header()->mtx;
}

allocator_buddies_system::block_metadata* allocator_buddies_system::first_block() noexcept
{
    return reinterpret_cast<block_metadata*>(bytes(_trusted_memory) + allocator_metadata_size);
}

const allocator_buddies_system::block_metadata* allocator_buddies_system::first_block() const noexcept
{
    return reinterpret_cast<const block_metadata*>(bytes(_trusted_memory) + allocator_metadata_size);
}

size_t allocator_buddies_system::block_size(const block_metadata* block) noexcept
{
    return size_t{1} << block->size;
}

void* allocator_buddies_system::user_memory(block_metadata* block) noexcept
{
    return bytes(block) + occupied_block_metadata_size;
}

const void* allocator_buddies_system::user_memory(const block_metadata* block) noexcept
{
    return bytes(const_cast<block_metadata*>(block)) + occupied_block_metadata_size;
}

allocator_buddies_system::~allocator_buddies_system()
{
    if (_trusted_memory == nullptr)
        return;

    mutex_ref().~mutex();

    const size_t total_size = allocator_metadata_size + (size_t{1} << space_power_ref());
    parent_allocator_ref()->deallocate(_trusted_memory, total_size);
    _trusted_memory = nullptr;
}

allocator_buddies_system::allocator_buddies_system(allocator_buddies_system&& other) noexcept
{
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;

    if (_trusted_memory == nullptr)
        return;

    auto* current = first_block();
    auto* finish = reinterpret_cast<block_metadata*>(
        const_cast<std::byte*>(bytes(first_block())) + (size_t{1} << space_power_ref())
    );

    while (current != finish)
    {
        if (current->occupied)
        {
            *reinterpret_cast<void**>(bytes(current) + sizeof(block_metadata)) = this;
        }

        current = reinterpret_cast<block_metadata*>(
            bytes(current) + block_size(current)
        );
    }
}

allocator_buddies_system& allocator_buddies_system::operator=(allocator_buddies_system&& other) noexcept
{
    if (this == &other)
        return *this;

    if (_trusted_memory != nullptr)
    {
        mutex_ref().~mutex();
        const size_t total_size = allocator_metadata_size + (size_t{1} << space_power_ref());
        parent_allocator_ref()->deallocate(_trusted_memory, total_size);
    }

    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;

    if (_trusted_memory == nullptr)
        return *this;

    auto* current = first_block();
    auto* finish = reinterpret_cast<block_metadata*>(
        const_cast<std::byte*>(bytes(first_block())) + (size_t{1} << space_power_ref())
    );

    while (current != finish)
    {
        if (current->occupied)
        {
            *reinterpret_cast<void**>(bytes(current) + sizeof(block_metadata)) = this;
        }

        current = reinterpret_cast<block_metadata*>(
            bytes(current) + block_size(current)
        );
    }

    return *this;
}

allocator_buddies_system::allocator_buddies_system(
    size_t space_size,
    std::pmr::memory_resource* parent_allocator,
    allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    if (space_size < (size_t{1} << min_k))
        throw std::logic_error("space size is too small");
    const size_t space_power = __detail::nearest_greater_k_of_2(space_size);
    space_size = size_t{1} << space_power;

    if (space_power > 127)
        throw std::logic_error("space size power is too large");

    if (parent_allocator == nullptr)
        parent_allocator = std::pmr::get_default_resource();

    const size_t total_size = allocator_metadata_size + space_size;

    _trusted_memory = parent_allocator->allocate(total_size);

    header()->parent_allocator = parent_allocator;
    header()->mode = allocate_fit_mode;
    header()->space_power = static_cast<unsigned char>(space_power);
    new (&header()->mtx) std::mutex();

    first_block()->occupied = false;
    first_block()->size = static_cast<unsigned char>(space_power);
}

[[nodiscard]] void* allocator_buddies_system::do_allocate_sm(size_t size)
{
    if (_trusted_memory == nullptr)
        throw std::bad_alloc();

    std::lock_guard<std::mutex> lock(mutex_ref());

    const size_t required_size =
        std::max(size + occupied_block_metadata_size, size_t{1} << min_k);

    size_t required_power = __detail::nearest_greater_k_of_2(required_size);

    if (required_power > space_power_ref())
        throw std::bad_alloc();

    block_metadata* candidate = nullptr;

    auto* current = first_block();
    auto* finish = reinterpret_cast<block_metadata*>(
        const_cast<std::byte*>(bytes(first_block())) + (size_t{1} << space_power_ref())
    );

    while (current != finish)
    {
        if (!current->occupied && current->size >= required_power)
        {
            if (fit_mode_ref() == fit_mode::first_fit)
            {
                candidate = current;
                break;
            }
            else if (fit_mode_ref() == fit_mode::the_best_fit)
            {
                if (candidate == nullptr || current->size < candidate->size)
                    candidate = current;
            }
            else if (fit_mode_ref() == fit_mode::the_worst_fit)
            {
                if (candidate == nullptr || current->size > candidate->size)
                    candidate = current;
            }
        }

        current = reinterpret_cast<block_metadata*>(
            bytes(current) + block_size(current)
        );
    }

    if (candidate == nullptr)
        throw std::bad_alloc();

    while (candidate->size > required_power)
    {
        const unsigned char new_size = static_cast<unsigned char>(candidate->size - 1);
        const size_t half_size = size_t{1} << new_size;

        auto* left = candidate;
        auto* right = reinterpret_cast<block_metadata*>(bytes(candidate) + half_size);

        left->occupied = false;
        left->size = new_size;

        right->occupied = false;
        right->size = new_size;

        candidate = left;
    }

    candidate->occupied = true;
    *reinterpret_cast<void**>(bytes(candidate) + sizeof(block_metadata)) = this;

    return user_memory(candidate);
}

void allocator_buddies_system::do_deallocate_sm(void* at)
{
    if (at == nullptr || _trusted_memory == nullptr)
        return;

    std::lock_guard<std::mutex> lock(mutex_ref());

    auto* block = reinterpret_cast<block_metadata*>(
        bytes(at) - occupied_block_metadata_size
    );

    std::byte* base = const_cast<std::byte*>(bytes(first_block()));
    std::byte* finish = base + (size_t{1} << space_power_ref());

    if (bytes(block) < base || bytes(block) >= finish)
        throw std::logic_error("pointer does not belong to this allocator");

    if (!block->occupied)
        throw std::logic_error("double free or invalid pointer");

    void* owner = *reinterpret_cast<void**>(
        bytes(block) + sizeof(block_metadata)
    );

    if (owner != this)
        throw std::logic_error("block does not belong to this allocator");

    block->occupied = false;

    while (block->size < space_power_ref())
    {
        const size_t current_size = size_t{1} << block->size;
        const size_t offset = static_cast<size_t>(bytes(block) - base);
        const size_t buddy_offset = offset ^ current_size;
        std::byte* buddy_addr = base + buddy_offset;

        if (buddy_addr < base || buddy_addr >= finish)
            break;

        auto* buddy = reinterpret_cast<block_metadata*>(buddy_addr);

        if (buddy->occupied || buddy->size != block->size)
            break;

        auto* merged = (bytes(block) < bytes(buddy)) ? block : buddy;
        merged->occupied = false;
        merged->size = static_cast<unsigned char>(block->size + 1);

        block = merged;
    }
}

allocator_buddies_system::allocator_buddies_system(const allocator_buddies_system& other)
{
    if (other._trusted_memory == nullptr)
    {
        _trusted_memory = nullptr;
        return;
    }

    const size_t managed_space_size = size_t{1} << other.space_power_ref();
    const size_t total_size = allocator_metadata_size + managed_space_size;

    std::pmr::memory_resource* parent =
        const_cast<std::pmr::memory_resource*>(other.parent_allocator_ref());

    _trusted_memory = parent->allocate(total_size);

    header()->parent_allocator = parent;
    header()->mode = other.fit_mode_ref();
    header()->space_power = other.space_power_ref();
    new (&header()->mtx) std::mutex();

    std::memcpy(
        bytes(_trusted_memory) + allocator_metadata_size,
        bytes(other._trusted_memory) + allocator_metadata_size,
        managed_space_size
    );

    auto* current = first_block();
    auto* finish = reinterpret_cast<block_metadata*>(
        const_cast<std::byte*>(bytes(first_block())) + (size_t{1} << space_power_ref())
    );

    while (current != finish)
    {
        if (current->occupied)
        {
            *reinterpret_cast<void**>(bytes(current) + sizeof(block_metadata)) = this;
        }

        current = reinterpret_cast<block_metadata*>(
            bytes(current) + block_size(current)
        );
    }
}

allocator_buddies_system& allocator_buddies_system::operator=(const allocator_buddies_system& other)
{
    if (this == &other)
        return *this;

    if (other._trusted_memory == nullptr)
    {
        if (_trusted_memory != nullptr)
        {
            mutex_ref().~mutex();
            const size_t total_size = allocator_metadata_size + (size_t{1} << space_power_ref());
            parent_allocator_ref()->deallocate(_trusted_memory, total_size);
        }

        _trusted_memory = nullptr;
        return *this;
    }

    const size_t managed_space_size = size_t{1} << other.space_power_ref();
    const size_t total_size = allocator_metadata_size + managed_space_size;

    std::pmr::memory_resource* parent =
        const_cast<std::pmr::memory_resource*>(other.parent_allocator_ref());

    void* new_memory = parent->allocate(total_size);
    auto* new_header = reinterpret_cast<allocator_header*>(new_memory);

    new_header->parent_allocator = parent;
    new_header->mode = other.fit_mode_ref();
    new_header->space_power = other.space_power_ref();
    new (&new_header->mtx) std::mutex();

    std::memcpy(
        bytes(new_memory) + allocator_metadata_size,
        bytes(other._trusted_memory) + allocator_metadata_size,
        managed_space_size
    );

    if (_trusted_memory != nullptr)
    {
        mutex_ref().~mutex();
        const size_t old_total_size = allocator_metadata_size + (size_t{1} << space_power_ref());
        parent_allocator_ref()->deallocate(_trusted_memory, old_total_size);
    }

    _trusted_memory = new_memory;

    auto* current = first_block();
    auto* finish = reinterpret_cast<block_metadata*>(
        const_cast<std::byte*>(bytes(first_block())) + (size_t{1} << space_power_ref())
    );

    while (current != finish)
    {
        if (current->occupied)
        {
            *reinterpret_cast<void**>(bytes(current) + sizeof(block_metadata)) = this;
        }

        current = reinterpret_cast<block_metadata*>(
            bytes(current) + block_size(current)
        );
    }

    return *this;
}

bool allocator_buddies_system::do_is_equal(const std::pmr::memory_resource& other) const noexcept
{
    return this == &other;
}

inline void allocator_buddies_system::set_fit_mode(allocator_with_fit_mode::fit_mode mode)
{
    std::lock_guard<std::mutex> lock(mutex_ref());
    fit_mode_ref() = mode;
}

std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info() const noexcept
{
    if (_trusted_memory == nullptr)
        return {};

    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_ref()));
    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> result;

    if (_trusted_memory == nullptr)
        return result;

    for (auto it = begin(); it != end(); ++it)
    {
        result.push_back({
            it.size(),
            it.occupied()
        });
    }

    return result;
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::begin() const noexcept
{
    if (_trusted_memory == nullptr)
        return buddy_iterator();

    return buddy_iterator(const_cast<void*>(static_cast<const void*>(first_block())));
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::end() const noexcept
{
    if (_trusted_memory == nullptr)
        return buddy_iterator();

    return buddy_iterator(
        const_cast<std::byte*>(bytes(first_block())) + (size_t{1} << space_power_ref())
    );
}

bool allocator_buddies_system::buddy_iterator::operator==(const allocator_buddies_system::buddy_iterator& other) const noexcept
{
    return _block == other._block;
}

bool allocator_buddies_system::buddy_iterator::operator!=(const allocator_buddies_system::buddy_iterator& other) const noexcept
{
    return _block != other._block;
}

allocator_buddies_system::buddy_iterator& allocator_buddies_system::buddy_iterator::operator++() & noexcept
{
    if (_block == nullptr)
        return *this;

    auto* block = reinterpret_cast<block_metadata*>(_block);
    _block = allocator_buddies_system::bytes(_block) + allocator_buddies_system::block_size(block);
    return *this;
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::buddy_iterator::operator++(int)
{
    buddy_iterator copy = *this;
    ++(*this);
    return copy;
}

size_t allocator_buddies_system::buddy_iterator::size() const noexcept
{
    if (_block == nullptr)
        return 0;

    auto* block = reinterpret_cast<block_metadata*>(_block);
    return allocator_buddies_system::block_size(block);
}

bool allocator_buddies_system::buddy_iterator::occupied() const noexcept
{
    if (_block == nullptr)
        return false;

    auto* block = reinterpret_cast<block_metadata*>(_block);
    return block->occupied;
}

void* allocator_buddies_system::buddy_iterator::operator*() const noexcept
{
    if (_block == nullptr)
        return nullptr;

    auto* block = reinterpret_cast<block_metadata*>(_block);
    return allocator_buddies_system::user_memory(block);
}

allocator_buddies_system::buddy_iterator::buddy_iterator(void* start)
{
    _block = start;
}

allocator_buddies_system::buddy_iterator::buddy_iterator()
{
    _block = nullptr;
}