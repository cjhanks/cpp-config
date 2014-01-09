
#include <Python.h>
#include "appconf/config.hh"


#ifdef __cplusplus
extern "C" {
#endif

/*=--------------------------------------------------------------------------=*/

static PyObject* ConfigError;
static PyObject* ConfigIOError;
//static PyObject* ConfigKeyError;
//static PyObject* ConfigTypeError;
static PyObject* ConfigParseException;


/*=--------------------------------------------------------------------------=*/
static PyObject*
new_constant(kwarg_const* cfg) {
    PyObject* tmp = 0x0;

    switch (cfg->type()) {
        case kwarg::BOOL:
            tmp = cfg->as<bool>() ? Py_True : Py_False;
            break;

        case kwarg::FLOATING:
            tmp = PyFloat_FromDouble(cfg->as<double>());
            break;

        case kwarg::INTEGRAL:
            tmp = PyLong_FromLongLong(cfg->as<int64_t>());
            break;

        case kwarg::STRING:
            tmp = PyString_FromString(cfg->as<std::string>().c_str());
            break;

        default:
            return 0x0;
    }

    return tmp;
}

static PyObject*
new_vector(kwarg_vector* cfg) {
    PyObject* tuple = PyTuple_New((*cfg)->size());

    for (size_t i = 0; i < (*cfg)->size(); ++i) {
        if (0 != PyTuple_SetItem(tuple, i, new_constant((*cfg)->at(i)))) {
            PyObject_Free(tuple);
            PyErr_SetString(ConfigParseException, "Invalid arg in vector (?)");
            return 0x0;
        }
    }

    return tuple;
}

static PyObject*
new_section(config_section* cfg) {
    PyObject* dict = PyDict_New();

    if (0x0 == dict)
        goto exit_fail;

    for (auto it  = cfg->cbegin(); it != cfg->cend(); ++it) {
        PyObject* tmp;
        PyObject* name = PyString_FromString(it->first.c_str());
        kwarg* ptr = it->second;

        switch (ptr->type()) {
            case kwarg::UNDEFINED:
                goto loop_exit_failure;

            case kwarg::SECTION:
                tmp = new_section(static_cast<config_section*>(ptr));
                break;

            case kwarg::VECTOR:
                tmp = new_vector(static_cast<kwarg_vector*>(ptr));
                break;

            default:
                tmp = new_constant(static_cast<kwarg_const*>(ptr));
                break;
        }

        if (0x0 == tmp)
            goto loop_exit_failure;

        if (0 != PyDict_SetItem(dict, name, tmp))
            goto loop_exit_failure;

        continue;

loop_exit_failure:
        if (name)
            PyObject_Free(name);

        goto exit_fail;
    }

    return dict;

exit_fail:
    if (dict)
        PyObject_Free(dict);

    return 0x0;
}

static PyObject*
parse_to_dict(PyObject *self, PyObject *args) {
    config* cfg = 0x0;
    const char* file_path;

    if (! PyArg_ParseTuple(args, "s", &file_path))
        return 0x0;

    try {
        cfg = new config(file_path);
    } catch (const config_io_error &e) {
        PyErr_SetString(ConfigIOError, e.what());
    } catch (const config_parse_exception& e) {
        PyErr_SetString(ConfigParseException, e.what());
    } catch (const std::runtime_error &e) {
        PyErr_SetString(ConfigError, e.what());
    } catch (...) {
        PyErr_SetString(ConfigError, "UNKNOWN ERROR");
    }

    if (0x0 == cfg)
        return 0x0;

    return new_section(static_cast<config_section*>(cfg));
}

/*=--------------------------------------------------------------------------=*/

static PyMethodDef appconf_module_methods [] = {
        {
                "parse_to_dict"
              , (PyCFunction) parse_to_dict
              , METH_VARARGS
              , ""
        },
        { 0x0, 0x0, 0x0, 0x0 }
};

#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif

PyMODINIT_FUNC
init__appconf() {
        PyObject *m;

        if ((m = Py_InitModule3("appconf.__appconf", appconf_module_methods, 0x0))
               == 0x0)
                return;

        ConfigError = PyErr_NewException("appconf.ConfigError", 0x0, 0x0);
        ConfigIOError = PyErr_NewException("appconf.ConfigIOError"
                                         , ConfigError, 0x0);
        ConfigParseException = PyErr_NewException("appconf.ConfigParseException"
                                                 , ConfigError, 0x0);
        Py_INCREF(ConfigError);
        Py_INCREF(ConfigIOError);
        Py_INCREF(ConfigParseException);

        PyModule_AddObject(m, "ConfigError", ConfigError);
        PyModule_AddObject(m, "ConfigIOError", ConfigIOError);
        PyModule_AddObject(m, "ConfigParseException", ConfigParseException);
}

#ifdef __cplusplus
}
#endif
