// Single Crypto++ include point for the lab.
#pragma once

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#endif

#include <cryptopp/aes.h>
#include <cryptopp/base64.h>
#include <cryptopp/ccm.h>
#include <cryptopp/cryptlib.h>
#include <cryptopp/files.h>
#include <cryptopp/filters.h>
#include <cryptopp/gcm.h>
#include <cryptopp/hex.h>
#include <cryptopp/modes.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>
#include <cryptopp/xts.h>
