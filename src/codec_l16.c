/*
 *  MAST: Multicast Audio Streaming Toolkit
 *
 *  Copyright (C) 2003-2007 Nicholas J. Humfrey <njh@ecs.soton.ac.uk>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  $Id$
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <netinet/in.h>
#include "codecs.h"
#include "mast.h"



// Calculate the number of samples per packet
static int mast_samples_per_packet_l16( mast_codec_t *codec, int max_bytes)
{
	int bytes_per_frame = sizeof(int16_t) * codec->channels;
	int bytes_per_unit = SAMPLES_PER_UNIT * bytes_per_frame;
	MAST_DEBUG("L16 bytes per frame = %d", bytes_per_frame);
	MAST_DEBUG("L16 bytes per unit = %d", bytes_per_unit);
	return (max_bytes / bytes_per_unit) * SAMPLES_PER_UNIT;
}


// Encode a packet's payload
static u_int32_t mast_encode_l16(
		mast_codec_t* codec,
		u_int32_t inputsize, 	// input size in frames
		int16_t *input,
		u_int32_t outputsize,	// output size in bytes
		u_int8_t *output)
{
	int input_samples = inputsize * codec->channels;
	int input_bytes = (input_samples * sizeof(int16_t));
	int16_t *output16 = (int16_t*)output;
	register int i;

	if (outputsize < input_bytes) {
		MAST_ERROR("encode_l16: output buffer isn't big enough");
		return 0;
	}

	for(i=0; i< input_samples; i++) {
		output16[i] = htons(input[i]);
	}

	return input_bytes;
}


// Decode a packet's payload
static u_int32_t mast_decode_l16(
		mast_codec_t* codec,
		u_int32_t inputsize,		// input size in bytes
		u_int8_t  *input,
		u_int32_t outputsize, 	// output size in samples
		int16_t  *output)
{
	register int num_samples = (inputsize/2);
	int16_t *input16 = (int16_t*)input;
	register int i;

	if (outputsize < inputsize) {
		MAST_ERROR("decode_l16: output buffer isn't big enough");
		return 0;
	}

	// Convert for network byte order, to machine byte order
	for(i=0; i< num_samples; i++) {
		output[i] = ntohs(input16[i]);
	}

	return num_samples/codec->channels;
}


// Initialise the L16 codec
int mast_init_l16( mast_codec_t* codec ) {

	// Set the callbacks
	codec->samples_per_packet = mast_samples_per_packet_l16;
	codec->encode_packet = mast_encode_l16;
	codec->decode_packet = mast_decode_l16;

	// Success
	return 0;
}



