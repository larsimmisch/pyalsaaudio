/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */ 

/*
 * alsaaudio -- Python interface to ALSA (Advanced Linux Sound Architecture).
 *              The standard audio API for Linux since kernel 2.6
 *
 * Contributed by Unispeed A/S (http://www.unispeed.com)
 * Author: Casper Wilstup (cwi@aves.dk)
 *
 * Bug fixes and maintenance by Lars Immisch <lars@ibp.de>
 * 
 * License: Python Software Foundation License
 *
 */

#include "Python.h"
#if PY_MAJOR_VERSION < 3 && PY_MINOR_VERSION < 6
#include "stringobject.h"
#define PyUnicode_FromString PyString_FromString
#endif
#include <alsa/asoundlib.h>
#include <stdio.h>

PyDoc_STRVAR(alsaaudio_module_doc,
             "This modules provides support for the ALSA audio API.\n"
             "\n"
             "To control the PCM device, use the PCM class, Mixers\n"
             "are controlled using the Mixer class.\n"
             "\n"
             "The following functions are also provided:\n"
             "mixers() -- Return a list of available mixer names\n");

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

/* Translate a card id to a ALSA cardname 

   Returns a newly allocated string.
*/
char *translate_cardname(char *name)
{
    static char dflt[] = "default";
    char *full = NULL;
    
    if (!name || !strcmp(name, dflt))
        return strdup(dflt);
    
    // If we find a colon, we assume it is a real ALSA cardname
    if (strchr(name, ':'))
        return strdup(name);

    full = malloc(strlen("default:CARD=") + strlen(name) + 1);  
    sprintf(full, "default:CARD=%s", name);

    return full;
}

/* Translate a card index to a ALSA cardname 

   Returns a newly allocated string.
*/
char *translate_cardidx(int idx)
{
    char name[32];

    sprintf(name, "hw:%d", idx);

    return strdup(name);
}

/******************************************/
/* PCM object wrapper                   */
/******************************************/

static PyTypeObject ALSAPCMType;
static PyObject *ALSAAudioError;

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
        char name[32];
        int err;
        PyObject *item;        

        /* One would be tempted to think that snd_card_get_name returns a name
           that is actually meaningful for any further operations.
           
           Not in ALSA land. Here we need the id, not the name */

        sprintf(name, "hw:%d", card);
        if ((err = snd_ctl_open(&handle, name, 0)) < 0) {
            PyErr_SetString(ALSAAudioError,snd_strerror(err));
            return NULL;
        }
        if ((err = snd_ctl_card_info(handle, info)) < 0) {
            PyErr_SetString(ALSAAudioError,snd_strerror(err));
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

PyDoc_STRVAR(cards_doc,
"cards()\n\
\n\
List the available card ids.");

static int alsapcm_setup(alsapcm_t *self) 
{
    int res,dir;
    unsigned int val;
    snd_pcm_format_t fmt;
    snd_pcm_uframes_t frames;
    snd_pcm_hw_params_t *hwparams;
    
    /* Allocate a hwparam structure on the stack, 
       and fill it with configuration space */
    snd_pcm_hw_params_alloca(&hwparams);
    res = snd_pcm_hw_params_any(self->handle, hwparams);
    if (res < 0) 
        return res;
    
    /* Fill it with default values. 

       We don't care if any of this fails - we'll read the actual values 
       back out.
     */
    snd_pcm_hw_params_any(self->handle, hwparams);
    snd_pcm_hw_params_set_access(self->handle, hwparams, 
                                 SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(self->handle, hwparams, self->format);
    snd_pcm_hw_params_set_channels(self->handle, hwparams, 
                                   self->channels);

    dir = 0;
    snd_pcm_hw_params_set_rate(self->handle, hwparams, self->rate, dir);
    snd_pcm_hw_params_set_period_size(self->handle, hwparams, 
                                      self->periodsize, dir);
    snd_pcm_hw_params_set_periods(self->handle, hwparams, 4, 0);
    
    /* Write it to the device */
    res = snd_pcm_hw_params(self->handle, hwparams);
    
    /* Query current settings. These may differ from the requested values,
       which should therefore be sync'ed with actual values */
    snd_pcm_hw_params_current(self->handle, hwparams);

    snd_pcm_hw_params_get_format(hwparams, &fmt); self->format = fmt;
    snd_pcm_hw_params_get_channels(hwparams, &val); self->channels = val;
    snd_pcm_hw_params_get_rate(hwparams, &val, &dir); self->rate = val;
    snd_pcm_hw_params_get_period_size(hwparams, &frames, &dir); 
    self->periodsize = (int) frames;
    
    self->framesize = self->channels * snd_pcm_hw_params_get_sbits(hwparams)/8;
    
    return res;
}

static PyObject *
alsapcm_new(PyTypeObject *type, PyObject *args, PyObject *kwds) 
{
    int res;
    alsapcm_t *self;
    int pcmtype = SND_PCM_STREAM_PLAYBACK;
    int pcmmode = 0;
    char *kw[] = { "type", "mode", "card", NULL };
    char *cardname = NULL;
    
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iiz", kw,
                                     &pcmtype, &pcmmode, &cardname)) 
        return NULL;
    
    if (!(self = (alsapcm_t *)PyObject_New(alsapcm_t, &ALSAPCMType))) 
        return NULL;
    
    if (pcmtype != SND_PCM_STREAM_PLAYBACK && 
        pcmtype != SND_PCM_STREAM_CAPTURE) 
    {
        PyErr_SetString(ALSAAudioError, "PCM type must be PCM_PLAYBACK (0) "
                        "or PCM_CAPTURE (1)");
        return NULL;
    }
    if (pcmmode < 0 || pcmmode > SND_PCM_ASYNC) {
        PyErr_SetString(ALSAAudioError, "Invalid PCM mode");
        return NULL;
    }
    self->handle = 0;
    self->pcmtype = pcmtype;
    self->pcmmode = pcmmode;
    self->cardname = translate_cardname(cardname);
    self->channels = 2;
    self->rate = 44100;
    self->format = SND_PCM_FORMAT_S16_LE;
    self->periodsize = 32;
    
    res = snd_pcm_open(&(self->handle), self->cardname, self->pcmtype,
                       self->pcmmode);
    
    if (res >= 0)
        res = alsapcm_setup(self);
    
    if (res < 0) 
    {
        if (self->handle) 
        {
            snd_pcm_close(self->handle);
            self->handle = 0;
        }
        PyErr_SetString(ALSAAudioError, snd_strerror(res));
        return NULL;    
    }
    return (PyObject *)self;
}

static void alsapcm_dealloc(alsapcm_t *self) 
{
    if (self->handle) {
        snd_pcm_drain(self->handle);
        snd_pcm_close(self->handle);
    }
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
        Py_BEGIN_ALLOW_THREADS
        snd_pcm_drain(self->handle);
        snd_pcm_close(self->handle);
        Py_END_ALLOW_THREADS
            
        self->handle = 0;
    }
    
    Py_INCREF(Py_None);
    return Py_None;
}

PyDoc_STRVAR(pcm_close_doc,
"close() -> None\n\
\n\
Close a PCM device.");

static PyObject *
alsapcm_dumpinfo(alsapcm_t *self, PyObject *args) 
{
    unsigned int val,val2;
    snd_pcm_format_t fmt;
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
    
    snd_pcm_hw_params_get_access(hwparams, (snd_pcm_access_t *) &val);
    printf("access type = %s\n", snd_pcm_access_name((snd_pcm_access_t)val));

    snd_pcm_hw_params_get_format(hwparams, &fmt);
    printf("format = '%s' (%s)\n", 
           snd_pcm_format_name(fmt),
           snd_pcm_format_description(fmt));
    
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

    snd_pcm_hw_params_get_period_time(hwparams, &val, &dir);
    printf("period time = %d us\n", val);

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

PyDoc_STRVAR(pcmtype_doc,
"pcmtype() -> int\n\
\n\
Returns either PCM_CAPTURE or PCM_PLAYBACK.");


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

PyDoc_STRVAR(pcmmode_doc,
"pcmmode() -> int\n\
\n\
Returns the mode of the PCM object. One of:\n\
 - PCM_NONBLOCK\n\
 - PCM_ASYNC\n\
 - PCM_NORMAL.");


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

PyDoc_STRVAR(cardname_doc,
"cardname() -> string\n\
\n\
Returns the name of the sound card used by this PCM object.");


static PyObject *
alsapcm_setchannels(alsapcm_t *self, PyObject *args) 
{
    int channels;
    int res;
    if (!PyArg_ParseTuple(args,"i:setchannels",&channels)) 
        return NULL;

    if (!self->handle) {
        PyErr_SetString(ALSAAudioError, "PCM device is closed");
        return NULL;
    }

    self->channels = channels;
    res = alsapcm_setup(self);
    if (res < 0) 
    {
        PyErr_SetString(ALSAAudioError, snd_strerror(res));
        return NULL;    
    }
    return PyLong_FromLong(self->channels);
}

PyDoc_STRVAR(setchannels_doc,
"setchannels(numchannels)\n\
\n\
Used to set the number of capture or playback channels. Common values\n\
are: 1 = mono, 2 = stereo, and 6 = full 6 channel audio.\n\
\n\
Few sound cards support more than 2 channels.");


static PyObject *
alsapcm_setrate(alsapcm_t *self, PyObject *args) 
{
    int rate;
    int res;
    if (!PyArg_ParseTuple(args,"i:setrate",&rate)) 
        return NULL;

    if (!self->handle) 
    {
        PyErr_SetString(ALSAAudioError, "PCM device is closed");
        return NULL;
    }

    self->rate = rate;
    res = alsapcm_setup(self);
    if (res < 0) 
    {
        PyErr_SetString(ALSAAudioError, snd_strerror(res));
        return NULL;    
    }
    return PyLong_FromLong(self->rate);
}

PyDoc_STRVAR(setrate_doc,
"setrate(rate)\n\
\n\
Set the sample rate in Hz for the device. Typical values are\n\
8000 (telephony), 11025, 44100 (CD), 48000 (DVD audio) and 96000");


static PyObject *
alsapcm_setformat(alsapcm_t *self, PyObject *args) 
{
    int format;
    int res;
    if (!PyArg_ParseTuple(args,"i:setformat",&format)) 
        return NULL;

    if (!self->handle) 
    {
        PyErr_SetString(ALSAAudioError, "PCM device is closed");
        return NULL;
    }

    self->format = format;
    res = alsapcm_setup(self);
    if (res < 0) 
    {
        PyErr_SetString(ALSAAudioError, snd_strerror(res));
        return NULL;    
    }
    return PyLong_FromLong(self->format);
}

PyDoc_STRVAR(setformat_doc,
"setformat(rate)\n");

static PyObject *
alsapcm_setperiodsize(alsapcm_t *self, PyObject *args) 
{
    int periodsize;
    int res;

    if (!PyArg_ParseTuple(args,"i:setperiodsize",&periodsize)) 
        return NULL;

    if (!self->handle) 
    {
        PyErr_SetString(ALSAAudioError, "PCM device is closed");
        return NULL;
    }
    
    self->periodsize = periodsize;
    res = alsapcm_setup(self);
    if (res < 0) 
    {
        PyErr_SetString(ALSAAudioError, snd_strerror(res));
        return NULL;    
    }
    return PyLong_FromLong(self->periodsize);
}

PyDoc_STRVAR(setperiodsize_doc,
"setperiodsize(period) -> int\n\
\n\
Sets the actual period size in frames. Each write should consist of\n\
exactly this number of frames, and each read will return this number of\n\
frames (unless the device is in PCM_NONBLOCK mode, in which case it\n\
may return nothing at all).");

static PyObject *
alsapcm_read(alsapcm_t *self, PyObject *args) 
{
    int res;
    char buffer[8000];

    if (self->framesize * self->periodsize > 8000) {
        PyErr_SetString(ALSAAudioError,"Capture data too large. "
                        "Try decreasing period size");
        return NULL;
    }

    if (!PyArg_ParseTuple(args,":read")) 
        return NULL;

    if (!self->handle) {
        PyErr_SetString(ALSAAudioError, "PCM device is closed");
        return NULL;
  }
    
    if (self->pcmtype != SND_PCM_STREAM_CAPTURE) 
    {
        PyErr_SetString(ALSAAudioError,"Cannot read from playback PCM");
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
    res = snd_pcm_readi(self->handle, buffer, self->periodsize);
    if (res == -EPIPE) 
    {
        /* EPIPE means overrun */
        snd_pcm_prepare(self->handle);
    }
    Py_END_ALLOW_THREADS

    if (res != -EPIPE)
    {
        if (res == -EAGAIN) 
        {
            res = 0;
        }
        else if (res < 0) {
            PyErr_SetString(ALSAAudioError, snd_strerror(res));
            return NULL;
        } 
    }

#if PY_MAJOR_VERSION < 3
    return Py_BuildValue("is#", res, buffer, res*self->framesize);
#else
    return Py_BuildValue("iy#", res, buffer, res*self->framesize);
#endif
}

PyDoc_STRVAR(read_doc,
"read() -> (size, data)\n\
\n\
In PCM_NORMAL mode, this function blocks until a full period is\n\
available, and then returns a tuple (length,data) where length is\n\
the number of frames of the captured data, and data is the captured sound\n\
frames as bytes (or a string in Python 2.x). The length of the returned data\n\
 will be periodsize*framesize bytes.\n\
\n\
In PCM_NONBLOCK mode, the call will not block, but will return (0,'')\n\
if no new period has become available since the last call to read.");


static PyObject *alsapcm_write(alsapcm_t *self, PyObject *args) 
{
    int res;
    int datalen;
    char *data;
    
#if PY_MAJOR_VERSION < 3
    if (!PyArg_ParseTuple(args,"s#:write",&data,&datalen)) 
        return NULL;
#else
    Py_buffer buf;

    if (!PyArg_ParseTuple(args,"y*:write",&buf)) 
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

    Py_BEGIN_ALLOW_THREADS
    res = snd_pcm_writei(self->handle, data, datalen/self->framesize);
    if (res == -EPIPE) 
    {
        /* EPIPE means underrun */
        res = snd_pcm_recover(self->handle, res, 1);
        if (res >= 0)
            res = snd_pcm_writei(self->handle, data, datalen/self->framesize);
    }
    Py_END_ALLOW_THREADS
  
    if (res == -EAGAIN) 
        return PyLong_FromLong(0);
    else if (res < 0) 
    {
        PyErr_SetString(ALSAAudioError,snd_strerror(res));
        return NULL;
    }  

  return PyLong_FromLong(res);
}

PyDoc_STRVAR(write_doc,
"write(data) -> bytes written\n\
\n\
Writes (plays) the sound in data. The length of data must be a multiple\n\
of the frame size, and should be exactly the size of a period. If less\n\
than 'period size' frames are provided, the actual playout will not\n\
happen until more data is written.\n\
If the device is not in PCM_NONBLOCK mode, this call will block if the\n\
kernel buffer is full, and until enough sound has been played to allow\n\
the sound data to be buffered. The call always returns the size of the\n\
data provided.\n\
\n\
In PCM_NONBLOCK mode, the call will return immediately, with a return\n\
value of zero, if the buffer is full. In this case, the data should be\n\
written at a later time.");


static PyObject *alsapcm_pause(alsapcm_t *self, PyObject *args) 
{
    int enabled=1, res;

    if (!PyArg_ParseTuple(args,"|i:pause",&enabled)) 
        return NULL;

    if (!self->handle) {
        PyErr_SetString(ALSAAudioError, "PCM device is closed");
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
    res = snd_pcm_pause(self->handle, enabled);
    Py_END_ALLOW_THREADS
  
    if (res < 0) 
    {
        PyErr_SetString(ALSAAudioError,snd_strerror(res));
        return NULL;
    }
    return PyLong_FromLong(res);
}

PyDoc_STRVAR(pause_doc,
"pause(enable=1)\n\
\n\
If enable is 1, playback or capture is paused. If enable is 0,\n\
playback/capture is resumed.");

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
        PyErr_SetString(ALSAAudioError,"Can't get poll descriptor count");
        return NULL;
    }

    fds = (struct pollfd*)calloc(count, sizeof(struct pollfd));
    if (!fds)
    {
        PyErr_SetString(PyExc_MemoryError, "Out of memory");
        return NULL;
    }

    result = PyList_New(count);
    rc = snd_pcm_poll_descriptors(self->handle, fds, (unsigned int)count);
    if (rc != count)
    {
        PyErr_SetString(ALSAAudioError,"Can't get poll descriptors");
        return NULL;
    }

    for (i = 0; i < count; ++i)
    {
        PyList_SetItem(result, i,
                       Py_BuildValue("II", fds[i].fd, fds[i].events));
    }

    return result;
}

PyDoc_STRVAR(pcm_polldescriptors_doc,
"polldescriptors() -> List of tuples (fd, eventmask).\n\
\n\
Return a list of file descriptors and event masks\n\
suitable for use with poll.");


/* ALSA PCM Object Bureaucracy */

static PyMethodDef alsapcm_methods[] = {
    {"pcmtype", (PyCFunction)alsapcm_pcmtype, METH_VARARGS, pcmtype_doc},
    {"pcmmode", (PyCFunction)alsapcm_pcmmode, METH_VARARGS, pcmmode_doc},
    {"cardname", (PyCFunction)alsapcm_cardname, METH_VARARGS, cardname_doc},
    {"setchannels", (PyCFunction)alsapcm_setchannels, METH_VARARGS, 
     setchannels_doc },
    {"setrate", (PyCFunction)alsapcm_setrate, METH_VARARGS, setrate_doc},
    {"setformat", (PyCFunction)alsapcm_setformat, METH_VARARGS, setformat_doc},
    {"setperiodsize", (PyCFunction)alsapcm_setperiodsize, METH_VARARGS, 
     setperiodsize_doc},
    {"dumpinfo", (PyCFunction)alsapcm_dumpinfo, METH_VARARGS},
    {"read", (PyCFunction)alsapcm_read, METH_VARARGS, read_doc},
    {"write", (PyCFunction)alsapcm_write, METH_VARARGS, write_doc},
    {"pause", (PyCFunction)alsapcm_pause, METH_VARARGS, pause_doc},
    {"close", (PyCFunction)alsapcm_close, METH_VARARGS, pcm_close_doc},
    {"polldescriptors", (PyCFunction)alsapcm_polldescriptors, METH_VARARGS,
     pcm_polldescriptors_doc},
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
    0,                              /* ob_size */
#else
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
#endif
    "alsaaudio.PCM",                /* tp_name */
    sizeof(alsapcm_t),              /* tp_basicsize */
    0,                              /* tp_itemsize */
    /* methods */    
    (destructor) alsapcm_dealloc,   /* tp_dealloc */
    0,                              /* print */
#if PY_VERSION_HEX < 0x02020000
    (getattrfunc)alsapcm_getattr,   /* tp_getattr */
#else
    0,                              /* tp_getattr */
#endif
    0,                              /* tp_setattr */
    0,                              /* tp_compare */ 
    0,                              /* tp_repr */
    0,                              /* tp_as_number */
    0,                              /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    0,                              /* tp_hash */
    0,                              /* tp_call */
    0,                              /* tp_str */
#if PY_VERSION_HEX >= 0x02020000 
    PyObject_GenericGetAttr,        /* tp_getattro */
#else
    0,                              /* tp_getattro */
#endif
    0,                              /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,             /* tp_flags */
    "ALSA PCM device.",             /* tp_doc */
    0,					          /* tp_traverse */
    0,					          /* tp_clear */
    0,					          /* tp_richcompare */
    0,					          /* tp_weaklistoffset */
    0,					          /* tp_iter */
    0,					          /* tp_iternext */
    alsapcm_methods,		          /* tp_methods */
    0,			                  /* tp_members */
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
alsamixer_gethandle(char *cardname, snd_mixer_t **handle) 
{
    int err;
    if ((err = snd_mixer_open(handle, 0)) < 0) return err;
    if ((err = snd_mixer_attach(*handle, cardname)) < 0) return err;
    if ((err = snd_mixer_selem_register(*handle, NULL, NULL)) < 0) return err;
    if ((err = snd_mixer_load(*handle)) < 0) return err;
    
    return 0;
}

static PyObject *
alsamixer_list(PyObject *self, PyObject *args) 
{
    snd_mixer_t *handle;
    snd_mixer_selem_id_t *sid;
    snd_mixer_elem_t *elem;
    int err;
    int cardidx = 0;
    char cardname[32];
    PyObject *result;
    
    if (!PyArg_ParseTuple(args,"|i:mixers",&cardidx)) 
        return NULL;
    
    sprintf(cardname, "hw:%d", cardidx);

    snd_mixer_selem_id_alloca(&sid);
    err = alsamixer_gethandle(cardname, &handle);
    if (err < 0) 
    {
        PyErr_SetString(ALSAAudioError,snd_strerror(err));
        snd_mixer_close(handle);
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

PyDoc_STRVAR(mixers_doc,
"mixers([cardname])\n\
\n\
List the available mixers. The optional cardname specifies\n\
which card should be queried (this is only relevant if you\n\
have more than one sound card). Omit to use the default sound card.");


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
    int cardindex = 0;
    char *control = "Master";
    int id = 0;
    snd_mixer_elem_t *elem;
    int channel;
    char *kw[] = { "control", "id", "cardindex", NULL };
    
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|sii", kw,
                                     &control, &id, &cardindex)) 
        return NULL;

    if (!(self = (alsamixer_t *)PyObject_New(alsamixer_t, &ALSAMixerType))) 
        return NULL;

    self->handle = 0;
    self->cardname = translate_cardidx(cardindex);

    err = alsamixer_gethandle(self->cardname, &self->handle);
    if (err<0) 
    {
        PyErr_SetString(ALSAAudioError,snd_strerror(err));
        free(self->cardname);
        return NULL;
    }

    self->controlname = strdup(control);
    self->controlid = id;
    
    elem = alsamixer_find_elem(self->handle,control,id);
    if (!elem) 
    {
        char errtext[128];
        sprintf(errtext,"Unable to find mixer control '%s',%i",
                self->controlname,
                self->controlid);
        snd_mixer_close(self->handle);
        PyErr_SetString(ALSAAudioError,errtext);
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
    
    snd_mixer_close(self->handle);
    free(self->cardname);
    free(self->controlname);
    self->handle = 0;
    
    Py_INCREF(Py_None);
    return Py_None;
}

PyDoc_STRVAR(mixer_close_doc,
"close() -> None\n\
\n\
Close a Mixer.");

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

PyDoc_STRVAR(mixer_cardname_doc,
"cardname() -> string\n\
\n\
Returns the name of the sound card used by this Mixer object.");

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

PyDoc_STRVAR(mixer_doc,
"mixer() -> string\n\
\n\
Returns the name of the specific mixer controlled by this object,\n\
for example 'Master' or 'PCM'");


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

PyDoc_STRVAR(mixerid_doc,
"mixerid() -> int\n\
\n\
Returns the ID of the ALSA mixer controlled by this object.");


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
    if (self->volume_cap&MIXER_CAP_VOLUME) 
    {
        item = PyUnicode_FromString("Volume");
        PyList_Append(result, item);
        Py_DECREF(item);
    }
    if (self->volume_cap&MIXER_CAP_VOLUME_JOINED) 
    {
        item = PyUnicode_FromString("Joined Volume"); 
        PyList_Append(result, item);
        Py_DECREF(item);
    }
    if (self->volume_cap&MIXER_CAP_PVOLUME) 
    {
        item = PyUnicode_FromString("Playback Volume");
        PyList_Append(result, item);
        Py_DECREF(item);
    }
    if (self->volume_cap&MIXER_CAP_PVOLUME_JOINED) 
    {
        item = PyUnicode_FromString("Joined Playback Volume");
        PyList_Append(result, item);
        Py_DECREF(item);
    }
    if (self->volume_cap&MIXER_CAP_CVOLUME) 
    {
        item = PyUnicode_FromString("Capture Volume");
        PyList_Append(result, item);
        Py_DECREF(item);
    }
    if (self->volume_cap&MIXER_CAP_CVOLUME_JOINED) 
    {
        item = PyUnicode_FromString("Joined Capture Volume");
        PyList_Append(result, item);
        Py_DECREF(item);
    }

    return result;
}

PyDoc_STRVAR(volumecap_doc,
"volumecap() -> List of volume capabilities (string)\n\
\n\
Returns a list of the volume control capabilities of this mixer.\n\
Possible values in this list are:\n\
 - 'Volume'\n\
 - 'Joined Volume'\n\
 - 'Playback Volume'\n\
 - 'Joined Playback Mute'\n\
 - 'Capture Volume'\n\
 - 'Joined Capture Volume'");


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
    if (self->volume_cap&MIXER_CAP_SWITCH) 
    {
        item = PyUnicode_FromString("Mute");
        PyList_Append(result, item);
        Py_DECREF(item);
    }
    if (self->volume_cap&MIXER_CAP_SWITCH_JOINED) 
    {
        item = PyUnicode_FromString("Joined Mute");
        PyList_Append(result, item);
        Py_DECREF(item);
    }
    if (self->volume_cap&MIXER_CAP_PSWITCH) 
    {
        item = PyUnicode_FromString("Playback Mute");
        PyList_Append(result, item);
        Py_DECREF(item);
    }
    if (self->volume_cap&MIXER_CAP_PSWITCH_JOINED) 
    {
        item = PyUnicode_FromString("Joined Playback Mute");
        PyList_Append(result, item);
        Py_DECREF(item);
    }
    if (self->volume_cap&MIXER_CAP_CSWITCH) 
    {
        item = PyUnicode_FromString("Capture Mute");
        PyList_Append(result, item);
        Py_DECREF(item);
    }
    if (self->volume_cap&MIXER_CAP_CSWITCH_JOINED) 
    {
        item = PyUnicode_FromString("Joined Capture Mute");
        PyList_Append(result, item);
        Py_DECREF(item);
    }
    if (self->volume_cap&MIXER_CAP_CSWITCH_EXCLUSIVE) 
    {
        item = PyUnicode_FromString("Capture Exclusive");
        PyList_Append(result, item);
        Py_DECREF(item);
    }

    return result;
}

PyDoc_STRVAR(switchcap_doc,
"switchcap() -> List of switch capabilities (string)\n\
\n\
Returns a list of the switches which are defined by this mixer.\n\
\n\
Possible values in this list are:\n\
 - 'Mute'\n\
 - 'Joined Mute'\n\
 - 'Playback Mute'\n\
 - 'Joined Playback Mute'\n\
 - 'Capture Mute'\n\
 - 'Joined Capture Mute'\n\
 - 'Capture Exclusive'\n");


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
alsamixer_getvolume(alsamixer_t *self, PyObject *args) 
{
    snd_mixer_elem_t *elem;
    int direction;
    int channel;
    long ival;
    char *dirstr = 0;
    PyObject *result;
    PyObject *item;
    
    if (!PyArg_ParseTuple(args,"|s:getvolume",&dirstr)) 
        return NULL;
    
    if (!self->handle) 
    {
        PyErr_SetString(ALSAAudioError, "Mixer is closed");
        return NULL;
    }
    
    elem = alsamixer_find_elem(self->handle,self->controlname,self->controlid);
    
    if (!dirstr) 
    {
        if (self->pchannels) 
            direction = 0;
        else 
            direction = 1;
    }
    else if (strcasecmp(dirstr,"playback")==0) 
        direction = 0;
    else if (strcasecmp(dirstr,"capture")==0) 
        direction = 1;
    else 
    {
        PyErr_SetString(ALSAAudioError,"Invalid direction argument for mixer");
        return NULL;
    }

    result = PyList_New(0);

    for (channel = 0; channel <= SND_MIXER_SCHN_LAST; channel++) {
        if (direction == 0 && 
            snd_mixer_selem_has_playback_channel(elem, channel)) 
        {
            snd_mixer_selem_get_playback_volume(elem, channel, &ival);
            item = PyLong_FromLong(alsamixer_getpercentage(self->pmin,
                                                           self->pmax,
                                                           ival));
            PyList_Append(result, item);
            Py_DECREF(item);
        }
        else if (direction == 1
                 && snd_mixer_selem_has_capture_channel(elem, channel)
                 && snd_mixer_selem_has_capture_volume(elem)) {
            snd_mixer_selem_get_capture_volume(elem, channel, &ival);
            item = PyLong_FromLong(alsamixer_getpercentage(self->cmin,
                                                           self->cmax,
                                                           ival));
            PyList_Append(result, item);
            Py_DECREF(item);
        }
    }

    return result;
}

PyDoc_STRVAR(getvolume_doc,
"getvolume([direction]) -> List of volume settings (int)\n\
\n\
Returns a list with the current volume settings for each channel.\n\
The list elements are integer percentages.\n\
\n\
The optional 'direction' argument can be either 'playback' or\n\
'capture', which is relevant if the mixer can control both\n\
playback and capture volume. The default value is 'playback'\n\
if the mixer has this capability, otherwise 'capture'");


static PyObject *
alsamixer_getrange(alsamixer_t *self, PyObject *args) 
{
    snd_mixer_elem_t *elem;
    int direction;
    char *dirstr = 0;
    
    if (!PyArg_ParseTuple(args,"|s:getrange",&dirstr)) 
        return NULL;

    if (!self->handle) 
    {
        PyErr_SetString(ALSAAudioError, "Mixer is closed");
        return NULL;
    }
    
    elem = alsamixer_find_elem(self->handle,self->controlname,self->controlid);
    
    if (!dirstr) 
    {
        if (self->pchannels) 
            direction = 0;
        else 
            direction = 1;
    }
    else if (strcasecmp(dirstr,"playback")==0) 
      direction = 0;
    else if (strcasecmp(dirstr,"capture")==0) 
      direction = 1;
    else 
    {
      PyErr_SetString(ALSAAudioError,"Invalid argument for direction");
      return NULL;
    }
    if (direction == 0) 
    {
        if (snd_mixer_selem_has_playback_channel(elem, 0)) 
        {
            return Py_BuildValue("[ii]", self->pmin, self->pmax);
        }

        PyErr_SetString(ALSAAudioError, "Mixer has no playback channel");
        return NULL;
    }
    else if (direction == 1) 
    { 
        if (snd_mixer_selem_has_capture_channel(elem, 0)
            && snd_mixer_selem_has_capture_volume(elem)) {
            return Py_BuildValue("[ii]", self->cmin, self->cmax);
        }

        PyErr_SetString(ALSAAudioError, "Mixer has no capture channel "
                        "or capture volume");
        return NULL;    
    }
  
    // Unreached statement
    PyErr_SetString(ALSAAudioError,"Huh?");
    return NULL;
}

PyDoc_STRVAR(getrange_doc,
"getrange([direction]) -> List of (min_volume, max_volume)\n\
\n\
Returns a list of tuples with the volume range (ints).\n\
\n\
The optional 'direction' argument can be either 'playback' or\n\
'capture', which is relevant if the mixer can control both\n\
playback and capture volume. The default value is 'playback'\n\
if the mixer has this capability, otherwise 'capture'");


static PyObject *
alsamixer_getenum(alsamixer_t *self, PyObject *args) 
{
    snd_mixer_elem_t *elem;
    PyObject *elems;
    int i, count, rc;
    unsigned int index;
    char name[32];
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
        PyErr_SetString(ALSAAudioError, snd_strerror(count));
        return NULL;
    }

    result = PyTuple_New(2);
    if (!result)
        return NULL;
    
    rc = snd_mixer_selem_get_enum_item(elem, 0, &index);
    if(rc) 
    {
        PyErr_SetString(ALSAAudioError, snd_strerror(rc));
        return NULL;
    }
    rc = snd_mixer_selem_get_enum_item_name(elem, index, sizeof(name)-1, name);
    if (rc) 
    {
        Py_DECREF(result);
        PyErr_SetString(ALSAAudioError, snd_strerror(rc));
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
            PyErr_SetString(ALSAAudioError, snd_strerror(rc));
            return NULL;
        }
        
        PyList_SetItem(elems, i, PyUnicode_FromString(name));
    }
    
    PyTuple_SetItem(result, 1, elems);

    return result;
}

PyDoc_STRVAR(getenum_doc,
"getenum() -> Tuple of (string, list of strings)\n\
\n\
Returns a a tuple. The first element is name of the active enumerated item, \n\
the second a list available enumerated items.");

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

    elem = alsamixer_find_elem(self->handle,self->controlname,self->controlid);
    if (!snd_mixer_selem_has_playback_switch(elem)) 
    {
        PyErr_SetString(ALSAAudioError,"Mixer has no mute switch");
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

PyDoc_STRVAR(getmute_doc,
"getmute() -> List of mute settings (int)\n\
\n\
Return a list indicating the current mute setting for each channel.\n\
0 means not muted, 1 means muted.\n\
\n\
This method will fail if the mixer has no playback switch capabilities.");


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

    elem = alsamixer_find_elem(self->handle,self->controlname,self->controlid);
    if (!snd_mixer_selem_has_capture_switch(elem)) 
    {
        PyErr_SetString(ALSAAudioError,"Mixer has no record switch");
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

PyDoc_STRVAR(getrec_doc,
"getrec() -> List of record mute settings (int)\n\
\n\
Return a list indicating the current record mute setting for each\n\
channel. 0 means not recording, 1 means recording.\n\
This method will fail if the mixer has no capture switch capabilities.");


static PyObject *
alsamixer_setvolume(alsamixer_t *self, PyObject *args) 
{
    snd_mixer_elem_t *elem;
    int direction;
    int i;
    long volume;
    int physvolume;
    char *dirstr = 0;
    int channel = MIXER_CHANNEL_ALL;
    int done = 0;

    if (!PyArg_ParseTuple(args,"l|is:setvolume",&volume,&channel,&dirstr)) 
        return NULL;

    if (volume < 0 || volume > 100) 
    {
        PyErr_SetString(ALSAAudioError,"Volume must be between 0 and 100");
        return NULL;
    }

    if (!self->handle) 
    {
        PyErr_SetString(ALSAAudioError, "Mixer is closed");
        return NULL;
    }
  
    elem = alsamixer_find_elem(self->handle,self->controlname,self->controlid);

    if (!dirstr) 
    {
        if (self->pchannels) 
            direction = 0;
        else 
            direction = 1;
    }
    else if (strcasecmp(dirstr,"playback")==0) 
        direction = 0;
    else if (strcasecmp(dirstr,"capture")==0) 
        direction = 1;
    else 
    {
        PyErr_SetString(ALSAAudioError,"Invalid direction argument. Use "
                        "'playback' or 'capture'");
        return NULL;
    }
    for (i = 0; i <= SND_MIXER_SCHN_LAST; i++) 
    {
        if (channel == -1 || channel == i) 
        {
            if (direction == 0 && 
                snd_mixer_selem_has_playback_channel(elem, i)) {
                physvolume = alsamixer_getphysvolume(self->pmin,
                                                     self->pmax, volume);
                snd_mixer_selem_set_playback_volume(elem, i, physvolume);
                done++;
            }
            else if (direction == 1
                     && snd_mixer_selem_has_capture_channel(elem, channel)
                     && snd_mixer_selem_has_capture_volume(elem)) 
            {
                physvolume = alsamixer_getphysvolume(self->cmin,self->cmax,
                                                     volume);
                snd_mixer_selem_set_capture_volume(elem, i, physvolume);
                done++;
            }
        }
    }
    if(!done) 
    {
        PyErr_SetString(ALSAAudioError,"No such channel");
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyDoc_STRVAR(setvolume_doc,
"setvolume(volume[[, channel] [, direction]])\n\
\n\
Change the current volume settings for this mixer. The volume argument\n\
controls the new volume setting as an integer percentage.\n\
If the optional argument channel is present, the volume is set only for\n\
this channel. This assumes that the mixer can control the volume for the\n\
channels independently.\n\
\n\
The optional direction argument can be either 'playback' or 'capture'.\n\
It is relevant if the mixer has independent playback and capture volume\n\
capabilities, and controls which of the volumes will be changed.\n\
The default is 'playback' if the mixer has this capability, otherwise\n\
'capture'.");


static PyObject *
alsamixer_setmute(alsamixer_t *self, PyObject *args) 
{
    snd_mixer_elem_t *elem;
    int i;
    int mute = 0;
    int done = 0;
    int channel = MIXER_CHANNEL_ALL;
    if (!PyArg_ParseTuple(args,"i|i:setmute",&mute,&channel)) 
        return NULL;

    if (!self->handle) 
    {
        PyErr_SetString(ALSAAudioError, "Mixer is closed");
        return NULL;
    }
    
    elem = alsamixer_find_elem(self->handle,self->controlname,self->controlid);
    if (!snd_mixer_selem_has_playback_switch(elem)) 
    {
        PyErr_SetString(ALSAAudioError,"Mixer has no mute switch");
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
        PyErr_SetString(ALSAAudioError,"Invalid channel number");
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyDoc_STRVAR(setmute_doc,
"setmute(mute [, channel])\n\
\n\
Sets the mute flag to a new value. The mute argument is either 0 for\n\
not muted, or 1 for muted.\n\
The optional channel argument controls which channel is muted.\n\
If omitted, the mute flag is set for for all channels.\n\
\n\
This method will fail if the mixer has no playback mute capabilities");


static PyObject *
alsamixer_setrec(alsamixer_t *self, PyObject *args) 
{
    snd_mixer_elem_t *elem;
    int i;
    int rec = 0;
    int done = 0;
    int channel = MIXER_CHANNEL_ALL;
    
    if (!PyArg_ParseTuple(args,"i|i:setrec",&rec,&channel)) 
        return NULL;
    
    if (!self->handle) 
    {
        PyErr_SetString(ALSAAudioError, "Mixer is closed");
        return NULL;
    }
    
    elem = alsamixer_find_elem(self->handle,self->controlname,self->controlid);
    if (!snd_mixer_selem_has_capture_switch(elem)) 
    {
        PyErr_SetString(ALSAAudioError,"Mixer has no record switch");
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
        PyErr_SetString(ALSAAudioError,"Invalid channel number");
        return NULL;
    }
    
    Py_INCREF(Py_None);
    return Py_None;
}

PyDoc_STRVAR(setrec_doc,
"setrec(capture [, channel])\n\
\n\
Sets the capture mute flag to a new value. The capture argument is\n\
either 0 for no capture, or 1 for capture.\n\
The optional channel argument controls which channel is changed.\n\
If omitted, the capture flag is set for all channels.\n\
\n\
This method will fail if the mixer has no capture switch capabilities");

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
        PyErr_SetString(ALSAAudioError,"Can't get poll descriptor count");
        return NULL;
    }

    fds = (struct pollfd*)calloc(count, sizeof(struct pollfd));
    if (!fds)
    {
        PyErr_SetString(PyExc_MemoryError, "Out of memory");
        return NULL;
    }

    result = PyList_New(count);
    rc = snd_mixer_poll_descriptors(self->handle, fds, (unsigned int)count);
    if (rc != count)
    {
        PyErr_SetString(ALSAAudioError,"Can't get poll descriptors");
        return NULL;
    }

    for (i = 0; i < count; ++i)
    {
        PyList_SetItem(result, i, 
                       Py_BuildValue("II", fds[i].fd, fds[i].events));
    }

    return result;
}

PyDoc_STRVAR(polldescriptors_doc,
"polldescriptors() -> List of tuples (fd, eventmask).\n\
\n\
Return a list of file descriptors and event masks\n\
suitable for use with poll to monitor changes on this mixer.");

static PyMethodDef alsamixer_methods[] = {
    {"cardname", (PyCFunction)alsamixer_cardname, METH_VARARGS, 
     mixer_cardname_doc},
    {"close", (PyCFunction)alsamixer_close, METH_VARARGS, mixer_close_doc},
    {"mixer", (PyCFunction)alsamixer_mixer, METH_VARARGS, mixer_doc},
    {"mixerid", (PyCFunction)alsamixer_mixerid, METH_VARARGS, mixerid_doc},
    {"switchcap", (PyCFunction)alsamixer_switchcap, METH_VARARGS, 
     switchcap_doc},
    {"volumecap", (PyCFunction)alsamixer_volumecap, METH_VARARGS, 
     volumecap_doc},
    {"getvolume", (PyCFunction)alsamixer_getvolume, METH_VARARGS, 
     getvolume_doc},
    {"getrange", (PyCFunction)alsamixer_getrange, METH_VARARGS, getrange_doc},
    {"getenum", (PyCFunction)alsamixer_getenum, METH_VARARGS, getenum_doc},
    {"getmute", (PyCFunction)alsamixer_getmute, METH_VARARGS, getmute_doc},
    {"getrec", (PyCFunction)alsamixer_getrec, METH_VARARGS, getrec_doc},
    {"setvolume", (PyCFunction)alsamixer_setvolume, METH_VARARGS, 
     setvolume_doc},
    {"setmute", (PyCFunction)alsamixer_setmute, METH_VARARGS, setmute_doc},
    {"setrec", (PyCFunction)alsamixer_setrec, METH_VARARGS, setrec_doc},
    {"polldescriptors", (PyCFunction)alsamixer_polldescriptors, METH_VARARGS, 
     polldescriptors_doc},
    
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
    0,                              /* ob_size */
#else
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
#endif
    "alsaaudio.Mixer",              /* tp_name */
    sizeof(alsamixer_t),            /* tp_basicsize */
    0,                              /* tp_itemsize */
    /* methods */    
    (destructor) alsamixer_dealloc, /* tp_dealloc */
    0,                              /* print */
#if PY_VERSION_HEX < 0x02020000 
    (getattrfunc)alsamixer_getattr, /* tp_getattr */
#else
    0,                              /* tp_getattr */
#endif
    0,                              /* tp_setattr */
    0,                              /* tp_compare */ 
    0,                              /* tp_repr */
    0,                              /* tp_as_number */
    0,                              /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    0,                              /* tp_hash */
    0,                              /* tp_call */
    0,                              /* tp_str */
#if PY_VERSION_HEX >= 0x02020000 
    PyObject_GenericGetAttr,        /* tp_getattro*/
#else
    0,                              /* tp_getattro*/
#endif
    0,                              /* tp_setattro*/
    0,                              /* tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,             /* tp_flags */
    "ALSA Mixer Control.",          /* tp_doc */
    0,					          /* tp_traverse */
    0,					          /* tp_clear */
    0,					          /* tp_richcompare */
    0,					          /* tp_weaklistoffset */
    0,					          /* tp_iter */
    0,					          /* tp_iternext */
    alsamixer_methods,		      /* tp_methods */
    0,			                  /* tp_members */
};


/******************************************/
/* Module initialization                  */
/******************************************/

static PyMethodDef alsaaudio_methods[] = {
    { "cards", alsacard_list, METH_VARARGS, cards_doc},
    { "mixers", alsamixer_list, METH_VARARGS, mixers_doc},
    { 0, 0 },
};


#if PY_MAJOR_VERSION >= 3

#define _EXPORT_INT(mod, name, value) \
  if (PyModule_AddIntConstant(mod, name, (long) value) == -1) return NULL;

static struct PyModuleDef alsaaudio_module = {
    PyModuleDef_HEAD_INIT,
    "alsaaudio",
    alsaaudio_module_doc,
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

    PyEval_InitThreads();

#if PY_MAJOR_VERSION < 3
    m = Py_InitModule3("alsaaudio", alsaaudio_methods, alsaaudio_module_doc);
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

#if PY_MAJOR_VERSION >= 3
    return m;
#endif
}
