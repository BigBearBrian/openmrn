/** \copyright
 * Copyright (c) 2016, Stuart W Baker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file CC32xxDeviceFile.cxx
 * This implements a CC32xx specific device file abstraction.
 *
 * @author Stuart W. Baker
 * @date 16 July 2016
 */

#include "CC32xxDeviceFile.hxx"

#include <fcntl.h>

#include "fs.h"

/*
 * CC32xxDeviceFile::open()
 */
int CC32xxDeviceFile::open(File* file, const char *path, int flags, int mode)
{
    if ((flags & O_ACCMODE) == O_RDWR)
    {
        return -EINVAL;
    }

    if (flags & O_APPEND)
    {
        /* we don't support append mode */
        return -EINVAL;
    }

    if ((flags & O_ACCMODE) != O_RDONLY && !(flags & O_TRUNC))
    {
        /* we only support truncating the file on create and/or write mode */
        return -EINVAL;
    }

    if ((flags & O_CREAT) && (flags & O_EXCL))
    {
        /* we don't support this flag combo, just too hard */
        return -EINVAL;
    }

    if ((flags & O_CREAT) && (flags & O_ACCMODE) == O_RDONLY)
    {
        /* O_CREATE cannot be used along with O_RDONLY */
        return -EINVAL;
    }

    file->offset = 0;

    lock_.lock();
    if (references_ > 0)
    {
        /* File system does not allow simultaneous access for both read and
         * write.  Check if file is already open with the wrong mode.
         */
        if ((((flags & O_ACCMODE) == O_WRONLY) && !writeEnable) ||
            (((flags & O_ACCMODE) == O_RDONLY) && writeEnable))
        {
            lock_.unlock();
            return -EACCES;
        }
    }
    else
    {
        /* file not open yet, open and intialize metadata */
        int32_t result;
        if (mode & O_CREAT)
        {
            result = sl_FsOpen((const unsigned char *)path,
                               FS_MODE_OPEN_CREATE(maxSizeOnCreate, 0),
                               nullptr, &handle);
            writeEnable = true;
        }
        else if (mode & O_WRONLY)
        {
            result = sl_FsOpen((const unsigned char *)path, FS_MODE_OPEN_WRITE,
                               nullptr, &handle);
            writeEnable = true;
        }
        else
        {
            result = sl_FsOpen((const unsigned char *)path, FS_MODE_OPEN_READ,
                               nullptr, &handle);
            writeEnable = false;
        }
        if (result != 0)
        {
            /* error occured in opening file */
            handle = -1;
            lock_.unlock();
            switch (result)
            {
                default:
                    static volatile int err = result;
                    err;
                    HASSERT(0);
                    return -ENOENT;
            }
        }
        SlFsFileInfo_t info;
        sl_FsGetInfo((const unsigned char *)path, 0, &info);
        size = info.FileLen;
        maxSize = info.AllocatedLen;
    }
    lock_.unlock();

    return Node::open(file, path, flags, mode);
}

/*
 * CC32xxDeviceFile::disable()
 */
void CC32xxDeviceFile::disable()
{
    if (handle > 0 && references_ <= 0)
    {
        sl_FsClose(handle, nullptr, nullptr, 0);
        handle = -1;
    }
}

/*
 * CC32xxDeviceFile::write()
 */
ssize_t CC32xxDeviceFile::write(unsigned int index, const void *buf, size_t len)
{
    if ((index + len) > maxSize)
    {
        return -EFBIG;
    }

    int32_t result = sl_FsWrite(handle, index, (unsigned char *)buf, len);

    HASSERT(result >= 0);

    if ((index + result) > size)
    {
        size = index + result;
    }

    return result;
}

/*
 * CC32xxDeviceFile::read()
 */
ssize_t CC32xxDeviceFile::read(unsigned int index, void *buf, size_t len)
{
    if (index >= size)
    {
        /* read starting position beyond end of file */
        return 0;
    }

    if ((index + len) > size)
    {
        len = size - index;
    }

    int32_t result = sl_FsRead(handle, index, (unsigned char *)buf, len);

    HASSERT(result >= 0);

    return result;
}

int CC32xxDeviceFile::fstat(File* file, struct stat *stat) {
    Node::fstat(file, stat);
    SlFsFileInfo_t fs_file_info;
    int ret = sl_FsGetInfo((const uint8_t*)name, 0, &fs_file_info);
    HASSERT(ret == 0);
    stat->st_size = fs_file_info.FileLen;
    stat->st_blocks = fs_file_info.AllocatedLen / 512;
    return 0;
}
