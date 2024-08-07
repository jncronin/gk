#ifndef MDMA_H
#define MDMA_H

void GKOS_FUNC(init_mdma)();
void GKOS_FUNC(mdma_register_handler)(void (*handler)(), unsigned int h_idx);

#endif
