#ifndef TAPE_SORTER_TAPE_SORTER_H
#define TAPE_SORTER_TAPE_SORTER_H

#include "utildef.h"
#include "tape.h"

#include <algorithm>

#include <cstring>
#include <string>

namespace tape {

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

    #define FAILED_TO_CREATE_BLANK_TAPE 2

    // TODO: create a error type for this method
    u32 sort()
    {
        // Algorithm:
        //      - [X] create 4 buffers
        //      - [X] fill first two with sorted chunks of items
        //      - [.] merge pairwise untill the chunk size >= max_elements
        //      - [ ] write to out_tape
        //      - PROFIT!!!!

        s32 *buffer             = reinterpret_cast<s32*>(memory);
        std::size_t buffer_size = memory_size / sizeof(buffer[0]);

        if (auto err = in_tape.shrink_to_filesize(); err.type)
            return (u32) err.type;

        const u64 max_elements = in_tape.get_elements_count();

        constexpr std::size_t tmp_tapes_count = 4;
        Tape                  tmp_tapes[tmp_tapes_count];

        { // TODO: extract to method
            std::string tmp_path = tmp_dir_path + "/tmp1x";
            for (std::size_t i = 0; i < tmp_tapes_count; ++i) {
                tmp_path[tmp_path.size() - 1] = '0' + i;
                if (!Tape::init_blank(tmp_tapes[i], tmp_path.c_str(),
                    max_elements))
                    return FAILED_TO_CREATE_BLANK_TAPE;
            }
        }

        { // filling first pair of temp tapes with sorted chunks of s32
            bool finished = false;
            for (std::size_t buf_idx = 0; !finished;
                    buf_idx = (buf_idx + 1) & 1) {
                std::size_t count = 0;
                while (count < buffer_size) {
                    s32 value;

                    if (auto err = in_tape.get(value); err.type)
                        return err.type;

                    buffer[count] = value;

                    if (in_tape.move_forward(1).type) {
                        finished = true;
                        break;
                    }

                    count += 1;
                }

                std::sort(buffer, buffer + buffer_size);

                for (std::size_t i = 0; i < count; ++i) {
                    if (auto err = tmp_tapes[buf_idx].set(buffer[i]);
                            err.type) {
                        return err.type;
                    }

                    tmp_tapes[buf_idx].move_forward(1);
                }
            }
        }

        { // Let the great merge begin
            // algorithm:
            //      1. we take one element from each of the tmp tapes and compare them
            //      2. the smallest one is written onto buf_idx buffer,
            //         untill we get to the chunk_size
            //      3. increase chunk_size by << 1
            //      4. swap the first pair of buffers with second
            //      5. repeat 1-4 untill the chunks_size is >= max_elements

            std::size_t chunk_size = buffer_size << 1;

            while (chunk_size <= max_elements) {
                std::size_t first_pair_offset  = 0;
                std::size_t second_pair_offset = 2;
                bool        finished           = false;

                const std::size_t chunk_size_half = chunk_size >> 1;

                while (!finished) {
                    std::size_t idx_min;
                    std::size_t out_idx = 0;

                    std::size_t counter[2] = { 0, 0 };

                    for (std::size_t i = 0; i < chunk_size; ++i) {
                        s32 first  {0};
                        s32 second {0};

                        if (counter[0] < chunk_size_half) {
                            if (auto err = tmp_tapes[first_pair_offset + 0].get(first); err.type) {
                                fprintf(stderr, "i/o problem: %s\n", err.msg);
                                return -1;
                            }

                            counter[0] += 1;
                        } else {
                            // we just load the rest elements of the second tape on the out
                            for (std::size_t j = 0; j < (counter[1] - chunk_size_half); ++j) {
                                s32 value;

                                if (auto err = tmp_tapes[first_pair_offset + 1].get(value); err.type) {
                                    fprintf(stderr, "i/o problem: %s\n", err.msg);
                                    return -1;
                                }

                                tmp_tapes[first_pair_offset + 1].move_forward(1);

                                if (auto err = tmp_tapes[second_pair_offset + out_idx].set(value); err.type) {
                                    fprintf(stderr, "i/o problem: %s\n", err.msg);
                                    return -1;
                                }

                                tmp_tapes[second_pair_offset + out_idx].move_forward(1);

                                goto OUT;
                            }
                        }

                        if (counter[1] < chunk_size_half) {
                            if (auto err = tmp_tapes[first_pair_offset + 1].get(second); err.type) {
                                fprintf(stderr, "i/o problem: %s\n", err.msg);
                                return -1;
                            }

                            counter[1] += 1;
                        } else {
                            // we just load the rest elements of the second tape on the out
                            for (std::size_t j = 0; j < (counter[0] - chunk_size_half); ++j) {
                                s32 value;

                                if (auto err = tmp_tapes[first_pair_offset + 0].get(value); err.type) {
                                    fprintf(stderr, "i/o problem: %s\n", err.msg);
                                    return -1;
                                }

                                tmp_tapes[first_pair_offset + 0].move_forward(1);

                                if (auto err = tmp_tapes[second_pair_offset + out_idx].set(value); err.type) {
                                    fprintf(stderr, "i/o problem: %s\n", err.msg);
                                    return -1;
                                }

                                tmp_tapes[second_pair_offset + out_idx].move_forward(0);

                                goto OUT;
                            }
                        }

                        idx_min = first <= second;
                        if (idx_min) {
                            if (auto err = tmp_tapes[second_pair_offset + out_idx].set(first); err.type) {
                                fprintf(stderr, "i/o problem: %s\n", err.msg);
                                return -1;
                            }
                        } else {
                            if (auto err = tmp_tapes[second_pair_offset + out_idx].set(second); err.type) {
                                fprintf(stderr, "i/o problem: %s\n", err.msg);
                                return -1;
                            }
                        }

                        tmp_tapes[second_pair_offset + out_idx].move_forward(1);

                        tmp_tapes[idx_min].move_forward(1);
                        counter[idx_min] += 1;
                    }
OUT:

                    out_idx = (out_idx + 1) & 1;
                }

                chunk_size <<= 1;
                second_pair_offset = first_pair_offset;
                first_pair_offset  = ((first_pair_offset + 1) & 1) << 1;
            }
        }

        return 0;
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
