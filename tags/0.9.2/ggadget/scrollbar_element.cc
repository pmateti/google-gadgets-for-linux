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

#include "scrollbar_element.h"
#include "canvas_interface.h"
#include "canvas_utils.h"
#include "gadget_consts.h"
#include "image_interface.h"
#include "logger.h"
#include "math_utils.h"
#include "scriptable_binary_data.h"
#include "scriptable_event.h"
#include "string_utils.h"
#include "view.h"

namespace ggadget {

enum DisplayState {
  STATE_NORMAL,
  STATE_DOWN,
  STATE_OVER,
  STATE_COUNT
};

enum ScrollBarImage {
  IMAGE_BACKGROUND,
  IMAGE_GRIPPY,
  IMAGE_THUMB_START,
  IMAGE_THUMB_NORMAL = IMAGE_THUMB_START,
  IMAGE_THUMB_DOWN,
  IMAGE_THUMB_OVER,
  IMAGE_LEFT_START,
  IMAGE_LEFT_NORMAL = IMAGE_LEFT_START,
  IMAGE_LEFT_DOWN,
  IMAGE_LEFT_OVER,
  IMAGE_RIGHT_START,
  IMAGE_RIGHT_NORMAL = IMAGE_RIGHT_START,
  IMAGE_RIGHT_DOWN,
  IMAGE_RIGHT_OVER,
  IMAGE_COUNT
};

static const char *kHorizontalImages[] = {
  kScrollDefaultBackgroundH,
  kScrollDefaultGrippyH,
  kScrollDefaultThumbH,
  kScrollDefaultThumbDownH,
  kScrollDefaultThumbOverH,
  kScrollDefaultLeft,
  kScrollDefaultLeftDown,
  kScrollDefaultLeftOver,
  kScrollDefaultRight,
  kScrollDefaultRightDown,
  kScrollDefaultRightOver
};

static const char *kVerticalImages[] = {
  kScrollDefaultBackgroundV,
  kScrollDefaultGrippyV,
  kScrollDefaultThumbV,
  kScrollDefaultThumbDownV,
  kScrollDefaultThumbOverV,
  kScrollDefaultUp,
  kScrollDefaultUpDown,
  kScrollDefaultUpOver,
  kScrollDefaultDown,
  kScrollDefaultDownDown,
  kScrollDefaultDownOver
};

enum ScrollBarComponent {
  COMPONENT_NONE,
  COMPONENT_LEFT_BUTTON,
  COMPONENT_RIGHT_BUTTON,
  COMPONENT_LEFT_BAR,
  COMPONENT_RIGHT_BAR,
  COMPONENT_THUMB_BUTTON
};

static const char *kOrientationNames[] = {
  "vertical", "horizontal"
};

static const double kThumbMinSize = 16;
static const double kGrippyOffset = 12;

class ScrollBarElement::Impl {
 public:
  Impl(ScrollBarElement *owner)
      : owner_(owner),
        left_state_(STATE_NORMAL), right_state_(STATE_NORMAL),
        thumb_state_(STATE_NORMAL),
        // The values below are the default ones in Windows.
        min_(0), max_(100), value_(0), pagestep_(10), linestep_(1),
        accum_wheel_delta_(0), drag_delta_(0.),
        // Windows default to horizontal for orientation,
        // but puzzlingly use vertical images as default.
        orientation_(ORIENTATION_VERTICAL) {
    for (int i = 0; i < IMAGE_COUNT; i++) {
      images_[i] = NULL;
      image_is_default_[i] = true;
    }
  }

  ~Impl() {
    for (int i = 0; i < IMAGE_COUNT; i++)
      DestroyImage(images_[i]);
  }

  // Called when the orientation changes.
  void DestroyDefaultImages() {
    for (int i = 0; i < IMAGE_COUNT; i++) {
      if (image_is_default_[i]) {
        DestroyImage(images_[i]);
        images_[i] = NULL;
      }
    }
  }

  void EnsureDefaultImages() {
    View *view = owner_->GetView();
    const char **images_src = orientation_ == ORIENTATION_HORIZONTAL ?
                              kHorizontalImages : kVerticalImages;
    for (int i = 0; i < IMAGE_COUNT; i++) {
      if (!images_[i] && image_is_default_[i])
        images_[i] = view->LoadImageFromGlobal(images_src[i], false);
    }
  }

  void ClearDisplayStates() {
    left_state_ = STATE_NORMAL;
    right_state_ = STATE_NORMAL;
    thumb_state_ = STATE_NORMAL;
  }

  void GetImageSize(ImageInterface *image, bool flip, double *w, double *h) {
    if (image) {
      *w = image->GetWidth();
      *h = image->GetHeight();
      if (flip)
        std::swap(*w, *h);
    } else {
      *w = *h = 0;
    }
  }

  void Layout() {
    double width = owner_->GetPixelWidth();
    double height = owner_->GetPixelHeight();
    // flip: whether flip the coordinates between vertical to horizontal,
    bool flip = (orientation_ == ORIENTATION_VERTICAL);
    if (flip)
      std::swap(width, height);

    EnsureDefaultImages();

    double left_w, left_h, right_w, right_h, thumb_w, thumb_h;
    GetImageSize(images_[IMAGE_LEFT_START + left_state_], flip,
                 &left_w, &left_h);
    GetImageSize(images_[IMAGE_RIGHT_START + right_state_], flip,
                 &right_w, &right_h);
    GetImageSize(images_[IMAGE_THUMB_START + thumb_state_], flip,
                 &thumb_w, &thumb_h);

    left_rect_.Set(0, (height - left_h) / 2, left_w, left_h);
    right_rect_.Set(width - right_w, (height - right_h) / 2,
                    right_w, right_h);

    double position = max_ == min_ ? 0 :
                      static_cast<double>(value_ - min_) / (max_ - min_);
    double space = width - left_w - right_w;
    if (space <= 0) {
      thumb_rect_.Reset();
    } else {
      if (images_[IMAGE_GRIPPY] && max_ != min_) {
        // Grippy image specified, use proportional thumb.
        thumb_w = std::max(kThumbMinSize,
                           pagestep_ * space / (pagestep_ + max_ - min_));
      }

      if (space >= thumb_w) {
        thumb_rect_.Set(left_w + (space - thumb_w) * position,
                        (height - thumb_h) / 2,
                        thumb_w, thumb_h);
      } else {
        // The thumb fills the space.
        thumb_rect_.Set(left_w, (height - thumb_h) / 2,
                        space, thumb_h);
      }
    }
  }

  // Utility function for getting the int value from a position on the
  // scrollbar. It does not check to make sure that the value is within range.
  int GetValueFromLocation(double x, double y) {
    if (orientation_ == ORIENTATION_VERTICAL)
      std::swap(x, y);

    int delta = max_ - min_;
    double position = 0;
    double denominator = right_rect_.x - thumb_rect_.w - left_rect_.w;
    if (denominator != 0)
      position = delta * (x - left_rect_.w - drag_delta_) / denominator;
    return min_ + static_cast<int>(position);
  }

  void SetValue(int value) {
    if (value > max_) {
      value = max_;
    } else if (value < min_) {
      value = min_;
    }

    if (value != value_) {
      value_ = value;
      owner_->QueueDraw();
      SimpleEvent event(Event::EVENT_CHANGE);
      ScriptableEvent s_event(&event, owner_, NULL);
      owner_->GetView()->FireEvent(&s_event, onchange_event_);
    }
  }

  void Scroll(bool upleft, bool line) {
    int delta = line ? linestep_ : pagestep_;
    int v = value_ + (upleft ? -delta : delta);
    SetValue(v);
  }

  // Returns the scrollbar component that is under the (x, y) position.
  // For buttons, also return the rectangle of that component.
  // The result rectangle is in the actual coordinates.
  ScrollBarComponent GetComponentFromPosition(double x, double y,
                                              Rectangle *rect) {
    if (orientation_ == ORIENTATION_VERTICAL)
      std::swap(x, y);

    ScrollBarComponent result;
    // Check in reverse of drawn order: thumb, right, left.
    if (thumb_rect_.IsPointIn(x, y)) {
      *rect = thumb_rect_;
      result = COMPONENT_THUMB_BUTTON;
    } else if (left_rect_.IsPointIn(x, y)) {
      *rect = left_rect_;
      result = COMPONENT_LEFT_BUTTON;
    } else if (right_rect_.IsPointIn(x, y)) {
      *rect = right_rect_;
      result = COMPONENT_RIGHT_BUTTON;
    } else if (x < thumb_rect_.x) {
      result = COMPONENT_LEFT_BAR;
    } else {
      result = COMPONENT_RIGHT_BAR;
    }
    return result;
  }

  void DrawImage(CanvasInterface *canvas, ImageInterface *image, bool flip,
                 // Not a reference because we need a copy.
                 Rectangle rect) {
    if (image && rect.h > 0 && rect.w > 0) {
      if (flip) {
        std::swap(rect.x, rect.y);
        std::swap(rect.w, rect.h);
      }
      StretchMiddleDrawImage(image, canvas, rect.x, rect.y, rect.w, rect.h,
                             -1, -1, -1, -1);
    }
  }

  void DoDraw(CanvasInterface *canvas) {
    double width = owner_->GetPixelWidth();
    double height = owner_->GetPixelHeight();
    // flip: whether flip the coordinates between vertical to horizontal,
    bool flip = (orientation_ == ORIENTATION_VERTICAL);
    if (flip)
      std::swap(width, height);

    // Drawing order: background, left, right, thumb.
    DrawImage(canvas, images_[IMAGE_BACKGROUND], flip,
              Rectangle(0, 0, width, height));
    DrawImage(canvas, images_[IMAGE_LEFT_START + left_state_],
              flip, left_rect_);
    DrawImage(canvas, images_[IMAGE_RIGHT_START + right_state_],
              flip, right_rect_);
    DrawImage(canvas, images_[IMAGE_THUMB_START + thumb_state_],
              flip, thumb_rect_);

    if (images_[IMAGE_GRIPPY]) {
      double grippy_w, grippy_h;
      GetImageSize(images_[IMAGE_GRIPPY], flip, &grippy_w, &grippy_h);
      double min_grippy_size = kGrippyOffset * 2 + grippy_w;
      if (thumb_rect_.w > min_grippy_size) {
        Rectangle grippy_rect(thumb_rect_.x + (thumb_rect_.w - grippy_w) / 2,
                              (height - grippy_h) / 2, grippy_w, grippy_h);
        // Because the default grippy image contains interlaced black and
        // white pixels, integerize the rect to prevent the grippy image from
        // being blurred in most cases.
        grippy_rect.Integerize(false);
        DrawImage(canvas, images_[IMAGE_GRIPPY], flip, grippy_rect);
      }
    }
  }

  void LoadImage(const Variant &src, ScrollBarImage image) {
    DestroyImage(images_[image]);
    images_[image] = owner_->GetView()->LoadImage(src, false);
    image_is_default_[image] = false;
  }

  ScrollBarElement *owner_;
  DisplayState left_state_, right_state_, thumb_state_;
  // All the following rects are in horizontal coordinates, that is,
  // x and y, w and h are swapped when the orientation is vertical.
  Rectangle left_rect_, right_rect_, thumb_rect_;
  ImageInterface *images_[IMAGE_COUNT];
  bool image_is_default_[IMAGE_COUNT];
  int min_, max_, value_, pagestep_, linestep_;
  int accum_wheel_delta_;
  double drag_delta_;
  Orientation orientation_;
  EventSignal onchange_event_;
};

ScrollBarElement::ScrollBarElement(BasicElement *parent, View *view,
                                   const char *name)
    : BasicElement(parent, view, "scrollbar", name, false),
      impl_(new Impl(this)) {
}

void ScrollBarElement::DoRegister() {
  BasicElement::DoRegister();
  RegisterProperty("background",
                   NewSlot(this, &ScrollBarElement::GetBackground),
                   NewSlot(this, &ScrollBarElement::SetBackground));
  RegisterProperty("grippyImage",
                   NewSlot(this, &ScrollBarElement::GetGrippyImage),
                   NewSlot(this, &ScrollBarElement::SetGrippyImage));
  RegisterProperty("leftDownImage",
                   NewSlot(this, &ScrollBarElement::GetLeftDownImage),
                   NewSlot(this, &ScrollBarElement::SetLeftDownImage));
  RegisterProperty("leftImage",
                   NewSlot(this, &ScrollBarElement::GetLeftImage),
                   NewSlot(this, &ScrollBarElement::SetLeftImage));
  RegisterProperty("leftOverImage",
                   NewSlot(this, &ScrollBarElement::GetLeftOverImage),
                   NewSlot(this, &ScrollBarElement::SetLeftOverImage));
  RegisterProperty("lineStep",
                   NewSlot(this, &ScrollBarElement::GetLineStep),
                   NewSlot(this, &ScrollBarElement::SetLineStep));
  RegisterProperty("max",
                   NewSlot(this, &ScrollBarElement::GetMax),
                   NewSlot(this, &ScrollBarElement::SetMax));
  RegisterProperty("min",
                   NewSlot(this, &ScrollBarElement::GetMin),
                   NewSlot(this, &ScrollBarElement::SetMin));
  RegisterStringEnumProperty("orientation",
                   NewSlot(this, &ScrollBarElement::GetOrientation),
                   NewSlot(this, &ScrollBarElement::SetOrientation),
                   kOrientationNames, arraysize(kOrientationNames));
  RegisterProperty("pageStep",
                   NewSlot(this, &ScrollBarElement::GetPageStep),
                   NewSlot(this, &ScrollBarElement::SetPageStep));
  RegisterProperty("rightDownImage",
                   NewSlot(this, &ScrollBarElement::GetRightDownImage),
                   NewSlot(this, &ScrollBarElement::SetRightDownImage));
  RegisterProperty("rightImage",
                   NewSlot(this, &ScrollBarElement::GetRightImage),
                   NewSlot(this, &ScrollBarElement::SetRightImage));
  RegisterProperty("rightOverImage",
                   NewSlot(this, &ScrollBarElement::GetRightOverImage),
                   NewSlot(this, &ScrollBarElement::SetRightOverImage));
  RegisterProperty("thumbDownImage",
                   NewSlot(this, &ScrollBarElement::GetThumbDownImage),
                   NewSlot(this, &ScrollBarElement::SetThumbDownImage));
  RegisterProperty("thumbImage",
                   NewSlot(this, &ScrollBarElement::GetThumbImage),
                   NewSlot(this, &ScrollBarElement::SetThumbImage));
  RegisterProperty("thumbOverImage",
                   NewSlot(this, &ScrollBarElement::GetThumbOverImage),
                   NewSlot(this, &ScrollBarElement::SetThumbOverImage));
  RegisterProperty("value",
                   NewSlot(this, &ScrollBarElement::GetValue),
                   NewSlot(this, &ScrollBarElement::SetValue));

  RegisterSignal(kOnChangeEvent, &impl_->onchange_event_);
}

ScrollBarElement::~ScrollBarElement() {
  delete impl_;
  impl_ = NULL;
}

void ScrollBarElement::Layout() {
  BasicElement::Layout();
  impl_->Layout();
}

void ScrollBarElement::DoDraw(CanvasInterface *canvas) {
  impl_->DoDraw(canvas);
}

int ScrollBarElement::GetMax() const {
  return impl_->max_;
}

void ScrollBarElement::SetMax(int value) {
  if (value != impl_->max_) {
    impl_->max_ = value;
    if (impl_->value_ > value) {
      impl_->value_ = value;
    }
    QueueDraw();
  }
}

int ScrollBarElement::GetMin() const {
  return impl_->min_;
}

void ScrollBarElement::SetMin(int value) {
  if (value != impl_->min_) {
    impl_->min_ = value;
    if (impl_->value_ < value) {
      impl_->value_ = value;
    }
    QueueDraw();
  }
}

int ScrollBarElement::GetPageStep() const {
  return impl_->pagestep_;
}

void ScrollBarElement::SetPageStep(int value) {
  if (impl_->pagestep_ != value) {
    // Changing page step may change the size of thumb, so must QueueDraw().
    impl_->pagestep_ = value;
    QueueDraw();
  }
}

int ScrollBarElement::GetLineStep() const {
  return impl_->linestep_;
}

void ScrollBarElement::SetLineStep(int value) {
  // Changing line step doesn't change visual effect, so no QueueDraw().
  impl_->linestep_ = value;
}

int ScrollBarElement::GetValue() const {
  return impl_->value_;
}

void ScrollBarElement::SetValue(int value) {
  impl_->SetValue(value);
}

ScrollBarElement::Orientation ScrollBarElement::GetOrientation() const {
  return impl_->orientation_;
}

void ScrollBarElement::SetOrientation(ScrollBarElement::Orientation o) {
  if (o != impl_->orientation_) {
    impl_->DestroyDefaultImages();
    impl_->orientation_ = o;
    QueueDraw();
  }
}

Variant ScrollBarElement::GetBackground() const {
  return Variant(GetImageTag(impl_->images_[IMAGE_BACKGROUND]));
}

void ScrollBarElement::SetBackground(const Variant &img) {
  if (img != GetBackground()) {
    impl_->LoadImage(img, IMAGE_BACKGROUND);
    QueueDraw();
  }
}

Variant ScrollBarElement::GetGrippyImage() const {
  return Variant(GetImageTag(impl_->images_[IMAGE_GRIPPY]));
}

void ScrollBarElement::SetGrippyImage(const Variant &img) {
  if (img != GetGrippyImage()) {
    impl_->LoadImage(img, IMAGE_GRIPPY);
    QueueDraw();
  }
}

Variant ScrollBarElement::GetLeftDownImage() const {
  return Variant(GetImageTag(impl_->images_[IMAGE_LEFT_DOWN]));
}

void ScrollBarElement::SetLeftDownImage(const Variant &img) {
  if (img != GetLeftDownImage()) {
    impl_->LoadImage(img, IMAGE_LEFT_DOWN);
    if (impl_->left_state_ == STATE_DOWN) {
      QueueDraw();
    }
  }
}

Variant ScrollBarElement::GetLeftImage() const {
  return Variant(GetImageTag(impl_->images_[IMAGE_LEFT_NORMAL]));
}

void ScrollBarElement::SetLeftImage(const Variant &img) {
  if (img != GetLeftImage()) {
    impl_->LoadImage(img, IMAGE_LEFT_NORMAL);
    if (impl_->left_state_ == STATE_NORMAL) {
      QueueDraw();
    }
  }
}

Variant ScrollBarElement::GetLeftOverImage() const {
  return Variant(GetImageTag(impl_->images_[IMAGE_LEFT_OVER]));
}

void ScrollBarElement::SetLeftOverImage(const Variant &img) {
  if (img != GetLeftOverImage()) {
    impl_->LoadImage(img, IMAGE_LEFT_OVER);
    if (impl_->left_state_ == STATE_OVER) {
      QueueDraw();
    }
  }
}

Variant ScrollBarElement::GetRightDownImage() const {
  return Variant(GetImageTag(impl_->images_[IMAGE_RIGHT_DOWN]));
}

void ScrollBarElement::SetRightDownImage(const Variant &img) {
  if (img != GetRightDownImage()) {
    impl_->LoadImage(img, IMAGE_RIGHT_DOWN);
    if (impl_->right_state_ == STATE_DOWN) {
      QueueDraw();
    }
  }
}

Variant ScrollBarElement::GetRightImage() const {
  return Variant(GetImageTag(impl_->images_[IMAGE_RIGHT_NORMAL]));
}

void ScrollBarElement::SetRightImage(const Variant &img) {
  if (img != GetRightImage()) {
    impl_->LoadImage(img, IMAGE_RIGHT_NORMAL);
    if (impl_->right_state_ == STATE_NORMAL) {
      QueueDraw();
    }
  }
}

Variant ScrollBarElement::GetRightOverImage() const {
  return Variant(GetImageTag(impl_->images_[IMAGE_RIGHT_OVER]));
}

void ScrollBarElement::SetRightOverImage(const Variant &img) {
  if (img != GetRightOverImage()) {
    impl_->LoadImage(img, IMAGE_RIGHT_OVER);
    if (impl_->right_state_ == STATE_OVER) {
      QueueDraw();
    }
  }
}

Variant ScrollBarElement::GetThumbDownImage() const {
  return Variant(GetImageTag(impl_->images_[IMAGE_THUMB_DOWN]));
}

void ScrollBarElement::SetThumbDownImage(const Variant &img) {
  if (img != GetThumbDownImage()) {
    impl_->LoadImage(img, IMAGE_THUMB_DOWN);
    if (impl_->thumb_state_ == STATE_DOWN) {
      QueueDraw();
    }
  }
}

Variant ScrollBarElement::GetThumbImage() const {
  return Variant(GetImageTag(impl_->images_[IMAGE_THUMB_NORMAL]));
}

void ScrollBarElement::SetThumbImage(const Variant &img) {
  if (img != GetThumbImage()) {
    impl_->LoadImage(img, IMAGE_THUMB_NORMAL);
    if (impl_->thumb_state_ == STATE_NORMAL) {
      QueueDraw();
    }
  }
}

Variant ScrollBarElement::GetThumbOverImage() const {
  return Variant(GetImageTag(impl_->images_[IMAGE_THUMB_OVER]));
}

void ScrollBarElement::SetThumbOverImage(const Variant &img) {
  if (img != GetThumbOverImage()) {
    impl_->LoadImage(img, IMAGE_THUMB_OVER);
    if (impl_->thumb_state_ == STATE_OVER) {
      QueueDraw();
    }
  }
}

BasicElement *ScrollBarElement::CreateInstance(BasicElement *parent,
                                               View *view, const char *name) {
  // Keep backward compatibility, default not to use grippy unless it is set
  // by the gadget.
  return new ScrollBarElement(parent, view, name);
}

EventResult ScrollBarElement::HandleMouseEvent(const MouseEvent &event) {
  EventResult result = EVENT_RESULT_HANDLED;
  Rectangle comp_rect;
  ScrollBarComponent c = COMPONENT_NONE;
  if (event.GetType() != Event::EVENT_MOUSE_OUT) {
    c = impl_->GetComponentFromPosition(event.GetX(), event.GetY(),
                                        &comp_rect);
  }

  // Resolve in opposite order as drawn: thumb, right, left.
  switch (event.GetType()) {
    case Event::EVENT_MOUSE_MOVE:
    case Event::EVENT_MOUSE_OUT:
    case Event::EVENT_MOUSE_OVER: {
      DisplayState oldthumb = impl_->thumb_state_;
      DisplayState oldleft = impl_->left_state_;
      DisplayState oldright = impl_->right_state_;
      impl_->ClearDisplayStates();
      if (c == COMPONENT_THUMB_BUTTON) {
        impl_->thumb_state_ = STATE_OVER;
      } else if (c == COMPONENT_RIGHT_BUTTON) {
        impl_->right_state_ = STATE_OVER;
      } else if (c == COMPONENT_LEFT_BUTTON) {
        impl_->left_state_ = STATE_OVER;
      }

      // Restore the down states, overwriting the over states if necessary.
      if (oldthumb == STATE_DOWN) {
        impl_->thumb_state_ = STATE_DOWN;
        // Special case, need to scroll.
        int v = impl_->GetValueFromLocation(event.GetX(), event.GetY());
        SetValue(v);
        break;
      } else if (oldright == STATE_DOWN) {
        impl_->right_state_ = STATE_DOWN;
      } else if (oldleft == STATE_DOWN) {
        impl_->left_state_ = STATE_DOWN;
      }

      bool redraw = (impl_->left_state_ != oldleft ||
                     impl_->right_state_ != oldright ||
                     impl_->thumb_state_ != oldthumb);
      if (redraw) {
        QueueDraw();
      }
      break;
    }

    case Event::EVENT_MOUSE_DOWN:
     if (event.GetButton() & MouseEvent::BUTTON_LEFT) {
       bool upleft = true, line = true;
       impl_->ClearDisplayStates();
       if (c == COMPONENT_THUMB_BUTTON) {
         impl_->thumb_state_ = STATE_DOWN;
         if (impl_->orientation_ == ORIENTATION_HORIZONTAL) {
           impl_->drag_delta_ = event.GetX() - comp_rect.x;
         } else {
           // Note: still use comp_rect.x because the rect is in flipped
           // coordinates.
           impl_->drag_delta_ = event.GetY() - comp_rect.x;
         }
         QueueDraw();
         break; // don't scroll, early exit
       } else if (c == COMPONENT_RIGHT_BUTTON) {
         impl_->right_state_ = STATE_DOWN;
         upleft = false; line = true;
       } else if (c == COMPONENT_RIGHT_BAR) {
         upleft = line = false;
       } else if (c == COMPONENT_LEFT_BUTTON) {
         impl_->left_state_ = STATE_DOWN;
         upleft = line = true;
       } else if (c == COMPONENT_LEFT_BAR) {
         upleft = true; line = false;
       }
       impl_->Scroll(upleft, line);
     }
     break;
    case Event::EVENT_MOUSE_UP:
     if (event.GetButton() & MouseEvent::BUTTON_LEFT) {
       DisplayState oldthumb = impl_->thumb_state_;
       DisplayState oldleft = impl_->left_state_;
       DisplayState oldright = impl_->right_state_;
       impl_->ClearDisplayStates();
       if (c == COMPONENT_THUMB_BUTTON) {
         impl_->thumb_state_ = STATE_OVER;
       } else if (c == COMPONENT_RIGHT_BUTTON) {
         impl_->right_state_ = STATE_OVER;
       } else if (c == COMPONENT_LEFT_BUTTON) {
         impl_->left_state_ = STATE_OVER;
       }
       bool redraw = (impl_->left_state_ != oldleft ||
           impl_->right_state_ != oldright ||
           impl_->thumb_state_ != oldthumb);
       if (redraw) {
         QueueDraw();
       }
     }
     break;
    case Event::EVENT_MOUSE_WHEEL: {
      impl_->accum_wheel_delta_ += event.GetWheelDeltaY();
      bool upleft;
      if (impl_->accum_wheel_delta_ >= MouseEvent::kWheelDelta) {
        impl_->accum_wheel_delta_ -= MouseEvent::kWheelDelta;
        upleft = false;
      } else if (impl_->accum_wheel_delta_ <= -MouseEvent::kWheelDelta) {
        impl_->accum_wheel_delta_ += MouseEvent::kWheelDelta;
        upleft = true;
      } else {
        break; // don't scroll in this case
      }
      impl_->Scroll(upleft, true);
      break;
    }

    default:
      result = EVENT_RESULT_UNHANDLED;
      break;
  }
  return result;
}

Connection *ScrollBarElement::ConnectOnChangeEvent(Slot0<void> *slot) {
  return impl_->onchange_event_.Connect(slot);
}

bool ScrollBarElement::HasOpaqueBackground() const {
  ImageInterface *background = impl_->images_[IMAGE_BACKGROUND];
  return background && background->IsFullyOpaque();
}


} // namespace ggadget
