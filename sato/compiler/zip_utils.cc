
#include "sato/compiler/zip_utils.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_format.h"

namespace sato {

absl::Status AddFileToZip(zip_t *zip, const std::string &full_message_name,
                  const std::string &content) {
  // Allocate buffer on heap and copy content so libzip can take ownership
  // This ensures the data persists until libzip writes it to the zip file
  zip_uint8_t *buffer = nullptr;
  zip_uint64_t buffer_size = content.size();
  if (buffer_size > 0) {
    buffer = static_cast<zip_uint8_t *>(malloc(buffer_size));
    if (buffer == nullptr) {
      return absl::InternalError("Failed to allocate buffer for zip source");
    }
    std::memcpy(buffer, content.data(), buffer_size);
  } 

    // Add the contents to the zip.
  // freep=1 means libzip will free the buffer when done
  zip_source_t *source = zip_source_buffer(zip, buffer, buffer_size, 1);
  if (source == nullptr) {
    return absl::InternalError(absl::StrFormat("Failed to create zip source: %s", zip_strerror(zip))); 
  }

  size_t pos = full_message_name.rfind(".");
  std::string base_name = full_message_name.substr(pos + 1);
  std::string dirname = full_message_name.substr(0, pos);
  // Replace dot with underscore in dirname
  dirname = absl::StrReplaceAll(dirname, {{".", "_"}});
  std::string filename = dirname + "/msg/" + base_name + ".msg";
  zip_int64_t index =
      zip_file_add(zip, filename.c_str(), source, ZIP_FL_ENC_UTF_8);
  if (index < 0) {
    zip_source_free(source);
    return absl::InternalError(absl::StrFormat("Failed to add file %s to zip: %s", filename, zip_strerror(zip)));
  }
  return absl::OkStatus();
}

} // namespace sato