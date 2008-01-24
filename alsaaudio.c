/*
 * alsaaudio -- Python interface to ALSA (Advanced Linux Sound Architecture).
 *              The standard audio API for Linux since kernel 2.0
 *
 * Contributed by Unispeed A/S (http://www.unispeed.com)
 * Primary author: Casper Wilstup (cwi@unispeed.dk)
 *
 *
 * (c) 2004 by Unispeed A/S. All rights reserved.
 *
 * License: CRNI's Python 1.6 license (or any later version of the same)
 *
 */

#include "Python.h"
#include <alsa/asoundlib.h>
#include <stdio.h>


typedef struct {
  PyObject_HEAD;
  int pcmtype;
  int pcmmode;
  char *pcmname;
  
  snd_pcm_t *handle;

  // Configurable parameters
  int channels;
  int rate;
  int format;
  snd_pcm_uframes_t periodsize;
  int framesize;

} alsapcm_t;

static PyTypeObject ALSAAudioType;

static PyObject *ALSAAudioError;

static int alsapcm_setup(alsapcm_t *self) {
  int res,dir;
  unsigned int val;
  snd_pcm_hw_params_t *hwparams;
  snd_pcm_uframes_t frames;

  if (self->handle) {
    snd_pcm_close(self->handle);
    self->handle = 0;
  }
  res = snd_pcm_open(&(self->handle),self->pcmname,self->pcmtype,self->pcmmode);
  if (res < 0) return res;

  snd_pcm_hw_params_alloca(&hwparams);
  res = snd_pcm_hw_params_any(self->handle, hwparams);
  if (res < 0) return res;

  snd_pcm_hw_params_alloca(&hwparams);

  /* Fill it in with default values. */
  snd_pcm_hw_params_any(self->handle, hwparams);
  snd_pcm_hw_params_set_access(self->handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(self->handle, hwparams, self->format);
  snd_pcm_hw_params_set_channels(self->handle, hwparams, self->channels);
  dir = 0;
  snd_pcm_hw_params_set_rate(self->handle, hwparams, self->rate, dir);
  snd_pcm_hw_params_set_period_size(self->handle, hwparams, self->periodsize, dir);
  snd_pcm_hw_params_set_periods(self->handle,hwparams,4,0);

  res = snd_pcm_hw_params(self->handle, hwparams);

  self->framesize = self->channels * snd_pcm_hw_params_get_sbits(hwparams)/8;
  return res;
}

/* ALSA PCM Object members */
static alsapcm_t *
newalsapcmobject(PyObject *args) {
  int res;
  alsapcm_t *self;
  int pcmtype=0;
  int pcmmode=0;
  char *pcmname = "default";
  if (!PyArg_ParseTuple(args,"|iis",&pcmtype,&pcmmode,&pcmname)) return NULL;
  if (!(self = (alsapcm_t *)PyObject_New(struct alsa_audio, &ALSAAudioType))) return NULL;

  if (pcmtype != SND_PCM_STREAM_PLAYBACK && pcmtype != SND_PCM_STREAM_CAPTURE) {
    PyErr_SetString(ALSAAudioError, "PCM type must be PCM_PLAYBACK (0) or PCM_CAPTUPE (1)");
    return NULL;
  }
  if (pcmmode < 0 || pcmmode > SND_PCM_ASYNC) {
    PyErr_SetString(ALSAAudioError, "Invalid PCM mode");
    return NULL;
  }
  self->pcmtype = pcmtype;
  self->pcmmode = pcmmode;
  self->pcmname = strdup(pcmname);
  
  self->channels = 2;
  self->rate = 44100;
  self->format = SND_PCM_FORMAT_S16_LE;
  self->periodsize = 32;

  self->handle = 0;
  res = alsapcm_setup(self);
  if (res < 0) {
    if (self->handle) {
      snd_pcm_close(self->handle);
      self->handle = 0;
    }
    PyErr_SetString(ALSAAudioError, snd_strerror(res));
    return NULL;    
  }
  return self;
}

static void alsapcm_dealloc(alsapcm_t *self) {
  if (self->handle) {
    snd_pcm_drain(self->handle);
    snd_pcm_close(self->handle);
  }
  PyObject_Del(self);
}

static PyObject *
alsapcm_dumpinfo(alsapcm_t *self, PyObject *args) {
  unsigned int val,val2;
  int dir;
  snd_pcm_uframes_t frames;
  snd_pcm_hw_params_t *hwparams;
  snd_pcm_hw_params_alloca(&hwparams);
  snd_pcm_hw_params_current(self->handle,hwparams);


  if (!PyArg_ParseTuple(args,"")) return NULL;

  printf("PCM handle name = '%s'\n", snd_pcm_name(self->handle));
  printf("PCM state = %s\n", snd_pcm_state_name(snd_pcm_state(self->handle)));

  snd_pcm_hw_params_get_access(hwparams, (snd_pcm_access_t *) &val);
  printf("access type = %s\n", snd_pcm_access_name((snd_pcm_access_t)val));

  snd_pcm_hw_params_get_format(hwparams, &val);
  printf("format = '%s' (%s)\n", 
	 snd_pcm_format_name((snd_pcm_format_t)val),
	 snd_pcm_format_description((snd_pcm_format_t)val));

  snd_pcm_hw_params_get_subformat(hwparams, (snd_pcm_subformat_t *)&val);
  printf("subformat = '%s' (%s)\n",
	 snd_pcm_subformat_name((snd_pcm_subformat_t)val),
	 snd_pcm_subformat_description((snd_pcm_subformat_t)val));

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

  snd_pcm_hw_params_get_buffer_size(hwparams, (snd_pcm_uframes_t *) &val);
  printf("buffer size = %d frames\n", val);

  snd_pcm_hw_params_get_periods(hwparams, &val, &dir);
  printf("periods per buffer = %d frames\n", val);

  snd_pcm_hw_params_get_rate_numden(hwparams, &val, &val2);
  printf("exact rate = %d/%d bps\n", val, val2);

  val = snd_pcm_hw_params_get_sbits(hwparams);
  printf("significant bits = %d\n", val);

  snd_pcm_hw_params_get_tick_time(hwparams, &val, &dir);
  printf("tick time = %d us\n", val);

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
alsapcm_pcmtype(alsapcm_t *self, PyObject *args) {
  if (!PyArg_ParseTuple(args,"")) return NULL;
  return PyInt_FromLong(self->pcmtype);
}

static PyObject *
alsapcm_pcmmode(alsapcm_t *self, PyObject *args) {
  if (!PyArg_ParseTuple(args,"")) return NULL;
  return PyInt_FromLong(self->pcmmode);
}

static PyObject *
alsapcm_pcmname(alsapcm_t *self, PyObject *args) {
  if (!PyArg_ParseTuple(args,"")) return NULL;
  return PyString_FromString(self->pcmname);
}

static PyObject *
alsapcm_setchannels(alsapcm_t *self, PyObject *args) {
  int channels;
  int res;
  if (!PyArg_ParseTuple(args,"i",&channels)) return NULL;
  self->channels = channels;
  res = alsapcm_setup(self);
  if (res < 0) {
    PyErr_SetString(ALSAAudioError, snd_strerror(res));
    return NULL;    
  }
  return PyInt_FromLong(channels);
}

static PyObject *
alsapcm_setrate(alsapcm_t *self, PyObject *args) {
  int rate;
  int res;
  if (!PyArg_ParseTuple(args,"i",&rate)) return NULL;
  self->rate = rate;
  res = alsapcm_setup(self);
  if (res < 0) {
    PyErr_SetString(ALSAAudioError, snd_strerror(res));
    return NULL;    
  }
  return PyInt_FromLong(self->rate);
}

static PyObject *
alsapcm_setformat(alsapcm_t *self, PyObject *args) {
  int format;
  int res;
  if (!PyArg_ParseTuple(args,"i",&format)) return NULL;
  self->format = format;
  res = alsapcm_setup(self);
  if (res < 0) {
    PyErr_SetString(ALSAAudioError, snd_strerror(res));
    return NULL;    
  }
  return PyInt_FromLong(self->format);
}

static PyObject *
alsapcm_setperiodsize(alsapcm_t *self, PyObject *args) {
  int periodsize;
  int res;
  if (!PyArg_ParseTuple(args,"i",&periodsize)) return NULL;
  self->periodsize = periodsize;
  res = alsapcm_setup(self);
  if (res < 0) {
    PyErr_SetString(ALSAAudioError, snd_strerror(res));
    return NULL;    
  }
  return PyInt_FromLong(self->periodsize);
}


static PyObject *
alsapcm_read(alsapcm_t *self, PyObject *args) {
  int res;
  int val,dir;
  char buffer[8000];
  snd_pcm_hw_params_t *hwparams;

  if (self->framesize * self->periodsize > 8000) {
    PyErr_SetString(ALSAAudioError,"Capture data too large. Try decreasing period size");
    return NULL;
  }

  if (!PyArg_ParseTuple(args,"")) return NULL;
  if (self->pcmtype != SND_PCM_STREAM_CAPTURE) {
    PyErr_SetString(ALSAAudioError,"Cannot read from playback PCM");
    return NULL;
  }

  res = snd_pcm_readi(self->handle, buffer, self->periodsize);
  if (res == -EPIPE) {
    /* EPIPE means overrun */
    snd_pcm_prepare(self->handle);
  }
  else if (res == -EAGAIN) {
    res = 0;
  }
  else if (res < 0) {
    PyErr_SetString(ALSAAudioError,snd_strerror(res));
    return NULL;
  } 

  return Py_BuildValue("is#",res,buffer,res*self->framesize);
}

static PyObject *alsapcm_write(alsapcm_t *self, PyObject *args) {
  char *data;
  int datalen;
  int res;
  if (!PyArg_ParseTuple(args,"s#",&data,&datalen)) return NULL;
  if (datalen%self->framesize) {
    PyErr_SetString(ALSAAudioError,"Data size must be a multipla of framesize");
    return NULL;
  }
  res = snd_pcm_writei(self->handle, data, datalen/self->framesize);
  if (res == -EPIPE) {
    /* EPIPE means underrun */
    snd_pcm_prepare(self->handle);
    snd_pcm_writei(self->handle, data, datalen/self->framesize);
    snd_pcm_writei(self->handle, data, datalen/self->framesize);
  }
  else if (res == -EAGAIN) {
    return PyInt_FromLong(0);
  }
  else if (res < 0) {
    PyErr_SetString(ALSAAudioError,snd_strerror(res));
    return NULL;
  }  
  return PyInt_FromLong(datalen);
}

/* Module functions */

static PyObject *
alsa_openpcm(PyObject *self, PyObject *args) {
  return (PyObject *)newalsapcmobject(args);
}


/* ALSA PCM Object Bureaucracy */

static PyMethodDef alsapcm_methods[] = {
  {"pcmtype", (PyCFunction)alsapcm_pcmtype, METH_VARARGS},
  {"pcmmode", (PyCFunction)alsapcm_pcmmode, METH_VARARGS},
  {"pcmname", (PyCFunction)alsapcm_pcmname, METH_VARARGS},
  {"setchannels", (PyCFunction)alsapcm_setchannels, METH_VARARGS},
  {"setrate", (PyCFunction)alsapcm_setrate, METH_VARARGS},
  {"setformat", (PyCFunction)alsapcm_setformat, METH_VARARGS},
  {"setperiodsize", (PyCFunction)alsapcm_setperiodsize, METH_VARARGS},

  {"dumpinfo", (PyCFunction)alsapcm_dumpinfo, METH_VARARGS},

  {"read", (PyCFunction)alsapcm_read, METH_VARARGS},
  {"write", (PyCFunction)alsapcm_write, METH_VARARGS},

  {NULL, NULL}
};

static PyObject *
alsapcm_getattr(alsapcm_t *self, char *name) {
  return Py_FindMethod(alsapcm_methods, (PyObject *)self, name);
}

static PyTypeObject ALSAAudioType = {
  PyObject_HEAD_INIT(&PyType_Type)
  0,
  "alsaaudio.pcm",
  sizeof(alsapcm_t),
  0,
  /* methods */
  (destructor) alsapcm_dealloc, 
  0,
  (getattrfunc)alsapcm_getattr,
  0,
  0,
  0,
};



/* Initialization */

static PyMethodDef alsaaudio_methods[] = {
  { "openpcm", alsa_openpcm, METH_VARARGS },
  { 0, 0 },
};

#define _EXPORT_CONSTANT(mod, name) \
  if (PyModule_AddIntConstant(mod, #name, (long) (name)) == -1) return;
#define _EXPORT_INT(mod, name, value) \
  if (PyModule_AddIntConstant(mod, name, (long) value) == -1) return;

void initalsaaudio(void) {
  PyObject *m;
  m = Py_InitModule("alsaaudio",alsaaudio_methods);

  ALSAAudioError = PyErr_NewException("alsaaudio.ALSAAudioError", NULL, NULL);
  if (ALSAAudioError) {
    /* Each call to PyModule_AddObject decrefs it; compensate: */
    Py_INCREF(ALSAAudioError);
    Py_INCREF(ALSAAudioError);
    PyModule_AddObject(m, "error", ALSAAudioError);
    PyModule_AddObject(m, "ALSAAudioError", ALSAAudioError);
  }


  _EXPORT_INT(m,"PCM_PLAYBACK",SND_PCM_STREAM_PLAYBACK);
  _EXPORT_INT(m,"PCM_CAPTURE",SND_PCM_STREAM_CAPTURE);

  _EXPORT_INT(m,"PCM_NONBLOCK",SND_PCM_NONBLOCK);
  _EXPORT_INT(m,"PCM_ASYNC",SND_PCM_ASYNC);

  /* PCM Formats */
  _EXPORT_INT(m,"PCM_FORMAT_S8",SND_PCM_FORMAT_S8);
  _EXPORT_INT(m,"PCM_FORMAT_U8",SND_PCM_FORMAT_U8);
  _EXPORT_INT(m,"PCM_FORMAT_S16_LE",SND_PCM_FORMAT_S16_LE);
  _EXPORT_INT(m,"PCM_FORMAT_S16_BE",SND_PCM_FORMAT_S16_BE);
  _EXPORT_INT(m,"PCM_FORMAT_U16_LE",SND_PCM_FORMAT_U16_LE);
  _EXPORT_INT(m,"PCM_FORMAT_U16_BE",SND_PCM_FORMAT_U16_BE);
  _EXPORT_INT(m,"PCM_FORMAT_S24_LE",SND_PCM_FORMAT_S24_LE);
  _EXPORT_INT(m,"PCM_FORMAT_S24_BE",SND_PCM_FORMAT_S24_BE);
  _EXPORT_INT(m,"PCM_FORMAT_U24_LE",SND_PCM_FORMAT_U24_LE);
  _EXPORT_INT(m,"PCM_FORMAT_U24_BE",SND_PCM_FORMAT_U24_BE);
  _EXPORT_INT(m,"PCM_FORMAT_S32_LE",SND_PCM_FORMAT_S32_LE);
  _EXPORT_INT(m,"PCM_FORMAT_S32_BE",SND_PCM_FORMAT_S32_BE);
  _EXPORT_INT(m,"PCM_FORMAT_U32_LE",SND_PCM_FORMAT_U32_LE);
  _EXPORT_INT(m,"PCM_FORMAT_U32_BE",SND_PCM_FORMAT_U32_BE);
  _EXPORT_INT(m,"PCM_FORMAT_FLOAT_LE",SND_PCM_FORMAT_FLOAT_LE);
  _EXPORT_INT(m,"PCM_FORMAT_FLOAT_BE",SND_PCM_FORMAT_FLOAT_BE);
  _EXPORT_INT(m,"PCM_FORMAT_FLOAT64_LE",SND_PCM_FORMAT_FLOAT64_LE);
  _EXPORT_INT(m,"PCM_FORMAT_FLOAT64_BE",SND_PCM_FORMAT_FLOAT64_BE);
  _EXPORT_INT(m,"PCM_FORMAT_MU_LAW",SND_PCM_FORMAT_MU_LAW);
  _EXPORT_INT(m,"PCM_FORMAT_A_LAW",SND_PCM_FORMAT_A_LAW);
  _EXPORT_INT(m,"PCM_FORMAT_IMA_ADPCM",SND_PCM_FORMAT_IMA_ADPCM);
  _EXPORT_INT(m,"PCM_FORMAT_MPEG",SND_PCM_FORMAT_MPEG);
  _EXPORT_INT(m,"PCM_FORMAT_GSM",SND_PCM_FORMAT_GSM);

}
