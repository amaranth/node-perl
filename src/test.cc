#include <napi.h>

#include <iostream>

#include <EXTERN.h>               /* from the Perl distribution     */
#include <perl.h>                 /* from the Perl distribution     */
#include <embed.h>
#include "ppport.h"
#include "config.h"
#include "nodeutil.h"

#ifdef New
#undef New
#endif

using namespace Napi;

// src/perlxsi.cc
EXTERN_C void xs_init(pTHX);

class PerlFoo {
protected:
	PerlInterpreter* my_perl;

	PerlFoo() : my_perl(NULL) { }
	PerlFoo(PerlInterpreter* myp) : my_perl(myp) { }
public:
	Value perl2js(const CallbackInfo& info, SV* sv) {
		Env env = info.Env();
		EscapableHandleScope scope(env);

		// see xs-src/pack.c in msgpack-perl
		SvGETMAGIC(sv);

		if (SvPOKp(sv)) {
			STRLEN len;
			const char* s = SvPV(sv, len);
			return scope.Escape(String::New(env, s, len));
		}
		if (SvNOK(sv)) {
			return scope.Escape(Number::New(env, SvNVX(sv)));
		}
		if (SvIOK(sv)) {
			return scope.Escape(Number::New(env, SvIVX(sv)));
		}
		if (SvROK(sv)) {
			return scope.Escape(this->perl2js_rv(sv));
		}
		if (!SvOK(sv)) {
			return scope.Escape(env.Undefined());
		}
		if (isGV(sv)) {
			std::cerr << "Cannot pass GV to v8 world" << std::endl;
			return scope.Escape(env.Undefined());
		}
		sv_dump(sv);
		TypeError::New(env, String::New(env, "node-perl-simple doesn't support this type")).ThrowAsJavaScriptException();

		return scope.Escape(env.Undefined());
		// TODO: return callback function for perl code.
		// Perl callbacks should be managed by objects.
		// TODO: Handle async.
	}

	SV* js2perl(Value val) const;

	Value CallMethod2(const CallbackInfo& info, bool in_list_context) {
		Env env = info.Env();
		EscapableHandleScope scope(env);
		Value method = info[0];
		if (!method.IsString()) {
			TypeError::New(env, "method name must be a string").ThrowAsJavaScriptException();
		}
		return this->CallMethod2(info, NULL, method.ToString().Utf8Value().c_str(), 1, info, in_list_context);
	}
	Value CallMethod2(const CallbackInfo& info, SV* self, const char* method, int offset, const CallbackInfo& args, bool in_list_context) {
		Env env = info.Env();
		EscapableHandleScope scope(env);

		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(SP);
		if (self) {
			XPUSHs(self);
		}
		for (int i = offset; i < args.Length(); i++) {
			SV* arg = this->js2perl(args[i]);
			if (!arg) {
				PUTBACK;
				SPAGAIN;
				PUTBACK;
				FREETMPS;
				LEAVE;
				Error::New(env, String::New(env, "There is no way to pass this value to perl world.")).ThrowAsJavaScriptException();

				return scope.Escape(env.Undefined());
			}
			XPUSHs(arg);
		}
		PUTBACK;
		if (in_list_context) {
			int n = self ? call_method(method, G_ARRAY | G_EVAL) : call_pv(method, G_ARRAY | G_EVAL);
			SPAGAIN;
			if (SvTRUE(ERRSV)) {
				POPs;
				PUTBACK;
				FREETMPS;
				LEAVE;
				Error::New(env, Value(env, this->perl2js(info, ERRSV)).ToString()).ThrowAsJavaScriptException();

				return scope.Escape(env.Undefined());
			}
			Array retval = Array::New(env);
			for (int i = 0; i < n; i++) {
				SV* retsv = POPs;
				retval.Set(n - i - 1, this->perl2js(info, retsv));
			}
			PUTBACK;
			FREETMPS;
			LEAVE;
			return scope.Escape(retval);
		}
		if (self) {
			call_method(method, G_SCALAR | G_EVAL);
		}
		else {
			call_pv(method, G_SCALAR | G_EVAL);
		}
		SPAGAIN;
		if (SvTRUE(ERRSV)) {
			POPs;
			PUTBACK;
			FREETMPS;
			LEAVE;
			Error::New(env, Value(env, this->perl2js(info, ERRSV)).ToString()).ThrowAsJavaScriptException();

			return scope.Escape(env.Undefined());
		}
		else {
			SV* retsv = TOPs;
			Value retval = this->perl2js(info, retsv);
			PUTBACK;
			FREETMPS;
			LEAVE;
			return scope.Escape(retval);
		}
	}

	Value perl2js_rv(SV* rv);
};


/**
  * Load lazily libperl for dynamic loaded xs.
  * I don't know the better way to resolve symbols in xs.
  * patches welcome.
  *
  * And this code is not portable.
  * patches welcome.
  */
#ifdef WIN32
#define PSAPI_VERSION 1
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
static Value InitPerl(const CallbackInfo & info) {
	Env env = info.Env();
	auto uMode = SetErrorMode(SEM_FAILCRITICALERRORS);
	auto hModule = LoadLibraryEx(LIBPERL_DIR LIBPERL, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
	auto error = GetLastError();
	if (hModule) {
		FreeLibrary(hModule);
	}
	else {
		CHAR errorBuffer[65535];
		auto pos = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorBuffer, sizeof(errorBuffer), NULL);
		if (pos > 0)
		{
			if (errorBuffer[pos - 2] == '\r' && errorBuffer[pos - 1] == '\n')
				errorBuffer[pos - 2] = '\0';

			std::cerr << "Error loading " << LIBPERL_DIR LIBPERL << ": " << errorBuffer << std::endl;
		}
	}
	SetErrorMode(uMode);
	return env.Undefined();
}
#else
#include <dlfcn.h>
static Napi::Value InitPerl(const Napi::CallbackInfo& info) {
	void* lib = dlopen(LIBPERL, RTLD_LAZY | RTLD_GLOBAL);
	if (lib) {
		dlclose(lib);
		return env.Undefined();
	}
	else {
		std::cerr << dlerror() << std::endl;
		return env.Undefined();
		// Napi::Error::New(env, v8::Exception::Error(Napi::New(env, dlerror()))).ThrowAsJavaScriptException();
		return env.Null();
	}
}
#endif

/* extern "C" Object init(Env env, Object exports) {
	{
		FunctionReference t = Function::New(env, InitPerl);
		target.Set(String::New(env, "InitPerl"), t->GetFunction());
	}

	NodePerl::Init(env, target, module);
	NodePerlObject::Init(env, target, module);
	NodePerlClass::Init(env, target, module);
	NodePerlMethod::Init(env, target, module);
} */



Napi::Object Init(Napi::Env env, Napi::Object exports) {
	exports.Set(
		Napi::String::New(env, "InitPerl"),
		Napi::Function::New(env, InitPerl)
	);
	return exports;
}

NODE_API_MODULE(perl, Init);
