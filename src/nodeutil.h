#pragma once

// #define BUILDING_NODE_EXTENSION
// #include <napi.h>
#include <uv.h>
// #include <node_buffer.h>
// #include <v8.h>

/* ******************************************************
 * exception utilities
 */
#define THROW_TYPE_ERROR(str) \
    Napi::ThrowError::New(env, Exception::TypeError(Napi::New(env, str)))

/* ******************************************************
 * Argument utilities.
 */
#define ARG_EXT(I, VAR) \
    if (args.Length() <= (I) || !args[I]->IsExternal()) { \
        Napi::ThrowError(v8::Exception::TypeError( \
            Napi::String::New(env, "Argument " #I " must be an external"))); \
	} \
    Napi::External VAR = args[I].As<Napi::External>();

/**
 * ARG_STR(0, src);
 *
 * see http://blog.64p.org/entry/2012/09/02/101609
 */
#define ARG_STR(I, VAR) \
    if (args.Length() <= (I)) { \
        Napi::ThrowError(v8::Exception::TypeError( \
            Napi::String::New(env, "Argument " #I " must be a string"))); \
	} \
    Napi::String VAR(env, args[I]->ToString());

#define ARG_OBJ(I, VAR) \
    if (args.Length() <= (I) || !args[I].IsObject()) { \
        Napi::ThrowError(v8::Exception::TypeError( \
            Napi::String::New(env, "Argument " #I " must be a object"))); \
	} \
    Napi::Object VAR = args[I].As<Napi::Object>();

#define ARG_INT(I, VAR) \
    if (args.Length() <= (I) || !args[I].IsNumber()) { \
        Napi::ThrowError(v8::Exception::TypeError( \
            Napi::String::New(env, "Argument " #I " must be an integer"))); \
	} \
    int32_t VAR = args[I].As<Napi::Number>().Int32Value();

#define ARG_BUF(I, VAR) \
    if (args.Length() <= (I) || !args[I].IsBuffer()) { \
        Napi::ThrowError(v8::Exception::TypeError( \
            Napi::String::New(env, "Argument " #I " must be an Buffer"))); \
	} \
    void * VAR = Buffer::Data(args[I]->ToObject());

/* ******************************************************
 * Class construction utilities
 */
#define SET_ENUM_VALUE(target, _value) \
        target.Set(Napi::New(env, #_value), \
                Napi::Number::New(env, _value), \
                static_cast<napi_property_attributes>(napi_enumerable | napi_configurable))

