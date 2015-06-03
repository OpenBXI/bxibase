BXI Base Library                                   {#index}
=====================

[TOC]

BXI Base Library for C Language                    {#BXIBase4C}
================================

## Overview ##                                           {#Overview_c}


The BXI Base Library is the basic library most bxi projects rely on. 
It provides the following plain C modules:

- mem.h: used for memory allocation and release
- str.h: used for string manipulation
- err.h: used for error handling
- time.h: provide timing related functions with error handling
- zmq.h: provide easy wrapper around ZeroMQ functions
- log.h: a high performance logging library

## BXI General API Conventions ##                        {#Conventions_c}

### Naming ### {#Naming}

- All BXI C functions are prefixed by `bxi` in order to organize namespace.
- Enumerations have the following format: `bxi<module>_name_e` 
    (see `::bxilog_level_e`)
- Structures have the following format: `bxi<module>_name_s` and are mostly unused 
    as such. Pointers on them are prefered, using 
    `typedef struct bxi<module>_name_s bxi_<module>_name_p;` 
    (see [Object like API](#OO_c)).

### Memory allocation ###                                       {#Allocation_c}
Use bximem_calloc() for memory allocation: this provides various guarantees:

1. returned memory has been nullified
2. the underlying returned code has been checked.

Also, use macro BXIFREE() for releasing a pointer: this provides various guarantees:

1. releasing a NULL pointer cause no problem
2. the given pointer is nullified.

See the following example:

    struct mystruct * s = bximem_calloc(sizeof(*s));
    // do something with 's'
    BXIFREE(s);         // release 's'
    assert(NULL == s);  // This is always true


### Module Initialization ###                                   {#Initialization_c}
Module initialization when required is done using `bxi*_init()`, 
and library cleanup is done using `bxi*_finalize()`. See for example
bxilog_init() and bxilog_finalize().

### Object like API ###                                         {#OO_c}
Object like module provides `bxi*_new()` and `bxi*_destroy()` functions. 
See for example: bxizmq_zocket_new() and bxizmq_zocket_destroy().
Note that `bxi*_destroy()` 
actually *nullify* the given pointer as shown in the following example:

    void * result;
    bxierr_p err = bxizmq_zocket_new(..., &result);
    ...
    // Do something with result
    bxizmq_zocket_destroy(&result);
    assert(NULL == result);  // This is true

Note also that objects are always refered to through pointers and 
their type name always ends with an '_p' to clearly state it. See for example:
`bxierr_p`, `bxilog_p`. Therefore, instantiating a BXI object, using it
and destroying it, always look like the following code snippet: 

    bxifoo_p object = bxifoo_new(...);
    ...
    result_p result = bxifoo_function(object, ...);
    ...
    bxifoo_destroy(&object);

### Error handling ###                                          {#Errors_c}
Dealing with error in C is known to be difficult. The BXI base library offers 
the err.h module that helps a lot in this regards. The following convention holds 
therefore for most BXI functions:

- Functions that deal with errors internally (using assert() internally, 
  or error() for example) use a traditionnal signature. See for example:
  bximem_calloc(), bxistr_new(), bxilog_get_level().
  
- Functions that ask the caller to deal with the error (most functions actually),
  return a `::bxierr_p` instance. If they must also provide a result, 
  a *pointer* on the result is taken as the *last* argument. See for example: 
  `bxizmq_zocket_new()`, `bxitime_get()`.

BXI Base Library for Python Language               {#BXIBase4Python}
=====================================

## Overview ##                                           {#Overview_py}

The BXI Base Library also provides a binding for the Python language, 
implemented using the C Foreign Function Interface (CFFI) for Python.

Since Python provides a very rich set of modules, only the following modules are provided 
as Python binding:


| BXI Base C Module | BXI Base Python Module | Description                      |
| ----------------- | -----------------------| -------------------------------- |
| err.h             | bxi.base.err           | Error Handling                   |
| log.h             | bxi.base.log           | High Performance Logging module  |





