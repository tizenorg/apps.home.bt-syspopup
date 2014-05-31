/*
* bt-syspopup
*
* Copyright 2012 Samsung Electronics Co., Ltd
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
#include "bt-syspopup.h"

#define PREDEF_FACTORY_RESET		"launchfr"

static void __bluetooth_delete_input_view(struct bt_popup_appdata *ad);
static void __bluetooth_win_del(void *data);

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

static void __bt_draw_toast_popup(struct bt_popup_appdata *ad, char *toast_text);

static int __bluetooth_term(bundle *b, void *data)
{
	BT_DBG("System-popup: terminate");
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
		evas_object_del(ad->popup);
		ad->popup = NULL;
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
	BT_DBG("feedback [%d], ret value [%d]", feedback, result);

	result = feedback_deinitialize();
	if (result != FEEDBACK_ERROR_NONE) {
		BT_DBG("feedback_initialize error : %d", result);
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
	else if (!strcasecmp(event_type, "pairing-retry-request"))
		ad->event_type = BT_EVENT_RETRY_PAIR_REQUEST;
	else if (!strcasecmp(event_type, "handsfree-disconnect-request"))
		ad->event_type = BT_EVENT_HANDSFREE_DISCONNECT_REQUEST;
	else if (!strcasecmp(event_type, "handsfree-connect-request"))
		ad->event_type = BT_EVENT_HANDSFREE_CONNECT_REQUEST;
	else if (!strcasecmp(event_type, "music-auto-connect-request"))
		ad->event_type = BT_EVENT_HANDSFREE_AUTO_CONNECT_REQUEST;
	else if (!strcasecmp(event_type, "factory-reset-request"))
		ad->event_type = BT_EVENT_FACTORY_RESET_REQUEST;
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
	BT_DBG("Remove event 0X%X", ad->event_type);
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

	case BT_EVENT_FACTORY_RESET_REQUEST:

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
static void __bluetooth_retry_pairing_cb(void *data,
				     Evas_Object *obj, void *event_info)
{
	BT_DBG("+ ");
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
				 DBUS_TYPE_INT32, &response,
				 DBUS_TYPE_INVALID);

	e_dbus_message_send(ad->EDBusHandle, msg, NULL, -1, NULL);

	dbus_message_unref(msg);

	evas_object_del(obj);

	__bluetooth_win_del(ad);
	BT_DBG("-");
}

static int __bluetooth_pairing_retry_timeout_cb(void *data)
{
	BT_DBG("+ ");

	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;

	DBusMessage *msg = NULL;
	int response;

	msg = dbus_message_new_signal(BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
					  BT_SYS_POPUP_INTERFACE,
					  BT_SYS_POPUP_METHOD_RESPONSE);

	response = BT_AGENT_REJECT;

	dbus_message_append_args(msg,
				 DBUS_TYPE_INT32, &response,
				 DBUS_TYPE_INVALID);

	e_dbus_message_send(ad->EDBusHandle, msg, NULL, -1, NULL);

	dbus_message_unref(msg);

	__bluetooth_win_del(ad);
	BT_DBG("-");

	return 0;
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

#ifdef PIN_REQUEST_FOR_BASIC_PAIRING
static void __bluetooth_input_request_cb(void *data,
				       Evas_Object *obj, void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	const char *event = elm_object_text_get(obj);
	int response;
	char *input_text = NULL;
	char *convert_input_text = NULL;

	if (ad == NULL)
		return;

	/* BT_EVENT_PIN_REQUEST / BT_EVENT_PASSKEY_REQUEST */

	input_text = (char *)elm_entry_entry_get(ad->entry);

	if (input_text) {
		convert_input_text =
		    elm_entry_markup_to_utf8(input_text);
	}

	if (!g_strcmp0(event, BT_STR_OK))
		response = BT_AGENT_ACCEPT;
	else
		response = BT_AGENT_CANCEL;

	if (convert_input_text == NULL)
		return;

	BT_DBG_SECURE("PIN/Passkey[%s] event[%d] response[%d - %s]",
		     convert_input_text, ad->event_type, response,
		     (response == BT_AGENT_ACCEPT) ? "Accept" : "Cancel");

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

	__bluetooth_win_del(ad);
}
#endif

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
	if (obj == NULL || data == NULL)
		return;

	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	const char *event = elm_object_text_get(obj);

	__bluetooth_send_signal_pairing_confirm_result(ad, 0);
	dbus_g_proxy_call_no_reply(ad->agent_proxy, "ReplyConfirmation",
				   G_TYPE_UINT, BT_AGENT_CANCEL,
				   G_TYPE_INVALID, G_TYPE_INVALID);
	__unlock_display();

	if (!g_strcmp0(event, BT_STR_RESET)) {
		BT_DBG("Factory reset");
		deviced_call_predef_action(PREDEF_FACTORY_RESET, 0, NULL);
	}

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
		BT_ERR("e_dbus_bus_get failed  \n ");
		return FALSE;
	}

	BT_DBG("e_dbus_bus_get success \n ");
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

static void __bluetooth_entry_change_cb(void *data, Evas_Object *obj,
				      void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	char *input_text = NULL;
	char *convert_input_text = NULL;
	char *output_text = NULL;
	int text_length = 0;

	input_text = (char *)elm_entry_entry_get(obj);

	if (input_text) {
		convert_input_text = elm_entry_markup_to_utf8(input_text);
		if (convert_input_text) {
			text_length = strlen(convert_input_text);

			if (text_length == 0) {
				elm_object_disabled_set(ad->edit_field_save_btn,
							EINA_TRUE);
			} else {
				elm_object_disabled_set(ad->edit_field_save_btn,
							EINA_FALSE);
			}

			if (ad->event_type == BT_EVENT_PASSKEY_REQUEST) {
				if (text_length > BT_PK_MLEN) {
					convert_input_text[BT_PK_MLEN] = '\0';
					output_text = elm_entry_utf8_to_markup(
							convert_input_text);

					elm_entry_entry_set(obj, output_text);
					elm_entry_cursor_end_set(obj);
					free(output_text);
				}
			} else {
				if (text_length > BT_PIN_MLEN) {
					convert_input_text[BT_PIN_MLEN] = '\0';
					output_text = elm_entry_utf8_to_markup(
							convert_input_text);

					elm_entry_entry_set(obj, output_text);
					elm_entry_cursor_end_set(obj);
					free(output_text);
				}
			}
			free(convert_input_text);
		}
	}
}

static void __bluetooth_auth_check_clicked_cb(void *data, Evas_Object *obj,
							void *event_info)
{
	struct bt_popup_appdata *ad = data;
	Eina_Bool state = elm_check_state_get(obj);

	BT_DBG("Check %d", state);
	ad->make_trusted = state;
}

static void __bluetooth_mouseup_auth_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Mouse_Up *ev = event_info;
	struct bt_popup_appdata *ad = data;
	DBusMessage *msg = NULL;
	int response = BT_AGENT_REJECT;

	BT_DBG("Mouse event callback function is called + \n");

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
	BT_DBG("Mouse event callback -\n");
}

static void __bluetooth_keyback_auth_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Key_Down *ev = event_info;
	struct bt_popup_appdata *ad = data;
	DBusMessage *msg = NULL;
	int response = BT_AGENT_REJECT;

	BT_DBG("Keyboard event callback function is called + \n");

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
	BT_DBG("Keyboard Mouse event callback -\n");
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
	Evas_Object *scroller;
	Evas_Object *label;

	BT_DBG("+");

	ad->popup = elm_popup_add(ad->win_main);
	evas_object_size_hint_weight_set(ad->popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	elm_object_part_text_set(ad->popup, "title,text", BT_STR_TITLE_CONNECT);

	if (msg != NULL) {
		scroller = elm_scroller_add(ad->popup);
		elm_object_style_set(scroller, "effect");

		evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND,
				EVAS_HINT_EXPAND);
		elm_object_content_set(ad->popup, scroller);
		evas_object_show(scroller);

		label = elm_label_add(scroller);
		elm_object_style_set(label, "popup/default");
		elm_label_line_wrap_set(label, ELM_WRAP_MIXED);

		// temporarily remove this routine for RTL mark.
		//txt = elm_entry_utf8_to_markup(msg);
		//elm_object_text_set(label, txt);
		//free(txt);
		elm_object_text_set(label, msg);

		evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND,
				EVAS_HINT_EXPAND);
		evas_object_size_hint_align_set(label, EVAS_HINT_FILL,
				EVAS_HINT_FILL);
		elm_object_content_set(scroller, label);
	}

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

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);
	elm_object_focus_set(ad->popup, EINA_TRUE);

	BT_DBG("-");
}

static void __bluetooth_mouseup_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Mouse_Up *ev = event_info;
	struct bt_popup_appdata *ad = data;

	BT_DBG("Mouse event callback function is called + \n");

	if (ev->button == 3) {
		evas_object_event_callback_del(obj, EVAS_CALLBACK_MOUSE_UP,
				__bluetooth_mouseup_cb);
		evas_object_event_callback_del(obj, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_keyback_cb);
		__bluetooth_remove_all_event(ad);
	}
	BT_DBG("Mouse event callback -\n");
}

static void __bluetooth_keyback_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Key_Down *ev = event_info;
	struct bt_popup_appdata *ad = data;

	BT_DBG("Keyboard event callback function is called %s+ \n", ev->keyname);

	if (!strcmp(ev->keyname, KEY_BACK)) {

		evas_object_event_callback_del(obj, EVAS_CALLBACK_MOUSE_UP,
				__bluetooth_mouseup_cb);
		evas_object_event_callback_del(obj, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_keyback_cb);
		__bluetooth_remove_all_event(ad);
	}
	BT_DBG("Keyboard Mouse event callback -\n");
}

static void __bluetooth_draw_popup(struct bt_popup_appdata *ad,
			const char *title, char *btn1_text,
			char *btn2_text, void (*func) (void *data,
			Evas_Object *obj, void *event_info))
{
	char temp_str[BT_TITLE_STR_MAX_LEN+BT_TEXT_EXTRA_LEN] = { 0 };
	Evas_Object *btn1;
	Evas_Object *btn2;
	Evas_Object *bg;
	Evas_Object *label;
	Evas_Object *scroller;
	Evas_Object *default_ly;
	Evas_Object *layout;
	Evas_Object *scroller_layout;
	char *txt;
	char *buf;

	BT_DBG("__bluetooth_draw_popup");

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
		BT_DBG("Title %s", title);
		label = elm_label_add(scroller_layout);
		elm_object_style_set(label, "popup/default");
		elm_label_line_wrap_set(label, ELM_WRAP_MIXED);

		elm_object_part_content_set(scroller_layout, "elm.text.block", label);
		evas_object_show(label);

		txt = elm_entry_utf8_to_markup(title);
		buf = g_strdup_printf(BT_SET_FONT_SIZE, BT_TITLE_FONT_30, txt);
		elm_object_text_set(label, buf);
		free(txt);
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

	BT_DBG("__bluetooth_draw_popup END");
}

static void __bluetooth_draw_loading_popup(struct bt_popup_appdata *ad,
			const char *title, char *btn1_text,
			char *btn2_text, void (*func) (void *data,
			Evas_Object *obj, void *event_info))
{
	char temp_str[BT_TITLE_STR_MAX_LEN+BT_TEXT_EXTRA_LEN] = { 0 };
	Evas_Object *btn1;
	Evas_Object *btn2;
	Evas_Object *bg;
	Evas_Object *label;
	Evas_Object *scroller;
	Evas_Object *default_ly;
	Evas_Object *layout;
	Evas_Object *scroller_layout;
	Evas_Object *icon;
	char *txt;
	char *buf;

	BT_DBG("__bluetooth_draw_loading_popup");

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

//	elm_object_signal_emit(scroller_layout, "connect.start,show", "");
	icon = elm_image_add(scroller_layout);
	elm_image_file_set(icon, CUSTOM_POPUP_PATH, BT_IMAGE_WATCH);
	elm_object_part_content_set(scroller_layout, "elm.swallow.img.watch", icon);

	icon = elm_image_add(scroller_layout);
	elm_image_file_set(icon, CUSTOM_POPUP_PATH, BT_IMAGE_PHONE);
	elm_object_part_content_set(scroller_layout, "elm.swallow.img.phone", icon);


	if (title) {
		BT_DBG("Title %s", title);
		label = elm_label_add(scroller_layout);
		elm_object_style_set(label, "popup/default");
		elm_label_line_wrap_set(label, ELM_WRAP_MIXED);

		elm_object_part_content_set(scroller_layout, "elm.text.block", label);
		evas_object_show(label);

		txt = elm_entry_utf8_to_markup(title);
		buf = g_strdup_printf(BT_SET_FONT_SIZE, BT_TITLE_FONT_30, txt);
		elm_object_text_set(label, buf);
		free(txt);
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

	BT_DBG("__bluetooth_draw_loading_popup END");
}

static void __bluetooth_input_mouseup_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Mouse_Up *ev = event_info;
	struct bt_popup_appdata *ad = data;
	int response = BT_AGENT_CANCEL;
	char *input_text = NULL;
	char *convert_input_text = NULL;
	BT_DBG("Mouse event callback function is called + \n");

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
	BT_DBG("Mouse event callback -\n");
}

static void __bluetooth_input_keyback_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Key_Down *ev = event_info;
	struct bt_popup_appdata *ad = data;
	int response = BT_AGENT_CANCEL;
	char *input_text = NULL;
	char *convert_input_text = NULL;


	BT_DBG("Keyboard event callback function is called + \n");

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
			BT_DBG("It is PIN Request event \n");
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyPinCode", G_TYPE_UINT, response,
					   G_TYPE_STRING, convert_input_text,
					   G_TYPE_INVALID, G_TYPE_INVALID);
		} else {
			BT_DBG("It is PASSKEYRequest event \n");
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyPasskey", G_TYPE_UINT, response,
					   G_TYPE_STRING, convert_input_text,
					   G_TYPE_INVALID, G_TYPE_INVALID);
		}
		__bluetooth_delete_input_view(ad);
		free(convert_input_text);
		__bluetooth_win_del(ad);
	}
	BT_DBG("Keyboard Mouse event callback -\n");
}

#ifdef PIN_REQUEST_FOR_BASIC_PAIRING
static void __bluetooth_draw_input_view(struct bt_popup_appdata *ad,
			const char *title, const char *text,
			void (*func)
			(void *data, Evas_Object *obj, void *event_info))
{
	Evas_Object *conformant = NULL;
	Evas_Object *content = NULL;
	Evas_Object *layout = NULL;
	Evas_Object *passpopup = NULL;
	Evas_Object *label = NULL;
	Evas_Object *entry = NULL;
	Evas_Object *l_button = NULL;
	Evas_Object *r_button = NULL;
	static Elm_Entry_Filter_Limit_Size limit_filter_data;

	if (ad == NULL || ad->win_main == NULL) {
		BT_ERR("Invalid parameter");
		return;
	}

	evas_object_show(ad->win_main);

	conformant = elm_conformant_add(ad->win_main);
	if (conformant == NULL) {
		BT_ERR("conformant is NULL");
		return;
	}
	ad->popup = conformant;

	elm_win_conformant_set(ad->win_main, EINA_TRUE);
	elm_win_resize_object_add(ad->win_main, conformant);
	evas_object_size_hint_weight_set(conformant, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(conformant, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_show(conformant);

	content = elm_layout_add(conformant);
	elm_object_content_set(conformant, content);

	passpopup = elm_popup_add(content);
	elm_object_part_text_set(passpopup, "title,text", title);

	elm_object_style_set(passpopup, "transparent");

	layout = elm_layout_add(passpopup);
	elm_layout_file_set(layout, CUSTOM_POPUP_PATH, "passwd_popup");
	evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	label = elm_label_add(passpopup);
	elm_object_style_set(label, "popup/default");
	elm_label_line_wrap_set(label, ELM_WRAP_WORD);
	elm_object_text_set(label, text);
	evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_show(label);

	entry = ea_editfield_add(passpopup, EA_EDITFIELD_SINGLELINE);
	/* As per specs PIN codes may be up to 16 characters*/
	limit_filter_data.max_char_count = 16;
	elm_entry_markup_filter_append(entry, elm_entry_filter_limit_size,
				     &limit_filter_data);
	elm_entry_scrollable_set(entry, EINA_TRUE);
	elm_entry_prediction_allow_set(entry, EINA_FALSE);
	elm_entry_password_set(entry, EINA_TRUE);
	elm_entry_input_panel_layout_set(entry, ELM_INPUT_PANEL_LAYOUT_NUMBERONLY);
	evas_object_show(entry);
	ad->entry = entry;

	evas_object_smart_callback_add(entry, "changed",
				__bluetooth_entry_change_cb,
				ad);

	l_button = elm_button_add(ad->win_main);
	elm_object_style_set(l_button, "popup");
	elm_object_text_set(l_button, BT_STR_CANCEL);
	elm_object_part_content_set(passpopup, "button1", l_button);
	evas_object_smart_callback_add(l_button, "clicked", func, ad);

	r_button = elm_button_add(ad->win_main);
	elm_object_style_set(r_button, "popup");
	elm_object_text_set(r_button, BT_STR_OK);
	elm_object_part_content_set(passpopup, "button2", r_button);
	evas_object_smart_callback_add(r_button, "clicked", func, ad);
	elm_object_disabled_set(r_button, EINA_TRUE);
	ad->edit_field_save_btn = r_button;

	evas_object_event_callback_add(entry, EVAS_CALLBACK_MOUSE_UP,
			__bluetooth_input_mouseup_cb, ad);
	evas_object_event_callback_add(entry, EVAS_CALLBACK_KEY_DOWN,
			__bluetooth_input_keyback_cb, ad);

	evas_object_event_callback_add(ad->popup, EVAS_CALLBACK_MOUSE_UP,
			__bluetooth_input_mouseup_cb, ad);
	evas_object_event_callback_add(ad->popup, EVAS_CALLBACK_KEY_DOWN,
			__bluetooth_input_keyback_cb, ad);

	evas_object_event_callback_add(r_button, EVAS_CALLBACK_MOUSE_UP,
			__bluetooth_input_mouseup_cb, ad);
	evas_object_event_callback_add(r_button, EVAS_CALLBACK_KEY_DOWN,
			__bluetooth_input_keyback_cb, ad);

	evas_object_event_callback_add(l_button, EVAS_CALLBACK_MOUSE_UP,
			__bluetooth_input_mouseup_cb, ad);
	evas_object_event_callback_add(l_button, EVAS_CALLBACK_KEY_DOWN,
			__bluetooth_input_keyback_cb, ad);


	elm_object_part_content_set(layout, "entry", entry);
	elm_object_part_content_set(layout, "label", label);

	elm_object_part_text_set(entry, "elm.guide", BT_STR_TAP_TO_ENTER);

	evas_object_show(layout);
	evas_object_show(content);
	evas_object_show(passpopup);
	elm_object_content_set(passpopup, layout);
	elm_object_focus_set(entry, EINA_TRUE);
}
#endif

static void __bluetooth_delete_input_view(struct bt_popup_appdata *ad)
{
	__bluetooth_ime_hide();
}

static DBusGProxy* __bluetooth_create_agent_proxy(DBusGConnection *conn,
								const char *path)
{
	DBusGProxy *proxy;

	proxy = dbus_g_proxy_new_for_name(conn, "org.projectx.bt",
			path, "org.bluez.Agent");
	if (!proxy)
		BT_ERR("dbus_g_proxy_new_for_name is failed");

	return proxy;
}

static int __bt_get_vconf_setup_wizard()
{
       int wizard_state = VCONFKEY_SETUP_WIZARD_UNLOCK;

       if (vconf_get_int(VCONFKEY_SETUP_WIZARD_STATE, &wizard_state))
               BT_DBG("Fail to get Wizard State");

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

	BT_ERR("Event Type = %s[0X%X]", event_type, ad->event_type);

#ifdef PIN_REQUEST_FOR_BASIC_PAIRING
	if (!strcasecmp(event_type, "pin-request")) {
		device_name = bundle_get_val(kb, "device-name");
		agent_path = bundle_get_val(kb, "agent-path");

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 "%s", BT_STR_BLUETOOTH_PAIRING_REQUEST);

		snprintf(text, BT_GLOBALIZATION_STR_LENGTH,
			 BT_STR_ENTER_PIN_TO_PAIR, conv_str);

		if (conv_str)
			free(conv_str);

		/* Request user inputted PIN for basic pairing */
		__bluetooth_draw_input_view(ad, view_title, text,
					  __bluetooth_input_request_cb);
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
			 "%s", BT_STR_BLUETOOTH_PAIRING_REQUEST);

		snprintf(text, BT_GLOBALIZATION_STR_LENGTH,
			 BT_STR_ENTER_PIN_TO_PAIR, conv_str);

		if (conv_str)
			free(conv_str);

		/* Request user inputted Passkey for basic pairing */
		__bluetooth_draw_input_view(ad, view_title, text,
					  __bluetooth_input_request_cb);

	}
#endif

	if (!strcasecmp(event_type, "passkey-confirm-request")) {
		device_name = bundle_get_val(kb, "device-name");
		passkey = bundle_get_val(kb, "passkey");
		agent_path = bundle_get_val(kb, "agent-path");

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name && passkey) {
			conv_str = elm_entry_utf8_to_markup(device_name);

			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
				BT_STR_CONFIRM_PASSKEY_PS_TO_PAIR_WITH_PS, conv_str, passkey);
			BT_DBG("title: %s", passkey);

			if (conv_str)
				free(conv_str);

			__bluetooth_draw_loading_popup(ad, view_title,
					BT_STR_CANCEL, BT_STR_OK,
					__bluetooth_passkey_confirm_cb);
		} else {
			BT_ERR("wrong parameter : %s, %s", device_name, passkey);
			timeout = BT_ERROR_TIMEOUT;
		}
	} else if (!strcasecmp(event_type, "passkey-display-request")) {
		device_name = bundle_get_val(kb, "device-name");
		passkey = bundle_get_val(kb, "passkey");

		if (device_name && passkey) {
			conv_str = elm_entry_utf8_to_markup(device_name);

			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			     BT_STR_ENTER_PS_ON_PS_TO_PAIR, passkey, conv_str);

			BT_DBG("title: %s", view_title);

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

			BT_DBG("title: %s", view_title);

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
	} else if (!strcasecmp(event_type, "pairing-retry-request")) {
		DBusMessage *msg = NULL;
		int response = BT_AGENT_REJECT;

		timeout = BT_TOAST_NOTIFICATION_TIMEOUT;
		__bt_draw_toast_popup(ad, BT_STR_UNABLE_TO_CONNECT);

		msg = dbus_message_new_signal(BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
				BT_SYS_POPUP_INTERFACE,
				BT_SYS_POPUP_METHOD_RESPONSE);

		dbus_message_append_args(msg, DBUS_TYPE_INT32,
				&response, DBUS_TYPE_INVALID);

		e_dbus_message_send(ad->EDBusHandle, msg, NULL, -1, NULL);
		dbus_message_unref(msg);
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

	} else if (!strcasecmp(event_type, "factory-reset-request")) {
		device_name = bundle_get_val(kb, "device-name");
		agent_path = bundle_get_val(kb, "agent-path");

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name) {
			conv_str = elm_entry_utf8_to_markup(device_name);

			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
				 BT_STR_FACTORY_RESET, conv_str, conv_str);

			if (conv_str)
				free(conv_str);

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
	Ecore_X_Atom atom;
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
	Ecore_X_Window xwin;

	ad->popup = elm_popup_add(ad->win_main);
	elm_object_style_set(ad->popup, "toast");
	elm_popup_orient_set(ad->popup, ELM_POPUP_ORIENT_BOTTOM);
	evas_object_size_hint_weight_set(ad->popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	ea_object_event_callback_add(ad->popup, EA_CALLBACK_BACK, ea_popup_back_cb, NULL);
	elm_object_part_text_set(ad->popup,"elm.text", toast_text);

	xwin = elm_win_xwindow_get(ad->popup);
	if (xwin == 0) {
		BT_ERR("elm_win_xwindow_get is failed");
	} else {
		BT_DBG("Setting window type");
		ecore_x_netwm_window_type_set(xwin,
				ECORE_X_WINDOW_TYPE_NOTIFICATION);
		utilx_set_system_notification_level(ecore_x_display_get(),
				xwin, UTILX_NOTIFICATION_LEVEL_NORMAL);
	}

	ecore_event_handler_add(ECORE_EVENT_MOUSE_BUTTON_UP, __bt_toast_mouseup_cb, ad);

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);
	elm_object_focus_set(ad->popup, EINA_TRUE);
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

static Evas_Object *__bluetooth_create_win(const char *name)
{
	Evas_Object *eo;
	int w;
	int h;

	eo = elm_win_add(NULL, name, ELM_WIN_DIALOG_BASIC);
	if (eo) {
		elm_win_title_set(eo, name);
		elm_win_borderless_set(eo, EINA_TRUE);
		ecore_x_window_size_get(ecore_x_window_root_first_get(),
					&w, &h);
		evas_object_resize(eo, w, h);
	}

	elm_win_alpha_set(eo, EINA_TRUE);

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

static bool __bluetooth_create(void *data)
{
	struct bt_popup_appdata *ad = data;
	Evas_Object *win = NULL;

	BT_DBG("__bluetooth_create() start.\n");

	/* create window */
	win = __bluetooth_create_win(PACKAGE);
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

	__bluetooth_session_init(ad);
	if (bt_initialize() != BT_ERROR_NONE) {
		BT_ERR("bt_initialize is failed");
	}

	return true;
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

	if (ad->popup)
		evas_object_del(ad->popup);

	if (ad->win_main)
		evas_object_del(ad->win_main);

	ad->popup = NULL;
	ad->win_main = NULL;
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

static void __bluetooth_reset(service_h service, void *user_data)
{
	struct bt_popup_appdata *ad = user_data;
	bundle *b = NULL;
	const char *event_type = NULL;
	int ret = 0;

	BT_DBG("__bluetooth_reset()");

	if (ad == NULL) {
		BT_ERR("App data is NULL");
		return;
	}

	ret = service_to_bundle(service, &b);

	/* Start Main UI */
	event_type = bundle_get_val(b, "event-type");
	if (event_type == NULL) {
		BT_ERR("event type is NULL");
		return;
	}
	BT_ERR("event_type : %s", event_type);

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
		ad->win_main = __bluetooth_create_win(PACKAGE);
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

	/* Change LCD brightness */
	if (display_change_state(LCD_NORMAL) != 0)
		BT_ERR("Fail to change LCD");

	if (ret != 0) {
		BT_ERR("__bluetooth_launch_handler is failed. event[%d], ret[%d]",
				ad->event_type, ret);
		__bluetooth_remove_all_event(ad);
	}

	if (ad->event_type == BT_EVENT_HANDSFREE_DISCONNECT_REQUEST) {
		__bluetooth_notify_event(FEEDBACK_PATTERN_DISCONNECTED);
	} else if (ad->event_type == BT_EVENT_HANDSFREE_CONNECT_REQUEST) {
		__bluetooth_notify_event(FEEDBACK_PATTERN_CONNECTED);
	} else if (ad->event_type == BT_EVENT_PASSKEY_CONFIRM_REQUEST ||
		   ad->event_type == BT_EVENT_FACTORY_RESET_REQUEST) {
		__bluetooth_notify_event(FEEDBACK_PATTERN_BT_PAIRING);
		ad->viberation_id = g_timeout_add(BT_VIBERATION_INTERVAL,
						  __bluetooth_pairing_pattern_cb, NULL);
		__lock_display();
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
	event_callback.service = __bluetooth_reset;
	event_callback.low_memory = NULL;
	event_callback.low_battery = NULL;
	event_callback.device_orientation = NULL;
	event_callback.language_changed = __bluetooth_lang_changed_cb;
	event_callback.region_format_changed = NULL;

	return app_efl_main(&argc, &argv, &event_callback, &ad);
}
