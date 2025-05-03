// File: esp_mbedtls_psram_alloc.cpp

#include "esp_heap_caps.h"
#include <stdlib.h>

extern "C" void *esp_mbedtls_psram_calloc(size_t n, size_t s)
{
    // allocate n*s bytes from SPIRAM (8-bit capable)
    return heap_caps_calloc(n, s, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}