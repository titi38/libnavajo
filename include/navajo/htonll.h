#ifndef htonll

inline static u_int64_t htonll( const u_int64_t v )
{
    union { u_int32_t lv[2]; u_int64_t llv; } u;
    u.lv[0] = htonl(v >> 32);
    u.lv[1] = htonl(v & 0xFFFFFFFFULL);
    return u.llv;
};

inline static u_int64_t ntohll( const u_int64_t v )
{
  return htonll(v);
}

#endif
