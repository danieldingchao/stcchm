
#include "chrome/browser/ui/lemon/lemon_mid.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"

#include <fcntl.h>
#include <linux/hdreg.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_thread.h"

// copyed from device_id_linux
#if defined(OS_LINUX)
const char kDiskByUuidDirectoryName[] = "/dev/disk/by-uuid";
const char* const kDeviceNames[] = {
  "sda1", "hda1", "dm-0", "xvda1", "sda2", "hda2", "dm-1", "xvda2",
};
typedef std::map<base::FilePath, base::FilePath> DiskEntries;
std::string GetDiskUuid() {
  base::ThreadRestrictions::AssertIOAllowed();

  DiskEntries disk_uuids;
  base::FileEnumerator files(base::FilePath(kDiskByUuidDirectoryName),
                             false,  // Recursive.
                             base::FileEnumerator::FILES);
  do {
    base::FilePath file_path = files.Next();
    if (file_path.empty())
      break;

    base::FilePath target_path;
    if (!base::ReadSymbolicLink(file_path, &target_path))
      continue;

    base::FilePath device_name = target_path.BaseName();
    base::FilePath disk_uuid = file_path.BaseName();
    disk_uuids[device_name] = disk_uuid;
  } while (true);

  // Look for first device name matching an entry of |kDeviceNames|.
  std::string result;
  for (size_t i = 0; i < arraysize(kDeviceNames); i++) {
    DiskEntries::iterator it =
        disk_uuids.find(base::FilePath(kDeviceNames[i]));
    if (it != disk_uuids.end()) {
      DVLOG(1) << "Returning uuid: \"" << it->second.value()
               << "\" for device \"" << it->first.value() << "\"";
      result = it->second.value();
      break;
    }
  }

  // Log failure (at most once) for diagnostic purposes.
  static bool error_logged = false;
  if (result.empty() && !error_logged) {
    error_logged = true;
    LOG(ERROR) << "Could not find appropriate disk uuid.";
    for (DiskEntries::iterator it = disk_uuids.begin();
        it != disk_uuids.end(); ++it) {
      LOG(ERROR) << "  DeviceID=" << it->first.value() << ", uuid="
                 << it->second.value();
    }
  }

  return result;
}
#elif defined(OS_MAC)

const char kRootDirectory[] = "/";

typedef base::Callback<bool(const void* bytes, size_t size)>
    IsValidMacAddressCallback;

// Return the BSD name (e.g. '/dev/disk1') of the root directory by enumerating
// through the mounted volumes .
// Return "" if an error occured.
std::string FindBSDNameOfSystemDisk() {
  struct statfs* mounted_volumes;
  int num_volumes = getmntinfo(&mounted_volumes, 0);
  if (num_volumes == 0) {
    VLOG(1) << "Cannot enumerate list of mounted volumes.";
    return std::string();
  }

  for (int i = 0; i < num_volumes; i++) {
    struct statfs* vol = &mounted_volumes[i];
    if (std::string(vol->f_mntonname) == kRootDirectory) {
      return std::string(vol->f_mntfromname);
    }
  }

  VLOG(1) << "Cannot find disk mounted as '" << kRootDirectory << "'.";
  return std::string();
}

// Return the Volume UUID property of a BSD disk name (e.g. '/dev/disk1').
// Return "" if an error occured.
std::string GetVolumeUUIDFromBSDName(const std::string& bsd_name) {
  const CFAllocatorRef allocator = NULL;

  base::ScopedCFTypeRef<DASessionRef> session(DASessionCreate(allocator));
  if (session.get() == NULL) {
    VLOG(1) << "Error creating DA Session.";
    return std::string();
  }

  base::ScopedCFTypeRef<DADiskRef> disk(
      DADiskCreateFromBSDName(allocator, session, bsd_name.c_str()));
  if (disk.get() == NULL) {
    VLOG(1) << "Error creating DA disk from BSD disk name.";
    return std::string();
  }

  base::ScopedCFTypeRef<CFDictionaryRef> disk_description(
      DADiskCopyDescription(disk));
  if (disk_description.get() == NULL) {
    VLOG(1) << "Error getting disk description.";
    return std::string();
  }

  CFUUIDRef volume_uuid = base::mac::GetValueFromDictionary<CFUUIDRef>(
      disk_description,
      kDADiskDescriptionVolumeUUIDKey);
  if (volume_uuid == NULL) {
    VLOG(1) << "Error getting volume UUID of disk.";
    return std::string();
  }

  base::ScopedCFTypeRef<CFStringRef> volume_uuid_string(
      CFUUIDCreateString(allocator, volume_uuid));
  if (volume_uuid_string.get() == NULL) {
    VLOG(1) << "Error creating string from CSStringRef.";
    return std::string();
  }

  return base::SysCFStringRefToUTF8(volume_uuid_string.get());
}

// Return Volume UUID property of disk mounted as "/".
std::string GetDiskUuid() {
  base::ThreadRestrictions::AssertIOAllowed();

  std::string result;
  std::string bsd_name = FindBSDNameOfSystemDisk();
  if (!bsd_name.empty()) {
    VLOG(4) << "BSD name of root directory: '" << bsd_name << "'";
    result = GetVolumeUUIDFromBSDName(bsd_name);
  }
  return result;
}
#endif
