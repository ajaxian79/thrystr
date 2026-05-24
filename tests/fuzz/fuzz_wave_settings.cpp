// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    volatile std::uint32_t checksum = 0;
    for (std::size_t i = 0; i < size; ++i) {
        checksum = (checksum * 16777619u) ^ data[i];
    }
    (void)checksum;
    return 0;
}
