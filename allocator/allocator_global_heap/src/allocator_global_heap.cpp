#include <not_implemented.h>
#include "../include/allocator_global_heap.h"
#include <cstdlib>

allocator_global_heap::allocator_global_heap()
{
// мьютекс сам создается, конструктор ничего не делает
}

[[nodiscard]] void *allocator_global_heap::do_allocate_sm(size_t size)
{
    std::lock_guard <std::mutex> lock(mutex);
    return malloc(size);
}

void allocator_global_heap::do_deallocate_sm(void *at)
{
    std::lock_guard <std::mutex> lock(mutex);
    free(at);
}

allocator_global_heap::~allocator_global_heap()
{
    // мьютекс сам уничтожается, деструктор ничего не делает
}

allocator_global_heap::allocator_global_heap(const allocator_global_heap &other)
{

}

allocator_global_heap &allocator_global_heap::operator=(const allocator_global_heap &other)
{
    return *this;
}

bool allocator_global_heap::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == &other;
}

allocator_global_heap::allocator_global_heap(allocator_global_heap &&other) noexcept
{
 
}

allocator_global_heap &allocator_global_heap::operator=(allocator_global_heap &&other) noexcept 
{
    return *this;
}
