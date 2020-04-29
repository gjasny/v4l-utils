#ifdef __cplusplus

#ifdef _LIBCPP_VERSION
#define fallthrough _LIBCPP_FALLTHROUGH()
#else

#ifdef __clang__
#define fallthrough [[clang::fallthrough]]
#else
#define fallthrough [[gnu::fallthrough]]
#endif

#endif
#endif
