#pragma once

#include <stddef.h>
#include <stdint.h>

int saveImage(const uint8_t *image, size_t size, uint32_t crc);
int selectImage(size_t size, uint32_t crc);
int clearImageSelection();
