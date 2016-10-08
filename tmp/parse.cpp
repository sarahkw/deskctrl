#include <iostream>
#include <fstream>

struct Parse {
    int input(int ch) {
        std::cout << ch << "\n";
        return -1;
    }
};

int main(int argc, char* argv[])
{
    if (argc != 2) {
        return 1;
    }

    std::ifstream instream(argv[1]);

    if (!instream) {
        std::cerr << "fail\n";
        return 1;
    }

    Parse p;

    int ch;
    while ((ch = instream.get()) != EOF) {
        int result = p.input(ch);
        if (result != -1) {
            std::cout << "Result: " << result << "\n";
        }
    }
    
    return 0;
}
