#ifndef MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_SORTED_LIST_H
#define MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_SORTED_LIST_H

#include <pp_allocator.h>
#include <allocator_test_utils.h>
#include <allocator_with_fit_mode.h>
#include <iterator>
#include <mutex>

class allocator_sorted_list final:
    public smart_mem_resource,
    public allocator_test_utils,
    public allocator_with_fit_mode
{

private:
    
    void *_trusted_memory; // едиснтвенное поле, остальные данные храняться внутри этой памяти (указатель на начало большого куска памяти)

    static constexpr const size_t allocator_metadata_size = sizeof(std::pmr::memory_resource *) + sizeof(fit_mode) + sizeof(size_t) + sizeof(std::mutex) + sizeof(void*);

    static constexpr const size_t block_metadata_size = sizeof(void*) + sizeof(size_t);
    
    mutable std::mutex _mtx;

public:

    explicit allocator_sorted_list(
            size_t space_size,
            std::pmr::memory_resource *parent_allocator = nullptr,
            allocator_with_fit_mode::fit_mode allocate_fit_mode = allocator_with_fit_mode::fit_mode::first_fit);
    
    allocator_sorted_list(
        allocator_sorted_list const &other);
    
    allocator_sorted_list &operator=(
        allocator_sorted_list const &other);

    allocator_sorted_list(
        allocator_sorted_list &&other) noexcept;
    
    allocator_sorted_list &operator=(
        allocator_sorted_list &&other) noexcept;

    ~allocator_sorted_list() override;

private:
    
    [[nodiscard]] void *do_allocate_sm(
        size_t size) override;
    
    void do_deallocate_sm(
        void *at) override;

    bool do_is_equal(const std::pmr::memory_resource&) const noexcept override;
    
    inline void set_fit_mode(
        allocator_with_fit_mode::fit_mode mode) override;

    std::vector<allocator_test_utils::block_info> get_blocks_info() const noexcept override;

private:

    std::vector<allocator_test_utils::block_info> get_blocks_info_inner() const override;

    class sorted_free_iterator // ходит по свободным блокам
    {
        void* _free_ptr; // укказатель на текущий свободный блок

    public:

        using iterator_category = std::forward_iterator_tag;
        using value_type = void*;
        using reference = void*&;
        using pointer = void**;
        using difference_type = ptrdiff_t;

        bool operator==(const sorted_free_iterator&) const noexcept; // указывают ли два итератора на один и тот же блок

        bool operator!=(const sorted_free_iterator&) const noexcept; // указывают ли итераторы на разные места

        sorted_free_iterator& operator++() & noexcept; // префиксный инкремент ++it; возвращает ссылку на то, на что передвигает (на следущий свободный блок)

        sorted_free_iterator operator++(int n); // постфикcный инкремент it++; возвращает исходную ссылку

        size_t size() const noexcept; // возвращает размер текущего свободного блока

        void* operator*() const noexcept; // разыменование, возвращает то, на что указывает итератор (текущий свободный блок)

        sorted_free_iterator(); // конструктор

        sorted_free_iterator(void* trusted); // создает итератор, который указывает на какой-то свободный блок
    };

    class sorted_iterator // ходит по всем блокам
    {
        void* _free_ptr;  // следующий свободный блок (может быть и текущий)
        void* _current_ptr; // указатель на текущий блок
        void* _trusted_memory; // границы области (указатель на начало выделенной области)

    public:

        using iterator_category = std::forward_iterator_tag;
        using value_type = void*;
        using reference = void*&;
        using pointer = void**;
        using difference_type = ptrdiff_t;

        bool operator==(const sorted_iterator&) const noexcept;

        bool operator!=(const sorted_iterator&) const noexcept;

        sorted_iterator& operator++() & noexcept;

        sorted_iterator operator++(int n);

        size_t size() const noexcept;

        void* operator*() const noexcept;

        bool occupied()const noexcept;

        sorted_iterator();

        sorted_iterator(void* trusted);
    };

    friend class sorted_iterator;
    friend class sorted_free_iterator;

    sorted_free_iterator free_begin() const noexcept;
    sorted_free_iterator free_end() const noexcept;

    sorted_iterator begin() const noexcept;
    sorted_iterator end() const noexcept;
};

#endif //MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_SORTED_LIST_H