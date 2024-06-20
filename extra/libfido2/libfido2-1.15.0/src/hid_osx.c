/*
 * Copyright (c) 2019-2023 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <Availability.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDManager.h>

#include "fido.h"

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 120000
#define kIOMainPortDefault kIOMasterPortDefault
#endif

#define IOREG "ioreg://"

struct hid_osx {
	IOHIDDeviceRef	ref;
	CFStringRef	loop_id;
	int		report_pipe[2];
	size_t		report_in_len;
	size_t		report_out_len;
	unsigned char	report[CTAP_MAX_REPORT_LEN];
};

static int
get_int32(IOHIDDeviceRef dev, CFStringRef key, int32_t *v)
{
	CFTypeRef ref;

	if ((ref = IOHIDDeviceGetProperty(dev, key)) == NULL ||
	    CFGetTypeID(ref) != CFNumberGetTypeID()) {
		fido_log_debug("%s: IOHIDDeviceGetProperty", __func__);
		return (-1);
	}

	if (CFNumberGetType(ref) != kCFNumberSInt32Type &&
	    CFNumberGetType(ref) != kCFNumberSInt64Type) {
		fido_log_debug("%s: CFNumberGetType", __func__);
		return (-1);
	}

	if (CFNumberGetValue(ref, kCFNumberSInt32Type, v) == false) {
		fido_log_debug("%s: CFNumberGetValue", __func__);
		return (-1);
	}

	return (0);
}

static int
get_utf8(IOHIDDeviceRef dev, CFStringRef key, void *buf, size_t len)
{
	CFTypeRef ref;

	memset(buf, 0, len);

	if ((ref = IOHIDDeviceGetProperty(dev, key)) == NULL ||
	    CFGetTypeID(ref) != CFStringGetTypeID()) {
		fido_log_debug("%s: IOHIDDeviceGetProperty", __func__);
		return (-1);
	}

	if (CFStringGetCString(ref, buf, (long)len,
	    kCFStringEncodingUTF8) == false) {
		fido_log_debug("%s: CFStringGetCString", __func__);
		return (-1);
	}

	return (0);
}

static int
get_report_len(IOHIDDeviceRef dev, int dir, size_t *report_len)
{
	CFStringRef	key;
	int32_t		v;

	if (dir == 0)
		key = CFSTR(kIOHIDMaxInputReportSizeKey);
	else
		key = CFSTR(kIOHIDMaxOutputReportSizeKey);

	if (get_int32(dev, key, &v) < 0) {
		fido_log_debug("%s: get_int32/%d", __func__, dir);
		return (-1);
	}

	if ((*report_len = (size_t)v) > CTAP_MAX_REPORT_LEN) {
		fido_log_debug("%s: report_len=%zu", __func__, *report_len);
		return (-1);
	}

	return (0);
}

static int
get_id(IOHIDDeviceRef dev, int16_t *vendor_id, int16_t *product_id)
{
	int32_t	vendor;
	int32_t product;

	if (get_int32(dev, CFSTR(kIOHIDVendorIDKey), &vendor) < 0 ||
	    vendor > UINT16_MAX) {
		fido_log_debug("%s: get_int32 vendor", __func__);
		return (-1);
	}

	if (get_int32(dev, CFSTR(kIOHIDProductIDKey), &product) < 0 ||
	    product > UINT16_MAX) {
		fido_log_debug("%s: get_int32 product", __func__);
		return (-1);
	}

	*vendor_id = (int16_t)vendor;
	*product_id = (int16_t)product;

	return (0);
}

static int
get_str(IOHIDDeviceRef dev, char **manufacturer, char **product)
{
	char	buf[512];
	int	ok = -1;

	*manufacturer = NULL;
	*product = NULL;

	if (get_utf8(dev, CFSTR(kIOHIDManufacturerKey), buf, sizeof(buf)) < 0)
		*manufacturer = strdup("");
	else
		*manufacturer = strdup(buf);

	if (get_utf8(dev, CFSTR(kIOHIDProductKey), buf, sizeof(buf)) < 0)
		*product = strdup("");
	else
		*product = strdup(buf);

	if (*manufacturer == NULL || *product == NULL) {
		fido_log_debug("%s: strdup", __func__);
		goto fail;
	}

	ok = 0;
fail:
	if (ok < 0) {
		free(*manufacturer);
		free(*product);
		*manufacturer = NULL;
		*product = NULL;
	}

	return (ok);
}

static char *
get_path(IOHIDDeviceRef dev)
{
	io_service_t	 s;
	uint64_t	 id;
	char		*path;

	if ((s = IOHIDDeviceGetService(dev)) == MACH_PORT_NULL) {
		fido_log_debug("%s: IOHIDDeviceGetService", __func__);
		return (NULL);
	}

	if (IORegistryEntryGetRegistryEntryID(s, &id) != KERN_SUCCESS) {
		fido_log_debug("%s: IORegistryEntryGetRegistryEntryID",
		    __func__);
		return (NULL);
	}

	if (asprintf(&path, "%s%llu", IOREG, (unsigned long long)id) == -1) {
		fido_log_error(errno, "%s: asprintf", __func__);
		return (NULL);
	}

	return (path);
}

static bool
is_fido(IOHIDDeviceRef dev)
{
	char		buf[32];
	uint32_t	usage_page;

	if (get_int32(dev, CFSTR(kIOHIDPrimaryUsagePageKey),
	    (int32_t *)&usage_page) < 0 || usage_page != 0xf1d0)
		return (false);

	if (get_utf8(dev, CFSTR(kIOHIDTransportKey), buf, sizeof(buf)) < 0) {
		fido_log_debug("%s: get_utf8 transport", __func__);
		return (false);
	}

#ifndef FIDO_HID_ANY
	if (strcasecmp(buf, "usb") != 0) {
		fido_log_debug("%s: transport", __func__);
		return (false);
	}
#endif

	return (true);
}

static int
copy_info(fido_dev_info_t *di, IOHIDDeviceRef dev)
{
	memset(di, 0, sizeof(*di));

	if (is_fido(dev) == false)
		return (-1);

	if (get_id(dev, &di->vendor_id, &di->product_id) < 0 ||
	    get_str(dev, &di->manufacturer, &di->product) < 0 ||
	    (di->path = get_path(dev)) == NULL) {
		free(di->path);
		free(di->manufacturer);
		free(di->product);
		explicit_bzero(di, sizeof(*di));
		return (-1);
	}

	return (0);
}

int
fido_hid_manifest(fido_dev_info_t *devlist, size_t ilen, size_t *olen)
{
	IOHIDManagerRef	 manager = NULL;
	CFSetRef	 devset = NULL;
	size_t		 devcnt;
	CFIndex		 n;
	IOHIDDeviceRef	*devs = NULL;
	int		 r = FIDO_ERR_INTERNAL;

	*olen = 0;

	if (ilen == 0)
		return (FIDO_OK); /* nothing to do */

	if (devlist == NULL)
		return (FIDO_ERR_INVALID_ARGUMENT);

	if ((manager = IOHIDManagerCreate(kCFAllocatorDefault,
	    kIOHIDManagerOptionNone)) == NULL) {
		fido_log_debug("%s: IOHIDManagerCreate", __func__);
		goto fail;
	}

	IOHIDManagerSetDeviceMatching(manager, NULL);

	if ((devset = IOHIDManagerCopyDevices(manager)) == NULL) {
		fido_log_debug("%s: IOHIDManagerCopyDevices", __func__);
		goto fail;
	}

	if ((n = CFSetGetCount(devset)) < 0) {
		fido_log_debug("%s: CFSetGetCount", __func__);
		goto fail;
	}

	devcnt = (size_t)n;

	if ((devs = calloc(devcnt, sizeof(*devs))) == NULL) {
		fido_log_debug("%s: calloc", __func__);
		goto fail;
	}

	CFSetGetValues(devset, (void *)devs);

	for (size_t i = 0; i < devcnt; i++) {
		if (copy_info(&devlist[*olen], devs[i]) == 0) {
			devlist[*olen].io = (fido_dev_io_t) {
				fido_hid_open,
				fido_hid_close,
				fido_hid_read,
				fido_hid_write,
			};
			if (++(*olen) == ilen)
				break;
		}
	}

	r = FIDO_OK;
fail:
	if (manager != NULL)
		CFRelease(manager);
	if (devset != NULL)
		CFRelease(devset);

	free(devs);

	return (r);
}

static void
report_callback(void *context, IOReturn result, void *dev, IOHIDReportType type,
    uint32_t id, uint8_t *ptr, CFIndex len)
{
	struct hid_osx	*ctx = context;
	ssize_t		 r;

	(void)dev;

	if (result != kIOReturnSuccess || type != kIOHIDReportTypeInput ||
	    id != 0 || len < 0 || (size_t)len != ctx->report_in_len) {
		fido_log_debug("%s: io error", __func__);
		return;
	}

	if ((r = write(ctx->report_pipe[1], ptr, (size_t)len)) == -1) {
		fido_log_error(errno, "%s: write", __func__);
		return;
	}

	if (r < 0 || (size_t)r != (size_t)len) {
		fido_log_debug("%s: %zd != %zu", __func__, r, (size_t)len);
		return;
	}
}

static void
removal_callback(void *context, IOReturn result, void *sender)
{
	(void)context;
	(void)result;
	(void)sender;

	CFRunLoopStop(CFRunLoopGetCurrent());
}

static int
set_nonblock(int fd)
{
	int flags;

	if ((flags = fcntl(fd, F_GETFL)) == -1) {
		fido_log_error(errno, "%s: fcntl F_GETFL", __func__);
		return (-1);
	}

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		fido_log_error(errno, "%s: fcntl F_SETFL", __func__);
		return (-1);
	}

	return (0);
}

static int
disable_sigpipe(int fd)
{
	int disabled = 1;

	if (fcntl(fd, F_SETNOSIGPIPE, &disabled) == -1) {
		fido_log_error(errno, "%s: fcntl F_SETNOSIGPIPE", __func__);
		return (-1);
	}

	return (0);
}

static io_registry_entry_t
get_ioreg_entry(const char *path)
{
	uint64_t id;

	if (strncmp(path, IOREG, strlen(IOREG)) != 0)
		return (IORegistryEntryFromPath(kIOMainPortDefault, path));

	if (fido_to_uint64(path + strlen(IOREG), 10, &id) == -1) {
		fido_log_debug("%s: fido_to_uint64", __func__);
		return (MACH_PORT_NULL);
	}

	return (IOServiceGetMatchingService(kIOMainPortDefault,
	    IORegistryEntryIDMatching(id)));
}

void *
fido_hid_open(const char *path)
{
	struct hid_osx		*ctx;
	io_registry_entry_t	 entry = MACH_PORT_NULL;
	char			 loop_id[32];
	int			 ok = -1;
	int			 r;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL) {
		fido_log_debug("%s: calloc", __func__);
		goto fail;
	}

	ctx->report_pipe[0] = -1;
	ctx->report_pipe[1] = -1;

	if (pipe(ctx->report_pipe) == -1) {
		fido_log_error(errno, "%s: pipe", __func__);
		goto fail;
	}

	if (set_nonblock(ctx->report_pipe[0]) < 0 ||
	    set_nonblock(ctx->report_pipe[1]) < 0) {
		fido_log_debug("%s: set_nonblock", __func__);
		goto fail;
	}

	if (disable_sigpipe(ctx->report_pipe[1]) < 0) {
		fido_log_debug("%s: disable_sigpipe", __func__);
		goto fail;
	}

	if ((entry = get_ioreg_entry(path)) == MACH_PORT_NULL) {
		fido_log_debug("%s: get_ioreg_entry: %s", __func__, path);
		goto fail;
	}

	if ((ctx->ref = IOHIDDeviceCreate(kCFAllocatorDefault,
	    entry)) == NULL) {
		fido_log_debug("%s: IOHIDDeviceCreate", __func__);
		goto fail;
	}

	if (get_report_len(ctx->ref, 0, &ctx->report_in_len) < 0 ||
	    get_report_len(ctx->ref, 1, &ctx->report_out_len) < 0) {
		fido_log_debug("%s: get_report_len", __func__);
		goto fail;
	}

	if (ctx->report_in_len > sizeof(ctx->report)) {
		fido_log_debug("%s: report_in_len=%zu", __func__,
		    ctx->report_in_len);
		goto fail;
	}

	if (IOHIDDeviceOpen(ctx->ref,
	    kIOHIDOptionsTypeSeizeDevice) != kIOReturnSuccess) {
		fido_log_debug("%s: IOHIDDeviceOpen", __func__);
		goto fail;
	}

	if ((r = snprintf(loop_id, sizeof(loop_id), "fido2-%p",
	    (void *)ctx->ref)) < 0 || (size_t)r >= sizeof(loop_id)) {
		fido_log_debug("%s: snprintf", __func__);
		goto fail;
	}

	if ((ctx->loop_id = CFStringCreateWithCString(NULL, loop_id,
	    kCFStringEncodingASCII)) == NULL) {
		fido_log_debug("%s: CFStringCreateWithCString", __func__);
		goto fail;
	}

	IOHIDDeviceRegisterInputReportCallback(ctx->ref, ctx->report,
	    (long)ctx->report_in_len, &report_callback, ctx);
	IOHIDDeviceRegisterRemovalCallback(ctx->ref, &removal_callback, ctx);

	ok = 0;
fail:
	if (entry != MACH_PORT_NULL)
		IOObjectRelease(entry);

	if (ok < 0 && ctx != NULL) {
		if (ctx->ref != NULL)
			CFRelease(ctx->ref);
		if (ctx->loop_id != NULL)
			CFRelease(ctx->loop_id);
		if (ctx->report_pipe[0] != -1)
			close(ctx->report_pipe[0]);
		if (ctx->report_pipe[1] != -1)
			close(ctx->report_pipe[1]);
		free(ctx);
		ctx = NULL;
	}

	return (ctx);
}

void
fido_hid_close(void *handle)
{
	struct hid_osx *ctx = handle;

	IOHIDDeviceRegisterInputReportCallback(ctx->ref, ctx->report,
	    (long)ctx->report_in_len, NULL, ctx);
	IOHIDDeviceRegisterRemovalCallback(ctx->ref, NULL, ctx);

	if (IOHIDDeviceClose(ctx->ref,
	    kIOHIDOptionsTypeSeizeDevice) != kIOReturnSuccess)
		fido_log_debug("%s: IOHIDDeviceClose", __func__);

	CFRelease(ctx->ref);
	CFRelease(ctx->loop_id);

	explicit_bzero(ctx->report, sizeof(ctx->report));
	close(ctx->report_pipe[0]);
	close(ctx->report_pipe[1]);

	free(ctx);
}

int
fido_hid_set_sigmask(void *handle, const fido_sigset_t *sigmask)
{
	(void)handle;
	(void)sigmask;

	return (FIDO_ERR_INTERNAL);
}

static void
schedule_io_loop(struct hid_osx *ctx, int ms)
{
	IOHIDDeviceScheduleWithRunLoop(ctx->ref, CFRunLoopGetCurrent(),
	    ctx->loop_id);

	if (ms == -1)
		ms = 5000; /* wait 5 seconds by default */

	CFRunLoopRunInMode(ctx->loop_id, (double)ms/1000.0, true);

	IOHIDDeviceUnscheduleFromRunLoop(ctx->ref, CFRunLoopGetCurrent(),
	    ctx->loop_id);
}

int
fido_hid_read(void *handle, unsigned char *buf, size_t len, int ms)
{
	struct hid_osx		*ctx = handle;
	ssize_t			 r;

	explicit_bzero(buf, len);
	explicit_bzero(ctx->report, sizeof(ctx->report));

	if (len != ctx->report_in_len || len > sizeof(ctx->report)) {
		fido_log_debug("%s: len %zu", __func__, len);
		return (-1);
	}

	/* check for pending frame  */
	if ((r = read(ctx->report_pipe[0], buf, len)) == -1) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			fido_log_error(errno, "%s: read", __func__);
			return (-1);
		}

		schedule_io_loop(ctx, ms);

		if ((r = read(ctx->report_pipe[0], buf, len)) == -1) {
			fido_log_error(errno, "%s: read", __func__);
			return (-1);
		}
	}

	if (r < 0 || (size_t)r != len) {
		fido_log_debug("%s: %zd != %zu", __func__, r, len);
		return (-1);
	}

	return ((int)len);
}

int
fido_hid_write(void *handle, const unsigned char *buf, size_t len)
{
	struct hid_osx *ctx = handle;

	if (len != ctx->report_out_len + 1 || len > LONG_MAX) {
		fido_log_debug("%s: len %zu", __func__, len);
		return (-1);
	}

	if (IOHIDDeviceSetReport(ctx->ref, kIOHIDReportTypeOutput, 0, buf + 1,
	    (long)(len - 1)) != kIOReturnSuccess) {
		fido_log_debug("%s: IOHIDDeviceSetReport", __func__);
		return (-1);
	}

	return ((int)len);
}

size_t
fido_hid_report_in_len(void *handle)
{
	struct hid_osx *ctx = handle;

	return (ctx->report_in_len);
}

size_t
fido_hid_report_out_len(void *handle)
{
	struct hid_osx *ctx = handle;

	return (ctx->report_out_len);
}
