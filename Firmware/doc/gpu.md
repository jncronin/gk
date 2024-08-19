# gkos GPU driver #

The STM32H747 chip has a built in DMA2D peripheral that supports hardware accelerated image and solid colour blitting.  Additionally it has a RGB LED controller (LTDC) that supports two layers.  Finally there is a master DMA controller (MDMA) which is also used by the GPU subsystem.

The gpu code runs on a separate thread and processes messages received in an queue from other threads.  Thus, it is able to serialize and control thread's accesses to these three hardware components.

The default resolution of the gk is 640x480 with support for indexed, 16-bit and 24-bit colour.  The gpu driver has built-in double buffering so the next frame can be drawn whilst the current one is being displayed.  Only when the new frame is complete is the LTDC updated to show the new frame.  Drawing then occurs back on the old frame.  By default the framebuffer's MPU attributes are write-through.

Applications can use the framebuffer in many ways depending on how they are written.  Direct framebuffer access is exposed to applications and an API provides a buffer swap mechanism and returns the address of the new backbuffer.  This is used, for example, in SDL code using the "Surface" method of rendering.  In which case, the only messages which need to be passed to the gpu thread are FlipBuffers and SignalThread (the latter wakes up a waiting thread once gpu processing is complete and is useful, for example, to signal a waiting thread that the gpu is ready for the next frame to be drawn).

Alternatively they can use a rendering interface via BlitColor, BlitImage and BlitImageNoBlend calls.  These will use the hardware DMA engines to directly write either from memory to the framebuffer or from memory to memory (useful for offscreen rendering).  These routines also support scaling, and in the BlitColor/BlitImageNoBlend situation will use the MDMA to accelerate this else the CPU is used.  In the absence of scaling the DMA2D is used as it also supports blending.  These gpu calls are directly used by SDL2's rendering interface.

If the application targets a lower screen resolution that 640x480 (320x240 and 160x120 are both supported) then upscaling is performed transparently to the application by decomposing the relevant gpu commands into a series of others.
