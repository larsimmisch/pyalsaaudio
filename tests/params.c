/*
 * hw_params.c - print hardware capabilities
 *
 * compile with: gcc -o hw_params hw_params.c -lasound
 */

#include <stdio.h>
#include <alsa/asoundlib.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof *(a))

static const snd_pcm_access_t accesses[] = {
		SND_PCM_ACCESS_MMAP_INTERLEAVED,
		SND_PCM_ACCESS_MMAP_NONINTERLEAVED,
		SND_PCM_ACCESS_MMAP_COMPLEX,
		SND_PCM_ACCESS_RW_INTERLEAVED,
		SND_PCM_ACCESS_RW_NONINTERLEAVED,
};

static const snd_pcm_format_t formats[] = {
		SND_PCM_FORMAT_S8,
		SND_PCM_FORMAT_U8,
		SND_PCM_FORMAT_S16_LE,
		SND_PCM_FORMAT_S16_BE,
		SND_PCM_FORMAT_U16_LE,
		SND_PCM_FORMAT_U16_BE,
		SND_PCM_FORMAT_S24_LE,
		SND_PCM_FORMAT_S24_BE,
		SND_PCM_FORMAT_U24_LE,
		SND_PCM_FORMAT_U24_BE,
		SND_PCM_FORMAT_S32_LE,
		SND_PCM_FORMAT_S32_BE,
		SND_PCM_FORMAT_U32_LE,
		SND_PCM_FORMAT_U32_BE,
		SND_PCM_FORMAT_FLOAT_LE,
		SND_PCM_FORMAT_FLOAT_BE,
		SND_PCM_FORMAT_FLOAT64_LE,
		SND_PCM_FORMAT_FLOAT64_BE,
		SND_PCM_FORMAT_IEC958_SUBFRAME_LE,
		SND_PCM_FORMAT_IEC958_SUBFRAME_BE,
		SND_PCM_FORMAT_MU_LAW,
		SND_PCM_FORMAT_A_LAW,
		SND_PCM_FORMAT_IMA_ADPCM,
		SND_PCM_FORMAT_MPEG,
		SND_PCM_FORMAT_GSM,
		SND_PCM_FORMAT_SPECIAL,
		SND_PCM_FORMAT_S24_3LE,
		SND_PCM_FORMAT_S24_3BE,
		SND_PCM_FORMAT_U24_3LE,
		SND_PCM_FORMAT_U24_3BE,
		SND_PCM_FORMAT_S20_3LE,
		SND_PCM_FORMAT_S20_3BE,
		SND_PCM_FORMAT_U20_3LE,
		SND_PCM_FORMAT_U20_3BE,
		SND_PCM_FORMAT_S18_3LE,
		SND_PCM_FORMAT_S18_3BE,
		SND_PCM_FORMAT_U18_3LE,
		SND_PCM_FORMAT_U18_3BE,
};

static const unsigned int rates[] = {
		5512,
		8000,
		11025,
		16000,
		22050,
		32000,
		44100,
		48000,
		64000,
		88200,
		96000,
		176400,
		192000,
};


struct info_t {
	int nChannels;
	int channels[16];
	int nRates;
	int rates[ARRAY_SIZE(formats)];
	int nFormats;
	int formats[ARRAY_SIZE(rates)];
};


int checkPCM(const char *device_name,unsigned direction,struct info_t *info) {

	snd_pcm_t *pcm;
	int err = snd_pcm_open(&pcm, device_name, direction, SND_PCM_NONBLOCK);
	if (err < 0) {
		fprintf(stderr, "cannot open device '%s': %s\n", device_name, snd_strerror(err));
		return 1;
	}

	snd_pcm_hw_params_t *params;
	snd_pcm_hw_params_alloca(&params);
	err = snd_pcm_hw_params_any(pcm, params);
	if (err < 0) {
		fprintf(stderr, "cannot get hardware parameters: %s\n", snd_strerror(err));
		snd_pcm_close(pcm);
		return 1;
	}

	unsigned min,max;
	err = snd_pcm_hw_params_get_channels_min(params, &min);
	if (err < 0) {
		fprintf(stderr, "cannot get minimum channels count: %s\n", snd_strerror(err));
		snd_pcm_close(pcm);
		return 1;
	}
	err = snd_pcm_hw_params_get_channels_max(params, &max);
	if (err < 0) {
		fprintf(stderr, "cannot get maximum channels count: %s\n", snd_strerror(err));
		snd_pcm_close(pcm);
		return 1;
	}

	int nChannels=0;
	for (int i = min; i <= max; ++i) {
		if (!snd_pcm_hw_params_test_channels(pcm, params, i)) {
			info->channels[nChannels++]=i;
		}
	}
	info->nChannels=nChannels;

	int nFormats=0;
	for (int i = 0; i < ARRAY_SIZE(formats); ++i) {
		if (!snd_pcm_hw_params_test_format(pcm, params, formats[i])) {
			info->formats[nFormats++]=formats[i];
		}
	}
	info->nFormats=nFormats;

	int nRates=0;
	for (int i = 0; i < ARRAY_SIZE(rates); ++i) {
		if (!snd_pcm_hw_params_test_rate(pcm, params, rates[i], 0)) {
			info->rates[nRates++]=rates[i];
		}
	}
	info->nRates=nRates;


	snd_pcm_close(pcm);
	return 0;
}

int main(int argc, char *argv[])
{
	const char *device_name = "hw";
	snd_pcm_t *pcm;
	snd_pcm_hw_params_t *hw_params;
	unsigned int i;
	unsigned int min, max;
	int any_rate;
	int err;

	if (argc > 1)
		device_name = argv[1];

	struct info_t info;
	checkPCM(device_name,SND_PCM_STREAM_PLAYBACK,&info);

	printf("Formats:");
	for (int i = 0; i < info.nFormats; ++i) {
		printf(" %s", snd_pcm_format_name(formats[i]));
	}
	putchar('\n');
	printf("Rates:");
	for (int i = 0; i < info.nRates; ++i) {
		printf(" %u", info.rates[i]);
	}
	putchar('\n');

	printf("Channels:");
	for (int i = 0; i < info.nChannels; ++i) {
		printf(" %u", info.channels[i]);
	}
	putchar('\n');
	/*

	err = snd_pcm_open(&pcm, device_name, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	if (err < 0) {
		fprintf(stderr, "cannot open device '%s': %s\n", device_name, snd_strerror(err));
		return 1;
	}

	snd_pcm_hw_params_alloca(&hw_params);
	err = snd_pcm_hw_params_any(pcm, hw_params);
	if (err < 0) {
		fprintf(stderr, "cannot get hardware parameters: %s\n", snd_strerror(err));
		snd_pcm_close(pcm);
		return 1;
	}

	printf("Device: %s (type: %s)\n", device_name, snd_pcm_type_name(snd_pcm_type(pcm)));

	printf("Access types:");
	for (i = 0; i < ARRAY_SIZE(accesses); ++i) {
		if (!snd_pcm_hw_params_test_access(pcm, hw_params, accesses[i]))
			printf(" %s", snd_pcm_access_name(accesses[i]));
	}
	putchar('\n');

	printf("Formats:");
	for (i = 0; i < ARRAY_SIZE(formats); ++i) {
		if (!snd_pcm_hw_params_test_format(pcm, hw_params, formats[i]))
			printf(" %s", snd_pcm_format_name(formats[i]));
	}
	putchar('\n');

	err = snd_pcm_hw_params_get_channels_min(hw_params, &min);
	if (err < 0) {
		fprintf(stderr, "cannot get minimum channels count: %s\n", snd_strerror(err));
		snd_pcm_close(pcm);
		return 1;
	}
	err = snd_pcm_hw_params_get_channels_max(hw_params, &max);
	if (err < 0) {
		fprintf(stderr, "cannot get maximum channels count: %s\n", snd_strerror(err));
		snd_pcm_close(pcm);
		return 1;
	}
	printf("Channels:");
	for (i = min; i <= max; ++i) {
		if (!snd_pcm_hw_params_test_channels(pcm, hw_params, i))
			printf(" %u", i);
	}
	putchar('\n');

	err = snd_pcm_hw_params_get_rate_min(hw_params, &min, NULL);
	if (err < 0) {
		fprintf(stderr, "cannot get minimum rate: %s\n", snd_strerror(err));
		snd_pcm_close(pcm);
		return 1;
	}
	err = snd_pcm_hw_params_get_rate_max(hw_params, &max, NULL);
	if (err < 0) {
		fprintf(stderr, "cannot get maximum rate: %s\n", snd_strerror(err));
		snd_pcm_close(pcm);
		return 1;
	}
	printf("Sample rates:");
	if (min == max)
		printf(" %u", min);
	else if (!snd_pcm_hw_params_test_rate(pcm, hw_params, min + 1, 0))
		printf(" %u-%u", min, max);
	else {
		any_rate = 0;
		for (i = 0; i < ARRAY_SIZE(rates); ++i) {
			if (!snd_pcm_hw_params_test_rate(pcm, hw_params, rates[i], 0)) {
				any_rate = 1;
				printf(" %u", rates[i]);
			}
		}
		if (!any_rate)
			printf(" %u-%u", min, max);
	}
	putchar('\n');

	err = snd_pcm_hw_params_get_period_time_min(hw_params, &min, NULL);
	if (err < 0) {
		fprintf(stderr, "cannot get minimum period time: %s\n", snd_strerror(err));
		snd_pcm_close(pcm);
		return 1;
	}
	err = snd_pcm_hw_params_get_period_time_max(hw_params, &max, NULL);
	if (err < 0) {
		fprintf(stderr, "cannot get maximum period time: %s\n", snd_strerror(err));
		snd_pcm_close(pcm);
		return 1;
	}
	printf("Interrupt interval: %u-%u us\n", min, max);

	err = snd_pcm_hw_params_get_buffer_time_min(hw_params, &min, NULL);
	if (err < 0) {
		fprintf(stderr, "cannot get minimum buffer time: %s\n", snd_strerror(err));
		snd_pcm_close(pcm);
		return 1;
	}
	err = snd_pcm_hw_params_get_buffer_time_max(hw_params, &max, NULL);
	if (err < 0) {
		fprintf(stderr, "cannot get maximum buffer time: %s\n", snd_strerror(err));
		snd_pcm_close(pcm);
		return 1;
	}
	printf("Buffer size: %u-%u us\n", min, max);

	snd_pcm_close(pcm);

	 */
	return 0;
}


