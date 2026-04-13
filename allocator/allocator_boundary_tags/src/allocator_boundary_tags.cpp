#include <not_implemented.h>
#include "../include/allocator_boundary_tags.h"
#include <cstring>
#include <stdexcept>

allocator_boundary_tags::~allocator_boundary_tags()
{
    if (_trusted_memory == nullptr) return;
    auto* meta = get_meta();
    auto* parent = meta->parent_allocator;
    size_t total_size = sizeof(allocator_meta) + meta->space_size;

    meta->mutex.~mutex();
    parent->deallocate(_trusted_memory, total_size);
}

allocator_boundary_tags::allocator_boundary_tags(
    allocator_boundary_tags &&other) noexcept
{
    if (other._trusted_memory == nullptr)
    {
        _trusted_memory = nullptr;
        return;
    }
    std::lock_guard<std::mutex> lock(other.get_meta()->mutex);
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;

    auto* meta = get_meta();
    auto* current = get_first_block();
    auto* end = reinterpret_cast<std::byte*>(_trusted_memory) + sizeof(allocator_meta) + meta->space_size;

    while (reinterpret_cast<std::byte*>(current) < end)
    {
        if (current->owner != nullptr) {
            current->owner = this;
        }
        current = reinterpret_cast<block_header*>(reinterpret_cast<std::byte*>(current) + current->block_size);
    }
}

allocator_boundary_tags &allocator_boundary_tags::operator=(
    allocator_boundary_tags &&other) noexcept
{
    if (this == &other) return *this;
    if (_trusted_memory == nullptr && other._trusted_memory == nullptr)
    {
        _trusted_memory = nullptr;
        return *this;
    }
    if (_trusted_memory == nullptr)
    {
        std::lock_guard<std::mutex> lock(other.get_meta()->mutex);
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
        return *this;
    }
    if (other._trusted_memory == nullptr)
    {
        auto* meta = get_meta();
        auto* parent = meta->parent_allocator;
        size_t total_size = sizeof(allocator_meta) + meta->space_size;
        
        { std::lock_guard<std::mutex> lock(meta->mutex); }
        
        meta->mutex.~mutex();
        parent->deallocate(_trusted_memory, total_size);

        _trusted_memory = nullptr;
        return *this;
    }
    
    auto* meta1 = get_meta();
    auto* meta2 = other.get_meta();
    auto* parent = meta1->parent_allocator;
    size_t total_size = sizeof(allocator_meta) + meta1->space_size;
    void* old_memory = _trusted_memory;

    {
        std::scoped_lock lock(meta1->mutex, meta2->mutex);
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }

    auto* meta_new = get_meta();
    auto* current = get_first_block();
    auto* end = reinterpret_cast<std::byte*>(_trusted_memory) + sizeof(allocator_meta) + meta_new->space_size;

    while (reinterpret_cast<std::byte*>(current) < end)
    {
        if (current->owner != nullptr) {
            current->owner = this;
        }
        current = reinterpret_cast<block_header*>(reinterpret_cast<std::byte*>(current) + current->block_size);
    }
    
    meta1->mutex.~mutex();
    parent->deallocate(old_memory, total_size);
    return *this;
}

allocator_boundary_tags::allocator_meta* allocator_boundary_tags::get_meta() const noexcept
{
    return reinterpret_cast<allocator_meta*>(_trusted_memory);
}

allocator_boundary_tags::block_header* allocator_boundary_tags::get_first_block() const noexcept
{
    return reinterpret_cast<block_header*>(reinterpret_cast<std::byte*>(_trusted_memory) + sizeof(allocator_meta));
}

allocator_boundary_tags::allocator_boundary_tags(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    if (parent_allocator == nullptr) parent_allocator = std::pmr::get_default_resource();
    if (space_size < sizeof(block_header)) throw std::bad_alloc();
    _trusted_memory = parent_allocator->allocate(sizeof(allocator_meta) + space_size);

    auto* meta = reinterpret_cast<allocator_meta*>(_trusted_memory);
    new (&meta->mutex) std::mutex();

    meta->parent_allocator = parent_allocator;
    meta->fit_mode = allocate_fit_mode;
    meta->space_size = space_size;
    meta->occupied_head = nullptr;

    auto* first = get_first_block();
    first->block_size = space_size;
    first->prev = nullptr;
    first->next = nullptr;
    first->owner = nullptr;
}

[[nodiscard]] void* allocator_boundary_tags::do_allocate_sm(size_t size)
{
    auto* meta = get_meta();
    std::lock_guard<std::mutex> lock(meta->mutex);

    auto* current = get_first_block();
    auto* end = reinterpret_cast<std::byte*>(_trusted_memory) + sizeof(allocator_meta) + meta->space_size;

    block_header* chosen = nullptr;

    if (meta->fit_mode == fit_mode::first_fit)
    {
        while (reinterpret_cast<std::byte*>(current) < end)
        {
            size_t payload = current->block_size - sizeof(block_header);
            if (current->owner == nullptr && payload >= size)
            {
                chosen = current;
                break;
            }
            current = reinterpret_cast<block_header*>(reinterpret_cast<std::byte*>(current) + current->block_size);
        }
    }
    else if (meta->fit_mode == fit_mode::the_best_fit || meta->fit_mode == fit_mode::the_worst_fit)
    {
        block_header* best = nullptr;
        while (reinterpret_cast<std::byte*>(current) < end)
        {
            size_t payload = current->block_size - sizeof(block_header);
            if (current->owner == nullptr && payload >= size)
            {
                if (best == nullptr ||
                    (meta->fit_mode == fit_mode::the_best_fit && current->block_size < best->block_size) ||
                    (meta->fit_mode == fit_mode::the_worst_fit && current->block_size > best->block_size))
                {
                    best = current;
                }
            }
            current = reinterpret_cast<block_header*>(reinterpret_cast<std::byte*>(current) + current->block_size);
        }
        chosen = best;
    }

    if (!chosen) throw std::bad_alloc();

    size_t total_needed = sizeof(block_header) + size;

    if (chosen->block_size >= total_needed + sizeof(block_header) + 1)
    {
        auto* new_block = reinterpret_cast<block_header*>(
            reinterpret_cast<std::byte*>(chosen) + total_needed
        );
        new_block->block_size = chosen->block_size - total_needed;
        new_block->owner = nullptr;
        new_block->prev = nullptr;
        new_block->next = nullptr;

        chosen->block_size = total_needed;
    }

    chosen->owner = this;
    chosen->prev = nullptr;
    chosen->next = meta->occupied_head;
    if (meta->occupied_head)
        meta->occupied_head->prev = chosen;
    meta->occupied_head = chosen;

    return reinterpret_cast<std::byte*>(chosen) + sizeof(block_header);
}

void allocator_boundary_tags::do_deallocate_sm(void *at)
{
    auto* meta = get_meta();
    std::lock_guard<std::mutex> lock(meta->mutex);

    if (at == nullptr) return;

    auto* start = reinterpret_cast<std::byte*>(_trusted_memory);
    auto* end = start + sizeof(allocator_meta) + meta->space_size;
    auto* min_ptr = start + sizeof(allocator_meta) + sizeof(block_header);
    auto* p = reinterpret_cast<std::byte*>(at);

    if (p < min_ptr || p >= end)
        throw std::logic_error("pointer does not belong to this allocator");

    auto* block = reinterpret_cast<block_header*>(p - sizeof(block_header));

    if (block->owner != this)
        throw std::logic_error("block does not belong to this allocator");

    if (block->prev != nullptr)
        block->prev->next = block->next;
    else
        meta->occupied_head = block->next;

    if (block->next != nullptr)
        block->next->prev = block->prev;

    block->owner = nullptr;
    block->prev = nullptr;
    block->next = nullptr;

    auto* right = reinterpret_cast<block_header*>(
        reinterpret_cast<std::byte*>(block) + block->block_size
    );

    if (reinterpret_cast<std::byte*>(right) < end && right->owner == nullptr)
    {
        block->block_size += right->block_size;
    }

    block_header* left = nullptr;
    auto* cur = get_first_block();

    while (reinterpret_cast<std::byte*>(cur) < reinterpret_cast<std::byte*>(block))
    {
        auto* next = reinterpret_cast<block_header*>(
            reinterpret_cast<std::byte*>(cur) + cur->block_size);
        if (next == block)
        {
            left = cur;
            break;
        }
        cur = next;
    }

    if (left != nullptr && left->owner == nullptr)
    {
        left->block_size += block->block_size;
    }
}

inline void allocator_boundary_tags::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    auto* meta = get_meta();
    std::lock_guard<std::mutex> lock(meta->mutex);
    meta->fit_mode = mode;
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info() const
{
    auto* meta = get_meta();
    std::lock_guard<std::mutex> lock(meta->mutex);
    return get_blocks_info_inner();
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::begin() const noexcept
{
    if (_trusted_memory == nullptr) return boundary_iterator();
    return boundary_iterator(_trusted_memory);
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::end() const noexcept
{
    return boundary_iterator();
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info_inner() const
{
    auto* meta = get_meta();
    auto* current = get_first_block();
    auto* end = reinterpret_cast<std::byte*>(_trusted_memory)
                + sizeof(allocator_meta)
                + meta->space_size;

    std::vector<allocator_test_utils::block_info> result;
    while (reinterpret_cast<std::byte*>(current) < end)
    {
        result.push_back({
            current->block_size,
            current->owner != nullptr
        });

        current = reinterpret_cast<block_header*>(reinterpret_cast<std::byte*>(current)
                + current->block_size);
    }
    return result;
}

allocator_boundary_tags::allocator_boundary_tags(const allocator_boundary_tags &other)
{
    if (other._trusted_memory == nullptr)
    {
        _trusted_memory = nullptr;
        return;
    }
    auto* other_meta = other.get_meta();
    std::lock_guard<std::mutex> lock(other_meta->mutex);
    size_t total_size = sizeof(allocator_meta) + other_meta->space_size;
    auto* parent = other_meta->parent_allocator;

    _trusted_memory = parent->allocate(total_size);
    std::memcpy(_trusted_memory, other._trusted_memory, total_size);
    auto* meta = get_meta();
    new (&meta->mutex) std::mutex();

    auto* old_base = reinterpret_cast<std::byte*>(other._trusted_memory);
    auto* new_base = reinterpret_cast<std::byte*>(_trusted_memory);
    if (other_meta->occupied_head)
    {
        auto* old_head = reinterpret_cast<std::byte*>(other_meta->occupied_head);
        meta->occupied_head = reinterpret_cast<block_header*>(new_base + (old_head - old_base));
    }
    else
    {
        meta->occupied_head = nullptr;
    }

    auto* current = reinterpret_cast<block_header*>(new_base + sizeof(allocator_meta));
    auto* end = new_base + total_size;

    while(reinterpret_cast<std::byte*>(current) < end)
    {
        if (current->owner != nullptr) current->owner = this;
        if (current->prev)
            current->prev = reinterpret_cast<block_header*>(new_base + 
                            (reinterpret_cast<std::byte*>(current->prev) - old_base));
        if (current->next)
            current->next = reinterpret_cast<block_header*>(new_base + 
                            (reinterpret_cast<std::byte*>(current->next) - old_base));
        current = reinterpret_cast<block_header*>(reinterpret_cast<std::byte*>(current)
                + current->block_size);
    }
}

allocator_boundary_tags &allocator_boundary_tags::operator=(const allocator_boundary_tags &other)
{
    if (this == &other) return *this;
    void* new_memory = nullptr;
    if (other._trusted_memory != nullptr)
    {
        auto* other_meta = other.get_meta();
        std::lock_guard<std::mutex> lock(other_meta->mutex);
        size_t total_size = sizeof(allocator_meta) + other_meta->space_size;
        auto* parent = other_meta->parent_allocator;

        new_memory = parent->allocate(total_size);
        std::memcpy(new_memory, other._trusted_memory, total_size);
        auto* new_meta = reinterpret_cast<allocator_meta*>(new_memory);
        new (&new_meta->mutex) std::mutex();

        if (other_meta->occupied_head)
        {
            auto* old_base = reinterpret_cast<std::byte*>(other._trusted_memory);
            auto* new_base = reinterpret_cast<std::byte*>(new_memory);
            auto* old_head = reinterpret_cast<std::byte*>(other_meta->occupied_head);
            new_meta->occupied_head = reinterpret_cast<block_header*>(new_base + (old_head - old_base));
        }
        auto* old_base = reinterpret_cast<std::byte*>(other._trusted_memory);
        auto* new_base = reinterpret_cast<std::byte*>(new_memory);

        auto* current = reinterpret_cast<block_header*>(new_base + sizeof(allocator_meta));
        auto* end = new_base + total_size;

        while (reinterpret_cast<std::byte*>(current) < end)
        {
            if (current->owner != nullptr)
                current->owner = this;

            if (current->prev)
                current->prev = reinterpret_cast<block_header*>(new_base +
                                (reinterpret_cast<std::byte*>(current->prev) - old_base));
            if (current->next)
                current->next = reinterpret_cast<block_header*>(new_base + 
                    (reinterpret_cast<std::byte*>(current->next) - old_base));
            current = reinterpret_cast<block_header*>(reinterpret_cast<std::byte*>(current) + current->block_size);
        }
    }
    if (_trusted_memory != nullptr)
    {
        auto* meta = get_meta();
        auto* parent = meta->parent_allocator;
        size_t total_size = sizeof(allocator_meta) + meta->space_size;
        
        { std::lock_guard<std::mutex> lock(meta->mutex); }
        
        meta->mutex.~mutex();
        parent->deallocate(_trusted_memory, total_size);
    }
    _trusted_memory = new_memory;
    return *this;
}

bool allocator_boundary_tags::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == &other;
}


bool allocator_boundary_tags::boundary_iterator::operator==(
        const allocator_boundary_tags::boundary_iterator &other) const noexcept
{
    return _occupied_ptr == other._occupied_ptr;
}

bool allocator_boundary_tags::boundary_iterator::operator!=(
        const allocator_boundary_tags::boundary_iterator & other) const noexcept
{
    return !(*this == other);
}

allocator_boundary_tags::boundary_iterator &allocator_boundary_tags::boundary_iterator::operator++() & noexcept
{
    if (_occupied_ptr == nullptr || _trusted_memory == nullptr) return *this;
    auto* meta = reinterpret_cast<allocator_boundary_tags::allocator_meta*>(_trusted_memory);
    auto* end = reinterpret_cast<std::byte*>(_trusted_memory) + sizeof(allocator_meta)
                + meta->space_size;
    auto* current_block = reinterpret_cast<block_header*>(_occupied_ptr);
    auto* next_block = reinterpret_cast<block_header*>(
        reinterpret_cast<std::byte*>(current_block) + current_block->block_size);
    
    if (reinterpret_cast<std::byte*>(next_block) >= end) _occupied_ptr = nullptr;
    else _occupied_ptr = next_block;
    return *this;
}

allocator_boundary_tags::boundary_iterator &allocator_boundary_tags::boundary_iterator::operator--() & noexcept
{
    if (_trusted_memory == nullptr) return *this;

    auto* meta = reinterpret_cast<allocator_boundary_tags::allocator_meta*>(_trusted_memory);
    auto* first = reinterpret_cast<std::byte*>(_trusted_memory) + sizeof(allocator_meta);
    auto* end = first + meta->space_size;

    if (_occupied_ptr == nullptr)
    {
        auto* cur = reinterpret_cast<block_header*>(first);
        auto* last = cur;

        while (reinterpret_cast<std::byte*>(cur) < end)
        {
            last = cur;
            auto* next = reinterpret_cast<block_header*>(
                reinterpret_cast<std::byte*>(cur) + cur->block_size
            );
            if (reinterpret_cast<std::byte*>(next) >= end) break;
            cur = next;
        }
        _occupied_ptr = last;
        return *this;
    }
    if (_occupied_ptr == first) return *this; 
    auto* cur = reinterpret_cast<block_header*>(first);
    block_header* prev_block = nullptr;

    while (reinterpret_cast<void*>(cur) != _occupied_ptr)
    {
        prev_block = cur;
        cur = reinterpret_cast<block_header*>(
            reinterpret_cast<std::byte*>(cur) + cur->block_size
        );
    }
    _occupied_ptr = prev_block;
    return *this;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator++(int n)
{
    boundary_iterator tmp = *this;
    ++(*this);
    return tmp;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator--(int n)
{
    boundary_iterator tmp = *this;
    --(*this);
    return tmp;
}

size_t allocator_boundary_tags::boundary_iterator::size() const noexcept
{
    auto* block = reinterpret_cast<block_header*>(_occupied_ptr);
    return block->block_size - sizeof(block_header);
}

bool allocator_boundary_tags::boundary_iterator::occupied() const noexcept
{
    auto* block = reinterpret_cast<block_header*>(_occupied_ptr);
    return block->owner != nullptr;
}

void* allocator_boundary_tags::boundary_iterator::operator*() const noexcept
{
    return reinterpret_cast<std::byte*>(_occupied_ptr) + sizeof(block_header);
}

allocator_boundary_tags::boundary_iterator::boundary_iterator()
{
    _occupied_ptr = nullptr;
    _occupied = false;
    _trusted_memory = nullptr;
}

allocator_boundary_tags::boundary_iterator::boundary_iterator(void *trusted)
{
    _trusted_memory = trusted;
    _occupied_ptr = nullptr;
    _occupied = false;

    if (trusted == nullptr) return;
    _occupied_ptr = reinterpret_cast<std::byte*>(trusted) + sizeof(allocator_meta);
}

void *allocator_boundary_tags::boundary_iterator::get_ptr() const noexcept
{
    return _occupied_ptr;
}