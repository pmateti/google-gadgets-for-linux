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

#ifndef GGADGET_NATIVE_JS_WRAPPER_H__
#define GGADGET_NATIVE_JS_WRAPPER_H__

#include <jsapi.h>
#include "ggadget/common.h"
#include "ggadget/scriptable_interface.h"

namespace ggadget {

class Connection;

/**
 * A wrapper wrapping a native @c ScriptableInterface object into a
 * JavaScript object.
 */
class NativeJSWrapper {

 public:
   NativeJSWrapper(JSContext *js_context, JSObject *js_object,
                   ScriptableInterface *scriptable);
  ~NativeJSWrapper();

  /**
   * Unwrap a native @c ScriptableInterface object from a JavaScript object.
   * The JS object must have been returned from the above Wrap.
   * Return @c JS_FALSE on errors.
   */
  static JSBool Unwrap(JSContext *cx, JSObject *obj,
                       ScriptableInterface **scriptable);

  JSObject *js_object() const { return js_object_; }
  ScriptableInterface *scriptable() const { return scriptable_; }
  ScriptableInterface::OwnershipPolicy ownership_policy() const {
    return ownership_policy_;
  }

  static JSClass *GetWrapperJSClass() { return &wrapper_js_class_; }

  /**
   * Detach the wrapper object from JavaScript so that the engine can
   * GC it. 
   */
  void DetachJS();

private:
  DISALLOW_EVIL_CONSTRUCTORS(NativeJSWrapper);

  void OnDelete();

  /**
   * Get the @c NativeJSWrapper pointer from a JS wrapped
   * @c ScriptableInterface object.
   */
  static NativeJSWrapper *GetWrapperFromJS(JSContext *cx, JSObject *js_object);

  static JSBool CallWrapperMethod(JSContext *cx, JSObject *obj,
                                  uintN argc, jsval *argv, jsval *rval);

  // This pair of methods handle all GetProperty and SetProperty callbacks
  // for system built-in properties, unknown properties or array indexes.
  static JSBool GetWrapperPropertyDefault(JSContext *cx, JSObject *obj,
                                          jsval id, jsval *vp);
  static JSBool SetWrapperPropertyDefault(JSContext *cx, JSObject *obj,
                                          jsval id, jsval *vp);

  // This pair of methods handle all GetProperty and SetProperty callbacks
  // for registered native properties with ids fitting in tinyid (-128>=id>0).
  static JSBool GetWrapperPropertyByIndex(JSContext *cx, JSObject *obj,
                                          jsval id, jsval *vp);
  static JSBool SetWrapperPropertyByIndex(JSContext *cx, JSObject *obj,
                                          jsval id, jsval *vp);

  // This pair of methods handle all GetProperty and SetProperty callbacks
  // for dynamic properties and registered native properties with ids not
  // fitting in tinyid (id<-128).
  static JSBool GetWrapperPropertyByName(JSContext *cx, JSObject *obj,
                                         jsval id, jsval *vp);
  static JSBool SetWrapperPropertyByName(JSContext *cx, JSObject *obj,
                                         jsval id, jsval *vp);
  static JSBool ResolveWrapperProperty(JSContext *cx, JSObject *obj, jsval id);
  static void FinalizeWrapper(JSContext *cx, JSObject *obj);

  JSBool CheckNotDeleted();
  JSBool InvokeMethod(uintN argc, jsval *argv, jsval *rval);
  JSBool GetPropertyDefault(jsval id, jsval *vp);
  JSBool SetPropertyDefault(jsval id, jsval vp);
  JSBool GetPropertyByIndex(jsval id, jsval *vp);
  JSBool SetPropertyByIndex(jsval id, jsval vp);
  JSBool GetPropertyByName(jsval id, jsval *vp);
  JSBool SetPropertyByName(jsval id, jsval vp);
  JSBool ResolveProperty(jsval id);

  static JSClass wrapper_js_class_;

  bool deleted_;
  JSContext *js_context_;
  JSObject *js_object_;
  ScriptableInterface *scriptable_;
  Connection *ondelete_connection_;
  ScriptableInterface::OwnershipPolicy ownership_policy_;
};

} // namespace ggadget

#endif // GGADGET_NATIVE_JS_WRAPPER_H__