#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lob {

// Fixed-size pool allocator (arena-style)
// Pre-allocates blocks of N objects to avoid malloc/free per order
// Provides O(1) alloc and O(1) dealloc via free-list
template <typename T, size_t BlockSize = 65536>
class PoolAllocator {
public:
    PoolAllocator() {
        allocate_block();
    }

    ~PoolAllocator() {
        for (auto* block : blocks_) {
            ::operator delete(block);
        }
    }

    // Non-copyable, non-movable
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    // Allocate one T — O(1)
    T* allocate() {
        if (!free_head_) {
            allocate_block();
        }
        Node* node = free_head_;
        free_head_ = node->next;
        ++allocated_;
        return reinterpret_cast<T*>(node);
    }

    // Deallocate one T — O(1), returns to free list
    void deallocate(T* ptr) {
        Node* node = reinterpret_cast<Node*>(ptr);
        node->next = free_head_;
        free_head_ = node;
        --allocated_;
    }

    size_t allocated_count() const { return allocated_; }
    size_t capacity() const { return blocks_.size() * BlockSize; }

private:
    union Node {
        T value;
        Node* next;
        Node() {}
        ~Node() {}
    };

    void allocate_block() {
        // Allocate raw memory for BlockSize nodes
        Node* block = static_cast<Node*>(::operator new(sizeof(Node) * BlockSize));
        blocks_.push_back(block);

        // Chain all nodes into the free list
        for (size_t i = 0; i < BlockSize - 1; ++i) {
            block[i].next = &block[i + 1];
        }
        block[BlockSize - 1].next = free_head_;
        free_head_ = block;
    }

    Node* free_head_ = nullptr;
    size_t allocated_ = 0;
    std::vector<Node*> blocks_;
};

} // namespace lob
