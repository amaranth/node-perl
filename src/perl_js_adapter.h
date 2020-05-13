#ifndef PERL_JS_ADAPTER_H
#define PERL_JS_ADAPTER_H

#include <napi.h>
#include <perl.h>

class PerlJsAdapter : public Napi::ObjectWrap<PerlJsAdapter> {
protected:
	PerlInterpreter* my_perl;
public:
	static Napi::Object Init(Napi::Env env, Napi::Object exports);
	PerlJsAdapter(const Napi::CallbackInfo& info);
	PerlJsAdapter(const Napi::CallbackInfo& info, PerlInterpreter* myp);
private:
	static Napi::FunctionReference constructor;

	Napi::Value GetValue(const Napi::CallbackInfo& info);
	Napi::Value PlusOne(const Napi::CallbackInfo& info);

	double value_;
};

#endif