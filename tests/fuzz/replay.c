/*
 * Replay driver: runs a fuzz harness on the files given on the command line,
 * without libFuzzer. Lets any compiler (and the sanitizer CI job) check the
 * corpus and every crash reproducer committed under tests/fuzz/corpus/.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int main(int argc, char** argv) {
    int ran = 0;
    for (int i = 1; i < argc; i++) {
        FILE* f = fopen(argv[i], "rb");
        if (!f) { fprintf(stderr, "replay: cannot open %s\n", argv[i]); return 1; }
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        uint8_t* buf = malloc(n > 0 ? (size_t)n : 1);
        if (!buf) { fclose(f); return 1; }
        size_t got = n > 0 ? fread(buf, 1, (size_t)n, f) : 0;
        fclose(f);
        LLVMFuzzerTestOneInput(buf, got);
        free(buf);
        ran++;
    }
    printf("replayed %d input(s)\n", ran);
    return 0;
}
