#ifndef FLASH_H
#define FLASH_H

#include "lfs.h"

#include <stdint.h>

/// A lightweight LittleFS wrapper. We mainly deal with NAND flashes, though Nor
/// flashes are possible as well. You can find differences between the two
/// online but generally NOR is better for reads while NAND is better for
/// writes. There are also differences regarding power efficiency but that is
/// not too relevant for us.
///
/// When constructing a class which subclasses from this, you must fill out the
/// lfs_config instance. Then You must implement the `init` method, in which you
/// must startup actions such as
///
///   * checking for the id of the device
///   * unlocking the device for writes (if needed)
///   * unlocking the device for features like QSPI (if needed)
///
/// Then you must call `mount` to mount the filesystem. Only after that may you
/// use other functions like `bootcount` or `append`. It is a good idea to call
/// `unmount` when you no longer need to write the the flash.
namespace Common {
class Flash {
protected:
    struct lfs_config config;
    lfs_t lfs; // Don't touch this, this is managed by lfs itself

public:
    virtual ~Flash() = default;

    /// @brief initialize the flash
    /// @return true if success, false if error
    virtual bool init() = 0;

    /// @brief mounts the filesystem
    ///
    /// Must be called after `init` and before you use any of the other
    /// functions. If the size is negative then it is a good indicator that
    /// something is wrong.
    ///
    /// @return the size of the filesystem
    uint32_t mount();

    /// @brief unmounts the filesystem
    ///
    /// Closes all files and releases all resources.
    ///
    /// @return lfs status code
    uint32_t unmount();

    /// @brief gets or increments the bootcount
    ///
    /// @param update   if true, increments the bootcount by one
    /// @return lfs status code
    uint32_t bootcount(bool update);

    /// @brief opens a file, creating it if it doesn't exist
    /// @param file       the lfs file to open/create
    /// @param file_name  the name of the file to open/create
    /// @return lfs status code
    uint32_t open(lfs_file_t* file, const char* filename);

    /// @brief closes the given file
    /// @param file   the opened lfs file to close
    /// @return lfs status code
    uint32_t close(lfs_file_t* file);

    /// @brief appends bytes to the given file (must be open)
    /// @param file   an opened lfs file
    /// @param bytes  buffer of bytes to append to the file
    /// @param size   size of the buffer
    /// @return true on success, false on error
    bool append(lfs_file_t* file, const uint8_t* bytes, size_t size);
};
} // namespace Common

#endif
