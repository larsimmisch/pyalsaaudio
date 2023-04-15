/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: t -*- */

/*
 * alsaaudio -- Python interface to ALSA (Advanced Linux Sound Architecture).
 *			  The standard audio API for Linux since kernel 2.6
 *
 * Contributed by Unispeed A/S (http://www.unispeed.com)
 * Author: Casper Wilstup (cwi@aves.dk) and Lars Immisch (lars@ibp.de)
 *
 * License: Python Software Foundation License
 *
 */

#include "Python.h"
#if PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION < 6
#include "stringobject.h"
#define PyUnicode_AS_DATA PyString_FromString
#define PyUnicode_FromString PyString_FromString
#define PyUnicode_Check PyString_Check
#define PyLong_Check PyInt_Check
#define PyLong_AS_LONG PyInt_AS_LONG
#endif

#if PY_MAJOR_VERSION < 3
	#define PyLong_FromLong PyInt_FromLong
#endif

#include <alsa/asoundlib.h>
#include <alsa/version.h>
#include <stdio.h>
#include <stdbool.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof *(a))
static const snd_pcm_format_t ALSAFormats[] = {
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
	SND_PCM_FORMAT_U18_3BE
};

static const unsigned ALSARates[] = {
	4000,
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
	352800,
	384000

};

typedef enum volume_units_t {
	VOLUME_UNITS_PERCENTAGE,
	VOLUME_UNITS_RAW,
	VOLUME_UNITS_DB,
} volume_units_t;

typedef struct {
	PyObject_HEAD;
	long pcmtype;
	int pcmmode;
	char *cardname;

	snd_pcm_t *handle;

	// Configurable parameters
	unsigned int channels;
	unsigned int rate;
	snd_pcm_format_t format;
	unsigned int periods;
	snd_pcm_uframes_t periodsize;
	int framesize;

} alsapcm_t;

typedef struct {
	PyObject_HEAD;

	/* Mixer identification */
	char *cardname;
	char *controlname;
	int controlid;

	/* Capabilities */
	unsigned int volume_cap;
	unsigned int switch_cap;
	unsigned int pchannels;
	unsigned int cchannels;

	/* min and max values for playback and capture volumes */
	long pmin, pmax;
	long cmin, cmax;
	/* min and max values for playback and capture volumes, in dB * 100 as
	 * reported by ALSA */
	long pmin_dB, pmax_dB;
	long cmin_dB, cmax_dB;

	snd_mixer_t *handle;
} alsamixer_t;

/******************************************/
/* PCM object wrapper				   */
/******************************************/

static PyTypeObject ALSAPCMType;
static PyObject *ALSAAudioError;

static long
get_pcmtype(PyObject *obj)
{
	if (!obj || (obj == Py_None)) {
		return SND_PCM_STREAM_PLAYBACK;
	}

#if PY_MAJOR_VERSION > 2
	if (PyLong_Check(obj)) {
		long pcmtype = PyLong_AS_LONG(obj);
		if (pcmtype == SND_PCM_STREAM_PLAYBACK ||
			pcmtype == SND_PCM_STREAM_CAPTURE) {
			return pcmtype;
		}
	}
#else
	if (PyInt_Check(obj)) {
		long pcmtype = PyInt_AS_LONG(obj);
		if (pcmtype == SND_PCM_STREAM_PLAYBACK ||
			pcmtype == SND_PCM_STREAM_CAPTURE) {
			return pcmtype;
		}
	}
#endif

	if (PyUnicode_Check(obj)) {
#if PY_MAJOR_VERSION > 2
		if (PyUnicode_CompareWithASCIIString(obj, "playback") == 0)
			return SND_PCM_STREAM_PLAYBACK;
		else if (PyUnicode_CompareWithASCIIString(obj, "capture") == 0)
			return SND_PCM_STREAM_CAPTURE;
#else
		const char *dirstr = PyUnicode_AS_DATA(obj);
		if (strcasecmp(dirstr, "playback")==0)
			return SND_PCM_STREAM_PLAYBACK;
		else if (strcasecmp(dirstr, "capture")==0)
			return SND_PCM_STREAM_CAPTURE;
#endif
	}

	PyErr_SetString(ALSAAudioError, "PCM type must be PCM_PLAYBACK (0) "
					"or PCM_CAPTURE (1)");
	return -1;
}

static bool is_value_volume_unit(long unit)
{
	if (unit == VOLUME_UNITS_PERCENTAGE ||
	    unit == VOLUME_UNITS_RAW ||
	    unit == VOLUME_UNITS_DB) {
		return true;
	}
	return false;
}

static PyObject *
alsacard_list(PyObject *self, PyObject *args)
{
	int rc;
	int card = -1;
	snd_ctl_card_info_t *info;
	snd_ctl_t *handle;
	PyObject *result = NULL;

	if (!PyArg_ParseTuple(args,":cards"))
		return NULL;

	snd_ctl_card_info_alloca(&info);
	result = PyList_New(0);

	for (rc = snd_card_next(&card); !rc && (card >= 0);
		 rc = snd_card_next(&card))
	{
		char name[64];
		int err;
		PyObject *item;

		/* One would be tempted to think that snd_card_get_name returns a name
		   that is actually meaningful for any further operations.

		   Not in ALSA land. Here we need the id, not the name */

		sprintf(name, "hw:%d", card);
		if ((err = snd_ctl_open(&handle, name, 0)) < 0) {
			PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(err), name);
			return NULL;
		}
		if ((err = snd_ctl_card_info(handle, info)) < 0) {
			PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(err), name);
			snd_ctl_close(handle);
			Py_DECREF(result);
			return NULL;
		}

		item = PyUnicode_FromString(snd_ctl_card_info_get_id(info));
		PyList_Append(result, item);
		Py_DECREF(item);

		snd_ctl_close(handle);
	}

	return result;
}

static PyObject *
alsacard_list_indexes(PyObject *self, PyObject *args)
{
	int rc;
	int card = -1;
	PyObject *result = NULL;

	if (!PyArg_ParseTuple(args,":card_indexes"))
		return NULL;

	result = PyList_New(0);

	for (rc = snd_card_next(&card); !rc && (card >= 0);
		 rc = snd_card_next(&card))
	{
		PyObject *item = PyLong_FromLong(card);

		PyList_Append(result, item);
		Py_DECREF(item);
	}

	return result;
}

static PyObject *
alsacard_name(PyObject *self, PyObject *args)
{
	int err, card;
	PyObject *result = NULL;
	char *name = NULL, *longname = NULL;

	if (!PyArg_ParseTuple(args,"i:card_name", &card))
		return NULL;

	err = snd_card_get_name(card, &name);
	if (err < 0) {
		PyErr_Format(ALSAAudioError, "%s [%d]", snd_strerror(err), card);
		goto exit;
	}

	err = snd_card_get_longname(card, &longname);
	if (err < 0) {
		PyErr_Format(ALSAAudioError, "%s [%d]", snd_strerror(err), card);
		goto exit;
	}

	result = PyTuple_New(2);
	PyTuple_SetItem(result, 0, PyUnicode_FromString(name));
	PyTuple_SetItem(result, 1, PyUnicode_FromString(longname));

exit:
	free(name);
	free(longname);

	return result;
}

static PyObject *
alsapcm_list(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *pcmtypeobj = NULL;
	long pcmtype;
	PyObject *result = NULL;
	PyObject *item;
	void **hints, **n;
	char *name, *io;
	const char *filter;

	char *kw[] = { "pcmtype", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O:pcms", kw, &pcmtypeobj)) {
		return NULL;
	}

	pcmtype = get_pcmtype(pcmtypeobj);
	if (pcmtype < 0) {
		return NULL;
	}

	result = PyList_New(0);

	if (snd_device_name_hint(-1, "pcm", &hints) < 0)
		return result;

	n = hints;
	filter = pcmtype == SND_PCM_STREAM_CAPTURE ? "Input" : "Output";
	while (*n != NULL) {
		name = snd_device_name_get_hint(*n, "NAME");
		io = snd_device_name_get_hint(*n, "IOID");
		if (io != NULL && strcmp(io, filter) != 0)
			goto __end;

		item = PyUnicode_FromString(name);
		PyList_Append(result, item);
		Py_DECREF(item);

	__end:
		if (name != NULL)
			free(name);
		if (io != NULL)
			free(io);
		n++;
	}
	snd_device_name_free_hint(hints);

	return result;
}

static int alsapcm_setup(alsapcm_t *self)
{
	int res,dir;
	snd_pcm_hw_params_t *hwparams;

	/* Allocate a hwparam structure on the stack,
	   and fill it with configuration space */
	snd_pcm_hw_params_alloca(&hwparams);
	res = snd_pcm_hw_params_any(self->handle, hwparams);
	if (res < 0) {
		return res;
	}

	/* Fill it with default values.

	   We don't care if any of this fails - we'll read the actual values
	   back out.
	 */
	snd_pcm_hw_params_set_access(self->handle, hwparams,
								 SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(self->handle, hwparams, self->format);
	snd_pcm_hw_params_set_channels(self->handle, hwparams,
								   self->channels);

	dir = 0;
	snd_pcm_hw_params_set_rate_near(self->handle, hwparams, &self->rate, &dir);
	snd_pcm_hw_params_set_period_size_near(self->handle, hwparams,
										   &self->periodsize, &dir);
	snd_pcm_hw_params_set_periods_near(self->handle, hwparams, &self->periods, &dir);

	/* Write it to the device */
	res = snd_pcm_hw_params(self->handle, hwparams);

	/* Query current settings. These may differ from the requested values,
	   which should therefore be sync'ed with actual values */
	snd_pcm_hw_params_current(self->handle, hwparams);

	snd_pcm_hw_params_get_format(hwparams, &self->format);
	snd_pcm_hw_params_get_channels(hwparams, &self->channels);
	snd_pcm_hw_params_get_rate(hwparams, &self->rate, &dir);
	snd_pcm_hw_params_get_period_size(hwparams, &self->periodsize, &dir);
	snd_pcm_hw_params_get_periods(hwparams, &self->periods, &dir);

	self->framesize = self->channels * snd_pcm_hw_params_get_sbits(hwparams)/8;

	return res;
}

static PyObject *
alsapcm_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	int res;
	alsapcm_t *self;
	PyObject *pcmtypeobj = NULL;
	long pcmtype;
	int pcmmode = 0;
	char *device = "default";
	char *card = NULL;
	int cardidx = -1;
	char hw_device[128];
	int rate = 44100;
	int channels = 2;
	int format = SND_PCM_FORMAT_S16_LE;
	int periods = 4;
	int periodsize = 32;

	char *kw[] = { "type", "mode", "device", "cardindex", "card",
				   "rate", "channels", "format", "periodsize", "periods", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|Oisiziiiii", kw,
									 &pcmtypeobj, &pcmmode, &device, &cardidx, &card,
									 &rate, &channels, &format, &periodsize, &periods))
		return NULL;

	if (cardidx >= 0) {
		if (cardidx < 32) {
			snprintf(hw_device, sizeof(hw_device), "hw:%d", cardidx);
			device = hw_device;
		}
		else {
			PyErr_Format(ALSAAudioError, "Invalid card number %d", cardidx);
			return NULL;
		}
	}
	else if (card) {
		// The card kw argument is deprecated
		PyErr_WarnEx(PyExc_DeprecationWarning,
					 "The `card` keyword argument is deprecated. "
					 "Please use `device` instead", 1);

		// If we find a colon, we assume it is a real ALSA cardname
		if (strchr(card, ':')) {
			device = card;
		}

		snprintf(hw_device, sizeof(hw_device), "default:CARD=%s", card);
		device = hw_device;
	}

	pcmtype = get_pcmtype(pcmtypeobj);
	if (pcmtype < 0) {
		return NULL;
	}

	if (pcmmode < 0 || pcmmode > SND_PCM_ASYNC) {
		PyErr_SetString(ALSAAudioError, "Invalid PCM mode");
		return NULL;
	}

	if (!(self = (alsapcm_t *)PyObject_New(alsapcm_t, &ALSAPCMType)))
		return NULL;

	self->handle = 0;
	self->pcmtype = pcmtype;
	self->pcmmode = pcmmode;
	self->channels = channels;
	self->rate = rate;
	self->format = format;
	self->periods = periods;
	self->periodsize = periodsize;

	res = snd_pcm_open(&(self->handle), device, self->pcmtype,
					   self->pcmmode);

	if (res >= 0) {
		res = alsapcm_setup(self);
	}

	if (res >= 0) {
		self->cardname = strdup(device);
	}
	else {
		if (self->handle)
		{
			snd_pcm_close(self->handle);
			self->handle = 0;
		}
		PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(res), device);
		return NULL;
	}
	return (PyObject *)self;
}

static void alsapcm_dealloc(alsapcm_t *self)
{
	if (self->handle)
		snd_pcm_close(self->handle);
	free(self->cardname);
	PyObject_Del(self);
}

static PyObject *
alsapcm_close(alsapcm_t *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args,":close"))
		return NULL;

	if (self->handle)
	{
		if (self->pcmtype == SND_PCM_STREAM_PLAYBACK) {
			Py_BEGIN_ALLOW_THREADS
			snd_pcm_drain(self->handle);
			Py_END_ALLOW_THREADS
		}
		snd_pcm_close(self->handle);

		self->handle = 0;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
alsapcm_dumpinfo(alsapcm_t *self, PyObject *args)
{
	unsigned int val,val2;
	snd_pcm_access_t acc;
	snd_pcm_format_t fmt;
	snd_pcm_subformat_t subfmt;
	int dir;
	snd_pcm_uframes_t frames;
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_hw_params_alloca(&hwparams);
	snd_pcm_hw_params_current(self->handle,hwparams);

	if (!PyArg_ParseTuple(args,":dumpinfo"))
		return NULL;

	if (!self->handle) {
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	printf("PCM handle name = '%s'\n", snd_pcm_name(self->handle));
	printf("PCM state = %s\n",
		   snd_pcm_state_name(snd_pcm_state(self->handle)));

	snd_pcm_hw_params_get_access(hwparams, &acc);
	printf("access type = %s\n", snd_pcm_access_name(acc));

	snd_pcm_hw_params_get_format(hwparams, &fmt);
	printf("format = '%s' (%s)\n",
		   snd_pcm_format_name(fmt),
		   snd_pcm_format_description(fmt));

	snd_pcm_hw_params_get_subformat(hwparams, &subfmt);
	printf("subformat = '%s' (%s)\n",
		   snd_pcm_subformat_name(subfmt),
		   snd_pcm_subformat_description(subfmt));

	snd_pcm_hw_params_get_channels(hwparams, &val);
	printf("channels = %d\n", val);

	snd_pcm_hw_params_get_rate(hwparams, &val, &dir);
	printf("rate = %d bps\n", val);

	snd_pcm_hw_params_get_period_time(hwparams, &val, &dir);
	printf("period time = %d us\n", val);

	snd_pcm_hw_params_get_period_size(hwparams, &frames, &dir);
	printf("period size = %d frames\n", (int)frames);

	snd_pcm_hw_params_get_buffer_time(hwparams, &val, &dir);
	printf("buffer time = %d us\n", val);

	snd_pcm_hw_params_get_buffer_size(hwparams, &frames);
	printf("buffer size = %d frames\n", (int)frames);

	snd_pcm_hw_params_get_periods(hwparams, &val, &dir);
	printf("periods per buffer = %d\n", val);

	snd_pcm_hw_params_get_rate_numden(hwparams, &val, &val2);
	printf("exact rate = %d/%d bps\n", val, val2);

	val = snd_pcm_hw_params_get_sbits(hwparams);
	printf("significant bits = %d\n", val);

	val = snd_pcm_hw_params_is_batch(hwparams);
	printf("is batch = %d\n", val);

	val = snd_pcm_hw_params_is_block_transfer(hwparams);
	printf("is block transfer = %d\n", val);

	val = snd_pcm_hw_params_is_double(hwparams);
	printf("is double = %d\n", val);

	val = snd_pcm_hw_params_is_half_duplex(hwparams);
	printf("is half duplex = %d\n", val);

	val = snd_pcm_hw_params_is_joint_duplex(hwparams);
	printf("is joint duplex = %d\n", val);

	val = snd_pcm_hw_params_can_overrange(hwparams);
	printf("can overrange = %d\n", val);

	val = snd_pcm_hw_params_can_mmap_sample_resolution(hwparams);
	printf("can mmap = %d\n", val);

	val = snd_pcm_hw_params_can_pause(hwparams);
	printf("can pause = %d\n", val);

	val = snd_pcm_hw_params_can_resume(hwparams);
	printf("can resume = %d\n", val);

	val = snd_pcm_hw_params_can_sync_start(hwparams);
	printf("can sync start = %d\n", val);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
alsapcm_info(alsapcm_t *self, PyObject *args)
{
	PyObject *info;
	PyObject *value;

	unsigned int val,val2;
	snd_pcm_access_t acc;
	snd_pcm_format_t fmt;
	snd_pcm_subformat_t subfmt;
	int dir;
	snd_pcm_uframes_t frames;
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_hw_params_alloca(&hwparams);
	snd_pcm_hw_params_current(self->handle,hwparams);

	snd_pcm_info_t * pcm_info;
	snd_pcm_info_alloca(&pcm_info);

	if (!PyArg_ParseTuple(args,":info"))
		return NULL;

	if (!self->handle) {
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	info = PyDict_New();

	value=PyUnicode_FromString(snd_pcm_name(self->handle));
	PyDict_SetItemString(info,"name",value);
	Py_DECREF(value);

	snd_pcm_info(self->handle, pcm_info);

	value = PyLong_FromLong((long) snd_pcm_info_get_card(pcm_info));
	PyDict_SetItemString(info,"card_no",value);
	Py_DECREF(value);

	value = PyLong_FromUnsignedLong((unsigned long) snd_pcm_info_get_device(pcm_info));
	PyDict_SetItemString(info,"device_no",value);
	Py_DECREF(value);

	value = PyLong_FromUnsignedLong((unsigned long) snd_pcm_info_get_subdevice(pcm_info));
	PyDict_SetItemString(info,"subdevice_no",value);
	Py_DECREF(value);

	value=PyUnicode_FromString(snd_pcm_state_name(snd_pcm_state(self->handle)));
	PyDict_SetItemString(info,"state",value);
	Py_DECREF(value);

	snd_pcm_hw_params_get_access(hwparams, &acc);
	value=PyUnicode_FromString(snd_pcm_access_name(acc));
	PyDict_SetItemString(info,"access_type",value);
	Py_DECREF(value);

	value = PyLong_FromUnsignedLong((unsigned long) self->pcmtype);
	PyDict_SetItemString(info," (call value) type",value);
	Py_DECREF(value);
	value=PyUnicode_FromString(snd_pcm_stream_name((snd_pcm_stream_t) self->pcmtype));
	PyDict_SetItemString(info," (call value) type_name",value);
	Py_DECREF(value);

	value = PyLong_FromUnsignedLong((unsigned long) self->pcmmode);
	PyDict_SetItemString(info," (call value) mode",value);
	Py_DECREF(value);
	switch(self->pcmmode){
		case 0:
			value = PyUnicode_FromString("PCM_NORMAL");
			break;
		case SND_PCM_NONBLOCK:
			value = PyUnicode_FromString("PCM_NONBLOCK");
			break;
		case SND_PCM_ASYNC:
			value = PyUnicode_FromString("PCM_ASYNC");
			break;
	}
	PyDict_SetItemString(info," (call value) mode_name",value);
	Py_DECREF(value);

	snd_pcm_hw_params_get_format(hwparams, &fmt);
	value=PyLong_FromUnsignedLong((unsigned long)fmt);
	PyDict_SetItemString(info,"format",value);
	Py_DECREF(value);
	value=PyUnicode_FromString(snd_pcm_format_name(fmt));
	PyDict_SetItemString(info,"format_name",value);
	Py_DECREF(value);
	value=PyUnicode_FromString(snd_pcm_format_description(fmt));
	PyDict_SetItemString(info,"format_description",value);
	Py_DECREF(value);


	snd_pcm_hw_params_get_subformat(hwparams, &subfmt);
	value=PyUnicode_FromString(snd_pcm_subformat_name(subfmt));
	PyDict_SetItemString(info,"subformat_name",value);
	Py_DECREF(value);
	value=PyUnicode_FromString(snd_pcm_subformat_description(subfmt));
	PyDict_SetItemString(info,"subformat_description",value);
	Py_DECREF(value);

	snd_pcm_hw_params_get_channels(hwparams, &val);
	value=PyLong_FromUnsignedLong((unsigned long) val);
	PyDict_SetItemString(info,"channels", value);
	Py_DECREF(value);

	snd_pcm_hw_params_get_rate(hwparams, &val, &dir);
	value=PyLong_FromUnsignedLong((unsigned long) val);
	PyDict_SetItemString(info,"rate", value);
	Py_DECREF(value);

	snd_pcm_hw_params_get_period_time(hwparams, &val, &dir);
	value=PyLong_FromUnsignedLong((unsigned long) val);
	PyDict_SetItemString(info,"period_time", value);
	Py_DECREF(value);

	snd_pcm_hw_params_get_period_size(hwparams, &frames, &dir);
	value=PyLong_FromUnsignedLong((unsigned long) frames);
	PyDict_SetItemString(info,"period_size", value);
	Py_DECREF(value);

	snd_pcm_hw_params_get_buffer_time(hwparams, &val, &dir);
	value=PyLong_FromUnsignedLong((unsigned long) val);
	PyDict_SetItemString(info,"buffer_time", value);
	Py_DECREF(value);

	snd_pcm_hw_params_get_buffer_size(hwparams, &frames);
	value=PyLong_FromUnsignedLong((unsigned long) frames);
	PyDict_SetItemString(info,"buffer_size", value);
	Py_DECREF(value);

	snd_pcm_hw_params_get_periods(hwparams, &val, &dir);
	value=PyLong_FromUnsignedLong((unsigned long) val);
	PyDict_SetItemString(info,"periods", value);
	Py_DECREF(value);

	snd_pcm_hw_params_get_rate_numden(hwparams, &val, &val2);
	value=PyTuple_Pack(2,PyLong_FromUnsignedLong((unsigned long) val) \
				,PyLong_FromUnsignedLong((unsigned long) val2));
	PyDict_SetItemString(info,"rate_numden", value);
	Py_DECREF(value);

	val = snd_pcm_hw_params_get_sbits(hwparams);
	value=PyLong_FromUnsignedLong((unsigned long) val);
	PyDict_SetItemString(info,"significant_bits", value);
	Py_DECREF(value);

	val = snd_pcm_hw_params_is_batch(hwparams);
	value=PyBool_FromLong((unsigned long) val);
	PyDict_SetItemString(info,"is_batch", value);
	Py_DECREF(value);

	val = snd_pcm_hw_params_is_block_transfer(hwparams);
	value=PyBool_FromLong((unsigned long) val);
	PyDict_SetItemString(info,"is_block_transfer", value);
	Py_DECREF(value);

	val = snd_pcm_hw_params_is_double(hwparams);
	value=PyBool_FromLong((unsigned long) val);
	PyDict_SetItemString(info,"is_double", value);
	Py_DECREF(value);

	val = snd_pcm_hw_params_is_half_duplex(hwparams);
	value=PyBool_FromLong((unsigned long) val);
	PyDict_SetItemString(info,"is_half_duplex", value);
	Py_DECREF(value);

	val = snd_pcm_hw_params_is_joint_duplex(hwparams);
	value=PyBool_FromLong((unsigned long) val);
	PyDict_SetItemString(info,"is_joint_duplex", value);
	Py_DECREF(value);

	val = snd_pcm_hw_params_can_overrange(hwparams);
	value=PyBool_FromLong((unsigned long) val);
	PyDict_SetItemString(info,"can_overrange", value);
	Py_DECREF(value);

	val = snd_pcm_hw_params_can_mmap_sample_resolution(hwparams);
	value=PyBool_FromLong((unsigned long) val);
	PyDict_SetItemString(info,"can_mmap_sample_resolution", value);
	Py_DECREF(value);

	val = snd_pcm_hw_params_can_pause(hwparams);
	value=PyBool_FromLong((unsigned long) val);
	PyDict_SetItemString(info,"can_pause", value);
	Py_DECREF(value);

	val = snd_pcm_hw_params_can_resume(hwparams);
	value=PyBool_FromLong((unsigned long) val);
	PyDict_SetItemString(info,"can_resume", value);
	Py_DECREF(value);

	val = snd_pcm_hw_params_can_sync_start(hwparams);
	value=PyBool_FromLong((unsigned long) val);
	PyDict_SetItemString(info,"can_sync_start", value);
	Py_DECREF(value);

	return info;
}

static PyObject *
alsa_asoundlib_version(PyObject * module, PyObject *args)
{
	if (!PyArg_ParseTuple(args,":asoundlib_version"))
		return NULL;

	return PyUnicode_FromString(snd_asoundlib_version());
}

static PyObject *
alsapcm_state(alsapcm_t *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args,":state"))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	return PyLong_FromUnsignedLong((unsigned long) snd_pcm_state(self->handle));
}

static PyObject *
alsapcm_htimestamp(alsapcm_t *self, PyObject *args)
{
	snd_htimestamp_t tstamp;
	snd_pcm_uframes_t avail;
	PyObject *result = NULL;

	if (!self->handle) {
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	snd_pcm_htimestamp(self->handle , &avail, &tstamp);

	result = PyTuple_New(3);
	PyTuple_SetItem(result, 0, PyLong_FromLongLong(tstamp.tv_sec));
	PyTuple_SetItem(result, 1, PyLong_FromLong(tstamp.tv_nsec));
	PyTuple_SetItem(result, 2, PyLong_FromLong(avail));

	return result;
}

static PyObject *
alsapcm_set_tstamp_mode(alsapcm_t *self, PyObject *args)
{
	snd_pcm_tstamp_t mode = SND_PCM_TSTAMP_ENABLE;
	int err;

	if (!PyArg_ParseTuple(args,"|i:set_tstamp_mode", &mode))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	snd_pcm_sw_params_t* swParams;
	snd_pcm_sw_params_alloca( &swParams);

	snd_pcm_sw_params_current(self->handle, swParams);

	snd_pcm_sw_params_set_tstamp_mode(self->handle, swParams, mode);

	err = snd_pcm_sw_params(self->handle, swParams);

	if (err < 0) {
		PyErr_SetString(PyExc_RuntimeError, "Unable to set pcm tstamp mode!");
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
alsapcm_get_tstamp_mode(alsapcm_t *self, PyObject *args)
{
	snd_pcm_tstamp_t mode;
	int err;

	if (!PyArg_ParseTuple(args,":get_tstamp_mode"))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	snd_pcm_sw_params_t* swParams;
	snd_pcm_sw_params_alloca( &swParams);

	snd_pcm_sw_params_current(self->handle, swParams);

	err = snd_pcm_sw_params_get_tstamp_mode(swParams, &mode);

	if (err < 0) {
		PyErr_SetString(PyExc_RuntimeError, "Unable to get pcm tstamp mode!");
		return NULL;
	}

	return PyLong_FromUnsignedLong((unsigned long) mode);
}

static PyObject *
alsapcm_set_tstamp_type(alsapcm_t *self, PyObject *args)
{
	snd_pcm_tstamp_type_t type = SND_PCM_TSTAMP_TYPE_GETTIMEOFDAY;
	int err;

	if (!PyArg_ParseTuple(args,"|i:set_tstamp_type", &type))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	snd_pcm_sw_params_t* swParams;
	snd_pcm_sw_params_alloca( &swParams);

	snd_pcm_sw_params_current(self->handle, swParams);

	snd_pcm_sw_params_set_tstamp_type(self->handle, swParams, type);

	err = snd_pcm_sw_params(self->handle, swParams);

	if (err < 0) {
		PyErr_SetString(PyExc_RuntimeError, "Unable to set pcm tstamp type!");
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
alsapcm_get_tstamp_type(alsapcm_t *self, PyObject *args)
{
	snd_pcm_tstamp_type_t type;
	int err;

	if (!PyArg_ParseTuple(args,":get_tstamp_type"))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	snd_pcm_sw_params_t* swParams;
	snd_pcm_sw_params_alloca( &swParams);

	snd_pcm_sw_params_current(self->handle, swParams);

	err = snd_pcm_sw_params_get_tstamp_type(swParams, &type);

	if (err < 0) {
		PyErr_SetString(PyExc_RuntimeError, "Unable to get pcm tstamp type!");
		return NULL;
	}

	return PyLong_FromUnsignedLong((unsigned long) type);
}


// auxiliary function

static PyObject *
alsapcm_getformats(alsapcm_t *self, PyObject *args)
{
	snd_pcm_t *pcm = self->handle;
	if (!pcm) {
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	snd_pcm_hw_params_t *params;
	snd_pcm_hw_params_alloca(&params);
	int err = snd_pcm_hw_params_any(pcm, params);
	if (err < 0) {
		PyErr_SetString(ALSAAudioError, "Cannot get hardware parameters");
		return NULL;
	}

	PyObject *fmts = PyDict_New();
	for (size_t i = 0; i < ARRAY_SIZE(ALSAFormats); ++i) {
		snd_pcm_format_t format = ALSAFormats[i];
		if (!snd_pcm_hw_params_test_format(pcm, params, format)) {
			const char *name = snd_pcm_format_name(format);
			PyObject *pname=PyUnicode_FromString(name);
			PyObject *value=PyLong_FromLong((long)format);
			PyDict_SetItem(fmts,pname,value);
		}
	}
	return fmts;
}

static PyObject *
alsapcm_getratemaxmin(alsapcm_t *self, PyObject *args)
{
	snd_pcm_t *pcm = self->handle;
	if (!pcm) {
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	snd_pcm_hw_params_t *params;
	snd_pcm_hw_params_alloca(&params);
	int err = snd_pcm_hw_params_any(pcm, params);
	if (err < 0) {
		PyErr_SetString(ALSAAudioError, "Cannot get hardware parameters");
		return NULL;
	}

	unsigned min, max;
	if (snd_pcm_hw_params_get_rate_min(params, &min,NULL)<0) {
		PyErr_SetString(ALSAAudioError, "Cannot get minimum supported bitrate");
		return NULL;
	}
	if (snd_pcm_hw_params_get_rate_max(params, &max,NULL)<0) {
		PyErr_SetString(ALSAAudioError, "Cannot get maximum supported bitrate");
		return NULL;
	}

	PyObject *minp=PyLong_FromLong(min);
	PyObject *maxp=PyLong_FromLong(max);
	return PyTuple_Pack(2, minp, maxp);
}

static PyObject *
alsapcm_getrates(alsapcm_t *self, PyObject *args)
{
	snd_pcm_t *pcm = self->handle;
	if (!pcm) {
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	snd_pcm_hw_params_t *params;
	snd_pcm_hw_params_alloca(&params);
	int err = snd_pcm_hw_params_any(pcm, params);
	if (err < 0) {
		PyErr_SetString(ALSAAudioError, "Cannot get hardware parameters");
		return NULL;
	}

	unsigned min, max;
	if (snd_pcm_hw_params_get_rate_min(params, &min, NULL) <0 ) {
		PyErr_SetString(ALSAAudioError, "Cannot get minimum supported bitrate");
		return NULL;
	}
	if (snd_pcm_hw_params_get_rate_max(params, &max, NULL) < 0) {
		PyErr_SetString(ALSAAudioError, "Cannot get maximum supported bitrate");
		return NULL;
	}

	if (min == max) {
		return PyLong_FromLong(min);
	}
	else if (!snd_pcm_hw_params_test_rate(pcm, params, min + 1, 0)) {
		PyObject *minp=PyLong_FromLong(min);
		PyObject *maxp=PyLong_FromLong(max);
		return PyTuple_Pack(2,minp,maxp);
	}
	else {
		PyObject *rates=PyList_New(0);
		for (size_t i=0; i<ARRAY_SIZE(ALSARates); i++) {
			unsigned rate = ALSARates[i];
			if (!snd_pcm_hw_params_test_rate(pcm, params, rate, 0)) {
				PyObject *prate=PyLong_FromLong(rate);
				PyList_Append(rates,prate);
			}
		}
		return rates;
	}
}

static PyObject *
alsapcm_getchannels(alsapcm_t *self,PyObject *args)
{
	snd_pcm_t *pcm = self->handle;
	if (!pcm) {
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	snd_pcm_hw_params_t *params;
	snd_pcm_hw_params_alloca(&params);
	int err = snd_pcm_hw_params_any(pcm, params);
	if (err < 0) {
		PyErr_SetString(ALSAAudioError, "Cannot get hardware parameters");
		return NULL;
	}

	unsigned min, max;
	if (snd_pcm_hw_params_get_channels_min(params, &min) < 0) {
		PyErr_SetString(ALSAAudioError, "Cannot get minimum supported number of channels");
		return NULL;
	}

	if (snd_pcm_hw_params_get_channels_max(params, &max) < 0) {
		PyErr_SetString(ALSAAudioError, "Cannot get maximum supported number of channels");
		return NULL;
	}

	PyObject *out = PyList_New(0);
	for (unsigned ch=min;ch<=max;++ch) {
		if (!snd_pcm_hw_params_test_channels(pcm, params, ch)) {
			PyObject *pch=PyLong_FromLong(ch);
			PyList_Append(out,pch);
		}
	}
	return out;
}

static PyObject *
alsapcm_pcmtype(alsapcm_t *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args,":pcmtype"))
		return NULL;

	if (!self->handle) {
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	return PyLong_FromLong(self->pcmtype);
}

static PyObject *
alsapcm_pcmmode(alsapcm_t *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args,":pcmmode"))
		return NULL;

	if (!self->handle) {
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	return PyLong_FromLong(self->pcmmode);
}

static PyObject *
alsapcm_cardname(alsapcm_t *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args,":cardname"))
		return NULL;

	if (!self->handle) {
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	return PyUnicode_FromString(self->cardname);
}

static PyObject *
alsapcm_setchannels(alsapcm_t *self, PyObject *args)
{
	int channels, saved;
	int res;

	if (!PyArg_ParseTuple(args,"i:setchannels", &channels))
		return NULL;

	if (!self->handle) {
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	PyErr_WarnEx(PyExc_DeprecationWarning,
				 "This function is deprecated. "
				 "Please use the named parameter `channels` to `PCM()` instead", 1);

	saved = self->channels;
	self->channels = channels;
	res = alsapcm_setup(self);
	if (res < 0)
	{
		self->channels = saved;
		PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(res),
					 self->cardname);
		return NULL;
	}
	return PyLong_FromLong(self->channels);
}

static PyObject *
alsapcm_setrate(alsapcm_t *self, PyObject *args)
{
	int rate, saved;
	int res;

	if (!PyArg_ParseTuple(args,"i:setrate", &rate))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	PyErr_WarnEx(PyExc_DeprecationWarning,
				 "This function is deprecated. "
				 "Please use the named parameter `rate` to `PCM()` instead", 1);

	saved = self->rate;
	self->rate = rate;
	res = alsapcm_setup(self);
	if (res < 0)
	{
		self->rate = saved;
		PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(res),
					 self->cardname);
		return NULL;
	}
	return PyLong_FromLong(self->rate);
}

static PyObject *
alsapcm_setformat(alsapcm_t *self, PyObject *args)
{
	int format, saved;
	int res;

	if (!PyArg_ParseTuple(args,"i:setformat", &format))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	PyErr_WarnEx(PyExc_DeprecationWarning,
				 "This function is deprecated. "
				 "Please use the named parameter `format` to `PCM()` instead", 1);

	saved = self->format;
	self->format = format;
	res = alsapcm_setup(self);
	if (res < 0)
	{
		self->format = saved;
		PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(res),
					 self->cardname);
		return NULL;
	}
	return PyLong_FromLong(self->format);
}

static PyObject *
alsapcm_setperiodsize(alsapcm_t *self, PyObject *args)
{
	int periodsize, saved;
	int res;

	if (!PyArg_ParseTuple(args,"i:setperiodsize", &periodsize))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	PyErr_WarnEx(PyExc_DeprecationWarning,
				 "This function is deprecated. "
				 "Please use the named parameter `periodsize` to `PCM()` instead", 1);

	saved = self->periodsize;
	self->periodsize = periodsize;
	res = alsapcm_setup(self);
	if (res < 0)
	{
		self->periodsize = saved;
		PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(res),
					 self->cardname);



		return NULL;
	}

	return PyLong_FromLong(self->periodsize);
}

static PyObject *
alsapcm_read(alsapcm_t *self, PyObject *args)
{
	snd_pcm_state_t state;
	int res;
	int size = self->framesize * self->periodsize;
	int sizeout = 0;
	PyObject *buffer_obj, *tuple_obj, *res_obj;
	char *buffer;

	if (!PyArg_ParseTuple(args,":read"))
		return NULL;

	if (!self->handle) {
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
  }

	if (self->pcmtype != SND_PCM_STREAM_CAPTURE)
	{
		PyErr_Format(ALSAAudioError, "Cannot read from playback PCM [%s]",
					 self->cardname);
		return NULL;
	}

#if PY_MAJOR_VERSION < 3
	buffer_obj = PyString_FromStringAndSize(NULL, size);
	if (!buffer_obj)
		return NULL;
	buffer = PyString_AS_STRING(buffer_obj);
#else
	buffer_obj = PyBytes_FromStringAndSize(NULL, size);
	if (!buffer_obj)
		return NULL;
	buffer = PyBytes_AS_STRING(buffer_obj);
#endif

	state = snd_pcm_state(self->handle);
	if ((state != SND_PCM_STATE_XRUN && state != SND_PCM_STATE_SETUP) ||
		(res = snd_pcm_prepare(self->handle)) >= 0) {
		Py_BEGIN_ALLOW_THREADS
		res = snd_pcm_readi(self->handle, buffer, self->periodsize);
		Py_END_ALLOW_THREADS
	}

	if (res != -EPIPE)
	{
		if (res == -EAGAIN)
		{
			res = 0;
		}
		else if (res < 0) {
			PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(res),
						 self->cardname);

			Py_DECREF(buffer_obj);
			return NULL;
		}
	}

	if (res > 0 ) {
		sizeout = res * self->framesize;
	}

	if (size != sizeout) {
#if PY_MAJOR_VERSION < 3
		/* If the following fails, it will free the object */
		if (_PyString_Resize(&buffer_obj, sizeout))
			return NULL;
#else
		/* If the following fails, it will free the object */
		if (_PyBytes_Resize(&buffer_obj, sizeout))
			return NULL;
#endif
	}

	res_obj = PyLong_FromLong(res);
	if (!res_obj) {
		Py_DECREF(buffer_obj);
		return NULL;
	}
	tuple_obj = PyTuple_New(2);
	if (!tuple_obj) {
		Py_DECREF(buffer_obj);
		Py_DECREF(res_obj);
		return NULL;
	}
	/* Steal reference counts */
	PyTuple_SET_ITEM(tuple_obj, 0, res_obj);
	PyTuple_SET_ITEM(tuple_obj, 1, buffer_obj);

	return tuple_obj;
}

static PyObject *alsapcm_write(alsapcm_t *self, PyObject *args)
{
	snd_pcm_state_t state;
	int res;
	int datalen;
	char *data;
	PyObject *rc = NULL;

#if PY_MAJOR_VERSION < 3
	if (!PyArg_ParseTuple(args,"s#:write", &data, &datalen))
		return NULL;
#else
	Py_buffer buf;

	if (!PyArg_ParseTuple(args,"y*:write", &buf))
		return NULL;

	data = buf.buf;
	datalen = buf.len;
#endif

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	if (datalen % self->framesize)
	{
		PyErr_SetString(ALSAAudioError,
						"Data size must be a multiple of framesize");
		return NULL;
	}

	state = snd_pcm_state(self->handle);
	if ((state != SND_PCM_STATE_XRUN && state != SND_PCM_STATE_SETUP) ||
		(res = snd_pcm_prepare(self->handle)) >= 0) {
		Py_BEGIN_ALLOW_THREADS
		res = snd_pcm_writei(self->handle, data, datalen/self->framesize);
		Py_END_ALLOW_THREADS
	}

	if (res == -EAGAIN) {
		rc = PyLong_FromLong(0);
	}
	else if (res < 0)
	{
		PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(res),
					 self->cardname);
	}
	else {
		rc = PyLong_FromLong(res);
	}

#if PY_MAJOR_VERSION >= 3
	PyBuffer_Release(&buf);
#endif

	return rc;
}

static PyObject *alsapcm_pause(alsapcm_t *self, PyObject *args)
{
	int enabled=1, res;

	if (!PyArg_ParseTuple(args,"|i:pause", &enabled))
		return NULL;

	if (!self->handle) {
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	res = snd_pcm_pause(self->handle, enabled);
	if (res < 0)
	{
		PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(res),
					 self->cardname);

		return NULL;
	}
	return PyLong_FromLong(res);
}

static PyObject *alsapcm_drop(alsapcm_t *self)
{
	int res;

	if (!self->handle) {
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	res = snd_pcm_drop(self->handle);

	if (res < 0)
	{
		PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(res),
					 self->cardname);

		return NULL;
	}

	return PyLong_FromLong(res);
}

static PyObject *alsapcm_drain(alsapcm_t *self)
{
	int res;

	if (!self->handle) {
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	Py_BEGIN_ALLOW_THREADS
	res = snd_pcm_drain(self->handle);
	Py_END_ALLOW_THREADS

	if (res < 0)
	{
		PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(res),
					 self->cardname);

		return NULL;
	}

	return PyLong_FromLong(res);
}

static PyObject *
alsapcm_polldescriptors(alsapcm_t *self, PyObject *args)
{
	int i, count, rc;
	PyObject *result;
	struct pollfd *fds;

	if (!PyArg_ParseTuple(args,":polldescriptors"))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "PCM device is closed");
		return NULL;
	}

	count = snd_pcm_poll_descriptors_count(self->handle);
	if (count < 0)
	{
		PyErr_Format(ALSAAudioError, "Can't get poll descriptor count [%s]",
					 self->cardname);
		return NULL;
	}

	fds = (struct pollfd*)calloc(count, sizeof(struct pollfd));
	if (!fds)
	{
		PyErr_Format(PyExc_MemoryError, "Out of memory [%s]",
					 self->cardname);
		return NULL;
	}

	result = PyList_New(count);
	rc = snd_pcm_poll_descriptors(self->handle, fds, (unsigned int)count);
	if (rc != count)
	{
		PyErr_Format(ALSAAudioError, "Can't get poll descriptors [%s]",
					 self->cardname);
		free(fds);
		return NULL;
	}

	for (i = 0; i < count; ++i)
	{
		PyList_SetItem(result, i,
					   Py_BuildValue("ih", fds[i].fd, fds[i].events));
	}
	free(fds);

	return result;
}

/* ALSA PCM Object Bureaucracy */

static PyMethodDef alsapcm_methods[] = {
	{"pcmtype", (PyCFunction)alsapcm_pcmtype, METH_VARARGS},
	{"pcmmode", (PyCFunction)alsapcm_pcmmode, METH_VARARGS},
	{"cardname", (PyCFunction)alsapcm_cardname, METH_VARARGS},
	{"setchannels", (PyCFunction)alsapcm_setchannels, METH_VARARGS},
	{"setrate", (PyCFunction)alsapcm_setrate, METH_VARARGS},
	{"setformat", (PyCFunction)alsapcm_setformat, METH_VARARGS},
	{"setperiodsize", (PyCFunction)alsapcm_setperiodsize, METH_VARARGS},
	{"htimestamp", (PyCFunction) alsapcm_htimestamp, METH_VARARGS},
	{"set_tstamp_type", (PyCFunction) alsapcm_set_tstamp_type, METH_VARARGS},
	{"set_tstamp_mode", (PyCFunction) alsapcm_set_tstamp_mode, METH_VARARGS},
	{"get_tstamp_type", (PyCFunction) alsapcm_get_tstamp_type, METH_VARARGS},
	{"get_tstamp_mode", (PyCFunction) alsapcm_get_tstamp_mode, METH_VARARGS},
	{"dumpinfo", (PyCFunction)alsapcm_dumpinfo, METH_VARARGS},
	{"info", (PyCFunction)alsapcm_info, METH_VARARGS},
	{"state", (PyCFunction)alsapcm_state, METH_VARARGS},
	{"getformats", (PyCFunction)alsapcm_getformats, METH_VARARGS},
	{"getratebounds", (PyCFunction)alsapcm_getratemaxmin, METH_VARARGS},
	{"getrates", (PyCFunction)alsapcm_getrates, METH_VARARGS},
	{"getchannels", (PyCFunction)alsapcm_getchannels, METH_VARARGS},
	{"read", (PyCFunction)alsapcm_read, METH_VARARGS},
	{"write", (PyCFunction)alsapcm_write, METH_VARARGS},
	{"pause", (PyCFunction)alsapcm_pause, METH_VARARGS},
	{"drop", (PyCFunction)alsapcm_drop, METH_VARARGS},
	{"drain", (PyCFunction)alsapcm_drain, METH_VARARGS},
	{"close", (PyCFunction)alsapcm_close, METH_VARARGS},
	{"polldescriptors", (PyCFunction)alsapcm_polldescriptors, METH_VARARGS},
	{NULL, NULL}
};

static PyMethodDef alsa_methods[] = {
	{"asoundlib_version", (PyCFunction) alsa_asoundlib_version, METH_VARARGS},
	{NULL, NULL}
};


#if PY_VERSION_HEX < 0x02020000
static PyObject *
alsapcm_getattr(alsapcm_t *self, char *name) {
	return Py_FindMethod(alsapcm_methods, (PyObject *)self, name);
}
#endif

static PyTypeObject ALSAPCMType = {
#if PY_MAJOR_VERSION < 3
	PyObject_HEAD_INIT(&PyType_Type)
	0,							  /* ob_size */
#else
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
#endif
	"alsaaudio.PCM",				/* tp_name */
	sizeof(alsapcm_t),			  /* tp_basicsize */
	0,							  /* tp_itemsize */
	/* methods */
	(destructor) alsapcm_dealloc,   /* tp_dealloc */
	0,							  /* print */
#if PY_VERSION_HEX < 0x02020000
	(getattrfunc)alsapcm_getattr,   /* tp_getattr */
#else
	0,							  /* tp_getattr */
#endif
	0,							  /* tp_setattr */
	0,							  /* tp_compare */
	0,							  /* tp_repr */
	0,							  /* tp_as_number */
	0,							  /* tp_as_sequence */
	0,							  /* tp_as_mapping */
	0,							  /* tp_hash */
	0,							  /* tp_call */
	0,							  /* tp_str */
#if PY_VERSION_HEX >= 0x02020000
	PyObject_GenericGetAttr,		/* tp_getattro */
#else
	0,							  /* tp_getattro */
#endif
	0,							  /* tp_setattro */
	0,							  /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,			 /* tp_flags */
	"ALSA PCM device.",			 /* tp_doc */
	0,							/* tp_traverse */
	0,							/* tp_clear */
	0,							/* tp_richcompare */
	0,							/* tp_weaklistoffset */
	0,							/* tp_iter */
	0,							/* tp_iternext */
	alsapcm_methods,				  /* tp_methods */
	0,							/* tp_members */
};


/******************************************/
/* Mixer object wrapper				   */
/******************************************/

static PyTypeObject ALSAMixerType;

#define MIXER_CAP_VOLUME			0x0001
#define MIXER_CAP_VOLUME_JOINED	 0x0002
#define MIXER_CAP_PVOLUME		   0x0004
#define MIXER_CAP_PVOLUME_JOINED	0x0008
#define MIXER_CAP_CVOLUME		   0x0010
#define MIXER_CAP_CVOLUME_JOINED	0x0020

#define MIXER_CAP_SWITCH			0x0001
#define MIXER_CAP_SWITCH_JOINED	 0x0002
#define MIXER_CAP_PSWITCH		   0x0004
#define MIXER_CAP_PSWITCH_JOINED	0x0008
#define MIXER_CAP_CSWITCH		   0x0010
#define MIXER_CAP_CSWITCH_JOINED	0x0020
#define MIXER_CAP_CSWITCH_EXCLUSIVE 0x0040

#define MIXER_CHANNEL_ALL -1

static int
alsamixer_gethandle(char *cardname, snd_mixer_t **handle)
{
	int err;
	if ((err = snd_mixer_open(handle, 0)) < 0)
		return err;
	if ((err = snd_mixer_attach(*handle, cardname)) >= 0 &&
		(err = snd_mixer_selem_register(*handle, NULL, NULL)) >= 0 &&
		(err = snd_mixer_load(*handle)) >= 0)
		return 0;
	snd_mixer_close(*handle);
	return err;
}

static PyObject *
alsamixer_list(PyObject *self, PyObject *args, PyObject *kwds)
{
	snd_mixer_t *handle;
	snd_mixer_selem_id_t *sid;
	snd_mixer_elem_t *elem;
	int err;
	int cardidx = -1;
	char hw_device[128];
	char *device = "default";
	PyObject *result;

	char *kw[] = { "cardindex", "device", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|is", kw,
									 &cardidx, &device))
		return NULL;

	if (cardidx >= 0) {
		if (cardidx < 32) {
			snprintf(hw_device, sizeof(hw_device), "hw:%d", cardidx);
			device = hw_device;
		}
		else {
			PyErr_Format(ALSAAudioError, "Invalid card number %d", cardidx);
			return NULL;
		}
	}

	snd_mixer_selem_id_alloca(&sid);
	err = alsamixer_gethandle(device, &handle);
	if (err < 0)
	{
		PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(err), device);
		return NULL;
	}

	result = PyList_New(0);

	for (elem = snd_mixer_first_elem(handle); elem;
		 elem = snd_mixer_elem_next(elem))
	{
		PyObject *mixer;
		snd_mixer_selem_get_id(elem, sid);
		mixer = PyUnicode_FromString(snd_mixer_selem_id_get_name(sid));
		PyList_Append(result,mixer);
		Py_DECREF(mixer);
	}
	snd_mixer_close(handle);

	return result;
}

static snd_mixer_elem_t *
alsamixer_find_elem(snd_mixer_t *handle, char *control, int id)
{
	snd_mixer_selem_id_t *sid;

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, id);
	snd_mixer_selem_id_set_name(sid, control);
	return snd_mixer_find_selem(handle, sid);
}

static PyObject *
alsamixer_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	alsamixer_t *self;
	int err;
	char *control = "Master";
	char *device = "default";
	char hw_device[128];
	int cardidx = -1;
	int id = 0;
	snd_mixer_elem_t *elem;
	int channel;
	char *kw[] = { "control", "id", "cardindex", "device", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|siis", kw,
									 &control, &id, &cardidx, &device)) {
		return NULL;
	}

	if (cardidx >= 0) {
		if (cardidx < 32) {
			snprintf(hw_device, sizeof(hw_device), "hw:%d", cardidx);
			device = hw_device;
		}
		else {
			PyErr_Format(ALSAAudioError, "Invalid card number %d", cardidx);
			return NULL;
		}
	}

	if (!(self = (alsamixer_t *)PyObject_New(alsamixer_t, &ALSAMixerType)))
		return NULL;

	self->handle = 0;

	err = alsamixer_gethandle(device, &self->handle);
	if (err < 0)
	{
		PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(err), device);

		return NULL;
	}

	self->cardname = strdup(device);
	self->controlname = strdup(control);
	self->controlid = id;

	elem = alsamixer_find_elem(self->handle,control, id);
	if (!elem)
	{
		snd_mixer_close(self->handle);
		PyErr_Format(ALSAAudioError,
					 "Unable to find mixer control %s,%i [%s]",
					 self->controlname, self->controlid, self->cardname);
		free(self->cardname);
		free(self->controlname);
		return NULL;
	}
	/* Determine mixer capabilities */
	self->volume_cap = self->switch_cap = 0;
	if (snd_mixer_selem_has_common_volume(elem))
	{
		self->volume_cap |= MIXER_CAP_VOLUME;
		if (snd_mixer_selem_has_playback_volume_joined(elem))
			self->volume_cap |= MIXER_CAP_VOLUME_JOINED;
	}
	else
	{
		if (snd_mixer_selem_has_playback_volume(elem))
		{
			self->volume_cap |= MIXER_CAP_PVOLUME;
			if (snd_mixer_selem_has_playback_volume_joined(elem))
				self->volume_cap |= MIXER_CAP_PVOLUME_JOINED;
		}
		if (snd_mixer_selem_has_capture_volume(elem))
		{
			self->volume_cap |= MIXER_CAP_CVOLUME;
			if (snd_mixer_selem_has_capture_volume_joined(elem))
				self->volume_cap |= MIXER_CAP_CVOLUME_JOINED;
		}
	}

	if (snd_mixer_selem_has_common_switch(elem))
	{
		self->switch_cap |= MIXER_CAP_SWITCH;
		if (snd_mixer_selem_has_playback_switch_joined(elem))
			self->switch_cap |= MIXER_CAP_SWITCH_JOINED;
	}
	else
	{
		if (snd_mixer_selem_has_playback_switch(elem)) {
			self->switch_cap |= MIXER_CAP_PSWITCH;
			if (snd_mixer_selem_has_playback_switch_joined(elem))
				self->switch_cap |= MIXER_CAP_PSWITCH_JOINED;
		}
		if (snd_mixer_selem_has_capture_switch(elem)) {
			self->switch_cap |= MIXER_CAP_CSWITCH;
			if (snd_mixer_selem_has_capture_switch_joined(elem))
				self->switch_cap |= MIXER_CAP_CSWITCH_JOINED;
			if (snd_mixer_selem_has_capture_switch_exclusive(elem))
				self->switch_cap |= MIXER_CAP_CSWITCH_EXCLUSIVE;
		}
	}
	self->pchannels = 0;
	if (self->volume_cap | MIXER_CAP_PVOLUME ||
		self->switch_cap | MIXER_CAP_PSWITCH)
	{
		if (snd_mixer_selem_is_playback_mono(elem)) self->pchannels = 1;
		else {
			for (channel=0; channel <= SND_MIXER_SCHN_LAST; channel++) {
				if (snd_mixer_selem_has_playback_channel(elem, channel))
					self->pchannels++;
				else break;
			}
		}
	}
	self->cchannels = 0;
	if (self->volume_cap | MIXER_CAP_CVOLUME ||
		self->switch_cap | MIXER_CAP_CSWITCH)
	{
		if (snd_mixer_selem_is_capture_mono(elem))
			self->cchannels = 1;
		else
		{
			for (channel=0; channel <= SND_MIXER_SCHN_LAST; channel++) {
				if (snd_mixer_selem_has_capture_channel(elem, channel))
					self->cchannels++;
				else break;
			}
		}
	}
	snd_mixer_selem_get_playback_volume_range(elem, &self->pmin, &self->pmax);
	snd_mixer_selem_get_capture_volume_range(elem, &self->cmin, &self->cmax);
	snd_mixer_selem_get_playback_dB_range(elem, &self->pmin_dB, &self->pmax_dB);
	snd_mixer_selem_get_capture_dB_range(elem, &self->cmin_dB, &self->cmax_dB);

	return (PyObject *)self;
}

static void alsamixer_dealloc(alsamixer_t *self)
{
	if (self->handle) {
		snd_mixer_close(self->handle);
		free(self->cardname);
		free(self->controlname);
		self->handle = 0;
	}
	PyObject_Del(self);
}

static PyObject *
alsamixer_close(alsamixer_t *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args,":close"))
		return NULL;

	if (self->handle) {
		snd_mixer_close(self->handle);
		free(self->cardname);
		free(self->controlname);
		self->handle = 0;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
alsamixer_cardname(alsamixer_t *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args,":cardname"))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "Mixer is closed");
		return NULL;
	}

	return PyUnicode_FromString(self->cardname);
}

static PyObject *
alsamixer_mixer(alsamixer_t *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args,":mixer"))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "Mixer is closed");
		return NULL;
	}

	return PyUnicode_FromString(self->controlname);
}

static PyObject *
alsamixer_mixerid(alsamixer_t *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args,":mixerid"))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "Mixer is closed");
		return NULL;
	}

	return PyLong_FromLong(self->controlid);
}

static PyObject *
alsamixer_volumecap(alsamixer_t *self, PyObject *args)
{
	PyObject *result;
	PyObject *item;

	if (!PyArg_ParseTuple(args,":volumecap")) return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "Mixer is closed");
		return NULL;
	}

	result = PyList_New(0);
	if (self->volume_cap & MIXER_CAP_VOLUME)
	{
		item = PyUnicode_FromString("Volume");
		PyList_Append(result, item);
		Py_DECREF(item);
	}
	if (self->volume_cap & MIXER_CAP_VOLUME_JOINED)
	{
		item = PyUnicode_FromString("Joined Volume");
		PyList_Append(result, item);
		Py_DECREF(item);
	}
	if (self->volume_cap & MIXER_CAP_PVOLUME)
	{
		item = PyUnicode_FromString("Playback Volume");
		PyList_Append(result, item);
		Py_DECREF(item);
	}
	if (self->volume_cap & MIXER_CAP_PVOLUME_JOINED)
	{
		item = PyUnicode_FromString("Joined Playback Volume");
		PyList_Append(result, item);
		Py_DECREF(item);
	}
	if (self->volume_cap & MIXER_CAP_CVOLUME)
	{
		item = PyUnicode_FromString("Capture Volume");
		PyList_Append(result, item);
		Py_DECREF(item);
	}
	if (self->volume_cap & MIXER_CAP_CVOLUME_JOINED)
	{
		item = PyUnicode_FromString("Joined Capture Volume");
		PyList_Append(result, item);
		Py_DECREF(item);
	}

	return result;
}

static PyObject *
alsamixer_switchcap(alsamixer_t *self, PyObject *args)
{
	PyObject *result;
	PyObject *item;

	if (!PyArg_ParseTuple(args,":switchcap"))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "Mixer is closed");
		return NULL;
	}

	result = PyList_New(0);
	if (self->switch_cap & MIXER_CAP_SWITCH)
	{
		item = PyUnicode_FromString("Mute");
		PyList_Append(result, item);
		Py_DECREF(item);
	}
	if (self->switch_cap & MIXER_CAP_SWITCH_JOINED)
	{
		item = PyUnicode_FromString("Joined Mute");
		PyList_Append(result, item);
		Py_DECREF(item);
	}
	if (self->switch_cap & MIXER_CAP_PSWITCH)
	{
		item = PyUnicode_FromString("Playback Mute");
		PyList_Append(result, item);
		Py_DECREF(item);
	}
	if (self->switch_cap & MIXER_CAP_PSWITCH_JOINED)
	{
		item = PyUnicode_FromString("Joined Playback Mute");
		PyList_Append(result, item);
		Py_DECREF(item);
	}
	if (self->switch_cap & MIXER_CAP_CSWITCH)
	{
		item = PyUnicode_FromString("Capture Mute");
		PyList_Append(result, item);
		Py_DECREF(item);
	}
	if (self->switch_cap & MIXER_CAP_CSWITCH_JOINED)
	{
		item = PyUnicode_FromString("Joined Capture Mute");
		PyList_Append(result, item);
		Py_DECREF(item);
	}
	if (self->switch_cap & MIXER_CAP_CSWITCH_EXCLUSIVE)
	{
		item = PyUnicode_FromString("Capture Exclusive");
		PyList_Append(result, item);
		Py_DECREF(item);
	}

	return result;
}

static int alsamixer_getpercentage(long min, long max, long value)
{
	/* Convert from number in range to percentage */
	int range = max - min;
	int tmp;

	if (range == 0)
		return 0;

	value -= min;
	tmp = rint((double)value/(double)range * 100);
	return tmp;
}

static long alsamixer_getphysvolume(long min, long max, int percentage)
{
	/* Convert from percentage to number in range */
	int range = max - min;
	int tmp;

	if (range == 0)
		return 0;

	tmp = rint((double)range * ((double)percentage*.01)) + min;
	return tmp;
}

static PyObject *
alsamixer_getvolume(alsamixer_t *self, PyObject *args, PyObject *kwds)
{
	snd_mixer_elem_t *elem;
	int channel;
	long ival;
	PyObject *pcmtypeobj = NULL;
	long pcmtype;
	int iunits = VOLUME_UNITS_PERCENTAGE;
	PyObject *result;
	PyObject *item;

	char *kw[] = { "pcmtype", "units", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|Oi:getvolume", kw, &pcmtypeobj, &iunits)) {
		return NULL;
	}

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "Mixer is closed");
		return NULL;
	}

	pcmtype = get_pcmtype(pcmtypeobj);
	if (pcmtype < 0) {
		return NULL;
	}

	if (!is_value_volume_unit(iunits)) {
		PyErr_SetString(ALSAAudioError, "Invalid volume units");
		return NULL;
	}
	volume_units_t units = iunits;

	elem = alsamixer_find_elem(self->handle,self->controlname,self->controlid);

	if (!pcmtypeobj || (pcmtypeobj == Py_None)) {
		if (self->pchannels) {
			pcmtype = SND_PCM_STREAM_PLAYBACK;
		}
		else {
			pcmtype = SND_PCM_STREAM_CAPTURE;
		}
	}

	result = PyList_New(0);

	for (channel = 0; channel <= SND_MIXER_SCHN_LAST; channel++) {
		if (pcmtype == SND_PCM_STREAM_PLAYBACK &&
			snd_mixer_selem_has_playback_channel(elem, channel))
		{
			switch (units)
			{
			case VOLUME_UNITS_PERCENTAGE:
				snd_mixer_selem_get_playback_volume(elem, channel, &ival);
				ival = alsamixer_getpercentage(self->pmin, self->pmax, ival);
				break;
			case VOLUME_UNITS_RAW:
				snd_mixer_selem_get_playback_volume(elem, channel, &ival);
				break;
			case VOLUME_UNITS_DB:
				snd_mixer_selem_get_playback_dB(elem, channel, &ival);
				break;
			}

			item = PyLong_FromLong(ival);
			PyList_Append(result, item);
			Py_DECREF(item);
		}
		else if (pcmtype == SND_PCM_STREAM_CAPTURE
				 && snd_mixer_selem_has_capture_channel(elem, channel)
				 && snd_mixer_selem_has_capture_volume(elem)) {
			switch (units)
			{
			case VOLUME_UNITS_PERCENTAGE:
				snd_mixer_selem_get_capture_volume(elem, channel, &ival);
				ival = alsamixer_getpercentage(self->cmin, self->cmax, ival);
				break;
			case VOLUME_UNITS_RAW:
				snd_mixer_selem_get_capture_volume(elem, channel, &ival);
				break;
			case VOLUME_UNITS_DB:
				snd_mixer_selem_get_capture_dB(elem, channel, &ival);
				break;
			}

			item = PyLong_FromLong(ival);
			PyList_Append(result, item);
			Py_DECREF(item);
		}
	}

	return result;
}

static PyObject *
alsamixer_getrange(alsamixer_t *self, PyObject *args, PyObject *kwds)
{
	snd_mixer_elem_t *elem;
	PyObject *pcmtypeobj = NULL;
	int iunits = VOLUME_UNITS_RAW;
	long pcmtype;
	long min = -1, max = -1;

	char *kw[] = { "pcmtype", "units", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|Oi:getrange", kw, &pcmtypeobj, &iunits)) {
		return NULL;
	}

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "Mixer is closed");
		return NULL;
	}

	pcmtype = get_pcmtype(pcmtypeobj);
	if (pcmtype < 0) {
		return NULL;
	}

	if (!is_value_volume_unit(iunits)) {
		PyErr_SetString(ALSAAudioError, "Invalid volume units");
		return NULL;
	}
	volume_units_t units = iunits;

	elem = alsamixer_find_elem(self->handle, self->controlname,
							   self->controlid);

	if (!pcmtypeobj || (pcmtypeobj == Py_None))
	{
		if (self->pchannels) {
			pcmtype = SND_PCM_STREAM_PLAYBACK;
		}
		else {
			pcmtype = SND_PCM_STREAM_CAPTURE;
		}
	}

	if (pcmtype == SND_PCM_STREAM_PLAYBACK)
	{
		if (snd_mixer_selem_has_playback_channel(elem, 0))
		{
			switch (units)
			{
			case VOLUME_UNITS_PERCENTAGE:
				min = 0;
				max = 100;
				break;
			case VOLUME_UNITS_RAW:
				min = self->pmin;
				max = self->pmax;
				break;
			case VOLUME_UNITS_DB:
				min = self->pmin_dB;
				max = self->pmax_dB;
				break;
			}
			return Py_BuildValue("[ii]", min, max);
		}

		PyErr_Format(ALSAAudioError, "Mixer %s,%d has no playback channel [%s]",
					 self->controlname, self->controlid, self->cardname);
		return NULL;
	}
	else
	{
		if (snd_mixer_selem_has_capture_channel(elem, 0)
			&& snd_mixer_selem_has_capture_volume(elem)) {
			switch (units)
			{
			case VOLUME_UNITS_PERCENTAGE:
				min = 0;
				max = 100;
				break;
			case VOLUME_UNITS_RAW:
				min = self->cmin;
				max = self->cmax;
				break;
			case VOLUME_UNITS_DB:
				min = self->cmin_dB;
				max = self->cmax_dB;
				break;
			}
			return Py_BuildValue("[ii]", min, max);
		}

		PyErr_Format(ALSAAudioError, "Mixer %s,%d has no capture channel "
					 "or capture volume [%s]",
					 self->controlname, self->controlid, self->cardname);
		return NULL;
	}

	// Unreached statement
	PyErr_SetString(ALSAAudioError, "Huh?");
	return NULL;
}

static PyObject *
alsamixer_getenum(alsamixer_t *self, PyObject *args)
{
	snd_mixer_elem_t *elem;
	PyObject *elems;
	int i, count, rc;
	unsigned int index;
	char name[64];
	PyObject *result;

	if (!PyArg_ParseTuple(args, ":getenum"))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "Mixer is closed");
		return NULL;
	}

	elem = alsamixer_find_elem(self->handle,self->controlname,self->controlid);
	if (!snd_mixer_selem_is_enumerated(elem)) {
		// Not an enumerated control, return an empty tuple
		return PyTuple_New(0);
	}

	count = snd_mixer_selem_get_enum_items(elem);
	if (count < 0)
	{
		PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(count),
					 self->cardname);
		return NULL;
	}

	result = PyTuple_New(2);
	if (!result)
		return NULL;

	rc = snd_mixer_selem_get_enum_item(elem, 0, &index);
	if (rc)
	{
		PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(rc),
					 self->cardname);
		return NULL;
	}
	rc = snd_mixer_selem_get_enum_item_name(elem, index, sizeof(name)-1, name);
	if (rc)
	{
		Py_DECREF(result);
		PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(rc),
					 self->cardname);
		return NULL;
	}

	PyTuple_SetItem(result, 0, PyUnicode_FromString(name));

	elems = PyList_New(count);
	if (!elems)
	{
		Py_DECREF(result);
		return NULL;
	}

	for (i = 0; i < count; ++i)
	{
		rc = snd_mixer_selem_get_enum_item_name(elem, i, sizeof(name)-1, name);
		if (rc) {
			Py_DECREF(elems);
			Py_DECREF(result);
			PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(rc),
						 self->cardname);

			return NULL;
		}

		PyList_SetItem(elems, i, PyUnicode_FromString(name));
	}

	PyTuple_SetItem(result, 1, elems);

	return result;
}

static PyObject *
alsamixer_setenum(alsamixer_t *self, PyObject *args)
{
	snd_mixer_elem_t *elem;
	int index, count, rc;

	if (!PyArg_ParseTuple(args, "i:setenum", &index))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "Mixer is closed");
		return NULL;
	}

	elem = alsamixer_find_elem(self->handle,self->controlname,self->controlid);
	if (!snd_mixer_selem_is_enumerated(elem)) {
		PyErr_SetString(ALSAAudioError, "Not an enumerated control");
		return NULL;
	}

	count = snd_mixer_selem_get_enum_items(elem);
	if (count < 0)
	{
		PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(count),
					 self->cardname);
		return NULL;
	}

	if (index < 0 || index >= count) {
		PyErr_Format(ALSAAudioError, "Enum index out of range 0 <= %d < %d",
					 index, count);
		return NULL;
	}

	rc = snd_mixer_selem_set_enum_item(elem, 0, index);
	if (rc)
	{
		PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(rc),
					 self->cardname);
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject *
alsamixer_getmute(alsamixer_t *self, PyObject *args)
{
	snd_mixer_elem_t *elem;
	int i;
	int ival;
	PyObject *result;
	PyObject *item;

	if (!PyArg_ParseTuple(args,":getmute"))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "Mixer is closed");
		return NULL;
	}

	elem = alsamixer_find_elem(self->handle, self->controlname,
							   self->controlid);
	if (!snd_mixer_selem_has_playback_switch(elem))
	{
		PyErr_Format(ALSAAudioError,
					 "Mixer %s,%d has no playback switch capabilities, [%s]",
					 self->controlname, self->controlid, self->cardname);

		return NULL;
	}

	result = PyList_New(0);

	for (i = 0; i <= SND_MIXER_SCHN_LAST; i++)
	{
		if (snd_mixer_selem_has_playback_channel(elem, i))
		{
			snd_mixer_selem_get_playback_switch(elem, i, &ival);

			item = PyLong_FromLong(!ival);
			PyList_Append(result, item);
			Py_DECREF(item);
		}
	}
	return result;
}

static PyObject *
alsamixer_getrec(alsamixer_t *self, PyObject *args)
{
	snd_mixer_elem_t *elem;
	int i;
	int ival;
	PyObject *result;
	PyObject *item;

	if (!PyArg_ParseTuple(args,":getrec"))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "Mixer is closed");
		return NULL;
	}

	elem = alsamixer_find_elem(self->handle, self->controlname,
							   self->controlid);
	if (!snd_mixer_selem_has_capture_switch(elem))
	{
		PyErr_Format(ALSAAudioError,
					 "Mixer %s,%d has no capture switch capabilities [%s]",
					 self->controlname, self->controlid, self->cardname);
		return NULL;
	}

	result = PyList_New(0);

	for (i = 0; i <= SND_MIXER_SCHN_LAST; i++)
	{
		if (snd_mixer_selem_has_capture_channel(elem, i))
		{
			snd_mixer_selem_get_capture_switch(elem, i, &ival);
			item = PyLong_FromLong(ival);
			PyList_Append(result, item);
			Py_DECREF(item);
		}
	}
	return result;
}

static PyObject *
alsamixer_setvolume(alsamixer_t *self, PyObject *args, PyObject *kwds)
{
	snd_mixer_elem_t *elem;
	int i;
	long volume;
	int physvolume;
	PyObject *pcmtypeobj = NULL;
	long pcmtype;
	int iunits = VOLUME_UNITS_PERCENTAGE;
	int channel = MIXER_CHANNEL_ALL;
	int done = 0;

	char *kw[] = { "volume", "channel", "pcmtype", "units", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "l|iOi:setvolume", kw, &volume,
		&channel, &pcmtypeobj, &iunits)) {
		return NULL;
	}

	pcmtype = get_pcmtype(pcmtypeobj);
	if (pcmtype < 0) {
		return NULL;
	}

	if (!is_value_volume_unit(iunits)) {
		PyErr_SetString(ALSAAudioError, "Invalid volume units");
		return NULL;
	}
	volume_units_t units = iunits;

	if (units == VOLUME_UNITS_PERCENTAGE && (volume < 0 || volume > 100))
	{
		PyErr_SetString(ALSAAudioError, "Volume out of range");
		return NULL;
	}

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "Mixer is closed");
		return NULL;
	}

	elem = alsamixer_find_elem(self->handle,self->controlname,self->controlid);

	if (!pcmtypeobj || (pcmtypeobj == Py_None))
	{
		if (self->pchannels)
			pcmtype = SND_PCM_STREAM_PLAYBACK;
		else
			pcmtype = SND_PCM_STREAM_CAPTURE;
	}

	for (i = 0; i <= SND_MIXER_SCHN_LAST; i++)
	{
		if (channel == -1 || channel == i)
		{
			if (pcmtype == SND_PCM_STREAM_PLAYBACK &&
				snd_mixer_selem_has_playback_channel(elem, i)) {
				switch (units)
				{
				case VOLUME_UNITS_PERCENTAGE:
					physvolume = alsamixer_getphysvolume(self->pmin,
													 	self->pmax, volume);
					snd_mixer_selem_set_playback_volume(elem, i, physvolume);
					break;
				case VOLUME_UNITS_RAW:
					snd_mixer_selem_set_playback_volume(elem, i, volume);
					break;
				case VOLUME_UNITS_DB:
					snd_mixer_selem_set_playback_dB(elem, i, volume, 0);
					break;
				}
				done++;
			}
			else if (pcmtype == SND_PCM_STREAM_CAPTURE
					 && snd_mixer_selem_has_capture_channel(elem, i)
					 && snd_mixer_selem_has_capture_volume(elem))
			{
				switch (units)
				{
				case VOLUME_UNITS_PERCENTAGE:
					physvolume = alsamixer_getphysvolume(self->cmin, self->cmax,
													 	volume);
					snd_mixer_selem_set_capture_volume(elem, i, physvolume);
					break;
				case VOLUME_UNITS_RAW:
					snd_mixer_selem_set_capture_volume(elem, i, volume);
					break;
				case VOLUME_UNITS_DB:
					snd_mixer_selem_set_capture_dB(elem, i, volume, 0);
					break;
				}
				done++;
			}
		}
	}

	if(!done)
	{
		PyErr_Format(ALSAAudioError, "No such channel [%s]",
					 self->cardname);
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
alsamixer_setmute(alsamixer_t *self, PyObject *args)
{
	snd_mixer_elem_t *elem;
	int i;
	int mute = 0;
	int done = 0;
	int channel = MIXER_CHANNEL_ALL;
	if (!PyArg_ParseTuple(args,"i|i:setmute", &mute, &channel))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "Mixer is closed");
		return NULL;
	}

	elem = alsamixer_find_elem(self->handle,self->controlname,self->controlid);
	if (!snd_mixer_selem_has_playback_switch(elem))
	{
		PyErr_Format(ALSAAudioError,
					 "Mixer %s,%d has no playback switch capabilities [%s]",
					 self->controlname, self->controlid, self->cardname);
		return NULL;
	}
	for (i = 0; i <= SND_MIXER_SCHN_LAST; i++)
	{
		if (channel == MIXER_CHANNEL_ALL || channel == i)
		{
			if (snd_mixer_selem_has_playback_channel(elem, i))
			{
				snd_mixer_selem_set_playback_switch(elem, i, !mute);
				done++;
			}
		}
	}
	if (!done)
	{
		PyErr_Format(ALSAAudioError, "Invalid channel number [%s]",
					 self->cardname);
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
alsamixer_setrec(alsamixer_t *self, PyObject *args)
{
	snd_mixer_elem_t *elem;
	int i;
	int rec = 0;
	int done = 0;
	int channel = MIXER_CHANNEL_ALL;

	if (!PyArg_ParseTuple(args,"i|i:setrec", &rec, &channel))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "Mixer is closed");
		return NULL;
	}

	elem = alsamixer_find_elem(self->handle, self->controlname,
							   self->controlid);
	if (!snd_mixer_selem_has_capture_switch(elem))
	{
		PyErr_Format(ALSAAudioError,
					 "Mixer %s,%d has no record switch capabilities [%s]",
					 self->controlname, self->controlid, self->cardname);
		return NULL;
	}
	for (i = 0; i <= SND_MIXER_SCHN_LAST; i++)
	{
		if (channel == MIXER_CHANNEL_ALL || channel == i)
		{
			if (snd_mixer_selem_has_capture_channel(elem, i))
			{
				snd_mixer_selem_set_capture_switch(elem, i, rec);
				done++;
			}
		}
	}
	if (!done)
	{
		PyErr_Format(ALSAAudioError, "Invalid channel number [%s]",
					 self->cardname);
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
alsamixer_polldescriptors(alsamixer_t *self, PyObject *args)
{
	int i, count, rc;
	PyObject *result;
	struct pollfd *fds;

	if (!PyArg_ParseTuple(args,":polldescriptors"))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "Mixer is closed");
		return NULL;
	}

	count = snd_mixer_poll_descriptors_count(self->handle);
	if (count < 0)
	{
		PyErr_Format(ALSAAudioError, "Can't get poll descriptor count [%s]",
					 self->cardname);
		return NULL;
	}

	fds = (struct pollfd*)calloc(count, sizeof(struct pollfd));
	if (!fds)
	{
		PyErr_Format(PyExc_MemoryError, "Out of memory [%s]",
					 self->cardname);
		return NULL;
	}

	result = PyList_New(count);
	rc = snd_mixer_poll_descriptors(self->handle, fds, (unsigned int)count);
	if (rc != count)
	{
		PyErr_Format(ALSAAudioError, "Can't get poll descriptors [%s]",
					 self->cardname);
		free(fds);
		return NULL;
	}

	for (i = 0; i < count; ++i)
	{
		PyList_SetItem(result, i,
					   Py_BuildValue("ih", fds[i].fd, fds[i].events));
	}
	free(fds);

	return result;
}

static PyObject *
alsamixer_handleevents(alsamixer_t *self, PyObject *args)
{
	int handled;

	if (!PyArg_ParseTuple(args,":handleevents"))
		return NULL;

	if (!self->handle)
	{
		PyErr_SetString(ALSAAudioError, "Mixer is closed");
		return NULL;
	}

	handled = snd_mixer_handle_events(self->handle);
	if (handled < 0)
	{
		PyErr_Format(ALSAAudioError, "%s [%s]", snd_strerror(handled),
					 self->cardname);
		return NULL;
	}

	return PyLong_FromLong(handled);
}

static PyMethodDef alsamixer_methods[] = {
	{"cardname", (PyCFunction)alsamixer_cardname, METH_VARARGS},
	{"close", (PyCFunction)alsamixer_close, METH_VARARGS},
	{"mixer", (PyCFunction)alsamixer_mixer, METH_VARARGS},
	{"mixerid", (PyCFunction)alsamixer_mixerid, METH_VARARGS},
	{"switchcap", (PyCFunction)alsamixer_switchcap, METH_VARARGS},
	{"volumecap", (PyCFunction)alsamixer_volumecap, METH_VARARGS},
	{"getvolume", (PyCFunction)alsamixer_getvolume, METH_VARARGS | METH_KEYWORDS},
	{"getrange", (PyCFunction)alsamixer_getrange, METH_VARARGS | METH_KEYWORDS},
	{"getenum", (PyCFunction)alsamixer_getenum, METH_VARARGS},
	{"getmute", (PyCFunction)alsamixer_getmute, METH_VARARGS},
	{"getrec", (PyCFunction)alsamixer_getrec, METH_VARARGS},
	{"setvolume", (PyCFunction)alsamixer_setvolume, METH_VARARGS | METH_KEYWORDS},
	{"setenum", (PyCFunction)alsamixer_setenum, METH_VARARGS},
	{"setmute", (PyCFunction)alsamixer_setmute, METH_VARARGS},
	{"setrec", (PyCFunction)alsamixer_setrec, METH_VARARGS},
	{"polldescriptors", (PyCFunction)alsamixer_polldescriptors, METH_VARARGS},
	{"handleevents", (PyCFunction)alsamixer_handleevents, METH_VARARGS},

	{NULL, NULL}
};

#if PY_VERSION_HEX < 0x02020000
static PyObject *
alsamixer_getattr(alsapcm_t *self, char *name)
{
	return Py_FindMethod(alsamixer_methods, (PyObject *)self, name);
}
#endif

static PyTypeObject ALSAMixerType = {
#if PY_MAJOR_VERSION < 3
	PyObject_HEAD_INIT(&PyType_Type)
	0,							  /* ob_size */
#else
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
#endif
	"alsaaudio.Mixer",			  /* tp_name */
	sizeof(alsamixer_t),			/* tp_basicsize */
	0,							  /* tp_itemsize */
	/* methods */
	(destructor) alsamixer_dealloc, /* tp_dealloc */
	0,							  /* print */
#if PY_VERSION_HEX < 0x02020000
	(getattrfunc)alsamixer_getattr, /* tp_getattr */
#else
	0,							  /* tp_getattr */
#endif
	0,							  /* tp_setattr */
	0,							  /* tp_compare */
	0,							  /* tp_repr */
	0,							  /* tp_as_number */
	0,							  /* tp_as_sequence */
	0,							  /* tp_as_mapping */
	0,							  /* tp_hash */
	0,							  /* tp_call */
	0,							  /* tp_str */
#if PY_VERSION_HEX >= 0x02020000
	PyObject_GenericGetAttr,		/* tp_getattro*/
#else
	0,							  /* tp_getattro*/
#endif
	0,							  /* tp_setattro*/
	0,							  /* tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,			 /* tp_flags */
	"ALSA Mixer Control.",		  /* tp_doc */
	0,							/* tp_traverse */
	0,							/* tp_clear */
	0,							/* tp_richcompare */
	0,							/* tp_weaklistoffset */
	0,							/* tp_iter */
	0,							/* tp_iternext */
	alsamixer_methods,			/* tp_methods */
	0,							/* tp_members */
};


/******************************************/
/* Module initialization				  */
/******************************************/

static PyMethodDef alsaaudio_methods[] = {
	{ "card_indexes", (PyCFunction)alsacard_list_indexes, METH_VARARGS},
	{ "card_name", (PyCFunction)alsacard_name, METH_VARARGS},
	{ "cards", (PyCFunction)alsacard_list, METH_VARARGS},
	{ "pcms", (PyCFunction)alsapcm_list, METH_VARARGS|METH_KEYWORDS},
	{ "mixers", (PyCFunction)alsamixer_list, METH_VARARGS|METH_KEYWORDS},
	{ 0, 0 },
};


#if PY_MAJOR_VERSION >= 3

#define _EXPORT_INT(mod, name, value) \
  if (PyModule_AddIntConstant(mod, name, (long) value) == -1) return NULL;

static struct PyModuleDef alsaaudio_module = {
	PyModuleDef_HEAD_INIT,
	"alsaaudio",
	NULL,  /* m_doc */
	-1,
	alsaaudio_methods,
	0,  /* m_reload */
	0,  /* m_traverse */
	0,  /* m_clear */
	0,  /* m_free */
};

#else

#define _EXPORT_INT(mod, name, value) \
  if (PyModule_AddIntConstant(mod, name, (long) value) == -1) return;

#endif // 3.0

#if PY_MAJOR_VERSION < 3
void initalsaaudio(void)
#else
PyObject *PyInit_alsaaudio(void)
#endif
{
	PyObject *m;
	ALSAPCMType.tp_new = alsapcm_new;
	ALSAMixerType.tp_new = alsamixer_new;

#if PY_VERSION_HEX < 0x03090000
	PyEval_InitThreads();
#endif

#if PY_MAJOR_VERSION < 3
	m = Py_InitModule3("alsaaudio", alsaaudio_methods);
	if (!m)
		return;
#else

	m = PyModule_Create(&alsaaudio_module);
	if (!m)
		return NULL;

#endif

	ALSAAudioError = PyErr_NewException("alsaaudio.ALSAAudioError", NULL,
										NULL);
	if (!ALSAAudioError)
#if PY_MAJOR_VERSION < 3
		return;
#else
		return NULL;
#endif

	/* Each call to PyModule_AddObject decrefs it; compensate: */

	Py_INCREF(&ALSAPCMType);
	PyModule_AddObject(m, "PCM", (PyObject *)&ALSAPCMType);

	Py_INCREF(&ALSAMixerType);
	PyModule_AddObject(m, "Mixer", (PyObject *)&ALSAMixerType);

	Py_INCREF(ALSAAudioError);
	PyModule_AddObject(m, "ALSAAudioError", ALSAAudioError);

	PyModule_AddFunctions(m, alsa_methods);

	_EXPORT_INT(m, "PCM_PLAYBACK",SND_PCM_STREAM_PLAYBACK);
	_EXPORT_INT(m, "PCM_CAPTURE",SND_PCM_STREAM_CAPTURE);

	_EXPORT_INT(m, "PCM_NORMAL",0);
	_EXPORT_INT(m, "PCM_NONBLOCK",SND_PCM_NONBLOCK);
	_EXPORT_INT(m, "PCM_ASYNC",SND_PCM_ASYNC);

	/* PCM Formats */
	_EXPORT_INT(m, "PCM_FORMAT_S8",SND_PCM_FORMAT_S8);
	_EXPORT_INT(m, "PCM_FORMAT_U8",SND_PCM_FORMAT_U8);
	_EXPORT_INT(m, "PCM_FORMAT_S16_LE",SND_PCM_FORMAT_S16_LE);
	_EXPORT_INT(m, "PCM_FORMAT_S16_BE",SND_PCM_FORMAT_S16_BE);
	_EXPORT_INT(m, "PCM_FORMAT_U16_LE",SND_PCM_FORMAT_U16_LE);
	_EXPORT_INT(m, "PCM_FORMAT_U16_BE",SND_PCM_FORMAT_U16_BE);
	_EXPORT_INT(m, "PCM_FORMAT_S24_LE",SND_PCM_FORMAT_S24_LE);
	_EXPORT_INT(m, "PCM_FORMAT_S24_BE",SND_PCM_FORMAT_S24_BE);
	_EXPORT_INT(m, "PCM_FORMAT_U24_LE",SND_PCM_FORMAT_U24_LE);
	_EXPORT_INT(m, "PCM_FORMAT_U24_BE",SND_PCM_FORMAT_U24_BE);
	_EXPORT_INT(m, "PCM_FORMAT_S32_LE",SND_PCM_FORMAT_S32_LE);
	_EXPORT_INT(m, "PCM_FORMAT_S32_BE",SND_PCM_FORMAT_S32_BE);
	_EXPORT_INT(m, "PCM_FORMAT_U32_LE",SND_PCM_FORMAT_U32_LE);
	_EXPORT_INT(m, "PCM_FORMAT_U32_BE",SND_PCM_FORMAT_U32_BE);
	_EXPORT_INT(m, "PCM_FORMAT_FLOAT_LE",SND_PCM_FORMAT_FLOAT_LE);
	_EXPORT_INT(m, "PCM_FORMAT_FLOAT_BE",SND_PCM_FORMAT_FLOAT_BE);
	_EXPORT_INT(m, "PCM_FORMAT_FLOAT64_LE",SND_PCM_FORMAT_FLOAT64_LE);
	_EXPORT_INT(m, "PCM_FORMAT_FLOAT64_BE",SND_PCM_FORMAT_FLOAT64_BE);
	_EXPORT_INT(m, "PCM_FORMAT_MU_LAW",SND_PCM_FORMAT_MU_LAW);
	_EXPORT_INT(m, "PCM_FORMAT_A_LAW",SND_PCM_FORMAT_A_LAW);
	_EXPORT_INT(m, "PCM_FORMAT_IMA_ADPCM",SND_PCM_FORMAT_IMA_ADPCM);
	_EXPORT_INT(m, "PCM_FORMAT_MPEG",SND_PCM_FORMAT_MPEG);
	_EXPORT_INT(m, "PCM_FORMAT_GSM",SND_PCM_FORMAT_GSM);
	_EXPORT_INT(m, "PCM_FORMAT_S24_3LE",SND_PCM_FORMAT_S24_3LE);
	_EXPORT_INT(m, "PCM_FORMAT_S24_3BE",SND_PCM_FORMAT_S24_3BE);
	_EXPORT_INT(m, "PCM_FORMAT_U24_3LE",SND_PCM_FORMAT_U24_3LE);
	_EXPORT_INT(m, "PCM_FORMAT_U24_3BE",SND_PCM_FORMAT_U24_3BE);

	/* PCM tstamp modes */
	_EXPORT_INT(m, "PCM_TSTAMP_NONE",SND_PCM_TSTAMP_NONE);
	_EXPORT_INT(m, "PCM_TSTAMP_ENABLE",SND_PCM_TSTAMP_ENABLE);

	/* PCM tstamp types */
	_EXPORT_INT(m, "PCM_TSTAMP_TYPE_GETTIMEOFDAY",SND_PCM_TSTAMP_TYPE_GETTIMEOFDAY);
	_EXPORT_INT(m, "PCM_TSTAMP_TYPE_MONOTONIC",SND_PCM_TSTAMP_TYPE_MONOTONIC);
	_EXPORT_INT(m, "PCM_TSTAMP_TYPE_MONOTONIC_RAW",SND_PCM_TSTAMP_TYPE_MONOTONIC_RAW);

	 /* DSD sample formats are included in ALSA 1.0.29 and higher
	  * define OVERRIDE_DSD_COMPILE to include DSD sample support
	  * if you use a patched ALSA lib version
	  */

#if SND_LIB_VERSION >= 0x1001d || defined OVERRIDE_DSD_COMPILE
	_EXPORT_INT(m, "PCM_FORMAT_DSD_U8", SND_PCM_FORMAT_DSD_U8);
	_EXPORT_INT(m, "PCM_FORMAT_DSD_U16_LE", SND_PCM_FORMAT_DSD_U16_LE);
	_EXPORT_INT(m, "PCM_FORMAT_DSD_U32_LE", SND_PCM_FORMAT_DSD_U32_LE);
	_EXPORT_INT(m, "PCM_FORMAT_DSD_U32_BE", SND_PCM_FORMAT_DSD_U32_BE);
#endif

	_EXPORT_INT(m, "PCM_STATE_OPEN", SND_PCM_STATE_OPEN);
	_EXPORT_INT(m, "PCM_STATE_SETUP", SND_PCM_STATE_SETUP);
	_EXPORT_INT(m, "PCM_STATE_PREPARED", SND_PCM_STATE_PREPARED);
	_EXPORT_INT(m, "PCM_STATE_RUNNING", SND_PCM_STATE_RUNNING);
	_EXPORT_INT(m, "PCM_STATE_XRUN", SND_PCM_STATE_XRUN);
	_EXPORT_INT(m, "PCM_STATE_DRAINING", SND_PCM_STATE_DRAINING);
	_EXPORT_INT(m, "PCM_STATE_PAUSED", SND_PCM_STATE_PAUSED);
	_EXPORT_INT(m, "PCM_STATE_SUSPENDED", SND_PCM_STATE_SUSPENDED);
	_EXPORT_INT(m, "PCM_STATE_DISCONNECTED", SND_PCM_STATE_DISCONNECTED);

	/* Mixer stuff */
	_EXPORT_INT(m, "MIXER_CHANNEL_ALL", MIXER_CHANNEL_ALL);

#if 0 // Omit for now - use case unknown
	_EXPORT_INT(m, "MIXER_SCHN_UNKNOWN", SND_MIXER_SCHN_UNKNOWN);
	_EXPORT_INT(m, "MIXER_SCHN_FRONT_LEFT", SND_MIXER_SCHN_FRONT_LEFT);
	_EXPORT_INT(m, "MIXER_SCHN_FRONT_RIGHT", SND_MIXER_SCHN_FRONT_RIGHT);
	_EXPORT_INT(m, "MIXER_SCHN_REAR_LEFT", SND_MIXER_SCHN_REAR_LEFT);
	_EXPORT_INT(m, "MIXER_SCHN_REAR_RIGHT", SND_MIXER_SCHN_REAR_RIGHT);
	_EXPORT_INT(m, "MIXER_SCHN_FRONT_CENTER", SND_MIXER_SCHN_FRONT_CENTER);
	_EXPORT_INT(m, "MIXER_SCHN_WOOFER", SND_MIXER_SCHN_WOOFER);
	_EXPORT_INT(m, "MIXER_SCHN_SIDE_LEFT", SND_MIXER_SCHN_SIDE_LEFT);
	_EXPORT_INT(m, "MIXER_SCHN_SIDE_RIGHT", SND_MIXER_SCHN_SIDE_RIGHT);
	_EXPORT_INT(m, "MIXER_SCHN_REAR_CENTER", SND_MIXER_SCHN_REAR_CENTER);
	_EXPORT_INT(m, "MIXER_SCHN_MONO", SND_MIXER_SCHN_MONO);
#endif

	_EXPORT_INT(m, "VOLUME_UNITS_PERCENTAGE", VOLUME_UNITS_PERCENTAGE)
	_EXPORT_INT(m, "VOLUME_UNITS_RAW", VOLUME_UNITS_RAW)
	_EXPORT_INT(m, "VOLUME_UNITS_DB", VOLUME_UNITS_DB)

#if PY_MAJOR_VERSION >= 3
	return m;
#endif
}
