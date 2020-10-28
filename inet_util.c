#include <Python.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef AF_INET6
#ifndef ENABLE_IPV6
#define ENABLE_IPV6
#endif
#endif

#ifndef Py_MAX
/* Maximum value between x and y */
#define Py_MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#if (_WIN32_WINNT <= _WIN32_WINNT_WINXP)
int inet_pton(int af, const char *csrc, void *dst) {
  char *src;

  if (csrc == NULL || (src = strdup(csrc)) == NULL) {
    _set_errno(ENOMEM);
    return 0;
  }

  switch (af) {
    case AF_INET: {
      struct sockaddr_in si4;
      INT r;
      INT s = sizeof(si4);

      si4.sin_family = AF_INET;
      r = WSAStringToAddress(src, AF_INET, NULL, (LPSOCKADDR)&si4, &s);
      free(src);
      src = NULL;

      if (r == 0) {
        memcpy(dst, &si4.sin_addr, sizeof(si4.sin_addr));
        return 1;
      }
    } break;

    case AF_INET6: {
      struct sockaddr_in6 si6;
      INT r;
      INT s = sizeof(si6);

      si6.sin6_family = AF_INET6;
      r = WSAStringToAddress(src, AF_INET6, NULL, (LPSOCKADDR)&si6, &s);
      free(src);
      src = NULL;

      if (r == 0) {
        memcpy(dst, &si6.sin6_addr, sizeof(si6.sin6_addr));
        return 1;
      }
    } break;

    default:
      _set_errno(EAFNOSUPPORT);
      return -1;
  }

  /* the call failed */
  {
    int le = WSAGetLastError();

    if (le == WSAEINVAL) return 0;

    _set_errno(le);
    return -1;
  }
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Python module and C API name */
#define PyInetUtil_MODULE_NAME "inet_util"
#define PyInetUtil_CAPI_NAME "CAPI"
#define PyInetUtil_CAPSULE_NAME PyInetUtil_MODULE_NAME "." PyInetUtil_CAPI_NAME
#define PyInetUtilModule_ImportModuleAndAPI() \
  PyCapsule_Import(PyInetUtil_CAPSULE_NAME, 1)

PyDoc_STRVAR(inet_pton_doc,
             "inet_pton(af, ip) -> packed IP address string\n\
\n\
Convert an IP address from string format to a packed string suitable\n\
for use with low-level network functions.");

static PyObject *socket_inet_pton(PyObject *self, PyObject *args) {
  int af;
  const char *ip;
  int retval;
#ifdef ENABLE_IPV6
  char packed[Py_MAX(sizeof(struct in_addr), sizeof(struct in6_addr))];
#else
  char packed[sizeof(in_addr)];
#endif
  if (!PyArg_ParseTuple(args, "is:inet_pton", &af, &ip)) {
    return NULL;
  }

#if !defined(ENABLE_IPV6) && defined(AF_INET6)
  if (af == AF_INET6) {
    PyErr_SetString(PyExc_OSError, "can't use AF_INET6, IPv6 is disabled");
    return NULL;
  }
#endif

  retval = inet_pton(af, ip, packed);
  if (retval < 0) {
    PyErr_SetFromErrno(PyExc_OSError);
    return NULL;
  } else if (retval == 0) {
    PyErr_SetString(PyExc_OSError,
                    "illegal IP address string passed to inet_pton");
    return NULL;
  } else if (af == AF_INET) {
    return PyBytes_FromStringAndSize(packed, sizeof(struct in_addr));
#ifdef ENABLE_IPV6
  } else if (af == AF_INET6) {
    return PyBytes_FromStringAndSize(packed, sizeof(struct in6_addr));
#endif
  } else {
    PyErr_SetString(PyExc_OSError, "unknown address family");
    return NULL;
  }
}

PyDoc_STRVAR(inet_ntop_doc,
             "inet_ntop(af, packed_ip) -> string formatted IP address\n\
\n\
Convert a packed IP address of the given family to string format.");

static PyObject *socket_inet_ntop(PyObject *self, PyObject *args) {
  int af;
#if PY_MAJOR_VERSION >= 3
  Py_buffer packed_ip;
#else
  char *packed_ip;
  int len;
#endif
  const char *retval;
#ifdef ENABLE_IPV6
  char ip[Py_MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];
#else
#if PY_MAJOR_VERSION >= 3
  char ip[INET_ADDRSTRLEN];
#else
  char ip[INET_ADDRSTRLEN + 1];
#endif
#endif

#if PY_MAJOR_VERSION >= 3
  if (!PyArg_ParseTuple(args, "iy*:inet_ntop", &af, &packed_ip))
#else
  /* Guarantee NUL-termination for PyString_FromString() below */
  memset((void *)&ip[0], '\0', sizeof(ip));
  if (!PyArg_ParseTuple(args, "is#:inet_ntop", &af, &packed_ip, &len))
#endif
  {
    return NULL;
  }

  if (af == AF_INET) {
#if PY_MAJOR_VERSION >= 3
    if (packed_ip.len != sizeof(struct in_addr))
#else
    if (len != sizeof(struct in_addr))
#endif
    {
      PyErr_SetString(PyExc_ValueError,
                      "invalid length of packed IP address string");
#if PY_MAJOR_VERSION >= 3
      PyBuffer_Release(&packed_ip);
#endif
      return NULL;
    }
#ifdef ENABLE_IPV6
  } else if (af == AF_INET6) {
#if PY_MAJOR_VERSION >= 3
    if (packed_ip.len != sizeof(struct in6_addr))
#else
    if (len != sizeof(struct in6_addr))
#endif
    {
      PyErr_SetString(PyExc_ValueError,
                      "invalid length of packed IP address string");
#if PY_MAJOR_VERSION >= 3
      PyBuffer_Release(&packed_ip);
#endif
      return NULL;
    }
#endif
  } else {
    PyErr_Format(PyExc_ValueError, "unknown address family %d", af);
#if PY_MAJOR_VERSION >= 3
    PyBuffer_Release(&packed_ip);
#endif
    return NULL;
  }

  /* inet_ntop guarantee NUL-termination of resulting string. */
#if PY_MAJOR_VERSION >= 3
  retval = inet_ntop(af, packed_ip.buf, ip, sizeof(ip));
  PyBuffer_Release(&packed_ip);
#else
  retval = inet_ntop(af, packed_ip, ip, sizeof(ip));
#endif
  if (!retval) {
    PyErr_SetFromErrno(PyExc_OSError);
    return NULL;
  } else {
#if PY_MAJOR_VERSION >= 3
    return PyUnicode_FromString(retval);
#else
    return PyString_FromString(retval);
#endif
  }
}

static PyMethodDef inet_util_methods[] = {
    {"inet_pton", socket_inet_pton, METH_VARARGS, inet_pton_doc},
    {"inet_ntop", socket_inet_ntop, METH_VARARGS, inet_ntop_doc},

    {NULL, NULL} /* Sentinel */
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef inet_util_module = {PyModuleDef_HEAD_INIT,
                                              PyInetUtil_MODULE_NAME,
                                              NULL,
                                              -1,
                                              inet_util_methods,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL};

PyMODINIT_FUNC PyInit_inet_util(void) {
  return PyModule_Create(&inet_util_module);
}
#else
PyMODINIT_FUNC initinet_util(void) {
  Py_InitModule(PyInetUtil_MODULE_NAME, inet_util_methods);
}
#endif

#ifdef __cplusplus
}
#endif
