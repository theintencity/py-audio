#include <Python.h>
#include <structmember.h>

#include "RtAudio.h"

extern "C" {
    PyMODINIT_FUNC initaudiodev(void);
}


RtAudio *_rtaudio = 0;
static PyObject *ModuleError;


struct callback_data_t {
    PyObject* callback;
    PyObject* userdata;
    int input_size;
    int output_size;
};

static callback_data_t callback_data;


static PyObject*
pyaudio_get_api_name(PyObject* self, PyObject* args)
{
    try {
        RtAudio::Api api = _rtaudio->getCurrentApi();
        const char* value = "unspecified";
        if (api == RtAudio::LINUX_ALSA)
            value = "linux-alsa";
        else if (api == RtAudio::LINUX_OSS)
            value = "linux-oss";
        else if (api == RtAudio::UNIX_JACK)
            value = "unix-jack";
        else if (api == RtAudio::MACOSX_CORE)
            value = "macosx-core";
        else if (api == RtAudio::WINDOWS_ASIO)
            value = "windows-asio";
        else if (api == RtAudio::WINDOWS_DS)
            value = "windows-directsound";
        else if (api == RtAudio::RTAUDIO_DUMMY)
            value = "rtaudio-dummy";
            
        return Py_BuildValue("s", value);
    } catch (RtError& e) {
        PyErr_SetString(ModuleError, e.what());
        return NULL;
    }
}

static const char* names[]= {"l8", "l16", "l24", "l32", "f32", "f64"};
static int codes[] = {RTAUDIO_SINT8, RTAUDIO_SINT16, RTAUDIO_SINT24, RTAUDIO_SINT32, RTAUDIO_FLOAT32, RTAUDIO_FLOAT64};

static int string2format(const char* str)
{
    for (unsigned i=0; i<sizeof(codes)/sizeof(codes[0]); ++i) {
        if (strcmp(str, names[i]) == 0)
            return codes[i];
    }
    return -1;
}

static int format2size(unsigned int format)
{
    switch (format) {
        case RTAUDIO_SINT8: return 1;
        case RTAUDIO_SINT16: return 2;
        case RTAUDIO_SINT24: return 3;
        case RTAUDIO_SINT32: return 4;
        case RTAUDIO_FLOAT32: return 4;
        case RTAUDIO_FLOAT64: return 8;
        default: return 0;
    }
}

PyObject* format2strings(int format)
{
    PyObject *f = PyList_New(0);
    for (unsigned i=0; i<sizeof(codes)/sizeof(codes[0]); ++i) {
        if (format & codes[i]) {
            PyList_Append(f, PyString_FromString(names[i]));
        }
    }
    return f;
}

static PyObject*
device2object(const RtAudio::DeviceInfo& info)
{
    PyObject* dev = PyDict_New();
    
    PyDict_SetItemString(dev, "probed", info.probed ? Py_True : Py_False);
    PyDict_SetItemString(dev, "name", PyString_FromString(info.name.c_str()));
    if (info.probed == true) {
        PyDict_SetItemString(dev, "output_channels", PyInt_FromLong(info.outputChannels));
        PyDict_SetItemString(dev, "input_channels", PyInt_FromLong(info.inputChannels));
        PyDict_SetItemString(dev, "duplex_channels", PyInt_FromLong(info.duplexChannels));
        PyDict_SetItemString(dev, "is_default_output", info.isDefaultOutput ? Py_True : Py_False);
        PyDict_SetItemString(dev, "is_default_input", info.isDefaultInput ? Py_True : Py_False);
        
        PyObject *r = PyTuple_New(info.sampleRates.size());
        for (unsigned int j=0; j<info.sampleRates.size(); ++j) {
            PyTuple_SetItem(r, j, PyInt_FromLong(info.sampleRates[j]));
        }
        PyDict_SetItemString(dev, "sample_rates", r);
        PyDict_SetItemString(dev, "native_formats", format2strings(info.nativeFormats));
    } else {
        PyDict_SetItemString(dev, "probed", Py_False);
    }
    return dev;
}

static const unsigned int invalid_device = (unsigned int) -1;

static unsigned int
deviceName2Id(const std::string& name, bool is_input)
{
    try {
        if (name == "default") {
            if (is_input) {
                return _rtaudio->getDefaultInputDevice();
            }
            else {
                return _rtaudio->getDefaultOutputDevice();
            }
        }
        
        unsigned device_count = _rtaudio->getDeviceCount();
        for (unsigned i=0; i<device_count; ++i) {
            RtAudio::DeviceInfo info = _rtaudio->getDeviceInfo(i);
            if (info.probed && info.name == name) {
                return i;
            }
        }
        
        PyErr_SetString(ModuleError, "device name not found");
        return invalid_device;
    } catch (RtError& e) {
        PyErr_SetString(ModuleError, e.what());
        return invalid_device;
    }
}


static PyObject*
pyaudio_get_devices(PyObject* self, PyObject* args)
{
    try {
        unsigned device_count = _rtaudio->getDeviceCount();
        PyObject *result = PyTuple_New(device_count);
        
        for (unsigned i=0; i<device_count; ++i) {
            RtAudio::DeviceInfo info = _rtaudio->getDeviceInfo(i);
            PyObject *dev = device2object(info);
            PyTuple_SetItem(result, i, dev);
        }
        return Py_BuildValue("N", result);
    } catch (RtError& e) {
        PyErr_SetString(ModuleError, e.what());
        return NULL;
    }
}


static int
inout(void *output_buffer, void *input_buffer, unsigned int buffer_frames,
    double stream_time, RtAudioStreamStatus status, void *userdata)
{
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
    
    unsigned int input_size = buffer_frames * callback_data.input_size;
    unsigned int output_size = buffer_frames* callback_data.output_size;
    
    PyObject* input = NULL;
    if (input_size > 0) {
        input = PyString_FromStringAndSize((const char *)input_buffer, input_size);
    }
    else {
        input = PyString_FromString("");
    }
        
    PyObject* arglist = Py_BuildValue("(OdO)", input, stream_time, callback_data.userdata);
    PyObject* output = PyObject_CallObject(callback_data.callback, arglist);
    Py_XDECREF(input);
    Py_XDECREF(arglist);
    
    if (output != NULL) {
        if (PyString_Check(output)) {
            unsigned int new_size = PyString_Size(output);
            if (output_size > 0 && new_size > 0) {
                memcpy(output_buffer, PyString_AsString(output), new_size < output_size ? new_size : output_size);
            } else {
                memset(output_buffer, 0, output_size);
            }
        }        
        Py_DECREF(output);
    }
    /* Release the thread. No Python API allowed beyond this point. */
    PyGILState_Release(gstate);
    
    return 0;
}

static PyObject*
pyaudio_open(PyObject* self, PyObject* args, PyObject* kwargs)
{
    RtAudio::StreamParameters input, output;
    RtAudio::StreamOptions options;
    int frame_duration = 20; // in milliseconds
    RtAudioFormat format = RTAUDIO_SINT16;
    const char* format_str = "l16";
    int sample_rate = 16000;
    PyObject* callback = NULL, *userdata = Py_None;
    
    const char* input_device = NULL, *output_device = NULL;
    const unsigned int invalid_device = (unsigned int) -1;
    input.deviceId = output.deviceId = invalid_device;
    input.nChannels = output.nChannels = 1;
    
    static const char *kwlist[] = {
        "callback", "output", "output_channels", "input", "input_channels", 
        "format", "sample_rate", "frame_duration", "userdata",
        "flags", "number_of_buffers", "priority",
    NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|zizisiiOiii", (char **)kwlist,
            &callback, &output_device, &output.nChannels, &input_device, &input.nChannels,
            &format_str, &sample_rate, &frame_duration, &userdata,
            &options.flags, &options.numberOfBuffers, &options.priority)) {
        return NULL;
    }
    
    unsigned int buffer_frames = frame_duration * sample_rate / 1000;
    
    if (input_device != NULL) {
        input.deviceId = deviceName2Id(std::string(input_device), true);
        if (input.deviceId == invalid_device) {
            return NULL;
        }
    }
    if (output_device != NULL) {
        output.deviceId = deviceName2Id(std::string(output_device), false);
        if (output.deviceId == invalid_device) {
            return NULL;
        }
    }
    
    format = string2format(format_str);
    if (format < 0) {
        PyErr_SetString(ModuleError, "invalid format, must be one of \"l16\", \"l8\", \"l24\", \"l32\", \"f32\", \"f64\"");
        return NULL;
    }
    
    if (callback == NULL || !PyCallable_Check(callback)) {
        PyErr_SetString(ModuleError, "mandatory callback parameter must be callable");
        return NULL;
    }
    
    // update the callback structure
    callback_data.input_size = (input.deviceId != invalid_device ? format2size(format) * input.nChannels : 0);
    callback_data.output_size = (output.deviceId != invalid_device ? format2size(format) * output.nChannels : 0);
    
    Py_XDECREF(callback_data.callback);
    Py_XINCREF(callback);
    callback_data.callback = callback;
    
    Py_XDECREF(callback_data.userdata);
    Py_XINCREF(userdata);
    callback_data.userdata = userdata;
    
    try {
        _rtaudio->openStream(output.deviceId != invalid_device ? &output : NULL,
                             input.deviceId != invalid_device ? &input : NULL,
                             format, sample_rate, &buffer_frames, &inout, NULL, &options);
        _rtaudio->startStream();
    } catch (RtError& e) {
        PyErr_SetString(ModuleError, e.what());
        Py_XDECREF(callback_data.callback);
        Py_XDECREF(callback_data.userdata);
        callback_data.callback = NULL;
        callback_data.userdata = NULL;
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject*
pyaudio_close(PyObject* self, PyObject* unused)
{
    try {
        _rtaudio->stopStream();
    } catch (const RtError& e) {
        // ignore
    }
    try {
        _rtaudio->closeStream();
    } catch (const RtError& e) {
        // ignore
    }
    Py_XDECREF(callback_data.callback);
    Py_XDECREF(callback_data.userdata);
    callback_data.callback = NULL;
    callback_data.userdata = NULL;
    
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject*
pyaudio_is_open(PyObject* self, PyObject* unused)
{
    try {
        return Py_BuildValue("i", _rtaudio->isStreamOpen());
    } catch (const RtError& e) {
        PyErr_SetString(ModuleError, e.what());
        return NULL;
    }
}

static PyObject*
pyaudio_get_stream_time(PyObject* self, PyObject* unused)
{
    try {
        return Py_BuildValue("d", _rtaudio->getStreamTime());
    } catch (const RtError& e) {
        PyErr_SetString(ModuleError, e.what());
        return NULL;
    }
}

static PyObject*
pyaudio_get_stream_latency(PyObject* self, PyObject* args)
{
    try {
        return Py_BuildValue("i", _rtaudio->getStreamLatency());
    } catch (const RtError& e) {
        PyErr_SetString(ModuleError, e.what());
        return NULL;
    }
}

static PyObject*
pyaudio_get_stream_sample_rate(PyObject* self, PyObject* args)
{
    try {
        return Py_BuildValue("i", _rtaudio->getStreamSampleRate());
    } catch (const RtError& e) {
        PyErr_SetString(ModuleError, e.what());
        return NULL;
    }
}



static PyMethodDef Module_methods[] = {
    {"get_api_name", (PyCFunction) pyaudio_get_api_name, METH_NOARGS,
        PyDoc_STR("get_api_name() -> name:str\n\n"
            "Get the currently used audio API name")},
    {"get_devices", (PyCFunction) pyaudio_get_devices, METH_NOARGS,
        PyDoc_STR("get_devices() -> (object1, object2, ...)\n\n"
            "Get the list of available audio devices and their properties."
            "Each object in the returned sequence is a dict with keys \"name\", \"sample_rates\", \"input_channels\", \"output_channels\", etc.")},
        
    {"open", (PyCFunction) pyaudio_open, METH_VARARGS | METH_KEYWORDS,
        PyDoc_STR("open(callback, output=None, output_channels=1, input=None, input_channels=1, format=\"l16\", sample_rate=16000, frame_duration=20, userdata=None, flags=0, number_of_buffers=0, priority=0)\n\n"
            "Open the audio device stream and start calling the callback to exchange audio fragments.\n"
            " callback - a function that is called to exchange audio data as callback(mic_data:str, stream_time:float, userdata) -> spkr_data:str\n"
            " output - name of output device or \"default\" to open audio output device\n"
            " output_channels - number of channels to use for output device.\n"
            " input - name of input device or \"default\" to open audio input device\n"
            " input_channels - number of channels to use for input device.\n"
            " format - format for audio samples is one of \"l8\", \"l16\", \"l24\", \"l32\", \"f32\", \"f64\" for various int and float values\n"
            " sample_rate - sampling rate to use for audio stream in Hz.\n"
            " frame_duration - frame duration for capture and playback in ms\n"
            " other parameters are not recommended to be changed")},
    {"close", (PyCFunction) pyaudio_close, METH_NOARGS,
        PyDoc_STR("close()\n\n"
            "Close the audio device stream and stop calling the callback to exchange audio fragments")},
        
    {"is_open", (PyCFunction) pyaudio_is_open, METH_NOARGS,
        PyDoc_STR("is_open() -> bool\n\n"
            "Whether the audio device is open and running")},
    {"get_stream_time", (PyCFunction) pyaudio_get_stream_time, METH_NOARGS,
        PyDoc_STR("get_stream_time() -> float\n\n"
            "Get the number of elapsed seconds since the stream was opened")},
    {"get_stream_latency", (PyCFunction) pyaudio_get_stream_latency, METH_NOARGS,
        PyDoc_STR("get_stream_latency() -> int\n\n"
            "Get the delay in milliseconds in input or output or both for an opened audio stream due to internal buffering")},
    {"get_stream_sample_rate", (PyCFunction) pyaudio_get_stream_sample_rate, METH_NOARGS,
        PyDoc_STR("get_stream_sample_rate() -> int\n\n"
            "Get the sample rate used for opening the audio stream")},
        
    {NULL, NULL, 0, NULL}  /* Sentinel */
};




PyMODINIT_FUNC
initaudiodev(void)
{
    PyObject *m;
    
    PyEval_InitThreads();

    m = Py_InitModule3("audiodev", Module_methods, "portable audio device module based on the RtAudio project");
    if (m == NULL)
        return;

    _rtaudio = new RtAudio();
    
    ModuleError = PyErr_NewException("audiodev.error", NULL, NULL);
    Py_INCREF(ModuleError);
    PyModule_AddObject(m, "error", ModuleError);
}

