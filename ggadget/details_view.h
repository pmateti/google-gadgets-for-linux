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

#ifndef GGADGET_DETAILS_VIEW_INFO_H__
#define GGADGET_DETAILS_VIEW_INFO_H__

#include <ggadget/common.h>
#include <ggadget/content_item.h>
#include <ggadget/scriptable_helper.h>

namespace ggadget {

class ScriptableOptions;

/**
 * DetailsView data structure. It has no built-in logic.
 */
class DetailsView : public ScriptableHelperOwnershipShared {
 public:
  DEFINE_CLASS_ID(0xf75ad2d79331421a, ScriptableInterface);

  DetailsView();
  virtual ~DetailsView();

 public:
  /**
   * Sets the content to be displayed in the details view content pane.
   * @param source origin of the content, @c NULL if not relevant.
   * @param time_created time at which the content was created (in UTC).
   * @param text actual text (plain text or html) of the content, or an XML
   *     filename.
   * @param time_absolute @c true if the time displayed is in absolute format
   *     or relative to current time.
   * @param layout layout of the details, usually the same layout as
   *     gadget content.
   */
  void SetContent(const char *source,
                  Date time_created,
                  const char *text,
                  bool time_absolute,
                  ContentItem::Layout layout);

  /**
   * Sets the content to be displayed directly from an item.
   * @param item item which gives the content.
   */
  void SetContentFromItem(ContentItem *item);

  std::string GetSource() const;
  Date GetTimeCreated() const;
  std::string GetText() const;
  bool IsTimeAbsolute() const;
  ContentItem::Layout GetLayout() const;

  /**
   * Gets and Sets if the content given to be displayed as HTML or plain text.
   * Use this in conjunction with the @c SetContent() or @c SetContentFromItem()
   * methods. Default is @c false.
   */
  bool ContentIsHTML();
  void SetContentIsHTML(bool is_html);

  /**
   * Gets and Sets if the content is an XML view. The plugin calls
   * @c SetContent() with the @c text parameter set to the name of the view
   * file, and sets this property to @c true. If the view file has the '.xml'
   * extension, this property will be set to @c true automatically.
   */
  bool ContentIsView() const;
  void SetContentIsView(bool is_view);

  /**
   * Gets the "detailsViewData" property used in XML details views.
   */ 
  const ScriptableOptions *GetDetailsViewData() const;
  ScriptableOptions *GetDetailsViewData();

  /**
   * Gets and sets the "external" property used in HTML details views.
   */ 
  ScriptableInterface *GetExternalObject() const;
  void SetExternalObject(ScriptableInterface *external_object);

 public:
  static DetailsView *CreateInstance();

 private:
  DISALLOW_EVIL_CONSTRUCTORS(DetailsView);
  class Impl;
  Impl *impl_;
};

} // namespace ggadget

#endif  // GGADGET_DETAILS_VIEW_INFO_H__