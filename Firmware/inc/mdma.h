#ifndef MDMA_H
#define MDMA_H

void init_mdma();
void mdma_register_handler(void (*handler)(), unsigned int h_idx);

#endif
