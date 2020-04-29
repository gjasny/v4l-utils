#ifdef _LIBCPP_VERSION
#define fallthrough _LIBCPP_FALLTHROUGH()
#else

#if __cplusplus >= 201103L

#ifdef __clang__
#define fallthrough [[clang::fallthrough]]
#else
#define fallthrough [[gnu::fallthrough]]
#endif // __clang__

#else
#define fallthrough ((void)0)

#endif // __cplusplus
#endif // _LIBCPP_VERSION
