/*
 * alsaaudio -- Python interface to ALSA (Advanced Linux Sound Architecture).
 *              The standard audio API for Linux since kernel 2.6
 *
 * Contributed by Unispeed A/S (http://www.unispeed.com)
 * Author: Casper Wilstup (cwi@unispeed.dk)
 *
 * License: Python Software Foundation License
 *
 */

#include "Python.h"
#include <alsa/asoundlib.h>
#include <stdio.h>

PyDoc_STRVAR(alsaaudio_module_doc,
	     "This modules provides support for the ALSA audio API.\n"
	     "\n"
	     "To control the PCM device, use the PCM class, Mixers\n"
	     "are controlled using the Mixer class.\n"
	     "\n"
	     "The following functions are also provided:\n"
	     "mixers() -- Return a list of available mixer names\n"
	     );

typedef struct {
  PyObject_HEAD;
  int pcmtype;
  int pcmmode;
  char *cardname;
  
  snd_pcm_t *handle;

  // Configurable parameters
  int channels;
  int rate;
  int format;
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
  long pmin;
  long pmax;
  long cmin;
  long cmax;
  snd_mixer_t *handle;

} alsamixer_t;

static PyObject *ALSAAudioError;


/******************************************/
/* PCM object wrapper                   */
/******************************************/

static PyTypeObject ALSAPCMType;

static int alsapcm_setup(alsapcm_t *self) {
  int res,dir;
  snd_pcm_hw_params_t *hwparams;

  if (self->handle) {
    snd_pcm_close(self->handle);
    self->handle = 0;
  }
  res = snd_pcm_open(&(self->handle),self->cardname,self->pcmtype,self->pcmmode);
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

static PyObject *
alsapcm_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  int res;
  alsapcm_t *self;
  int pcmtype=0;
  int pcmmode=0;
  char *cardname = "default";
  if (!PyArg_ParseTuple(args,"|iis",&pcmtype,&pcmmode,&cardname)) return NULL;
  if (!(self = (alsapcm_t *)PyObject_New(alsapcm_t, &ALSAPCMType))) return NULL;

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
  self->cardname = strdup(cardname);
  
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
  return (PyObject *)self;
}

static void alsapcm_dealloc(alsapcm_t *self) {
  if (self->handle) {
    snd_pcm_drain(self->handle);
    snd_pcm_close(self->handle);
  }
  free(self->cardname);
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
alsapcm_cardname(alsapcm_t *self, PyObject *args) {
  if (!PyArg_ParseTuple(args,"")) return NULL;
  return PyString_FromString(self->cardname);
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
  char buffer[8000];

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
  return PyInt_FromLong(res);
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

  {"dumpinfo", (PyCFunction)alsapcm_dumpinfo, METH_VARARGS},

  {"read", (PyCFunction)alsapcm_read, METH_VARARGS},
  {"write", (PyCFunction)alsapcm_write, METH_VARARGS},

  {NULL, NULL}
};

static PyObject *
alsapcm_getattr(alsapcm_t *self, char *name) {
  return Py_FindMethod(alsapcm_methods, (PyObject *)self, name);
}

static PyTypeObject ALSAPCMType = {
  PyObject_HEAD_INIT(&PyType_Type)
  0,                              /*ob_size*/
  "alsaaudio.pcm",                /*tp_name*/
  sizeof(alsapcm_t),              /*tp_basicsize*/
  0,                              /*tp_itemsize*/
  /* methods */    
  (destructor) alsapcm_dealloc,   /*tp_dealloc*/
  0,                              /*print*/
  (getattrfunc)alsapcm_getattr,   /*tp_getattr*/
  0,                              /*tp_setattr*/
  0,                              /*tp_compare*/ 
  0,                              /*tp_repr*/
  0,                              /*tp_as_number*/
  0,                              /*tp_as_sequence*/
  0,                              /*tp_as_mapping*/
  0,                              /*tp_hash*/
  0,                              /*tp_call*/
  0,                              /*tp_str*/
  0,                              /*tp_getattro*/
  0,                              /*tp_setattro*/
  0,                              /*tp_as_buffer*/
  Py_TPFLAGS_DEFAULT,             /*tp_flags*/
  "ALSA PCM device",              /*tp_doc*/
};


/******************************************/
/* Mixer object wrapper                   */
/******************************************/

static PyTypeObject ALSAMixerType;

#define MIXER_CAP_VOLUME            0x0001
#define MIXER_CAP_VOLUME_JOINED     0x0002
#define MIXER_CAP_PVOLUME           0x0004
#define MIXER_CAP_PVOLUME_JOINED    0x0008
#define MIXER_CAP_CVOLUME           0x0010
#define MIXER_CAP_CVOLUME_JOINED    0x0020

#define MIXER_CAP_SWITCH            0x0001
#define MIXER_CAP_SWITCH_JOINED     0x0002
#define MIXER_CAP_PSWITCH           0x0004
#define MIXER_CAP_PSWITCH_JOINED    0x0008
#define MIXER_CAP_CSWITCH           0x0010
#define MIXER_CAP_CSWITCH_JOINED    0x0020
#define MIXER_CAP_CSWITCH_EXCLUSIVE 0x0040

#define MIXER_CHANNEL_ALL -1

int
alsamixer_gethandle(char *cardname, snd_mixer_t **handle) {
  int err;
  if ((err = snd_mixer_open(handle, 0)) < 0) return err;
  if ((err = snd_mixer_attach(*handle, cardname)) < 0) return err;
  if ((err = snd_mixer_selem_register(*handle, NULL, NULL)) < 0) return err;
  if ((err = snd_mixer_load(*handle)) < 0) return err;

  return 0;
}

static PyObject *
alsamixer_list(PyObject *self, PyObject *args) {
  snd_mixer_t *handle;
  snd_mixer_selem_id_t *sid;
  snd_mixer_elem_t *elem;
  int err;
  char *cardname = "default";
  PyObject *result = PyList_New(0);

  if (!PyArg_ParseTuple(args,"|s",&cardname)) return NULL;
	
  snd_mixer_selem_id_alloca(&sid);
  err = alsamixer_gethandle(cardname,&handle);
  if (err < 0) {
    PyErr_SetString(ALSAAudioError,snd_strerror(err));
    snd_mixer_close(handle);
    return NULL;
  }
  for (elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem)) {
    PyObject *mixer;
    snd_mixer_selem_get_id(elem, sid);
    mixer = PyString_FromString(snd_mixer_selem_id_get_name(sid));
    PyList_Append(result,mixer);
    Py_DECREF(mixer);
  }
  snd_mixer_close(handle);

  return result;
}

static snd_mixer_elem_t *
alsamixer_find_elem(snd_mixer_t *handle, char *control, int id) {
  snd_mixer_selem_id_t *sid;

  snd_mixer_selem_id_alloca(&sid);
  snd_mixer_selem_id_set_index(sid, id);
  snd_mixer_selem_id_set_name(sid, control);
  return snd_mixer_find_selem(handle, sid);
}

static PyObject *
alsamixer_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  alsamixer_t *self;
  int err;
  char *cardname = "default";
  char *control = "Master";
  int id = 0;
  snd_mixer_elem_t *elem;
  int channel;
  
  if (!PyArg_ParseTuple(args,"|sis",&control,&id,&cardname)) return NULL;
  if (!(self = (alsamixer_t *)PyObject_New(alsamixer_t, &ALSAMixerType))) return NULL;

  err = alsamixer_gethandle(cardname,&self->handle);
  if (err<0) {
    PyErr_SetString(ALSAAudioError,snd_strerror(err));
    return NULL;
  }    
  self->cardname = strdup(cardname);
  self->controlname = strdup(control);
  self->controlid = id;

  elem = alsamixer_find_elem(self->handle,control,id);
  if (!elem) {
    char errtext[128];
    sprintf(errtext,"Unable to find mixer control '%s',%i",self->controlname,self->controlid);
    snd_mixer_close(self->handle);
    PyErr_SetString(ALSAAudioError,errtext);
    return NULL;
  }
  /* Determine mixer capabilities */
  self->volume_cap = self->switch_cap = 0;
  if (snd_mixer_selem_has_common_volume(elem)) {
    self->volume_cap |= MIXER_CAP_VOLUME;
    if (snd_mixer_selem_has_playback_volume_joined(elem)) self->volume_cap |= MIXER_CAP_VOLUME_JOINED;
  }
  else {
    if (snd_mixer_selem_has_playback_volume(elem)) {
      self->volume_cap |= MIXER_CAP_PVOLUME;
      if (snd_mixer_selem_has_playback_volume_joined(elem)) self->volume_cap |= MIXER_CAP_PVOLUME_JOINED;
    }
    if (snd_mixer_selem_has_capture_volume(elem)) {
      self->volume_cap |= MIXER_CAP_CVOLUME;
      if (snd_mixer_selem_has_capture_volume_joined(elem)) self->volume_cap |= MIXER_CAP_CVOLUME_JOINED;
    }
  }

  if (snd_mixer_selem_has_common_switch(elem)) {
    self->switch_cap |= MIXER_CAP_SWITCH;
    if (snd_mixer_selem_has_playback_switch_joined(elem)) self->switch_cap |= MIXER_CAP_SWITCH_JOINED;
  }
  else {
    if (snd_mixer_selem_has_playback_switch(elem)) {
      self->switch_cap |= MIXER_CAP_PSWITCH;
      if (snd_mixer_selem_has_playback_switch_joined(elem)) self->switch_cap |= MIXER_CAP_PSWITCH_JOINED;
    }
    if (snd_mixer_selem_has_capture_switch(elem)) {
      self->switch_cap |= MIXER_CAP_CSWITCH;
      if (snd_mixer_selem_has_capture_switch_joined(elem)) self->switch_cap |= MIXER_CAP_CSWITCH_JOINED;
      if (snd_mixer_selem_has_capture_switch_exclusive(elem)) self->switch_cap |= MIXER_CAP_CSWITCH_EXCLUSIVE;
    }
  }
  self->pchannels = 0;
  if (self->volume_cap | MIXER_CAP_PVOLUME || self->switch_cap | MIXER_CAP_PSWITCH) {
    if (snd_mixer_selem_is_playback_mono(elem)) self->pchannels = 1;
    else {
      for (channel=0; channel <= SND_MIXER_SCHN_LAST; channel++) {
	if (snd_mixer_selem_has_playback_channel(elem, channel)) self->pchannels++;
	else break;
      }
    }
  }
  self->cchannels = 0;
  if (self->volume_cap | MIXER_CAP_CVOLUME || self->switch_cap | MIXER_CAP_CSWITCH) {
    if (snd_mixer_selem_is_capture_mono(elem)) self->cchannels = 1;
    else {
      for (channel=0; channel <= SND_MIXER_SCHN_LAST; channel++) {
	if (snd_mixer_selem_has_capture_channel(elem, channel)) self->cchannels++;
	else break;
      }
    }
  }
  snd_mixer_selem_get_playback_volume_range(elem, &self->pmin, &self->pmax);
  snd_mixer_selem_get_capture_volume_range(elem, &self->cmin, &self->cmax);
  return (PyObject *)self;
}

static void alsamixer_dealloc(alsamixer_t *self) {
  if (self->handle) {
    snd_mixer_close(self->handle);
    free(self->cardname);
    free(self->controlname);
    self->handle = 0;
  }
  PyObject_Del(self);
}

static PyObject *
alsamixer_cardname(alsamixer_t *self, PyObject *args) {
  if (!PyArg_ParseTuple(args,"")) return NULL;
  return PyString_FromString(self->cardname);
}

static PyObject *
alsamixer_mixer(alsamixer_t *self, PyObject *args) {
  if (!PyArg_ParseTuple(args,"")) return NULL;
  return PyString_FromString(self->controlname);
}

static PyObject *
alsamixer_mixerid(alsamixer_t *self, PyObject *args) {
  if (!PyArg_ParseTuple(args,"")) return NULL;
  return PyInt_FromLong(self->controlid);
}

static PyObject *
alsamixer_volumecap(alsamixer_t *self, PyObject *args) {
  PyObject *result;
  if (!PyArg_ParseTuple(args,"")) return NULL;
  result = PyList_New(0);
  if (self->volume_cap&MIXER_CAP_VOLUME) 
    PyList_Append(result,PyString_FromString("Volume"));
  if (self->volume_cap&MIXER_CAP_VOLUME_JOINED) 
    PyList_Append(result,PyString_FromString("Joined Volume"));
  if (self->volume_cap&MIXER_CAP_PVOLUME) 
    PyList_Append(result,PyString_FromString("Playback Volume"));
  if (self->volume_cap&MIXER_CAP_PVOLUME_JOINED) 
    PyList_Append(result,PyString_FromString("Joined Playback Volume"));
  if (self->volume_cap&MIXER_CAP_CVOLUME) 
    PyList_Append(result,PyString_FromString("Capture Volume"));
  if (self->volume_cap&MIXER_CAP_CVOLUME_JOINED) 
    PyList_Append(result,PyString_FromString("Joined Capture Volume"));
   
  return result;
}
static PyObject *
alsamixer_switchcap(alsamixer_t *self, PyObject *args) {
  PyObject *result;
  if (!PyArg_ParseTuple(args,"")) return NULL;
  result = PyList_New(0);
  if (self->volume_cap&MIXER_CAP_SWITCH) 
    PyList_Append(result,PyString_FromString("Mute"));
  if (self->volume_cap&MIXER_CAP_SWITCH_JOINED) 
    PyList_Append(result,PyString_FromString("Joined Mute"));
  if (self->volume_cap&MIXER_CAP_PSWITCH) 
    PyList_Append(result,PyString_FromString("Playback Mute"));
  if (self->volume_cap&MIXER_CAP_PSWITCH_JOINED) 
    PyList_Append(result,PyString_FromString("Joined Playback Mute"));
  if (self->volume_cap&MIXER_CAP_CSWITCH) 
    PyList_Append(result,PyString_FromString("Capture Mute"));
  if (self->volume_cap&MIXER_CAP_CSWITCH_JOINED) 
    PyList_Append(result,PyString_FromString("Joined Capture Mute"));
  if (self->volume_cap&MIXER_CAP_CSWITCH_EXCLUSIVE) 
    PyList_Append(result,PyString_FromString("Capture Exclusive"));
  return result;
}

static int alsamixer_getpercentage(long min, long max, long value) {
  /* Convert from number in range to percentage */
  int range = max - min;
  int tmp;

  if (range == 0) return 0;
  value -= min;
  tmp = rint((double)value/(double)range * 100);
  return tmp;

}

static long alsamixer_getphysvolume(long min, long max, int percentage) {
  /* Convert from percentage to number in range */
  int range = max - min;
  int tmp;

  if (range == 0) return 0;
  tmp = rint((double)range * ((double)percentage*.01)) + min;
  return tmp;
}

static PyObject *
alsamixer_getvolume(alsamixer_t *self, PyObject *args) {
  snd_mixer_elem_t *elem;
  int direction;
  int channel;
  long ival;
  char *dirstr = 0;
  PyObject *result;

  if (!PyArg_ParseTuple(args,"|s",&dirstr)) return NULL;

  elem = alsamixer_find_elem(self->handle,self->controlname,self->controlid);

  if (!dirstr) {
    if (self->pchannels) direction = 0;
    else direction = 1;
  }
  else if (strcasecmp(dirstr,"playback")==0) direction = 0;
  else if (strcasecmp(dirstr,"capture")==0) direction = 1;
  else {
    PyErr_SetString(ALSAAudioError,"Invalid direction argument for mixer");
    return NULL;
  }
  result = PyList_New(0);
  for (channel = 0; channel <= SND_MIXER_SCHN_LAST; channel++) {
    if (direction == 0 && snd_mixer_selem_has_playback_channel(elem, channel)) {
      snd_mixer_selem_get_playback_volume(elem, channel, &ival);
      PyList_Append(result,PyInt_FromLong(alsamixer_getpercentage(self->pmin,self->pmax,ival)));
    }
    else if (direction == 1 && snd_mixer_selem_has_capture_channel(elem, channel)
	     && snd_mixer_selem_has_capture_volume(elem)) {
      snd_mixer_selem_get_capture_volume(elem, channel, &ival);
      PyList_Append(result,PyInt_FromLong(alsamixer_getpercentage(self->cmin,self->cmax,ival)));
    }
  }
  return result;
}

static PyObject *
alsamixer_getmute(alsamixer_t *self, PyObject *args) {
  snd_mixer_elem_t *elem;
  int i;
  int ival;
  PyObject *result;
  if (!PyArg_ParseTuple(args,"")) return NULL;

  elem = alsamixer_find_elem(self->handle,self->controlname,self->controlid);
  if (!snd_mixer_selem_has_playback_switch(elem)) {
    PyErr_SetString(ALSAAudioError,"Mixer has no mute switch");
    return NULL;
  }    
  result = PyList_New(0);
  for (i = 0; i <= SND_MIXER_SCHN_LAST; i++) {
    if (snd_mixer_selem_has_playback_channel(elem, i)) {
      snd_mixer_selem_get_playback_switch(elem, i, &ival);
      PyList_Append(result,PyInt_FromLong(!ival));
    }
  }
  return result;
}

static PyObject *
alsamixer_getrec(alsamixer_t *self, PyObject *args) {
  snd_mixer_elem_t *elem;
  int i;
  int ival;
  PyObject *result;
  if (!PyArg_ParseTuple(args,"")) return NULL;

  elem = alsamixer_find_elem(self->handle,self->controlname,self->controlid);
  if (!snd_mixer_selem_has_capture_switch(elem)) {
    PyErr_SetString(ALSAAudioError,"Mixer has no record switch");
    return NULL;
  }    
  result = PyList_New(0);
  for (i = 0; i <= SND_MIXER_SCHN_LAST; i++) {
    if (snd_mixer_selem_has_capture_channel(elem, i)) {
      snd_mixer_selem_get_capture_switch(elem, i, &ival);
      PyList_Append(result,PyInt_FromLong(!ival));
    }
  }
  return result;
}

static PyObject *
alsamixer_setvolume(alsamixer_t *self, PyObject *args) {
  snd_mixer_elem_t *elem;
  int direction;
  int i;
  long volume;
  int physvolume;
  char *dirstr = 0;
  int channel = MIXER_CHANNEL_ALL;
  int done = 0;

  if (!PyArg_ParseTuple(args,"l|is",&volume,&channel,&dirstr)) return NULL;
  if (volume < 0 || volume > 100) {
    PyErr_SetString(ALSAAudioError,"Volume must be between 0 and 100");
    return NULL;
  }
  
  elem = alsamixer_find_elem(self->handle,self->controlname,self->controlid);

  if (!dirstr) {
    if (self->pchannels) direction = 0;
    else direction = 1;
  }
  else if (strcasecmp(dirstr,"playback")==0) direction = 0;
  else if (strcasecmp(dirstr,"capture")==0) direction = 1;
  else {
    PyErr_SetString(ALSAAudioError,"Invalid direction argument. Use 'playback' or 'capture'");
    return NULL;
  }
  for (i = 0; i <= SND_MIXER_SCHN_LAST; i++) {
    if (channel == -1 || channel == i) {
      if (direction == 0 && snd_mixer_selem_has_playback_channel(elem, i)) {
	physvolume = alsamixer_getphysvolume(self->pmin,self->pmax,volume);
	snd_mixer_selem_set_playback_volume(elem, i, physvolume);
	done++;
      }
      else if (direction == 1 && snd_mixer_selem_has_capture_channel(elem, channel)
	       && snd_mixer_selem_has_capture_volume(elem)) {
	physvolume = alsamixer_getphysvolume(self->cmin,self->cmax,volume);
	snd_mixer_selem_set_capture_volume(elem, i, physvolume);
	done++;
      }
    }
  }
  if(!done) {
    PyErr_SetString(ALSAAudioError,"No such channel");
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *
alsamixer_setmute(alsamixer_t *self, PyObject *args) {
  snd_mixer_elem_t *elem;
  int i;
  int mute = 0;
  int done = 0;
  int channel = MIXER_CHANNEL_ALL;
  if (!PyArg_ParseTuple(args,"i|i",&mute,&channel)) return NULL;

  elem = alsamixer_find_elem(self->handle,self->controlname,self->controlid);
  if (!snd_mixer_selem_has_playback_switch(elem)) {
    PyErr_SetString(ALSAAudioError,"Mixer has no mute switch");
    return NULL;
  }    
  for (i = 0; i <= SND_MIXER_SCHN_LAST; i++) {
    if (channel == MIXER_CHANNEL_ALL || channel == i) {
      if (snd_mixer_selem_has_playback_channel(elem, i)) {
	snd_mixer_selem_set_playback_switch(elem, i, !mute);
	done++;
      }
    }
  }
  if (!done) {
    PyErr_SetString(ALSAAudioError,"Invalid channel number");
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *
alsamixer_setrec(alsamixer_t *self, PyObject *args) {
  snd_mixer_elem_t *elem;
  int i;
  int rec = 0;
  int done = 0;
  int channel = MIXER_CHANNEL_ALL;
  if (!PyArg_ParseTuple(args,"i|i",&rec,&channel)) return NULL;

  elem = alsamixer_find_elem(self->handle,self->controlname,self->controlid);
  if (!snd_mixer_selem_has_capture_switch(elem)) {
    PyErr_SetString(ALSAAudioError,"Mixer has no record switch");
    return NULL;
  }    
  for (i = 0; i <= SND_MIXER_SCHN_LAST; i++) {
    if (channel == MIXER_CHANNEL_ALL || channel == i) {
      if (snd_mixer_selem_has_capture_channel(elem, i)) {
	snd_mixer_selem_set_playback_switch(elem, i, rec);
	done++;
      }
    }
  }
  if (!done) {
    PyErr_SetString(ALSAAudioError,"Invalid channel number");
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static PyMethodDef alsamixer_methods[] = {
  {"cardname", (PyCFunction)alsamixer_cardname, METH_VARARGS},
  {"mixer", (PyCFunction)alsamixer_mixer, METH_VARARGS},
  {"mixerid", (PyCFunction)alsamixer_mixerid, METH_VARARGS},
  {"switchcap", (PyCFunction)alsamixer_switchcap, METH_VARARGS},
  {"volumecap", (PyCFunction)alsamixer_volumecap, METH_VARARGS},
  {"getvolume", (PyCFunction)alsamixer_getvolume, METH_VARARGS},
  {"getmute", (PyCFunction)alsamixer_getmute, METH_VARARGS},
  {"getrec", (PyCFunction)alsamixer_getrec, METH_VARARGS},
  {"setvolume", (PyCFunction)alsamixer_setvolume, METH_VARARGS},
  {"setmute", (PyCFunction)alsamixer_setmute, METH_VARARGS},
  {"setrec", (PyCFunction)alsamixer_setrec, METH_VARARGS},
  {NULL, NULL}
};


static PyObject *
alsamixer_getattr(alsapcm_t *self, char *name) {
  return Py_FindMethod(alsamixer_methods, (PyObject *)self, name);
}

static PyTypeObject ALSAMixerType = {
  PyObject_HEAD_INIT(&PyType_Type)
  0,                              /*ob_size*/
  "alsaaudio.mixer",              /*tp_name*/
  sizeof(alsamixer_t),            /*tp_basicsize*/
  0,                              /*tp_itemsize*/
  /* methods */    
  (destructor) alsamixer_dealloc, /*tp_dealloc*/
  0,                              /*print*/
  (getattrfunc)alsamixer_getattr, /*tp_getattr*/
  0,                              /*tp_setattr*/
  0,                              /*tp_compare*/ 
  0,                              /*tp_repr*/
  0,                              /*tp_as_number*/
  0,                              /*tp_as_sequence*/
  0,                              /*tp_as_mapping*/
  0,                              /*tp_hash*/
  0,                              /*tp_call*/
  0,                              /*tp_str*/
  0,                              /*tp_getattro*/
  0,                              /*tp_setattro*/
  0,                              /*tp_as_buffer*/
  Py_TPFLAGS_DEFAULT,             /*tp_flags*/
  "ALSA Mixer Control",           /*tp_doc*/
};


/******************************************/
/* Module initialization                  */
/******************************************/

static PyMethodDef alsaaudio_methods[] = {
  { "mixers", alsamixer_list, METH_VARARGS },
  { 0, 0 },
};

#define _EXPORT_INT(mod, name, value) \
  if (PyModule_AddIntConstant(mod, name, (long) value) == -1) return;

void initalsaaudio(void) {
  PyObject *m;
  ALSAPCMType.tp_new = alsapcm_new;
  ALSAMixerType.tp_new = alsamixer_new;
  m = Py_InitModule3("alsaaudio",alsaaudio_methods,alsaaudio_module_doc);

  ALSAAudioError = PyErr_NewException("alsaaudio.ALSAAudioError", NULL, NULL);
  if (ALSAAudioError) {
    /* Each call to PyModule_AddObject decrefs it; compensate: */

    Py_INCREF(&ALSAPCMType);
    PyModule_AddObject(m,"PCM",(PyObject *)&ALSAPCMType);

    Py_INCREF(&ALSAMixerType);
    PyModule_AddObject(m,"Mixer",(PyObject *)&ALSAMixerType);

    Py_INCREF(ALSAAudioError);
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

  /* Mixer stuff */
  _EXPORT_INT(m,"MIXER_CHANNEL_ALL",MIXER_CHANNEL_ALL);

}
