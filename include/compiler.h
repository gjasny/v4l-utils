#ifdef __cplusplus

#ifdef __clang__
#define fallthrough [[clang::fallthrough]]
#else
#define fallthrough [[gnu::fallthrough]]
#endif

#endif
