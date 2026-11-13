#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef PLATFORM_EXPORT
#define PLATFORM_API __declspec(dllexport)
#else
#define PLATFORM_API __declspec(dllimport)
#endif
#else
#if defined(__GNUC__) && __GNUC__ >= 4
#define PLATFORM_API __attribute__ ((visibility ("default")))
#else
#define PLATFORM_API
#endif
#endif
