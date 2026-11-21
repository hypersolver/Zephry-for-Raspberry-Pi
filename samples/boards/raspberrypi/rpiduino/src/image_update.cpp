#include <rpiduino/image_update.hpp>

#include <errno.h>
#include <ff.h>
#include <zephyr/fs/fs.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/printk.h>

namespace {
constexpr char mountPoint[] = "/SD:";
constexpr char imageTemp[] = "/SD:/RPITMP.IMG";
constexpr char selectorTemp[] = "/SD:/RPINEXT.TMP";
constexpr char selector[] = "/SD:/RPINEXT.TXT";
FATFS fat;
fs_mount_t sdMount = {
    .type = FS_FATFS,
    .mnt_point = mountPoint,
    .fs_data = &fat,
    .flags = FS_MOUNT_FLAG_NO_FORMAT | FS_MOUNT_FLAG_USE_DISK_ACCESS,
};
bool mounted;

int mount()
{
    if (mounted) {
        return 0;
    }
    const int result = fs_mount(&sdMount);
    mounted = result == 0;
    return result;
}

int verifyImage(const char *path, size_t size, uint32_t checksum)
{
    fs_file_t file;
    fs_file_t_init(&file);
    int result = fs_open(&file, path, FS_O_READ);
    if (result < 0) {
        return result;
    }
    uint8_t buffer[512];
    uint32_t crc = 0;
    size_t total = 0;
    while (true) {
        const ssize_t count = fs_read(&file, buffer, sizeof(buffer));
        if (count <= 0) {
            result = static_cast<int>(count);
            break;
        }
        total += static_cast<size_t>(count);
        if (total > size) {
            result = -EBADMSG;
            break;
        }
        crc = crc32_ieee_update(crc, buffer, static_cast<size_t>(count));
    }
    const int closed = fs_close(&file);
    if (result < 0) {
        return result;
    }
    if (closed < 0) {
        return closed;
    }
    return total == size && crc == checksum ? 0 : -EBADMSG;
}

int writeFile(const char *path, const void *bytes, size_t size)
{
    fs_file_t file;
    fs_file_t_init(&file);
    int result = fs_open(&file, path, FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
    if (result < 0) {
        return result;
    }
    const ssize_t count = fs_write(&file, bytes, size);
    result = count < 0 ? static_cast<int>(count)
                       : static_cast<size_t>(count) == size ? fs_sync(&file) : -ENOSPC;
    const int closed = fs_close(&file);
    return result < 0 ? result : closed;
}

void imagePath(char (&path)[18], uint32_t crc)
{
    snprintk(path, sizeof(path), "%s/%08X.IMG", mountPoint, static_cast<unsigned int>(crc));
}
}

int saveImage(const uint8_t *image, size_t size, uint32_t crc)
{
    int result = mount();
    if (result < 0) {
        return result;
    }
    char path[18];
    imagePath(path, crc);
    result = verifyImage(path, size, crc);
    if (result != -ENOENT) {
        return result;
    }
    result = writeFile(imageTemp, image, size);
    if (result == 0) {
        result = verifyImage(imageTemp, size, crc);
    }
    if (result == 0) {
        result = fs_rename(imageTemp, path);
    }
    if (result < 0) {
        fs_unlink(imageTemp);
    }
    return result;
}

int selectImage(size_t size, uint32_t crc)
{
    char path[18];
    imagePath(path, crc);
    int result = mount();
    if (result == 0) {
        result = verifyImage(path, size, crc);
    }
    if (result < 0) {
        return result;
    }
    char config[32];
    const int length = snprintk(config, sizeof(config), "kernel=%08X.IMG\n",
                                static_cast<unsigned int>(crc));
    result = writeFile(selectorTemp, config, static_cast<size_t>(length));
    if (result == 0) {
        result = fs_rename(selectorTemp, selector);
    }
    if (result < 0) {
        fs_unlink(selectorTemp);
    }
    return result;
}

int clearImageSelection()
{
    const int mountedResult = mount();
    if (mountedResult < 0) {
        return mountedResult;
    }
    const int result = fs_unlink(selector);
    return result == -ENOENT ? 0 : result;
}
