#ifndef MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_BUDDIES_SYSTEM_H
#define MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_BUDDIES_SYSTEM_H

#include <pp_allocator.h>
#include <allocator_test_utils.h>
#include <allocator_with_fit_mode.h>
#include <mutex>
#include <cmath>

namespace __detail
{
    constexpr size_t nearest_greater_k_of_2(size_t size) noexcept
    {
        int ones_counter = 0, index = -1;

        constexpr const size_t o = 1;

        for (int i = sizeof(size_t) * 8 - 1; i >= 0; --i)
        {
            if (size & (o << i))
            {
                if (ones_counter == 0)
                    index = i;
                ++ones_counter;
            }
        }

        return ones_counter <= 1 ? index : index + 1;
    }
}

class allocator_buddies_system final:
    public smart_mem_resource,
    public allocator_test_utils,
    public allocator_with_fit_mode
{

private:


    struct block_metadata
    {
        bool occupied : 1; // занят или нет
        unsigned char size : 7; // степень двойки
    };

    struct allocator_header
    {
        std::pmr::memory_resource* parent_allocator;
        allocator_with_fit_mode::fit_mode mode;
        unsigned char space_power;
        std::mutex mtx;
    };

    void *_trusted_memory;

    /**
     * TODO: You must improve it for alignment support
     */

    static constexpr size_t allocator_metadata_size = sizeof(allocator_header);
    static constexpr const size_t occupied_block_metadata_size = sizeof(block_metadata) + sizeof(void*);

    static constexpr const size_t free_block_metadata_size = sizeof(block_metadata);

    static constexpr const size_t min_k = __detail::nearest_greater_k_of_2(occupied_block_metadata_size);

public:

    explicit allocator_buddies_system(
            size_t space_size,
            std::pmr::memory_resource *parent_allocator = nullptr,
            allocator_with_fit_mode::fit_mode allocate_fit_mode = allocator_with_fit_mode::fit_mode::first_fit);

    allocator_buddies_system(
        allocator_buddies_system const &other);
    
    allocator_buddies_system &operator=(
        allocator_buddies_system const &other);
    
    allocator_buddies_system(
        allocator_buddies_system &&other) noexcept;
    
    allocator_buddies_system &operator=(
        allocator_buddies_system &&other) noexcept;

    ~allocator_buddies_system() override;

private:
    
    [[nodiscard]] void *do_allocate_sm(
        size_t size) override;
    
    void do_deallocate_sm(
        void *at) override;

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override;

    inline void set_fit_mode(
        allocator_with_fit_mode::fit_mode mode) override;


    std::vector<allocator_test_utils::block_info> get_blocks_info() const noexcept override;

private:

    std::vector<allocator_test_utils::block_info> get_blocks_info_inner() const override;

    static std::byte* bytes(void* p) noexcept; // превращает указатель на байты, вместо reinterpret_cast
    static const std::byte* bytes(const void* p) noexcept;

    std::pmr::memory_resource*& parent_allocator_ref() noexcept; // родитель аллокатора
    const std::pmr::memory_resource* parent_allocator_ref() const noexcept;

    fit_mode& fit_mode_ref() noexcept; // получить режим поиска
    const fit_mode& fit_mode_ref() const noexcept;

    unsigned char& space_power_ref() noexcept; // степень двойки
    const unsigned char& space_power_ref() const noexcept;

    std::mutex& mutex_ref() noexcept; // получить mutex
    const std::mutex& mutex_ref() const noexcept;

    block_metadata* first_block() noexcept; // указатель на первый блок (тот, который в самом начале большой)
    const block_metadata* first_block() const noexcept;

    static size_t block_size(const block_metadata* block) noexcept; // из степени двойки в реальный размер
    static void* user_memory(block_metadata* block) noexcept; // указатель на память, которую нужно отдать пользователю
    static const void* user_memory(const block_metadata* block) noexcept;
    allocator_header* header() noexcept;
    const allocator_header* header() const noexcept;


    /** TODO: Highly recommended for helper functions to return references */

    class buddy_iterator
    {   
        void* _block;

    public:

        using iterator_category = std::forward_iterator_tag;
        using value_type = void*;
        using reference = void*&;
        using pointer = void**;
        using difference_type = ptrdiff_t;

        bool operator==(const buddy_iterator&) const noexcept;

        bool operator!=(const buddy_iterator&) const noexcept;

        buddy_iterator& operator++() & noexcept;

        buddy_iterator operator++(int n);

        size_t size() const noexcept;

        bool occupied() const noexcept;

        void* operator*() const noexcept;

        buddy_iterator();

        buddy_iterator(void* start);
    };

    friend class buddy_iterator;

    buddy_iterator begin() const noexcept;

    buddy_iterator end() const noexcept;
    
};

#endif //MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_BUDDIES_SYSTEM_H
