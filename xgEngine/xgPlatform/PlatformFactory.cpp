#include "pch.h"
#include "FileWin.h"

namespace xg
{
    std::unique_ptr<File> LoadFile(const wchar_t* path, FileAccessMode mode)
    {
        auto file = std::make_unique<FileWin>(path);
        if (file->Open(mode))
            return file;

        return nullptr;
    }
}
