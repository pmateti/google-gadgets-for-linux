#
# Copyright 2007 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

IF(GGL_BUILD_LIBGGADGET_QT)

APPLY_CONFIG(QT)

SET(CMAKE_INCLUDE_CURRENT_DIR ON)
SET(SRCS
  qt_canvas.cc
  qt_font.cc
  qt_gadget_widget.cc
  qt_graphics.cc
  qt_image.cc
  qt_main_loop.cc
  qt_menu.cc
  qt_view_host.cc
  utilities.cc
)

QT4_AUTOMOC(qt_menu.cc qt_main_loop.cc qt_gadget_widget.cc qt_view_host.cc)

ADD_LIBRARY(ggadget-qt SHARED ${SRCS})
TARGET_LINK_LIBRARIES(ggadget-qt ${QT_LIBRARIES})

OUTPUT_LIBRARY(ggadget-qt)

ENDIF(GGL_BUILD_LIBGGADGET_QT)