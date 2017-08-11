
#include <rpg/script_system.hpp>

#include <angelscript/add_on/scriptstdstring/scriptstdstring.h>
#include <angelscript/add_on/scriptmath/scriptmath.h>

#include <engine/parsers.hpp>
#include <engine/log.hpp>

#include "../xmlshortcuts.hpp"

#include <functional>
#include <algorithm>

using namespace rpg;
using namespace AS;

/*
template<typename Tret, typename...Tparams>
class script_bound_functiontemplate
{
public:
typedef std::function<void*(void*, void**)> implicit_function;

protected:
void angelscript_call(asIScriptGeneric *gen)
{
std::vector<void*> paramlist;
if (gen->GetObject())
paramlist.push_back(gen->GetObject());
for (size_t i = 0; i < gen->GetArgCount(); i++)
{
paramlist.push_back(gen->GetAddressOfArg(i));
}
Tret* ret = nullptr;
if (gen->GetAddressOfReturnLocation())
ret = new(gen->GetAddressOfReturnLocation()) Tret();
mFunction(ret, &paramlist[0]);
}

private:
implicit_function mFunction;
};*/

// #########
// script_system
// #########

void script_system::message_callback(const asSMessageInfo * msg)
{
	logger::level type = logger::level::error;
	if (msg->type == asEMsgType::asMSGTYPE_INFORMATION)
		type = logger::level::info;
	else if (msg->type == asEMsgType::asMSGTYPE_WARNING)
		type = logger::level::warning;

	logger::print(msg->section, msg->row, msg->col, type, msg->message);
}

void script_system::script_abort()
{
	assert(mCurrect_thread_context != nullptr);
	mCurrect_thread_context->context->Abort();
}

void script_system::script_create_thread(AS::asIScriptFunction * func, AS::CScriptDictionary * arg)
{
	if (!func)
	{
		logger::error("Invalid function");
		return;
	}

	// Create a new context for the co-routine
	asIScriptContext *ctx = create_thread(func)->context;

	// Pass the argument to the context
	ctx->SetArgObject(0, arg);
}

void script_system::script_create_thread_noargs(AS::asIScriptFunction * func)
{
	if (func == 0)
	{
		logger::error("Invalid function");
		return;
	}

	create_thread(func);
}

bool script_system::script_yield()
{
	assert(mCurrect_thread_context != nullptr);
	mCurrect_thread_context->context->Suspend();
	return true;
}

void script_system::script_make_shared(AS::CScriptHandle pHandle, const std::string& pName)
{
	mShared_handles[pName] = pHandle;
}

AS::CScriptHandle script_system::script_get_shared(const std::string& pName)
{
	return mShared_handles[pName];
}

void script_system::load_script_interface()
{
	add_function("rand", &std::rand);

	mEngine->RegisterFuncdef("void coroutine(dictionary@)");
	mEngine->RegisterFuncdef("void coroutine_noargs()");
	add_function("void create_thread(coroutine @+)", asMETHOD(script_system, script_create_thread_noargs), this);
	add_function("void create_thread(coroutine @+, dictionary @+)", asMETHOD(script_system, script_create_thread), this);

	add_function("dprint", &script_system::script_debug_print, this);
	add_function("eprint", &script_system::script_error_print, this);
	add_function("abort", &script_system::script_abort, this);
	add_function("yield", &script_system::script_yield, this);

	add_function("void make_shared(ref@, const string&in)", asMETHOD(script_system, script_make_shared), this);
	add_function("ref@ get_shared(const string&in)", asMETHOD(script_system, script_get_shared), this);
}

void script_system::timeout_callback(AS::asIScriptContext *ctx)
{
	if (mTimeout_timer.is_reached())
	{
		logger::error("Script running too long. (Infinite loop?)");
		ctx->Abort();
		logger::info("In script '" + std::string(ctx->GetFunction()->GetModuleName()) + "' :");
		logger::info("  Script aborted at line " + std::to_string(ctx->GetLineNumber())
			+ " in function '" + std::string(ctx->GetFunction()->GetDeclaration(true, true)) + "'");
	}
}

void
script_system::script_debug_print(std::string &pMessage)
{
	if (!is_executing())
	{
		logger::print("Unknown", 0, 0, logger::level::debug, pMessage);
		return;
	}

	assert(mCurrect_thread_context->context != nullptr);
	assert(mCurrect_thread_context->context->GetFunction() != nullptr);

	std::string details = std::string(mCurrect_thread_context->context->GetFunction()->GetModuleName());
	logger::print(details, get_current_line(), 0, logger::level::debug, pMessage);
}

void script_system::script_error_print(std::string & pMessage)
{
	if (!is_executing())
	{
		logger::print("Unknown", 0, 0, logger::level::error, pMessage);
		return;
	}

	assert(mCurrect_thread_context->context != nullptr);
	assert(mCurrect_thread_context->context->GetFunction() != nullptr);

	std::string details = std::string(mCurrect_thread_context->context->GetFunction()->GetModuleName());
	logger::print(details, get_current_line(), 0, logger::level::error, pMessage);
}

void
script_system::register_vector_type()
{
	// Added asOBJ_APP_CLASS_ALLFLOATS so linux won't complain
	mEngine->RegisterObjectType("vec", sizeof(engine::fvector), asOBJ_VALUE | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<engine::fvector>());

	// Constructors and deconstructors
	mEngine->RegisterObjectBehaviour("vec", asBEHAVE_CONSTRUCT, "void f()"
		, asFUNCTION(script_default_constructor<engine::fvector>)
		, asCALL_CDECL_OBJLAST);
	mEngine->RegisterObjectBehaviour("vec", asBEHAVE_CONSTRUCT, "void f(float, float)"
		, asFUNCTIONPR(script_constructor<engine::fvector>, (float, float, void*), void)
		, asCALL_CDECL_OBJLAST);
	mEngine->RegisterObjectBehaviour("vec", asBEHAVE_CONSTRUCT, "void f(const vec&in)"
		, asFUNCTIONPR(script_constructor<engine::fvector>, (const engine::fvector&, void*), void)
		, asCALL_CDECL_OBJLAST);
	mEngine->RegisterObjectBehaviour("vec", asBEHAVE_DESTRUCT, "void f()"
		, asFUNCTION(script_default_deconstructor<engine::fvector>), asCALL_CDECL_OBJLAST);

	// Assignments
	mEngine->RegisterObjectMethod("vec", "vec& opAssign(const vec &in)"
		, asMETHODPR(engine::fvector, operator=, (const engine::fvector&), engine::fvector&)
		, asCALL_THISCALL);
	mEngine->RegisterObjectMethod("vec", "vec& opAddAssign(const vec &in)"
		, asMETHODPR(engine::fvector, operator+=, (const engine::fvector&), engine::fvector&)
		, asCALL_THISCALL);
	mEngine->RegisterObjectMethod("vec", "vec& opSubAssign(const vec &in)"
		, asMETHODPR(engine::fvector, operator-=, (const engine::fvector&), engine::fvector&)
		, asCALL_THISCALL);
	mEngine->RegisterObjectMethod("vec", "vec& opMulAssign(const vec &in)"
		, asMETHODPR(engine::fvector, operator*=, (const engine::fvector&), engine::fvector&)
		, asCALL_THISCALL);
	mEngine->RegisterObjectMethod("vec", "vec& opMulAssign(float)"
		, asMETHODPR(engine::fvector, operator*=, (float), engine::fvector&)
		, asCALL_THISCALL);
	mEngine->RegisterObjectMethod("vec", "vec& opDivAssign(float)"
		, asMETHODPR(engine::fvector, operator/=, (float), engine::fvector&)
		, asCALL_THISCALL);

	// Arithmic
	mEngine->RegisterObjectMethod("vec", "vec opAdd(const vec &in) const"
		, asMETHODPR(engine::fvector, operator+, (const engine::fvector&) const, engine::fvector)
		, asCALL_THISCALL);
	mEngine->RegisterObjectMethod("vec", "vec opSub(const vec &in) const"
		, asMETHODPR(engine::fvector, operator-, (const engine::fvector&) const, engine::fvector)
		, asCALL_THISCALL);
	mEngine->RegisterObjectMethod("vec", "vec opMul(const vec &in) const"
		, asMETHODPR(engine::fvector, operator*, (const engine::fvector&) const, engine::fvector)
		, asCALL_THISCALL);
	mEngine->RegisterObjectMethod("vec", "vec opMul(float) const"
		, asMETHODPR(engine::fvector, operator*, (float) const, engine::fvector)
		, asCALL_THISCALL);
	mEngine->RegisterObjectMethod("vec", "vec opDiv(float) const"
		, asMETHODPR(engine::fvector, operator/, (float) const, engine::fvector)
		, asCALL_THISCALL);
	mEngine->RegisterObjectMethod("vec", "vec opNeg() const"
		, asMETHODPR(engine::fvector, operator-, () const, engine::fvector)
		, asCALL_THISCALL);

	// Distance
	mEngine->RegisterObjectMethod("vec", "float distance() const"
		, asMETHODPR(engine::fvector, distance, () const, float)
		, asCALL_THISCALL);
	mEngine->RegisterObjectMethod("vec", "float distance(const vec &in) const"
		, asMETHODPR(engine::fvector, distance, (const engine::fvector&) const, float)
		, asCALL_THISCALL);
	mEngine->RegisterObjectMethod("vec", "float manhattan() const"
		, asMETHODPR(engine::fvector, manhattan, () const, float)
		, asCALL_THISCALL);
	mEngine->RegisterObjectMethod("vec", "float manhattan(const vec &in) const"
		, asMETHODPR(engine::fvector, manhattan, (const engine::fvector&) const, float)
		, asCALL_THISCALL);

	// Rotate
	mEngine->RegisterObjectMethod("vec", "vec& rotate(float)"
		, asMETHODPR(engine::fvector, rotate, (float), engine::fvector&)
		, asCALL_THISCALL);
	mEngine->RegisterObjectMethod("vec", "vec& rotate(const vec &in, float)"
		, asMETHODPR(engine::fvector, rotate, (const engine::fvector&, float), engine::fvector&)
		, asCALL_THISCALL);

	mEngine->RegisterObjectMethod("vec", "vec& normalize()"
		, asMETHOD(engine::fvector, normalize)
		, asCALL_THISCALL);

	mEngine->RegisterObjectMethod("vec", "vec& floor()"
		, asMETHOD(engine::fvector, floor)
		, asCALL_THISCALL);

	mEngine->RegisterObjectMethod("vec", "float angle() const"
		, asMETHODPR(engine::fvector, angle, () const, float)
		, asCALL_THISCALL);

	mEngine->RegisterObjectMethod("vec", "float angle(const vec&in) const"
		, asMETHODPR(engine::fvector, angle, (const engine::fvector&) const, float)
		, asCALL_THISCALL);

	// Members
	mEngine->RegisterObjectProperty("vec", "float x", asOFFSET(engine::fvector, x));
	mEngine->RegisterObjectProperty("vec", "float y", asOFFSET(engine::fvector, y));
}

void script_system::register_timer_type()
{
	set_namespace("util");
	create_object<engine::timer>("timer");
	add_method<engine::timer, void, float>("timer", "start", &engine::timer::start);
	add_method("timer", "is_reached", &engine::timer::is_reached);
	reset_namespace();
}

script_system::script_system()
{
	mEngine = asCreateScriptEngine();

	mEngine->SetEngineProperty(asEP_REQUIRE_ENUM_SCOPE, true);
	//mEngine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);

	mEngine->SetMessageCallback(asMETHOD(script_system, message_callback), this, asCALL_THISCALL);

	RegisterStdString(mEngine);
	RegisterScriptMath(mEngine);
	RegisterScriptArray(mEngine, true);
	RegisterScriptDictionary(mEngine);
	RegisterScriptHandle(mEngine);

	register_vector_type();
	register_timer_type();

	load_script_interface();
}

script_system::~script_system()
{
	mShared_handles.clear();
	abort_all();

	// Just screw it!
	mEngine->ShutDownAndRelease();
}


void
script_system::add_function(const char* pDeclaration, const asSFuncPtr& pPtr, void* pInstance)
{
	int r = mEngine->RegisterGlobalFunction(pDeclaration, pPtr, asCALL_THISCALL_ASGLOBAL, pInstance);
	assert(r >= 0);
}

void
script_system::add_function(const char * pDeclaration, const asSFuncPtr& pPtr)
{
	int r = mEngine->RegisterGlobalFunction(pDeclaration, pPtr, asCALL_CDECL);
	assert(r >= 0);
}

void script_system::abort_all()
{
	for (auto& i : mThread_contexts)
	{
		i->context->Abort();
		mEngine->ReturnContext(i->context);
		i->context = nullptr;
	}
	mThread_contexts.clear();
	mEngine->GarbageCollect(asGC_FULL_CYCLE | asGC_DESTROY_GARBAGE);
}

void script_system::return_context(AS::asIScriptContext * pContext)
{
	mEngine->ReturnContext(pContext);
}

int script_system::tick()
{

	for (size_t i = 0; i < mThread_contexts.size(); i++)
	{
		mCurrect_thread_context = mThread_contexts[i];
		
		// Timeout feature disabled in release mode to remove overhead
#ifndef LOCKED_RELEASE_MODE

		// 5 second timeout for scripts
		mTimeout_timer.start(5);
		mCurrect_thread_context->context->SetLineCallback(AS::asMETHOD(script_system, timeout_callback), this, asCALL_THISCALL);
#endif

		int r = mCurrect_thread_context->context->Execute();

		if (r != AS::asEXECUTION_SUSPENDED)
		{
			if (!mCurrect_thread_context->keep_context)
			{
				mEngine->ReturnContext(mCurrect_thread_context->context);
				mCurrect_thread_context->context = nullptr;
			}
			mThread_contexts.erase(mThread_contexts.begin() + i);
			--i;
		}

		mCurrect_thread_context = nullptr;

		mEngine->GarbageCollect(asGC_ONE_STEP | asGC_DETECT_GARBAGE);
	}

	return mThread_contexts.size();
}

int script_system::get_current_line()
{
	if (mCurrect_thread_context)
	{
		return mCurrect_thread_context->context->GetLineNumber();
	}
	return 0;
}

void script_system::set_namespace(const std::string & pName)
{
	mEngine->SetDefaultNamespace(pName.c_str());
}

void script_system::reset_namespace()
{
	mEngine->SetDefaultNamespace("");
}

AS::asIScriptEngine& script_system::get_engine()
{
	assert(mEngine != nullptr);
	return *mEngine;
}

std::shared_ptr<script_system::thread> script_system::create_thread(AS::asIScriptFunction * pFunc, bool pKeep_context)
{
	asIScriptContext* context = mEngine->RequestContext();
	if (!context)
		return nullptr;

	int r = context->Prepare(pFunc);
	if (r < 0)
	{
		mEngine->ReturnContext(context);
		return nullptr;
	}

	// Create a new one if necessary
	std::shared_ptr<thread> new_thread(new thread);
	new_thread->context = context;
	new_thread->keep_context = pKeep_context;
	mThread_contexts.push_back(new_thread);

	return new_thread;
}

bool script_system::is_executing()
{
	return mCurrect_thread_context != nullptr;
}

