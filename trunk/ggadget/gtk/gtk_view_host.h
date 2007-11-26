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

#ifndef GGADGET_GTK_GTK_VIEW_HOST_H__
#define GGADGET_GTK_GTK_VIEW_HOST_H__

#include <set>

#include <gtk/gtk.h>
#include <ggadget/ggadget.h>

class GadgetViewWidget;

namespace ggadget {

/**
 * An implementation of @c ViewHostInterface for the simple gadget host.
 * In this implementation, there is one instance of @c GtkViewHost per view,
 * and one instance of GraphicsInterface per @c GtkViewHost.
 */
class GtkViewHost : public ViewHostInterface {
 public:
  GtkViewHost(GadgetHostInterface *gadget_host,
              GadgetHostInterface::ViewType type,
              ScriptableInterface *prototype,
              bool composited, bool useshapemask, 
	      double zoom, int debug_mode);
  virtual ~GtkViewHost();

#if 0
  // TODO: This host should encapsulate all widget creating/switching jobs
  // inside of it, so the interface of this method should be revised.
  /**
   * Switches the GadgetViewWidget associated with this host.
   * When this is done, the original GadgetViewWidget will no longer have a
   * valid GtkCairoHost object. The new GadgetViewWidget is responsible for
   * freeing this host.
   */
  void SwitchWidget(GadgetViewWidget *new_gvw);
#endif

  virtual GadgetHostInterface *GetGadgetHost() const {
    return gadget_host_;
  }
  virtual ViewInterface *GetView() { return view_; }
  virtual const ViewInterface *GetView() const { return view_; }
  virtual ScriptContextInterface *GetScriptContext() const {
    return script_context_;
  }
  virtual XMLHttpRequestInterface *NewXMLHttpRequest();
  virtual const GraphicsInterface *GetGraphics() const { return gfx_; }

  virtual void QueueDraw();
  virtual bool GrabKeyboardFocus();

  virtual void SetResizeable();
  virtual void SetCaption(const char *caption);
  virtual void SetShowCaptionAlways(bool always);
  virtual void SetCursor(ElementInterface::CursorType type);
  virtual void RunDialog();
  virtual void ShowInDetailsView(const char *title, int flags,
                                 Slot1<void, int> *feedback_handler);
  virtual void CloseDetailsView();

  GadgetViewWidget *GetWidget() { ASSERT(gvw_); return gvw_; }

 private:
  static void OnDetailsViewDestroy(GtkObject *object, gpointer user_data);

  GadgetHostInterface *gadget_host_;
  ViewInterface *view_;
  ScriptContextInterface *script_context_;
  GadgetViewWidget *gvw_;
  GraphicsInterface *gfx_;
  Connection *onoptionchanged_connection_;
  GtkWidget *details_window_;
  Slot1<void, int> *details_feedback_handler_;

  DISALLOW_EVIL_CONSTRUCTORS(GtkViewHost);
};

} // namespace ggadget

#endif // GGADGET_GTK_GTK_VIEW_HOST_H__
