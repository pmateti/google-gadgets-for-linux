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

#include <cmath>
#include "div_element.h"
#include "canvas_interface.h"
#include "elements.h"
#include "event.h"
#include "string_utils.h"
#include "texture.h"
#include "view_interface.h"

namespace ggadget {

class DivElement::Impl {
 public:
  Impl()
      : background_texture_(NULL),
        autoscroll_(false),
        scroll_pos_x_(0), scroll_pos_y_(0),
        scroll_width_(0), scroll_height_(0),
        scroll_range_x_(0), scroll_range_y_(0) {
  }
  ~Impl() {
    delete background_texture_;
    background_texture_ = NULL;
  }

  void UpdateScrollPos(DivElement *owner, size_t width, size_t height) {
    scroll_width_ = static_cast<int>(width);
    scroll_height_ = static_cast<int>(height);
    int owner_width = static_cast<int>(ceil(owner->GetPixelWidth())); 
    int owner_height = static_cast<int>(ceil(owner->GetPixelHeight()));
    scroll_range_x_ = std::max(0, scroll_width_ - owner_width);
    scroll_range_y_ = std::max(0, scroll_height_ - owner_height);
    scroll_pos_x_ = std::min(scroll_pos_x_, scroll_range_x_);
    scroll_pos_y_ = std::min(scroll_pos_y_, scroll_range_y_);
    // TODO: consider the width and height of scroll bars.
  }

  void ScrollX(int distance) {
    scroll_pos_x_ += distance;
    scroll_pos_x_ = std::min(scroll_range_x_, std::max(0, scroll_pos_x_));
  }

  void ScrollY(int distance) {
    scroll_pos_y_ += distance;
    scroll_pos_y_ = std::min(scroll_range_y_, std::max(0, scroll_pos_y_));
  }

  void OnKeyEvent(DivElement *owner, KeyboardEvent *event) {
    if (autoscroll_ && event->GetType() == Event::EVENT_KEY_DOWN) {
      switch (event->GetKeyCode()) {
        case KeyboardEvent::KEY_UP:
          ScrollY(-kLineHeight);
          break;
        case KeyboardEvent::KEY_DOWN:
          ScrollY(kLineHeight);
          break;
        case KeyboardEvent::KEY_LEFT:
          ScrollX(-kLineWidth);
          break;
        case KeyboardEvent::KEY_RIGHT:
          ScrollX(kLineWidth);
          break;
        case KeyboardEvent::KEY_PAGE_UP:
          ScrollY(-static_cast<int>(ceil(owner->GetPixelHeight())));
          break;
        case KeyboardEvent::KEY_PAGE_DOWN:
          ScrollY(static_cast<int>(ceil(owner->GetPixelHeight())));
          break;
      }
      owner->GetView()->QueueDraw();
    }
  }

  static const int kLineHeight = 5;
  static const int kLineWidth = 5;

  std::string background_;
  Texture *background_texture_;
  bool autoscroll_;
  int scroll_pos_x_, scroll_pos_y_;
  int scroll_width_, scroll_height_;
  int scroll_range_x_, scroll_range_y_;
};

DivElement::DivElement(ElementInterface *parent,
                       ViewInterface *view,
                       const char *name)
    : BasicElement(parent, view, "div", name, true),
      impl_(new Impl) {
  RegisterProperty("autoscroll",
                   NewSlot(this, &DivElement::IsAutoscroll),
                   NewSlot(this, &DivElement::SetAutoscroll));
  RegisterProperty("background",
                   NewSlot(this, &DivElement::GetBackground),
                   NewSlot(this, &DivElement::SetBackground));
}

DivElement::~DivElement() {
  delete impl_;
}

void DivElement::DoDraw(CanvasInterface *canvas,
                        const CanvasInterface *children_canvas) {
  if (impl_->background_texture_)
    impl_->background_texture_->Draw(canvas);

  // TODO: scroll.
  if (children_canvas) {
    if (impl_->autoscroll_) {
      impl_->UpdateScrollPos(this,
                             children_canvas->GetWidth(),
                             children_canvas->GetHeight());
      canvas->DrawCanvas(-impl_->scroll_pos_x_, -impl_->scroll_pos_y_,
                         children_canvas);
    } else {
      canvas->DrawCanvas(0, 0, children_canvas);
    }
  }
}

const char *DivElement::GetBackground() const {
  return impl_->background_.c_str();
}

void DivElement::SetBackground(const char *background) {
  if (AssignIfDiffer(background, &impl_->background_)) {
    SetSelfChanged(true);
    delete impl_->background_texture_;
    impl_->background_texture_ = GetView()->LoadTexture(background);
  }
}

bool DivElement::IsAutoscroll() const {
  return impl_->autoscroll_;
}

void DivElement::SetAutoscroll(bool autoscroll) {
  if (impl_->autoscroll_ != autoscroll) {
    impl_->autoscroll_ = autoscroll;
    GetChildren()->SetScrollable(autoscroll);
    SetSelfChanged(true);
  }
}

ElementInterface *DivElement::CreateInstance(ElementInterface *parent,
                                             ViewInterface *view,
                                             const char *name) {
  return new DivElement(parent, view, name);
}

ElementInterface *DivElement::OnMouseEvent(MouseEvent *event, bool direct) {
  ElementInterface *fired = BasicElement::OnMouseEvent(event, direct);

  if (fired) {
    if (impl_->autoscroll_ && event->GetType() == Event::EVENT_MOUSE_WHEEL) {
      // TODO:
    }
  }
  return fired;
}

void DivElement::OnKeyEvent(KeyboardEvent *event) {
  impl_->OnKeyEvent(this, event);
  BasicElement::OnKeyEvent(event);
}

void DivElement::SelfCoordToChildCoord(ElementInterface *child,
                                       double x, double y,
                                       double *child_x, double *child_y) {
  BasicElement::SelfCoordToChildCoord(child,
                                      x - impl_->scroll_pos_x_,
                                      y - impl_->scroll_pos_y_,
                                      child_x, child_y);
}

} // namespace ggadget