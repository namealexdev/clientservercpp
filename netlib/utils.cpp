#include "utils.h"
#include "const.h"

int getRandomNumber(int from, int to) {
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<int> dis(from, to);
    return dis(gen);
}
