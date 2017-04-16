#ifndef INCLUDED_CIRCULARBUFFER_H
#define INCLUDED_CIRCULARBUFFER_H

// TODO Add includes for types we use

// Usage:
//
//    CircularBuffer<int, 4> cb;
//    cb.push_back(1);
//    cb.push_back(2);
//    cb.push_back(3);
//    cb.push_back(4);
//    cb.push_back(5);
//    for (const int* p = cb.begin();
//         p != cb.end();
//         p = cb.next(p)) {
//        std::cout << *p << "\n";
//    }

template <typename T, size_t N>
struct CircularBuffer {

    // Keep one more item for code simplicity. Otherwise, begin() and
    // end() will return the same pointer, and iteration will be more
    // difficult to implement. When using an array as a circular
    // buffer, one past the last element would be the beginning
    // element.
    enum { ACTUAL_SIZE = N + 1 };

    T d_buffer[ACTUAL_SIZE];
    size_t d_nextPosition = 0; // Position of next item.
    size_t d_size = 0;

    const T* begin() const {
        if (d_size >= N) {
            return &d_buffer[(d_nextPosition + 1) % ACTUAL_SIZE];
        } else {
            return &d_buffer[0];
        }
    }

    const T* end() const {
        return &d_buffer[d_nextPosition];
    }

    const T* next(const T* current, size_t count = 1) const {
        auto nextIndex = current - d_buffer + count;
        return &d_buffer[nextIndex % ACTUAL_SIZE];
    }

    void push_back(T value) {
        d_buffer[d_nextPosition] = value;
        if (d_size < ACTUAL_SIZE) {
            ++d_size;
        }
        d_nextPosition = (d_nextPosition + 1) % ACTUAL_SIZE;
    }

    size_t size() const {
        // In arduino libs, min is defined as a macro. Import the
        // namespace so this code works with both arduino libs, and
        // without.
        using namespace std;

        return min(d_size, N);
    }

    void clear() {
        d_nextPosition = 0;
        d_size = 0;
    }
};

#endif
