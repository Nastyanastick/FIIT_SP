#include <not_implemented.h>
#include "../include/allocator_sorted_list.h"
#include <cstring>


namespace
{
    struct allocator_meta
    {
        size_t total_size; // размер всей памяти аллокатора (доверенная область)
        std::pmr::memory_resource* parent_allocator; // у этого аллокатора мы просили память для нашей доверенной области
        allocator_with_fit_mode::fit_mode mode; // режим поиска
        void* first_free_block;
    };

    struct free_block_header
    {
        size_t block_size;
        void* next_free;
        allocator_sorted_list* owner; // владелец блока
        bool is_free;
    };
}

static const allocator_meta* get_meta(const void* trusted_memory) noexcept // мы говорим, что по этому адресу лежит аллокатор (как структура)
{
    return reinterpret_cast<const allocator_meta*>(trusted_memory); // преобразование типом из void в allocator_meta
}

static allocator_meta* get_meta(void* trusted_memory) noexcept // для записи
{
    return reinterpret_cast<allocator_meta*>(trusted_memory);
}
 
allocator_sorted_list::~allocator_sorted_list()
{
    if (_trusted_memory == nullptr) return;
    auto* meta = get_meta(_trusted_memory);
    meta->parent_allocator->deallocate(_trusted_memory, meta->total_size);
}

allocator_sorted_list::allocator_sorted_list( // перенос владения памятью
    allocator_sorted_list &&other) noexcept
{
    std::lock_guard<std::mutex> lock(other._mtx);
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_sorted_list &allocator_sorted_list::operator=( // сналчала обнулить свою, потом уже перенос
    allocator_sorted_list &&other) noexcept
{
    if (this == &other) return *this; // не присваиваем ли мы сами себе (проверка)

    std::scoped_lock lock(_mtx, other._mtx); // блокируем мьютекс обоих объектов
    if (_trusted_memory != nullptr)
    {
        auto* meta = get_meta(_trusted_memory);
        meta->parent_allocator->deallocate(_trusted_memory, meta->total_size);
    }
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
    return *this;
}

allocator_sorted_list::allocator_sorted_list(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    if (parent_allocator == nullptr) parent_allocator = std::pmr::get_default_resource(); // если нет родитея -> берем из глобальной кучи
    if (space_size < sizeof(allocator_meta) + sizeof(free_block_header))
    {
        throw std::bad_alloc();
    }
    _trusted_memory = parent_allocator->allocate(space_size); // указатель на начало выделенной памяти
    auto* meta = get_meta(_trusted_memory);
    meta->total_size = space_size;
    meta->parent_allocator = parent_allocator;
    meta->mode = allocate_fit_mode;
    meta->first_free_block = nullptr;
    auto* first_block = reinterpret_cast<free_block_header*>(reinterpret_cast<unsigned char*>(_trusted_memory) + sizeof(allocator_meta));
    first_block->block_size = space_size - sizeof(allocator_meta) - sizeof(free_block_header);
    first_block->next_free = nullptr;
    first_block->is_free = true;
    first_block->owner = this;
    meta->first_free_block = first_block;
}

[[nodiscard]] void *allocator_sorted_list::do_allocate_sm(
    size_t size)
{
    std::lock_guard<std::mutex>lock(_mtx);
    if (size == 0) return nullptr;
    auto* meta = get_meta(_trusted_memory);
    auto* current = reinterpret_cast<free_block_header*>(meta->first_free_block); // текущий
    free_block_header* previous = nullptr; // предыдущий
    if (meta->mode == fit_mode::first_fit)
    {
        while (current != nullptr) // первый свободный first_fit
        {
        if (current->block_size >= size) break;
        previous = current;
        current = reinterpret_cast <free_block_header*>(current->next_free);
        }
        if (current == nullptr) throw std::bad_alloc();
    }
    else if (meta->mode == fit_mode::the_best_fit)
    {
        free_block_header* best_block = nullptr;
        free_block_header* best_previous = nullptr;
        while (current != nullptr) // ищем первый подходящий блок для best_fit
        {
            if (current->block_size >= size)
            {
                if (best_block == nullptr || current->block_size < best_block->block_size)
                {
                    best_block = current;
                    best_previous = previous;
                }
            }
            previous = current;
            current = reinterpret_cast<free_block_header*>(current->next_free);
        }
        if (best_block == nullptr) throw std::bad_alloc();
        current = best_block;
        previous = best_previous;
    }
    else if (meta->mode == fit_mode::the_worst_fit)
    {
        free_block_header* worst_block = nullptr;
        free_block_header* worst_previous = nullptr;

        while (current != nullptr)
        {
            if (current->block_size >= size)
            {
                if (worst_block == nullptr || current->block_size > worst_block->block_size)
                {
                    worst_block = current;
                    worst_previous = previous;
                }
            }
            previous = current;
            current = reinterpret_cast<free_block_header*>(current->next_free);
        }
        if (worst_block == nullptr) throw std::bad_alloc();
        current = worst_block;
        previous = worst_previous;
    }

    if (current == nullptr) return nullptr;
    if (current->block_size > size + sizeof(free_block_header))
    {
        size_t old_size = current->block_size;
        current->block_size = size;
        current->is_free = false;
        current->owner = this;
        auto* new_block = reinterpret_cast<free_block_header*>(reinterpret_cast<unsigned char*>(current) + sizeof(free_block_header) + size); // остаток старого блока (свободный)
        new_block->block_size = old_size - size - sizeof(free_block_header);
        new_block->next_free = current->next_free;
        new_block->is_free = true;
        new_block->owner = this;
        if (previous == nullptr) meta->first_free_block = new_block;
        else previous->next_free = new_block;
        return reinterpret_cast<void*>(reinterpret_cast<unsigned char*>(current) + sizeof(free_block_header)); // начало того, куда записали
    }
    if (previous == nullptr) meta->first_free_block = current->next_free;
    else previous->next_free = current->next_free;
    current->is_free = false;
    current->owner = this;
    return reinterpret_cast<void*>(reinterpret_cast <unsigned char*>(current) + sizeof(free_block_header));

}

allocator_sorted_list::allocator_sorted_list(const allocator_sorted_list &other) // копирование
{
    std::lock_guard<std::mutex> lock(other._mtx);
    if (other._trusted_memory == nullptr)
    {
        _trusted_memory = nullptr;
        return;
    }
    auto* other_meta = get_meta(other._trusted_memory);
    size_t total_size = other_meta->total_size;
    auto* parent = other_meta->parent_allocator;
    _trusted_memory = parent->allocate(total_size);
    std::memcpy(_trusted_memory, other._trusted_memory, total_size); // куда, откуда, сколько

    auto* new_meta = get_meta(_trusted_memory);
    auto* old_base = reinterpret_cast<unsigned char*>(other._trusted_memory);
    auto* new_base = reinterpret_cast<unsigned char*>(_trusted_memory);

    if (new_meta->first_free_block != nullptr)
    {
        auto* old_p = reinterpret_cast<unsigned char*>(new_meta->first_free_block);
        new_meta->first_free_block = new_base + (old_p - old_base);
    }

    auto* current = new_base + sizeof(allocator_meta);
    auto* end = new_base + new_meta->total_size;

    while (current < end)
    {
        auto* block = reinterpret_cast<free_block_header*>(current);
        block->owner = this;
        if (block->is_free && block->next_free != nullptr)
        {
            auto* old_p = reinterpret_cast<unsigned char*>(block->next_free);
            block->next_free = new_base + (old_p - old_base);
        }
        current += sizeof(free_block_header) + block->block_size;
    }
}

allocator_sorted_list &allocator_sorted_list::operator=(const allocator_sorted_list &other) // копирование присваиванием
{
    if (this == &other) return *this;
    std::scoped_lock lock(_mtx, other._mtx);
    void* new_memory = nullptr;

    if (other._trusted_memory != nullptr)
    {
        auto* other_meta = get_meta(other._trusted_memory);
        size_t total_size = other_meta->total_size;
        auto* parent = other_meta->parent_allocator;

        new_memory = parent->allocate(total_size);
        std::memcpy(new_memory, other._trusted_memory, total_size);

        auto* new_meta = get_meta(new_memory);
        auto* old_base = reinterpret_cast<unsigned char*>(other._trusted_memory);
        auto* new_base = reinterpret_cast<unsigned char*>(new_memory);

        if (new_meta->first_free_block != nullptr)
        {
            auto* old_p = reinterpret_cast<unsigned char*>(new_meta->first_free_block);
            new_meta->first_free_block = new_base + (old_p - old_base);
        }
        auto* current = new_base + sizeof(allocator_meta);
        auto* end = new_base + new_meta->total_size;

        while (current < end)
        {
            auto* block = reinterpret_cast<free_block_header*>(current);
            block->owner = this;
            if (block->is_free && block->next_free != nullptr)
            {
                auto* old_p = reinterpret_cast<unsigned char*>(block->next_free);
                block->next_free = new_base + (old_p - old_base);
            }
            current += sizeof(free_block_header) + block->block_size;
        }
    }
    if (_trusted_memory != nullptr)
    {
        auto* meta = get_meta(_trusted_memory);
        meta->parent_allocator->deallocate(_trusted_memory, meta->total_size);
    }
    _trusted_memory = new_memory;
    return *this;

}

bool allocator_sorted_list::do_is_equal(const std::pmr::memory_resource &other) const noexcept // равны ли два ресурса
{
    return this == &other;
}

void allocator_sorted_list::do_deallocate_sm(void *at)
{
    std::lock_guard<std::mutex> lock(_mtx);
    if (at == nullptr) return;
    auto* meta = get_meta(_trusted_memory);
    auto* start = reinterpret_cast<unsigned char*>(_trusted_memory); // начало области
    auto* end = start + meta->total_size; // конец области
    auto* min_start = start + sizeof(allocator_meta) + sizeof(free_block_header);
    auto* p = reinterpret_cast<unsigned char*>(at);
    if (p < min_start || p >= end)
    {
        throw std::logic_error("pointer does not belong to this allocator");
    }
    auto* block = reinterpret_cast<free_block_header*>(reinterpret_cast<unsigned char*>(at) - sizeof(free_block_header)); // заголовок освобождаемого блока
    if (block->owner != this)
    {
        throw std::logic_error("block does not belong to this allocator");
    }
    if (block->is_free)
    {
        throw std::logic_error("block if already free");
    }
    block->is_free = true;
    auto* current = reinterpret_cast<free_block_header*>(meta->first_free_block); // близкий больше
    free_block_header* previous = nullptr; // близкий меньше

    while (current != nullptr && current < block)
    {
        previous = current;
        current = reinterpret_cast<free_block_header*>(current->next_free);
    }

    block->next_free = current;

    if (previous == nullptr) meta->first_free_block = block;
    else previous->next_free = block;

    if (current != nullptr && reinterpret_cast<unsigned char*>(block) + sizeof(free_block_header) + block->block_size 
    == reinterpret_cast<unsigned char*>(current))
    {
        // можно склеивать
        block->block_size += current->block_size + sizeof(free_block_header);
        block->next_free = current->next_free;
    }
    if (previous != nullptr && reinterpret_cast<unsigned char*>(previous) + sizeof(free_block_header) + previous->block_size 
    == reinterpret_cast<unsigned char*>(block))
    {
        previous->next_free = block->next_free;
        previous->block_size += block->block_size + sizeof(free_block_header);
    }

}

inline void allocator_sorted_list::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode) // смена текущего режима поиска
{
    std::lock_guard<std::mutex> lock(_mtx);
    auto* meta = get_meta(_trusted_memory);
    meta->mode = mode;
}

std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info() const noexcept // просто вызывает get_blocks_info_inner
{
    std::lock_guard<std::mutex> lock(_mtx);
    return get_blocks_info_inner();
}


std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info_inner() const // проходит всю память аллокатора и возвращает список блоков
{
    auto* meta = get_meta(_trusted_memory);
    auto* current = reinterpret_cast<unsigned char*>(_trusted_memory) + sizeof(allocator_meta);
    auto* end = reinterpret_cast<unsigned char*>(_trusted_memory) + meta->total_size;
    std::vector<allocator_test_utils::block_info> result;

    while (current < end)
    {
        auto* block = reinterpret_cast<free_block_header*>(current);
        result.push_back({block->block_size, block->is_free});
        current += sizeof(free_block_header) + block->block_size;
    }
    return result;
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::free_begin() const noexcept
{
    if (_trusted_memory == nullptr) return sorted_free_iterator(nullptr);
    auto* meta = get_meta(_trusted_memory);
    return sorted_free_iterator(meta->first_free_block);
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::free_end() const noexcept
{
    return sorted_free_iterator(nullptr);
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::begin() const noexcept
{
    if (_trusted_memory == nullptr) return sorted_iterator();
    return sorted_iterator(_trusted_memory);
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::end() const noexcept
{
    return sorted_iterator();
}


bool allocator_sorted_list::sorted_free_iterator::operator==(
        const allocator_sorted_list::sorted_free_iterator & other) const noexcept
{
    return _free_ptr == other._free_ptr;
}

bool allocator_sorted_list::sorted_free_iterator::operator!=(
        const allocator_sorted_list::sorted_free_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_sorted_list::sorted_free_iterator &allocator_sorted_list::sorted_free_iterator::operator++() & noexcept
{
    if (_free_ptr != nullptr)
    {
        auto* block = reinterpret_cast<free_block_header*>(_free_ptr);
        _free_ptr = block->next_free;
    }
    return *this;
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::sorted_free_iterator::operator++(int n)
{
    (void)n;
    sorted_free_iterator tmp = *this;
    ++(*this);
    return tmp;
}

size_t allocator_sorted_list::sorted_free_iterator::size() const noexcept
{
    auto* block = reinterpret_cast<free_block_header*>(_free_ptr);
    return block->block_size;
}

void *allocator_sorted_list::sorted_free_iterator::operator*() const noexcept
{
    return reinterpret_cast<unsigned char*>(_free_ptr) + sizeof(free_block_header);
}

allocator_sorted_list::sorted_free_iterator::sorted_free_iterator()
{
    _free_ptr = nullptr;
}

allocator_sorted_list::sorted_free_iterator::sorted_free_iterator(void *trusted)
{
    _free_ptr = trusted;
}



bool allocator_sorted_list::sorted_iterator::operator==(const allocator_sorted_list::sorted_iterator & other) const noexcept
{
    return _current_ptr == other._current_ptr;
}

bool allocator_sorted_list::sorted_iterator::operator!=(const allocator_sorted_list::sorted_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_sorted_list::sorted_iterator &allocator_sorted_list::sorted_iterator::operator++() & noexcept
{
    if (_current_ptr == nullptr || _trusted_memory == nullptr) return *this;

    auto* meta = get_meta(_trusted_memory);
    auto* current_block = reinterpret_cast<free_block_header*>(_current_ptr);

    if (_free_ptr == _current_ptr) _free_ptr = current_block->next_free;
    auto* next_block = reinterpret_cast<unsigned char*>(_current_ptr) + sizeof(free_block_header) + current_block->block_size;
    auto* end = reinterpret_cast<unsigned char*>(_trusted_memory) + meta->total_size;
    if (next_block >= end)
    {
        _current_ptr = nullptr;
        _free_ptr = nullptr;
    }
    else _current_ptr = next_block;
    return *this;
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::sorted_iterator::operator++(int n)
{
    (void)n;
    sorted_iterator tmp = *this;
    ++(*this);
    return tmp;
}

size_t allocator_sorted_list::sorted_iterator::size() const noexcept
{
    auto* block = reinterpret_cast<free_block_header*>(_current_ptr);
    return block->block_size;
}

void *allocator_sorted_list::sorted_iterator::operator*() const noexcept
{
    return reinterpret_cast<unsigned char*>(_current_ptr) + sizeof(free_block_header);
}

allocator_sorted_list::sorted_iterator::sorted_iterator()
{
    _free_ptr = nullptr;
    _current_ptr = nullptr;
    _trusted_memory = nullptr;
}

allocator_sorted_list::sorted_iterator::sorted_iterator(void *trusted)
{
    _free_ptr = nullptr;
    _current_ptr = nullptr;
    _trusted_memory = trusted;
    if (trusted == nullptr) return;
    auto* meta = get_meta(trusted);
    _current_ptr = reinterpret_cast<unsigned char*>(trusted) + sizeof(allocator_meta);
    _free_ptr = meta->first_free_block;

}

bool allocator_sorted_list::sorted_iterator::occupied() const noexcept
{
    if (_current_ptr == nullptr) return false;
    return _current_ptr != _free_ptr;
}
