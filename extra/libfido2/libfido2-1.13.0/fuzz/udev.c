/*
 * Copyright (c) 2021 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>

#include <linux/hidraw.h>
#include <linux/input.h>

#include <assert.h>
#include <errno.h>
#include <libudev.h>
#include <stdlib.h>

#include "mutator_aux.h"

struct udev {
	int magic;
};

struct udev_enumerate {
	int magic;
	struct udev_list_entry *list_entry;
};

struct udev_list_entry {
	int magic;
};

struct udev_device {
	int magic;
	struct udev_device *parent;
};

#define UDEV_MAGIC		0x584492cc
#define UDEV_DEVICE_MAGIC	0x569180dd
#define UDEV_LIST_ENTRY_MAGIC	0x497422ee
#define UDEV_ENUM_MAGIC		0x583570ff

#define ASSERT_TYPE(x, m)		assert((x) != NULL && (x)->magic == (m))
#define ASSERT_UDEV(x)			ASSERT_TYPE((x), UDEV_MAGIC)
#define ASSERT_UDEV_ENUM(x)		ASSERT_TYPE((x), UDEV_ENUM_MAGIC)
#define ASSERT_UDEV_LIST_ENTRY(x)	ASSERT_TYPE((x), UDEV_LIST_ENTRY_MAGIC)
#define ASSERT_UDEV_DEVICE(x)		ASSERT_TYPE((x), UDEV_DEVICE_MAGIC)

static const char *uevent;
static const struct blob *report_descriptor;

struct udev *__wrap_udev_new(void);
struct udev_device *__wrap_udev_device_get_parent_with_subsystem_devtype(
    struct udev_device *, const char *, const char *);
struct udev_device *__wrap_udev_device_new_from_syspath(struct udev *,
    const char *);
struct udev_enumerate *__wrap_udev_enumerate_new(struct udev *);
struct udev_list_entry *__wrap_udev_enumerate_get_list_entry(
    struct udev_enumerate *);
struct udev_list_entry *__wrap_udev_list_entry_get_next(
    struct udev_list_entry *);
const char *__wrap_udev_device_get_sysattr_value(struct udev_device *,
    const char *);
const char *__wrap_udev_list_entry_get_name(struct udev_list_entry *);
const char *__wrap_udev_device_get_devnode(struct udev_device *);
const char *__wrap_udev_device_get_sysnum(struct udev_device *);
int __wrap_udev_enumerate_add_match_subsystem(struct udev_enumerate *,
    const char *);
int __wrap_udev_enumerate_scan_devices(struct udev_enumerate *);
int __wrap_ioctl(int, unsigned long , ...);
void __wrap_udev_device_unref(struct udev_device *);
void __wrap_udev_enumerate_unref(struct udev_enumerate *);
void __wrap_udev_unref(struct udev *);
void set_udev_parameters(const char *, const struct blob *);

struct udev_device *
__wrap_udev_device_get_parent_with_subsystem_devtype(struct udev_device *child,
    const char *subsystem, const char *devtype)
{
	ASSERT_UDEV_DEVICE(child);
	fido_log_debug("%s", subsystem); /* XXX consume */
	fido_log_debug("%s", devtype); /* XXX consume */
	if (child->parent != NULL)
		return child->parent;
	if ((child->parent = calloc(1, sizeof(*child->parent))) == NULL)
		return NULL;
	child->parent->magic = UDEV_DEVICE_MAGIC;

	return child->parent;
}

const char *
__wrap_udev_device_get_sysattr_value(struct udev_device *udev_device,
    const char *sysattr)
{
	ASSERT_UDEV_DEVICE(udev_device);
	if (uniform_random(400) < 1)
		return NULL;
	if (!strcmp(sysattr, "manufacturer") || !strcmp(sysattr, "product"))
		return "product info"; /* XXX randomise? */
	else if (!strcmp(sysattr, "uevent"))
		return uevent;

	return NULL;
}

const char *
__wrap_udev_list_entry_get_name(struct udev_list_entry *entry)
{
	ASSERT_UDEV_LIST_ENTRY(entry);
	return uniform_random(400) < 1 ? NULL : "name"; /* XXX randomise? */
}

struct udev_device *
__wrap_udev_device_new_from_syspath(struct udev *udev, const char *syspath)
{
	struct udev_device *udev_device;

	ASSERT_UDEV(udev);
	fido_log_debug("%s", syspath);
	if ((udev_device = calloc(1, sizeof(*udev_device))) == NULL)
		return NULL;
	udev_device->magic = UDEV_DEVICE_MAGIC;

	return udev_device;
}

const char *
__wrap_udev_device_get_devnode(struct udev_device *udev_device)
{
	ASSERT_UDEV_DEVICE(udev_device);
	return uniform_random(400) < 1 ? NULL : "/dev/zero";
}

const char *
__wrap_udev_device_get_sysnum(struct udev_device *udev_device)
{
	ASSERT_UDEV_DEVICE(udev_device);
	return uniform_random(400) < 1 ? NULL : "101010"; /* XXX randomise? */
}

void
__wrap_udev_device_unref(struct udev_device *udev_device)
{
	ASSERT_UDEV_DEVICE(udev_device);
	if (udev_device->parent) {
		ASSERT_UDEV_DEVICE(udev_device->parent);
		free(udev_device->parent);
	}
	free(udev_device);
}

struct udev *
__wrap_udev_new(void)
{
	struct udev *udev;

	if ((udev = calloc(1, sizeof(*udev))) == NULL)
		return NULL;
	udev->magic = UDEV_MAGIC;

	return udev;
}

struct udev_enumerate *
__wrap_udev_enumerate_new(struct udev *udev)
{
	struct udev_enumerate *udev_enum;

	ASSERT_UDEV(udev);
	if ((udev_enum = calloc(1, sizeof(*udev_enum))) == NULL)
		return NULL;
	udev_enum->magic = UDEV_ENUM_MAGIC;

	return udev_enum;
}

int
__wrap_udev_enumerate_add_match_subsystem(struct udev_enumerate *udev_enum,
    const char *subsystem)
{
	ASSERT_UDEV_ENUM(udev_enum);
	fido_log_debug("%s:", subsystem);
	return uniform_random(400) < 1 ? -EINVAL : 0;
}

int
__wrap_udev_enumerate_scan_devices(struct udev_enumerate *udev_enum)
{
	ASSERT_UDEV_ENUM(udev_enum);
	return uniform_random(400) < 1 ? -EINVAL : 0;
}

struct udev_list_entry *
__wrap_udev_enumerate_get_list_entry(struct udev_enumerate *udev_enum)
{
	ASSERT_UDEV_ENUM(udev_enum);
	if ((udev_enum->list_entry = calloc(1,
	    sizeof(*udev_enum->list_entry))) == NULL)
		return NULL;
	udev_enum->list_entry->magic = UDEV_LIST_ENTRY_MAGIC;

	return udev_enum->list_entry;
}

struct udev_list_entry *
__wrap_udev_list_entry_get_next(struct udev_list_entry *udev_list_entry)
{
	ASSERT_UDEV_LIST_ENTRY(udev_list_entry);
	return uniform_random(400) < 1 ? NULL : udev_list_entry;
}

void
__wrap_udev_enumerate_unref(struct udev_enumerate *udev_enum)
{
	ASSERT_UDEV_ENUM(udev_enum);
	if (udev_enum->list_entry)
		ASSERT_UDEV_LIST_ENTRY(udev_enum->list_entry);
	free(udev_enum->list_entry);
	free(udev_enum);
}

void
__wrap_udev_unref(struct udev *udev)
{
	ASSERT_UDEV(udev);
	free(udev);
}

int
__wrap_ioctl(int fd, unsigned long request, ...)
{
	va_list ap;
	struct hidraw_report_descriptor *hrd;

	(void)fd;

	if (uniform_random(400) < 1) {
		errno = EINVAL;
		return -1;
	}

	va_start(ap, request);

	switch (IOCTL_REQ(request)) {
	case IOCTL_REQ(HIDIOCGRDESCSIZE):
		*va_arg(ap, int *) = (int)report_descriptor->len;
		break;
	case IOCTL_REQ(HIDIOCGRDESC):
		hrd = va_arg(ap, struct hidraw_report_descriptor *);
		assert(hrd->size == report_descriptor->len);
		memcpy(hrd->value, report_descriptor->body, hrd->size);
		break;
	default:
		warnx("%s: unknown request 0x%lx", __func__, request);
		abort();
	}

	va_end(ap);

	return 0;
}

void
set_udev_parameters(const char *uevent_ptr,
    const struct blob *report_descriptor_ptr)
{
	uevent = uevent_ptr;
	report_descriptor = report_descriptor_ptr;
}
