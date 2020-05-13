#include <napi.h>

#include <string>
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

// src/perlxsi.cc
EXTERN_C void xs_init (pTHX);

class NodePerl;

#define INTERPRETER_NAME "node-perl-simple"

// TODO: pass the NodePerlObject to perl5 world.

class PerlFoo {
protected:
    PerlInterpreter *my_perl;

    PerlFoo(): my_perl(NULL) { }
    PerlFoo(PerlInterpreter *myp): my_perl(myp) { }
public:
	Napi::Value perl2js(const Napi::CallbackInfo& info, SV * sv) {
		Napi::Env env = info.Env();
		Napi::EscapableHandleScope scope(env);

        // see xs-src/pack.c in msgpack-perl
        SvGETMAGIC(sv);

        if (SvPOKp(sv)) {
            STRLEN len;
            const char *s = SvPV(sv, len);
            return scope.Escape(Napi::String::New(env, s, len));
        } else if (SvNOK(sv)) {
            return scope.Escape(Napi::Number::New(env, SvNVX(sv)));
        } else if (SvIOK(sv)) {
            return scope.Escape(Napi::Number::New(env, SvIVX(sv)));
        } else if (SvROK(sv)) {
            return scope.Escape(this->perl2js_rv(sv));
        } else if (!SvOK(sv)) {
            return scope.Escape(env.Undefined());
        } else if (isGV(sv)) {
            std::cerr << "Cannot pass GV to v8 world" << std::endl;
            return scope.Escape(env.Undefined());
        } else {
            sv_dump(sv);
			Napi::TypeError::New(env, Napi::String::New(env, "node-perl-simple doesn't support this type")).ThrowAsJavaScriptException();

            return scope.Escape(env.Undefined());
        }
        // TODO: return callback function for perl code.
        // Perl callbacks should be managed by objects.
        // TODO: Handle async.
    }

    SV* js2perl(Napi::Value val) const;

	Napi::Value CallMethod2(const Napi::CallbackInfo& info, bool in_list_context) {
		Napi::Env env = info.Env();
		Napi::EscapableHandleScope scope(env);
		Napi::Value method = info[0];
		if (!method.IsString()) {
			Napi::TypeError::New(env, "method name must be a string").ThrowAsJavaScriptException();
		}
        return this->CallMethod2(info, NULL, method.ToString().Utf8Value().c_str(), 1, info, in_list_context);
    }
    Napi::Value CallMethod2(const Napi::CallbackInfo& info, SV * self, const char *method, int offset, const Napi::CallbackInfo& args, bool in_list_context) {
		Napi::Env env = info.Env();
        Napi::EscapableHandleScope scope(env);

        dSP;
        ENTER;
        SAVETMPS;
        PUSHMARK(SP);
        if (self) {
            XPUSHs(self);
        }
        for (int i=offset; i<args.Length(); i++) {
            SV * arg = this->js2perl(args[i]);
            if (!arg) {
                PUTBACK;
                SPAGAIN;
                PUTBACK;
                FREETMPS;
                LEAVE;
                Napi::Error::New(env, Napi::String::New(env, "There is no way to pass this value to perl world.")).ThrowAsJavaScriptException();

                return scope.Escape(env.Undefined());
            }
            XPUSHs(arg);
        }
        PUTBACK;
        if (in_list_context) {
            int n = self ? call_method(method, G_ARRAY|G_EVAL) : call_pv(method, G_ARRAY|G_EVAL);
            SPAGAIN;
            if (SvTRUE(ERRSV)) {
                POPs;
                PUTBACK;
                FREETMPS;
                LEAVE;
                Napi::Error::New(env, Napi::Value(env, this->perl2js(info, ERRSV)).ToString()).ThrowAsJavaScriptException();

                return scope.Escape(env.Undefined());
            } else {
				Napi::Array retval = Napi::Array::New(env);
                for (int i=0; i<n; i++) {
                    SV* retsv = POPs;
                    retval.Set(n-i-1, this->perl2js(info, retsv));
                }
                PUTBACK;
                FREETMPS;
                LEAVE;
                return scope.Escape(retval);
            }
        } else {
            if (self) {
                call_method(method, G_SCALAR|G_EVAL);
            } else {
                call_pv(method, G_SCALAR|G_EVAL);
            }
            SPAGAIN;
            if (SvTRUE(ERRSV)) {
                POPs;
                PUTBACK;
                FREETMPS;
                LEAVE;
                Napi::Error::New(env, Napi::Value(env, this->perl2js(info, ERRSV)).ToString()).ThrowAsJavaScriptException();

                return scope.Escape(env.Undefined());
            } else {
                SV* retsv = TOPs;
                Napi::Value retval = this->perl2js(info, retsv);
                PUTBACK;
                FREETMPS;
                LEAVE;
                return scope.Escape(retval);
            }
        }
    }

    Napi::Value perl2js_rv(SV * rv);
};

class NodePerlMethod : public Napi::ObjectWrap<NodePerlMethod>, public PerlFoo {
public:
    SV * sv_;
    std::string name_;

    NodePerlMethod(const Napi::CallbackInfo& info, SV *sv, const char * name, PerlInterpreter *myp): Napi::ObjectWrap<NodePerlMethod>(info), PerlFoo(myp), sv_(sv), name_(name) {
        SvREFCNT_inc(sv);
    }
    ~NodePerlMethod() {
        SvREFCNT_dec(sv_);
    }
	// static Napi::FunctionReference constructor;

	static inline Napi::FunctionReference & constructor() {
		static Napi::FunctionReference my_constructor;
		return my_constructor;
	}

    static Napi::Object Init(Napi::Env env, Napi::Object exports) {

		Napi::HandleScope scope(env);

		Napi::Function func =
			DefineClass(env,
				"MyObject",
				{ 
					InstanceMethod("plusOne", &NodePerlMethod::call),
					InstanceMethod("value", &NodePerlMethod::callList)
				});

		constructor = Napi::Persistent(func);
		constructor.SuppressDestruct();

		exports.Set("MyObject", func);
		return exports;

		// Prepare constructor template
		Napi::FunctionReference tpl = Napi::Function::New(env, NodePerlMethod::New);
		tpl->SetClassName(Napi::String::New(env, "NodePerlMethod"));

		Napi::SetCallAsFunctionHandler(tpl->InstanceTemplate(), NodePerlMethod::call);
		        
        Napi::SetPrototypeMethod(tpl, "call", NodePerlMethod::call);
        Napi::SetPrototypeMethod(tpl, "callList", NodePerlMethod::callList);
		//Napi::SetPrototypeMethod(tpl, "eval", NodePerl::evaluate);

		constructor.Reset(tpl);

		constructor().Reset(Napi::GetFunction(tpl));
		(target).Set(Napi::String::New(env, "NodePerlMethod"),
			Napi::GetFunction(tpl));
    }
    static Napi::Value New(const Napi::CallbackInfo& info) {
		if (info.IsConstructCall()) {
			const Napi::CallbackInfo& args = info;
			auto jssv = info[0];
			auto jsmyp = info[1];
			auto jsname = info[2];
			SV* sv = jssv.As<SV*>();
			PerlInterpreter* myp = jsmyp.As<PerlInterpreter*>();
			auto name = jsname.ToString().Utf8Value().c_str();
			NodePerlMethod *obj = new NodePerlMethod(info, sv, name, myp);
			obj->Wrap(info.This());
			return info.This();
		}
		else {
			const int argc = 3;
			Napi::Value argv[argc] = { info[0], info[1], info[2] };
			return Napi::NewInstance(Napi::New(env, constructor()), argc, argv);
		}
    }
    static Napi::Value call(const Napi::CallbackInfo& info) {
        return Unwrap(info.This().As<Napi::Object>())->Call(info, false);
    }
    static Napi::Value callList(const Napi::CallbackInfo& info) {
        return Unwrap(info.This().As<Napi::Object>())->Call(info, true);
    }

    Napi::Value Call(const Napi::CallbackInfo& info, bool in_list_context) {
        return this->CallMethod2(info, this->sv_, name_.c_str(), 0, info, in_list_context);
    }
};

class NodePerlObject : public Napi::ObjectWrap<NodePerlObject>, public PerlFoo {
protected:
    SV * sv_;

public:
	static Napi::FunctionReference constructor;

	static inline Napi::FunctionReference & constructor() {
		static Napi::FunctionReference my_constructor;
		return my_constructor;
	}

	static Napi::Object Init(Napi::Env env, Napi::Object exports) {
		// Prepare constructor template
		Napi::FunctionReference tpl = Napi::Function::New(env, New);
		tpl->SetClassName(Napi::String::New(env, "NodePerlObject"));

		Napi::SetNamedPropertyHandler(tpl->InstanceTemplate(), NodePerlObject::GetNamedProperty);

		Napi::SetPrototypeMethod(tpl, "getClassName", NodePerlObject::getClassName);
		
		constructor.Reset(tpl);

		constructor().Reset(Napi::GetFunction(tpl));
		(target).Set(Napi::String::New(env, "NodePerlObject"),
			Napi::GetFunction(tpl));
    }

    static void GetNamedProperty(
		Napi::String propertyName,
		const Napi::PropertyCallbackInfo<v8::Value>& info
	) {
        if (info.This().InternalFieldCount() < 1 || info.Data().IsEmpty()) {
            Napi::Error::New(env, v8::Exception::Error(Napi::String::New(env, "SetNamedProperty intercepted by non-Proxy object"))).ThrowAsJavaScriptException();

            return env.Undefined();
			return;
        }

        return Unwrap<NodePerlObject>(info.This())->getNamedProperty(propertyName);
		return;
    }

    Napi::Value getNamedProperty(const Napi::String propertyName) const
    {
        Napi::EscapableHandleScope scope(env);
        Napi::String stmt(env, propertyName);
        Napi::Value arg0 = Napi::External::New(env, sv_);
        Napi::Value arg1 = Napi::External::New(env, my_perl);
        Napi::Value arg2 = propertyName;
        Napi::Value args[] = {arg0, arg1, arg2};
        Napi::Object retval(
			Napi::NewInstance(Napi::Function::New(env, NodePerlMethod::constructor), 3, args)
        );
        return scope.Escape(retval);
    }

    NodePerlObject(SV *sv, PerlInterpreter *myp): sv_(sv), PerlFoo(myp) {
        SvREFCNT_inc(sv);
    }
    ~NodePerlObject() {
        SvREFCNT_dec(sv_);
    }
    static Napi::Value getClassName(const Napi::CallbackInfo& info) {
        return return Unwrap<NodePerlObject>(info.This())->getClassName();
    }
	Napi::Value getClassName() const
	{
		Napi::EscapableHandleScope scope(env);
        if (SvPOK(sv_)) {
            STRLEN len;
            const char * str = SvPV(sv_, len);
            return scope.Escape(Napi::New(env, str, len));
        } else {
            return scope.Escape(Napi::New(env, sv_reftype(SvRV(sv_), TRUE)));
        }
    }
    static SV* getSV(Napi::Object val) {
        return Unwrap<NodePerlObject>(val)->sv_;
    }
    static Napi::Value blessed(Napi::Object val) {
        return Unwrap<NodePerlObject>(val)->blessed();
    }
    Napi::Value blessed() const
    {
        Napi::EscapableHandleScope scope(env);
        if(!(SvROK(sv_) && SvOBJECT(SvRV(sv_)))) {
            return scope.Escape(env.Undefined());
        }
        return scope.Escape(Napi::New(env, sv_reftype(SvRV(sv_),TRUE)));
    }

    static Napi::Value New(const Napi::CallbackInfo& info) {
		const Napi::CallbackInfo& args = info;

        ARG_EXT(0, jssv);
        ARG_EXT(1, jsmyp);
        SV* sv = static_cast<SV*>(jssv->Value());
        PerlInterpreter* myp = static_cast<PerlInterpreter*>(jsmyp->Value());
        (new NodePerlObject(sv, myp))->Wrap(args.Holder());
        return return args.Holder();
    }
};

class NodePerlClass : public NodePerlObject {
public:
	static Napi::FunctionReference constructor;

	static inline Napi::FunctionReference & constructor() {
		static Napi::FunctionReference my_constructor;
		return my_constructor;
	}

    static Napi::Object Init(Napi::Env env, Napi::Object exports) {
		// Prepare constructor template
		Napi::FunctionReference tpl = Napi::Function::New(env, New);
		tpl->SetClassName(Napi::String::New(env, "NodePerlClass"));

    	Napi::SetNamedPropertyHandler(tpl->InstanceTemplate(), NodePerlObject::GetNamedProperty);

		constructor.Reset(tpl);

		constructor().Reset(Napi::GetFunction(tpl));
		(target).Set(Napi::String::New(env, "NodePerlClass"),
			Napi::GetFunction(tpl));
    }
};

class NodePerl : public Napi::ObjectWrap<NodePerl>, public PerlFoo {

public:
	static inline Napi::FunctionReference & constructor() {
		static Napi::FunctionReference my_constructor;
		return my_constructor;
	}

	static Napi::Object Init(Napi::Env env, Napi::Object exports) {
		Napi::FunctionReference tpl = Napi::Function::New(env, New);
        Napi::SetPrototypeMethod(tpl, "evaluate", NodePerl::evaluate);
        Napi::SetPrototypeMethod(tpl, "getClass", NodePerl::getClass);
        Napi::SetPrototypeMethod(tpl, "call",	 NodePerl::call);
        Napi::SetPrototypeMethod(tpl, "callList", NodePerl::callList);
        Napi::SetPrototypeMethod(tpl, "destroy", NodePerl::destroy);

        Napi::SetMethod(tpl, "blessed", NodePerl::blessed);
        
		constructor().Reset(Napi::GetFunction(tpl));
		(target).Set(Napi::String::New(env, "Perl"),
			Napi::GetFunction(tpl));
    }

    NodePerl() : PerlFoo() {
        // std::cerr << "[Construct Perl]" << std::endl;

        char **av = {NULL};
        const char *embedding[] = { "", "-e", "0" };

        // XXX PL_origalen makes segv.
        // PL_origalen = 1; // for save $0
        PERL_SYS_INIT3(0,&av,NULL);
        my_perl = perl_alloc();
        perl_construct(my_perl);

        perl_parse(my_perl, xs_init, 3, (char**)embedding, NULL);
        PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
        perl_run(my_perl);
    }

    ~NodePerl() {
        // std::cerr << "[Destruct Perl]" << std::endl;
        PL_perl_destruct_level = 2;
        perl_destruct(my_perl);
        perl_free(my_perl);
    }

    static Napi::Value New(const Napi::CallbackInfo& info) {
		if (!info.IsConstructCall()) {
			return return Napi::NewInstance(Napi::New(env, constructor()), 0, {});
		}
        (new NodePerl())->Wrap(info.Holder());
        return return info.Holder();
    }

    static Napi::Value blessed(const Napi::CallbackInfo& info) {
		const auto& args = info;
        ARG_OBJ(0, jsobj);

        if (Napi::Function::New(env, NodePerlObject::constructor)->HasInstance(jsobj)) {
            return return NodePerlObject::blessed(jsobj);
        } else {
            return return env.Undefined();
        }
    }

    static Napi::Value evaluate(const Napi::CallbackInfo& info) {
        if (!info[0].IsString()) {
            Napi::Error::New(env, v8::Exception::Error(Napi::String::New(env, "Arguments must be string"))).ThrowAsJavaScriptException();

            return return env.Undefined();
	}
        Napi::String stmt(env, info[0]);

        Napi::Value retval = Unwrap<NodePerl>(info.This())->evaluate(*stmt);
        return return retval;
    }

    static Napi::Value getClass(const Napi::CallbackInfo& info) {
        if (!info[0].IsString()) {
            Napi::Error::New(env, v8::Exception::Error(Napi::String::New(env, "Arguments must be string"))).ThrowAsJavaScriptException();

            return return env.Undefined();
        }
        Napi::String stmt(env, info[0]);

        Napi::Value retval = Unwrap<NodePerl>(info.This())->getClass(*stmt);
        return return retval;
    }

    static Napi::Value call(const Napi::CallbackInfo& info) {
		NodePerl *nodePerl = Unwrap<NodePerl>(info.This());
		return nodePerl->CallMethod2(info, false);
    }
    static Napi::Value callList(const Napi::CallbackInfo& info) {
		NodePerl *nodePerl = Unwrap<NodePerl>(info.This());
		return nodePerl->CallMethod2(info, true);
    }

	static Napi::Value destroy(const Napi::CallbackInfo& info) {
		return Unwrap<NodePerl>(info.This())->destroy();
	}

private:
	void destroy()
	{
		NodePerl *nodePerl = this;
		this->persistent().Reset();		
		this->~NodePerl();
	}

    Napi::Value getClass(const char *name) const
    {
        Napi::EscapableHandleScope scope(env);
        Napi::Value arg0 = Napi::External::New(env, sv_2mortal(newSVpv(name, 0)));
        Napi::Value arg1 = Napi::External::New(env, my_perl);
        Napi::Value info[] = {arg0, arg1};
        Napi::Object retval(
			Napi::NewInstance(Napi::Function::New(env, NodePerlClass::constructor), 2, info)
        );
        return scope.Escape(retval);
    }
    Napi::Value evaluate(const char *stmt) {
        return perl2js(eval_pv(stmt, TRUE));
    }

public:
};

SV* PerlFoo::js2perl(Napi::Value val) const {
    if (val->IsTrue()) {
        return &PL_sv_yes;
    } else if (val->IsFalse()) {
        return &PL_sv_no;
    } else if (val.IsString()) {
        Napi::String method(env, val);
        return sv_2mortal(newSVpv(*method, method.Length()));
    } else if (val->IsArray()) {
		Napi::Array jsav = val.As<Napi::Array>();
        AV * av = newAV();
        av_extend(av, jsav->Length());
        for (int i=0; i<jsav->Length(); ++i) {
            SV * elem = this->js2perl(jsav->Get(i));
            av_push(av, SvREFCNT_inc(elem));
        }
        return sv_2mortal(newRV_noinc((SV*)av));
    } else if (val.IsObject()) {
		Napi::Object jsobj = val.As<Napi::Object>();
		Napi::FunctionReference NodePerlObject = Napi::Function::New(env, NodePerlObject::constructor);
        if (NodePerlObject->HasInstance(jsobj)) {
            SV * ret = NodePerlObject::getSV(jsobj);
            return ret;
        } else if (NodePerlObject->HasInstance(jsobj)) {
            SV * ret = NodePerlObject::getSV(jsobj);
            return ret;
        } else {
			Napi::Array keys = jsobj->GetOwnPropertyNames();
            HV * hv = newHV();
            hv_ksplit(hv, keys->Length());
            for (int i=0; i<keys->Length(); ++i) {
                SV * k = this->js2perl(keys->Get(i));
                SV * v = this->js2perl(jsobj->Get(keys->Get(i)));
                hv_store_ent(hv, k, v, 0);
                // SvREFCNT_dec(k);
            }
            return sv_2mortal(newRV_inc((SV*)hv));
        }
    } else if (val.IsNumber()) {
        return sv_2mortal(newSViv(val.As<Napi::Number>().Int32Value()));
    } else if (val->IsUint32()) {
        return sv_2mortal(newSVuv(val.As<Napi::Number>().Uint32Value()));
    } else if (val.IsNumber()) {
        return sv_2mortal(newSVnv(val.As<Napi::Number>().DoubleValue()));
    } else {
        // RegExp, Date, External
        return NULL;
    }
}

Napi::Value PerlFoo::perl2js_rv(SV * rv) {
    Napi::EscapableHandleScope scope(env);

    SV *sv = SvRV(rv);
    SvGETMAGIC(sv);
    svtype svt = (svtype)SvTYPE(sv);

    if (SvOBJECT(sv)) { // blessed object.
	    Napi::Value arg0 = Napi::External::New(env, rv);
		Napi::Value arg1 = Napi::External::New(env, my_perl);
	    Napi::Value args[] = {arg0, arg1};
        Napi::Object retval(
            Napi::NewInstance(Napi::Function::New(env, NodePerlObject::constructor),2, args)
        );
        return scope.Escape(retval);
    } else if (svt == SVt_PVHV) {
        HV* hval = (HV*)sv;
        HE* he;
        Napi::Object retval = Napi::Object::New(env);
        while ((he = hv_iternext(hval))) {
            retval.Set(
                this->perl2js(hv_iterkeysv(he)),
                this->perl2js(hv_iterval(hval, he))
            );
        }
        return scope.Escape(retval);
    } else if (svt == SVt_PVAV) {
        AV* ary = (AV*)sv;
        Napi::Array retval = Napi::Array::New(env);
        int len = av_len(ary) + 1;
        for (int i=0; i<len; ++i) {
            SV** svp = av_fetch(ary, i, 0);
            if (svp) {
                retval.Set(Napi::Number::New(env, i), this->perl2js(*svp));
            } else {
                retval.Set(Napi::Number::New(env, i), env.Undefined());
            }
        }
        return scope.Escape(retval);
    } else if (svt < SVt_PVAV) {
        sv_dump(sv);
        Napi::Error::New(env, v8::Exception::Error(Napi::String::New(env, "node-perl-simple doesn't support scalarref"))).ThrowAsJavaScriptException();

        return scope.Escape(env.Undefined());
    } else {
        return scope.Escape(env.Undefined());
    }
}

Napi::FunctionReference NodePerlObject::constructor;
Napi::FunctionReference NodePerlMethod::constructor;
Napi::FunctionReference NodePerlClass::constructor;

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
static Napi::Value InitPerl(const Napi::CallbackInfo& info) {
	Napi::Env env = info.Env();
	auto uMode = SetErrorMode(SEM_FAILCRITICALERRORS);
	auto hModule = LoadLibraryEx(LIBPERL_DIR LIBPERL, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
	auto error = GetLastError();
	if (hModule)	{
		FreeLibrary(hModule);
	} else {
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
    void *lib = dlopen(LIBPERL, RTLD_LAZY|RTLD_GLOBAL);
    if (lib) {
        dlclose(lib);
        return return env.Undefined();
    } else {
        std::cerr << dlerror() << std::endl;
        return return env.Undefined();
        // Napi::Error::New(env, v8::Exception::Error(Napi::New(env, dlerror()))).ThrowAsJavaScriptException();
 return env.Null();
    }
}
#endif

extern "C" Napi::Object init(Napi::Env env, Napi::Object exports) {
    {
	    Napi::FunctionReference t = Napi::Function::New(env, InitPerl);
        target.Set(Napi::String::New(env, "InitPerl"), t->GetFunction());
    }

    NodePerl::Init(env, target, module);
    NodePerlObject::Init(env, target, module);
    NodePerlClass::Init(env, target, module);
    NodePerlMethod::Init(env, target, module);
}

NODE_API_MODULE(perl, init)

