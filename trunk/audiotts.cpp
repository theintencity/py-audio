#include <Python.h>
#include <structmember.h>
#include "arpa/inet.h"

extern "C" {
    #include "flite.h"
    cst_voice *register_cmu_us_kal(const char*);
    cst_voice *register_cmu_us_awb(const char *voxdir);
    cst_voice *register_cmu_us_rms(const char *voxdir);
    cst_voice *register_cmu_us_slt(const char *voxdir);
}

extern "C" {
    PyMODINIT_FUNC initaudiotts(void);
}

static PyObject *ModuleError;

static PyObject*
pyaudio_tts(PyObject* self, PyObject* args, PyObject* kwargs)
{
    const char* format_str = "l16"; // result will be in linear 16 format. Alternatives is ulaw
    int sample_rate = 8000;  // result will be in 8000 Hz
    int frame_duration = 20; // result will be multiple of 20 ms
    const char* name = "default"; // the name is currently ignored
    const char* text = NULL; // the text to convert
    
    static const char *kwlist[] = {
        "text", "format", "sample_rate", "frame_duration", "name",
    NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|siis", (char **)kwlist,
            &text, &format_str, &sample_rate, &frame_duration, &name)) {
        return NULL;
    }
    
    if (sample_rate != 8000 && sample_rate != 16000) {
        PyErr_SetString(ModuleError, "invalid sample_rate argument, must be 8000 or 16000");
        return NULL;
    }
    
    if (strcmp(format_str, "l16") != 0 && strcmp(format_str, "ulaw") != 0) {
        PyErr_SetString(ModuleError, "invalid format argument, must be \"l16\" or \"ulaw\"");
        return NULL;
    }
    
    cst_wave *wave;
    cst_voice *voice;
    int i, nsamples;
    
    if (strcmp(name, "default") == 0) {
        voice = flite_voice_select(NULL);
    }
    else {
        voice = flite_voice_select(name);
    }
    if (voice == 0) {
        // just use the cmu_us_kal voice
        fprintf(stderr, "warning: cannot find voice name %s, using default\n", name);
        voice = register_cmu_us_kal(NULL);
    }
    
    wave = flite_text_to_wave(text, voice);
    
    if (sample_rate != wave->sample_rate) {
        cst_wave_resample(wave, sample_rate);
    }
    
    nsamples = wave->num_samples;
    int sample_size = strcmp(format_str, "l16") == 0 ? 2 : 1;
    int frame_samples = sample_rate * frame_duration / 1000;
    
    if ((wave->num_samples % frame_samples) != 0) {
        frame_samples = wave->num_samples + frame_samples - (wave->num_samples % frame_samples);
    }
    else {
        frame_samples = wave->num_samples;
    }
    
    char* data = (char*) calloc(sample_size, frame_samples);
    if (strcmp(format_str, "l16") == 0) {
        for (i=0; i<wave->num_samples; ++i) {
            ((short *) data)[i] = wave->samples[i];
        }
        for (i=wave->num_samples; i<frame_samples; ++i) {
            ((short *) data)[i] = 0;
        }
    }
    else if (strcmp(format_str, "ulaw") == 0) {
        for (i=0; i<wave->num_samples; ++i) {
            data[i] = cst_short_to_ulaw(wave->samples[i]);
        }
        for (i=wave->num_samples; i<frame_samples; ++i) {
            ((short *) data)[i] = cst_short_to_ulaw(0);
        }
    }
    
    PyObject* output = PyString_FromStringAndSize(data, frame_samples * sample_size);
    free(data);
    return Py_BuildValue("N", output);
}


static PyMethodDef Module_methods[] = {
    {"convert", (PyCFunction) pyaudio_tts, METH_VARARGS | METH_KEYWORDS,
        PyDoc_STR("convert(text, format=\"l16\", sample_rate=8000, frame_duration=20, name='default') -> samples\n\n"
            "Convert the given text to speech samples in the given format and sample rate using the given voice name and assuming given frame duration.\n"
            " text - a string that is converted to the returned speech samples\n"
            " format - format for returned speech samples is one of \"l16\" or \"ulaw\"\n"
            " sample_rate - sampling rate to use for returned speech samples in Hz and is one of 8000 or 16000\n"
            " frame_duration - frame duration for capture and playback in ms\n"
            " name - name of the voice to use for conversion can be one of \"kal\" (male, default), \"rms\" (male), \"slt\" (female), \"awb\" (scottish male)\n")},
    {NULL, NULL, 0, NULL}  /* Sentinel */
};


PyMODINIT_FUNC
initaudiotts(void)
{
    PyObject *m;
    
    m = Py_InitModule3("audiotts", Module_methods, "portable audio text-to-speech module based on CMU's FLite project");
    if (m == NULL)
        return;

    flite_init();
    
    flite_voice_list = cons_val(voice_val(register_cmu_us_kal(NULL)),flite_voice_list);
    flite_voice_list = cons_val(voice_val(register_cmu_us_awb(NULL)),flite_voice_list);
    flite_voice_list = cons_val(voice_val(register_cmu_us_rms(NULL)),flite_voice_list);
    flite_voice_list = cons_val(voice_val(register_cmu_us_slt(NULL)),flite_voice_list);
    flite_voice_list = val_reverse(flite_voice_list);

    ModuleError = PyErr_NewException("audiotts.error", NULL, NULL);
    Py_INCREF(ModuleError);
    PyModule_AddObject(m, "error", ModuleError);
}

