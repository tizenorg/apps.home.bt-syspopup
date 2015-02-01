/*
* bt-syspopup
*
* Copyright 2013 Samsung Electronics Co., Ltd
*
* Contact: Hocheol Seo <hocheol.seo@samsung.com>
*           GirishAshok Joshi <girish.joshi@samsung.com>
*           DoHyun Pyun <dh79.pyun@samsung.com>
*
* Licensed under the Flora License, Version 1.1 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.tizenopensource.org/license
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*/

#include <stdio.h>
#include <dd-display.h>
#include <app.h>
#include <Ecore_X.h>
#include <utilX.h>
#include <vconf.h>
#include <vconf-keys.h>
#include <syspopup.h>
#include <E_DBus.h>
#include <aul.h>
#include <bluetooth.h>
#include <feedback.h>
#include <dd-deviced.h>
#include <efl_assist.h>
#include "bt-syspopup-w.h"
#include <dbus/dbus-glib-lowlevel.h>

#define COLOR_TABLE "/usr/apps/org.tizen.bt-syspopup/shared/res/tables/org.tizen.bt-syspopup_ChangeableColorTable.xml"
#define FONT_TABLE "/usr/apps/org.tizen.bt-syspopup/shared/res/tables/org.tizen.bt-syspopup_FontInfoTable.xml"

static char pin_value[BT_PIN_MLEN + 1] = {0,};
static int pin_index = 0;
static Ecore_Timer* pass_timer = 0;

static struct _info {
	const char *part_name;
	const char *popup_name;
	int pressed;
	const char *tts_name;
	Evas_Object *tts_button;
} keypad_info[12] = {
	{"button_00", "popup_00", 0, "0", NULL},
	{"button_01", "popup_01", 0, "1", NULL},
	{"button_02", "popup_02", 0, "2", NULL},
	{"button_03", "popup_03", 0, "3", NULL},
	{"button_04", "popup_04", 0, "4", NULL},
	{"button_05", "popup_05", 0, "5", NULL},
	{"button_06", "popup_06", 0, "6", NULL},
	{"button_07", "popup_07", 0, "7", NULL},
	{"button_08", "popup_08", 0, "8", NULL},
	{"button_09", "popup_09", 0, "9", NULL},
	{"confirm", "popup_confirm", 0, "Enter", NULL},
	{"button_clear", "popup_clear", 0, "Back", NULL},
};

static void __bluetooth_delete_input_view(struct bt_popup_appdata *ad);
static void __bluetooth_win_del(void *data);

static void __bluetooth_set_win_level(Evas_Object *parent);

static void __bluetooth_input_keyback_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info);

static void __bluetooth_input_mouseup_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info);

static void __bluetooth_keyback_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info);

static void __bluetooth_mouseup_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info);

static void __bluetooth_keyback_auth_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info);

static void __bluetooth_mouseup_auth_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info);
static void __bluetooth_terminate(void *data);

static int __bt_error_toast_timeout_cb(void *data);

static void __bt_draw_error_toast_popup(struct bt_popup_appdata *ad, char *toast_text);

static void __bt_draw_toast_popup(struct bt_popup_appdata *ad, char *toast_text);

static void __bluetooth_remove_all_event(struct bt_popup_appdata *ad);

static int __bluetooth_term(bundle *b, void *data)
{
	BT_DBG("System-popup: terminate");
	__bluetooth_remove_all_event((struct bt_popup_appdata *)data);
	__bluetooth_terminate(data);
	return 0;
}

static int __bluetooth_timeout(bundle *b, void *data)
{
	BT_DBG("System-popup: timeout");
	return 0;
}

syspopup_handler handler = {
	.def_term_fn = __bluetooth_term,
	.def_timeout_fn = __bluetooth_timeout
};

/* Cleanup objects to avoid mem-leak */
static void __bluetooth_cleanup(struct bt_popup_appdata *ad)
{
	BT_DBG("+");

	if (ad == NULL)
		return;

	if (ad->viberation_id > 0) {
		g_source_remove(ad->viberation_id);
		ad->viberation_id = 0;
	}

	if (ad->timer) {
		ecore_timer_del(ad->timer);
		ad->timer = NULL;
	}

	if (ad->popup) {
		elm_access_object_unregister(ad->popup);
		evas_object_del(ad->popup);
		ad->popup = NULL;
	}

	if (ad->ly_pass) {
		evas_object_del(ad->ly_pass);
		ad->ly_pass = NULL;
	}

	if (ad->ly_keypad) {
		evas_object_del(ad->ly_keypad);
		ad->ly_keypad = NULL;
	}

	if (ad->win_main) {
		evas_object_del(ad->win_main);
		ad->win_main = NULL;
	}

	if (ad->agent_proxy) {
		g_object_unref(ad->agent_proxy);
		ad->agent_proxy = NULL;
	}

	BT_DBG("-");
}

static void __bluetooth_set_win_level(Evas_Object *parent)
{
	Ecore_X_Window xwin;
	xwin = elm_win_xwindow_get(parent);
	if (xwin == 0) {
		BT_ERR("elm_win_xwindow_get is failed");
	} else {
		BT_DBG("Setting window type");
		ecore_x_netwm_window_type_set(xwin,
				ECORE_X_WINDOW_TYPE_NOTIFICATION);
		utilx_set_system_notification_level(ecore_x_display_get(),
				xwin, UTILX_NOTIFICATION_LEVEL_HIGH);
	}
}

static void __lock_display()
{
	int ret = display_lock_state(LCD_NORMAL, GOTO_STATE_NOW | HOLD_KEY_BLOCK, 0);
	if (ret < 0)
		BT_ERR("LCD Lock failed");
}

static void __unlock_display()
{
	int ret = display_unlock_state(LCD_NORMAL, PM_RESET_TIMER);
	if (ret < 0)
		BT_ERR("LCD Unlock failed");
}

static void __bluetooth_notify_event(feedback_pattern_e feedback)
{
	int result;

	BT_DBG("Notify event");

	result = feedback_initialize();
	if (result != FEEDBACK_ERROR_NONE) {
		BT_ERR("feedback_initialize error : %d", result);
		return;
	}

	result = feedback_play(feedback);
	BT_INFO("feedback [%d], ret value [%d]", feedback, result);

	result = feedback_deinitialize();
	if (result != FEEDBACK_ERROR_NONE) {
		BT_INFO("feedback_initialize error : %d", result);
		return;
	}
}

static gboolean __bluetooth_pairing_pattern_cb(gpointer data)
{
	__bluetooth_notify_event(FEEDBACK_PATTERN_BT_WAITING);

	return TRUE;
}

static void __bluetooth_parse_event(struct bt_popup_appdata *ad, const char *event_type)
{
	BT_DBG("+");

	if (!strcasecmp(event_type, "pin-request"))
		ad->event_type = BT_EVENT_PIN_REQUEST;
	else if (!strcasecmp(event_type, "passkey-confirm-request"))
		ad->event_type = BT_EVENT_PASSKEY_CONFIRM_REQUEST;
	else if (!strcasecmp(event_type, "passkey-request"))
		ad->event_type = BT_EVENT_PASSKEY_REQUEST;
	else if (!strcasecmp(event_type, "authorize-request"))
		ad->event_type = BT_EVENT_AUTHORIZE_REQUEST;
	else if (!strcasecmp(event_type, "app-confirm-request"))
		ad->event_type = BT_EVENT_APP_CONFIRM_REQUEST;
	else if (!strcasecmp(event_type, "push-authorize-request"))
		ad->event_type = BT_EVENT_PUSH_AUTHORIZE_REQUEST;
	else if (!strcasecmp(event_type, "confirm-overwrite-request"))
		ad->event_type = BT_EVENT_CONFIRM_OVERWRITE_REQUEST;
	else if (!strcasecmp(event_type, "keyboard-passkey-request"))
		ad->event_type = BT_EVENT_KEYBOARD_PASSKEY_REQUEST;
	else if (!strcasecmp(event_type, "bt-information"))
		ad->event_type = BT_EVENT_INFORMATION;
	else if (!strcasecmp(event_type, "exchange-request"))
		ad->event_type = BT_EVENT_EXCHANGE_REQUEST;
	else if (!strcasecmp(event_type, "phonebook-request"))
		ad->event_type = BT_EVENT_PHONEBOOK_REQUEST;
	else if (!strcasecmp(event_type, "message-request"))
		ad->event_type = BT_EVENT_MESSAGE_REQUEST;
	else if (!strcasecmp(event_type, "unable-to-pairing"))
		ad->event_type = BT_EVENT_UNABLE_TO_PAIRING;
	else if (!strcasecmp(event_type, "handsfree-disconnect-request"))
		ad->event_type = BT_EVENT_HANDSFREE_DISCONNECT_REQUEST;
	else if (!strcasecmp(event_type, "handsfree-connect-request"))
		ad->event_type = BT_EVENT_HANDSFREE_CONNECT_REQUEST;
	else if (!strcasecmp(event_type, "music-auto-connect-request"))
		ad->event_type = BT_EVENT_HANDSFREE_AUTO_CONNECT_REQUEST;
	else if (!strcasecmp(event_type, "system-reset-request"))
		ad->event_type = BT_EVENT_SYSTEM_RESET_REQUEST;
	else
		ad->event_type = 0x0000;

	BT_DBG("-");
	return;

}

static void __bluetooth_request_to_cancel(void)
{
	bt_device_cancel_bonding();
}

static void __bluetooth_remove_all_event(struct bt_popup_appdata *ad)
{
	BT_INFO("Remove event 0X%X", ad->event_type);
	switch (ad->event_type) {
	case BT_EVENT_PIN_REQUEST:

		dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyPinCode",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_STRING, "", G_TYPE_INVALID,
					   G_TYPE_INVALID);

		break;


	case BT_EVENT_KEYBOARD_PASSKEY_REQUEST:

		__bluetooth_request_to_cancel();

		break;

	case BT_EVENT_PASSKEY_CONFIRM_REQUEST:

		dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyConfirmation",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);
		__unlock_display();

		break;

	case BT_EVENT_PASSKEY_REQUEST:

		dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyPasskey",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_STRING, "", G_TYPE_INVALID,
					   G_TYPE_INVALID);

		break;

	case BT_EVENT_PASSKEY_DISPLAY_REQUEST:
		/* Nothing to do */
		break;

	case BT_EVENT_AUTHORIZE_REQUEST:

		dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyAuthorize",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);

		break;

	case BT_EVENT_APP_CONFIRM_REQUEST:
		{
			DBusMessage *msg;
			int response;

			msg = dbus_message_new_signal(
					BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
					BT_SYS_POPUP_INTERFACE,
					BT_SYS_POPUP_METHOD_RESPONSE);

			/* For timeout rejection is sent to  be handled in
			   application */
			response = BT_AGENT_REJECT;

			dbus_message_append_args(msg,
				 DBUS_TYPE_INT32, &response,
				 DBUS_TYPE_INVALID);

			e_dbus_message_send(ad->EDBusHandle,
				msg, NULL, -1, NULL);

			dbus_message_unref(msg);
		}
		break;

	case BT_EVENT_PUSH_AUTHORIZE_REQUEST:
	case BT_EVENT_EXCHANGE_REQUEST:

		dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyAuthorize",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);

		break;

	case BT_EVENT_CONFIRM_OVERWRITE_REQUEST: {
		DBusMessage *msg;
		int response = BT_AGENT_REJECT;

		msg = dbus_message_new_signal(BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
						BT_SYS_POPUP_INTERFACE,
						BT_SYS_POPUP_METHOD_RESPONSE);
		if (msg == NULL) {
			BT_ERR("msg == NULL, Allocation failed");
			break;
		}

		dbus_message_append_args(msg, DBUS_TYPE_INT32,
						&response, DBUS_TYPE_INVALID);

		e_dbus_message_send(ad->EDBusHandle, msg, NULL, -1, NULL);
		dbus_message_unref(msg);
		break;
	}

	case BT_EVENT_SYSTEM_RESET_REQUEST:

		dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyConfirmation",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);
		__unlock_display();

		break;

	default:
		break;
	}

	__bluetooth_win_del(ad);
}

static void _bt_clear_btn_up_cb(void *data, Evas_Object *o, const char *emission, const char *source)
{
	BT_DBG("+");
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	ret_if(ad == NULL);

	int i = 0;
	char buf[20] = {0,};

	if(pin_index > 0) {
		pin_value[pin_index-1] = 0;

		pin_index--;

		for(i = 0; i < pin_index; i++) {
			strcat(buf, "*");
		}
		elm_object_part_text_set(ad->ly_pass, "elm.text.password", buf);
	}
}

static int __bluetooth_request_timeout_cb(void *data)
{
	struct bt_popup_appdata *ad;

	if (data == NULL)
		return 0;

	ad = (struct bt_popup_appdata *)data;

	BT_DBG("Request time out, Canceling reqeust");

	/* Destory UI and timer */
	if (ad->timer) {
		ecore_timer_del(ad->timer);
		ad->timer = NULL;
	}

	__bluetooth_remove_all_event(ad);
	return 0;
}

static void __bluetooth_input_cancel_cb(void *data,
				       Evas_Object *obj, void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;

	bt_device_cancel_bonding();

	__bluetooth_win_del(ad);
}

static void __bluetooth_send_signal_pairing_confirm_result(void *data, int response)
{
	if (data == NULL)
		return;

	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	DBusMessage *msg = NULL;

	BT_DBG("+");

	msg = dbus_message_new_signal(BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
			      BT_SYS_POPUP_INTERFACE,
			      BT_SYS_POPUP_METHOD_RESPONSE);

	dbus_message_append_args(msg,
				 DBUS_TYPE_INT32, &response, DBUS_TYPE_INVALID);

	e_dbus_message_send(ad->EDBusHandle, msg, NULL, -1, NULL);
	dbus_message_unref(msg);

	BT_DBG("-");
}

static void __bluetooth_send_signal_reset_confirm_result(void *data, int response)
{
	if (data == NULL)
		return;

	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	DBusMessage *msg = NULL;

	BT_DBG("+");

	msg = dbus_message_new_signal(BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
			      BT_SYS_POPUP_INTERFACE,
			      BT_SYS_POPUP_METHOD_RESET_RESPONSE);

	dbus_message_append_args(msg,
				 DBUS_TYPE_INT32, &response, DBUS_TYPE_INVALID);

	e_dbus_message_send(ad->EDBusHandle, msg, NULL, -1, NULL);
	dbus_message_unref(msg);

	BT_DBG("-");
}

static void __bluetooth_passkey_confirm_cb(void *data,
					 Evas_Object *obj, void *event_info)
{
	if (obj == NULL || data == NULL)
		return;

	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	const char *event = elm_object_text_get(obj);

	if (!g_strcmp0(event, BT_STR_OK)) {
		__bluetooth_send_signal_pairing_confirm_result(ad, 1);
		dbus_g_proxy_call_no_reply(ad->agent_proxy, "ReplyConfirmation",
					   G_TYPE_UINT, BT_AGENT_ACCEPT,
					   G_TYPE_INVALID, G_TYPE_INVALID);
	} else {
		__bluetooth_send_signal_pairing_confirm_result(ad, 0);
		dbus_g_proxy_call_no_reply(ad->agent_proxy, "ReplyConfirmation",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);
	}
	__unlock_display();

	evas_object_del(obj);
	__bluetooth_win_del(ad);
}

static void __bluetooth_reset_cb(void *data, Evas_Object *obj, void *event_info)
{
	DBusMessage *message = NULL;
	DBusConnection *conn = NULL;
	int reset = 0;

	if (obj == NULL || data == NULL)
		return;

	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	const char *event = elm_object_text_get(obj);

	__bluetooth_send_signal_pairing_confirm_result(ad, 0);
	dbus_g_proxy_call_no_reply(ad->agent_proxy, "ReplyConfirmation",
				   G_TYPE_UINT, BT_AGENT_CANCEL,
				   G_TYPE_INVALID, G_TYPE_INVALID);

	if (!g_strcmp0(event, BT_STR_RESET)) {
		BT_INFO("Confirm Soft reset");
		reset = 1;
	}
	__bluetooth_send_signal_reset_confirm_result(ad, reset);

	__unlock_display();

	evas_object_del(obj);
	__bluetooth_win_del(ad);
}

static int __bluetooth_init_app_signal(struct bt_popup_appdata *ad)
{
	if (NULL == ad)
		return FALSE;

	e_dbus_init();
	ad->EDBusHandle = e_dbus_bus_get(DBUS_BUS_SYSTEM);
	if (!ad->EDBusHandle) {
		BT_ERR("e_dbus_bus_get failed");
		return FALSE;
	}

	BT_DBG("e_dbus_bus_get success  ");
	return TRUE;
}

static void __bluetooth_app_confirm_cb(void *data,
				     Evas_Object *obj, void *event_info)
{
	BT_DBG("__bluetooth_app_confirm_cb ");
	if (obj == NULL || data == NULL)
		return;

	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	const char *event = elm_object_text_get(obj);

	DBusMessage *msg = NULL;
	int response;

	msg = dbus_message_new_signal(BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
				      BT_SYS_POPUP_INTERFACE,
				      BT_SYS_POPUP_METHOD_RESPONSE);

	if (!g_strcmp0(event, BT_STR_OK))
		response = BT_AGENT_ACCEPT;
	else
		response = BT_AGENT_REJECT;

	dbus_message_append_args(msg,
				 DBUS_TYPE_INT32, &response, DBUS_TYPE_INVALID);

	e_dbus_message_send(ad->EDBusHandle, msg, NULL, -1, NULL);
	dbus_message_unref(msg);

	evas_object_del(obj);

	__bluetooth_win_del(ad);
}

static void __bluetooth_authorization_request_cb(void *data,
					       Evas_Object *obj,
					       void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	guint reply_val;

	if (obj == NULL || ad == NULL)
		return;

	const char *event = elm_object_text_get(obj);

	if (!g_strcmp0(event, BT_STR_OK)) {
		reply_val = (ad->make_trusted == TRUE) ?
				BT_AGENT_ACCEPT_ALWAYS : BT_AGENT_ACCEPT;
	} else {
		reply_val = BT_AGENT_CANCEL;
	}

	dbus_g_proxy_call_no_reply(ad->agent_proxy, "ReplyAuthorize",
		G_TYPE_UINT, reply_val,
		G_TYPE_INVALID, G_TYPE_INVALID);

	ad->make_trusted = FALSE;

	__bluetooth_win_del(ad);
}

static void __bluetooth_push_authorization_request_cb(void *data,
						    Evas_Object *obj,
						    void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	if (obj == NULL || ad == NULL)
		return;

	const char *event = elm_object_text_get(obj);

	if (!g_strcmp0(event, BT_STR_OK))
		dbus_g_proxy_call_no_reply(ad->obex_proxy, "ReplyAuthorize",
					   G_TYPE_UINT, BT_AGENT_ACCEPT,
					   G_TYPE_INVALID, G_TYPE_INVALID);
	else
		dbus_g_proxy_call_no_reply(ad->obex_proxy, "ReplyAuthorize",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);

	__bluetooth_win_del(ad);
}

static void __bluetooth_ime_hide(void)
{
	Ecore_IMF_Context *imf_context = NULL;
	imf_context = ecore_imf_context_add(ecore_imf_context_default_id_get());
	if (imf_context)
		ecore_imf_context_input_panel_hide(imf_context);
}

static Eina_Bool timer_cb(void *data)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	retv_if(ad == NULL, ECORE_CALLBACK_CANCEL);

	char buf[BT_PIN_MLEN + 1] = {0,};
	int i = 0;

	if(pass_timer != NULL) {
		ecore_timer_del(pass_timer);
		pass_timer = NULL;
	}

	if(pin_index != 0) {
		for(i = 0; i < pin_index; i++) {
			strcat(buf,"*");
		}
		elm_object_part_text_set(ad->ly_pass, "elm.text.password", buf);
	}

	return ECORE_CALLBACK_CANCEL;
}

static void __bluetooth_auth_check_clicked_cb(void *data, Evas_Object *obj,
							void *event_info)
{
	struct bt_popup_appdata *ad = data;
	Eina_Bool state = elm_check_state_get(obj);

	BT_INFO("Check %d", state);
	ad->make_trusted = state;
}

static void __bluetooth_mouseup_auth_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Mouse_Up *ev = event_info;
	struct bt_popup_appdata *ad = data;
	DBusMessage *msg = NULL;
	int response = BT_AGENT_REJECT;

	BT_DBG("Mouse event callback function is called + ");

	if (ev->button == 3) {
		evas_object_event_callback_del(obj, EVAS_CALLBACK_MOUSE_UP,
				__bluetooth_mouseup_auth_cb);
		evas_object_event_callback_del(obj, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_keyback_auth_cb);
		msg = dbus_message_new_signal(BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
				      BT_SYS_POPUP_INTERFACE,
				      BT_SYS_POPUP_METHOD_RESPONSE);

		dbus_message_append_args(msg,
					 DBUS_TYPE_INT32, &response, DBUS_TYPE_INVALID);

		e_dbus_message_send(ad->EDBusHandle, msg, NULL, -1, NULL);
		dbus_message_unref(msg);
		__bluetooth_win_del(ad);
	}
	BT_DBG("Mouse event callback -");
}

static void __bluetooth_keyback_auth_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Key_Down *ev = event_info;
	struct bt_popup_appdata *ad = data;
	DBusMessage *msg = NULL;
	int response = BT_AGENT_REJECT;

	BT_DBG("Keyboard event callback function is called + ");

	if (!strcmp(ev->keyname, KEY_BACK)) {
		evas_object_event_callback_del(obj, EVAS_CALLBACK_MOUSE_UP,
				__bluetooth_mouseup_auth_cb);
		evas_object_event_callback_del(obj, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_keyback_auth_cb);

		msg = dbus_message_new_signal(BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
				      BT_SYS_POPUP_INTERFACE,
				      BT_SYS_POPUP_METHOD_RESPONSE);

		dbus_message_append_args(msg,
					 DBUS_TYPE_INT32, &response, DBUS_TYPE_INVALID);

		e_dbus_message_send(ad->EDBusHandle, msg, NULL, -1, NULL);
		dbus_message_unref(msg);
		__bluetooth_win_del(ad);
	}
	BT_DBG("Keyboard Mouse event callback -");
}

static void __bluetooth_draw_auth_popup(struct bt_popup_appdata *ad,
			const char *title, char *btn1_text,
			char *btn2_text, void (*func) (void *data,
			Evas_Object *obj, void *event_info))
{
	char temp_str[BT_TITLE_STR_MAX_LEN + BT_TEXT_EXTRA_LEN] = { 0 };
	Evas_Object *btn1;
	Evas_Object *btn2;
	Evas_Object *layout;
	Evas_Object *label;
	Evas_Object *label2;
	Evas_Object *check;
	Evas_Object *ao = NULL;
	BT_DBG("+");

	ad->make_trusted = TRUE;

	ad->popup = elm_popup_add(ad->win_main);
	evas_object_size_hint_weight_set(ad->popup, EVAS_HINT_EXPAND,
					 	EVAS_HINT_EXPAND);

	elm_object_style_set(ad->popup, "transparent");

	layout = elm_layout_add(ad->popup);
	elm_layout_file_set(layout, CUSTOM_POPUP_PATH, "auth_popup");
	evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND,
							EVAS_HINT_EXPAND);

	if (title != NULL) {
		snprintf(temp_str, BT_TITLE_STR_MAX_LEN + BT_TEXT_EXTRA_LEN,
					"%s", title);

		label = elm_label_add(ad->popup);
		elm_object_style_set(label, "popup/default");
		elm_label_line_wrap_set(label, ELM_WRAP_MIXED);
		elm_object_text_set(label, temp_str);
		evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, 0.0);
		evas_object_size_hint_align_set(label, EVAS_HINT_FILL,
								EVAS_HINT_FILL);
		elm_object_part_content_set(layout, "popup_title", label);
		evas_object_show(label);
	}

	check = elm_check_add(ad->popup);
	elm_check_state_set(check, EINA_TRUE);
	evas_object_size_hint_align_set(check, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_size_hint_weight_set(check, EVAS_HINT_EXPAND,
							EVAS_HINT_EXPAND);
	evas_object_smart_callback_add(check, "changed",
					__bluetooth_auth_check_clicked_cb, ad);
	elm_object_part_content_set(layout, "check", check);
	evas_object_show(check);

	label2 = elm_label_add(ad->popup);
	elm_object_style_set(label2, "popup/default");
	elm_label_line_wrap_set(label2, ELM_WRAP_MIXED);
	elm_object_text_set(label2, BT_STR_DONT_ASK_AGAIN);
	evas_object_size_hint_weight_set(label2, EVAS_HINT_EXPAND, 0.0);
	evas_object_size_hint_align_set(label2, EVAS_HINT_FILL,
							EVAS_HINT_FILL);
	elm_object_part_content_set(layout, "check_label", label2);
	evas_object_show(label2);

	evas_object_show(layout);
	elm_object_content_set(ad->popup, layout);

	btn1 = elm_button_add(ad->popup);
	elm_object_style_set(btn1, "popup");
	elm_object_text_set(btn1, btn1_text);
	elm_object_part_content_set(ad->popup, "button1", btn1);
	evas_object_smart_callback_add(btn1, "clicked", func, ad);

	btn2 = elm_button_add(ad->popup);
	elm_object_style_set(btn2, "popup");
	elm_object_text_set(btn2, btn2_text);
	elm_object_part_content_set(ad->popup, "button2", btn2);
	evas_object_smart_callback_add(btn2, "clicked", func, ad);

	evas_object_event_callback_add(ad->popup, EVAS_CALLBACK_MOUSE_UP,
			__bluetooth_mouseup_auth_cb, ad);
	evas_object_event_callback_add(ad->popup, EVAS_CALLBACK_KEY_DOWN,
			__bluetooth_keyback_auth_cb, ad);

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);

	ao = elm_access_object_register(ad->popup, ad->win_main);
	if (ao != NULL) {
		elm_access_info_set(ao, ELM_ACCESS_INFO, temp_str);
	} else {
		BT_ERR("elm_access_object_register error!");
	}

	BT_DBG("-");
}

static void __bluetooth_draw_reset_popup(struct bt_popup_appdata *ad,
			const char *msg, char *btn1_text,
			char *btn2_text, void (*func) (void *data,
			Evas_Object *obj, void *event_info))
{
	char *txt;
	Evas_Object *btn1;
	Evas_Object *btn2;
	Evas_Object *label;
	Evas_Object *ao = NULL;

	BT_DBG("+");

	ad->popup = elm_popup_add(ad->win_main);
	if (ad->popup == NULL) {
		BT_ERR("elm_popup_add is failed");
		return;
	}
	evas_object_size_hint_weight_set(ad->popup,
			EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_text_set(ad->popup, "title,text", BT_STR_TITLE_CONNECT);
	ea_object_event_callback_add(ad->popup, EA_CALLBACK_BACK,
			ea_popup_back_cb, NULL);

	if (msg != NULL) {
		txt = elm_entry_utf8_to_markup(msg);
		elm_object_text_set(ad->popup, txt);
		free(txt);
	}

	btn1 = elm_button_add(ad->popup);
	elm_object_style_set(btn1, "popup");
	evas_object_size_hint_weight_set(btn1, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_text_set(btn1, btn1_text);
	elm_object_part_content_set(ad->popup, "button1", btn1);
	evas_object_smart_callback_add(btn1, "clicked", func, ad);

	btn2 = elm_button_add(ad->popup);
	elm_object_style_set(btn2, "popup");
	evas_object_size_hint_weight_set(btn2, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_text_set(btn2, btn2_text);
	elm_object_part_content_set(ad->popup, "button2", btn2);
	evas_object_smart_callback_add(btn2, "clicked", func, ad);

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);
	elm_object_focus_set(ad->popup, EINA_TRUE);

	ao = elm_access_object_register(ad->popup, ad->win_main);
	if (ao != NULL) {
		elm_access_info_set(ao, ELM_ACCESS_INFO, msg);
	} else {
		BT_ERR("elm_access_object_register error!");
	}

	BT_DBG("-");
}

static void __bluetooth_mouseup_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Mouse_Up *ev = event_info;
	struct bt_popup_appdata *ad = data;

	BT_DBG("Mouse event callback function is called + ");

	if (ev->button == 3) {
		evas_object_event_callback_del(obj, EVAS_CALLBACK_MOUSE_UP,
				__bluetooth_mouseup_cb);
		evas_object_event_callback_del(obj, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_keyback_cb);
		__bluetooth_remove_all_event(ad);
	}
	BT_DBG("Mouse event callback -");
}

static void __bluetooth_keyback_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Key_Down *ev = event_info;
	struct bt_popup_appdata *ad = data;

	BT_INFO("Keyboard event callback function is called %s+ ", ev->keyname);

	if (!strcmp(ev->keyname, KEY_BACK)) {

		evas_object_event_callback_del(obj, EVAS_CALLBACK_MOUSE_UP,
				__bluetooth_mouseup_cb);
		evas_object_event_callback_del(obj, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_keyback_cb);
		__bluetooth_remove_all_event(ad);
	}
	BT_DBG("Keyboard Mouse event callback -");
}

char* __bluetooth_convert_rgba_to_hex(int r, int g, int b, int a)
{
	int hexcolor = 0;
	char* string = NULL;

	string = g_try_malloc0(sizeof(char )* 255);

	hexcolor = (r << 24) + (g << 16) + (b << 8) + a;
	sprintf(string, "%08x", hexcolor );

	return string;
}


static void __bluetooth_draw_popup(struct bt_popup_appdata *ad,
			const char *title, char *btn1_text,
			char *btn2_text, void (*func) (void *data,
			Evas_Object *obj, void *event_info))
{
	BT_DBG("__bluetooth_draw_popup");
	Evas_Object *btn1;
	Evas_Object *btn2;
	Evas_Object *bg;
	Evas_Object *label = NULL;
	Evas_Object *scroller;
	Evas_Object *default_ly;
	Evas_Object *layout;
	Evas_Object *scroller_layout;
	Evas_Object *ao = NULL;
	char *txt;
	char *buf;
	int r = 0, g = 0, b = 0, a = 0;
	char *font;
	int size;


	bg = elm_bg_add(ad->win_main);
	evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_win_resize_object_add(ad->win_main, bg);
	evas_object_show(bg);

	default_ly = elm_layout_add(bg);
	elm_layout_theme_set(default_ly, "layout", "application", "default");
	evas_object_size_hint_weight_set(default_ly, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_content_set(bg, "elm.swallow.content", default_ly);
	evas_object_show(default_ly);

	layout = elm_layout_add(default_ly);
	elm_layout_file_set(layout, CUSTOM_POPUP_PATH, "passkey_confirm_popup");
	evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_content_set(default_ly, "elm.swallow.content", layout);
	evas_object_show(layout);

	scroller = elm_scroller_add(layout);
	elm_object_style_set(scroller, "effect");
	evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND,
			EVAS_HINT_EXPAND);
	evas_object_show(scroller);

	scroller_layout = elm_layout_add(scroller);
	elm_layout_file_set(scroller_layout, CUSTOM_POPUP_PATH, "passkey_confirm_popup_scroller");
	evas_object_size_hint_weight_set(scroller_layout, EVAS_HINT_EXPAND,
							EVAS_HINT_EXPAND);

	if (title) {
		BT_INFO("Title %s", title);
		label = elm_label_add(scroller_layout);
		elm_label_line_wrap_set(label, ELM_WRAP_MIXED);
		ea_theme_color_get("AT012",&r, &g, &b, &a,
			NULL, NULL, NULL, NULL,
			NULL, NULL, NULL, NULL);
		ea_theme_font_get("AT012", &font, &size);
		BT_INFO("font : %s, size : %d", font, size);

		elm_object_part_content_set(scroller_layout, "elm.text.block", label);
		evas_object_show(label);

		txt = elm_entry_utf8_to_markup(title);

		buf = g_strdup_printf("<font=%s><font_size=%d><color=#%s>%s</color></font_size></font>",
				font, size,
			__bluetooth_convert_rgba_to_hex(r, g, b, a),
			txt);
		free(txt);

		elm_object_text_set(label, buf);
		g_free(font);
		g_free(buf);
	}

	elm_object_content_set(scroller, scroller_layout);
	elm_object_part_content_set(layout, "scroller", scroller);

	elm_object_content_set(ad->win_main, bg);

	btn1 = elm_button_add(layout);
	elm_object_text_set(btn1,BT_STR_CANCEL);
	evas_object_size_hint_weight_set(btn1, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_content_set(layout, "btn1", btn1);
	evas_object_smart_callback_add(btn1, "clicked", func, ad);

	btn2 = elm_button_add(layout);
	elm_object_text_set(btn2,BT_STR_OK);
	evas_object_size_hint_weight_set(btn2, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_content_set(layout, "btn2", btn2);
	evas_object_smart_callback_add(btn2, "clicked", func, ad);

	evas_object_show(ad->win_main);

	ao = elm_access_object_register(label, layout);
	if (ao != NULL) {
		elm_access_info_set(ao, ELM_ACCESS_INFO, title);
	} else {
		BT_ERR("elm_access_object_register error!");
	}

	BT_DBG("__bluetooth_draw_popup END");
}

static void __bluetooth_draw_loading_popup(struct bt_popup_appdata *ad,
			const char *title, char *btn1_text,
			char *btn2_text, void (*func) (void *data,
			Evas_Object *obj, void *event_info))
{
	BT_DBG("+");
	Evas_Object *btn1;
	Evas_Object *btn2;
	Evas_Object *bg;
	Evas_Object *label = NULL;
	Evas_Object *scroller;
	Evas_Object *default_ly;
	Evas_Object *layout;
	Evas_Object *scroller_layout;
	Evas_Object *ao;
	char *txt;
	char *buf;
	int r = 0, g = 0, b = 0, a = 0;
	char *font;
	int size;

	__bluetooth_set_win_level(ad->win_main);

	bg = elm_bg_add(ad->win_main);
	evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_win_resize_object_add(ad->win_main, bg);
	evas_object_show(bg);

	default_ly = elm_layout_add(bg);
	elm_layout_theme_set(default_ly, "layout", "application", "default");
	evas_object_size_hint_weight_set(default_ly, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_content_set(bg, "elm.swallow.content", default_ly);
	evas_object_show(default_ly);

	layout = elm_layout_add(default_ly);
	elm_layout_file_set(layout, CUSTOM_POPUP_PATH, "passkey_confirm");
	evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_content_set(default_ly, "elm.swallow.content", layout);
	evas_object_show(layout);

	scroller = elm_scroller_add(layout);
	elm_object_style_set(scroller, "effect");
	evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND,
			EVAS_HINT_EXPAND);
	evas_object_show(scroller);

	scroller_layout = elm_layout_add(scroller);
	elm_layout_file_set(scroller_layout, CUSTOM_POPUP_PATH, "passkey_confirm_scroller");
	evas_object_size_hint_weight_set(scroller_layout, EVAS_HINT_EXPAND,
							EVAS_HINT_EXPAND);

	if (title) {
		BT_INFO("Title %s", title);
		label = elm_label_add(scroller_layout);
		elm_object_style_set(label, "popup/default");
		elm_label_line_wrap_set(label, ELM_WRAP_MIXED);
		elm_object_part_content_set(scroller_layout, "elm.text.block", label);
		evas_object_show(label);

		txt = elm_entry_utf8_to_markup(title);
		ea_theme_color_get("AT012",&r, &g, &b, &a,
					NULL, NULL, NULL, NULL,
					NULL, NULL, NULL, NULL);
		ea_theme_font_get("AT012", &font, &size);
		buf = g_strdup_printf("<font=%s><font_size=%d><color=#%s>%s</color></font_size></font>",
				font, size,
				__bluetooth_convert_rgba_to_hex(r, g, b, a),
				txt);
		free(txt);

		elm_object_text_set(label, buf);
		g_free(buf);
		g_free(font);
	}

	elm_object_content_set(scroller, scroller_layout);
	elm_object_part_content_set(layout, "scroller", scroller);

	elm_object_content_set(ad->win_main, bg);

	btn1 = elm_button_add(layout);
	elm_object_text_set(btn1,BT_STR_CANCEL);
	evas_object_size_hint_weight_set(btn1, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_content_set(layout, "btn1", btn1);
	evas_object_smart_callback_add(btn1, "clicked", func, ad);

	btn2 = elm_button_add(layout);
	elm_object_text_set(btn2,BT_STR_OK);
	evas_object_size_hint_weight_set(btn2, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_content_set(layout, "btn2", btn2);
	evas_object_smart_callback_add(btn2, "clicked", func, ad);

	evas_object_show(ad->win_main);

	ao = elm_access_object_register(label, layout);
	if (ao != NULL) {
		elm_access_info_set(ao, ELM_ACCESS_INFO, title);
	} else {
		BT_ERR("elm_access_object_register error!");
	}

	BT_DBG("__bluetooth_draw_loading_popup END");
}

static void __bluetooth_draw_text_popup(struct bt_popup_appdata *ad,
			const char *text,
			char *btn1_text, char *btn2_text,
			void (*func) (void *data,
			Evas_Object *obj, void *event_info))
{
	BT_DBG("__bluetooth_draw_text_popup");
	Evas_Object *btn1;
	Evas_Object *btn2;
	Evas_Object *label;
	Evas_Object *scroller_layout;
	Evas_Object *ao = NULL;
	char *txt;
	char *buf;
	int r = 0, g = 0, b = 0, a = 0;
	char *font;
	int size;

	ret_if(!ad);

	ad->popup = elm_popup_add(ad->win_main);
	evas_object_size_hint_weight_set(ad->popup, EVAS_HINT_EXPAND,
					 	EVAS_HINT_EXPAND);
	ea_object_event_callback_add(ad->popup, EA_CALLBACK_BACK,
			ea_popup_back_cb, NULL);

	__bluetooth_set_win_level(ad->popup);
	txt = elm_entry_utf8_to_markup(text);
	elm_object_text_set(ad->popup, txt);
	free(txt);

	btn1 = elm_button_add(ad->popup);
	elm_object_style_set(btn1, "popup");
	evas_object_size_hint_weight_set(btn1, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_text_set(btn1, btn1_text);
	elm_object_part_content_set(ad->popup, "button1", btn1);
	evas_object_smart_callback_add(btn1, "clicked", func, ad);

	btn2 = elm_button_add(ad->popup);
	elm_object_style_set(btn2, "popup");
	evas_object_size_hint_weight_set(btn2, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_text_set(btn2, btn2_text);
	elm_object_part_content_set(ad->popup, "button2", btn2);
	evas_object_smart_callback_add(btn2, "clicked", func, ad);

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);
	elm_object_focus_set(ad->popup, EINA_TRUE);
	BT_DBG("__bluetooth_draw_text_popup END");
}

static void __bluetooth_input_mouseup_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Mouse_Up *ev = event_info;
	struct bt_popup_appdata *ad = data;
	int response = BT_AGENT_CANCEL;
	char *input_text = NULL;
	char *convert_input_text = NULL;
	BT_DBG("Mouse event callback function is called + ");

	if (ev->button == 3) {
		if (ad == NULL)
			return;
		evas_object_event_callback_del(ad->entry, EVAS_CALLBACK_MOUSE_UP,
				__bluetooth_input_mouseup_cb);
		evas_object_event_callback_del(ad->entry, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_input_keyback_cb);
		evas_object_event_callback_del(ad->popup, EVAS_CALLBACK_MOUSE_UP,
				__bluetooth_input_mouseup_cb);
		evas_object_event_callback_del(ad->popup, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_input_keyback_cb);

		/* BT_EVENT_PIN_REQUEST / BT_EVENT_PASSKEY_REQUEST */
		input_text = (char *)elm_entry_entry_get(ad->entry);
		if (input_text) {
			convert_input_text =
				elm_entry_markup_to_utf8(input_text);
		}
		if (convert_input_text == NULL)
			return;

		if (ad->event_type == BT_EVENT_PIN_REQUEST) {
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyPinCode", G_TYPE_UINT, response,
					   G_TYPE_STRING, convert_input_text,
					   G_TYPE_INVALID, G_TYPE_INVALID);
		} else {
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyPasskey", G_TYPE_UINT, response,
					   G_TYPE_STRING, convert_input_text,
					   G_TYPE_INVALID, G_TYPE_INVALID);
		}
		__bluetooth_delete_input_view(ad);
		free(convert_input_text);
		if (ad->entry) {
			evas_object_del(ad->entry);
			ad->entry = NULL;
		}
		__bluetooth_win_del(ad);
	}
	BT_DBG("Mouse event callback -");
}

static void __bluetooth_input_keyback_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Key_Down *ev = event_info;
	struct bt_popup_appdata *ad = data;
	int response = BT_AGENT_CANCEL;
	char *input_text = NULL;
	char *convert_input_text = NULL;


	BT_DBG("Keyboard event callback function is called + ");

	if (!strcmp(ev->keyname, KEY_BACK)) {
		if (ad == NULL)
			return;
		evas_object_event_callback_del(ad->entry, EVAS_CALLBACK_MOUSE_UP,
				__bluetooth_input_mouseup_cb);
		evas_object_event_callback_del(ad->entry, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_input_keyback_cb);
		evas_object_event_callback_del(ad->popup, EVAS_CALLBACK_MOUSE_UP,
				__bluetooth_input_mouseup_cb);
		evas_object_event_callback_del(ad->popup, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_input_keyback_cb);
		/* BT_EVENT_PIN_REQUEST / BT_EVENT_PASSKEY_REQUEST */
		input_text = (char *)elm_entry_entry_get(ad->entry);
		if (input_text) {
			convert_input_text =
				elm_entry_markup_to_utf8(input_text);
		}
		if (convert_input_text == NULL)
			return;

		if (ad->event_type == BT_EVENT_PIN_REQUEST) {
			BT_DBG("It is PIN Request event ");
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyPinCode", G_TYPE_UINT, response,
					   G_TYPE_STRING, convert_input_text,
					   G_TYPE_INVALID, G_TYPE_INVALID);
		} else {
			BT_DBG("It is PASSKEYRequest event ");
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyPasskey", G_TYPE_UINT, response,
					   G_TYPE_STRING, convert_input_text,
					   G_TYPE_INVALID, G_TYPE_INVALID);
		}
		__bluetooth_delete_input_view(ad);
		free(convert_input_text);
		__bluetooth_win_del(ad);
	}
	BT_DBG("Keyboard Mouse event callback -");
}

void __bt_handle_keypad_value(void *data,int index)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	ret_if(ad == NULL);
	int max_value = 0;
	if(ad->event_type == BT_EVENT_PIN_REQUEST) {
		max_value = BT_PIN_MLEN;
	} else {
		max_value = BT_PK_MLEN;
	}

	if (pin_index >= max_value) {
		if(index >= 0 && index <= 9) {
			char buf[512] = { 0, };
			snprintf(buf,sizeof(buf),BT_STR_PIN_LENGTH_ERROR,max_value);
			__bt_draw_error_toast_popup(ad, buf);

			if(ad->timer != NULL) {
				ecore_timer_del(ad->timer);
				ad->timer = NULL;
			}
			ad->timer = ecore_timer_add(BT_TOAST_NOTIFICATION_TIMEOUT,
					(Ecore_Task_Cb)__bt_error_toast_timeout_cb, ad);
			return;
		}
	}

	if(index >= 0 && index <= 9) {
		char buf[20] = { 0, };
		char buf1[20] = { 0, };
		int i = 0;

		snprintf(buf, sizeof(buf), "%d", index);
		strcpy(&pin_value[pin_index++], buf);

		if(pin_index == 0) {
		} else {
			for(i = 0; i < pin_index-1; i++) {
				strcat(buf1,"*");
			}
		}
		strcat(buf1,buf);

		elm_object_part_text_set(ad->ly_pass, "elm.text.password", buf1);
		pass_timer = ecore_timer_add(1.5f, (Ecore_Task_Cb)timer_cb, data);

	} else if(index == 10) {//OK
		char *input_text = NULL;

		if (ad == NULL)
			return;

		if (pin_value)
			BT_DBG_SECURE("PIN/Passkey[%s] event[%d] response[%s]",
				pin_value, ad->event_type, "Accept");

		if(ad->event_type == BT_EVENT_PIN_REQUEST) {
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
						"ReplyPinCode", G_TYPE_UINT, BT_AGENT_ACCEPT,
						G_TYPE_STRING, pin_value,
						G_TYPE_INVALID, G_TYPE_INVALID);
		} else {
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
						"ReplyPasskey", G_TYPE_UINT, BT_AGENT_ACCEPT,
						G_TYPE_STRING, pin_value,
						G_TYPE_INVALID, G_TYPE_INVALID);
		}
		memset(pin_value, 0x00, sizeof(pin_value));
		pin_index = 0;

		__bluetooth_win_del(ad);

	} else if(index == 11) {
		/* Clear button */
		_bt_clear_btn_up_cb(ad, NULL, NULL, NULL);
	}
}

static void __bt_keypad_clicked_cb(void *data, Evas_Object *obj, const char *emission, const char *source)
{
	struct bt_popup_appdata *ad = data;
	ret_if(ad == NULL);

	int idx = (int)evas_object_data_get(obj, "__INDEX__");

	__bt_handle_keypad_value(ad, idx);
}
static void __bluetooth_draw_input_view(struct bt_popup_appdata *ad,
			const char *title, const char *text)
{
	BT_DBG("__bluetooth_draw_input_view");
	Evas_Object *bg = NULL;
	Evas_Object *layout = NULL;
	Evas_Object *label = NULL;
	Evas_Object *default_ly = NULL;
	Evas_Object *scroller;
	Elm_Object_Item *navi_item;
	static	char *buf;
	int i = 0, r = 0, g = 0, b = 0, a = 0;
	char *font;
	int size;

	if (ad == NULL || ad->win_main == NULL) {
		BT_ERR("Invalid parameter");
		return;
	}

	bg = elm_bg_add(ad->win_main);
	evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_win_resize_object_add(ad->win_main, bg);
	evas_object_show(bg);

	default_ly = elm_layout_add(bg);
	elm_layout_theme_set(default_ly, "layout", "application", "default");
	evas_object_size_hint_weight_set(default_ly, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_content_set(bg, "elm.swallow.content", default_ly);
	evas_object_show(default_ly);

	Evas_Object *naviframe;

	naviframe = elm_naviframe_add(default_ly);
	elm_object_part_content_set(default_ly, "elm.swallow.content", naviframe);

	layout = elm_layout_add(default_ly);
	elm_layout_file_set(layout, CUSTOM_POPUP_PATH, "passwd_popup");
	evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	ad->ly_pass = layout;

	scroller = elm_scroller_add(layout);
	evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND,
			EVAS_HINT_EXPAND);
	evas_object_show(scroller);

	label = elm_label_add(layout);
	elm_object_style_set(label, "popup/default");
	elm_label_line_wrap_set(label, ELM_WRAP_CHAR);
	ea_theme_color_get("AT012",&r, &g, &b, &a,
				NULL, NULL, NULL, NULL,
				NULL, NULL, NULL, NULL);
	if (EINA_TRUE == ea_theme_font_get("AT012", &font, &size))
		BT_INFO("font : %s, size : %d", font, size);
	else
		BT_INFO("ea_theme_font_get fail!");
	buf = g_strdup_printf("<font=%s><font_size=%d><color=#%s>%s</color></font_size></font>",
			font, 28,
			__bluetooth_convert_rgba_to_hex(r, g, b, a),
			text);

	BT_DBG("buf : %s, rgba:%d,%d,%d,%d", buf,r,g,b,a);

	elm_object_text_set(label, buf);
	g_free(buf);
	evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_show(label);
	elm_object_content_set(scroller, label);

	elm_object_part_content_set(ad->ly_pass, "label", scroller);

	BT_DBG("Create keypad");
	Evas_Object *layout_key = elm_layout_add(ad->ly_pass);
	elm_layout_file_set(layout_key, CUSTOM_POPUP_PATH, "keypad");

	ad->ly_keypad = layout_key;
	elm_object_part_content_set(ad->ly_pass, "sw.keypad", layout_key);
	elm_object_part_text_set(layout_key, "confirm", "OK");

	for (i = 0; i< 12; i++) {
		Evas_Object *button = elm_button_add(layout_key);
		char buf[32] = {0,};
		if(button == NULL) {
			BT_DBG("elm_button_add() failed");
			continue;
		}
		elm_object_style_set(button, "focus");
		snprintf(buf, sizeof(buf)-1, "%s,sw", keypad_info[i].part_name);
		elm_object_part_content_set(layout_key, buf, button);
		elm_access_info_set(button, ELM_ACCESS_INFO, keypad_info[i].tts_name);
		evas_object_data_set(button, "__INDEX__", (void *)i);
		evas_object_smart_callback_add(button, "clicked",
				__bt_keypad_clicked_cb, ad);
		keypad_info[i].tts_button = button;
	}

	navi_item = elm_naviframe_item_push(naviframe, BT_STR_PAIRING_REQUEST,
			NULL, NULL, layout, NULL);
	elm_naviframe_item_title_enabled_set(navi_item, EINA_TRUE, EINA_TRUE);
	elm_naviframe_item_title_visible_set(navi_item, EINA_TRUE);
	elm_object_content_set(ad->win_main, bg);
	evas_object_show(naviframe);
	evas_object_show(default_ly);
	evas_object_show(layout);
	evas_object_show(layout_key);
	evas_object_show(ad->win_main);

}

static void __bluetooth_delete_input_view(struct bt_popup_appdata *ad)
{
	__bluetooth_ime_hide();
}

static DBusGProxy* __bluetooth_create_agent_proxy(DBusGConnection *conn,
								const char *path)
{
	DBusGProxy *proxy;

	proxy = dbus_g_proxy_new_for_name(conn, "org.projectx.bt",
			path, "org.bluez.Agent1");
	if (!proxy)
		BT_ERR("dbus_g_proxy_new_for_name is failed");

	return proxy;

}

static int __bt_get_vconf_setup_wizard()
{
       int wizard_state = VCONFKEY_SETUP_WIZARD_UNLOCK;

       if (vconf_get_int(VCONFKEY_SETUP_WIZARD_STATE, &wizard_state))
               BT_ERR("Fail to get Wizard State");

       return wizard_state;
}

/* AUL bundle handler */
static int __bluetooth_launch_handler(struct bt_popup_appdata *ad,
			     void *reset_data, const char *event_type)
{
	bundle *kb = (bundle *) reset_data;
	char view_title[BT_TITLE_STR_MAX_LEN] = { 0 };
	char text[BT_GLOBALIZATION_STR_LENGTH] = { 0 };
	int timeout = 0;
	const char *device_name = NULL;
	const char *passkey = NULL;
	const char *file = NULL;
	const char *agent_path;
	char *conv_str = NULL;

	BT_DBG("+");

	if (!reset_data || !event_type) {
		BT_ERR("reset_data : %d, event_type : %d",
				reset_data, event_type);
		return -1;
	}

	BT_INFO("Event Type = %s[0X%X]", event_type, ad->event_type);

	if (!strcasecmp(event_type, "pin-request")) {
		device_name = bundle_get_val(kb, "device-name");
		agent_path = bundle_get_val(kb, "agent-path");

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 "%s", BT_STR_PAIRING_REQUEST);

		snprintf(text, BT_GLOBALIZATION_STR_LENGTH,
			 BT_STR_ENTER_PIN_TO_PAIR, conv_str);

		if (conv_str)
			free(conv_str);

		/* Request user inputted PIN for basic pairing */
		__bluetooth_draw_input_view(ad, view_title, text);
	} else if (!strcasecmp(event_type, "passkey-confirm-request")) {
		device_name = bundle_get_val(kb, "device-name");
		passkey = bundle_get_val(kb, "passkey");
		agent_path = bundle_get_val(kb, "agent-path");

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name && passkey) {
			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			     BT_STR_CONFIRM_PASSKEY_PS_TO_PAIR_WITH_PS,
			     device_name, passkey);

			BT_INFO("title: %s", view_title);

			__bluetooth_draw_text_popup(ad, view_title,
								BT_STR_CANCEL, BT_STR_OK,
								__bluetooth_passkey_confirm_cb);
		} else {
			timeout = BT_ERROR_TIMEOUT;
		}
	} else if (!strcasecmp(event_type, "passkey-request")) {
		const char *device_name = NULL;

		device_name = bundle_get_val(kb, "device-name");
		agent_path = bundle_get_val(kb, "agent-path");

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 "%s", BT_STR_PAIRING_REQUEST);

		snprintf(text, BT_GLOBALIZATION_STR_LENGTH,
			 BT_STR_ENTER_PIN_TO_PAIR, conv_str);

		if (conv_str)
			free(conv_str);

		/* Request user inputted Passkey for basic pairing */
		__bluetooth_draw_input_view(ad, view_title, text);

	} else if (!strcasecmp(event_type, "passkey-display-request")) {
		device_name = bundle_get_val(kb, "device-name");
		passkey = bundle_get_val(kb, "passkey");

		if (device_name && passkey) {
			conv_str = elm_entry_utf8_to_markup(device_name);

			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			     BT_STR_ENTER_PS_ON_PS_TO_PAIR, passkey, conv_str);

			BT_INFO("title: %s", view_title);

			if (conv_str)
				free(conv_str);

			__bluetooth_draw_popup(ad, view_title,
						BT_STR_CANCEL, NULL,
						__bluetooth_input_cancel_cb);
		} else {
			BT_ERR("wrong parameter : %s, %s", device_name, passkey);
			timeout = BT_ERROR_TIMEOUT;
		}
	} else if (!strcasecmp(event_type, "authorize-request")) {
		timeout = BT_AUTHORIZATION_TIMEOUT;

		device_name = bundle_get_val(kb, "device-name");
		agent_path = bundle_get_val(kb, "agent-path");

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 BT_STR_ALLOW_PS_TO_CONNECT_Q, conv_str);

		if (conv_str)
			free(conv_str);

		__bluetooth_draw_auth_popup(ad, view_title, BT_STR_CANCEL, BT_STR_OK,
				     __bluetooth_authorization_request_cb);
	} else if (!strcasecmp(event_type, "app-confirm-request")) {
		BT_DBG("app-confirm-request");
		timeout = BT_AUTHORIZATION_TIMEOUT;

		const char *title = NULL;
		const char *type = NULL;

		title = bundle_get_val(kb, "title");
		type = bundle_get_val(kb, "type");

		if (!title) {
			BT_ERR("title is NULL");
			return -1;
		}

		if (strcasecmp(type, "twobtn") == 0) {
			__bluetooth_draw_popup(ad, title, BT_STR_CANCEL, BT_STR_OK,
					     __bluetooth_app_confirm_cb);
		} else if (strcasecmp(type, "onebtn") == 0) {
			timeout = BT_NOTIFICATION_TIMEOUT;
			__bluetooth_draw_popup(ad, title, BT_STR_OK, NULL,
					     __bluetooth_app_confirm_cb);
		} else if (strcasecmp(type, "none") == 0) {
			timeout = BT_NOTIFICATION_TIMEOUT;
			__bluetooth_draw_popup(ad, title, NULL, NULL,
					     __bluetooth_app_confirm_cb);
		}
	} else if (!strcasecmp(event_type, "push-authorize-request")) {
		timeout = BT_AUTHORIZATION_TIMEOUT;

		device_name = bundle_get_val(kb, "device-name");
		file = bundle_get_val(kb, "file");

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 BT_STR_RECEIVE_PS_FROM_PS_Q, file, conv_str);

		if (conv_str)
			free(conv_str);

		__bluetooth_draw_popup(ad, view_title, BT_STR_CANCEL, BT_STR_OK,
				__bluetooth_push_authorization_request_cb);
	} else if (!strcasecmp(event_type, "confirm-overwrite-request")) {
		timeout = BT_AUTHORIZATION_TIMEOUT;

		file = bundle_get_val(kb, "file");

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 BT_STR_OVERWRITE_FILE_Q, file);

		__bluetooth_draw_popup(ad, view_title, BT_STR_CANCEL, BT_STR_OK,
				__bluetooth_app_confirm_cb);
	} else if (!strcasecmp(event_type, "keyboard-passkey-request")) {
		device_name = bundle_get_val(kb, "device-name");
		passkey = bundle_get_val(kb, "passkey");

		if (device_name && passkey) {
			conv_str = elm_entry_utf8_to_markup(device_name);

			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			     BT_STR_ENTER_PS_ON_PS_TO_PAIR, passkey, conv_str);

			BT_INFO("title: %s", view_title);

			if (conv_str)
				free(conv_str);

			__bluetooth_draw_popup(ad, view_title,
						BT_STR_CANCEL, NULL,
						__bluetooth_input_cancel_cb);
		} else {
			BT_ERR("wrong parameter : %s, %s", device_name, passkey);
			timeout = BT_ERROR_TIMEOUT;
		}
	} else if (!strcasecmp(event_type, "bt-information")) {
		BT_DBG("bt-information");
		timeout = BT_NOTIFICATION_TIMEOUT;

		const char *title = NULL;
		const char *type = NULL;

		title = bundle_get_val(kb, "title");
		type = bundle_get_val(kb, "type");

		if (title != NULL) {
			if (strlen(title) > 255) {
				BT_ERR("titls is too long");
				return -1;
			}
		} else {
			BT_ERR("titls is NULL");
			return -1;
		}

		if (strcasecmp(type, "onebtn") == 0) {
			__bluetooth_draw_popup(ad, title, BT_STR_OK, NULL,
					     __bluetooth_app_confirm_cb);
		} else if (strcasecmp(type, "none") == 0) {
			__bluetooth_draw_popup(ad, title, NULL, NULL,
					     __bluetooth_app_confirm_cb);
		}
	} else if (!strcasecmp(event_type, "exchange-request")) {
		timeout = BT_AUTHORIZATION_TIMEOUT;

		device_name = bundle_get_val(kb, "device-name");
		agent_path = bundle_get_val(kb, "agent-path");

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 BT_STR_RECEIVE_FILE_FROM_PS_Q, conv_str);

		if (conv_str)
			free(conv_str);

		__bluetooth_draw_popup(ad, view_title, BT_STR_CANCEL, BT_STR_OK,
				     __bluetooth_authorization_request_cb);
	} else if (!strcasecmp(event_type, "phonebook-request")) {
		timeout = BT_AUTHORIZATION_TIMEOUT;

		device_name = bundle_get_val(kb, "device-name");
		agent_path = bundle_get_val(kb, "agent-path");

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 BT_STR_ALLOW_PS_PHONEBOOK_ACCESS_Q, conv_str);

		if (conv_str)
			free(conv_str);

		__bluetooth_draw_auth_popup(ad, view_title, BT_STR_CANCEL, BT_STR_OK,
				     __bluetooth_authorization_request_cb);
	} else if (!strcasecmp(event_type, "message-request")) {
		timeout = BT_AUTHORIZATION_TIMEOUT;

		device_name = bundle_get_val(kb, "device-name");
		agent_path = bundle_get_val(kb, "agent-path");

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 BT_STR_ALLOW_PS_TO_ACCESS_MESSAGES_Q, conv_str);

		if (conv_str)
			free(conv_str);

		__bluetooth_draw_auth_popup(ad, view_title, BT_STR_CANCEL, BT_STR_OK,
				     __bluetooth_authorization_request_cb);
	} else if (!strcasecmp(event_type, "unable-to-pairing")) {
		DBusMessage *msg = NULL;
		int response = BT_AGENT_REJECT;

		timeout = BT_TOAST_NOTIFICATION_TIMEOUT;
		__bt_draw_toast_popup(ad, BT_STR_UNABLE_TO_CONNECT);
	} else if (!strcasecmp(event_type, "handsfree-disconnect-request")) {
		if (__bt_get_vconf_setup_wizard() == VCONFKEY_SETUP_WIZARD_LOCK) {
			BT_DBG("VCONFKEY_SETUP_WIZARD_LOCK: No toast shown");
			return -1;
		}

		timeout = BT_TOAST_NOTIFICATION_TIMEOUT;
		__bt_draw_toast_popup(ad, BT_STR_BLUETOOTH_HAS_BEEN_DISCONNECTED);

	} else if (!strcasecmp(event_type, "handsfree-connect-request")) {
		if (__bt_get_vconf_setup_wizard() == VCONFKEY_SETUP_WIZARD_LOCK) {
			BT_DBG("VCONFKEY_SETUP_WIZARD_LOCK: No toast shown");
			return -1;
		}

		timeout = BT_TOAST_NOTIFICATION_TIMEOUT;
		__bt_draw_toast_popup(ad, BT_STR_BLUETOOTH_CONNECTED);

	} else if (!strcasecmp(event_type, "music-auto-connect-request")) {
		timeout = BT_TOAST_NOTIFICATION_TIMEOUT;
		__bt_draw_toast_popup(ad, BT_STR_AUTO_CONNECT);

	} else if (!strcasecmp(event_type, "system-reset-request")) {
		device_name = bundle_get_val(kb, "device-name");
		agent_path = bundle_get_val(kb, "agent-path");

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name) {
			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
					BT_STR_FACTORY_RESET, device_name, device_name);
			__bluetooth_draw_reset_popup(ad, view_title,
					BT_STR_CANCEL, BT_STR_RESET,
					__bluetooth_reset_cb);
		} else {
			BT_ERR("device name NULL");
			timeout = BT_ERROR_TIMEOUT;
		}
	} else {
		BT_ERR("Unknown event_type : %s", event_type);
		return -1;
	}

	if (ad->event_type != BT_EVENT_FILE_RECEIVED && timeout != 0) {
		ad->timer = ecore_timer_add(timeout, (Ecore_Task_Cb)
					__bluetooth_request_timeout_cb, ad);
	}
	BT_DBG("-");
	return 0;
}

static Eina_Bool __bt_toast_mouseup_cb(void *data, int type, void *event)
{
	Ecore_Event_Key *ev = event;
	struct bt_popup_appdata *ad;

	ad = (struct bt_popup_appdata *)data;
	if(ev == NULL || ev->keyname == NULL || ad == NULL){
		return ECORE_CALLBACK_DONE;
	}

	__bluetooth_win_del(ad);

	return ECORE_CALLBACK_DONE;
}

static void __bt_draw_toast_popup(struct bt_popup_appdata *ad, char *toast_text)
{
	Evas_Object *ao = NULL;

	ad->popup = elm_popup_add(ad->win_main);
	elm_object_style_set(ad->popup, "toast");
	elm_popup_orient_set(ad->popup, ELM_POPUP_ORIENT_BOTTOM);
	evas_object_size_hint_weight_set(ad->popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	ea_object_event_callback_add(ad->popup, EA_CALLBACK_BACK, ea_popup_back_cb, NULL);

	elm_object_part_text_set(ad->popup,"elm.text", toast_text);

	__bluetooth_set_win_level(ad->popup);

	ecore_event_handler_add(ECORE_EVENT_MOUSE_BUTTON_UP, __bt_toast_mouseup_cb, ad);

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);
	elm_object_focus_set(ad->popup, EINA_TRUE);

	ao = elm_object_part_access_object_get(ad->popup, "access.outline");
        if (ao != NULL)
		elm_access_info_set(ao, ELM_ACCESS_INFO, toast_text);
}

static void __bt_draw_error_toast_popup(struct bt_popup_appdata *ad, char *toast_text)
{
	Evas_Object *ao = NULL;

	ad->popup = elm_popup_add(ad->win_main);
	elm_object_style_set(ad->popup, "toast");
	elm_popup_orient_set(ad->popup, ELM_POPUP_ORIENT_BOTTOM);
	evas_object_size_hint_weight_set(ad->popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	ea_object_event_callback_add(ad->popup, EA_CALLBACK_BACK, ea_popup_back_cb, NULL);
	elm_object_part_text_set(ad->popup,"elm.text", toast_text);

	__bluetooth_set_win_level(ad->popup);

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);
	elm_object_focus_set(ad->popup, EINA_TRUE);

	ao = elm_object_part_access_object_get(ad->popup, "access.outline");
        if (ao != NULL)
		elm_access_info_set(ao, ELM_ACCESS_INFO, toast_text);
}

static Eina_Bool __exit_idler_cb(void *data)
{
	elm_exit();
	return ECORE_CALLBACK_CANCEL;
}

static void __popup_terminate(void)
{
	if (ecore_idler_add(__exit_idler_cb, NULL))
		return;

	__exit_idler_cb(NULL);
}

static void __bluetooth_win_del(void *data)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;

	__bluetooth_cleanup(ad);
	__popup_terminate();
}

static int __bt_error_toast_timeout_cb(void *data)
{
	struct bt_popup_appdata *ad;

	if (data == NULL)
		return 0;

	ad = (struct bt_popup_appdata *)data;

	BT_DBG("Toast Popup timeout");

	/* Destory toast popup and timer */
	if (ad->timer) {
		ecore_timer_del(ad->timer);
		ad->timer = NULL;
	}

	if (ad->popup) {
		elm_access_object_unregister(ad->popup);
		evas_object_del(ad->popup);
		ad->popup = NULL;
	}
	return 0;
}
static Evas_Object *__bluetooth_create_win(const char *name, void *data)
{
	Evas_Object *eo;
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	int w;
	int h;

	eo = elm_win_add(NULL, name, ELM_WIN_DIALOG_BASIC);
	if (eo) {
		elm_win_alpha_set(eo, EINA_TRUE);
		elm_win_title_set(eo, name);
		elm_win_borderless_set(eo, EINA_TRUE);
		ecore_x_window_size_get(ecore_x_window_root_first_get(),
					&w, &h);
		evas_object_resize(eo, w, h);
	}

	return eo;
}

static void __bluetooth_session_init(struct bt_popup_appdata *ad)
{
	DBusGConnection *conn = NULL;
	GError *err = NULL;

	g_type_init();

	conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, &err);

	if (!conn) {
		BT_ERR("ERROR: Can't get on system bus [%s]",
			     err->message);
		g_error_free(err);
		return;
	}

	ad->conn = conn;

	ad->obex_proxy = dbus_g_proxy_new_for_name(conn,
						   "org.bluez.frwk_agent",
						   "/org/obex/ops_agent",
						   "org.openobex.Agent");
	if (!ad->obex_proxy)
		BT_ERR("Could not create obex dbus proxy");

	if (!__bluetooth_init_app_signal(ad))
		BT_ERR("__bt_syspopup_init_app_signal failed");
}

void __bluetooth_set_color_table(void *data)
{
	FN_START;
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;

	/* Set color table */
	ea_theme_changeable_ui_enabled_set(EINA_TRUE);
	ad->color_table = ea_theme_color_table_new(COLOR_TABLE);
	if (ad->color_table == NULL)
		BT_ERR("ea_theme_color_table_new failed!");
	else if (EINA_TRUE != ea_theme_colors_set(ad->color_table, EA_THEME_STYLE_DEFAULT))
		BT_ERR("ea_theme_colors_set failed!");

	ad->font_table = ea_theme_font_table_new(FONT_TABLE);
	if (ad->font_table == NULL)
		BT_ERR("ea_theme_color_table_new failed!");
	else if (EINA_TRUE != ea_theme_fonts_set(ad->font_table))
		BT_ERR("ea_theme_fonts_set failed!");

	FN_END;
}

static bool __bluetooth_create(void *data)
{
	struct bt_popup_appdata *ad = data;
	Evas_Object *win = NULL;

	BT_DBG("__bluetooth_create() start.");

	/* create window */
	win = __bluetooth_create_win(PACKAGE, ad);
	if (win == NULL) {
		BT_ERR("__bluetooth_create_win is failed");
		return false;
	}
	ad->win_main = win;
	ad->viberation_id = 0;
	/* Handle rotation */
	if (elm_win_wm_rotation_supported_get(ad->win_main)) {
		int rots[4] = {0, 90, 180, 270};
		elm_win_wm_rotation_available_rotations_set(ad->win_main, rots, 4);
	}

	/* init internationalization */
	bindtextdomain(BT_COMMON_PKG, BT_LOCALEDIR);
	textdomain(BT_COMMON_PKG);

	ecore_imf_init();
	__bluetooth_set_color_table(ad);

	__bluetooth_session_init(ad);
	if (bt_initialize() != BT_ERROR_NONE) {
		BT_ERR("bt_initialize is failed");
	}

	return true;
}


static int __vzw_launch_modem_syspopup(void)
{
	int ret;
	bundle* b;

	b = bundle_create();
	if (!b) {
		BT_ERR("Failed to create bundle");
		return -1;
	}

	bundle_add(b, "event-type", "grayzone_alert");
	ret = syspopup_launch("wc-syspopup", b);
	if (ret < 0)
		BT_ERR("Failed to launch syspopup");

	bundle_free(b);

	return ret;
}

static int __na_common_launch_modem_popup(void)
{
       int ret;
       app_control_h app_control = NULL;

       ret = app_control_create(&app_control);
       if (ret != APP_CONTROL_ERROR_NONE) {
               BT_ERR("Failed to create appcontrol[%d]", ret);
               return -1;
       }

       app_control_add_extra_data(app_control,
                       "http://samsung.com/appcontrol/data/connection_type",
                       "grayzone_alert");
       app_control_set_app_id(app_control, "org.tizen.weconn-popup");

       ret = app_control_send_launch_request(app_control, NULL, NULL);
       if (ret != APP_CONTROL_ERROR_NONE) {
               BT_ERR("failed appcontrol launch request [%d]", ret);
               app_control_destroy(app_control);
               return -1;
       }

       app_control_destroy(app_control);

       return 0;
}

static void __bluetooth_terminate(void *data)
{
	BT_DBG("__bluetooth_terminate()");

	struct bt_popup_appdata *ad = data;

	if (bt_deinitialize() != BT_ERROR_NONE) {
		BT_ERR("bt_deinitialize is failed");
	}
	__bluetooth_ime_hide();

	if (ad->conn) {
		dbus_g_connection_unref(ad->conn);
		ad->conn = NULL;
	}

	if (ad->color_table != NULL) {
		ea_theme_color_table_free(ad->color_table);
		ad->color_table = NULL;
	}

	if (ad->font_table != NULL) {
		ea_theme_font_table_free(ad->font_table);
		ad->font_table = NULL;
	}

	if (ad->popup)
		evas_object_del(ad->popup);

	if (ad->win_main)
		evas_object_del(ad->win_main);

	ad->popup = NULL;
	ad->win_main = NULL;

	if (ad->event_type == BT_EVENT_HANDSFREE_DISCONNECT_REQUEST) {
		char *salescode = NULL;
		salescode = vconf_get_str(VCONFKEY_CSC_SALESCODE);

		if (g_strcmp0(salescode, "VZW") == 0) {
				__vzw_launch_modem_syspopup();
		} else if (g_strcmp0(salescode, "ATT") == 0 ||
					g_strcmp0(salescode, "TMB") == 0 ||
					g_strcmp0(salescode, "SPR") == 0 ||
					g_strcmp0(salescode, "USC") == 0 ||
					g_strcmp0(salescode, "XAC") == 0) {
			__na_common_launch_modem_popup();
		}

		g_free(salescode);
	}
}

static void __bluetooth_pause(void *data)
{
	BT_DBG("__bluetooth_pause()");
	return;
}

static void __bluetooth_resume(void *data)
{
	BT_DBG("__bluetooth_resume()");
	return;
}

static void __bluetooth_reset(app_control_h app_control, void *user_data)
{
	struct bt_popup_appdata *ad = user_data;
	bundle *b = NULL;
	const char *event_type = NULL;
	int block = 0;
	int ret = 0;

	BT_DBG("__bluetooth_reset()");

	if (ad == NULL) {
		BT_ERR("App data is NULL");
		return;
	}

	ret = app_control_to_bundle(app_control, &b);

	/* Start Main UI */
	event_type = bundle_get_val(b, "event-type");
	if (event_type == NULL) {
		BT_ERR("event type is NULL");
		return;
	}
	BT_INFO("event_type : %s", event_type);

	__bluetooth_parse_event(ad, event_type);

	if (!strcasecmp(event_type, "terminate")) {
		BT_ERR("get terminate event");
		__bluetooth_win_del(ad);
		return;
	}

	if (syspopup_has_popup(b)) {
		/* Destroy the existing popup*/
		BT_ERR("Aleady popup existed");
		__bluetooth_cleanup(ad);

		/* create window */
		ad->win_main = __bluetooth_create_win(PACKAGE, ad);
		if (ad->win_main == NULL) {
			BT_ERR("fail to create win!");
			return;
		}

		ret = syspopup_reset(b);
		if (ret == -1) {
			BT_ERR("syspopup_reset err");
			return;
		}

		goto DONE;
	}

	ret = syspopup_create(b, &handler, ad->win_main, ad);
	if (ret == -1) {
		BT_ERR("syspopup_create err");
		__bluetooth_remove_all_event(ad);
		return;
	}


DONE:
	ret = __bluetooth_launch_handler(ad, b, event_type);

	if (vconf_get_bool(VCONFKEY_SETAPPL_BLOCKMODE_WEARABLE_BOOL, &block)) {
		BT_ERR("Get Block Status fail!!");
	}

	if (ret != 0) {
		BT_ERR("__bluetooth_launch_handler is failed. event[%d], ret[%d]",
				ad->event_type, ret);
		__bluetooth_remove_all_event(ad);
	}

	if (!block) {
		/* Change LCD brightness */
		if (display_change_state(LCD_NORMAL) != 0)
			BT_ERR("Fail to change LCD");

		if (ad->event_type == BT_EVENT_HANDSFREE_DISCONNECT_REQUEST) {
			__bluetooth_notify_event(FEEDBACK_PATTERN_DISCONNECTED);
		} else if (ad->event_type == BT_EVENT_HANDSFREE_CONNECT_REQUEST) {
			__bluetooth_notify_event(FEEDBACK_PATTERN_CONNECTED);
		} else if (ad->event_type == BT_EVENT_PASSKEY_CONFIRM_REQUEST ||
			   ad->event_type == BT_EVENT_SYSTEM_RESET_REQUEST) {
			__bluetooth_notify_event(FEEDBACK_PATTERN_BT_PAIRING);
			ad->viberation_id = g_timeout_add(BT_VIBERATION_INTERVAL,
						  __bluetooth_pairing_pattern_cb, NULL);
			__lock_display();
		}
	}

	return;
}

static void __bluetooth_lang_changed_cb(void *data)
{
	BT_DBG("+");
	ret_if(data == NULL);
	BT_DBG("-");
}

EXPORT int main(int argc, char *argv[])
{
	struct bt_popup_appdata ad;
	memset(&ad, 0x0, sizeof(struct bt_popup_appdata));

	app_event_callback_s event_callback;

	event_callback.create = __bluetooth_create;
	event_callback.terminate = __bluetooth_terminate;
	event_callback.pause = __bluetooth_pause;
	event_callback.resume = __bluetooth_resume;
	event_callback.app_control = __bluetooth_reset;
	event_callback.low_memory = NULL;
	event_callback.low_battery = NULL;
	event_callback.device_orientation = NULL;
	event_callback.language_changed = __bluetooth_lang_changed_cb;
	event_callback.region_format_changed = NULL;

	return app_efl_main(&argc, &argv, &event_callback, &ad);
}
