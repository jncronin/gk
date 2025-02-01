#ifndef BTNLED_H
#define BTNLED_H

#include <cstdint>

void init_btnled();
void btnled_setcolor(uint32_t rgb);
void btnled_setcolor_init(uint32_t rgb);

#endif
