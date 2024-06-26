#include <cstdio>
#include <cstdlib>

#include "utildef.h"

#include "tape.h"
#include "tape_sorter.h"

const char* TMP_DIR_PATH = "./tmp";

constexpr std::size_t MAX_ALLOWED_BYTES      = sizeof(s32) * 3;    // 3 ints
constexpr u64 MAX_ALLOWED_TAPE_ELEMENT_COUNT = 1024 * 1024 * 1024; // 1 TB!!!

s32 main(s32 argc, const char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "error: not enough arguments supplied\n"
                        "USAGE\n\t./%s <in tape path> <out tape_path>\n",
                argv[0]);
        return -1;
    }

    auto in_tape = std::move(tape::Tape::load_from(argv[1],
                                         MAX_ALLOWED_TAPE_ELEMENT_COUNT));

    if (!in_tape) {
        fprintf(stderr, "failed to initialize in_tape");
        return -1;
    }

    auto out_tape = std::move(tape::Tape::create_at(argv[2],
                                                    MAX_ALLOWED_TAPE_ELEMENT_COUNT));

    if (!out_tape) {
        fprintf(stderr, "failed to initialize out_tape");
        return -1;
    }

    std::string tmp_dir_path = { TMP_DIR_PATH };
    auto sorter = std::move(tape::Tape_Sorter::init(in_tape.value(),
                                                    out_tape.value(),
                                                    tmp_dir_path,
                                                    MAX_ALLOWED_BYTES));

    if (!sorter) {
        fprintf(stderr, "failed to intialize sorter");
        return -1;
    }

   /* if (sorter->sort() != u32(tape::SortError::NO_ERROR)) {
        fprintf(stderr, "sort failed!");
        return -1;
    }*/

    printf("sorter() = %d\n", sorter->sort());

    return 0;
}