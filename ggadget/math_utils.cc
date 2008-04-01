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
#include <cmath>

#include "math_utils.h"
#include <ggadget/logger.h>

namespace ggadget {

void ParentCoordToChildCoord(double parent_x, double parent_y,
                             double child_x_pos, double child_y_pos,
                             double child_pin_x, double child_pin_y,
                             double rotation_radians,
                             double *child_x, double *child_y) {
  double sin_theta = sin(rotation_radians);
  double cos_theta = cos(rotation_radians);
  double a_13 = child_pin_x - child_y_pos * sin_theta - child_x_pos * cos_theta;
  double a_23 = child_pin_y + child_x_pos * sin_theta - child_y_pos * cos_theta;

  *child_x = parent_x * cos_theta + parent_y * sin_theta + a_13;
  *child_y = parent_y * cos_theta - parent_x * sin_theta + a_23;
}

void ChildCoordToParentCoord(double child_x, double child_y,
                             double child_x_pos, double child_y_pos,
                             double child_pin_x, double child_pin_y,
                             double rotation_radians,
                             double *parent_x, double *parent_y) {
  double sin_theta = sin(rotation_radians);
  double cos_theta = cos(rotation_radians);
  double x0 = child_x_pos + child_pin_y * sin_theta - child_pin_x * cos_theta;
  double y0 = child_y_pos - child_pin_x * sin_theta - child_pin_y * cos_theta;

  *parent_x = child_x * cos_theta - child_y * sin_theta + x0;
  *parent_y = child_y * cos_theta + child_x * sin_theta + y0;
}

ChildCoordCalculator::ChildCoordCalculator(double child_x_pos,
                                           double child_y_pos,
                                           double child_pin_x,
                                           double child_pin_y,
                                           double rotation_radians) {
  sin_theta_ = sin(rotation_radians);
  cos_theta_ = cos(rotation_radians);
  a_13_ = child_pin_x - child_y_pos * sin_theta_ - child_x_pos * cos_theta_;
  a_23_ = child_pin_y + child_x_pos * sin_theta_ - child_y_pos * cos_theta_;
}

void GetChildExtentInParent(double child_x_pos, double child_y_pos,
                            double child_pin_x, double child_pin_y,
                            double child_width, double child_height,
                            double rotation_radians,
                            double *extent_width, double *extent_height) {
  rotation_radians = remainder(rotation_radians, 2 * M_PI);
  double sample_width_x, sample_width_y, sample_height_x, sample_height_y;
  if (rotation_radians < -M_PI_2) {
    // The bottom-left corner is the right most.
    sample_width_x = 0;
    sample_width_y = child_height;
    // The top-left corner is the lowest.
    sample_height_x = 0;
    sample_height_y = 0;
  } else if (rotation_radians < 0) {
    // The bottom-right corner is the right most.
    sample_width_x = child_width;
    sample_width_y = child_height;
    // The bottom-left corner is the lowest.
    sample_height_x = 0;
    sample_height_y = child_height;
  } else if (rotation_radians < M_PI_2) {
    // The top-right corner is the right most.
    sample_width_x = child_width;
    sample_width_y = 0;
    // The bottom-right corner is the lowest.
    sample_height_x = child_width;
    sample_height_y = child_height;
  } else {
    // The top-left corner is the right most.
    sample_width_x = 0;
    sample_width_y = 0;
    // The top-right corner is the lowest.
    sample_height_x = child_width;
    sample_height_y = 0;
  }

  ParentCoordCalculator calculator(child_x_pos, child_y_pos,
                                   child_pin_x, child_pin_y,
                                   rotation_radians);
  *extent_width = calculator.GetParentX(sample_width_x, sample_width_y);
  *extent_height = calculator.GetParentY(sample_height_x, sample_height_y);
}

void ChildCoordCalculator::Convert(double parent_x, double parent_y,
                                   double *child_x, double *child_y) {
  *child_x = parent_x * cos_theta_ + parent_y * sin_theta_ + a_13_;
  *child_y = parent_y * cos_theta_ - parent_x * sin_theta_ + a_23_;
}

double ChildCoordCalculator::GetChildX(double parent_x, double parent_y) {
  return parent_x * cos_theta_ + parent_y * sin_theta_ + a_13_;
}

double ChildCoordCalculator::GetChildY(double parent_x, double parent_y) {
  return parent_y * cos_theta_ - parent_x * sin_theta_ + a_23_;
}

ParentCoordCalculator::ParentCoordCalculator(double child_x_pos,
                                             double child_y_pos,
                                             double child_pin_x,
                                             double child_pin_y,
                                             double rotation_radians) {
  sin_theta_ = sin(rotation_radians);
  cos_theta_ = cos(rotation_radians);
  x0_ = child_x_pos + child_pin_y * sin_theta_ - child_pin_x * cos_theta_;
  y0_ = child_y_pos - child_pin_x * sin_theta_ - child_pin_y * cos_theta_;
}

void ParentCoordCalculator::Convert(double child_x, double child_y,
                                    double *parent_x, double *parent_y) {
  *parent_x = child_x * cos_theta_ - child_y * sin_theta_ + x0_;
  *parent_y = child_y * cos_theta_ + child_x * sin_theta_ + y0_;
}

double ParentCoordCalculator::GetParentX(double child_x, double child_y) {
  return child_x * cos_theta_ - child_y * sin_theta_ + x0_;
}

double ParentCoordCalculator::GetParentY(double child_x, double child_y) {
  return child_y * cos_theta_ + child_x * sin_theta_ + y0_;
}

double DegreesToRadians(double degrees) {
  return degrees * M_PI / 180.;
}

double RadiansToDegrees(double radians) {
  return radians * 180. / M_PI;
}

bool IsPointInElement(double x, double y, double width, double height) {
  return 0. <= x && 0. <= y && x < width && y < height;
}

static inline void GetMaxMin(double x, double y, double *max, double *min) {
  if (x > y) {
    *max = x;
    *min = y;
  } else {
    *max = y;
    *min = x;
  }
}

Rectangle::Rectangle(double ax, double ay, double aw, double ah)
  : x(ax), y(ay), w(aw), h(ah) {
}

Rectangle::Rectangle() : x(0), y(0), w(0), h(0) {
}

Rectangle Rectangle::GetPolygonExtents(size_t n, const double *vertexes) {
  ASSERT(n);
  ASSERT(vertexes);
  double xmin, xmax, ymin, ymax;

  xmin = xmax = vertexes[0];
  ymin = ymax = vertexes[1];

  n *= 2;
  for (size_t i = 2; i < n; i += 2) {
    xmin = std::min(xmin, vertexes[i]);
    xmax = std::max(xmax, vertexes[i]);
    ymin = std::min(ymin, vertexes[i + 1]);
    ymax = std::max(ymax, vertexes[i + 1]);
  }

  return Rectangle(xmin, ymin, xmax - xmin, ymax - ymin);
}

void Rectangle::Union(const Rectangle &rect) {
  double nx = std::min(x, rect.x);
  double ny = std::min(y, rect.y);
  double nw = std::max(x + w, rect.x + rect.w) - nx;
  double nh = std::max(y + h, rect.y + rect.h) - ny;
  x = nx;
  y = ny;
  w = nw;
  h = nh;
}

bool Rectangle::Intersect(const Rectangle &rect) {
  double xmax = std::min(x + w, rect.x + rect.w);
  double xmin = std::max(x, rect.x);
  double ymax = std::min(y + h, rect.y + rect.h);
  double ymin = std::max(y, rect.y);
  if (xmax <= xmin || ymax <= ymin) return false;

  x = xmin;
  y = ymin;
  w = xmax - xmin;
  h = ymax - ymin;
  return true;
}

bool Rectangle::Overlaps(const Rectangle &another) const {
  double xmax = std::min(x + w, another.x + another.w);
  double xmin = std::max(x, another.x);
  double ymax = std::min(y + h, another.y + another.h);
  double ymin = std::max(y, another.y);
  if (xmax <= xmin || ymax <= ymin) return false;
  return true;
}

bool Rectangle::IsInside(const Rectangle &another) const {
  return x >= another.x && (x + w) <= (another.x + another.w) &&
         y >= another.y && (y + h) <= (another.y + another.h);
}

bool Rectangle::IsPointIn(double px, double py) const {
  return px >= x && py >= y && px < x + w && py < y + h;
}

void Rectangle::Integerize(bool expand) {
  if (expand) {
    double nx = floor(x);
    double ny = floor(y);
    Set(nx, ny, ceil(w + x - nx), ceil(h + y - ny));
  } else {
    Set(round(x), round(y), round(w), round(h));
  }
}

void Rectangle::Set(double a_x, double a_y, double a_w, double a_h) {
  x = a_x;
  y = a_y;
  w = a_w;
  h = a_h;
}

void Rectangle::Reset() {
  x = y = w = h = 0;
}

bool Rectangle::operator==(const Rectangle &rect) const {
  return x == rect.x && y == rect.y && w == rect.w && h == rect.h;
}

} // namespace ggadget
