#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <unistd.h>
#include <string>
#include <vector>

int getRandomNumber(int from, int to);
std::vector<uint8_t> generateRandomData(size_t size);
void write2file(std::string& sfilename, const char* data, ssize_t size);

std::array<uint8_t, 16> generateUuid();
bool write_session_uuid(const std::array<uint8_t, 16>& client_session_uuid, const std::string &filename);
bool read_session_uuid(const std::string& filename, std::array<uint8_t, 16>& result);


#endif // UTILS_H
