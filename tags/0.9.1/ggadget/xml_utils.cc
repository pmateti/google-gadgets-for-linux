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

#include <cstring>
#include <cmath>

#include "xml_utils.h"
#include "basic_element.h"
#include "object_element.h"     // Special process for object element.
#include "elements.h"
#include "file_manager_interface.h"
#include "gadget_consts.h"
#include "logger.h"
#include "scriptable_interface.h"
#include "script_context_interface.h"
#include "unicode_utils.h"
#include "view.h"
#include "xml_dom_interface.h"
#include "xml_parser_interface.h"

namespace ggadget {

static void SetScriptableProperty(ScriptableInterface *scriptable,
                                  ScriptContextInterface *script_context,
                                  const char *filename, int row, int column,
                                  const char *name, const char *value,
                                  const char *tag_name) {
  Variant prototype;
  if (scriptable->GetPropertyInfo(name, &prototype) !=
      ScriptableInterface::PROPERTY_NORMAL) {
    LOG("%s:%d:%d Can't set property %s for %s", filename, row, column,
        name, tag_name);
    return;
  }

  Variant str_value_variant(value);
  Variant property_value;
  switch (prototype.type()) {
    case Variant::TYPE_BOOL: {
      bool b;
      if (str_value_variant.ConvertToBool(&b)) {
        property_value = Variant(b);
      } else {
        LOG("%s:%d:%d: Invalid bool '%s' for property %s of %s",
            filename, row, column, value, name, tag_name);
        property_value = Variant(GadgetStrCmp("true", value) == 0 ?
                               true : false);
        return;
      }
      break;
    }
    case Variant::TYPE_INT64: {
      int64_t i;
      if (str_value_variant.ConvertToInt64(&i)) {
        property_value = Variant(i);
      } else {
        LOG("%s:%d:%d: Invalid Integer '%s' for property %s of %s",
            filename, row, column, value, name, tag_name);
        return;
      }
      break;
    }
    case Variant::TYPE_DOUBLE: {
      double d;
      if (str_value_variant.ConvertToDouble(&d)) {
        property_value = Variant(d);
      } else {
        LOG("%s:%d:%d: Invalid double '%s' for property %s of %s",
            filename, row, column, value, name, tag_name);
        return;
      }
      break;
    }
    case Variant::TYPE_STRING:
      property_value = str_value_variant;
      break;

    case Variant::TYPE_VARIANT: {
      int64_t i;
      double d;
      bool b;
      if (!*value) {
        property_value = str_value_variant;
      } else if (strchr(value, '.') == NULL &&
                 str_value_variant.ConvertToInt64(&i)) {
        property_value = Variant(i);
      } else if (str_value_variant.ConvertToDouble(&d)) {
        property_value = Variant(d);
      } else if (str_value_variant.ConvertToBool(&b)) {
        property_value = Variant(b);
      } else {
        property_value = str_value_variant;
      }
      break;
    }
    case Variant::TYPE_SLOT: {
      if (script_context) {
        property_value = Variant(script_context->Compile(value, filename, row));
        break;
      } else {
        LOG("%s:%d:%d: Can't set script '%s' for property %s of %s: "
            "ScriptContext is not available.",
            filename, row, column, value, name, tag_name);
        return;
      }
    }

    default:
      LOG("%s:%d:%d: Unsupported type %s when setting property %s for %s",
          filename, row, column, prototype.Print().c_str(), name, tag_name);
      return;
  }

  if (!scriptable->SetProperty(name, property_value))
    LOG("%s:%d:%d: Can't set readonly property %s for %s",
        filename, row, column, name, tag_name);
}

void SetupScriptableProperties(ScriptableInterface *scriptable,
                               ScriptContextInterface *script_context,
                               const DOMElementInterface *xml_element,
                               const char *filename) {
  std::string tag_name = xml_element->GetTagName();
  const DOMNamedNodeMapInterface *attributes = xml_element->GetAttributes();
  size_t length = attributes->GetLength();

  if (scriptable->IsInstanceOf(ObjectElement::CLASS_ID)) {
    // Special process for the "classId" attribute if it's an object element.
    // ClassId must be set before all other properties, because the existence
    // of other properties depends on classId.
    bool has_classid = false;
    // Must use a loop because the classid property may be in different cases,
    // such as "classid" and "classId", etc.
    for (size_t i = 0; i < length; i++) {
      const DOMAttrInterface *attr =
          down_cast<const DOMAttrInterface *>(attributes->GetItem(i));
      std::string name = attr->GetName();
      std::string value = attr->GetValue();
      if (GadgetStrCmp(kClassIdAttr, name.c_str()) == 0) {
        has_classid = true;
        down_cast<ObjectElement *>(scriptable)->SetObjectClassId(value);
        // Remove this attribute from list since it has been handled, otherwise
        // it'll be handled at least twice and cause problem.
        const_cast<DOMNamedNodeMapInterface*>(attributes)->
            RemoveNamedItem(name.c_str());
        length--;
        break;
      }
    }
    if (!has_classid) {
      LOG("%s:%d:%d: No classid is specified for the object element",
          filename, xml_element->GetRow(), xml_element->GetColumn());
    }
  }

  for (size_t i = 0; i < length; i++) {
    const DOMAttrInterface *attr =
        down_cast<const DOMAttrInterface *>(attributes->GetItem(i));
    std::string name = attr->GetName();
    std::string value = attr->GetValue();
    if (GadgetStrCmp(kInnerTextProperty, name.c_str()) == 0) {
      LOG("%s is not allowed in XML as an attribute: ", kInnerTextProperty);
      continue;
    }

    if (GadgetStrCmp(kNameAttr, name.c_str()) != 0) {
      SetScriptableProperty(scriptable, script_context, filename,
                            attr->GetRow(), attr->GetColumn(),
                            name.c_str(), value.c_str(), tag_name.c_str());
    }
  }

  delete attributes;
  // "innerText" property is set in InsertElementFromDOM().
}

BasicElement *InsertElementFromDOM(Elements *elements,
                                   ScriptContextInterface *script_context,
                                   const DOMElementInterface *xml_element,
                                   const BasicElement *before,
                                   const char *filename) {
  std::string tag_name = xml_element->GetTagName();
  if (GadgetStrCmp(tag_name.c_str(), kScriptTag) == 0)
    return NULL;

  // Here doesn't comform to gadget case-sensitivity, but we believe nobody
  // will use "name" as other lower/upper char combinations.
  std::string name = xml_element->GetAttribute(kNameAttr);
  BasicElement *element = elements->InsertElement(tag_name.c_str(), before,
                                                  name.c_str());
  if (!element) {
    LOG("%s:%d:%d: Failed to create element %s", filename,
        xml_element->GetRow(), xml_element->GetColumn(), tag_name.c_str());
    return element;
  }

  SetupScriptableProperties(element, script_context, xml_element, filename);
  Elements *children = element->GetChildren();
  std::string text;
  for (const DOMNodeInterface *child = xml_element->GetFirstChild();
       child; child = child->GetNextSibling()) {
    DOMNodeInterface::NodeType type = child->GetNodeType();
    if (type == DOMNodeInterface::ELEMENT_NODE) {
      const DOMElementInterface *child_element =
          down_cast<const DOMElementInterface*>(child);
      std::string child_tag = child_element->GetTagName();

      // Special process for the child element (i.e. param element) of object
      // element. This is for the compatability with GDWin.
      // We set each param as a property of the real object wrapped in the
      // object element.
      if (element->IsInstanceOf(ObjectElement::CLASS_ID) &&
          GadgetStrCmp(child_tag.c_str(), kParamTag) == 0) {
        BasicElement *object = down_cast<ObjectElement*>(element)->GetObject();
        if (object) {
          // Here doesn't comform to gadget case-sensitivity, but we believe
          // nobody will use "name" or "value" as other lower/upper char
          // combinations.
          std::string param_name = child_element->GetAttribute(kNameAttr);
          std::string param_value = child_element->GetAttribute(kValueAttr);
          if (param_name.empty() || param_value.empty()) {
            LOG("%s:%d:%d: No name or value specified for param",
                filename, child_element->GetRow(), child_element->GetColumn());
          } else {
            SetScriptableProperty(object, script_context, filename,
                                  child_element->GetRow(),
                                  child_element->GetColumn(),
                                  param_name.c_str(), param_value.c_str(),
                                  kParamTag);
          }
        } else {
          // DLOG instead of LOG, because this must be caused by missing
          // classId, which has been LOG'ed.
          DLOG("%s:%d:%d: No object has been created for the object element",
               filename, xml_element->GetRow(), xml_element->GetColumn());
        }
      } else if (children) {
        InsertElementFromDOM(children, script_context,
                             child_element,
                             NULL, filename);
      }
    } else if (type == DOMNodeInterface::TEXT_NODE ||
               type == DOMNodeInterface::CDATA_SECTION_NODE) {
      text += down_cast<const DOMTextInterface *>(child)->GetTextContent();
    }
  }
  // Set the "innerText" property.
  text = TrimString(text);
  if (!text.empty()) {
    SetScriptableProperty(element, script_context, filename,
                          xml_element->GetRow(), xml_element->GetColumn(),
                          kInnerTextProperty, text.c_str(), tag_name.c_str());
  }
  return element;
}

} // namespace ggadget