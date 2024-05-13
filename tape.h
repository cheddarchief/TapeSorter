#ifndef TAPE_SORTER_TAPE_H
#define TAPE_SORTER_TAPE_H

#include <optional>

#include "utildef.h"

namespace tape {

template <typename Result>
struct ITape {
    virtual Result get(s32 &dest)          = 0;
    virtual Result set(s32 value)          = 0;

    virtual Result move_forward(u64 n)     = 0;
    virtual Result move_backward(u64 n)    = 0;

    virtual u64 get_elements_count() const = 0;
};

struct Tape_Result {
    enum {
        NO_ERROR = 0,

        TRIED_TO_MOVE_FURTHER_LAST_ELEMENT = 1,
        TRIED_TO_MOVE_BEFORE_FIRST_ELEMENT = 1,

        FAILED_TO_MOVE_FILE_POINTER = 2,
        FAILED_TO_READ_FROM_FILE    = 3,
        FAILED_TO_WRITE_TO_FILE     = 4,
    } type          { NO_ERROR };
    const char* msg { 0 };
};

class Tape: public ITape<Tape_Result> {
    bool  is_owner;

    FILE *handle;
    u64   pointer;
    u64   elements_count;

public:
    using Result = Tape_Result;

    // good option for when we need to preallocate an array of tapes
    Tape(): is_owner(false), handle(nullptr), pointer(0), elements_count(0)
    {}

    static bool init_existing(Tape &tape, const char* path, u64 max_elements)
    {
        return init(tape, path, "r+", max_elements);
    }

    static bool init_blank(Tape &tape, const char* path, u64 max_elements)
    {
        return init(tape, path, "w+", max_elements);
    }

    ~Tape()
    {
        if (is_owner)
            fclose(handle);
    }

    Tape(Tape &&o) : Tape(o.handle, o.pointer, o.elements_count, o.is_owner)
    {
        // ensures ownership
        o.is_owner = false;
    }

    Tape(Tape &o) : Tape(o.handle, o.pointer, o.elements_count, false)
    {}

    static std::optional<Tape> load_from(const char* path, u64 max_elements)
    {
        FILE* handle = fopen(path, "r+");
        return std::move(Tape::create(handle, max_elements));
    }

    static std::optional<Tape> create_at(const char* path, u64 max_elements)
    {
        FILE* handle = fopen(path, "w+");
        return std::move(Tape::create(handle, max_elements));
    }

    Result get(s32 &dest) override
    {
        update_file_pointer();

        fread(&dest, sizeof(s32), 1, handle);

        if (ferror(handle))
            return Result {
                Result::FAILED_TO_READ_FROM_FILE,
                strerror(errno)
            };

        return Result {};
    }

    Result set(s32 value) override
    {
        update_file_pointer();

        fwrite(&value, sizeof(s32), 1, handle);

        if (ferror(handle))
            return Result {
                Result::FAILED_TO_WRITE_TO_FILE,
                strerror(errno)
            };

        return Result {};
    }

    Result move_forward(u64 n) override
    {
        u64 new_pos = pointer + n;
        if (new_pos > elements_count)
            return Result {
                Result::TRIED_TO_MOVE_FURTHER_LAST_ELEMENT
            };

        pointer = new_pos;

        return Result {};
    }

    Result move_backward(u64 n) override
    {
        if (n > pointer)
            return Result {
                Result::TRIED_TO_MOVE_BEFORE_FIRST_ELEMENT
            };

        pointer -= n;

        return Result {};
    }

    u64 get_elements_count() const override
    {
        return elements_count;
    }

    Result shrink_to_filesize()
    {
        fseek(handle, 0, SEEK_END);
        u64 file_size = ftell(handle);

        if (ferror(handle)) {
            return Result {
                Result::FAILED_TO_MOVE_FILE_POINTER,
                strerror(errno),
            };
        }

        this->elements_count = file_size / sizeof(s32);

        return Result {};
    }

private:
    Tape(FILE *handle,
         u64   pointer,
         u64   elements_count,
         bool  is_owner) : handle(handle), pointer(pointer),
                           elements_count(elements_count), is_owner(is_owner)
    {}

    /* This allows for exception-free Tape initialization */
    static std::optional<Tape> create(FILE *handle,
                                      u64   elements_count)
    {
        if (!handle)
            return std::nullopt;

        return Tape(handle, 0, elements_count, true);
    }

    static bool init(Tape &self, const char* path, const char* flags,
                     u64 max_elements)
    {
        FILE* handle = fopen(path, flags);
        if (!handle)
            return false;

        self.handle         = handle;
        self.elements_count = max_elements;
        self.pointer        = 0;
        self.is_owner       = true;

        return true;
    }

    Result update_file_pointer()
    {
        fseek(handle, pointer * sizeof(s32), SEEK_SET);

        if (ferror(handle)) {
            return Result {
                Result::FAILED_TO_MOVE_FILE_POINTER,
                strerror(errno),
            };
        }

        return Result {};
    }
};

}

#endif // TAPE_SORTER_TAPE_H