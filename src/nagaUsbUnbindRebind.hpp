#pragma once
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <unistd.h>

// Helper: resolve symlink to real device file
inline std::string resolve_symlink(const std::string& path) {
    char buf[PATH_MAX] = {0};
    ssize_t len = readlink(path.c_str(), buf, sizeof(buf)-1);
    if (len > 0) {
        buf[len] = '\0';
        std::string resolved = buf;
        if (resolved[0] != '/') {
            size_t last_slash = path.find_last_of('/');
            if (last_slash != std::string::npos)
                resolved = path.substr(0, last_slash+1) + resolved;
        }
        return resolved;
    }
    return path;
}

// Helper: get sysfs device path for /dev/input/eventX
inline std::string get_sysfs_path(const std::string& devfile) {
    struct stat st;
    if (stat(devfile.c_str(), &st) != 0) return "";
    char sysfs_path[PATH_MAX];
    snprintf(sysfs_path, sizeof(sysfs_path), "/sys/dev/char/%u:%u/device", major(st.st_rdev), minor(st.st_rdev));
    struct stat sysst;
    if (stat(sysfs_path, &sysst) == 0) return sysfs_path;
    return "";
}

// Clean sysfs USB unbind/rebind helper using device file path
inline bool usb_unbind_rebind(const std::string& device_path) {
    std::clog << "[naga] usb_unbind_rebind: device_path=" << device_path << std::endl;
    std::string real_path = resolve_symlink(device_path);
    std::clog << "[naga] usb_unbind_rebind: real_path=" << real_path << std::endl;
    std::string sysfs_link_path = get_sysfs_path(real_path);
    std::clog << "[naga] usb_unbind_rebind: sysfs_link_path=" << sysfs_link_path << std::endl;
    if (sysfs_link_path.empty()) {
        std::cerr << "[naga] ERROR: Could not resolve sysfs path for " << device_path << std::endl;
        return false;
    }
    char resolved_device_path[PATH_MAX] = {0};
    if (realpath(sysfs_link_path.c_str(), resolved_device_path) == nullptr) {
        std::cerr << "[naga] ERROR: Could not resolve real device path for " << sysfs_link_path << std::endl;
        return false;
    }
    std::string usb_sysfs_path = resolved_device_path;
    std::clog << "[naga] usb_unbind_rebind: starting walk from resolved device path=" << usb_sysfs_path << std::endl;
    std::string interface_id;
    DIR* dir = opendir("/sys/bus/usb/drivers/usbhid/");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name(entry->d_name);
            if (name.find(":") != std::string::npos && name.find("-") != std::string::npos) {
                std::string link_path = std::string("/sys/bus/usb/drivers/usbhid/") + name;
                char resolved_link[PATH_MAX] = {0};
                if (realpath(link_path.c_str(), resolved_link)) {
                    std::string resolved_str = resolved_link;
                    std::clog << "[naga] usb_unbind_rebind: candidate symlink=" << name << " resolved_path=" << resolved_str << std::endl;
                    if (usb_sysfs_path.find(resolved_str) == 0) {
                        std::clog << "[naga] usb_unbind_rebind: MATCH symlink=" << name << " resolved_path=" << resolved_str << " is prefix of usb_sysfs_path=" << usb_sysfs_path << std::endl;
                        interface_id = name;
                        break;
                    } else {
                        std::clog << "[naga] usb_unbind_rebind: NO MATCH symlink=" << name << " resolved_path=" << resolved_str << " is not prefix of usb_sysfs_path=" << usb_sysfs_path << std::endl;
                    }
                } else {
                    std::clog << "[naga] usb_unbind_rebind: candidate symlink=" << name << " could not resolve realpath" << std::endl;
                }
            }
        }
        closedir(dir);
    }
    if (interface_id.empty()) {
        std::cerr << "[naga] ERROR: Could not find correct interface ID for device " << device_path << std::endl;
        return false;
    }
    std::clog << "[naga] usb_unbind_rebind: using interface_id=" << interface_id << std::endl;
    std::string rebind_cmd = "sudo /usr/local/bin/usb_rebind_root.sh '" + interface_id + "'";
    std::clog << "[naga] usb_unbind_rebind: running " << rebind_cmd << std::endl;
    int ret = system(rebind_cmd.c_str());
    if (ret != 0) {
        std::cerr << "[naga] ERROR: Could not sudo rebind interface_id: " << interface_id << " (cmd: " << rebind_cmd << ")" << std::endl;
        return false;
    }
    std::clog << "[naga] Rebind succeeded for interface_id: " << interface_id << std::endl;
    return true;
}
