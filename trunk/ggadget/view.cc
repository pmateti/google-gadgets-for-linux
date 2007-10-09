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

#include <vector>
#include "view.h"
#include "element_factory_interface.h"
#include "element_interface.h"
#include "elements.h"
#include "event.h"
#include "file_manager_interface.h"
#include "gadget_interface.h"
#include "graphics_interface.h"
#include "host_interface.h"
#include "image.h"
#include "math_utils.h"
#include "script_context_interface.h"
#include "scriptable_delegator.h"
#include "scriptable_event.h"
#include "slot.h"
#include "texture.h"
#include "xml_utils.h"

namespace ggadget {

class View::Impl {
 public:
  Impl(ScriptContextInterface *script_context,
       GadgetInterface *gadget,
       ElementFactoryInterface *element_factory,
       View *owner)
    : owner_(owner),
      script_context_(script_context),
      gadget_(gadget),
      element_factory_(element_factory),
      children_(element_factory, NULL, owner),
      width_(200), height_(200),
      host_(NULL),
      // TODO: Make sure the default value.
      resizable_(ViewInterface::RESIZABLE_TRUE),
      show_caption_always_(false),
      current_timer_token_(1),
      focused_element_(NULL),
      mouseover_element_(NULL),
      grabmouse_element_(NULL),
      non_strict_delegator_(new ScriptableDelegator(owner, false)) {
  }

  ~Impl() {
    delete non_strict_delegator_;
    non_strict_delegator_ = NULL;

    TimerMap::iterator it = timer_map_.begin();
    while (it != timer_map_.end()) {
      TimerMap::iterator next = it;
      ++next;
      RemoveTimer(it->first);
      it = next;
    }
  }

  void MapChildMouseEvent(MouseEvent *org_event, ElementInterface *child,
                          MouseEvent *new_event) {
    ASSERT(child);
    std::vector<ElementInterface *> elements;
    for (ElementInterface *e = child; e != NULL; e = e->GetParentElement())
      elements.push_back(e);

    double x, y;
    ElementInterface *top = *(elements.end() - 1);
    ParentCoordToChildCoord(org_event->GetX(), org_event->GetY(),
                            top->GetPixelX(), top->GetPixelY(),
                            top->GetPixelPinX(), top->GetPixelPinY(),
                            DegreesToRadians(top->GetRotation()),
                            &x, &y);

    for (std::vector<ElementInterface *>::reverse_iterator it =
             elements.rbegin();
         // Note: Don't iterator to the last element.
         it < elements.rend() - 1; ++it) {
      // Make copies to prevent them from being overriden.
      double x1 = x, y1 = y;
      (*it)->SelfCoordToChildCoord(*(it + 1), x1, y1, &x, &y); 
    }
    new_event->SetX(x);
    new_event->SetY(y);
  }

  bool SendMouseEventToChildren(MouseEvent *event) {
    Event::Type type = event->GetType();
    if (type == Event::EVENT_MOUSE_OVER)
      // View's EVENT_MOUSE_OVER only applicable to itself.
      // Children's EVENT_MOUSE_OVER is triggered by other mouse events.
      return true;

    // If some element is grabbing mouse, send all EVENT_MOUSE_MOVE and
    // EVENT_MOUSE_UP events to it directly, until an EVENT_MOUSE_UP received.
    if (grabmouse_element_ && grabmouse_element_->IsEnabled() &&
        (type == Event::EVENT_MOUSE_MOVE || type == Event::EVENT_MOUSE_UP)) {
      MouseEvent new_event(*event);
      MapChildMouseEvent(event, grabmouse_element_, &new_event);
      grabmouse_element_->OnMouseEvent(event, true);

      // Release the grabbing.
      if (type == Event::EVENT_MOUSE_UP)
        grabmouse_element_ = NULL;
      return true;
    }

    if (type == Event::EVENT_MOUSE_OUT) {
      // Clear the mouseover state.
      if (mouseover_element_) {
        MouseEvent new_event(*event);
        MapChildMouseEvent(event, mouseover_element_, &new_event);
        mouseover_element_->OnMouseEvent(event, true);
        mouseover_element_ = NULL;
      }
      return true;
    }

    // Dispatch the event to children normally.
    ElementInterface *fired_element = children_.OnMouseEvent(event);
    if (fired_element && type == Event::EVENT_MOUSE_DOWN) {
      // Start grabbing.
      grabmouse_element_ = fired_element;
      SetFocus(fired_element);
      // In the focusin handler, the element may be removed and fired_element
      // points to invalid element.  However, grabmouse_element_ will be valid
      // or has been set to NULL.
      fired_element = grabmouse_element_;
    }

    if (fired_element != mouseover_element_) {
      ElementInterface *old_mouseover_element = mouseover_element_;
      // Store it early to prevent crash if fired_element is removed in
      // the mouseout handler.
      mouseover_element_ = fired_element;
      
      if (old_mouseover_element) {
        MouseEvent mouseout_event(Event::EVENT_MOUSE_OUT,
                                  event->GetX(), event->GetY(),
                                  event->GetButton(),
                                  event->GetWheelDelta());
        MapChildMouseEvent(event, old_mouseover_element, &mouseout_event);
        old_mouseover_element->OnMouseEvent(&mouseout_event, true);
      }

      if (mouseover_element_) {
        if (!mouseover_element_->IsEnabled())
          mouseover_element_ = NULL;
        else {
          MouseEvent mouseover_event(Event::EVENT_MOUSE_OVER,
                                     event->GetX(), event->GetY(),
                                     event->GetButton(),
                                     event->GetWheelDelta());
          MapChildMouseEvent(event, mouseover_element_, &mouseover_event);
          mouseover_element_->OnMouseEvent(&mouseover_event, true);
        }
      }
    }

    return fired_element != NULL;
  }

  void OnMouseEvent(MouseEvent *event) {
    // Send event to children first.
    if (SendMouseEventToChildren(event)) {
      // Then send event to view.
      ScriptableEvent scriptable_event(event, owner_, 0, 0);
      if (event->GetType() != Event::EVENT_MOUSE_MOVE)
        DLOG("%s(view): %g %g %d %d", scriptable_event.GetName(),
             event->GetX(), event->GetY(),
             event->GetButton(), event->GetWheelDelta());
      switch (event->GetType()) {
        case Event::EVENT_MOUSE_MOVE:
          // Put the high volume events near top.
          // View itself doesn't have onmousemove handler. 
          break;
        case Event::EVENT_MOUSE_DOWN:
          FireEvent(&scriptable_event, onmousedown_event_);
          break;
        case Event::EVENT_MOUSE_UP:
          FireEvent(&scriptable_event, onmouseup_event_);
          break;
        case Event::EVENT_MOUSE_CLICK:
          FireEvent(&scriptable_event, onclick_event_);
          break;
        case Event::EVENT_MOUSE_DBLCLICK:
          FireEvent(&scriptable_event, ondblclick_event_);
          break;
        case Event::EVENT_MOUSE_OUT:
          FireEvent(&scriptable_event, onmouseout_event_);
          break;
        case Event::EVENT_MOUSE_OVER:
          FireEvent(&scriptable_event, onmouseover_event_);
          break;
        case Event::EVENT_MOUSE_WHEEL:
          // View doesn't have mouse wheel event according to the API document.
          break;
        default:
          ASSERT(false);
      }
    }
  }

  void OnKeyEvent(KeyboardEvent *event) {
    ScriptableEvent scriptable_event(event, owner_, 0, 0);
    // TODO: dispatch to children.
    DLOG("%s(view): %d", scriptable_event.GetName(), event->GetKeyCode());
    switch (event->GetType()) {
      case Event::EVENT_KEY_DOWN:
        FireEvent(&scriptable_event, onkeydown_event_);
        break;
      case Event::EVENT_KEY_UP:
        FireEvent(&scriptable_event, onkeyup_event_);
        break;
      case Event::EVENT_KEY_PRESS:
        FireEvent(&scriptable_event, onkeypress_event_);
        break;
      default:
        ASSERT(false);
    }

    if (focused_element_) {
      if (!focused_element_->IsEnabled())
        focused_element_ = NULL;
      else
        focused_element_->OnKeyEvent(event);
    }
  }

  void OnTimerEvent(TimerEvent *event) {
    ASSERT(event->GetType() == Event::EVENT_TIMER_TICK);
    ElementInterface *target = event->GetTarget();
    if (target) {
      target->OnTimerEvent(event);
    } else {
      // The target is this view.
      int token = reinterpret_cast<int>(event->GetData());
      TimerMap::iterator it = timer_map_.find(token);
      if (it == timer_map_.end()) {
        LOG("Timer has been removed but event still fired: %d", token);
        return;
      }

      TimerInfo &info = it->second;
      ASSERT(info.token == token);

      switch (info.type) {
        case TIMER_TIMEOUT: {
          ScriptableEvent scriptable_event(event, owner_, token, 0);
          event_stack_.push_back(&scriptable_event);
          event->StopReceivingMore();
          info.slot->Call(0, NULL);
          RemoveTimer(token);
          event_stack_.pop_back();
          break;
        }
        case TIMER_INTERVAL: {
          ScriptableEvent scriptable_event(event, owner_, token, 0);
          event_stack_.push_back(&scriptable_event);
          info.slot->Call(0, NULL);
          event_stack_.pop_back();
          break;
        }
        case TIMER_ANIMATION: {
          uint64_t event_time = event->GetTimeStamp();
          double progress = static_cast<double>(event_time - info.start_time) /
                            1000.0 / info.duration;
          progress = std::min(1.0, std::max(0.0, progress));
          int value = info.start_value +
                      static_cast<int>(progress * info.spread + 0.5);
          if (value != info.last_value) {
            ScriptableEvent scriptable_event(event, owner_, token, value);
            event_stack_.push_back(&scriptable_event);
            info.last_value = value;
            Variant param(value);
            info.slot->Call(1, &param);
            event_stack_.pop_back();
          }

          event->StopReceivingMore();
          if (progress < 1.0 && host_) {
            // Remove and re-register timer to let the actual interval adapt
            // to the system performance. 
            host_->RemoveTimer(info.host_timer);
            // Reschedule timer
            info.host_timer = host_->RegisterTimer(
                kAnimationInterval, NULL, reinterpret_cast<void *>(token));
          } else {
            RemoveTimer(token);
          }
          break;
        }
        default:
          ASSERT(false);
          break;
      }
    }
  }

  void OnOtherEvent(Event *event) {
    switch (event->GetType()) {
      case Event::EVENT_FOCUS_IN:
        // For now we don't automatically set focus to some element.
        DLOG("focusin");
        break;
      case Event::EVENT_FOCUS_OUT:
        DLOG("focusout");
        SetFocus(NULL);
        break;
      default:
        ASSERT(false);
    }
  }

  void OnElementAdd(ElementInterface *element) {
    ASSERT(element);
    const char *name = element->GetName();
    if (name && *name &&
        // Don't overwrite the existing element with the same name.
        all_elements_.find(name) == all_elements_.end())
      all_elements_[name] = element;
  }

  void OnElementRemove(ElementInterface *element) {
    ASSERT(element);
    if (element == focused_element_)
      // Don't send EVENT_FOCUS_OUT because the element is being removed.
      focused_element_ = NULL;
    if (element == mouseover_element_)
      mouseover_element_ = NULL;
    if (element == grabmouse_element_)
      grabmouse_element_ = NULL;

    const char *name = element->GetName();
    if (name && *name) {
      ElementsMap::iterator it = all_elements_.find(name);
      if (it != all_elements_.end() && it->second == element)
        all_elements_.erase(it);
    }
  }

  void FireEvent(ScriptableEvent *event, const EventSignal &event_signal) {
    // Note: there is another place also do the similar thing: OnTimerEvent().
    event_stack_.push_back(event);
    event_signal();
    event_stack_.pop_back();
  }

  ScriptableEvent *GetEvent() const {
    return event_stack_.empty() ? NULL : event_stack_[event_stack_.size() - 1];
  }

  void SetFocus(ElementInterface *element) {
    if (element != focused_element_) {
      ElementInterface *old_focused_element = focused_element_;
      // Set it early to prevent the local "element" variable from being stale
      // if the element is removed in the event handler.
      focused_element_ = element;
      // Remove the current focus first.
      if (old_focused_element) {
        Event event(Event::EVENT_FOCUS_OUT);
        old_focused_element->OnOtherEvent(&event);
      }
  
      if (focused_element_) {
        if (!focused_element_->IsEnabled())
          focused_element_ = NULL;
        else {
          Event event(Event::EVENT_FOCUS_IN);
          focused_element_->OnOtherEvent(&event);
        }
      }
    }
  }

  bool SetWidth(int width) {
    if (width != width_) {
      // TODO check if allowed first
      width_ = width;  
      children_.OnParentWidthChange(width);
      if (host_) {
        host_->QueueDraw();
      }

      // TODO: Is NULL a proper current event object for the script?
      FireEvent(NULL, onsize_event_);
    }
    return true;
  }

  bool SetHeight(int height) {
    if (height != height_) {
      // TODO check if allowed first
      height_ = height;
      children_.OnParentHeightChange(height);
      if (host_) {
        host_->QueueDraw();
      }

      // TODO: Is NULL a proper current event object for the script?
      FireEvent(NULL, onsize_event_);
    }
    return true;
  }

  bool SetSize(int width, int height) {
    if (width != width_ || height != height_) {
      // TODO check if allowed first
      if (width != width_) {
        width_ = width;
        children_.OnParentWidthChange(width);
      }
      if (height != height_) {
        height_ = height;
        children_.OnParentHeightChange(height);
      }
      if (host_) {
        host_->QueueDraw();
      } 
      
      // TODO: Is NULL a proper current event object for the script?
      FireEvent(NULL, onsize_event_);
    }
    return true;
  }

  bool ResizeBy(int width, int height) {
    return SetSize(width_ + width, height_ + height);
  }

  bool AttachHost(HostInterface *host) {
    if (host_) {
      ASSERT(!host);
      // Detach old host first
      if (!host_->DetachFromView()) {
        return false;
      }
    }
    
    host_ = host;
  
    return true;
  }
   
  const CanvasInterface *Draw(bool *changed) {  
    ASSERT(host_);
    return children_.Draw(changed);
  }

  void SetResizable(ViewInterface::ResizableMode resizable) {
    resizable_ = resizable;
    // TODO:
  }

  void SetCaption(const char *caption) {
    caption_ = caption ? caption : "";
    // TODO: Redraw?
  }

  void SetShowCaptionAlways(bool show_always) {
    show_caption_always_ = show_always;
    // TODO: Redraw?
  }

  ElementInterface *GetElementByName(const char *name) {
    ElementsMap::iterator it = all_elements_.find(name);
    return it == all_elements_.end() ? NULL : it->second;
  }

  // For script.
  Variant GetElementByNameVariant(const char *name) {
    ElementInterface *result = GetElementByName(name);
    return result ? Variant(result) : Variant();
  }

  enum TimerType { TIMER_ANIMATION, TIMER_TIMEOUT, TIMER_INTERVAL };
  int NewTimer(TimerType type, Slot *slot,
               int start_value, int end_value, unsigned int duration) {
    ASSERT(slot);
    if (duration == 0 || !host_)
      return 0;

    // Find the next available timer token.
    // Ignore the error when all timer tokens are allocated.
    do {
      if (current_timer_token_ < INT_MAX) current_timer_token_++;
      else current_timer_token_ = 1;
    } while (timer_map_.find(current_timer_token_) != timer_map_.end());

    TimerInfo &info = timer_map_[current_timer_token_];
    info.token = current_timer_token_;
    info.type = type;
    info.slot = slot;
    info.start_value = start_value;
    info.last_value = end_value;
    info.spread = end_value - start_value;
    info.duration = duration;
    info.start_time = host_->GetCurrentTime();
    info.host_timer = host_->RegisterTimer(
        // For animation, the first event should be triggered imeediately.
        type == TIMER_ANIMATION ? 0 : duration,
        NULL,
        // Passing an integer is safer than passing a struct pointer.
        reinterpret_cast<void *>(current_timer_token_));
    return current_timer_token_;
  }

  void RemoveTimer(int token) {
    if (token == 0)
      return;

    TimerMap::iterator it = timer_map_.find(token);
    if (it == timer_map_.end()) {
      LOG("Invalid timer token to remove: %d", token);
      return;
    }

    if (host_)
      host_->RemoveTimer(it->second.host_timer);
    delete it->second.slot;
    timer_map_.erase(it);
  }

  int BeginAnimation(Slot *slot, int start_value, int end_value,
                     unsigned int duration) {
    return NewTimer(TIMER_ANIMATION, slot, start_value, end_value, duration);
  }

  void CancelAnimation(int token) {
    RemoveTimer(token);
  }

  int SetTimeout(Slot *slot, unsigned int duration) {
    return NewTimer(TIMER_TIMEOUT, slot, 0, 0, duration);
  }

  void ClearTimeout(int token) {
    RemoveTimer(token);
  }

  int SetInterval(Slot *slot, unsigned int duration) {
    return NewTimer(TIMER_INTERVAL, slot, 0, 0, duration);
  }

  void ClearInterval(int token) {
    RemoveTimer(token);
  }

  void Alert(const char *message) {
    LOG("ALERT: %s", message);
    // TODO:
  }

  bool Confirm(const char *message) {
    LOG("CONFIRM: %s", message);
    // TODO:
    return true;
  }

  EventSignal oncancle_event_;
  EventSignal onclick_event_;
  EventSignal onclose_event_;
  EventSignal ondblclick_event_;
  EventSignal ondock_event_;
  EventSignal onkeydown_event_;
  EventSignal onkeypress_event_;
  EventSignal onkeyup_event_;
  EventSignal onminimize_event_;
  EventSignal onmousedown_event_;
  EventSignal onmouseout_event_;
  EventSignal onmouseover_event_;
  EventSignal onmouseup_event_;
  EventSignal onok_event_;
  EventSignal onopen_event_;
  EventSignal onoptionchanged_event_;
  EventSignal onpopin_event_;
  EventSignal onpopout_event_;
  EventSignal onrestore_event_;
  EventSignal onsize_event_;
  EventSignal onsizing_event_;
  EventSignal onundock_event_;

  // Put all_elements_ here to make it the alst member to be destructed,
  // because destruction of children_ needs it.
  typedef std::map<std::string, ElementInterface *, GadgetStringComparator>
      ElementsMap;
  ElementsMap all_elements_;

  View *owner_;
  ScriptContextInterface *script_context_;
  GadgetInterface *gadget_;
  ElementFactoryInterface *element_factory_;
  Elements children_;
  int width_, height_;
  HostInterface *host_;
  ViewInterface::ResizableMode resizable_;
  std::string caption_;
  bool show_caption_always_;

  std::vector<ScriptableEvent *> event_stack_;

  static const unsigned int kAnimationInterval = 30;
  struct TimerInfo {
    int token;
    TimerType type;
    Slot *slot;
    int start_value;
    int last_value;
    int spread;
    unsigned int duration;
    uint64_t start_time;
    void *host_timer;
  };
  typedef std::map<int, TimerInfo> TimerMap;
  TimerMap timer_map_;
  int current_timer_token_;
  ElementInterface *focused_element_;
  ElementInterface *mouseover_element_;
  ElementInterface *grabmouse_element_;

  ScriptableDelegator *non_strict_delegator_;
};

static const char *kResizableNames[] = { "false", "true", "zoom" };

View::View(ScriptContextInterface *script_context,
           GadgetInterface *gadget,
           ScriptableInterface *prototype,
           ElementFactoryInterface *element_factory)
    : impl_(new Impl(script_context, gadget, element_factory, this)) {
  RegisterProperty("caption", NewSlot(this, &View::GetCaption),
                   NewSlot(this, &View::SetCaption));
  RegisterConstant("children", GetChildren());
  RegisterProperty("event", NewSlot(impl_, &Impl::GetEvent), NULL);
  RegisterProperty("height", NewSlot(this, &View::GetHeight),
                   NewSlot(this, &View::SetHeight));
  RegisterProperty("width", NewSlot(this, &View::GetWidth),
                   NewSlot(this, &View::SetWidth));
  RegisterStringEnumProperty("resizable", NewSlot(this, &View::GetResizable),
                             NewSlot(this, &View::SetResizable),
                             kResizableNames, arraysize(kResizableNames));
  RegisterProperty("showCaptionAlways",
                   NewSlot(this, &View::GetShowCaptionAlways),
                   NewSlot(this, &View::SetShowCaptionAlways));
  // The global view object is itself.
  RegisterConstant("view", this);

  RegisterMethod("appendElement",
                 NewSlot(GetChildren(), &Elements::AppendElementFromXML));
  RegisterMethod("insertElement",
                 NewSlot(GetChildren(), &Elements::InsertElementFromXML));
  RegisterMethod("removeElement",
                 NewSlot(GetChildren(), &Elements::RemoveElement));

  // Here register ViewImpl::BeginAnimation because the Slot1<void, int> *
  // parameter in View::BeginAnimation can't be automatically reflected.
  RegisterMethod("beginAnimation", NewSlot(impl_, &Impl::BeginAnimation));
  RegisterMethod("cancelAnimation", NewSlot(this, &View::CancelAnimation));
  RegisterMethod("setTimeout", NewSlot(impl_, &Impl::SetTimeout));
  RegisterMethod("clearTimeout", NewSlot(this, &View::ClearTimeout));
  RegisterMethod("setInterval", NewSlot(impl_, &Impl::SetInterval));
  RegisterMethod("clearInterval", NewSlot(this, &View::ClearInterval));

  RegisterMethod("alert", NewSlot(impl_, &Impl::Alert));
  RegisterMethod("confirm", NewSlot(impl_, &Impl::Confirm));

  RegisterMethod("resizeBy", NewSlot(impl_, &Impl::ResizeBy));
  RegisterMethod("resizeTo", NewSlot(this, &View::SetSize));

  // TODO: Move it to OptionsView?
  RegisterSignal(kOnCancelEvent, &impl_->oncancle_event_);
  RegisterSignal(kOnClickEvent, &impl_->onclick_event_);
  RegisterSignal(kOnCloseEvent, &impl_->onclose_event_);
  RegisterSignal(kOnDblClickEvent, &impl_->ondblclick_event_);
  RegisterSignal(kOnDockEvent, &impl_->ondock_event_);
  RegisterSignal(kOnKeyDownEvent, &impl_->onkeydown_event_);
  RegisterSignal(kOnKeyPressEvent, &impl_->onkeypress_event_);
  RegisterSignal(kOnKeyUpEvent, &impl_->onkeyup_event_);
  RegisterSignal(kOnMinimizeEvent, &impl_->onminimize_event_);
  RegisterSignal(kOnMouseDownEvent, &impl_->onmousedown_event_);
  RegisterSignal(kOnMouseOutEvent, &impl_->onmouseout_event_);
  RegisterSignal(kOnMouseOverEvent, &impl_->onmouseover_event_);
  RegisterSignal(kOnMouseUpEvent, &impl_->onmouseup_event_);
  // TODO: Move it to OptionsView?
  RegisterSignal(kOnOkEvent, &impl_->onok_event_);
  RegisterSignal(kOnOpenEvent, &impl_->onopen_event_);
  // TODO: Move it to OptionsView?
  RegisterSignal(kOnOptionChangedEvent, &impl_->onoptionchanged_event_);
  RegisterSignal(kOnPopInEvent, &impl_->onpopin_event_);
  RegisterSignal(kOnPopOutEvent, &impl_->onpopout_event_);
  RegisterSignal(kOnRestoreEvent, &impl_->onrestore_event_);
  RegisterSignal(kOnSizeEvent, &impl_->onsize_event_);
  RegisterSignal(kOnSizingEvent, &impl_->onsizing_event_);
  RegisterSignal(kOnUndockEvent, &impl_->onundock_event_);

  SetDynamicPropertyHandler(NewSlot(impl_, &Impl::GetElementByNameVariant),
                            NULL);

  if (prototype)
    SetPrototype(prototype);

  if (script_context)
    script_context->SetGlobalObject(impl_->non_strict_delegator_);
}

View::~View() {
  delete impl_;
  impl_ = NULL;
}

bool View::AttachHost(HostInterface *host) {
  return impl_->AttachHost(host);
}

bool View::InitFromFile(const char *filename) {
  std::string contents;
  std::string real_path;
  if (impl_->gadget_->GetFileManager()->GetXMLFileContents(
          filename, &contents, &real_path) &&
      SetupViewFromXML(this, contents.c_str(), real_path.c_str())) {
    impl_->onopen_event_();
    return true;
  } else {
    return false;
  }
}

ScriptContextInterface *View::GetScriptContext() const {
  return impl_->script_context_;
}

FileManagerInterface *View::GetFileManager() const {
  return impl_->gadget_->GetFileManager();
}

int View::GetWidth() const {
  return impl_->width_;
}

int View::GetHeight() const {
  return impl_->height_;
}
 
const CanvasInterface *View::Draw(bool *changed) {
  return impl_->Draw(changed);
}

void View::QueueDraw() {
  // Host may not be initialized during element construction.
  if (impl_->host_) {
    impl_->host_->QueueDraw();
  }
}

const GraphicsInterface *View::GetGraphics() const {
  return impl_->host_->GetGraphics();  
}

void View::OnMouseEvent(MouseEvent *event) {
  impl_->OnMouseEvent(event);
}

void View::OnKeyEvent(KeyboardEvent *event) {
  impl_->OnKeyEvent(event);
}

void View::OnTimerEvent(TimerEvent *event) {
  impl_->OnTimerEvent(event);
}

void View::OnOtherEvent(Event *event) {
  impl_->OnOtherEvent(event);
}

void View::OnElementAdd(ElementInterface *element) {
  impl_->OnElementAdd(element);
}

void View::OnElementRemove(ElementInterface *element) {
  impl_->OnElementRemove(element);
}

void View::FireEvent(ScriptableEvent *event, const EventSignal &event_signal) {
  impl_->FireEvent(event, event_signal);
}

ScriptableEvent *View::GetEvent() {
  return impl_->GetEvent();
}

const ScriptableEvent *View::GetEvent() const {
  return impl_->GetEvent();
}

bool View::SetWidth(int width) {
  return impl_->SetWidth(width);
}

bool View::SetHeight(int height) {
  return impl_->SetHeight(height);
}

bool View::SetSize(int width, int height) {
  return impl_->SetSize(width, height);
}

void View::SetResizable(ViewInterface::ResizableMode resizable) {
  impl_->SetResizable(resizable);
}

ElementFactoryInterface *View::GetElementFactory() const {
  return impl_->element_factory_;
}

const Elements *View::GetChildren() const {
  return &impl_->children_;
}

Elements *View::GetChildren() {
  return &impl_->children_;
}

ElementInterface *View::GetElementByName(const char *name) {
  return impl_->GetElementByName(name);
}

const ElementInterface *View::GetElementByName(const char *name) const {
  return impl_->GetElementByName(name);
}

ViewInterface::ResizableMode View::GetResizable() const {
  return impl_->resizable_;
}

void View::SetCaption(const char *caption) {
  impl_->SetCaption(caption);
}

const char *View::GetCaption() const {
  return impl_->caption_.c_str();
}

void View::SetShowCaptionAlways(bool show_always) {
  impl_->SetShowCaptionAlways(show_always);
}

bool View::GetShowCaptionAlways() const {
  return impl_->show_caption_always_;
}

int View::BeginAnimation(Slot1<void, int> *slot,
                         int start_value,
                         int end_value,
                         unsigned int duration) {
  return impl_->BeginAnimation(slot,
                               start_value, end_value, duration);
}

void View::CancelAnimation(int token) {
  impl_->CancelAnimation(token);
}

int View::SetTimeout(Slot0<void> *slot, unsigned int duration) {
  return impl_->SetTimeout(slot, duration);
}

void View::ClearTimeout(int token) {
  impl_->ClearTimeout(token);
}

int View::SetInterval(Slot0<void> *slot, unsigned int duration) {
  return impl_->SetInterval(slot, duration);
}

void View::ClearInterval(int token) {
  impl_->ClearInterval(token);
}

int View::GetDebugMode() const {
  return impl_->host_ ? impl_->host_->GetDebugMode() : 0;
}

Image *View::LoadImage(const char *name, bool is_mask) {
  ASSERT(impl_->host_);
  return new Image(GetGraphics(), impl_->gadget_->GetFileManager(),
                   name, is_mask);
}

Texture *View::LoadTexture(const char *name) {
  ASSERT(impl_->host_);
  return new Texture(GetGraphics(), impl_->gadget_->GetFileManager(), name);
}

void View::SetFocus(ElementInterface *element) {
  impl_->SetFocus(element);
}

} // namespace ggadget
