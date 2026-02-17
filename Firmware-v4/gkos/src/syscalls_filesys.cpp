#include "syscalls_int.h"
#include "osmutex.h"
#include "thread.h"
#include "process.h"
#include "ramdisk.h"
#include "fatfs_file.h"
#include "pipe.h"

#include <cstring>
#include <fcntl.h>
#include <ext4.h>
#include <string>
#include <sstream>

#include "ext4_thread.h"
#include "vmem.h"

#define DEBUG_SYSCALL_FILESYS       0

int syscall_fstat(int file, struct stat *st, int *_errno)
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard(p->open_files.sl);
    if(file < 0 || (size_t)file >= p->open_files.f.size() || !p->open_files.f[file])
    {
        *_errno = EBADF;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(st);

    struct stat kernel_st;

    auto ret = p->open_files.f[file]->Fstat(&kernel_st, _errno);
    if(ret == -2)
    {
        ret = deferred_return(_errno);
    }

    if(ret == 0)
    {
        *st = kernel_st;
    }
    return ret;
}

int syscall_write(int file, char *buf, int nbytes, int *_errno)
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard(p->open_files.sl);
    if(file < 0 || (size_t)file >= p->open_files.f.size() || !p->open_files.f[file])
    {
        *_errno = EBADF;
        return -1;
    }
    ADDR_CHECK_BUFFER_W(buf, nbytes);

    return p->open_files.f[file]->Write(buf, nbytes, _errno);
}

int syscall_read(int file, char *buf, int nbytes, int *_errno)
{
    auto p = GetCurrentProcessForCore();
#if DEBUG_SYSCALL_FILESYS
    klog("syscall_read: %s.%u\n", p->name.c_str(), file);
#endif
    CriticalGuard(p->open_files.sl);
    if(file < 0 || (size_t)file >= p->open_files.f.size() || !p->open_files.f[file])
    {
        *_errno = EBADF;
        return -1;
    }
    ADDR_CHECK_BUFFER_R(buf, nbytes);

    auto ret = p->open_files.f[file]->Read(buf, nbytes, _errno);
    if(ret == -3)
    {
        if(p->open_files.f[file]->opts & O_NONBLOCK)
        {
            *_errno = EWOULDBLOCK;
            return -1;
        }
        else
        {
            // TODO: block on some signal
            Yield();
            return -3;
        }
    }
    return ret;
}

int syscall_isatty(int file, int *_errno)
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard(p->open_files.sl);
    if(file < 0 || (size_t)file >= p->open_files.f.size() || !p->open_files.f[file])
    {
        *_errno = EBADF;
        return -1;
    }

    return p->open_files.f[file]->Isatty(_errno);
}

off_t syscall_lseek(int file, off_t offset, int whence, int *_errno)
{
    auto p = GetCurrentProcessForCore();
#if DEBUG_SYSCALL_FILESYS
    klog("syscall_lseek: %s.%u\n", p->name.c_str(), file);
#endif
    CriticalGuard(p->open_files.sl);
    if(file < 0 || (size_t)file >= p->open_files.f.size() || !p->open_files.f[file])
    {
        *_errno = EBADF;
        return (off_t)-1;
    }

    return p->open_files.f[file]->Lseek(offset, whence, _errno);
}

int syscall_ftruncate(int file, off_t length, int *_errno)
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard(p->open_files.sl);
    if(file < 0 || (size_t)file >= p->open_files.f.size() || !p->open_files.f[file])
    {
        *_errno = EBADF;
        return -1;
    }

    return p->open_files.f[file]->Ftruncate(length, _errno);
}

int Process::open_files_t::get_free_fildes(int start_fd)
{
    if(start_fd < 0) start_fd = 0;
    for(int i = start_fd; (size_t)i < f.size(); i++)
    {
        if(f[i] == nullptr)
        {
            return i;
        }
    }
    
    // add to end
    if(f.size() < GK_MAX_OPEN_FILES)
    {
        f.push_back(PFile {});
        return (int)(f.size() - 1);
    }
    return -1;
}

int Process::open_files_t::get_fixed_fildes(int fd)
{
    if(fd < 0)
        return -1;
    if(fd >= GK_MAX_OPEN_FILES)
        return -1;
    while((size_t)fd >= f.size())
        f.push_back(PFile {});
    return f[fd] == nullptr ? fd : -1;
}

static inline bool starts_with(const std::string &s, char c)
{
    return s.length() > 0 && s[0] == c;
}

static void add_part(std::vector<std::string> &output, const std::string &input)
{
    if(input == "." || input == "")
    {
        return;
    }
    else if(input == "..")
    {
        if(output.size() > 0)
        {
            output.pop_back();
        }
    }
    else if(input == "~")
    {
        output.push_back("home");
        output.push_back("user");
    }
    else
    {
        output.push_back(input);
    }
}

static void add_parts(std::vector<std::string> &output, const std::string &input)
{
    size_t last = 0;
    size_t next = 0;
    const std::string delimiter = "/";
    while ((next = input.find(delimiter, last)) != std::string::npos)
    {
        add_part(output, input.substr(last, next - last));
        last = next + 1;
    }
    add_part(output, input.substr(last));
}

static std::string parse_fname(const std::string &pname)
{
    std::vector<std::string> pnames;

    if(!starts_with(pname, '/') && !starts_with(pname, '~'))
    {
        // add cwd
        std::string cwd;
        {
            auto p = GetCurrentProcessForCore();
            CriticalGuard cg(p->env.sl);
            cwd = p->env.cwd;
        }
        add_parts(pnames, cwd);
    }
    add_parts(pnames, pname);

    std::ostringstream ss;
    for(auto iter = pnames.begin(); iter != pnames.end(); iter++)
    {
        ss << "/";
        ss << *iter;
    }

#if 0
    klog("parse_fname: %s, cwd=%s -> %s\n", pname.c_str(), GetCurrentThreadForCore()->p->cwd.c_str(),
        ss.str().c_str());
#endif

    if(ss.str().size() == 0)
        return "/";         // opendir("/")

    return ss.str();
}

int syscall_open(const char *pathname, int flags, int mode, int *_errno)
{
    // try and get free process file handle
    auto t = GetCurrentThreadForCore();
    auto p = GetCurrentProcessForCore();
    bool is_opendir = (mode == S_IFDIR) && (flags == O_RDONLY);

    CriticalGuard cg(p->open_files.sl);
    ADDR_CHECK_BUFFER_R(pathname, 1);
    auto act_name = parse_fname(pathname);

    auto fd = p->open_files.get_free_fildes();
    if(fd == -1)
    {
        *_errno = EMFILE;
        return -1;
    }

    // special case /dev files
    if(act_name == "/dev/stdin")
    {
        if(is_opendir)
        {
            *_errno = ENOTDIR;
            return -1;
        }
        p->open_files.f[fd] = std::make_shared<UARTFile>(true, false);
        return fd;
    }
    if(act_name == "/dev/stdout")
    {
        if(is_opendir)
        {
            *_errno = ENOTDIR;
            return -1;
        }
        p->open_files.f[fd] = std::make_shared<UARTFile>(false, true);
        return fd;
    }
    if(act_name == "/dev/stderr")
    {
        if(is_opendir)
        {
            *_errno = ENOTDIR;
            return -1;
        }
        p->open_files.f[fd] = std::make_shared<UARTFile>(false, true);
        return fd;
    }
    if(act_name.starts_with("/dev/ramdisk/"))
    {
        if(is_opendir)
        {
            *_errno = ENOTDIR;
            return -1;
        }
        auto rdret = ramdisk_open(act_name.substr(13), &p->open_files.f[fd],
            (flags & 3) != O_WRONLY, (flags & 3) != O_RDONLY);
        if(rdret == 0)
            return fd;
        else
        {
            *_errno = ENOENT;
            return rdret;
        }
    }
    if(act_name.starts_with("/dev/fat/"))
    {
        if(is_opendir)
        {
            *_errno = ENOTDIR;
            return -1;
        }
        auto ffret = fatfs_open(act_name.substr(9), &p->open_files.f[fd],
            (flags & 3) != O_WRONLY, (flags & 3) != O_RDONLY);
        if(ffret == 0)
            return fd;
        else
        {
            *_errno = ENOENT;
            return ffret;
        }
    }
    if(act_name == "/dev/ttyUSB0")
    {
        //BKPT_IF_DEBUGGER();
#if 0
        if(is_opendir)
        {
            *_errno = ENOTDIR;
            return -1;
        }
        p->open_files.f[fd] = new USBTTYFile();
        return fd;
#endif
        return -1;
    }

    // use lwext4
    ext4_file _f = { 0 };
    auto lwf = std::make_shared<LwextFile>(_f, act_name);
    p->open_files.f[fd] = lwf;
#if DEBUG_SYSCALL_FILESYS
    klog("syscall_open: %s.%u is LwextFile\n", p->name.c_str(), fd);
#endif
    cg.unlock();
    auto msg = ext4_open_message(lwf->fname.c_str(), flags, mode,
        fd, t->ss, t->ss_p);
    if(ext4_send_message(msg))
    {
        return deferred_return(_errno);
    }
    else
    {
        cg.relock();
        p->open_files.f[fd] = nullptr;
        *_errno = ENOMEM;
        return -1;
    }

    return -1;
}

int syscall_opendir(const char *pathname, int *_errno)
{
    return syscall_open(pathname, O_RDONLY, S_IFDIR, _errno);
}

int syscall_close1(int file, int *_errno)
{
    auto p = GetCurrentProcessForCore();
#if DEBUG_SYSCALL_FILESYS
    klog("syscall_close1: %s.%u\n", p->name.c_str(), file);
#endif
    CriticalGuard(p->open_files.sl);
    if(file < 0 || (size_t)file >= p->open_files.f.size() || !p->open_files.f[file])
    {
        *_errno = EBADF;
        return -1;
    }

    if(p->open_files.f[file].use_count() == 1)
        return p->open_files.f[file]->Close(_errno);
    else
        return 0;
}

int syscall_close2(int file, int *_errno)
{
    auto p = GetCurrentProcessForCore();
#if DEBUG_SYSCALL_FILESYS
    klog("syscall_close2: %s.%u\n", p->name.c_str(), file);
#endif
    CriticalGuard(p->open_files.sl);
    if(file < 0 || (size_t)file >= p->open_files.f.size() || !p->open_files.f[file])
    {
        *_errno = EBADF;
        return -1;
    }

    int ret = 0;
    if(p->open_files.f[file].use_count() == 1)
        ret = p->open_files.f[file]->Close2(_errno);
    p->open_files.f[file] = nullptr;

    return ret;
}

int syscall_readdir(int file, dirent *de, int *_errno)
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard(p->open_files.sl);
    if(file < 0 || (size_t)file >= p->open_files.f.size() || !p->open_files.f[file])
    {
        *_errno = EBADF;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(de);

    return p->open_files.f[file]->ReadDir(de, _errno);
}

int syscall_mkdir(const char *pathname, mode_t mode, int *_errno)
{
    if(!pathname)
    {
        *_errno = EFAULT;
        return -1;
    }
    ADDR_CHECK_BUFFER_R(pathname, 1);

    auto act_name = parse_fname(pathname);

    auto t = GetCurrentThreadForCore();
    auto msg = ext4_mkdir_message(act_name.c_str(), mode, t->ss, t->ss_p);
    if(ext4_send_message(msg))
    {
        while(!t->ss.Wait(SimpleSignal::Set, 0));
        return t->ss_p.ival1;
    }
    else
    {
        *_errno = ENOMEM;
        return -1;
    }
}

int syscall_unlink(const char *pathname, int *_errno)
{
    if(!pathname)
    {
        *_errno = EFAULT;
        return -1;
    }
    ADDR_CHECK_BUFFER_R(pathname, 1);

    auto act_name = parse_fname(pathname);

    auto t = GetCurrentThreadForCore();
    auto msg = ext4_unlink_message(act_name.c_str(), t->ss, t->ss_p);
    if(ext4_send_message(msg))
    {
        while(!t->ss.Wait(SimpleSignal::Set, 0));
        return t->ss_p.ival1;
    }
    else
    {
        *_errno = ENOMEM;
        return -1;
    }
}

int syscall_chdir(const char *pathname, int *_errno)
{
    if(!pathname)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_BUFFER_R(pathname, 1);

    auto act_name = parse_fname(pathname);

    auto p = GetCurrentProcessForCore();
    CriticalGuard cg(p->env.sl);
    p->env.cwd = act_name;
    return 0;
}

int syscall_realpath(const char *path, char *resolved_path, size_t len, int *_errno)
{
    // we should really do this in userland, looping on each element to see if it is a symlink
    //  instead, just resolve .. etc

    ADDR_CHECK_BUFFER_R(path, 1);
    ADDR_CHECK_BUFFER_W(resolved_path, len);

    auto act_name = parse_fname(path);

    strncpy(resolved_path, act_name.c_str(), len-1);
    resolved_path[len-1] = 0;

    return 0;
}

int syscall_getcwd(char *path, size_t bufsize, int *_errno)
{
    ADDR_CHECK_BUFFER_W(path, bufsize);

    auto p = GetCurrentProcessForCore();
    CriticalGuard cg(p->env.sl);
    if(p->env.cwd.length() > (bufsize - 1U))
    {
        *_errno = ERANGE;
        return -1;
    }
    else
    {
        strcpy(path, p->env.cwd.c_str());
        *_errno = 0;
        return 0;
    }
}

int syscall_pipe(int pipefd[2], int *_errno)
{
    if(!pipefd)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_BUFFER_W(pipefd, sizeof(int) * 2);

    // try and get free process file handles
    auto p = GetCurrentProcessForCore();
    CriticalGuard cg(p->open_files.sl);

    auto newpipe = make_pipe();

    if(!newpipe.first || !newpipe.second)
    {
        *_errno = ENOMEM;
        return -1;
    }

    int fd1 = p->open_files.get_free_fildes();
    if(fd1 == -1)
    {
        *_errno = EMFILE;
        return -1;
    }
    p->open_files.f[fd1] = newpipe.first;
    int fd2 = p->open_files.get_free_fildes();
    if(fd2 == -1)
    {
        *_errno = EMFILE;
        p->open_files.f[fd1] = nullptr;
        return -1;
    }
    p->open_files.f[fd2] = newpipe.second;

    pipefd[0] = fd1;
    pipefd[1] = fd2;

    return 0;
}

int syscall_dup2(int oldfd, int newfd, int *_errno)
{
    auto p = GetCurrentProcessForCore();
    CriticalGuard cg(p->open_files.sl);

    if(oldfd < 0 || oldfd >= GK_MAX_OPEN_FILES)
    {
        *_errno = EBADF;
        return -1;
    }

    if(p->open_files.f[oldfd] == nullptr)
    {
        *_errno = EBADF;
        return -1;
    }
    if(newfd != -1)
    {
        if(newfd < 0 || newfd >= GK_MAX_OPEN_FILES)
        {
            *_errno = EBADF;
            return -1;
        }
        if(p->open_files.f[newfd] == nullptr)
        {
            *_errno = EBADF;
            return -1;
        }
        else
        {
            p->open_files.f[newfd]->Close(_errno);
        }
    }
    else
    {
        newfd = p->open_files.get_free_fildes();
        if(newfd < 0)
        {
            *_errno = EBADF;
            return -1;
        }
    }

    p->open_files.f[newfd] = p->open_files.f[oldfd];

    if(p->open_files.f[newfd] == nullptr)
    {
        *_errno = ENOSYS;
        return -1;
    }

    return newfd;
}
