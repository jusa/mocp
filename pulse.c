/*
 * MOC - music on console
 * Copyright (C) 2020 Juho Hämäläinen <jusa@hilvi.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <pulse/simple.h>

#define DEBUG

#include "common.h"
#include "server.h"
#include "audio.h"
#include "options.h"
#include "log.h"


static pa_simple *handle = NULL;
static pa_sample_spec sample_spec;

static const struct {
	pa_sample_format_t pulse;
	long format;
} format_list[] = {
#ifdef WORDS_BIGENDIAN
	{ PA_SAMPLE_U8,         SFMT_BE | SFMT_U8       },
	{ PA_SAMPLE_S16BE,      SFMT_BE | SFMT_S16      },
	{ PA_SAMPLE_S24_32BE,   SFMT_BE | SFMT_S32      },
	{ PA_SAMPLE_FLOAT32BE,  SFMT_BE | SFMT_FLOAT    },
#else
	{ PA_SAMPLE_U8,         SFMT_LE | SFMT_U8       },
	{ PA_SAMPLE_S16LE,      SFMT_LE | SFMT_S16      },
	{ PA_SAMPLE_S24_32LE,   SFMT_LE | SFMT_S32      },
	{ PA_SAMPLE_FLOAT32LE,  SFMT_LE | SFMT_FLOAT    },
#endif
};

static pa_sample_format_t format_to_pulse (long format)
{
	pa_sample_format_t result = PA_SAMPLE_INVALID;

	for (size_t ix = 0; ix < ARRAY_SIZE(format_list); ix += 1) {
		if (format_list[ix].format == format) {
			result = format_list[ix].pulse;
			break;
		}
	}

	return result;
}

static long pulse_to_mask ()
{
	long mask = 0;

	for (size_t ix = 0; ix < ARRAY_SIZE(format_list); ix += 1)
		mask |= format_list[ix].format;

	return mask;
}

static int simple_open ()
{
	if (handle)
		return 1;

	handle = pa_simple_new (NULL,
	                        "mocp",
	                        PA_STREAM_PLAYBACK,
	                        NULL,
	                        "Music",
	                        &sample_spec,
	                        NULL,
	                        NULL,
	                        NULL);

	if (handle)
		return 1;

	return 0;
}

static void simple_close ()
{
	if (handle) {
		pa_simple_free (handle);
		handle = NULL;
	}
}

static void pulse_shutdown ()
{
	if (handle)
		pa_simple_drain (handle, NULL);

	simple_close ();
}

static int pulse_init (struct output_driver_caps *caps)
{
	caps->min_channels = 1;
	caps->max_channels = PA_CHANNELS_MAX;
	caps->formats = pulse_to_mask ();

	return 1;
}

static int pulse_open (struct sound_params *sound_params)
{
	sample_spec.format = format_to_pulse (sound_params->fmt);
	if (sample_spec.format == PA_SAMPLE_INVALID) {
		logit ("Invalid format.");
		return 0;
	}
	sample_spec.channels = sound_params->channels;
	sample_spec.rate = sound_params->rate;

	if (simple_open ())
		return 1;

	return 0;
}

static void pulse_close ()
{
	simple_close ();
}

static int pulse_play (const char *buff, const size_t size)
{
	if (!simple_open ())
		return size;

	pa_simple_write (handle, buff, size, NULL);

	return size;
}

static int pulse_read_mixer ()
{
	return 100;
}

static void pulse_set_mixer (int vol)
{
	(void) vol;
}

static int pulse_get_buff_fill ()
{
	return 0;
}

static int pulse_reset ()
{
	if (!handle)
		return 1;

	pa_simple_drain(handle, NULL);
	simple_close();
	return 1;
}

static int pulse_get_rate ()
{
	return sample_spec.rate;
}

static void pulse_toggle_mixer_channel ()
{
}

static char *pulse_get_mixer_channel_name ()
{
	return xstrdup("PulseAudio");
}

void pulse_funcs (struct hw_funcs *funcs)
{
	funcs->init = pulse_init;
	funcs->shutdown = pulse_shutdown;
	funcs->open = pulse_open;
	funcs->close = pulse_close;
	funcs->play = pulse_play;
	funcs->read_mixer = pulse_read_mixer;
	funcs->set_mixer = pulse_set_mixer;
	funcs->get_buff_fill = pulse_get_buff_fill;
	funcs->reset = pulse_reset;
	funcs->get_rate = pulse_get_rate;
	funcs->toggle_mixer_channel = pulse_toggle_mixer_channel;
	funcs->get_mixer_channel_name = pulse_get_mixer_channel_name;
}
