#include <Python.h>
#include <structmember.h>
#include <string.h>


extern "C" {
    #include "speex/speex.h"
    #include "speex/speex_preprocess.h"
    #include "speex/speex_echo.h"
    #include "speex/speex_resampler.h"
    
    PyMODINIT_FUNC initaudiospeex(void);
}


static PyObject *ModuleError;

enum {
    TYPE_ENCODER = 1,
    TYPE_DECODER,
    TYPE_RESAMPLER,
    TYPE_PREPROCESS,
    TYPE_ECHO
};

typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
    int  type;
    void *value;
    SpeexBits bits;
    unsigned int input_size;
    unsigned int output_size;
} State;

static void
State_dealloc(State* self)
{
    if (self->value) {
        switch (self->type) {
        case TYPE_ENCODER:
            speex_encoder_destroy(self->value);
            break;
        case TYPE_DECODER:
            speex_decoder_destroy(self->value);
            break;
        case TYPE_RESAMPLER:
            speex_resampler_destroy((SpeexResamplerState*) self->value);
            break;
        case TYPE_PREPROCESS:
            speex_preprocess_state_destroy((SpeexPreprocessState*)self->value);
            break;
        case TYPE_ECHO:
            speex_echo_state_destroy((SpeexEchoState*)self->value);
            break;
        }
        self->value = NULL;
    }
    if (self->type == TYPE_ENCODER || self->type == TYPE_DECODER) {
        speex_bits_destroy(&self->bits);
    }
    
    //printf("------- destroyed codec context of type %d\n", self->type);
    self->ob_type->tp_free((PyObject*) self);
}

static PyObject *
State_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    State *self;

    self = (State *)type->tp_alloc(type, 0);
    
    if (self != NULL) {
        self->value = NULL;
        speex_bits_init(&self->bits);
    }

    return (PyObject *)self;
}


static PyTypeObject StateType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "audiospeex.State",        /*tp_name*/
    sizeof(State),             /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)State_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Internal state objects",  /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    0,                         /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    State_new,                 /* tp_new */
};



static PyObject*
pyaudio_lin2speex(PyObject* self, PyObject* args, PyObject* kwargs)
{
    PyObject* input = NULL;
    int sample_rate = 0;
    PyObject* state = Py_None;
    
    static const char *kwlist[] = {
        "fragment", "sample_rate", "state",
    NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "S|iO", (char **)kwlist,
            &input, &sample_rate, &state)) {
        return NULL;
    }
 
    if (state == Py_None) {
        if (sample_rate != 8000 && sample_rate != 16000 && sample_rate != 32000) {
            PyErr_SetString(ModuleError, "invalid or missing sample_rate argument, must be 8000, 16000 or 32000");
            return NULL;
        }
        
        state = State_new(&StateType, NULL, NULL);
        ((State*)state)->type = TYPE_ENCODER;
        ((State*)state)->value = speex_encoder_init(sample_rate == 8000 ? &speex_nb_mode :
                            (sample_rate == 16000 ? &speex_wb_mode : &speex_uwb_mode));
        if (((State*)state)->value == NULL) {
            PyErr_SetString(ModuleError, "failed to create encoder state");
            return NULL;
        }
    }
    else if (((State*)state)->type != TYPE_ENCODER) {
        PyErr_SetString(ModuleError, "invalid state argument, not an encoder state");
        return NULL;
    }
    else {
        Py_XINCREF(state);
    }
    
    short* input_frame = (short*) PyString_AsString(input);
    speex_bits_reset(&((State*)state)->bits);
    speex_encode_int(((State*)state)->value, input_frame, &((State*)state)->bits);

    int output_size = speex_bits_nbytes(&((State*)state)->bits);
    char output_bytes[output_size];
    output_size = speex_bits_write(&((State*)state)->bits, output_bytes, output_size);
    PyObject* output = PyString_FromStringAndSize(output_bytes, output_size);
    return Py_BuildValue("(NN)", output, state);
}


static PyObject*
pyaudio_speex2lin(PyObject* self, PyObject* args, PyObject* kwargs)
{
    PyObject* input = NULL;
    int sample_rate = 0;
    PyObject* state = Py_None;
    
    static const char *kwlist[] = {
        "fragment", "sample_rate", "state",
    NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "S|iO", (char **)kwlist,
            &input, &sample_rate, &state)) {
        return NULL;
    }
 
    if (state == Py_None) {
        if (sample_rate != 8000 && sample_rate != 16000 && sample_rate != 32000) {
            PyErr_SetString(ModuleError, "invalid or missing sample_rate argument, must be 8000, 16000 or 32000");
            return NULL;
        }
        
        state = State_new(&StateType, NULL, NULL);
        ((State*)state)->type = TYPE_DECODER;
        ((State*)state)->value = speex_decoder_init(sample_rate == 8000 ? &speex_nb_mode :
                            (sample_rate == 16000 ? &speex_wb_mode : &speex_uwb_mode));
        if (((State*)state)->value == NULL) {
            PyErr_SetString(ModuleError, "failed to create decoder state");
            return NULL;
        }
    }
    else if (((State*)state)->type != TYPE_DECODER) {
        PyErr_SetString(ModuleError, "invalid state argument, not a decoder state");
        return NULL;
    }
    else {
        Py_XINCREF(state);
    }
    
    char* input_bytes = PyString_AsString(input);
    unsigned int input_size = PyString_Size(input);
    speex_bits_read_from(&((State*)state)->bits, input_bytes, input_size);
    
    int frame_size = 0;
    speex_decoder_ctl(((State*)state)->value, SPEEX_GET_FRAME_SIZE, &frame_size);
    if (frame_size < 0) {
        PyErr_SetString(ModuleError, "internal error in getting frame size");
        return NULL;
    }
    
    short output_frame[frame_size];
    speex_decode_int(((State*)state)->value, &((State*)state)->bits, output_frame);
    PyObject* output = PyString_FromStringAndSize((char*) output_frame, frame_size * 2);
    return Py_BuildValue("(NN)", output, state);
}


static PyObject*
pyaudio_resample(PyObject* self, PyObject* args, PyObject* kwargs)
{
    PyObject* input = NULL;
    int input_rate = 0;
    int output_rate = 0;
    int channels = 1;
    int quality = 3;
    PyObject* state = Py_None;
    
    static const char *kwlist[] = {
        "fragment", "input_rate", "output_rate", "quality", "state",
    NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "S|iiiO", (char **)kwlist,
            &input, &input_rate, &output_rate, &quality, &state)) {
        return NULL;
    }
    
    if (state == Py_None) {
        if (input_rate == 0 || output_rate == 0) {
            PyErr_SetString(ModuleError, "invalid or missing input_rate or output_rate argument");
            return NULL;
        }
        
        state = State_new(&StateType, NULL, NULL);
        ((State*)state)->type = TYPE_RESAMPLER;
        int err = 0;
        ((State*)state)->value = speex_resampler_init(channels, input_rate, output_rate, quality, &err);
        if (((State*)state)->value == NULL) {
            PyErr_SetString(ModuleError, "failed to create resampler state");
            return NULL;
        }
    }
    else if (((State*)state)->type != TYPE_RESAMPLER) {
        PyErr_SetString(ModuleError, "invalid state argument, not a resampler state");
        return NULL;
    }
    else {
        Py_XINCREF(state);
    }
    
    short* input_bytes = (short*) PyString_AsString(input);
    unsigned int input_size = PyString_Size(input) / 2;
    unsigned int output_size = input_size * output_rate / input_rate + 100;
    short output_bytes[output_size];
    
    speex_resampler_process_int((SpeexResamplerState*)(((State*)state)->value), 0, input_bytes, &input_size, output_bytes, &output_size);
    
    PyObject* output = PyString_FromStringAndSize((char*) output_bytes, output_size * 2);
    return Py_BuildValue("(NN)", output, state);
}


static PyObject*
pyaudio_preprocess(PyObject* self, PyObject* args, PyObject* kwargs)
{
    PyObject* input = NULL;
    int frame_size = 0;
    int sampling_rate = 0;
    PyObject* state = Py_None;
    
    static const char *kwlist[] = {
        "fragment", "frame_size", "sampling_rate", "state",
    NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "S|iiO", (char **)kwlist,
            &input, &frame_size, &sampling_rate, &state)) {
        return NULL;
    }
    
    if (state == Py_None) {
        if (frame_size == 0 || sampling_rate == 0) {
            PyErr_SetString(ModuleError, "invalid or missing frame_size or sampling_rate argument");
            return NULL;
        }
        
        state = State_new(&StateType, NULL, NULL);
        ((State*)state)->type = TYPE_PREPROCESS;
        ((State*)state)->value = speex_preprocess_state_init(frame_size, sampling_rate);
        if (((State*)state)->value == NULL) {
            PyErr_SetString(ModuleError, "failed to create preprocess state");
            return NULL;
        }
    }
    else if (((State*)state)->type != TYPE_PREPROCESS) {
        PyErr_SetString(ModuleError, "invalid state argument, not a proprocess state");
        return NULL;
    }
    else {
        Py_XINCREF(state);
    }
    
    short* input_bytes = (short*) PyString_AsString(input);
    unsigned int output_size = PyString_Size(input) / 2;
    short output_bytes[output_size];
    memcpy(output_bytes, input_bytes, output_size * 2);
    
    speex_preprocess_run((SpeexPreprocessState*)(((State*)state)->value), output_bytes);
    
    PyObject* output = PyString_FromStringAndSize((char*) output_bytes, output_size * 2);
    return Py_BuildValue("(NN)", output, state);
}


static PyObject*
pyaudio_cancel_echo(PyObject* self, PyObject* args, PyObject* kwargs)
{
    PyObject* input = NULL;
    PyObject* echo = NULL;
    int frame_size = 0;
    int filter_length = 0;
    PyObject* state = Py_None;
    
    static const char *kwlist[] = {
        "captured_fragment", "played_fragment", "frame_size", "filter_length", "state",
    NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "SS|iiO", (char **)kwlist,
            &input, &echo, &frame_size, &filter_length, &state)) {
        return NULL;
    }
    
    if (state == Py_None) {
        if (frame_size == 0 || filter_length == 0) {
            PyErr_SetString(ModuleError, "invalid or missing frame_size or filter_length argument");
            return NULL;
        }
        
        state = State_new(&StateType, NULL, NULL);
        ((State*)state)->type = TYPE_ECHO;
        ((State*)state)->value  = speex_echo_state_init(frame_size, filter_length);
        if (((State*)state)->value == NULL) {
            PyErr_SetString(ModuleError, "failed to create echo cancellation state");
            return NULL;
        }
    }
    else if (((State*)state)->type != TYPE_PREPROCESS) {
        PyErr_SetString(ModuleError, "invalid state argument, not an echo cancellation state");
        return NULL;
    }
    else {
        Py_XINCREF(state);
    }
    
    short* input_bytes = (short*) PyString_AsString(input);
    short* echo_bytes = (short*) PyString_AsString(echo);
    unsigned int output_size = PyString_Size(input) / 2;
    short output_bytes[output_size];

    speex_echo_cancellation((SpeexEchoState*)(((State*)state)->value), input_bytes, echo_bytes, output_bytes);    
    
    PyObject* output = PyString_FromStringAndSize((char*) output_bytes, output_size * 2);
    return Py_BuildValue("(NN)", output, state);
}

static PyMethodDef Module_methods[] = {
    {"lin2speex", (PyCFunction) pyaudio_lin2speex, METH_VARARGS | METH_KEYWORDS,
        PyDoc_STR("Convert samples in the audio fragment to Speex encoding and return this as a Python string.")},
    {"speex2lin", (PyCFunction) pyaudio_speex2lin, METH_VARARGS | METH_KEYWORDS,
        PyDoc_STR("Convert the Speex encoded fragment to linear fragment and return this as a Python string.")},
        
    {"resample", (PyCFunction) pyaudio_resample, METH_VARARGS | METH_KEYWORDS,
        PyDoc_STR("Convert the sampling rate of the linear fragment and return this as a Python string.")},
    {"preprocess", (PyCFunction) pyaudio_preprocess, METH_VARARGS | METH_KEYWORDS,
        PyDoc_STR("Apply preprocessing steps to the linear fragment and return this as a Python string.")},
    {"cancel_echo", (PyCFunction) pyaudio_cancel_echo, METH_VARARGS | METH_KEYWORDS,
        PyDoc_STR("Apply echo cancellation steps to the captured and played linear fragments and return this as a Python string.")},
        
    {NULL, NULL, 0, NULL}  /* Sentinel */
};


PyMODINIT_FUNC
initaudiospeex(void)
{
    PyObject *m;
    
    StateType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&StateType) < 0)
        return;
    
    m = Py_InitModule3("audiospeex", Module_methods, "speex voice codec and quality engine based on the open source speex library");
    if (m == NULL)
        return;

    ModuleError = PyErr_NewException((char*) "audiospeex.error", NULL, NULL);
    Py_INCREF(ModuleError);
    PyModule_AddObject(m, "error", ModuleError);
    
    Py_INCREF(&StateType);
    PyModule_AddObject(m, "State", (PyObject *)&StateType);
}

