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

#ifndef GGADGET_GTK_GLOBAL_FILE_MANAGER_H__
#define GGADGET_GTK_GLOBAL_FILE_MANAGER_H__

#include <cstddef>
#include <string>
#include <third_party/unzip/unzip.h>
#include <ggadget/common.h>
#include <ggadget/file_manager_interface.h>

namespace ggadget {

/**
 * Handles all global file resources (i.e. resources not belonging to a 
 * gadget package).
 */
class GlobalFileManager : public ggadget::FileManagerInterface {
 public:
  GlobalFileManager();
  ~GlobalFileManager();

  /** @see FileManagerInterface::Init() */
  virtual bool Init(const char *base_path);
  /** @see FileManagerInterface::GetFileContents() */
  virtual bool GetFileContents(const char *file,
                               std::string *data,
                               std::string *path);
  /** @see FileManagerInterface::GetXMLFileContents() */
  virtual bool GetXMLFileContents(const char *file,
                                  std::string *data,
                                  std::string *path);
  /** @see FileManagerInterface::ExtractFile() */
  virtual bool ExtractFile(const char *file, std::string *into_file);
  /** @see FileManagerInterface::GetStringTable() */
  virtual GadgetStringMap *GetStringTable();
  /** @see FileManagerInterface::FileExists() */
  virtual bool FileExists(const char *file);

 private:
  std::string res_zip_path_;
  std::string locale_prefix_;
  std::string locale_lang_prefix_;

  bool InitLocaleStrings();
  bool GetZipFileContents(const char *file, 
                          std::string *data, std::string *path);
  bool SeekToFile(unzFile zip, const char *file, std::string *path);

  DISALLOW_EVIL_CONSTRUCTORS(GlobalFileManager);
};

} // namespace ggadget

#endif  // GGADGET_GTK_GLOBAL_FILE_MANAGER_H__
