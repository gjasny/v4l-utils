#if !defined(__cplusplus) || __cplusplus < 201103L
	#define fallthrough ((void)0)
#else
	#include <ciso646>
	#ifdef _LIBCPP_VERSION
		#define fallthrough _LIBCPP_FALLTHROUGH()
	#else
		#ifdef __clang__
			#define fallthrough [[clang::fallthrough]]
		#else
			#define fallthrough [[gnu::fallthrough]]
		#endif // __clang__
	#endif // _LIBCPP_VERSION
#endif // __cplusplus
