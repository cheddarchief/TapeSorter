#ifndef TAPE_SORTER_TAPE_SORTER_H
#define TAPE_SORTER_TAPE_SORTER_H

#include "utildef.h"
#include "tape.h"

#include <algorithm>

#include <cstring>
#include <string>

namespace tape {

enum class SortError : u32 {
    NO_ERROR                    = 0,
    FAILED_TO_CREATE_BLANK_TAPE = 1,
    FAILED_TO_READ_FROM_TAPE    = 2,
    FAILED_TO_WRITE_TO_TAPE     = 3,
    MEMORY_ALLOCATION_FAILED    = 4,
    TAPE_MOVE_FORWARD_FAILED    = 5,
    TAPE_REWIND_FAILED          = 6
};

class Tape_Sorter {
    bool is_owner;

    Tape &in_tape;
    Tape &out_tape;

    std::string tmp_dir_path;

    void       *memory;
    std::size_t memory_size;

public:
    ~Tape_Sorter()
    {
        if (is_owner)
            free(memory);
    }

    Tape_Sorter(Tape_Sorter&& o) : Tape_Sorter(o.in_tape, o.out_tape,
                                               o.tmp_dir_path, o.memory,
                                               o.memory_size, o.is_owner)
    {
        o.is_owner = false;
    }

    Tape_Sorter(const Tape_Sorter &o) : Tape_Sorter(o.in_tape, o.out_tape,
                                                    o.tmp_dir_path, o.memory,
                                                    o.memory_size, false)
    {}

    /* only reason to return std::nullopt is failing at allocationg memory */
    static std::optional<Tape_Sorter> init(Tape &in, Tape &out,
                                           const std::string &tmp_dir_path,
                                           std::size_t max_memory_size)
    {
        void *memory = malloc(max_memory_size);
        if (!memory)
            return std::nullopt;

        return Tape_Sorter { in, out, tmp_dir_path, memory, max_memory_size,
                             true };
    }

    u32 sort()
    {
        s32* buffer = reinterpret_cast<s32*>(memory);
        std::size_t buffer_size = memory_size / sizeof(s32);

        if (auto err = in_tape.shrink_to_filesize(); err.type)
            return static_cast<u32>(SortError::FAILED_TO_READ_FROM_TAPE);

        const u64 max_elements = in_tape.get_elements_count();

        constexpr std::size_t tmp_tapes_count = 4;
        Tape tmp_tapes[tmp_tapes_count];

        // Initialize blank tapes
        std::string tmp_path = tmp_dir_path + "/tmp1x";
        for (std::size_t i = 0; i < tmp_tapes_count; ++i) {
            tmp_path[tmp_path.size() - 1] = '0' + i;
            if (!Tape::init_blank(tmp_tapes[i], tmp_path.c_str(), max_elements))
                return static_cast<u32>(SortError::FAILED_TO_CREATE_BLANK_TAPE);
        }

        // Fill first pair of temp tapes with sorted chunks of s32
        bool finished = false;
        for (std::size_t buf_idx = 0; !finished; buf_idx = (buf_idx + 1) & 1) {
            std::size_t count = 0;
            while (count < buffer_size) {
                s32 value;

                if (auto err = in_tape.get(value); err.type)
                    return static_cast<u32>(SortError::FAILED_TO_READ_FROM_TAPE);

                buffer[count] = value;

                if (in_tape.move_forward(1).type) {
                    finished = true;
                    break;
                }

                count += 1;
            }

            std::sort(buffer, buffer + count);

            for (std::size_t i = 0; i < count; ++i) {
                if (auto err = tmp_tapes[buf_idx].set(buffer[i]); err.type)
                    return static_cast<u32>(SortError::FAILED_TO_WRITE_TO_TAPE);

                if (auto err = tmp_tapes[buf_idx].move_forward(1); err.type)
                    return static_cast<u32>(SortError::TAPE_MOVE_FORWARD_FAILED);
            }
        }

        // Merging sorted chunks
        std::size_t chunk_size = buffer_size;
        while (chunk_size < max_elements) {
            for (std::size_t i = 0; i < tmp_tapes_count; i += 2) {
                tmp_tapes[i].rewind();
                tmp_tapes[i + 1].rewind();

                Tape& src_tape1 = tmp_tapes[i];
                Tape& src_tape2 = tmp_tapes[i + 1];
                Tape& dst_tape  = tmp_tapes[(i + 2) % tmp_tapes_count];

                s32 value1, value2;
                bool has_value1 = !src_tape1.get(value1).type;
                bool has_value2 = !src_tape2.get(value2).type;

                std::size_t elements_merged = 0;

                while (has_value1 && has_value2 && elements_merged < chunk_size) {
                    if (value1 < value2) {
                        dst_tape.set(value1);
                        has_value1 = !src_tape1.move_forward(1).type && !src_tape1.get(value1).type;
                    }
                    else {
                        dst_tape.set(value2);
                        has_value2 = !src_tape2.move_forward(1).type && !src_tape2.get(value2).type;
                    }
                    ++elements_merged;
                }

                // Flush the remaining values from both tapes, but only up to chunk_size
                while (has_value1 && elements_merged < chunk_size) {
                    dst_tape.set(value1);
                    has_value1 = !src_tape1.move_forward(1).type && !src_tape1.get(value1).type;
                    ++elements_merged;
                }
                while (has_value2 && elements_merged < chunk_size) {
                    dst_tape.set(value2);
                    has_value2 = !src_tape2.move_forward(1).type && !src_tape2.get(value2).type;
                    ++elements_merged;
                }

                dst_tape.rewind();
            }

            chunk_size *= 2;
        }

        // Write the final sorted data to out_tape
        tmp_tapes[0].rewind();
        s32 value;
        u64 elements_written = 0;
        while (elements_written < max_elements && !tmp_tapes[0].get(value).type) {
            if (auto err = out_tape.set(value); err.type)
                return static_cast<u32>(SortError::FAILED_TO_WRITE_TO_TAPE);

            out_tape.move_forward(1);
            tmp_tapes[0].move_forward(1);
            ++elements_written;
        }


        return static_cast<u32>(SortError::NO_ERROR);
    }

private:
    Tape_Sorter(Tape              &in_tape,
                Tape              &out_tape,
                const std::string &tmp_dir_path,
                void              *memory,
                std::size_t        memory_size,
                bool               is_owner) : in_tape(in_tape),
                                               out_tape(out_tape),
                                               tmp_dir_path(
                                                    std::move(tmp_dir_path)),
                                               memory(memory),
                                               memory_size(memory_size),
                                               is_owner(is_owner)
    {}
};

}

#endif
