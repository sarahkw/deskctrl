#include <iostream>
#include <vector>

template <typename T, size_t N>
struct CircularBuffer {

    // Keep one more item for code simplicity. Otherwise, begin() and
    // end() will return the same pointer, and iteration will be more
    // difficult.
    enum { ACTUAL_SIZE = N + 1 };

    T d_buffer[ACTUAL_SIZE];
    size_t d_nextPosition; // Position of next item.
    size_t d_size;

    CircularBuffer() : d_nextPosition(), d_size() {}

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

    void insert(T value) {
        d_buffer[d_nextPosition] = value;
        if (d_size < ACTUAL_SIZE) {
            ++d_size;
        }
        d_nextPosition = (d_nextPosition + 1) % ACTUAL_SIZE;
    }

    size_t size() const {
        return std::min(d_size, N);
    }

    std::ostream& debug(std::ostream& os) const {
        os << "[";
        for (size_t i = 0; i < ACTUAL_SIZE; ++i) {
            os << " " << d_buffer[i] << " ";
        }
        os << "; d_nextPosition = " << d_nextPosition << ", d_size = " << d_size
           << ", begin()index = " << (begin() - d_buffer) << "]";
        return os;
    }

    std::ostream& print(std::ostream& os) const {
        os << "[";
        for (const T* iter = begin(); iter != end(); iter = next(iter)) {
            os << " " << *iter << " ";
        }
        os << "; d_size = " << d_size
           << "]";
        return os;
    }
};

struct Parse {
    int input(int ch) {
        d_buffer.insert(ch);

//        d_buffer.debug(std::cout);
//        d_buffer.print(std::cout) << "\n";

        if (d_buffer.size() != 8) {
            return -1;
        }

        const int *ptr1 = d_buffer.begin(),
                  *ptr2 = d_buffer.next(d_buffer.begin(), 4);
        if (*ptr1 != 1) {
          return -1;
        }

        for (size_t i = 0; i < 4; ++i) {
//            std::cout << "compare " << *ptr1 << " " << *ptr2 << "\n";
            
            if (*ptr1 != *ptr2) {
                return -1;
            }
            ptr1 = d_buffer.next(ptr1);
            ptr2 = d_buffer.next(ptr2);
        }

        return 999;
    }
    CircularBuffer<int, 8> d_buffer;
};

int main(int argc, char* argv[])
{
    Parse p;

    int ch;
    while ((ch = std::cin.get()) != EOF) {
        std::cout << ch << " ";
        int result = p.input(ch);
        if (result != -1) {
            std::cout << "\nResult: " << result << "\n";
        }
    }
    
    return 0;
}
