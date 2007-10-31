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

#ifndef GGADGETS_TEST_MOCKED_VIEW_H__
#define GGADGETS_TEST_MOCKED_VIEW_H__

#include <string>
#include "ggadget/signal.h"
#include "ggadget/scriptable_helper.h"
#include "ggadget/view_interface.h"

class MockedView : public ggadget::ViewInterface {
 public:
  MockedView(ggadget::ElementFactoryInterface *factory)
      : factory_(factory), draw_queued_(false) { }
  virtual ~MockedView() {}
 public:
  virtual int GetWidth() const { return 400; }
  virtual int GetHeight() const { return 300; }

  virtual bool AttachHost(ggadget::HostInterface *host) { return true; };
  virtual ggadget::ScriptContextInterface *GetScriptContext() const
      { return NULL; }
  virtual bool InitFromFile(const char *filename) { return true; }
  virtual ggadget::FileManagerInterface *GetFileManager() const { return NULL; }

  virtual void OnMouseEvent(ggadget::MouseEvent *event) {};
  virtual void OnKeyEvent(ggadget::KeyboardEvent *event) {};
  virtual void OnOtherEvent(ggadget::Event *event) {};
  virtual void OnTimerEvent(ggadget::TimerEvent *event) {};

  virtual bool SetWidth(int width) { return true; };
  virtual bool SetHeight(int height) { return true; };
  virtual bool SetSize(int width, int height) { return true; };   

  virtual void OnElementAdd(ggadget::ElementInterface *element) {};
  virtual void OnElementRemove(ggadget::ElementInterface *element) {};
  virtual void FireEvent(ggadget::ScriptableEvent *event,
                         const ggadget::EventSignal &event_signal) {};
  virtual ggadget::ScriptableEvent *GetEvent() { return NULL; }
  virtual const ggadget::ScriptableEvent *GetEvent() const { return NULL; }

  virtual const ggadget::CanvasInterface *Draw(bool *changed) { return NULL; };
  virtual void QueueDraw() { draw_queued_ = true; }
  virtual const ggadget::GraphicsInterface *GetGraphics() const { return NULL; }

  virtual void SetResizable(ResizableMode resizable) {};
  virtual ResizableMode GetResizable() const { return RESIZABLE_TRUE; };

  virtual void SetCaption(const char *caption) {};
  virtual const char *GetCaption() const { return ""; };
  virtual void SetShowCaptionAlways(bool show_always) {};
  virtual bool GetShowCaptionAlways() const { return false; };

  virtual ggadget::ElementFactoryInterface *GetElementFactory() const
      { return factory_; }
  virtual const ggadget::Elements *GetChildren() const { return NULL; };
  virtual ggadget::Elements *GetChildren() { return NULL; };
  virtual ggadget::ElementInterface *GetElementByName(
      const char *name) { return NULL; };
  virtual const ggadget::ElementInterface *GetElementByName(
      const char *name) const { return NULL; };

  virtual int BeginAnimation(ggadget::Slot1<void, int> *slot,
                             int start_value,
                             int end_value,
                             unsigned int duration) { return 0; }
  virtual void CancelAnimation(int token) { }
  virtual int SetTimeout(ggadget::Slot0<void> *slot,
                         unsigned int duration) { return 0; }
  virtual void ClearTimeout(int token) { }
  virtual int SetInterval(ggadget::Slot0<void> *slot,
                          unsigned int duration) { return 0; }
  virtual void ClearInterval(int token) { }
  virtual ggadget::Image *LoadImage(const char *name,
                                    bool is_mask) { return NULL; }
  virtual ggadget::Texture *LoadTexture(const char *name) { return NULL; }
  virtual int GetDebugMode() const { return 2; }
  virtual void SetFocus(ggadget::ElementInterface *element) { }

  DEFINE_CLASS_ID(0x8840c50905e84f15, ViewInterface)
  DEFAULT_OWNERSHIP_POLICY
  DELEGATE_SCRIPTABLE_INTERFACE(scriptable_helper_)
  virtual bool IsStrict() const { return true; }

  bool GetQueuedDraw() {
    bool b = draw_queued_;
    draw_queued_ = false;
    return b;
  }

 private:
  ggadget::ElementFactoryInterface *factory_;
  bool draw_queued_;
  ggadget::ScriptableHelper scriptable_helper_;
};

#endif // GGADGETS_TEST_MOCKED_VIEW_H__