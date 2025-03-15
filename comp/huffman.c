#include "bit_stream.h"
#include <stdlib.h>
#include <string.h>

enum { MODE_DECOMPRESS = 0, MODE_COMPRESS };

struct ct_node {
    struct ct_node *left, *right;
    int value;
    int freq;
};

struct ctq {
    struct ct_node **nodes;
    int size;
};

int popcnt(unsigned x) {
    x = ((x >> 1) & 0x55555555) + (x & 0x55555555);
    x = ((x >> 2) & 0x33333333) + (x & 0x33333333);
    x = ((x >> 4) & 0x0f0f0f0f) + (x & 0x0f0f0f0f);
    x = ((x >> 8) & 0x00ff00ff) + (x & 0x00ff00ff);
    x = ((x >> 16) & 0x0000ffff) + (x & 0x0000ffff);
    return x;
}

int log2i(unsigned x) {
    x = x | x >> 1;
    x = x | x >> 2;
    x = x | x >> 4;
    x = x | x >> 8;
    x = x | x >> 16;
    return popcnt(x) - 1;
}

void print_heap(struct ctq *ctq) {
    int i, j;
    int num_levels = log2i(ctq->size) + 1;
    int level_start = 0;

    if (ctq->size == 0) {
        printf("Heap is empty.\n");
        return;
    }

    for (i = 0; i < num_levels; i++) {
        int level_end = level_start + (1 << i); // 2^i elements in level
        if (level_end > ctq->size) {
            level_end = ctq->size;
        }

        // Print the current level
        for (j = level_start; j < level_end; j++) {
            printf("%d ", ctq->nodes[j]->freq);
        }
        printf("\n");

        level_start = level_end;
    }
}

void print_tree(struct ct_node *node, int depth) {
    int i;

    if (node == NULL) {
        return;
    }

    // Print right subtree first (makes visualization clearer)
    print_tree(node->right, depth + 1);

    // Print current node with indentation
    for (i = 0; i < depth; i++) {
        printf("    "); // Indentation (4 spaces per level)
    }

    // Print node value and frequency
    if (node->left == NULL && node->right == NULL) {
        printf("[%d] (freq: %d)\n", node->value, node->freq); // Leaf node
    } else {
        printf("(freq: %d)\n", node->freq); // Internal node
    }

    // Print left subtree
    print_tree(node->left, depth + 1);
}

int get_parent_index(int child_index) { return (child_index - 1) >> 1; }

int get_left_child_index(int parent_index) { return (parent_index << 1) + 1; }

void ctq_insert(struct ctq *ctq, struct ct_node *ct_node) {
    int index;

    ctq->nodes[ctq->size] = ct_node;
    index = ctq->size++;

    while (index) {
        int parent = get_parent_index(index);

        if (ct_node->freq < ctq->nodes[parent]->freq) {
            ctq->nodes[index] = ctq->nodes[parent];
            ctq->nodes[parent] = ct_node;
            index = parent;
        } else {
            break;
        }
    }
}

struct ct_node *ctq_remove(struct ctq *ctq) {
    int index;
    struct ct_node *node = ctq->nodes[0];

    ctq->nodes[0] = ctq->nodes[--ctq->size];
    index = 0;

    while (1) {
        int left_child = get_left_child_index(index);
        int right_child = left_child + 1;
        int smallest = index;
        struct ct_node *tmp;

        if (left_child < ctq->size && ctq->nodes[left_child]->freq < ctq->nodes[smallest]->freq) {
            smallest = left_child;
        }

        if (right_child < ctq->size && ctq->nodes[right_child]->freq < ctq->nodes[smallest]->freq) {
            smallest = right_child;
        }

        if (smallest == index) {
            break;  // Heap property is restored
        }

        tmp = ctq->nodes[index];
        ctq->nodes[index] = ctq->nodes[smallest];
        ctq->nodes[smallest] = tmp;

        index = smallest;
    }

    return node;
}

int freqs[257];
int nz_freqs_count;
struct ct_node *code_tree;

int64_t codes[257][4];
int code_lengths[257];

struct ct_node *reconstruct_tree(BitStream *reader) {
    struct ct_node *node;
    int bit = bit_stream_read(reader);

    if (bit == 0) {
        // Internal node.
        struct ct_node *left, *right;
        left = reconstruct_tree(reader);
        right = reconstruct_tree(reader);

        node = malloc(sizeof(*node));
        node->left = left;
        node->right = right;
    } else {
        // Leaf node.
        int i, value;

        for (i = 0, value = 0; i < 9; i++) {
            value |= bit_stream_read(reader) << (8 - i);
        }

        if (value == 256) {
            printf("Found peof!\n");
        }

        node = malloc(sizeof(*node));
        node->left = node->right = NULL;
        node->freq = -1;
        node->value = value;
    }

    return node;
}

int reconstruct_value(BitStream *reader) {
    struct ct_node *node = code_tree;

    while (node->left && node->right) {
        int bit = bit_stream_read(reader);

        if (bit == 0) {
            // Go left.
            node = node->left;
        } else {
            // Go right.
            node = node->right;
        }
    }

    return node->value;
}

void huffman_decompress(FILE *input, FILE *output) {
    int value;
    BitStream reader = bit_stream_reader(input);

    code_tree = reconstruct_tree(&reader);

    while ((value = reconstruct_value(&reader)) != 256) {
        unsigned char byte = value;
        fwrite(&byte, 1, 1, output);
    }

    fclose(input);
    fclose(output);
}

void print_code(int64_t code[4], int code_length) {
    int i, j;
    int i_start, j_start;

    i_start = (code_length - 1) / 64;
    j_start = (code_length - 1) % 64;
    for (i = i_start; i >= 0; i--) {
        for (j = j_start; j >= 0; j--) {
            int bit = (code[i] >> j) & 1;
            printf("%d", bit);
        }
    }
}

void collect_freqs(unsigned char *buffer, size_t size) {
    int i;

    for (i = 0; i < size; i++) {
        freqs[buffer[i]]++;
    }

    freqs[256] = 1;

    for (i = 0; i < 256; i++) {
        if (freqs[i] != 0) {
            nz_freqs_count++;
        }
    }
}

void build_code_tree(void) {
    struct ctq ctq;
    int i;
    struct ct_node *peof;

    ctq.nodes = malloc(nz_freqs_count * sizeof(*ctq.nodes));
    ctq.size = 0;

    /* 256 -> peof */
    for (i = 0; i < 257; i++) {
        if (freqs[i]) {
            struct ct_node *node = malloc(sizeof(*node));
            node->freq = freqs[i];
            node->value = i;

            ctq_insert(&ctq, node);
        }
    }

    while (ctq.size >= 2) {
        struct ct_node *left = ctq_remove(&ctq);
        struct ct_node *right = ctq_remove(&ctq);

        struct ct_node *node = malloc(sizeof(*node));
        node->left = left;
        node->right = right;
        node->value = -1;
        node->freq = left->freq + right->freq;

        ctq_insert(&ctq, node);
    }

    code_tree = ctq_remove(&ctq);

    free(ctq.nodes);
}

void traverse(struct ct_node *node, int64_t code[4], int length) {
    int i;
    int64_t new_code[4];

    if (!node->left && !node->right) {
        /* Leaf node. */

        memcpy(codes[node->value], code, sizeof(new_code));
        code_lengths[node->value] = length;
        return;
    }

    memcpy(new_code, code, sizeof(new_code));

    for (i = 3; i >= 1; i--) {
        new_code[i] <<= 1;
        new_code[i] |= (new_code[i - 1] >> 63) & 1;
    }

    new_code[0] <<= 1;
    traverse(node->left, new_code, length + 1);

    new_code[0] |= 1;
    traverse(node->right, new_code, length + 1);
}

void generate_codes(void) {
    struct ct_node *node = code_tree;
    int64_t code[4] = {0};

    traverse(code_tree, code, 0);
}

void write_code(BitStream *stream, int64_t code[4], int code_length) {
    int i, j;
    int i_start, j_start;

    i_start = (code_length - 1) / 64;
    j_start = (code_length - 1) % 64;
    for (i = i_start; i >= 0; i--) {
        for (j = j_start; j >= 0; j--) {
            int bit = (code[i] >> j) & 1;
            bit_stream_write(stream, bit);
        }
    }
}

void write_tree(BitStream *stream, struct ct_node *node) {
    if (!node->left && !node->right) {
        // Leaf node.
        int i;
        bit_stream_write(stream, 1);

        for (i = 0; i < 9; i++) {
            bit_stream_write(stream, node->value >> (8 - i));
        }

        return;
    }

    bit_stream_write(stream, 0);
    write_tree(stream, node->left);
    write_tree(stream, node->right);
}

void translate_to_code(unsigned char *input, size_t size, BitStream output) {
    int i;

    write_tree(&output, code_tree);

    for (i = 0; i < size; i++) {
        int val = input[i];
        write_code(&output, codes[val], code_lengths[val]);
    }

    write_code(&output, codes[256], code_lengths[256]);
    bit_stream_flush(&output);
}

void huffman_compress(FILE *input, FILE *output) {
    size_t size;
    unsigned char *buf;

    fseek(input, 0, SEEK_END);
    size = ftell(input);
    fseek(input, 0, SEEK_SET);

    buf = malloc(size);
    fread(buf, 1, size, input);

    fclose(input);

    collect_freqs(buf, size);
    build_code_tree();
    generate_codes();


    translate_to_code(buf, size, bit_stream_writer(output));
    fclose(output);

    printf("peof code: ");
    print_code(codes[256], code_lengths[256]);
}

void do_huffman(FILE *input, FILE *output, int mode) {
    switch (mode) {
        case MODE_DECOMPRESS:
            huffman_decompress(input, output);
            break;
        case MODE_COMPRESS:
            huffman_compress(input, output);
            break;
    }
}

void print_usage(const char *s) {
    fprintf(stderr, "usage: %s <d|c> <in_file> <out_file>", s);
}

int main(int argc, char *argv[]) {
    const char *mode_string, *in_file_string, *out_file_string;
    FILE *in_fp, *out_fp;
    int mode;

    if (argc != 4) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    mode_string = argv[1];
    in_file_string = argv[2];
    out_file_string = argv[3];

    if (strcmp(mode_string, "d") == 0) {
        mode = MODE_DECOMPRESS;
    } else if (strcmp(mode_string, "c") == 0) {
        mode = MODE_COMPRESS;
    } else {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    in_fp = fopen(in_file_string, "rb");
    if (!in_fp) {
        perror("error:");
        return EXIT_FAILURE;
    }

    out_fp = fopen(out_file_string, "wb");
    if (!out_fp) {
        perror("error:");
        return EXIT_FAILURE;
    }

    do_huffman(in_fp, out_fp, mode);
}
