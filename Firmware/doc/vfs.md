# gkos vfs #

The virtual file system in gkos is mostly built around the excellent Lwext (https://github.com/gkostka/lwext4).

The 'open' syscall (src/syscalls_filesys.cpp) interprets the path, checks for a valid free file number and then creates a object of a class which implements 'File' (inc/osfile.h).  This is either a usb serial device (/dev/ttyUSB0), Segger RTT debug dump (/dev/stdout/err/in), a named pipe or a Lwext file.  All these classes are defined in inc/osfile.h.  socket() also uses a similar mechanism to open a SocketFile object and bind it to an fd.  The relevant File object is then stored in the process (inc/process.h) open file table.

Any subsequent file operation syscall uses the inherited members of the File object previously created.  In the case of LwextFiles, these generate a request pushed to a queue which are implemented by the ext thread.  The ext thread simply calls the relevant lwext4 function and returns its result.  Various compilation-time checks are performes to ensure that the types/defines used by lwext4 are the same as those used by newlib in the gk userland.

As a final optimisation a LRU cache of fstat values is maintained reducing the need to load these from disk.  These are currently wiped on _any_ write access to the SD card.
