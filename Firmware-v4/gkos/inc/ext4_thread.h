#ifndef EXT4_THREAD_H
#define EXT4_THREAD_H

#include "osmutex.h"
#include "osfile.h"
#include "syscalls.h"
#include "thread.h"
#include "ext4.h"
#include "_sys_dirent.h"
#include "process.h"
#include "ostypes.h"

void init_ext4();

struct ext4_message
{
    enum msg_type
    {
        Open,
        Close,
        Read,
        Write,
        Lseek,
        Fstat,
        Mkdir,
        ReadDir,
        Unlink,
        Ftruncate,
        Unmount,
    };

    msg_type type;
    id_t tid;

    union params_t
    {
        struct open_params_t
        {
            const char *pathname;
            int flags;
            int mode;
            int f;      // Pre-prepared file descriptor to place into
        } open_params;
        struct rw_params_t
        {
            ext4_file *e4f;
            char *buf;
            int nbytes;
        } rw_params;
        struct lseek_params_t
        {
            ext4_file *e4f;
            off_t offset;
            int whence;
        } lseek_params;
        struct fstat_params_t
        {
            ext4_file *e4f;
            ext4_dir *e4d;
            struct stat *st;
            const char *pathname;
        } fstat_params;
        struct close_params_t
        {
            ext4_file e4f;      // copy here because LwextFile object is already deleted
        } close_params;
        struct readdir_params_t
        {
            ext4_dir *e4d;
            struct dirent *de;
        } readdir_params;
        struct unlink_params_t
        {
            const char *pathname;
        } unlink_params;
        struct ftruncate_params_t
        {
            ext4_file *e4f;
            off_t length;
        } ftruncate_params;
    } params;

    SimpleSignal *ss;
    WaitSimpleSignal_params *ss_p;
};

bool ext4_send_message(ext4_message &msg);
ext4_message ext4_mkdir_message(const char *pathname, int mode,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p);
ext4_message ext4_open_message(const char *pathname, int flags, int mode,
    int f, SimpleSignal &ss, WaitSimpleSignal_params &ss_p);
ext4_message ext4_read_message(ext4_file &e4f, char *buf, int nbytes,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p);
ext4_message ext4_write_message(ext4_file &e4f, char *buf, int nbytes,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p);
ext4_message ext4_lseek_message(ext4_file &e4f, off_t offset, int whence,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p);
ext4_message ext4_ftruncate_message(ext4_file &e4f, off_t length,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p);
ext4_message ext4_fstat_message(ext4_file &e4f, ext4_dir &e4d, bool is_dir,
    struct stat *st,
    const char *pathname,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p);
ext4_message ext4_close_message(ext4_file &e4f,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p);
ext4_message ext4_readdir_message(ext4_dir &e4d,
    struct dirent *de,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p);
ext4_message ext4_unlink_message(const char *pathname,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p);
ext4_message ext4_unmount_message(SimpleSignal &ss, WaitSimpleSignal_params &ss_p);
    

class LwextFile : public File
{
    public:
        ssize_t Write(const char *buf, size_t count, int *_errno);
        ssize_t Read(char *buf, size_t count, int *_errno);
        int ReadDir(dirent *de, int *_errno);

        int Fstat(struct stat *buf, int *_errno);
        off_t Lseek(off_t offset, int whence, int *_errno);
        int Ftruncate(off_t length, int *_errno);

        int Close(int *_errno);

        LwextFile(ext4_file fildes, std::string fname);
        ext4_file f;
        ext4_dir d;

        bool is_dir = false;
        std::string fname;
};

#endif
