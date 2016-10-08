#include <iostream>

struct Parse {
    int input(int ch) {
        std::cout << ch << "\n";
        return -1;
    }
};

int main(int argc, char* argv[])
{
    Parse p;

    int ch;
    while ((ch = std::cin.get()) != EOF) {
        int result = p.input(ch);
        if (result != -1) {
            std::cout << "Result: " << result << "\n";
        }
    }
    
    return 0;
}
