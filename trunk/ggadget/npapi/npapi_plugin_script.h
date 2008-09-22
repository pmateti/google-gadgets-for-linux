/*
  Copyright 2008 Google Inc.

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

#ifndef GGADGET_NPAPI_NPAPI_PLUGIN_SCRIPT_H__
#define GGADGET_NPAPI_NPAPI_PLUGIN_SCRIPT_H__

#include <third_party/npapi/npupp.h>
#include <third_party/npapi/npapi.h>
#include <third_party/npapi/npruntime.h>

#include <ggadget/scriptable_helper.h>

namespace ggadget {
namespace npapi {

/**
 * Class definition for wrapping native scriptable object,
 * which can then be accessed by plugin.
 */
class NPNativeObject {
 public:
  NPNativeObject(NPP instance, ScriptableInterface *object);
  ~NPNativeObject();

  /**
   * Returns the native scriptable object that is wrapped.
   */
  ScriptableInterface *UnWrap();

 private:
  class Impl;

  // The first part of this object must be NPObject.
  NPObject np_obj_;
  Impl *impl_;
};

/**
 * Class definition for Wrapping plugin scriptable object,
 * which can then be accessed directly from native JS engine.
 */
class NPPluginObject : public ScriptableHelperDefault {
 public:
  DEFINE_CLASS_ID(0xec31413d89ab02ce, ScriptableInterface);

 public:
  NPPluginObject(NPP instance, NPObject *np_obj);
  ~NPPluginObject();

  /**
   * Return the NPObject that is wrapped.
   */
  NPObject *UnWrap();

 private:
  class Impl;
  Impl *impl_;
};

/**
 * For Unittest.
 */
Variant ConvertNPToLocal(NPP instance, const NPVariant *np_var);
void ConvertLocalToNP(NPP instance, const Variant &var, NPVariant *np_var);

} // namespace npapi
} // namespace ggadget

#endif // GGADGET_NPAPI_NPAPI_PLUGIN_SCRIPT_H__
