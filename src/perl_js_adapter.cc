#include <napi.h>
#include "perl_js_adapter.h"

#ifdef New
#undef New
#endif

Napi::FunctionReference PerlJsAdapter::constructor;

Napi::Object PerlJsAdapter::Init(Napi::Env env, Napi::Object exports) {
	Napi::HandleScope scope(env);

	Napi::Function func =
		DefineClass(env,
			"PerlJsAdapter",
			{
				 InstanceMethod("value", &PerlJsAdapter::GetValue),
				 InstanceMethod("plusOne", &PerlJsAdapter::PlusOne),
			});

	constructor = Napi::Persistent(func);
	constructor.SuppressDestruct();

	exports.Set("MyObject", func);
	return exports;
}

PerlJsAdapter::PerlJsAdapter(const Napi::CallbackInfo& info): PerlJsAdapter::PerlJsAdapter(info, nullptr) {}

PerlJsAdapter::PerlJsAdapter(const Napi::CallbackInfo& info, PerlInterpreter* myp): Napi::ObjectWrap<PerlJsAdapter>(info), my_perl(myp)
{
	Napi::Env env = info.Env();
	Napi::HandleScope scope(env);

	int length = info.Length();

	if (length <= 0 || !info[0].IsNumber()) {
		Napi::TypeError::New(env, "Number expected").ThrowAsJavaScriptException();
		return;
	}

	Napi::Number value = info[0].As<Napi::Number>();
	this->value_ = value.DoubleValue();
	
}


Napi::Value PerlJsAdapter::GetValue(const Napi::CallbackInfo& info) {
	double num = this->value_;

	return Napi::Number::New(info.Env(), num);
}

Napi::Value PerlJsAdapter::PlusOne(const Napi::CallbackInfo& info) {
	this->value_ = this->value_ + 1;

	return PerlJsAdapter::GetValue(info);
}