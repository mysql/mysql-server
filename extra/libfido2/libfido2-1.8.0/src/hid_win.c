/*
 * Copyright (c) 2019 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <sys/types.h>

#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <devpkey.h>
#include <devpropdef.h>
#include <hidclass.h>
#include <hidsdi.h>
#include <wchar.h>

#include "fido.h"

#if defined(__MINGW32__) &&  __MINGW64_VERSION_MAJOR < 6
WINSETUPAPI WINBOOL WINAPI SetupDiGetDevicePropertyW(HDEVINFO,
    PSP_DEVINFO_DATA, const DEVPROPKEY *, DEVPROPTYPE *, PBYTE,
    DWORD, PDWORD, DWORD);
#endif

#if defined(__MINGW32__)
DEFINE_DEVPROPKEY(DEVPKEY_Device_Parent, 0x4340a6c5, 0x93fa, 0x4706, 0x97,
    0x2c, 0x7b, 0x64, 0x80, 0x08, 0xa5, 0xa7, 8);
#endif

struct hid_win {
	HANDLE		dev;
	OVERLAPPED	overlap;
	int		report_pending;
	size_t		report_in_len;
	size_t		report_out_len;
	unsigned char	report[1 + CTAP_MAX_REPORT_LEN];
};

static bool
is_fido(HANDLE dev)
{
	PHIDP_PREPARSED_DATA	data = NULL;
	HIDP_CAPS		caps;
	int			fido = 0;

	if (HidD_GetPreparsedData(dev, &data) == false) {
		fido_log_debug("%s: HidD_GetPreparsedData", __func__);
		goto fail;
	}

	if (HidP_GetCaps(data, &caps) != HIDP_STATUS_SUCCESS) {
		fido_log_debug("%s: HidP_GetCaps", __func__);
		goto fail;
	}

	fido = (uint16_t)caps.UsagePage == 0xf1d0;
fail:
	if (data != NULL)
		HidD_FreePreparsedData(data);

	return (fido);
}

static int
get_report_len(HANDLE dev, int dir, size_t *report_len)
{
	PHIDP_PREPARSED_DATA	data = NULL;
	HIDP_CAPS		caps;
	USHORT			v;
	int			ok = -1;

	if (HidD_GetPreparsedData(dev, &data) == false) {
		fido_log_debug("%s: HidD_GetPreparsedData/%d", __func__, dir);
		goto fail;
	}

	if (HidP_GetCaps(data, &caps) != HIDP_STATUS_SUCCESS) {
		fido_log_debug("%s: HidP_GetCaps/%d", __func__, dir);
		goto fail;
	}

	if (dir == 0)
		v = caps.InputReportByteLength;
	else
		v = caps.OutputReportByteLength;

	if ((*report_len = (size_t)v) == 0) {
		fido_log_debug("%s: report_len == 0", __func__);
		goto fail;
	}

	ok = 0;
fail:
	if (data != NULL)
		HidD_FreePreparsedData(data);

	return (ok);
}

static int
get_int(HANDLE dev, int16_t *vendor_id, int16_t *product_id)
{
	HIDD_ATTRIBUTES attr;

	attr.Size = sizeof(attr);

	if (HidD_GetAttributes(dev, &attr) == false) {
		fido_log_debug("%s: HidD_GetAttributes", __func__);
		return (-1);
	}

	*vendor_id = (int16_t)attr.VendorID;
	*product_id = (int16_t)attr.ProductID;

	return (0);
}

static int
get_str(HANDLE dev, char **manufacturer, char **product)
{
	wchar_t	buf[512];
	int	utf8_len;
	int	ok = -1;

	*manufacturer = NULL;
	*product = NULL;

	if (HidD_GetManufacturerString(dev, &buf, sizeof(buf)) == false) {
		fido_log_debug("%s: HidD_GetManufacturerString", __func__);
		goto fail;
	}

	if ((utf8_len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, buf,
	    -1, NULL, 0, NULL, NULL)) <= 0 || utf8_len > 128) {
		fido_log_debug("%s: WideCharToMultiByte", __func__);
		goto fail;
	}

	if ((*manufacturer = malloc((size_t)utf8_len)) == NULL) {
		fido_log_debug("%s: malloc", __func__);
		goto fail;
	}

	if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, buf, -1,
	    *manufacturer, utf8_len, NULL, NULL) != utf8_len) {
		fido_log_debug("%s: WideCharToMultiByte", __func__);
		goto fail;
	}

	if (HidD_GetProductString(dev, &buf, sizeof(buf)) == false) {
		fido_log_debug("%s: HidD_GetProductString", __func__);
		goto fail;
	}

	if ((utf8_len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, buf,
	    -1, NULL, 0, NULL, NULL)) <= 0 || utf8_len > 128) {
		fido_log_debug("%s: WideCharToMultiByte", __func__);
		goto fail;
	}

	if ((*product = malloc((size_t)utf8_len)) == NULL) {
		fido_log_debug("%s: malloc", __func__);
		goto fail;
	}

	if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, buf, -1,
	    *product, utf8_len, NULL, NULL) != utf8_len) {
		fido_log_debug("%s: WideCharToMultiByte", __func__);
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
get_path(HDEVINFO devinfo, SP_DEVICE_INTERFACE_DATA *ifdata)
{
	SP_DEVICE_INTERFACE_DETAIL_DATA_A	*ifdetail = NULL;
	char					*path = NULL;
	DWORD					 len = 0;

	/*
	 * "Get the required buffer size. Call SetupDiGetDeviceInterfaceDetail
	 * with a NULL DeviceInterfaceDetailData pointer, a
	 * DeviceInterfaceDetailDataSize of zero, and a valid RequiredSize
	 * variable. In response to such a call, this function returns the
	 * required buffer size at RequiredSize and fails with GetLastError
	 * returning ERROR_INSUFFICIENT_BUFFER."
	 */
	if (SetupDiGetDeviceInterfaceDetailA(devinfo, ifdata, NULL, 0, &len,
	    NULL) != false || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		fido_log_debug("%s: SetupDiGetDeviceInterfaceDetailA 1",
		    __func__);
		goto fail;
	}

	if ((ifdetail = malloc(len)) == NULL) {
		fido_log_debug("%s: malloc", __func__);
		goto fail;
	}

	ifdetail->cbSize = sizeof(*ifdetail);

	if (SetupDiGetDeviceInterfaceDetailA(devinfo, ifdata, ifdetail, len,
	    NULL, NULL) == false) {
		fido_log_debug("%s: SetupDiGetDeviceInterfaceDetailA 2",
		    __func__);
		goto fail;
	}

	if ((path = strdup(ifdetail->DevicePath)) == NULL) {
		fido_log_debug("%s: strdup", __func__);
		goto fail;
	}

fail:
	free(ifdetail);

	return (path);
}

#ifndef FIDO_HID_ANY
static bool
hid_ok(HDEVINFO devinfo, DWORD idx)
{
	SP_DEVINFO_DATA	 devinfo_data;
	wchar_t		*parent = NULL;
	DWORD		 parent_type = DEVPROP_TYPE_STRING;
	DWORD		 len = 0;
	bool		 ok = false;

	memset(&devinfo_data, 0, sizeof(devinfo_data));
	devinfo_data.cbSize = sizeof(devinfo_data);

	if (SetupDiEnumDeviceInfo(devinfo, idx, &devinfo_data) == false) {
		fido_log_debug("%s: SetupDiEnumDeviceInfo", __func__);
		goto fail;
	}

	if (SetupDiGetDevicePropertyW(devinfo, &devinfo_data,
	    &DEVPKEY_Device_Parent, &parent_type, NULL, 0, &len, 0) != false ||
	    GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		fido_log_debug("%s: SetupDiGetDevicePropertyW 1", __func__);
		goto fail;
	}

	if ((parent = malloc(len)) == NULL) {
		fido_log_debug("%s: malloc", __func__);
		goto fail;
	}

	if (SetupDiGetDevicePropertyW(devinfo, &devinfo_data,
	    &DEVPKEY_Device_Parent, &parent_type, (PBYTE)parent, len, NULL,
	    0) == false) {
		fido_log_debug("%s: SetupDiGetDevicePropertyW 2", __func__);
		goto fail;
	}

	ok = wcsncmp(parent, L"USB\\", 4) == 0;
fail:
	free(parent);

	return (ok);
}
#endif

static int
copy_info(fido_dev_info_t *di, HDEVINFO devinfo, DWORD idx,
    SP_DEVICE_INTERFACE_DATA *ifdata)
{
	HANDLE	dev = INVALID_HANDLE_VALUE;
	int	ok = -1;

	memset(di, 0, sizeof(*di));

	if ((di->path = get_path(devinfo, ifdata)) == NULL) {
		fido_log_debug("%s: get_path", __func__);
		goto fail;
	}

	fido_log_debug("%s: path=%s", __func__, di->path);

#ifndef FIDO_HID_ANY
	if (hid_ok(devinfo, idx) == false) {
		fido_log_debug("%s: hid_ok", __func__);
		goto fail;
	}
#endif

	dev = CreateFileA(di->path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
	    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (dev == INVALID_HANDLE_VALUE) {
		fido_log_debug("%s: CreateFileA", __func__);
		goto fail;
	}

	if (is_fido(dev) == false) {
		fido_log_debug("%s: is_fido", __func__);
		goto fail;
	}

	if (get_int(dev, &di->vendor_id, &di->product_id) < 0 ||
	    get_str(dev, &di->manufacturer, &di->product) < 0) {
		fido_log_debug("%s: get_int/get_str", __func__);
		goto fail;
	}

	ok = 0;
fail:
	if (dev != INVALID_HANDLE_VALUE)
		CloseHandle(dev);

	if (ok < 0) {
		free(di->path);
		free(di->manufacturer);
		free(di->product);
		explicit_bzero(di, sizeof(*di));
	}

	return (ok);
}

int
fido_hid_manifest(fido_dev_info_t *devlist, size_t ilen, size_t *olen)
{
	GUID				hid_guid = GUID_DEVINTERFACE_HID;
	HDEVINFO			devinfo = INVALID_HANDLE_VALUE;
	SP_DEVICE_INTERFACE_DATA	ifdata;
	DWORD				idx;
	int				r = FIDO_ERR_INTERNAL;

	*olen = 0;

	if (ilen == 0)
		return (FIDO_OK); /* nothing to do */
	if (devlist == NULL)
		return (FIDO_ERR_INVALID_ARGUMENT);

	if ((devinfo = SetupDiGetClassDevsA(&hid_guid, NULL, NULL,
	    DIGCF_DEVICEINTERFACE | DIGCF_PRESENT)) == INVALID_HANDLE_VALUE) {
		fido_log_debug("%s: SetupDiGetClassDevsA", __func__);
		goto fail;
	}

	ifdata.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	for (idx = 0; SetupDiEnumDeviceInterfaces(devinfo, NULL, &hid_guid,
	    idx, &ifdata) == true; idx++) {
		if (copy_info(&devlist[*olen], devinfo, idx, &ifdata) == 0) {
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
	if (devinfo != INVALID_HANDLE_VALUE)
		SetupDiDestroyDeviceInfoList(devinfo);

	return (r);
}

void *
fido_hid_open(const char *path)
{
	struct hid_win *ctx;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL)
		return (NULL);

	ctx->dev = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
	    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
	    FILE_FLAG_OVERLAPPED, NULL);

	if (ctx->dev == INVALID_HANDLE_VALUE) {
		free(ctx);
		return (NULL);
	}

	if ((ctx->overlap.hEvent = CreateEventA(NULL, FALSE, FALSE,
	    NULL)) == NULL) {
		fido_log_debug("%s: CreateEventA", __func__);
		fido_hid_close(ctx);
		return (NULL);
	}

	if (get_report_len(ctx->dev, 0, &ctx->report_in_len) < 0 ||
	    get_report_len(ctx->dev, 1, &ctx->report_out_len) < 0) {
		fido_log_debug("%s: get_report_len", __func__);
		fido_hid_close(ctx);
		return (NULL);
	}

	return (ctx);
}

void
fido_hid_close(void *handle)
{
	struct hid_win *ctx = handle;

	if (ctx->overlap.hEvent != NULL) {
		if (ctx->report_pending) {
			fido_log_debug("%s: report_pending", __func__);
			if (CancelIoEx(ctx->dev, &ctx->overlap) == 0)
				fido_log_debug("%s CancelIoEx: 0x%lx",
				    __func__, GetLastError());
		}
		CloseHandle(ctx->overlap.hEvent);
	}

	explicit_bzero(ctx->report, sizeof(ctx->report));
	CloseHandle(ctx->dev);
	free(ctx);
}

int
fido_hid_set_sigmask(void *handle, const fido_sigset_t *sigmask)
{
	(void)handle;
	(void)sigmask;

	return (FIDO_ERR_INTERNAL);
}

int
fido_hid_read(void *handle, unsigned char *buf, size_t len, int ms)
{
	struct hid_win	*ctx = handle;
	DWORD		 n;

	if (len != ctx->report_in_len - 1 || len > sizeof(ctx->report) - 1) {
		fido_log_debug("%s: len %zu", __func__, len);
		return (-1);
	}

	if (ctx->report_pending == 0) {
		memset(&ctx->report, 0, sizeof(ctx->report));
		ResetEvent(ctx->overlap.hEvent);
		if (ReadFile(ctx->dev, ctx->report, (DWORD)(len + 1), &n,
		    &ctx->overlap) == 0 && GetLastError() != ERROR_IO_PENDING) {
			CancelIo(ctx->dev);
			fido_log_debug("%s: ReadFile", __func__);
			return (-1);
		}
		ctx->report_pending = 1;
	}

	if (ms > -1 && WaitForSingleObject(ctx->overlap.hEvent,
	    (DWORD)ms) != WAIT_OBJECT_0)
		return (0);

	ctx->report_pending = 0;

	if (GetOverlappedResult(ctx->dev, &ctx->overlap, &n, TRUE) == 0) {
		fido_log_debug("%s: GetOverlappedResult", __func__);
		return (-1);
	}

	if (n != len + 1) {
		fido_log_debug("%s: expected %zu, got %zu", __func__,
		    len + 1, (size_t)n);
		return (-1);
	}

	memcpy(buf, ctx->report + 1, len);
	explicit_bzero(ctx->report, sizeof(ctx->report));

	return ((int)len);
}

int
fido_hid_write(void *handle, const unsigned char *buf, size_t len)
{
	struct hid_win	*ctx = handle;
	OVERLAPPED	 overlap;
	DWORD		 n;

	memset(&overlap, 0, sizeof(overlap));

	if (len != ctx->report_out_len) {
		fido_log_debug("%s: len %zu", __func__, len);
		return (-1);
	}

	if (WriteFile(ctx->dev, buf, (DWORD)len, NULL, &overlap) == 0 &&
	    GetLastError() != ERROR_IO_PENDING) {
		fido_log_debug("%s: WriteFile", __func__);
		return (-1);
	}

	if (GetOverlappedResult(ctx->dev, &overlap, &n, TRUE) == 0) {
		fido_log_debug("%s: GetOverlappedResult", __func__);
		return (-1);
	}

	if (n != len) {
		fido_log_debug("%s: expected %zu, got %zu", __func__, len,
		    (size_t)n);
		return (-1);
	}

	return ((int)len);
}

size_t
fido_hid_report_in_len(void *handle)
{
	struct hid_win *ctx = handle;

	return (ctx->report_in_len - 1);
}

size_t
fido_hid_report_out_len(void *handle)
{
	struct hid_win *ctx = handle;

	return (ctx->report_out_len - 1);
}
