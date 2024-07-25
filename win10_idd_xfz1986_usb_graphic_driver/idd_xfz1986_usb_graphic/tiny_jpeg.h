/**
 * tiny_jpeg.h
 *
 * Tiny JPEG Encoder
 *  - Sergio Gonzalez
 *
 * This is a readable and simple single-header JPEG encoder.
 *
 * Features
 *  - Implements Baseline DCT JPEG compression.
 *  - No dynamic allocations.
 *
 * This library is coded in the spirit of the stb libraries and mostly follows
 * the stb guidelines.
 *
 * It is written in C99. And depends on the C standard library.
 * Works with C++11
 *
 *
 * ==== Thanks ====
 *
 *  AssociationSirius (Bug reports)
 *  Bernard van Gastel (Thread-safe defaults, BSD compilation)
 *
 *
 * ==== License ====
 *
 * This software is in the public domain. Where that dedication is not
 * recognized, you are granted a perpetual, irrevocable license to copy and
 * modify this file as you see fit.
 *
 */

// ============================================================
// Usage
// ============================================================
// Include "tiny_jpeg.h" to and use the public interface defined below.
//
// You *must* do:
//
//      #define TJE_IMPLEMENTATION
//      #include "tiny_jpeg.h"
//
// in exactly one of your C files to actually compile the implementation.


// Here is an example program that loads a bmp with stb_image and writes it
// with Tiny JPEG

/*

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h"


int main()
{
    int width, height, num_components;
    unsigned char* data = stbi_load("in.bmp", &width, &height, &num_components, 0);
    if ( !data ) {
        puts("Could not find file");
        return EXIT_FAILURE;
    }

    if ( !tje_encode_to_file("out.jpg", width, height, num_components, data) ) {
        fprintf(stderr, "Could not write JPEG\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

*/



#ifdef __cplusplus
extern "C"
{
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"  // We use {0}, which will zero-out the struct.
#pragma GCC diagnostic ignored "-Wmissing-braces"
#pragma GCC diagnostic ignored "-Wpadded"
#endif

// ============================================================
// Public interface:
// ============================================================

#ifndef TJE_HEADER_GUARD
#define TJE_HEADER_GUARD

#define KERNEL_MODE
#define FLOAT_INT_MODE


#define JPEG_MAX_SIZE (600*480*4)

#ifdef KERNEL_MODE

#if 0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>
#endif
#if 0
#define LOG(args...) do {printk(args);} while(0)
#else
//#define LOG(fmt, ...) do {;} while(0)
#endif
//#define LOGI(fmt, ...) do {;} while(0)
//#define LOGW(fmt, ...) do {;} while(0)
#define xmalloc(x) malloc
#define xfree(x) free((x))
#define assert(x)  do {;} while(0)

#else
#define __linux__
#define LOG(args...) do {;} while(0)
//#define LOG(args...) do {printf(args);} while(0)
#define LOGI(args...) do {printf(args);} while(0)
#define LOGW(args...) do {printf(args);} while(0)
#define xmalloc malloc

// C std lib
#include <assert.h>
#include <inttypes.h>
#include <math.h>   // floorf, ceilf
#include <stdio.h>  // FILE, puts
#include <string.h> // memcpy

#endif

    typedef  __int32 int32_t;
    typedef unsigned __int32 uint32_t;
    typedef unsigned __int16 uint16_t;
    typedef unsigned __int8 uint8_t;
    typedef  __int64  int64_t;



#ifdef FLOAT_INT_MODE
#define FLOAT_INT32_T  int32_t
#define FLOAT_2_INT32(X) ((int32_t)((X)*(1024)))
#define INT8_2_INT32(X) ((int32_t)((X)*(1024)))
#define FLOAT_2_INT32_SCALE_BACK(X) ((FLOAT_INT32_T)((X)/1024))
#define floorf(x) (x)
#define ceilf(x) (x)

#else

#define FLOAT_INT32_T  float
#define FLOAT_2_INT32(X)  (X)
#define INT8_2_INT32(X)   (X)
#define FLOAT_2_INT32_SCALE_BACK(X) (X)

#endif



// - tje_encode_to_file -
//
// Usage:
//  Takes bitmap data and writes a JPEG-encoded image to disk.
//
//  PARAMETERS
//      dest_path:          filename to which we will write. e.g. "out.jpg"
//      width, height:      image size in pixels
//      num_components:     3 is RGB. 4 is RGBA. Those are the only supported values
//      src_data:           pointer to the pixel data.
//
//  RETURN:
//      0 on error. 1 on success.

    int tje_encode_to_ctx(
        void * ctx,
        const int width,
        const int height,
        const int num_components,
        const unsigned char* src_data,
        const int quality);

// - tje_encode_to_file_at_quality -
//
// Usage:
//  Takes bitmap data and writes a JPEG-encoded image to disk.
//
//  PARAMETERS
//      dest_path:          filename to which we will write. e.g. "out.jpg"
//      quality:            3: Highest. Compression varies wildly (between 1/3 and 1/20).
//                          2: Very good quality. About 1/2 the size of 3.
//                          1: Noticeable. About 1/6 the size of 3, or 1/3 the size of 2.
//      width, height:      image size in pixels
//      num_components:     3 is RGB. 4 is RGBA. Those are the only supported values
//      src_data:           pointer to the pixel data.
//
//  RETURN:
//      0 on error. 1 on success.

    int tje_encode_to_ctx_at_quality(
        void * ctx,
        const int quality,
        const int width,
        const int height,
        const int num_components,
        const unsigned char* src_data);

// - tje_encode_with_func -
//
// Usage
//  Same as tje_encode_to_file_at_quality, but it takes a callback that knows
//  how to handle (or ignore) `context`. The callback receives an array `data`
//  of `size` bytes, which can be written directly to a file. There is no need
//  to free the data.

    typedef void tje_write_func(void* context, void* data, int size);

    int tje_encode_with_func(tje_write_func* func,
                             void* context,
                             const int quality,
                             const int width,
                             const int height,
                             const int num_components,
                             const unsigned char* src_data);

#endif // TJE_HEADER_GUARD



// Implementation: In exactly one of the source files of your application,
// define TJE_IMPLEMENTATION and include tiny_jpeg.h


    typedef struct {
        uint8_t * data;
        int max;
        int   dp;
    } stream_mgr_t;



#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif


#ifdef __cplusplus
}  // extern C
#endif

