template <typename T, size_t N>
struct CircularBuffer {

    // Keep one more item for code simplicity. Otherwise, begin() and
    // end() will return the same pointer, and iteration will be more
    // difficult.
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
        return min(d_size, N);
    }

    void clear() {
        d_nextPosition = 0;
        d_size = 0;
    }
};
