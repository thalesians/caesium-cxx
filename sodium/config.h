/**
 * Copyright (c) 2012-2014, Stephen Blackheath and Anthony Jones
 * Released under a BSD3 licence.
 *
 * C++ implementation courtesy of International Telematics Ltd.
 */
#ifndef _SODIUM_CONFIG_H_
#define _SODIUM_CONFIG_H_

#include <limits.h>  // for __WORDSIZE
#if defined(SODIUM_EXTRA_INCLUDE)
#include SODIUM_EXTRA_INCLUDE
#endif

#if __WORDSIZE == 32
#define SODIUM_STRONG_BITS 1
#define SODIUM_STREAM_BITS  14
#define SODIUM_NODE_BITS   14
#define SODIUM_CONSERVE_MEMORY
#elif __WORDSIZE == 64
#define SODIUM_STRONG_BITS 1
#define SODIUM_STREAM_BITS  31
#define SODIUM_NODE_BITS   31
#define SODIUM_CONSERVE_MEMORY
#endif
#define SODIUM_SHARED_PTR   std::shared_ptr
#define SODIUM_MAKE_SHARED  std::make_shared
#define SODIUM_WEAK_PTR     std::weak_ptr
#define SODIUM_TUPLE        std::tuple
#define SODIUM_MAKE_TUPLE   std::make_tuple
#define SODIUM_TUPLE_GET    std::get
#define SODIUM_FORWARD_LIST std::forward_list
#ifndef SODIUM_THROW
#define SODIUM_THROW(text)  throw std::runtime_error(text)
#endif

#endif
