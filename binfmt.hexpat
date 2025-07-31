struct Header {
    char magic[8];
    le u64 ngrams;
    le u64 pathslen;
};

struct Entry {
    le u32 postlen;
    char ngram[3];
    padding[1];
    le u64 offsets[postlen];
};

Header header @ 0x0;

u8 paths[header.pathslen] @ sizeof(Header);

Entry index[header.ngrams] @ sizeof(Header) + header.pathslen;
