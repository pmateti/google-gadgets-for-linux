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

#include <algorithm>

#include "combobox_element.h"
#include "canvas_interface.h"
#include "edit_element.h"
#include "elements.h"
#include "event.h"
#include "listbox_element.h"
#include "math_utils.h"
#include "gadget_consts.h"
#include "image.h"
#include "item_element.h"
#include "texture.h"
#include "scriptable_event.h"
#include "view.h"

namespace ggadget {

static const char kOnChangeEvent[] = "onchange";
static const char kOnTextChangeEvent[] = "ontextchange";

static const char *kTypeNames[] = {
  "dropdown", "droplist"
};

class ComboBoxElement::Impl {
 public:
  Impl(ComboBoxElement *owner, View *view)
      : owner_(owner), 
        mouseover_child_(NULL), grabbed_child_(NULL),
        maxitems_(10),
        keyboard_(false),
        listbox_(new ListBoxElement(owner, view, "listbox", "")),
        edit_(NULL),
        button_over_(false), button_down_(false), update_edit_value_(true),
        button_up_img_(view->LoadImageFromGlobal(kScrollDefaultRight, false)), 
        button_down_img_(view->LoadImageFromGlobal(kScrollDefaultRightDown, false)), 
        button_over_img_(view->LoadImageFromGlobal(kScrollDefaultRightOver, false)),
        background_(NULL) {
    // Register container methods since combobox is really a container.
    ElementsInterface *elements = listbox_->GetChildren();
    ASSERT(elements->IsInstanceOf(Elements::CLASS_ID));
    owner_->RegisterConstant("children", elements);
    owner_->RegisterMethod("appendElement",
                           NewSlot(down_cast<Elements *>(elements), 
                                   &Elements::AppendElementFromXML));
    owner_->RegisterMethod("insertElement",
                           NewSlot(down_cast<Elements *>(elements), 
                                   &Elements::InsertElementFromXML));
    owner_->RegisterMethod("removeElement",
                           NewSlot(down_cast<Elements *>(elements), 
                                   &Elements::RemoveElement));
    owner_->RegisterMethod("removeAllElements",
                           NewSlot(down_cast<Elements *>(elements), 
                                   &Elements::RemoveAllElements));

    listbox_->SetPixelX(0);
    listbox_->SetVisible(false);
    listbox_->SetAutoscroll(true);
    listbox_->ConnectOnChangeEvent(NewSlot(this, &Impl::ListBoxUpdated));
    listbox_->SetImplicit(true);
    view->OnElementAdd(listbox_); // ListBox is exposed to the View.

    CreateEdit(); // COMBO_DROPDOWN is default.
  }

  ~Impl() {
    owner_->GetView()->OnElementRemove(listbox_);
    delete listbox_;
    listbox_ = NULL;

    delete edit_;
    edit_ = NULL;

    delete background_;
    background_ = NULL;
  }

  ComboBoxElement *owner_;
  BasicElement *mouseover_child_, *grabbed_child_;
  int maxitems_;
  bool keyboard_;
  ListBoxElement *listbox_;
  EditElement *edit_; // is NULL if and only if COMBO_DROPLIST mode
  bool button_over_, button_down_;
  bool update_edit_value_;
  Image *button_up_img_, *button_down_img_, *button_over_img_;
  Texture *background_;
  EventSignal onchange_event_, ontextchange_event_;

  std::string GetSelectedText() {
    const ItemElement *item = listbox_->GetSelectedItem();
    if (item) {
      return item->GetLabelText();
    }
    return std::string();
  }

  void CreateEdit() {
    edit_ = new EditElement(owner_, owner_->GetView(), "");
    update_edit_value_ = true;
    edit_->ConnectOnChangeEvent(NewSlot(this, &Impl::TextChanged));
    edit_->SetImplicit(true);
  }

  void TextChanged() {    
    SimpleEvent event(Event::EVENT_CHANGE);
    ScriptableEvent s_event(&event, owner_, NULL);
    owner_->GetView()->FireEvent(&s_event, ontextchange_event_);
  }

  void ListBoxUpdated() {
    if (!keyboard_ && listbox_->IsVisible()) {
      // Close dropdown on selection.
      listbox_->SetVisible(false);
    }

    update_edit_value_ = true;

    // Relay this event to combobox's listeners.
    SimpleEvent event(Event::EVENT_CHANGE);
    ScriptableEvent s_event(&event, owner_, NULL);
    owner_->GetView()->FireEvent(&s_event, onchange_event_);
  }

  void SetListBoxHeight() {
    // GetPixelHeight is overridden, so specify BasicElement::
    double elem_height = owner_->BasicElement::GetPixelHeight();
    double item_height = listbox_->GetItemPixelHeight();

    double max_height = maxitems_ * item_height;    
    double height = std::min(max_height, elem_height - item_height);
    if (height < 0) {
      height = 0;
    }

    listbox_->SetPixelHeight(height);
  }

  void ScrollList(bool down) {
    int count = listbox_->GetChildren()->GetCount();
    if (count == 0) {
      return;
    }

    int index = listbox_->GetSelectedIndex();
    // Scroll wraps around.
    index += count + (down ? 1 : -1);
    index %= count;
    listbox_->SetSelectedIndex(index);
    listbox_->ScrollToIndex(index);
  }

  Image *GetButtonImage() {
    if (button_down_) {
      return button_down_img_;
    } else if (button_over_) {
      return button_over_img_;
    } else {
      return button_up_img_;
    }
  }
};

ComboBoxElement::ComboBoxElement(BasicElement *parent, View *view,
                                 const char *name)
    : BasicElement(parent, view, "combobox", name, false),
      impl_(new Impl(this, view)) {
  SetEnabled(true);  

  RegisterProperty("background",
                   NewSlot(this, &ComboBoxElement::GetBackground),
                   NewSlot(this, &ComboBoxElement::SetBackground));
  RegisterProperty("itemHeight",
                   NewSlot(impl_->listbox_, &ListBoxElement::GetItemHeight),
                   NewSlot(impl_->listbox_, &ListBoxElement::SetItemHeight));
  RegisterProperty("itemWidth",
                   NewSlot(impl_->listbox_, &ListBoxElement::GetItemWidth),
                   NewSlot(impl_->listbox_, &ListBoxElement::SetItemWidth));
  RegisterProperty("itemOverColor",
                   NewSlot(impl_->listbox_, &ListBoxElement::GetItemOverColor),
                   NewSlot(impl_->listbox_, &ListBoxElement::SetItemOverColor));
  RegisterProperty("itemSelectedColor",
                   NewSlot(impl_->listbox_, &ListBoxElement::GetItemSelectedColor),
                   NewSlot(impl_->listbox_, &ListBoxElement::SetItemSelectedColor));
  RegisterProperty("itemSeparator",
                   NewSlot(impl_->listbox_, &ListBoxElement::HasItemSeparator),
                   NewSlot(impl_->listbox_, &ListBoxElement::SetItemSeparator));
  RegisterProperty("selectedIndex",
                   NewSlot(impl_->listbox_, &ListBoxElement::GetSelectedIndex),
                   NewSlot(impl_->listbox_, &ListBoxElement::SetSelectedIndex));
  RegisterProperty("selectedItem",
                   NewSlot(impl_->listbox_, &ListBoxElement::GetSelectedItem),
                   NewSlot(impl_->listbox_, &ListBoxElement::SetSelectedItem));
  RegisterProperty("droplistVisible",
                   NewSlot(this, &ComboBoxElement::IsDroplistVisible),
                   NewSlot(this, &ComboBoxElement::SetDroplistVisible));
  RegisterProperty("maxDroplistItems",
                   NewSlot(this, &ComboBoxElement::GetMaxDroplistItems),
                   NewSlot(this, &ComboBoxElement::SetMaxDroplistItems));
  RegisterProperty("value",
                   NewSlot(this, &ComboBoxElement::GetValue),
                   NewSlot(this, &ComboBoxElement::SetValue));
  RegisterStringEnumProperty("type",
                             NewSlot(this, &ComboBoxElement::GetType),
                             NewSlot(this, &ComboBoxElement::SetType),
                             kTypeNames, arraysize(kTypeNames));

  RegisterMethod("clearSelection",
                 NewSlot(impl_->listbox_, &ListBoxElement::ClearSelection));

  // Version 5.5 newly added methods and properties.
  RegisterProperty("itemSeparatorColor",
                   NewSlot(impl_->listbox_, 
                           &ListBoxElement::GetItemSeparatorColor),
                   NewSlot(impl_->listbox_, 
                           &ListBoxElement::SetItemSeparatorColor));
  RegisterMethod("appendString",
                 NewSlot(impl_->listbox_, &ListBoxElement::AppendString));
  RegisterMethod("insertStringAt",
                 NewSlot(impl_->listbox_, &ListBoxElement::InsertStringAt));
  RegisterMethod("removeString",
                 NewSlot(impl_->listbox_, &ListBoxElement::RemoveString));

  // Disabled
  RegisterProperty("autoscroll",
                   NewSlot(this, &ComboBoxElement::IsAutoscroll),
                   NewSlot(this, &ComboBoxElement::SetAutoscroll));
  RegisterProperty("multiSelect",
                   NewSlot(this, &ComboBoxElement::IsMultiSelect),
                   NewSlot(this, &ComboBoxElement::SetMultiSelect));

  RegisterSignal(kOnChangeEvent, &impl_->onchange_event_);
  RegisterSignal(kOnTextChangeEvent, &impl_->ontextchange_event_);
}

ComboBoxElement::~ComboBoxElement() {
  delete impl_;
  impl_ = NULL;
}

void ComboBoxElement::DoDraw(CanvasInterface *canvas,
                             const CanvasInterface *children_canvas) {
  bool expanded = impl_->listbox_->IsVisible();
  double item_height = impl_->listbox_->GetItemPixelHeight();
  double elem_width = GetPixelWidth();
  bool c; // Used for drawing.

  if (impl_->background_) {
    // Crop before drawing background.
    double crop_height = item_height;
    if (expanded) {
      crop_height += impl_->listbox_->GetPixelHeight();
    }
    canvas->IntersectRectClipRegion(0, 0, elem_width, crop_height);
    impl_->background_->Draw(canvas);
  }

  if (impl_->edit_) {
    const CanvasInterface *editbox = impl_->edit_->Draw(&c);
    canvas->DrawCanvas(.0, .0, editbox);    
  } else {
    // Draw item      
    ItemElement *item = impl_->listbox_->GetSelectedItem();
    if (item) {
      item->SetDrawOverlay(false);
      const CanvasInterface *item_canvas = item->Draw(&c);
      item->SetDrawOverlay(true);

      // Support rotations, masks, etc. here. Windows version supports these, 
      // but is this really intended?
      double rotation = item->GetRotation();
      double pinx = item->GetPixelPinX(), piny = item->GetPixelPinY();
      bool transform = (rotation != 0 || pinx != 0 || piny != 0); 
      if (transform) {
        canvas->PushState();

        canvas->IntersectRectClipRegion(0, 0, elem_width, item_height);
        canvas->RotateCoordinates(DegreesToRadians(rotation));
        canvas->TranslateCoordinates(-pinx, -piny);
      }  

      const CanvasInterface *mask = item->GetMaskCanvas();
      if (mask) {
        canvas->DrawCanvasWithMask(0, 0, item_canvas, 0, 0, mask);
      } else {
        canvas->DrawCanvas(.0, .0, item_canvas);
      }

      if (transform) {
        canvas->PopState();
      }      
    }  
  }

  // Draw button
  Image *img = impl_->GetButtonImage();
  if (img) {
    double imgw = img->GetWidth();
    double x = elem_width - imgw;
    // Windows default color is 206 203 206 and leaves a 1px margin.
    canvas->DrawFilledRect(x, 1, imgw - 1, item_height - 2, 
                           Color::ColorFromChars(206, 203, 206));
    img->Draw(canvas, x, (item_height - img->GetHeight()) / 2);
  }

  // Draw listbox
  if (expanded) {
    const CanvasInterface *listbox = impl_->listbox_->Draw(&c);
    canvas->DrawCanvas(0, item_height, listbox);
  } 
}

EditElement *ComboBoxElement::GetEdit() {
  return impl_->edit_;
}

const EditElement *ComboBoxElement::GetEdit() const {
  return impl_->edit_;
}

ListBoxElement *ComboBoxElement::GetListBox() {
  return impl_->listbox_;
}

const ListBoxElement *ComboBoxElement::GetListBox() const {
  return impl_->listbox_;
}

const ElementsInterface *ComboBoxElement::GetChildren() const {
  return impl_->listbox_->GetChildren();
}

ElementsInterface *ComboBoxElement::GetChildren() {
  return impl_->listbox_->GetChildren();  
}

double ComboBoxElement::GetPixelHeight() const {
  if (impl_->listbox_->IsVisible()) {
    return BasicElement::GetPixelHeight(); 
  } else {
    return impl_->listbox_->GetItemPixelHeight();
  }
}

bool ComboBoxElement::IsDroplistVisible() const {
  return impl_->listbox_->IsVisible();
}

void ComboBoxElement::SetDroplistVisible(bool visible) {
  if (visible != impl_->listbox_->IsVisible()) {
    if (visible) {
      impl_->listbox_->ScrollToIndex(impl_->listbox_->GetSelectedIndex());
    }
    impl_->listbox_->SetVisible(visible);
  }
}

int ComboBoxElement::GetMaxDroplistItems() const {
  return impl_->maxitems_;
}

void ComboBoxElement::SetMaxDroplistItems(int max_droplist_items) {
  if (max_droplist_items != impl_->maxitems_) {
    impl_->maxitems_ = max_droplist_items;
    QueueDraw();
  }
}

ComboBoxElement::Type ComboBoxElement::GetType() const {
  if (impl_->edit_) {
    return COMBO_DROPDOWN;
  } else {
    return COMBO_DROPLIST;
  }
}

void ComboBoxElement::SetType(Type type) {
  if (type == COMBO_DROPDOWN) {
    if (!impl_->edit_) {
      impl_->CreateEdit();
      QueueDraw();
    }
  } else if (impl_->edit_) {
    delete impl_->edit_;
    impl_->edit_ = NULL;
    QueueDraw();
  }
}

std::string ComboBoxElement::GetValue() const {
  if (impl_->edit_) {
    return impl_->edit_->GetValue();
  } 
  // not used in droplist mode
  return std::string();
}

void ComboBoxElement::SetValue(const char *value) {
  if (impl_->edit_) {
    impl_->edit_->SetValue(value);
  } 
  // not used in droplist mode
}

bool ComboBoxElement::IsAutoscroll() const {
  return false; // Disabled
}

void ComboBoxElement::SetAutoscroll(bool autoscroll) {
  // Disabled
}

bool ComboBoxElement::IsMultiSelect() const {
  return false; // Disabled
}

void ComboBoxElement::SetMultiSelect(bool multiselect) {
  // Disabled
}

Variant ComboBoxElement::GetBackground() const {
  return Variant(Texture::GetSrc(impl_->background_));
}

void ComboBoxElement::SetBackground(const Variant &background) {
  delete impl_->background_;
  impl_->background_ = GetView()->LoadTexture(background);
  QueueDraw();
}

void ComboBoxElement::Layout() {
  BasicElement::Layout();
  double itemheight = impl_->listbox_->GetItemPixelHeight();
  double elem_width = GetPixelWidth();
  impl_->listbox_->SetPixelY(itemheight);
  impl_->listbox_->SetPixelWidth(elem_width);
  impl_->SetListBoxHeight();
  impl_->listbox_->Layout();
  if (impl_->edit_) {
    Image *img = impl_->GetButtonImage();
    impl_->edit_->SetPixelWidth(elem_width - (img ? img->GetWidth() : 0));
    impl_->edit_->SetPixelHeight(itemheight);

    if (impl_->update_edit_value_) {
      impl_->edit_->SetValue(impl_->GetSelectedText().c_str());
    }

    impl_->edit_->Layout();
  }
  impl_->update_edit_value_ = false;
}

EventResult ComboBoxElement::OnMouseEvent(const MouseEvent &event, bool direct,
                                     BasicElement **fired_element,
                                     BasicElement **in_element) {
  BasicElement *new_fired = NULL, *new_in = NULL;
  double new_y = event.GetY() - impl_->listbox_->GetPixelY();
  Event::Type t = event.GetType();
  bool expanded = impl_->listbox_->IsVisible();

  if (!expanded && new_y >= 0 && !direct) {
    // In listbox
    // This combobox will need to appear to be transparent to the elements 
    // below it if listbox is invisible.      
    return EVENT_RESULT_UNHANDLED;  
  }

  if (impl_->edit_) {
    EventResult r;   
    if (t == Event::EVENT_MOUSE_OUT && impl_->mouseover_child_) {
      // Case: Mouse moved out of parent and child at same time.
      // Clone mouse out event and send to child in addition to parent.
      MouseEvent new_event(event);
      impl_->mouseover_child_->OnMouseEvent(new_event, true, 
                                            &new_fired, &new_in);
      impl_->mouseover_child_ = NULL;

      // Do not return, parent needs to handle this mouse out event too.
    } else if (impl_->grabbed_child_ &&  
               (t == Event::EVENT_MOUSE_MOVE || t == Event::EVENT_MOUSE_UP 
                || t == Event::EVENT_MOUSE_CLICK)) { 
      // Case: Mouse is grabbed by child. Send to child regardless of position.
      // Send to child directly.
      MouseEvent new_event(event);
      r = impl_->grabbed_child_->OnMouseEvent(new_event, true, 
                                              fired_element, in_element);
      if (t == Event::EVENT_MOUSE_CLICK) {
        impl_->grabbed_child_->Focus();
      }
      if (t == Event::EVENT_MOUSE_CLICK || 
          !(event.GetButton() & MouseEvent::BUTTON_LEFT)) {
        impl_->grabbed_child_ = NULL; 
      }
      // Make editbox invisible to caller
      if (*fired_element == impl_->edit_) {
        *fired_element = this;
      }
      if (*in_element == impl_->edit_) {
        *in_element = this;
      }
      return r;
    } else if (event.GetX() < impl_->edit_->GetPixelWidth() && 
               new_y < 0 && !direct) { 
      // !direct is necessary to eliminate events grabbed when clicked on 
      // inactive parts of the combobox.
      // Case: Mouse is inside child. Dispatch event to child,
      // except in the case where the event is a mouse over event 
      // (when the mouse enters the child and parent at the same time).
      if (!impl_->mouseover_child_) {
        // Case: Mouse just moved inside child. Turn on mouseover bit and  
        // synthesize a mouse over event. The original event still needs to 
        // be dispatched to child.
        impl_->mouseover_child_ = impl_->edit_;
        MouseEvent in(Event::EVENT_MOUSE_OVER, event.GetX(), event.GetY(), 
                      event.GetButton(), event.GetWheelDelta(), 
                      event.GetModifier());
        impl_->mouseover_child_->OnMouseEvent(in, true, &new_fired, &new_in);
        // Ignore return from handler and don't return to continue processing.
        if (t == Event::EVENT_MOUSE_OVER) {
          // Case: Mouse entered child and parent at same time.
          // Parent also needs this event, so send to parent
          // and return.
          return BasicElement::OnMouseEvent(event, direct,  
                                            fired_element, in_element);         
        } 
      }

      // Send event to child.
      MouseEvent new_event(event);
      r = impl_->edit_->OnMouseEvent(new_event, direct,
                                     fired_element, in_element);
      // Make child invisible to caller
      if (*fired_element == impl_->edit_) {
        // Only grab events fired on combobox, and not its children 
        if (t == Event::EVENT_MOUSE_DOWN && 
            (event.GetButton() & MouseEvent::BUTTON_LEFT)) {
          impl_->grabbed_child_ = impl_->edit_;
        }
        *fired_element = this;
      }
      if (*in_element == impl_->edit_) {
        *in_element = this;
      }
      return r;
    } else if (impl_->mouseover_child_) {
      // Case: Mouse isn't in child, but mouseover bit is on, so turn 
      // it off and send a mouse out event to child. The original event is 
      // still dispatched to parent.
      MouseEvent new_event(Event::EVENT_MOUSE_OUT, event.GetX(), event.GetY(), 
                           event.GetButton(), event.GetWheelDelta(),
                           event.GetModifier());
      impl_->mouseover_child_->OnMouseEvent(new_event, true, 
                                            &new_fired, &new_in);
      impl_->mouseover_child_ = NULL;

      // Don't return, dispatch event to parent.
    }

    // Else not handled, send to BasicElement::OnMouseEvent
  }

  if (expanded && new_y >= 0 && !direct) {        
    MouseEvent new_event(event);
    new_event.SetY(new_y);
    return impl_->listbox_->OnMouseEvent(new_event, direct, 
                                         fired_element, in_element);
  }

  return  BasicElement::OnMouseEvent(event, direct, fired_element, in_element);
}

EventResult ComboBoxElement::OnDragEvent(const DragEvent &event, bool direct,
                                     BasicElement **fired_element) {
  double new_y = event.GetY() - impl_->listbox_->GetPixelY();
  if (!direct) { 
    if (new_y >= 0) {
      // In the listbox region.
      if (impl_->listbox_->IsVisible()) {
        DragEvent new_event(event);
        new_event.SetY(new_y);
        EventResult r = impl_->listbox_->OnDragEvent(new_event,
                                                     direct, fired_element);
        if (*fired_element == impl_->listbox_) {
          *fired_element = this;
        }
        return r;
      } else  {
        // This combobox will need to appear to be transparent to the elements 
        // below it if listbox is invisible.
        return EVENT_RESULT_UNHANDLED;
      }    
    } else if (impl_->edit_ && event.GetX() < impl_->edit_->GetPixelWidth()) {
      // In editbox.
      EventResult r = impl_->edit_->OnDragEvent(event, direct, fired_element);
      if (*fired_element == impl_->edit_) {
        *fired_element = this;
      }
      return r;
    }
  }

  return BasicElement::OnDragEvent(event, direct, fired_element);
}

EventResult ComboBoxElement::HandleMouseEvent(const MouseEvent &event) {
  // Only events NOT in the listbox region are ever passed to this handler.
  // So it's save to assume that these events are not for the listbox, with the 
  // exception of mouse wheel events.
  EventResult r = EVENT_RESULT_HANDLED;
  bool oldvalue;
  bool in_button = event.GetY() < impl_->listbox_->GetPixelY() &&
        event.GetX() >= (GetPixelWidth() - impl_->button_up_img_->GetWidth());
  switch (event.GetType()) {
    case Event::EVENT_MOUSE_MOVE:
      r = EVENT_RESULT_UNHANDLED;
      // Fall through.
    case Event::EVENT_MOUSE_OVER:
      oldvalue = impl_->button_over_;
      impl_->button_over_ = in_button;
      if (oldvalue != impl_->button_over_) {
        QueueDraw();
      }    
     break;
    case Event::EVENT_MOUSE_UP:
     if (impl_->button_down_) {
       impl_->button_down_ = false;
       QueueDraw();
     }
     break;    
    case Event::EVENT_MOUSE_DOWN:
     if (in_button && event.GetButton() & MouseEvent::BUTTON_LEFT) {
       impl_->button_down_ = true;
       QueueDraw();
     }
     break;
    case Event::EVENT_MOUSE_CLICK:
      // Toggle droplist visibility.
      SetDroplistVisible(!impl_->listbox_->IsVisible());
     break;
    case Event::EVENT_MOUSE_OUT:
     if (impl_->button_over_) {
       impl_->button_over_ = false;
       QueueDraw();
     }
     break;
    case Event::EVENT_MOUSE_WHEEL: 
     if (impl_->listbox_->IsVisible()) {
       r = impl_->listbox_->HandleMouseEvent(event);
     }
     break;   
   default:
    r = EVENT_RESULT_UNHANDLED;
    break; 
  }

  return r;
}

EventResult ComboBoxElement::HandleKeyEvent(const KeyboardEvent &event) {
  EventResult result = EVENT_RESULT_UNHANDLED;
  if (event.GetType() == Event::EVENT_KEY_DOWN) {
    result = EVENT_RESULT_HANDLED;
    switch (event.GetKeyCode()) {
     case KeyboardEvent::KEY_UP:
      impl_->keyboard_ = true;
      impl_->ScrollList(false);
      impl_->keyboard_ = false;
      break;
     case KeyboardEvent::KEY_DOWN:
      impl_->keyboard_ = true;
      impl_->ScrollList(true);
      impl_->keyboard_ = false;
      break;
     case KeyboardEvent::KEY_RETURN:
       // Windows only allows the box to be closed with the enter key,
       // not opened. Weird.
      if (impl_->listbox_->IsVisible()) {
        // Close dropdown on selection.
        impl_->listbox_->SetVisible(false);
      }
      break;
     default:
      result = EVENT_RESULT_UNHANDLED;
      break;
    }
  }
  return result;
}

Connection *ComboBoxElement::ConnectOnChangeEvent(Slot0<void> *slot) {
  return impl_->onchange_event_.Connect(slot);
}

BasicElement *ComboBoxElement::CreateInstance(BasicElement *parent,
                                              View *view,
                                              const char *name) {
  return new ComboBoxElement(parent, view, name);
}

} // namespace ggadget