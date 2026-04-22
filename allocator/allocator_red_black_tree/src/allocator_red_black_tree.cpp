#include <not_implemented.h>
#include <cstring>

#include "../include/allocator_red_black_tree.h"

allocator_red_black_tree::allocator_meta* allocator_red_black_tree::meta() noexcept
{
    return reinterpret_cast<allocator_meta*>(_trusted_memory);
}

const allocator_red_black_tree::allocator_meta* allocator_red_black_tree::meta() const noexcept
{
    return reinterpret_cast<const allocator_meta*>(_trusted_memory);
}

void* allocator_red_black_tree::first_block() noexcept
{
    return reinterpret_cast<char*>(_trusted_memory) + allocator_metadata_size;
}

const void* allocator_red_black_tree::first_block() const noexcept
{
    return reinterpret_cast<const char*>(_trusted_memory) + allocator_metadata_size;
}

allocator_red_black_tree::block_data& allocator_red_black_tree::block_header(void* block) noexcept
{
    return reinterpret_cast<occupied_block_meta*>(block)->data;
}

const allocator_red_black_tree::block_data& allocator_red_black_tree::block_header(const void* block) const noexcept
{
    return reinterpret_cast<const occupied_block_meta*>(block)->data;
}

size_t& allocator_red_black_tree::block_size(void* block) noexcept
{
    return reinterpret_cast<occupied_block_meta*>(block)->size;
}

const size_t& allocator_red_black_tree::block_size(const void* block) const noexcept
{
    return reinterpret_cast<const occupied_block_meta*>(block)->size;
}

void*& allocator_red_black_tree::block_prev(void* block) noexcept
{
    return reinterpret_cast<occupied_block_meta*>(block)->prev;
}

void* const& allocator_red_black_tree::block_prev(const void* block) const noexcept
{
    return reinterpret_cast<const occupied_block_meta*>(block)->prev;
}


void* allocator_red_black_tree::next_block(void* block) noexcept
{
    return reinterpret_cast<char*>(block) + block_size(block);
}

const void* allocator_red_black_tree::next_block(const void* block) const noexcept
{
    return reinterpret_cast<const char*>(block) + block_size(block);
}

bool allocator_red_black_tree::is_last_block(const void* block) const noexcept
{
    return next_block(block) >= reinterpret_cast<const char*>(_trusted_memory) + meta()->space_size;
}

void* allocator_red_black_tree::user_memory(void* block) noexcept
{
    return reinterpret_cast<char*>(block) + occupied_block_metadata_size;
}

const void* allocator_red_black_tree::user_memory(const void* block) const noexcept
{
    return reinterpret_cast<const char*>(block) + occupied_block_metadata_size;
}

void* allocator_red_black_tree::block_from_user_memory(void* user_ptr) noexcept
{
    return reinterpret_cast<char*>(user_ptr) - occupied_block_metadata_size;
}

const void* allocator_red_black_tree::block_from_user_memory(const void* user_ptr) const noexcept
{
    return reinterpret_cast<const char*>(user_ptr) - occupied_block_metadata_size;
}



allocator_red_black_tree::block_color allocator_red_black_tree::color_of(const void* block) const noexcept
{
    if (block == nullptr)
        return block_color::BLACK;

    return as_free(block)->data.color;
}

void allocator_red_black_tree::set_color(void* block, block_color color) noexcept
{
    if (block != nullptr)
        as_free(block)->data.color = color;
}

void* allocator_red_black_tree::parent_of(void* block) const noexcept
{
    return block == nullptr ? nullptr : as_free(block)->parent;
}

const void* allocator_red_black_tree::parent_of(const void* block) const noexcept
{
    return block == nullptr ? nullptr : as_free(block)->parent;
}

void* allocator_red_black_tree::left_of(void* block) const noexcept
{
    return block == nullptr ? nullptr : as_free(block)->left;
}

const void* allocator_red_black_tree::left_of(const void* block) const noexcept
{
    return block == nullptr ? nullptr : as_free(block)->left;
}

void* allocator_red_black_tree::right_of(void* block) const noexcept
{
    return block == nullptr ? nullptr : as_free(block)->right;
}

const void* allocator_red_black_tree::right_of(const void* block) const noexcept
{
    return block == nullptr ? nullptr : as_free(block)->right;
}

void allocator_red_black_tree::set_parent(void* block, void* parent) noexcept
{
    if (block != nullptr)
        as_free(block)->parent = parent;
}

void allocator_red_black_tree::set_left(void* block, void* left) noexcept
{
    if (block != nullptr)
        as_free(block)->left = left;
}

void allocator_red_black_tree::set_right(void* block, void* right) noexcept
{
    if (block != nullptr)
        as_free(block)->right = right;
}

bool allocator_red_black_tree::less_blocks(const void* lhs, const void* rhs) const noexcept
{
    const size_t lhs_size = block_size(lhs);
    const size_t rhs_size = block_size(rhs);

    if (lhs_size != rhs_size)
        return lhs_size < rhs_size;

    return lhs < rhs;
}



allocator_red_black_tree::free_block_meta* allocator_red_black_tree::as_free(void* block) noexcept
{
    return reinterpret_cast<free_block_meta*>(block);
}

const allocator_red_black_tree::free_block_meta* allocator_red_black_tree::as_free(const void* block) const noexcept
{
    return reinterpret_cast<const free_block_meta*>(block);
}

allocator_red_black_tree::occupied_block_meta* allocator_red_black_tree::as_occupied(void* block) noexcept
{
    return reinterpret_cast<occupied_block_meta*>(block);
}

const allocator_red_black_tree::occupied_block_meta* allocator_red_black_tree::as_occupied(const void* block) const noexcept
{
    return reinterpret_cast<const occupied_block_meta*>(block);
}

void allocator_red_black_tree::init_free_block(void* block, size_t size, void* prev) noexcept
{
    auto* b = as_free(block);
    b->data.occupied = false;
    b->data.color = block_color::RED;
    b->size = size;
    b->prev = prev;
    b->parent = nullptr;
    b->left = nullptr;
    b->right = nullptr;
}

void allocator_red_black_tree::init_occupied_block(void* block, size_t size, void* prev) noexcept
{
    auto* b = as_occupied(block);
    b->data.occupied = true;
    b->data.color = block_color::BLACK;
    b->size = size;
    b->prev = prev;
    b->owner = _trusted_memory;
}

void* allocator_red_black_tree::find_first_fit(size_t required_size) noexcept
{
    return find_best_fit(required_size);
}

void* allocator_red_black_tree::find_best_fit(size_t required_size) noexcept
{
    void* current = meta()->free_root;
    void* result = nullptr;

    while (current != nullptr)
    {
        if (block_size(current) >= required_size)
        {
            result = current;
            current = left_of(current);
        }
        else
        {
            current = right_of(current);
        }
    }

    return result;
}

void* allocator_red_black_tree::find_worst_fit(size_t required_size) noexcept
{
    void* current = meta()->free_root;
    void* result = nullptr;

    while (current != nullptr)
    {
        if (block_size(current) >= required_size)
        {
            result = current;
            current = right_of(current);
        }
        else
        {
            current = right_of(current);
        }
    }

    return result;
}

void* allocator_red_black_tree::find_suitable_block(size_t required_size) noexcept
{
    switch (meta()->mode)
    {
        case fit_mode::first_fit:
            return find_first_fit(required_size);

        case fit_mode::the_best_fit:
            return find_best_fit(required_size);

        case fit_mode::the_worst_fit:
            return find_worst_fit(required_size);
    }

    return nullptr;
}

void allocator_red_black_tree::left_rotate(void* x) noexcept
{
    void* y = right_of(x);
    set_right(x, left_of(y));

    if (left_of(y) != nullptr)
        set_parent(left_of(y), x);

    set_parent(y, parent_of(x));

    if (parent_of(x) == nullptr)
    {
        meta()->free_root = y;
    }
    else if (x == left_of(parent_of(x)))
    {
        set_left(parent_of(x), y);
    }
    else
    {
        set_right(parent_of(x), y);
    }

    set_left(y, x);
    set_parent(x, y);
}

void allocator_red_black_tree::right_rotate(void* x) noexcept
{
    void* y = left_of(x);
    set_left(x, right_of(y));

    if (right_of(y) != nullptr)
        set_parent(right_of(y), x);

    set_parent(y, parent_of(x));

    if (parent_of(x) == nullptr)
    {
        meta()->free_root = y;
    }
    else if (x == right_of(parent_of(x)))
    {
        set_right(parent_of(x), y);
    }
    else
    {
        set_left(parent_of(x), y);
    }

    set_right(y, x);
    set_parent(x, y);
}

void allocator_red_black_tree::rb_insert_fixup(void* z) noexcept
{
    while (parent_of(z) != nullptr && color_of(parent_of(z)) == block_color::RED)
    {
        void* p = parent_of(z);
        void* g = parent_of(p);

        if (p == left_of(g))
        {
            void* y = right_of(g);

            if (color_of(y) == block_color::RED)
            {
                set_color(p, block_color::BLACK);
                set_color(y, block_color::BLACK);
                set_color(g, block_color::RED);
                z = g;
            }
            else
            {
                if (z == right_of(p))
                {
                    z = p;
                    left_rotate(z);
                    p = parent_of(z);
                    g = parent_of(p);
                }

                set_color(parent_of(z), block_color::BLACK);
                set_color(g, block_color::RED);
                right_rotate(g);
            }
        }
        else
        {
            void* y = left_of(g);

            if (color_of(y) == block_color::RED)
            {
                set_color(p, block_color::BLACK);
                set_color(y, block_color::BLACK);
                set_color(g, block_color::RED);
                z = g;
            }
            else
            {
                if (z == left_of(p))
                {
                    z = p;
                    right_rotate(z);
                    p = parent_of(z);
                    g = parent_of(p);
                }

                set_color(parent_of(z), block_color::BLACK);
                set_color(g, block_color::RED);
                left_rotate(g);
            }
        }
    }

    set_color(meta()->free_root, block_color::BLACK);
}

void* allocator_red_black_tree::tree_minimum(void* node) const noexcept
{
    while (left_of(node) != nullptr)
        node = left_of(node);
    return node;
}

void allocator_red_black_tree::transplant(void* u, void* v) noexcept
{
    if (parent_of(u) == nullptr)
    {
        meta()->free_root = v;
    }
    else if (u == left_of(parent_of(u)))
    {
        set_left(parent_of(u), v);
    }
    else
    {
        set_right(parent_of(u), v);
    }

    if (v != nullptr)
        set_parent(v, parent_of(u));
}

void allocator_red_black_tree::rb_remove_fixup(void* x, void* x_parent) noexcept
{
    while (x != meta()->free_root && color_of(x) == block_color::BLACK)
    {
        if (x_parent == nullptr) break;
        if (x == left_of(x_parent))
        {
            void* w = right_of(x_parent);

            if (color_of(w) == block_color::RED)
            {
                set_color(w, block_color::BLACK);
                set_color(x_parent, block_color::RED);
                left_rotate(x_parent);
                w = right_of(x_parent);
            }

            if (color_of(left_of(w)) == block_color::BLACK &&
                color_of(right_of(w)) == block_color::BLACK)
            {
                set_color(w, block_color::RED);
                x = x_parent;
                x_parent = parent_of(x);
            }
            else
            {
                if (color_of(right_of(w)) == block_color::BLACK)
                {
                    set_color(left_of(w), block_color::BLACK);
                    set_color(w, block_color::RED);
                    right_rotate(w);
                    w = right_of(x_parent);
                }

                set_color(w, color_of(x_parent));
                set_color(x_parent, block_color::BLACK);
                set_color(right_of(w), block_color::BLACK);
                left_rotate(x_parent);
                x = meta()->free_root;
                x_parent = nullptr;
            }
        }
        else
        {
            void* w = left_of(x_parent);

            if (color_of(w) == block_color::RED)
            {
                set_color(w, block_color::BLACK);
                set_color(x_parent, block_color::RED);
                right_rotate(x_parent);
                w = left_of(x_parent);
            }

            if (color_of(right_of(w)) == block_color::BLACK &&
                color_of(left_of(w)) == block_color::BLACK)
            {
                set_color(w, block_color::RED);
                x = x_parent;
                x_parent = parent_of(x);
            }
            else
            {
                if (color_of(left_of(w)) == block_color::BLACK)
                {
                    set_color(right_of(w), block_color::BLACK);
                    set_color(w, block_color::RED);
                    left_rotate(w);
                    w = left_of(x_parent);
                }

                set_color(w, color_of(x_parent));
                set_color(x_parent, block_color::BLACK);
                set_color(left_of(w), block_color::BLACK);
                right_rotate(x_parent);
                x = meta()->free_root;
                x_parent = nullptr;
            }
        }
    }

    set_color(x, block_color::BLACK);
}

void allocator_red_black_tree::rb_insert(void* block) noexcept
{
    set_left(block, nullptr);
    set_right(block, nullptr);
    set_parent(block, nullptr);
    set_color(block, block_color::RED);

    void* y = nullptr;
    void* x = meta()->free_root;

    while (x != nullptr)
    {
        y = x;
        if (less_blocks(block, x))
            x = left_of(x);
        else
            x = right_of(x);
    }

    set_parent(block, y);

    if (y == nullptr)
    {
        meta()->free_root = block;
    }
    else if (less_blocks(block, y))
    {
        set_left(y, block);
    }
    else
    {
        set_right(y, block);
    }

    rb_insert_fixup(block);
}

void allocator_red_black_tree::rb_remove(void* z) noexcept
{
    void* y = z;
    block_color y_original_color = color_of(y);
    void* x = nullptr;
    void* x_parent = nullptr;

    if (left_of(z) == nullptr)
    {
        x = right_of(z);
        x_parent = parent_of(z);
        transplant(z, right_of(z));
    }
    else if (right_of(z) == nullptr)
    {
        x = left_of(z);
        x_parent = parent_of(z);
        transplant(z, left_of(z));
    }
    else
    {
        y = tree_minimum(right_of(z));
        y_original_color = color_of(y);
        x = right_of(y);

        if (parent_of(y) == z)
        {
            x_parent = y;
            if (x != nullptr)
                set_parent(x, y);
        }
        else
        {
            x_parent = parent_of(y);
            transplant(y, right_of(y));
            set_right(y, right_of(z));
            set_parent(right_of(y), y);
        }

        transplant(z, y);
        set_left(y, left_of(z));
        set_parent(left_of(y), y);
        set_color(y, color_of(z));
    }

    if (y_original_color == block_color::BLACK)
        rb_remove_fixup(x, x_parent);
}







allocator_red_black_tree::~allocator_red_black_tree()
{
    if (_trusted_memory == nullptr)
        return;

    auto* parent = meta()->parent_allocator;
    const size_t space_size = meta()->space_size;

    meta()->mutex.~mutex();
    parent->deallocate(_trusted_memory, space_size);
    _trusted_memory = nullptr;
}

allocator_red_black_tree::allocator_red_black_tree(
    allocator_red_black_tree &&other) noexcept
{
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_red_black_tree &allocator_red_black_tree::operator=(
    allocator_red_black_tree &&other) noexcept
{
    if (this != &other)
    {
        this->~allocator_red_black_tree();
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }
    return *this;
}

allocator_red_black_tree::allocator_red_black_tree(
    size_t space_size,
    std::pmr::memory_resource *parent_allocator,
    allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    if (space_size < allocator_metadata_size + free_block_metadata_size)
        throw std::logic_error("not enough space for allocator_red_black_tree");

    if (parent_allocator == nullptr)
        parent_allocator = std::pmr::get_default_resource();

    _trusted_memory = parent_allocator->allocate(space_size);

    auto* m = meta();
    m->parent_allocator = parent_allocator;
    m->mode = allocate_fit_mode;
    m->space_size = space_size;
    new (&m->mutex) std::mutex();

    void* first = first_block();
    m->free_root = first;

    auto* first_free = as_free(first);
    first_free->data.occupied = false;
    first_free->data.color = block_color::BLACK;
    first_free->size = space_size - allocator_metadata_size;
    first_free->prev = nullptr;
    first_free->parent = nullptr;
    first_free->left = nullptr;
    first_free->right = nullptr;
}

allocator_red_black_tree::allocator_red_black_tree(const allocator_red_black_tree &other)
{
    _trusted_memory = nullptr;
    if (other._trusted_memory == nullptr)
        return;

    std::lock_guard<std::mutex> lock(other.meta()->mutex);

    const auto* other_meta = other.meta();
    std::pmr::memory_resource* parent_allocator = other_meta->parent_allocator;
    if (parent_allocator == nullptr)
        parent_allocator = std::pmr::get_default_resource();

    const size_t space_size = other_meta->space_size;
    void* new_memory = parent_allocator->allocate(space_size);

    try
    {
        std::memcpy(new_memory, other._trusted_memory, space_size);
        _trusted_memory = new_memory;

        auto* new_meta = meta();
        new_meta->mutex.~mutex();
        new (&new_meta->mutex) std::mutex();

        if (new_meta->free_root != nullptr)
        {
            new_meta->free_root =
                reinterpret_cast<char*>(_trusted_memory) +
                (reinterpret_cast<char*>(new_meta->free_root) -
                 reinterpret_cast<const char*>(other._trusted_memory));
        }

        void* current = first_block();
        const char* end = reinterpret_cast<const char*>(_trusted_memory) + new_meta->space_size;

        while (reinterpret_cast<const char*>(current) < end)
        {
            if (block_prev(current) != nullptr)
            {
                block_prev(current) =
                    reinterpret_cast<char*>(_trusted_memory) +
                    (reinterpret_cast<char*>(block_prev(current)) -
                     reinterpret_cast<const char*>(other._trusted_memory));
            }

            if (block_header(current).occupied)
            {
                as_occupied(current)->owner = _trusted_memory;
            }
            else
            {
                auto* fb = as_free(current);

                if (fb->parent != nullptr)
                {
                    fb->parent =
                        reinterpret_cast<char*>(_trusted_memory) +
                        (reinterpret_cast<char*>(fb->parent) -
                         reinterpret_cast<const char*>(other._trusted_memory));
                }

                if (fb->left != nullptr)
                {
                    fb->left =
                        reinterpret_cast<char*>(_trusted_memory) +
                        (reinterpret_cast<char*>(fb->left) -
                         reinterpret_cast<const char*>(other._trusted_memory));
                }

                if (fb->right != nullptr)
                {
                    fb->right =
                        reinterpret_cast<char*>(_trusted_memory) +
                        (reinterpret_cast<char*>(fb->right) -
                         reinterpret_cast<const char*>(other._trusted_memory));
                }
            }

            if (is_last_block(current))
                break;

            current = next_block(current);
        }
    }
    catch (...)
    {
        parent_allocator->deallocate(new_memory, space_size);
        _trusted_memory = nullptr;
        throw;
    }
}

allocator_red_black_tree &allocator_red_black_tree::operator=(const allocator_red_black_tree &other)
{
    if (this == &other)
        return *this;

    if (other._trusted_memory == nullptr)
    {
        if (_trusted_memory != nullptr)
        {
            auto* m = meta();
            auto* parent = m->parent_allocator;
            const size_t space_size = m->space_size;

            m->mutex.~mutex();
            parent->deallocate(_trusted_memory, space_size);
            _trusted_memory = nullptr;
        }
        return *this;
    }

    std::lock_guard<std::mutex> lock(other.meta()->mutex);

    const auto* other_meta = other.meta();
    std::pmr::memory_resource* parent_allocator = other_meta->parent_allocator;
    if (parent_allocator == nullptr)
        parent_allocator = std::pmr::get_default_resource();

    const size_t new_space_size = other_meta->space_size;
    void* new_memory = parent_allocator->allocate(new_space_size);

    try
    {
        std::memcpy(new_memory, other._trusted_memory, new_space_size);

        auto* new_meta = reinterpret_cast<allocator_meta*>(new_memory);

        new_meta->mutex.~mutex();
        new (&new_meta->mutex) std::mutex();

        if (new_meta->free_root != nullptr)
        {
            new_meta->free_root =
                reinterpret_cast<char*>(new_memory) +
                (reinterpret_cast<char*>(new_meta->free_root) -
                 reinterpret_cast<const char*>(other._trusted_memory));
        }

        void* current = reinterpret_cast<char*>(new_memory) + allocator_metadata_size;
        const char* end = reinterpret_cast<const char*>(new_memory) + new_meta->space_size;

        while (reinterpret_cast<const char*>(current) < end)
        {
            auto& prev = reinterpret_cast<occupied_block_meta*>(current)->prev;
            if (prev != nullptr)
            {
                prev = reinterpret_cast<char*>(new_memory) +
                       (reinterpret_cast<char*>(prev) -
                        reinterpret_cast<const char*>(other._trusted_memory));
            }

            auto& hdr = reinterpret_cast<occupied_block_meta*>(current)->data;
            auto& sz = reinterpret_cast<occupied_block_meta*>(current)->size;

            if (hdr.occupied)
            {
                reinterpret_cast<occupied_block_meta*>(current)->owner = new_memory;
            }
            else
            {
                auto* fb = reinterpret_cast<free_block_meta*>(current);

                if (fb->parent != nullptr)
                {
                    fb->parent =
                        reinterpret_cast<char*>(new_memory) +
                        (reinterpret_cast<char*>(fb->parent) -
                         reinterpret_cast<const char*>(other._trusted_memory));
                }

                if (fb->left != nullptr)
                {
                    fb->left =
                        reinterpret_cast<char*>(new_memory) +
                        (reinterpret_cast<char*>(fb->left) -
                         reinterpret_cast<const char*>(other._trusted_memory));
                }

                if (fb->right != nullptr)
                {
                    fb->right =
                        reinterpret_cast<char*>(new_memory) +
                        (reinterpret_cast<char*>(fb->right) -
                         reinterpret_cast<const char*>(other._trusted_memory));
                }
            }

            void* next = reinterpret_cast<char*>(current) + sz;
            if (next >= end)
                break;

            current = next;
        }
    }
    catch (...)
    {
        parent_allocator->deallocate(new_memory, new_space_size);
        throw;
    }

    if (_trusted_memory != nullptr)
    {
        auto* old_meta = meta();
        auto* old_parent = old_meta->parent_allocator;
        const size_t old_space_size = old_meta->space_size;

        old_meta->mutex.~mutex();
        old_parent->deallocate(_trusted_memory, old_space_size);
    }

    _trusted_memory = new_memory;
    return *this;
}

bool allocator_red_black_tree::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == &other;
}

[[nodiscard]] void *allocator_red_black_tree::do_allocate_sm(size_t size)
{
    if (size == 0)
        size = 1;

    std::lock_guard<std::mutex> lock(meta()->mutex);

    const size_t required_size = occupied_block_metadata_size + size;

    void* block = find_suitable_block(required_size);
    if (block == nullptr)
        throw std::bad_alloc();

    rb_remove(block);

    const size_t old_size = block_size(block);
    void* prev = block_prev(block);

    if (old_size >= required_size + free_block_metadata_size)
    {
        void* remainder = reinterpret_cast<char*>(block) + required_size;
        const size_t remainder_size = old_size - required_size;

        init_occupied_block(block, required_size, prev);
        init_free_block(remainder, remainder_size, block);

        if (!is_last_block(remainder))
            block_prev(next_block(remainder)) = remainder;

        rb_insert(remainder);
    }
    else
    {
        init_occupied_block(block, old_size, prev);

        if (!is_last_block(block))
            block_prev(next_block(block)) = block;
    }

    return user_memory(block);
}

void allocator_red_black_tree::do_deallocate_sm(void* at)
{
    if (at == nullptr)
        return;

    std::lock_guard<std::mutex> lock(meta()->mutex);

    void* block = block_from_user_memory(at);

    const char* begin = reinterpret_cast<const char*>(first_block());
    const char* end = reinterpret_cast<const char*>(_trusted_memory) + meta()->space_size;
    const char* ptr = reinterpret_cast<const char*>(block);

    if (ptr < begin || ptr >= end)
        throw std::logic_error("pointer does not belong to allocator");

    auto* occ = as_occupied(block);
    if (occ->owner != _trusted_memory)
        throw std::logic_error("pointer does not belong to allocator");

    void* merged_block = block;
    size_t merged_size = block_size(block);
    void* merged_prev = block_prev(block);

    void* right = nullptr;
    if (!is_last_block(block))
        right = next_block(block);

    if (right != nullptr && !block_header(right).occupied)
    {
        rb_remove(right);
        merged_size += block_size(right);
    }

    void* left = block_prev(block);
    if (left != nullptr && !block_header(left).occupied)
    {
        rb_remove(left);
        merged_block = left;
        merged_prev = block_prev(left);
        merged_size += block_size(left);
    }

    init_free_block(merged_block, merged_size, merged_prev);

    if (!is_last_block(merged_block))
    {
        void* after = next_block(merged_block);
        if (reinterpret_cast<const char*>(after) < end)
            block_prev(after) = merged_block;
    }

    rb_insert(merged_block);
}

void allocator_red_black_tree::set_fit_mode(allocator_with_fit_mode::fit_mode mode)
{
    std::lock_guard<std::mutex> lock(meta()->mutex);
    meta()->mode = mode;
}


std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info() const
{
    std::lock_guard<std::mutex> lock((this->meta())->mutex);
    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> result;

    const void* current = first_block();
    const char* end = reinterpret_cast<const char*>(_trusted_memory) + meta()->space_size;
    while (reinterpret_cast<const char*>(current) < end)
    {
        result.push_back({
            block_size(current),
            block_header(current).occupied
        });
        if (is_last_block(current)) break;
        current = next_block(current);
    }
    return result;
}


allocator_red_black_tree::rb_iterator allocator_red_black_tree::begin() const noexcept
{
    return rb_iterator(_trusted_memory);
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::end() const noexcept
{
    return rb_iterator();
}

bool allocator_red_black_tree::rb_iterator::operator==(const allocator_red_black_tree::rb_iterator &other) const noexcept
{
    return _block_ptr == other._block_ptr && _trusted == other._trusted;
}

bool allocator_red_black_tree::rb_iterator::operator!=(const allocator_red_black_tree::rb_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_red_black_tree::rb_iterator &allocator_red_black_tree::rb_iterator::operator++() & noexcept
{
    if (_block_ptr == nullptr || _trusted == nullptr)
        return *this;

    auto* meta = reinterpret_cast<allocator_meta*>(_trusted);
    auto* end = reinterpret_cast<char*>(_trusted) + meta->space_size;

    auto* current = reinterpret_cast<char*>(_block_ptr);
    auto current_size = reinterpret_cast<occupied_block_meta*>(_block_ptr)->size;
    auto* next = current + current_size;

    if (next >= end)
        _block_ptr = nullptr;
    else
        _block_ptr = next;

    return *this;
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::rb_iterator::operator++(int n)
{
    rb_iterator copy = *this;
    ++(*this);
    return copy;
}

size_t allocator_red_black_tree::rb_iterator::size() const noexcept
{
    if (_block_ptr == nullptr)
        return 0;
    return reinterpret_cast<occupied_block_meta*>(_block_ptr)->size;
}

void *allocator_red_black_tree::rb_iterator::operator*() const noexcept
{
    return _block_ptr;
}

allocator_red_black_tree::rb_iterator::rb_iterator()
{
    _block_ptr = nullptr;
    _trusted = nullptr;
}

allocator_red_black_tree::rb_iterator::rb_iterator(void *trusted)
{
    _block_ptr = nullptr;
    _trusted = trusted;
    if (_trusted == nullptr)
    {
        _block_ptr = nullptr;
        return;
    }

    _block_ptr = reinterpret_cast<char*>(_trusted) + allocator_metadata_size;
}

bool allocator_red_black_tree::rb_iterator::occupied() const noexcept
{
    if (_block_ptr == nullptr)
        return false;
    return reinterpret_cast<occupied_block_meta*>(_block_ptr)->data.occupied;
}
