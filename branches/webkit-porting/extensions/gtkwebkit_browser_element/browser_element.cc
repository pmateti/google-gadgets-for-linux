/*
  Copyright 2009 Google Inc.

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
#include <string>
#include <cstring>
#include <gtk/gtk.h>
#include <ggadget/element_factory.h>
#include <ggadget/gadget.h>
#include <ggadget/logger.h>
#include <ggadget/main_loop_interface.h>
#include <ggadget/script_context_interface.h>
#include <ggadget/scriptable_array.h>
#include <ggadget/string_utils.h>
#include <ggadget/view.h>
#include <webkit/webkit.h>

#include "browser_element.h"

#ifdef GGL_GTK_WEBKIT_SUPPORT_JSC
#include <ggadget/script_runtime_manager.h>
#include "../webkit_script_runtime/js_script_runtime.h"
#include "../webkit_script_runtime/js_script_context.h"
#endif

#define Initialize gtkwebkit_browser_element_LTX_Initialize
#define Finalize gtkwebkit_browser_element_LTX_Finalize
#define RegisterElementExtension \
    gtkwebkit_browser_element_LTX_RegisterElementExtension

extern "C" {
  bool Initialize() {
    LOGI("Initialize gtkwebkit_browser_element extension.");
    return true;
  }

  void Finalize() {
    LOGI("Finalize gtkwebkit_browser_element extension.");
  }

  bool RegisterElementExtension(ggadget::ElementFactory *factory) {
    LOGI("Register gtkwebkit_browser_element extension, "
         "using name \"_browser\".");
    if (factory) {
      factory->RegisterElementClass(
          "_browser", &ggadget::gtkwebkit::BrowserElement::CreateInstance);
    }
    return true;
  }
}

using namespace ggadget::webkit;

namespace ggadget {
namespace gtkwebkit {

class BrowserElement::Impl {
 public:
  Impl(BrowserElement *owner)
    : content_type_("text/html"),
      owner_(owner),
      web_view_(NULL),
#ifdef GGL_GTK_WEBKIT_SUPPORT_JSC
      browser_context_(NULL),
#endif
      minimized_connection_(owner_->GetView()->ConnectOnMinimizeEvent(
          NewSlot(this, &Impl::OnViewMinimized))),
      restored_connection_(owner_->GetView()->ConnectOnRestoreEvent(
          NewSlot(this, &Impl::OnViewRestored))),
      popout_connection_(owner_->GetView()->ConnectOnPopOutEvent(
          NewSlot(this, &Impl::OnViewPoppedOut))),
      popin_connection_(owner_->GetView()->ConnectOnPopInEvent(
          NewSlot(this, &Impl::OnViewPoppedIn))),
      dock_connection_(owner_->GetView()->ConnectOnDockEvent(
          NewSlot(this, &Impl::OnViewDockUndock))),
      undock_connection_(owner_->GetView()->ConnectOnUndockEvent(
          NewSlot(this, &Impl::OnViewDockUndock))),
      popped_out_(false),
      minimized_(false),
      x_(0),
      y_(0),
      width_(0),
      height_(0) {
  }

  ~Impl() {
    // Indicates it's being destroyed.
    owner_ = NULL;

    minimized_connection_->Disconnect();
    restored_connection_->Disconnect();
    popout_connection_->Disconnect();
    popin_connection_->Disconnect();
    dock_connection_->Disconnect();
    undock_connection_->Disconnect();

#ifdef GGL_GTK_WEBKIT_SUPPORT_JSC
    delete browser_context_;
    browser_context_ = NULL;
#endif

    if (GTK_IS_WIDGET(web_view_)) {
      gtk_widget_destroy(web_view_);
      web_view_ = NULL;
    }
  }

  void GetWidgetExtents(gint *x, gint *y, gint *width, gint *height) {
    double widget_x0, widget_y0;
    double widget_x1, widget_y1;
    owner_->SelfCoordToViewCoord(0, 0, &widget_x0, &widget_y0);
    owner_->SelfCoordToViewCoord(owner_->GetPixelWidth(),
                                 owner_->GetPixelHeight(),
                                 &widget_x1, &widget_y1);

    owner_->GetView()->ViewCoordToNativeWidgetCoord(widget_x0, widget_y0,
                                                    &widget_x0, &widget_y0);
    owner_->GetView()->ViewCoordToNativeWidgetCoord(widget_x1, widget_y1,
                                                    &widget_x1, &widget_y1);

    *x = static_cast<gint>(round(widget_x0));
    *y = static_cast<gint>(round(widget_y0));
    *width = static_cast<gint>(ceil(widget_x1 - widget_x0));
    *height = static_cast<gint>(ceil(widget_y1 - widget_y0));
  }

  void EnsureBrowser() {
    if (!web_view_) {
      GtkWidget *container = GTK_WIDGET(owner_->GetView()->GetNativeWidget());
      if (!GTK_IS_FIXED(container)) {
        LOG("BrowserElement needs a GTK_FIXED parent. Actual type: %s",
            G_OBJECT_TYPE_NAME(container));
        return;
      }

      web_view_ = GTK_WIDGET(webkit_web_view_new());
      ASSERT(web_view_);

      g_signal_connect(G_OBJECT(web_view_), "destroy",
                       G_CALLBACK(WebViewDestroyed), this);
      g_signal_connect(G_OBJECT(web_view_), "console-message",
                       G_CALLBACK(WebViewConsoleMessage), this);
      g_signal_connect(G_OBJECT(web_view_), "load-started",
                       G_CALLBACK(WebViewLoadStarted), this);
      g_signal_connect(G_OBJECT(web_view_), "load-committed",
                       G_CALLBACK(WebViewLoadCommitted), this);
      g_signal_connect(G_OBJECT(web_view_), "load-progress-changed",
                       G_CALLBACK(WebViewLoadProgressChanged), this);
      g_signal_connect(G_OBJECT(web_view_), "load-finished",
                       G_CALLBACK(WebViewLoadFinished), this);
      g_signal_connect(G_OBJECT(web_view_), "hovering-over-link",
                       G_CALLBACK(WebViewHoveringOverLink), this);

#if WEBKIT_CHECK_VERSION(1,0,3)
      WebKitWebWindowFeatures *features =
          webkit_web_view_get_window_features(WEBKIT_WEB_VIEW(web_view_));
      ASSERT(features);
      g_signal_connect(G_OBJECT(features), "notify::width",
                       G_CALLBACK(WebViewWindowWidthNotify), this);
      g_signal_connect(G_OBJECT(features), "notify::height",
                       G_CALLBACK(WebViewWindowHeightNotify), this);

      g_signal_connect(G_OBJECT(web_view_), "create-web-view",
                       G_CALLBACK(WebViewCreateWebView), this);
      g_signal_connect(G_OBJECT(web_view_),
                       "navigation-policy-decision-requested",
                       G_CALLBACK(WebViewNavigationPolicyDecisionRequested),
                       this);
#else
      g_signal_connect(G_OBJECT(web_view_), "navigation-requested",
                       G_CALLBACK(WebViewNavigationRequested), this);
#endif

      GetWidgetExtents(&x_, &y_, &width_, &height_);

      gtk_fixed_put(GTK_FIXED(container), web_view_, x_, y_);
      gtk_widget_set_size_request(GTK_WIDGET(web_view_), width_, height_);
      gtk_widget_show(web_view_);

#ifdef GGL_GTK_WEBKIT_SUPPORT_JSC
      JSScriptRuntime *runtime = down_cast<JSScriptRuntime *>(
          ScriptRuntimeManager::get()->GetScriptRuntime("webkitjs"));

      if (runtime) {
        WebKitWebFrame *main_frame =
            webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(web_view_));
        ASSERT(main_frame);
        JSGlobalContextRef js_context =
            webkit_web_frame_get_global_context(main_frame);
        ASSERT(js_context);

        browser_context_ = runtime->WrapExistingContext(js_context);
        browser_context_->AssignFromNative(NULL, "", "external",
                                           Variant(external_object_.Get()));
      } else {
        LOGE("webkit-script-runtime is not loaded.");
      }
#endif

      if (content_.length()) {
        webkit_web_view_load_html_string(WEBKIT_WEB_VIEW(web_view_),
                                         content_.c_str(), "");
      }
    }
  }

  void Layout() {
    EnsureBrowser();
    GtkWidget *container = GTK_WIDGET(owner_->GetView()->GetNativeWidget());
    if (GTK_IS_FIXED(container) && WEBKIT_IS_WEB_VIEW(web_view_)) {
      bool force_layout = false;
      // check if the contain has changed.
      if (gtk_widget_get_parent(web_view_) != container) {
        gtk_widget_reparent(GTK_WIDGET(web_view_), container);
        force_layout = true;
      }

      gint x, y, width, height;
      GetWidgetExtents(&x, &y, &width, &height);

      if (x != x_ || y != y_ || force_layout) {
        x_ = x;
        y_ = y;
        gtk_fixed_move(GTK_FIXED(container), GTK_WIDGET(web_view_), x, y);
      }
      if (width != width_ || height != height_ || force_layout) {
        width_ = width;
        height_ = height;
        gtk_widget_set_size_request(GTK_WIDGET(web_view_), width, height);
      }
      if (owner_->IsReallyVisible() && (!minimized_ || popped_out_))
        gtk_widget_show(web_view_);
      else
        gtk_widget_hide(web_view_);
    }
  }

  void SetContent(const std::string &content) {
    DLOG("SetContent:\n%s", content.c_str());
    content_ = content;
    if (GTK_IS_WIDGET(web_view_)) {
      webkit_web_view_load_html_string(WEBKIT_WEB_VIEW(web_view_),
                                       content.c_str(), "");
    }
  }

  void SetExternalObject(ScriptableInterface *object) {
    DLOG("SetExternalObject(%p, CLSID=%ju)",
         object, object ? object->GetClassId() : 0);
    external_object_.Reset(object);
#ifdef GGL_GTK_WEBKIT_SUPPORT_JSC
    if (browser_context_)
      browser_context_->AssignFromNative(NULL, "", "external", Variant(object));
#endif
  }

  void OnViewMinimized() {
    // The browser widget must be hidden when the view is minimized.
    if (GTK_IS_WIDGET(web_view_) && !popped_out_) {
      gtk_widget_hide(web_view_);
    }
    minimized_ = true;
  }

  void OnViewRestored() {
    if (GTK_IS_WIDGET(web_view_) && owner_->IsReallyVisible() && !popped_out_) {
      gtk_widget_show(web_view_);
    }
    minimized_ = false;
  }

  void OnViewPoppedOut() {
    popped_out_ = true;
    Layout();
  }

  void OnViewPoppedIn() {
    popped_out_ = false;
    Layout();
  }

  void OnViewDockUndock() {
    // The toplevel window might be changed, so it's necessary to reparent the
    // browser widget.
    Layout();
  }

  bool OpenURL(const char *url) {
    Gadget *gadget = owner_->GetView()->GetGadget();
    bool result = false;
    if (gadget) {
      // Let the gadget allow this OpenURL gracefully.
      bool old_interaction = gadget->SetInUserInteraction(true);
      result = gadget->OpenURL(url);
      gadget->SetInUserInteraction(old_interaction);
    }
    return result;
  }

  bool HandleNavigationRequest(const char *old_uri, const char *new_uri) {
    bool result = false;
    size_t new_len = strlen(new_uri);
    size_t old_len = strlen(old_uri);
    const char *new_end = strrchr(new_uri, '#');
    if (new_end)
      new_len = new_end - new_uri;
    const char *old_end = strrchr(old_uri, '#');
    if (old_end)
      old_len = old_end - old_uri;
    // Treats urls with the same base url but different refs as equal.
    if (new_len != old_len || strncmp(new_uri, old_uri, new_len) != 0) {
      result = OpenURL(new_uri);
    }
    return result;
  }

  static void WebViewDestroyed(GtkWidget *widget, Impl *impl) {
    DLOG("WebViewDestroyed(Impl=%p, web_view=%p)", impl, widget);

    impl->web_view_ = NULL;
#ifdef GGL_GTK_WEBKIT_SUPPORT_JSC
    delete impl->browser_context_;
    impl->browser_context_ = NULL;
#endif
  }

  static gboolean WebViewConsoleMessage(WebKitWebView *web_view,
                                        gchar *message, gint line,
                                        gchar *source_id, Impl *impl) {
    if (!impl->owner_) return FALSE;
    ScopedLogContext log_context(impl->owner_->GetView()->GetGadget());
    LOGI("WebViewConsoleMessage(%s:%d): %s", source_id, line, message);
    return TRUE;
  }

  static void WebViewLoadStarted(WebKitWebView *web_view,
                                 WebKitWebFrame *web_frame,
                                 Impl *impl) {
    if (!impl->owner_) return;
    ScopedLogContext log_context(impl->owner_->GetView()->GetGadget());
    DLOG("WebViewLoadStarted(Impl=%p, web_view=%p, web_frame=%p)",
         impl, web_view, web_frame);
  }

  static void WebViewLoadCommitted(WebKitWebView *web_view,
                                   WebKitWebFrame *web_frame,
                                   Impl *impl) {
    if (!impl->owner_) return;
    ScopedLogContext log_context(impl->owner_->GetView()->GetGadget());
    DLOG("WebViewLoadCommitted(Impl=%p, web_view=%p, web_frame=%p)",
         impl, web_view, web_frame);
  }

  static void WebViewLoadProgressChanged(WebKitWebView *web_view,
                                         gint progress,
                                         Impl *impl) {
    if (!impl->owner_) return;
    ScopedLogContext log_context(impl->owner_->GetView()->GetGadget());
    DLOG("WebViewLoadProgressChanged(Impl=%p, web_view=%p, progress=%d)",
         impl, web_view, progress);
  }

  static void WebViewLoadFinished(WebKitWebView *web_view,
                                  WebKitWebFrame *web_frame,
                                  Impl *impl) {
    if (!impl->owner_) return;
    ScopedLogContext log_context(impl->owner_->GetView()->GetGadget());
    DLOG("WebViewLoadFinished(Impl=%p, web_view=%p, web_frame=%p)",
         impl, web_view, web_frame);
  }

  static void WebViewHoveringOverLink(WebKitWebView *web_view,
                                      const char *title,
                                      const char *uri,
                                      Impl *impl) {
    if (!impl->owner_) return;
    ScopedLogContext log_context(impl->owner_->GetView()->GetGadget());
    DLOG("WebViewHoveringOverLink(Impl=%p, web_view=%p, title=%s, uri=%s)",
         impl, web_view, title, uri);

    impl->hovering_over_uri_ = uri ? uri : "";
  }

#if WEBKIT_CHECK_VERSION(1,0,3)
  static WebKitWebView* WebViewCreateWebView(WebKitWebView *web_view,
                                             WebKitWebFrame *web_frame,
                                             Impl *impl) {
    if (!impl->owner_) return NULL;
    ScopedLogContext log_context(impl->owner_->GetView()->GetGadget());
    DLOG("WebViewCreateWebView(Impl=%p, web_view=%p, web_frame=%p)",
         impl, web_view, web_frame);

    // FIXME: is it necessary to create a hidden new webview and handle
    // navigation policy of the new webview?
    if (IsValidURL(impl->hovering_over_uri_.c_str()))
      impl->OpenURL(impl->hovering_over_uri_.c_str());

    return NULL;
  }

  static gboolean WebViewNavigationPolicyDecisionRequested(
      WebKitWebView *web_view, WebKitWebFrame *web_frame,
      WebKitNetworkRequest *request, WebKitWebNavigationAction *action,
      WebKitWebPolicyDecision *decision, Impl *impl) {
    if (!impl->owner_) return FALSE;
    const char *new_uri =
        webkit_network_request_get_uri(request);

    // original uri in action is not reliable, especially when the original
    // content has no uri.
    const char *original_uri = impl->loaded_uri_.c_str();

    WebKitWebNavigationReason reason =
        webkit_web_navigation_action_get_reason(action);

    ScopedLogContext log_context(impl->owner_->GetView()->GetGadget());
    DLOG("WebViewNavigationPolicyDecisionRequested"
         "(Impl=%p, web_view=%p, web_frame=%p):\n"
         "  New URI: %s\n"
         "  Reason: %d\n"
         "  Original URI: %s\n"
         "  Button: %d\n"
         "  Modifier: %d",
         impl, web_view, web_frame, new_uri, reason, original_uri,
         webkit_web_navigation_action_get_button(action),
         webkit_web_navigation_action_get_modifier_state(action));

    gboolean result = FALSE;
    if (reason == WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED) {
      result = impl->HandleNavigationRequest(original_uri, new_uri);
      if (result)
        webkit_web_policy_decision_ignore(decision);
    }

    if (!result)
      impl->loaded_uri_ = new_uri ? new_uri : "";

    return result;
  }

  static void WebViewWindowWidthNotify(WebKitWebWindowFeatures *features,
                                       GParamSpec *param,
                                       Impl *impl) {
    if (!impl->owner_) return;
    gint width = 0;
    g_object_get(features, "width", &width, NULL);
    ScopedLogContext log_context(impl->owner_->GetView()->GetGadget());
    DLOG("WebViewWindowWidthNotify(Impl=%p, width=%d)", impl, width);
  }

  static void WebViewWindowHeightNotify(WebKitWebWindowFeatures *features,
                                        GParamSpec *param,
                                        Impl *impl) {
    if (!impl->owner_) return;
    gint height = 0;
    g_object_get(features, "height", &height, NULL);
    ScopedLogContext log_context(impl->owner_->GetView()->GetGadget());
    DLOG("WebViewWindowHeightNotify(Impl=%p, width=%d)", impl, height);
  }
#else
  static WebKitNavigationResponse WebViewNavigationRequested(
      WebKitWebView *web_view, WebKitWebFrame *web_frame,
      WebKitNetworkRequest *request, Impl *impl) {
    if (!impl->owner_) return WEBKIT_NAVIGATION_RESPONSE_ACCEPT;
    const char *new_uri = webkit_network_request_get_uri(request);
    ScopedLogContext log_context(impl->owner_->GetView()->GetGadget());
    DLOG("WebViewNavigationRequested(Impl=%p, web_view=%p, "
         "web_frame=%p, uri=%s)",
         impl, web_view, web_frame, webkit_network_request_get_uri(request));

    if (strcmp(impl->hovering_over_uri_.c_str(), new_uri) == 0 &&
        impl->HandleNavigationRequest(impl->loaded_uri_.c_str(), new_uri)) {
      return WEBKIT_NAVIGATION_RESPONSE_IGNORE;
    }

    impl->loaded_uri_ = new_uri ? new_uri : "";
    return WEBKIT_NAVIGATION_RESPONSE_ACCEPT;
  }
#endif

  std::string content_type_;
  std::string content_;
  std::string hovering_over_uri_;
  std::string loaded_uri_;

  BrowserElement *owner_;
  GtkWidget *web_view_;

#ifdef GGL_GTK_WEBKIT_SUPPORT_JSC
  JSScriptContext *browser_context_;
#endif

  Connection *minimized_connection_;
  Connection *restored_connection_;
  Connection *popout_connection_;
  Connection *popin_connection_;
  Connection *dock_connection_;
  Connection *undock_connection_;

  ScriptableHolder<ScriptableInterface> external_object_;

  bool popped_out_;
  bool minimized_;

  gint x_;
  gint y_;
  gint width_;
  gint height_;
};

BrowserElement::BrowserElement(View *view, const char *name)
  : BasicElement(view, "browser", name, true),
    impl_(new Impl(this)) {
}

BrowserElement::~BrowserElement() {
  delete impl_;
  impl_ = NULL;
}

std::string BrowserElement::GetContentType() const {
  return impl_->content_type_;
}

void BrowserElement::SetContentType(const char *content_type) {
  impl_->content_type_ =
      content_type && *content_type ? content_type : "text/html";
}

void BrowserElement::SetContent(const std::string &content) {
  impl_->SetContent(content);
}

void BrowserElement::SetExternalObject(ScriptableInterface *object) {
  impl_->SetExternalObject(object);
}

void BrowserElement::Layout() {
  BasicElement::Layout();
  impl_->Layout();
}

void BrowserElement::DoDraw(CanvasInterface *canvas) {
}

BasicElement *BrowserElement::CreateInstance(View *view, const char *name) {
  return new BrowserElement(view, name);
}

void BrowserElement::DoClassRegister() {
  BasicElement::DoClassRegister();
  RegisterProperty("contentType",
                   NewSlot(&BrowserElement::GetContentType),
                   NewSlot(&BrowserElement::SetContentType));
  RegisterProperty("innerText", NULL,
                   NewSlot(&BrowserElement::SetContent));
  RegisterProperty("external", NULL,
                   NewSlot(&BrowserElement::SetExternalObject));
}

} // namespace gtkwebkit
} // namespace ggadget