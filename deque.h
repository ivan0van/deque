#include <vector>

template <typename T>
class Deque {
  private:
    template <bool IsConst>
    class CommonIterator {
      private:
        const static int kBlockSize = 32;
        T** block;
        int index;
        T* element;

        void swap(CommonIterator& other) {
            std::swap(block, other.block);
            std::swap(index, other.index);
        }

      public:
        using value_type = std::conditional_t<IsConst, const T, T>;
        using pointer = std::conditional_t<IsConst, const T*, T*>;
        using reference = std::conditional_t<IsConst, const T&, T&>;
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = ptrdiff_t;
        CommonIterator() : block(nullptr), index(0) {}

        CommonIterator(T** init_block, int init_index)
            : block(init_block),
              index(init_index),
              element(*init_block + init_index) {}

        operator CommonIterator<true>() const {
            return CommonIterator<true>(block, index);
        }
        CommonIterator& operator++() {
            if (++index == kBlockSize) {
                index = 0;
                ++block;
            }
            element = *block + index;
            return *this;
        }

        CommonIterator operator++(int) {
            CommonIterator copy(*this);
            ++(*this);
            return copy;
        }

        CommonIterator& operator--() {
            if (--index == -1) {
                index = kBlockSize - 1;
                --block;
            }
            element = *block + index;
            return *this;
        }

        CommonIterator operator--(int) {
            CommonIterator copy = *this;
            --(*this);
            return copy;
        }

        CommonIterator& operator+=(ptrdiff_t shift) {
            ptrdiff_t res_index = index + shift;
            ptrdiff_t block_shift;
            if (res_index < 0) {
                block_shift = (res_index - kBlockSize + 1) / kBlockSize;
            } else {
                block_shift = res_index / kBlockSize;
            }
            index = (res_index % kBlockSize + kBlockSize) % kBlockSize;
            block += block_shift;
            element = *block + index;
            return *this;
        }

        CommonIterator& operator-=(ptrdiff_t shift) {
            return *this += -shift;
        }

        bool operator<(const CommonIterator<IsConst>& b) const {
            if (block != b.block) {
                return block < b.block;
            }
            return index < b.index;
        }

        bool operator>(const CommonIterator<IsConst>& b) const {
            return b < *this;
        }

        bool operator<=(const CommonIterator<IsConst>& b) const {
            return !(b < *this);
        }

        bool operator>=(const CommonIterator<IsConst>& b) const {
            return !(b > *this);
        }

        bool operator==(const CommonIterator<IsConst>& b) const {
            return block == b.block && index == b.index;
        }

        bool operator!=(const CommonIterator<IsConst>& b) const {
            return !(*this == b);
        }

        CommonIterator operator+(ptrdiff_t b) const {
            auto copy = *this;
            return copy += b;
        }

        CommonIterator operator-(ptrdiff_t b) const {
            auto copy = *this;
            return copy -= b;
        }

        ptrdiff_t operator-(const CommonIterator<IsConst>& b) const {
            return (block - b.block) * kBlockSize + index - b.index;
        }

        reference operator*() const {
            return *element;
        }

        pointer operator->() const {
            return element;
        }
    };
    std::vector<T*> blocks_;
    static const size_t kBlockSize = 32;
    static const size_t kSizeCoefficient = 3;
    size_t size_;

    void allocate_from_to(std::vector<T*>& blocks, size_t from, size_t to) {
        for (size_t i = from; i < to; ++i) {
            blocks[i] = reinterpret_cast<T*>(new char[kBlockSize * sizeof(T)]);
        }
    }

    void allocate(std::vector<T*>& blocks) {
        allocate_from_to(blocks, 0, blocks.size());
    }

    void allocate() {
        allocate(blocks_);
    }

    void clear(std::vector<T*>& blocks) {
        for (size_t i = 0; i < blocks.size(); ++i) {
            delete[] reinterpret_cast<char*>(blocks[i]);
        }
    }

    void clear() {
        clear(blocks_);
    }

  public:
    using iterator = CommonIterator<false>;
    using const_iterator = CommonIterator<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  private:
    iterator true_begin_, begin_;
    iterator true_end_, end_;

    ptrdiff_t end_diff() const {
        return true_end_ - end_;
    }

    ptrdiff_t begin_diff() const {
        return begin_ - true_begin_;
    }

    size_t need_blocks(int a) {
        return (a + kBlockSize - 1) / kBlockSize;
    }

    void delete_from_till(const iterator& begin, const iterator& end) {
        for (iterator it = begin; it < end; ++it) {
            it->~T();
        }
    }

    void delete_till(const iterator& end) {
        delete_from_till(begin_, end);
    }

    void initiate_iterators(std::vector<T*>& blocks, size_t begin_diff,
                            size_t end_diff) {
        true_begin_ = iterator(blocks.data(), 0);
        begin_ = static_cast<iterator>(true_begin_ + begin_diff);
        true_end_ = iterator(&blocks[blocks.size() - 1], kBlockSize - 1);
        end_ = static_cast<iterator>(true_end_ - end_diff);
    }

    void reallocate_from_till(size_t sz, const_iterator begin,
                              const_iterator end, ptrdiff_t begin_diff) {
        std::vector<T*> new_blocks(sz);
        allocate(new_blocks);
        iterator new_begin(iterator(new_blocks.data(), 0) + begin_diff);
        iterator cur(new_begin);
        try {
            for (auto it = begin; it < end; ++it) {
                new (&(*cur)) T(*it);
                ++cur;
            }
        } catch (...) {
            delete_from_till(new_begin, cur);
            clear(new_blocks);
            throw;
        }
        delete_till(end_);
        clear();
        blocks_ = new_blocks;
    }

    void reallocate_to_increase(size_t sz) {
        std::vector<T*> new_blocks(kSizeCoefficient * sz);
        size_t shift = (kSizeCoefficient - 1) / 2;
        allocate_from_to(new_blocks, 0, shift * sz);
        for (size_t i = shift * sz; i < (shift + 1) * sz; ++i) {
            new_blocks[i] = blocks_[i - sz];
        }
        allocate_from_to(new_blocks, (shift + 1) * sz, kSizeCoefficient * sz);
        blocks_ = new_blocks;
    }

    void reallocate() {
        size_t sz = blocks_.size() * kBlockSize;
        reallocate_to_increase(blocks_.size());
        initiate_iterators(blocks_, begin_diff() + sz, end_diff() + sz);
    }

  public:
    iterator begin() {
        return begin_;
    }

    const_iterator begin() const {
        return begin_;
    }

    const_iterator cbegin() const {
        return begin_;
    }

    iterator end() {
        return end_;
    }

    const_iterator end() const {
        return end_;
    }

    const_iterator cend() const {
        return end_;
    }

    Deque(const Deque<T>& other)
        : blocks_(other.blocks_.size()), size_(other.size_) {
        allocate();
        initiate_iterators(blocks_, other.begin_diff(), other.end_diff());
        iterator cur = begin_;
        try {
            for (auto that = other.begin(); that != other.end();
                 ++that, ++cur) {
                new (&(*cur)) T(*that);
            }
        } catch (...) {
            delete_till(cur);
            clear();
            throw;
        }
    }

    Deque() : blocks_(3), size_(0) {
        allocate();
        true_begin_ = iterator(blocks_.data(), 0);
        begin_ = iterator(&blocks_[1], 0);
        true_end_ = iterator(&blocks_[2], kBlockSize - 1);
        end_ = iterator(&blocks_[1], 0);
    }

    Deque(int sz) : blocks_(need_blocks(sz)), size_(sz) {
        allocate();
        true_begin_ = iterator(blocks_.data(), 0);
        true_end_ = iterator(&blocks_[need_blocks(sz) - 1], kBlockSize - 1);
        end_ = iterator(&blocks_[need_blocks(sz) - 1], sz % kBlockSize);
        begin_ = true_begin_;
        iterator cur = begin_;
        try {
            for (; cur < end_; ++cur) {
                new (&(*cur)) T();
            }
        } catch (...) {
            delete_till(cur);
            clear();
            throw;
        }
    }

    Deque(int sz, const T& value) : blocks_(need_blocks(sz)), size_(sz) {
        allocate();
        true_begin_ = iterator(blocks_.data(), 0);
        true_end_ = iterator(&blocks_[need_blocks(sz) - 1], kBlockSize - 1);
        end_ = iterator(&blocks_[need_blocks(sz) - 1], sz % kBlockSize);
        begin_ = true_begin_;
        iterator cur = begin_;
        try {
            for (; cur < end_; ++cur) {
                new (&(*cur)) T(value);
            }
        } catch (...) {
            delete_till(cur);
            clear();
            throw;
        }
    }

    Deque& operator=(const Deque& other) {
        reallocate_from_till(other.blocks_.size(), other.begin(), other.end(),
                             other.begin_diff());
        initiate_iterators(blocks_, other.begin_diff(), other.end_diff());
        size_ = other.size_;
        return *this;
    }

    ~Deque() {
        delete_till(end_);
        clear();
    }

    size_t size() const {
        return size_;
    }

    T& operator[](size_t index) {
        return *(begin() + index);
    }

    const T& operator[](size_t index) const {
        return *(begin() + index);
    }

    T& at(size_t index) {
        if (static_cast<size_t>(end() - begin()) <= index) {
            throw std::out_of_range("\"-_-\"");
        }
        return *(begin() + index);
    }

    const T& at(size_t index) const {
        if (static_cast<size_t>(end() - begin()) <= index) {
            throw std::out_of_range("\"-_-\"");
        }
        return *(begin() + index);
    }

    void push_back(const T& value) {
        if (end_ == true_end_) {
            reallocate();
        }
        new (&(*end_)) T(value);
        ++end_;
        ++size_;
    }

    void push_front(const T& value) {
        if (begin_ == true_begin_) {
            reallocate();
        }
        --begin_;
        ++size_;
        try {
            new (&(*begin_)) T(value);
        } catch (...) {
            ++begin_;
            --size_;
            throw;
        }
    }

    void pop_back() {
        end_->~T();
        --end_;
        --size_;
    }

    void pop_front() {
        begin_->~T();
        ++begin_;
        --size_;
    }

    reverse_iterator rbegin() {
        return reverse_iterator(end());
    }

    const_reverse_iterator rbegin() const {
        return reverse_iterator(end());
    }

    const_reverse_iterator crbegin() const {
        return reverse_iterator(end());
    }

    reverse_iterator rend() {
        return reverse_iterator(begin());
    }

    const_reverse_iterator rend() const {
        return reverse_iterator(begin());
    }

    const_reverse_iterator crend() const {
        return reverse_iterator(begin());
    }

    void insert(iterator it, const T& value) {
        ptrdiff_t shift = it - begin_;
        if (end_ == true_end_) {
            reallocate();
        }
        it = begin_ + shift;
        for (auto cur = end_; cur > it; --cur) {
            if (static_cast<size_t>(cur - begin_) < size_) {
                cur->~T();
            }
            new (&(*cur)) T(*(cur - 1));
        }
        if (static_cast<size_t>(it - begin_) < size_) {
            it->~T();
        }
        new (&(*it)) T(value);
        ++end_;
        ++size_;
    }

    void erase(const iterator& it) {
        it->~T();
        for (auto cur = it; cur < end_; ++cur) {
            *cur = *(cur + 1);
        }
        --end_;
        --size_;
    }
};
