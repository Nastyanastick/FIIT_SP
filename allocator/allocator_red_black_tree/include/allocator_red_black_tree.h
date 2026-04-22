#ifndef MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_RED_BLACK_TREE_H
#define MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_RED_BLACK_TREE_H

#include <pp_allocator.h>
#include <allocator_test_utils.h>
#include <allocator_with_fit_mode.h>
#include <mutex>

class allocator_red_black_tree final:
    public smart_mem_resource,
    public allocator_test_utils,
    public allocator_with_fit_mode
{

private:

    enum class block_color : unsigned char
    { RED, BLACK };

    struct block_data
    {
        bool occupied : 4;
        block_color color : 4;
    };

    struct allocator_meta
    {
        std::pmr::memory_resource* parent_allocator; // от куда взяли память
        fit_mode mode;
        size_t space_size; // вся область
        mutable std::mutex mutex;
        void* free_root;
    };

    struct occupied_block_meta
    {
        block_data data;
        size_t size; // размер включая методанные
        void* prev; // предыдщий физ блок в памяти
        void* owner;
    };

    struct free_block_meta
    {
        block_data data;
        size_t size;
        void* prev;
        void* parent;
        void* left;
        void* right;
    };

    void *_trusted_memory;

    static constexpr const size_t allocator_metadata_size = sizeof(allocator_meta);
    static constexpr const size_t occupied_block_metadata_size = sizeof(occupied_block_meta);
    static constexpr const size_t free_block_metadata_size = sizeof(free_block_meta);

public:
    
    ~allocator_red_black_tree() override;
    
    allocator_red_black_tree(
        allocator_red_black_tree const &other);
    
    allocator_red_black_tree &operator=(
        allocator_red_black_tree const &other);
    
    allocator_red_black_tree(
        allocator_red_black_tree &&other) noexcept;
    
    allocator_red_black_tree &operator=(
        allocator_red_black_tree &&other) noexcept;

public:
    
    explicit allocator_red_black_tree(
            size_t space_size,
            std::pmr::memory_resource *parent_allocator = nullptr,
            allocator_with_fit_mode::fit_mode allocate_fit_mode = allocator_with_fit_mode::fit_mode::first_fit);

private:
    
    [[nodiscard]] void *do_allocate_sm(
        size_t size) override;
    
    void do_deallocate_sm(
        void *at) override;

    bool do_is_equal(const std::pmr::memory_resource&) const noexcept override;

    std::vector<allocator_test_utils::block_info> get_blocks_info() const override;
    
    inline void set_fit_mode(allocator_with_fit_mode::fit_mode mode) override;

    void* find_first_fit(size_t required_size) noexcept;
    void* find_best_fit(size_t required_size) noexcept;
    void* find_worst_fit(size_t required_size) noexcept;



    allocator_meta* meta() noexcept;
    const allocator_meta* meta() const noexcept;

    void* first_block() noexcept;
    const void* first_block() const noexcept;

    size_t& block_size(void* block) noexcept;
    const size_t& block_size(const void* block) const noexcept;

    block_data& block_header(void* block) noexcept;
    const block_data& block_header(const void* block) const noexcept;

    void*& block_prev(void* block) noexcept;
    void* const& block_prev(const void* block) const noexcept;

    free_block_meta* as_free(void* block) noexcept;
    const free_block_meta* as_free(const void* block) const noexcept;

    occupied_block_meta* as_occupied(void* block) noexcept;
    const occupied_block_meta* as_occupied(const void* block) const noexcept;



    void* next_block(void* block) noexcept;
    const void* next_block(const void* block) const noexcept;

    bool is_last_block(const void* block) const noexcept;

    void* user_memory(void* block) noexcept;
    const void* user_memory(const void* block) const noexcept;

    void* block_from_user_memory(void* user_ptr) noexcept;
    const void* block_from_user_memory(const void* user_ptr) const noexcept;

    void init_free_block(void* block, size_t size, void* prev) noexcept;
    void init_occupied_block(void* block, size_t size, void* prev) noexcept;



    void rb_insert(void* block) noexcept;
    void rb_remove(void* block) noexcept;
    void* find_suitable_block(size_t size) noexcept;

    block_color color_of(const void* block) const noexcept;
    void set_color(void* block, block_color color) noexcept;

    void* parent_of(void* block) const noexcept;
    const void* parent_of(const void* block) const noexcept;

    void* left_of(void* block) const noexcept;
    const void* left_of(const void* block) const noexcept;

    void* right_of(void* block) const noexcept;
    const void* right_of(const void* block) const noexcept;

    void set_parent(void* block, void* parent) noexcept;
    void set_left(void* block, void* left) noexcept;
    void set_right(void* block, void* right) noexcept;

    bool less_blocks(const void* lhs, const void* rhs) const noexcept;

    void left_rotate(void* x) noexcept;
    void right_rotate(void* x) noexcept;

    void rb_insert_fixup(void* z) noexcept;

    void transplant(void* u, void* v) noexcept;
    void* tree_minimum(void* node) const noexcept;
    void rb_remove_fixup(void* x, void* x_parent) noexcept;

private:

    std::vector<allocator_test_utils::block_info> get_blocks_info_inner() const override;

    class rb_iterator
    {
        void* _block_ptr;
        void* _trusted;

    public:

        using iterator_category = std::forward_iterator_tag;
        using value_type = void*;
        using reference = void*&;
        using pointer = void**;
        using difference_type = ptrdiff_t;

        bool operator==(const rb_iterator&) const noexcept;

        bool operator!=(const rb_iterator&) const noexcept;

        rb_iterator& operator++() & noexcept;

        rb_iterator operator++(int n);

        size_t size() const noexcept;

        void* operator*() const noexcept;

        bool occupied()const noexcept;

        rb_iterator();

        rb_iterator(void* trusted);
    };

    friend class rb_iterator;

    rb_iterator begin() const noexcept;
    rb_iterator end() const noexcept;

};

#endif //MATH_PRACTICE_AND_OPERATING_SYSTEMS_ALLOCATOR_ALLOCATOR_RED_BLACK_TREE_H