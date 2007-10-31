/*
  Copyright 2007 Google Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include "js_script_context.h"
#include "js_script_runtime.h"
#include "ggadget/common.h"
#include "ggadget/scoped_ptr.h"
#include "ggadget/slot.h"
#include "ggadget/unicode_utils.h"
#include "converter.h"
#include "native_js_wrapper.h"

namespace ggadget {

/**
 * A Slot that wraps a JavaScript function object.
 */
class JSFunctionSlot : public Slot {
 public:
  JSFunctionSlot(const Slot *prototype,
                 JSContext *context,
                 jsval function_val);
  virtual ~JSFunctionSlot();

  virtual Variant Call(int argc, Variant argv[]) const;

  virtual bool HasMetadata() const { return prototype_ != NULL; }
  virtual Variant::Type GetReturnType() const {
    return prototype_ ? prototype_->GetReturnType() : Variant::TYPE_VOID;
  }
  virtual int GetArgCount() const {
    return prototype_ ? prototype_->GetArgCount() : 0;
  }
  virtual const Variant::Type *GetArgTypes() const {
    return prototype_ ? prototype_->GetArgTypes() : NULL;
  }
  virtual bool operator==(const Slot &another) const {
    return function_val_ ==
           down_cast<const JSFunctionSlot *>(&another)->function_val_;
  }

 private:
  const Slot *prototype_;
  JSContext *context_;
  jsval function_val_;
#ifdef _DEBUG
  std::string root_name_;
#endif
};

JSFunctionSlot::JSFunctionSlot(const Slot *prototype,
                               JSContext *context,
                               jsval function_val)
    : prototype_(prototype),
      context_(context),
      function_val_(function_val) {
  // Add a reference to the function object to prevent it from being GC'ed.
#ifdef _DEBUG
  root_name_ = "JSFunctionSlot jsfunc=";
  root_name_ += JS_GetFunctionName(JS_ValueToFunction(context, function_val));
  JS_AddNamedRoot(context_, &function_val_, root_name_.c_str());
#else
  JS_AddRoot(context_, &function_val_);
#endif
}

JSFunctionSlot::~JSFunctionSlot() {
  JS_RemoveRoot(context_, &function_val_);
}

Variant JSFunctionSlot::Call(int argc, Variant argv[]) const {
  scoped_array<jsval> js_args;
  Variant return_value(GetReturnType());
  if (argc > 0) {
    js_args.reset(new jsval[argc]);
    for (int i = 0; i < argc; i++) {
      if (!ConvertNativeToJS(context_, argv[i], &js_args[i])) {
        JS_ReportError(context_, "Failed to convert argument %d(%s) to jsval",
                       i, argv[i].ToString().c_str());
        return return_value;
      }
    }
  }

  jsval rval;
  JSBool result = JS_CallFunctionValue(context_, NULL, function_val_,
                                       argc, js_args.get(), &rval);
  if (result) {
    result = ConvertJSToNative(context_, return_value, rval, &return_value);
    if (!result)
      JS_ReportError(context_,
                     "Failed to convert JS function return value(%s) to native",
                     PrintJSValue(context_, rval).c_str());
  }
  return return_value;
}

JSScriptContext::JSScriptContext(JSContext *context)
    : context_(context),
      filename_(NULL),
      lineno_(0) {
  JS_SetContextPrivate(context_, this);
  // JS_SetOptions(context_, JS_GetOptions(context_) | JSOPTION_STRICT);
}

JSScriptContext::~JSScriptContext() {
  JS_DestroyContext(context_);
}

static JSScriptContext *GetJSScriptContext(JSContext *context) {
  return reinterpret_cast<JSScriptContext *>(JS_GetContextPrivate(context));
}

// As we want to depend on only the public SpiderMonkey APIs, the only
// way to get the current filename and lineno is from the JSErrorReport.
void JSScriptContext::RecordFileAndLine(JSContext *cx, const char *message,
                                        JSErrorReport *report) {
  JSScriptContext *context = GetJSScriptContext(cx);
  if (context) {
    context->filename_ = report->filename;
    context->lineno_ = report->lineno;
  }
}

void JSScriptContext::GetCurrentFileAndLineInternal(const char **filename,
                                                    int *lineno) {
  filename_ = NULL;
  lineno_ = 0;
  JSErrorReporter old_reporter = JS_SetErrorReporter(context_,
                                                     RecordFileAndLine);
  // Let the JavaScript engine call RecordFileAndLine.
  JS_ReportError(context_, "");
  JS_SetErrorReporter(context_, old_reporter);
  *filename = filename_;
  *lineno = lineno_;
}

void JSScriptContext::GetCurrentFileAndLine(JSContext *context,
                                            const char **filename,
                                            int *lineno) {
  JSScriptContext *context_wrapper = GetJSScriptContext(context);
  if (context_wrapper) {
    context_wrapper->GetCurrentFileAndLineInternal(filename, lineno);
  } else {
    *filename = NULL;
    *lineno = 0;
  }
}

JSObject *JSScriptContext::WrapNativeObjectToJSInternal(
    ScriptableInterface *scriptable) {
  ASSERT(scriptable);
  WrapperMap::const_iterator it = wrapper_map_.find(scriptable);
  if (it == wrapper_map_.end()) {
    NativeJSWrapper *wrapper = new NativeJSWrapper(context_, scriptable);
    wrapper_map_[scriptable] = wrapper;
    ASSERT(wrapper->scriptable() == scriptable);
    return wrapper->js_object();
  } else {
    return it->second->js_object();
  }
}

JSObject *JSScriptContext::WrapNativeObjectToJS(
    JSContext *cx, ScriptableInterface *scriptable) {
  JSScriptContext *context_wrapper = GetJSScriptContext(cx);
  ASSERT(context_wrapper);
  if (context_wrapper)
    return context_wrapper->WrapNativeObjectToJSInternal(scriptable);
  return NULL;
}

void JSScriptContext::FinalizeNativeJSWrapperInternal(
    NativeJSWrapper *wrapper) {
  wrapper_map_.erase(wrapper->scriptable());
}

void JSScriptContext::FinalizeNativeJSWrapper(JSContext *cx,
                                              NativeJSWrapper *wrapper) {
  JSScriptContext *context_wrapper = GetJSScriptContext(cx);
  ASSERT(context_wrapper);
  if (context_wrapper)
    context_wrapper->FinalizeNativeJSWrapperInternal(wrapper);
}

jsval JSScriptContext::ConvertSlotToJSInternal(Slot *slot) {
  ASSERT(slot);
  SlotJSMap::const_iterator it = slot_js_map_.find(slot);
  if (it != slot_js_map_.end()) {
    // If found, it->second JavaScript function object that has been wrapped
    // into a JSFunctionSlot.
    return it->second;
  }
  // We don't allow JavaScript to call native slot in this way.
  return JSVAL_NULL;
}

jsval JSScriptContext::ConvertSlotToJS(JSContext *cx, Slot *slot) {
  JSScriptContext *context_wrapper = GetJSScriptContext(cx);
  ASSERT(context_wrapper);
  if (context_wrapper)
    return context_wrapper->ConvertSlotToJSInternal(slot);
  return JSVAL_NULL;
}

JSBool JSScriptContext::CallNativeSlot(JSContext *cx, JSObject *obj,
                                       uintN argc, jsval *argv, jsval *rval) {
  // According to JS stack structure, argv[-2] is the current function object.
  JSObject *func_object = JSVAL_TO_OBJECT(argv[-2]);
  // Get the method slot from the reserved slot.
  jsval val;
  if (!JS_GetReservedSlot(cx, func_object, 0, &val) ||
      !JSVAL_IS_INT(val))
    return JS_FALSE;
  Slot *slot = reinterpret_cast<Slot *>(JSVAL_TO_PRIVATE(val));

  const Variant::Type *arg_types = NULL;
  if (slot->HasMetadata()) {
    if (argc != static_cast<uintN>(slot->GetArgCount())) {
      // Argc mismatch.
      JS_ReportError(cx, "Wrong number of arguments: %d (expected: %d)",
                     argc, slot->GetArgCount());
      return JS_FALSE;
    }
    arg_types = slot->GetArgTypes();
  }

  scoped_array<Variant> params;
  if (argc > 0) {
    params.reset(new Variant[argc]);
    for (uintN i = 0; i < argc; i++) {
      JSBool result = arg_types != NULL ?
          ConvertJSToNative(cx, Variant(arg_types[i]), argv[i], &params[i]) :
          ConvertJSToNativeVariant(cx, argv[i], &params[i]);
      if (!result) {
        JS_ReportError(cx, "Failed to convert argument %d(%s) to native",
                       i, PrintJSValue(cx, argv[i]).c_str());
        return JS_FALSE;
      }
    }
  }

  try {
    Variant return_value = slot->Call(argc, params.get());
    JSBool result = ConvertNativeToJS(cx, return_value, rval);
    if (!result)
      JS_ReportError(cx,
                     "Failed to convert native function result(%s) to jsval",
                     return_value.ToString().c_str());
    return result;
  } catch (ScriptableExceptionHolder e) {
    HandleException(cx, e);
    return JS_FALSE;
  }
}

JSBool JSScriptContext::HandleException(JSContext *cx,
                                        const ScriptableExceptionHolder &e) {
  jsval js_exception;
  if (!ConvertNativeToJS(cx, Variant(e.scriptable_exception()),
                         &js_exception)) {
    JS_ReportError(cx, "Failed to convert native exception to jsval");
    return JS_FALSE;
  }
  JS_SetPendingException(cx, js_exception);
  return JS_TRUE;
}

Slot *JSScriptContext::NewJSFunctionSlotInternal(const Slot *prototype,
                                                 jsval function_val) {
  Slot *slot = new JSFunctionSlot(prototype, context_, function_val);
  // Put it here to make it possible for ConvertNativeSlotToJS to unwrap
  // a JSFunctionSlot.
  slot_js_map_[slot] = function_val;
  return slot;
}

Slot *JSScriptContext::NewJSFunctionSlot(JSContext *cx,
                                         const Slot *prototype,
                                         jsval function_val) {
  JSScriptContext *context_wrapper = GetJSScriptContext(cx);
  ASSERT(context_wrapper);
  if (context_wrapper)
    return context_wrapper->NewJSFunctionSlotInternal(prototype, function_val);
  return NULL;
}

void JSScriptContext::Destroy() {
  delete this;
}

void JSScriptContext::Execute(const char *script,
                              const char *filename,
                              int lineno) {
  UTF16String utf16_string;
  ConvertStringUTF8ToUTF16(script, strlen(script), &utf16_string);
  jsval rval;
  JS_EvaluateUCScript(context_, JS_GetGlobalObject(context_),
                      utf16_string.c_str(), utf16_string.size(),
                      filename, lineno, &rval);
}

Slot *JSScriptContext::Compile(const char *script,
                               const char *filename,
                               int lineno) {
  UTF16String utf16_string;
  ConvertStringUTF8ToUTF16(script, strlen(script), &utf16_string);
  JSFunction *function = JS_CompileUCFunction(
      context_, NULL, NULL, 0, NULL,  // No name and no argument.
      // Don't cast utf16_string.c_str() to jschar *, to let the compiler check
      // if they are compatible.
      utf16_string.c_str(), utf16_string.size(),
      filename, lineno);
  if (!function)
    return NULL;

  return new JSFunctionSlot(
      NULL, context_, OBJECT_TO_JSVAL(JS_GetFunctionObject(function)));
}

bool JSScriptContext::SetGlobalObject(ScriptableInterface *global_object) {
  JSObject *js_global = WrapNativeObjectToJS(context_, global_object);
  if (!js_global)
    return false;
  return JS_InitStandardClasses(context_, js_global);
}

} // namespace ggadget