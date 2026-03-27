#pragma array_limit 1000000
#pragma pattern_limit 1000000000

const u32 NGRAM_SIZE = 3;

struct Header {
    char magic[8];
    le u64 pathslen;
    le u64 ngrams;
};

struct Path {
    le u16 allocation_size;
    le u16 offset_to_prefix;
    le u16 prefix_length;
    le u16 suffix_length;
    char suffix_bytes[allocation_size - 4*sizeof(u16)];
};

struct Entry {
    le u32 postlen;
    char ngram[NGRAM_SIZE];
    padding[NGRAM_SIZE % 2];
    le u64 offsets[postlen];
};

Header header @ $;
Path paths[while($ < sizeof(Header) + header.pathslen)] @ $;
Entry index[header.ngrams] @ $;
