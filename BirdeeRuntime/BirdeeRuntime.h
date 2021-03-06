#pragma once

#include <stdint.h>

struct GenericArray
{
	union {
		struct
		{
			uint32_t sz;
			char cbuf[1];
		}packed;
		struct
		{
			uint32_t sz;
			void* buf[1];
		}unpacked;
	};
};


struct BirdeeString
{
	GenericArray* arr;
	uint32_t sz;
};

struct BirdeeTypeInfo
{
	BirdeeString* name;
};

struct BirdeeRTTIObject
{
	BirdeeTypeInfo* type;
};