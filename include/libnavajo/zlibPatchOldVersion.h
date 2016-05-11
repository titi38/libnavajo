//********************************************************
/**
 * @file  zlibPatchOldVersion.h
 *
 * @brief functions inflateSetDictionary and 
 *  inflateGetDictionary for versions older than 1.8
 *        
 *
 * @author T.Descombes (thierry.descombes@gmail.com)
 *
 * @version 1        
 * @date 11/05/2016
 */
//********************************************************


typedef enum {
    HEAD,       /* i: waiting for magic header */
            FLAGS,      /* i: waiting for method and flags (gzip) */
            TIME,       /* i: waiting for modification time (gzip) */
            OS,         /* i: waiting for extra flags and operating system (gzip) */
            EXLEN,      /* i: waiting for extra length (gzip) */
            EXTRA,      /* i: waiting for extra bytes (gzip) */
            NAME,       /* i: waiting for end of file name (gzip) */
            COMMENT,    /* i: waiting for end of comment (gzip) */
            HCRC,       /* i: waiting for header crc (gzip) */
            DICTID,     /* i: waiting for dictionary check value */
            DICT,       /* waiting for inflateSetDictionary() call */
            TYPE,       /* i: waiting for type bits, including last-flag bit */
            TYPEDO,     /* i: same, but skip check to exit inflate on new block */
            STORED,     /* i: waiting for stored size (length and complement) */
            COPY_,      /* i/o: same as COPY below, but only first time in */
            COPY,       /* i/o: waiting for input or output to copy stored block */
            TABLE,      /* i: waiting for dynamic block table lengths */
            LENLENS,    /* i: waiting for code length code lengths */
            CODELENS,   /* i: waiting for length/lit and distance code lengths */
            LEN_,       /* i: same as LEN below, but only first time in */
            LEN,        /* i: waiting for length/lit/eob code */
            LENEXT,     /* i: waiting for length extra bits */
            DIST,       /* i: waiting for distance code */
            DISTEXT,    /* i: waiting for distance extra bits */
            MATCH,      /* o: waiting for output space to copy string */
            LIT,        /* o: waiting for output space to write literal */
            CHECK,      /* i: waiting for 32-bit check value */
            LENGTH,     /* i: waiting for 32-bit length (gzip) */
            DONE,       /* finished check, done -- remain here until reset */
            BAD,        /* got a data error -- remain here until reset */
            MEM,        /* got an inflate() memory error -- remain here until reset */
            SYNC        /* looking for synchronization bytes to restart inflate() */
} inflate_mode;

struct inflate_state2 {
    inflate_mode mode;
    /* current inflate mode */
    int last;
    /* true if processing last block */
    int wrap;
    /* bit 0 true for zlib, bit 1 true for gzip */
    int havedict;
    /* true if dictionary provided */
    int flags;
    /* gzip header method and flags (0 if zlib) */
    unsigned dmax;
    /* zlib header max distance (INFLATE_STRICT) */
    unsigned long check;
    /* protected copy of check value */
    unsigned long total;
    /* protected copy of output count */
    gz_headerp head;            /* where to save gzip header information */
    /* sliding window */
    unsigned wbits;
    /* log base 2 of requested window size */
    unsigned wsize;
    /* window size or zero if not using window */
    unsigned whave;
    /* valid bytes in the window */
    unsigned wnext;
    /* window write index */
    unsigned char FAR *window;  /* allocated sliding window, if needed */
};


inline int ZEXPORT inflateGetDictionary(z_streamp strm, Bytef *dictionary, uInt *dictLength)
{
struct inflate_state2 FAR *state;
/* check state */
if (strm == Z_NULL || strm->state == Z_NULL) return Z_STREAM_ERROR;
state = (inflate_state2 FAR *) strm->state;
/* copy dictionary */
if (state->whave && dictionary != Z_NULL) {
memcpy(dictionary, state->window + state->wnext,
       state->whave - state->wnext);
memcpy(dictionary + state->whave - state->wnext,
       state->window, state->wnext);
}
if (dictLength != Z_NULL)
*dictLength = state->whave;
return Z_OK;
}
