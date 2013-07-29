/*
 * Copyright (c) 2012-2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Flora License, Version 1.1 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __DEF_BT_SYSPOPUP_H_
#define __DEF_BT_SYSPOPUP_H_

#include <Elementary.h>
#include <dlog.h>
#include <glib.h>
#include <dbus/dbus-glib.h>

#ifndef PACKAGE_NAME
#define PACKAGE_NAME "org.tizen.bt-syspopup"
#endif

#ifndef PREFIX
#define PREFIX "/opt/apps"PACKAGE_NAME
#endif

#define EXPORT __attribute__((visibility("default")))

#define TEMP_DIR	"/tmp"

#define PACKAGE		"bt-syspopup"
#define APPNAME		"bt-syspopup"
#define ICON_DIR	PREFIX"/res/default/small/icon"

#define CUSTOM_POPUP_PATH PREFIX"/res/edje/custom_popup.edj"

#define BT_COMMON_PKG		"ug-setting-bluetooth-efl"
#define BT_COMMON_RES		"/usr/ug/res/locale"

#define _EDJ(obj) elm_layout_edje_get(obj)

#define BT_AUTHENTICATION_TIMEOUT		35
#define BT_AUTHORIZATION_TIMEOUT		25
#define BT_NOTIFICATION_TIMEOUT		2
#define BT_ERROR_TIMEOUT			1

#define BT_PIN_MLEN 16		/* Pin key max length */
#define BT_PK_MLEN 6		/* Passkey max length */
#define BT_CONTROLBAR_MAX_LENGTH 3

#define BT_GLOBALIZATION_STR_LENGTH 256
#define BT_DEVICE_NAME_LENGTH_MAX 256
#define BT_FILE_NAME_LENGTH_MAX 256
#define BT_TEXT_EXTRA_LEN 20

#define BT_TITLE_STR_MAX_LEN \
	(BT_GLOBALIZATION_STR_LENGTH+BT_DEVICE_NAME_LENGTH_MAX+BT_FILE_NAME_LENGTH_MAX)

#define BT_MESSAGE_STRING_SIZE 256*2+1

#undef LOG_TAG
#define LOG_TAG "BT_SYSPOPUP"

#define BT_DBG(format, args...) SLOGD(format, ##args)
#define BT_ERR(format, args...) SLOGE(format, ##args)

#define BT_SYS_POPUP_IPC_NAME "org.projectx"
#define BT_SYS_POPUP_IPC_RESPONSE_OBJECT "/org/projectx/bt_syspopup_res"
#define BT_SYS_POPUP_INTERFACE "User.Bluetooth.syspopup"
#define BT_SYS_POPUP_METHOD_RESPONSE "Response"

/* String defines to support multi-languages */
#define BT_STR_ENTER_PIN	\
	dgettext(BT_COMMON_PKG, "IDS_BT_HEADER_ENTERPIN")
#define BT_STR_ALLOW_PS_TO_CONNECT_Q	\
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_ALLOW_PS_TO_CONNECT_Q")

#define BT_STR_RECEIVE_PS_FROM_PS_Q \
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_RECEIVE_PS_FROM_PS_Q")

#define BT_STR_PASSKEY_MATCH_Q \
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_MATCH_PASSKEYS_ON_PS_Q")

#define BT_STR_OVERWRITE_FILE_Q \
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_PS_ALREADY_EXISTS_OVERWRITE_Q")

#define BT_STR_ENTER_PS_ON_PS_TO_PAIR \
	dgettext(BT_COMMON_PKG, "IDS_BT_BODY_ENTER_P1SS_ON_P2SS_TO_PAIR_THEN_TAP_RETURN_OR_ENTER")

#define BT_STR_EXCHANGE_OBJECT_WITH_PS_Q \
       dgettext(BT_COMMON_PKG, "IDS_BT_POP_EXCHANGEOBJECT")

/* Need to convert the design ID */
#define BT_STR_BLUETOOTH_PAIRING_REQUEST \
	dgettext(BT_COMMON_PKG, "IDS_BT_HEADER_BLUETOOTH_PAIRING_REQUEST")

#define BT_STR_ENTER_PIN_TO_PAIR \
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_ENTER_PIN_TO_PAIR_WITH_PS_HTRY_0000_OR_1234")

#define BT_STR_SHOW_PASSWORD \
	dgettext(BT_COMMON_PKG, "IDS_BT_BODY_SHOW_PASSWORD")

#define BT_STR_CONFIRM_PASSKEY_PS_TO_PAIR_WITH_PS \
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_CONFIRM_PASSKEY_IS_P2SS_TO_PAIR_WITH_P1SS")

#define BT_STR_ALLOW_PS_PHONEBOOK_ACCESS_Q \
	dgettext(BT_COMMON_PKG, "IDS_BT_BODY_ALLOW_PS_PHONEBOOK_ACCESS")

#define BT_STR_ALLOW_PS_TO_ACCESS_MESSAGES_Q \
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_ALLOW_PS_TO_ACCESS_MESSAGES_Q")

#define BT_STR_DONT_ASK_AGAIN \
	dgettext(BT_COMMON_PKG, "IDS_BT_BODY_DONT_ASK_AGAIN")

#define BT_STR_PIN_ERROR_TRY_AGAIN_Q \
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_INCORRECT_PIN_TRY_AGAIN_Q")

#define BT_STR_TIMEOUT_TRY_AGAIN_Q \
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_BLUETOOTH_TIMEOUT_TRY_AGAIN_Q")

#define BT_STR_BLUETOOTH_ERROR_TRY_AGAIN_Q \
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_BLUETOOTH_ERROR_TRY_AGAIN_Q")

#define BT_STR_OK dgettext("sys_string", "IDS_COM_SK_OK")
#define BT_STR_YES dgettext("sys_string", "IDS_COM_SK_YES")
#define BT_STR_NO dgettext("sys_string", "IDS_COM_SK_NO")
#define BT_STR_DONE dgettext("sys_string", "IDS_COM_SK_DONE")
#define BT_STR_CANCEL dgettext("sys_string", "IDS_COM_SK_CANCEL")

typedef enum {
	BT_CHANGED_MODE_ENABLE,
	BT_CHANGED_MODE_DISABLE,
} bt_changed_mode_type_t;

typedef enum {
	BT_EVENT_PIN_REQUEST = 0x0001,
	BT_EVENT_PASSKEY_CONFIRM_REQUEST = 0x0002,
	BT_EVENT_PASSKEY_REQUEST = 0x0004,
	BT_EVENT_PASSKEY_DISPLAY_REQUEST = 0x0008,
	BT_EVENT_AUTHORIZE_REQUEST = 0x0010,
	BT_EVENT_APP_CONFIRM_REQUEST = 0x0020,
	BT_EVENT_PUSH_AUTHORIZE_REQUEST = 0x0040,
	BT_EVENT_CONFIRM_OVERWRITE_REQUEST = 0x0080,
	BT_EVENT_FILE_RECEIVED = 0x0100,
	BT_EVENT_KEYBOARD_PASSKEY_REQUEST = 0x0200,
	BT_EVENT_INFORMATION = 0x0400,
	BT_EVENT_TERMINATE = 0x0800,
	BT_EVENT_EXCHANGE_REQUEST = 0x1000,
	BT_EVENT_PHONEBOOK_REQUEST = 0x2000,
	BT_EVENT_MESSAGE_REQUEST = 0x4000,
} bt_popup_event_type_t;

typedef enum {
	BT_AGENT_ACCEPT,
	BT_AGENT_REJECT,
	BT_AGENT_CANCEL,
	BT_CORE_AGENT_TIMEOUT,
	BT_AGENT_ACCEPT_ALWAYS,
} bt_agent_accept_type_t;

struct bt_popup_appdata {
	Evas *evas;
	Evas_Object *win_main;
	Evas_Object *popup;

	/* Passkey layout objects */
	Evas_Object *entry;
	Evas_Object *editfield;
	Evas_Object *edit_field_save_btn;
	Evas_Object *ticker_noti;

	Ecore_Timer *timer;
	Ecore_Event_Handler *event_handle;

	DBusGProxy *agent_proxy;
	DBusGProxy *obex_proxy;
	E_DBus_Connection *EDBusHandle;
	DBusGConnection *conn;

	int changed_mode;
	gboolean make_trusted;
	bt_popup_event_type_t event_type;
};

#endif				/* __DEF_BT_SYSPOPUP_H_ */
