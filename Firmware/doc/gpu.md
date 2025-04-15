# gkos GPU driver #

The STM32H7S7 chip has a built in DMA2D peripheral that supports hardware accelerated image and solid colour blitting.  Additionally it has a RGB LED controller (LTDC) that supports two layers.

The gpu code runs on a separate thread and processes messages received in an queue from other threads.  Thus, it is able to serialize and control thread's accesses to these three hardware components.

The default resolution of the gk is 640x480 with support for indexed, 16-bit and 24-bit colour.  The gpu driver has built-in double buffering so the next frame can be drawn whilst the current one is being displayed.  Only when the new frame is complete is the LTDC updated to show the new frame.  Drawing then occurs back on the old frame.  By default the framebuffer's MPU attributes are write-through.

Applications can use the framebuffer in many ways depending on how they are written.  Direct framebuffer access is exposed to applications and an API provides a buffer swap mechanism and returns the address of the new backbuffer.  This is used, for example, in SDL code using the "Surface" method of rendering.  In which case, the only messages which need to be passed to the gpu thread are FlipBuffers and SignalThread (the latter wakes up a waiting thread once gpu processing is complete and is useful, for example, to signal a waiting thread that the gpu is ready for the next frame to be drawn).

Alternatively they can use a rendering interface via BlitColor, BlitImage and BlitImageNoBlend calls.  These will use the hardware DMA engines to directly write either from memory to the framebuffer or from memory to memory (useful for offscreen rendering).  These routines also support scaling.  In the absence of scaling the DMA2D is used as it also supports blending.  These gpu calls are directly used by SDL2's rendering interface.

Hardware upscaling from 320x240 to 640x480 is supported through use of the GFXMMU peripheral in the STM32H7S7 for vertical scaling, and an external pixel clock doubler (570BLF) for horizontal scaling.

LTDC Layer 2 is used exclusively by the "supervisor" process, which provides an overlay for interacting with the system, e.g. closing current process, volume and brightness controls and custom options dependent on the process which typically include load/save game functionality.
