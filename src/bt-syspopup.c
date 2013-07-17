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

#include <stdio.h>
#include <pmapi.h>
#include <appcore-efl.h>
#include <Ecore_X.h>
#include <utilX.h>
#include <vconf.h>
#include <vconf-keys.h>
#include <syspopup.h>
#include <E_DBus.h>
#include <aul.h>
#include <bluetooth-api.h>
#include <feedback.h>

#include "bt-syspopup.h"

static void __bluetooth_delete_input_view(struct bt_popup_appdata *ad);
static void __bluetooth_win_del(void *data);

static int __bluetooth_term(bundle *b, void *data)
{
	BT_DBG("System-popup: terminate");
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
	if (ad == NULL)
		return;

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
}

static void __bluetooth_notify_event(void)
{
	int result;

	BT_DBG("Notify event");

	result = feedback_initialize();
	if (result != FEEDBACK_ERROR_NONE) {
		BT_ERR("feedback_initialize error : %d", result);
		return;
	}

	result = feedback_play(FEEDBACK_PATTERN_GENERAL);
	BT_DBG("ret value : %d", result);

	result = feedback_deinitialize();
	if (result != FEEDBACK_ERROR_NONE) {
		BT_DBG("feedback_initialize error : %d", result);
		return;
	}
}

static void __bluetooth_parse_event(struct bt_popup_appdata *ad, const char *event_type)
{
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
	else
		ad->event_type = 0x0000;
		return;
}

static void __bluetooth_request_to_cancel(void)
{
	bluetooth_cancel_bonding();
}

static void __bluetooth_remove_all_event(struct bt_popup_appdata *ad)
{
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
			DBusMessage *msg = NULL;
			int response = 2;

			msg = dbus_message_new_signal(
					BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
					BT_SYS_POPUP_INTERFACE,
					BT_SYS_POPUP_METHOD_RESPONSE);

			/* For timeout rejection is sent to  be handled in
			   application */
			response = 1;

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

		dbus_g_proxy_call_no_reply(ad->obex_proxy,
					   "ReplyAuthorize",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);

		break;

	case BT_EVENT_CONFIRM_OVERWRITE_REQUEST:

		dbus_g_proxy_call_no_reply(ad->obex_proxy,
					   "ReplyOverwrite",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);

		break;

	default:
		break;
	}

	__bluetooth_win_del(ad);
}

static int __bluetooth_keydown_cb(void *data, int type, void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	Ecore_Event_Key *ev = event_info;

	if (!g_strcmp0(ev->keyname, KEY_END) || !g_strcmp0(ev->keyname, KEY_SELECT)) {
		BT_DBG("Key [%s]", ev->keyname);
		/* remove_all_event(); */

		if (!g_strcmp0(ev->keyname, KEY_END)) {
			__bluetooth_remove_all_event(ad);
		}
	}

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
		response = 1;
	else
		response = 0;

	if (convert_input_text == NULL)
		return;

	BT_DBG("PIN/Passkey[%s] event[%d] response[%d]",
		     convert_input_text, ad->event_type, response);

	if (response == 1) {
		BT_DBG("Done case");
		if (ad->event_type == BT_EVENT_PIN_REQUEST) {
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
						   "ReplyPinCode",
						   G_TYPE_UINT, BT_AGENT_ACCEPT,
						   G_TYPE_STRING,
						   convert_input_text,
						   G_TYPE_INVALID,
						   G_TYPE_INVALID);
		} else {
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
						   "ReplyPasskey",
						   G_TYPE_UINT, BT_AGENT_ACCEPT,
						   G_TYPE_STRING,
						   convert_input_text,
						   G_TYPE_INVALID,
						   G_TYPE_INVALID);
		}
	} else {
		BT_DBG("Cancel case");
		if (ad->event_type == BT_EVENT_PIN_REQUEST) {
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
						   "ReplyPinCode",
						   G_TYPE_UINT, BT_AGENT_CANCEL,
						   G_TYPE_STRING, "",
						   G_TYPE_INVALID,
						   G_TYPE_INVALID);
		} else {
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
						   "ReplyPasskey",
						   G_TYPE_UINT, BT_AGENT_CANCEL,
						   G_TYPE_STRING, "",
						   G_TYPE_INVALID,
						   G_TYPE_INVALID);
		}
	}

	__bluetooth_delete_input_view(ad);

	free(convert_input_text);

	__bluetooth_win_del(ad);
}

static void __bluetooth_input_cancel_cb(void *data,
				       Evas_Object *obj, void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;

	bluetooth_cancel_bonding();

	__bluetooth_win_del(ad);
}

static void __bluetooth_passkey_confirm_cb(void *data,
					 Evas_Object *obj, void *event_info)
{
	if (obj == NULL || data == NULL)
		return;

	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	const char *event = elm_object_text_get(obj);

	if (!g_strcmp0(event, BT_STR_OK)) {
		dbus_g_proxy_call_no_reply(ad->agent_proxy, "ReplyConfirmation",
					   G_TYPE_UINT, BT_AGENT_ACCEPT,
					   G_TYPE_INVALID, G_TYPE_INVALID);
	} else {
		dbus_g_proxy_call_no_reply(ad->agent_proxy, "ReplyConfirmation",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);
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
	} else {
		BT_DBG("e_dbus_bus_get success \n ");
		e_dbus_request_name(ad->EDBusHandle,
				    BT_SYS_POPUP_IPC_NAME, 0, NULL, NULL);
	}
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

	if (!g_strcmp0(event, BT_STR_YES) || !g_strcmp0(event, BT_STR_OK))
		response = 0;
	else
		response = 1;

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
	if (obj == NULL || ad == NULL)
		return;

	const char *event = elm_object_text_get(obj);

	if (!g_strcmp0(event, BT_STR_YES)) {
		dbus_g_proxy_call_no_reply(ad->agent_proxy, "ReplyAuthorize",
					   G_TYPE_UINT, BT_AGENT_ACCEPT,
					   G_TYPE_INVALID, G_TYPE_INVALID);
	} else {
		dbus_g_proxy_call_no_reply(ad->agent_proxy, "ReplyAuthorize",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);
	}

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

	if (!g_strcmp0(event, BT_STR_YES))
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
				elm_object_signal_emit(ad->editfield,
							"elm,state,eraser,hide",
							"elm");
			} else {
				elm_object_disabled_set(ad->edit_field_save_btn,
							EINA_FALSE);
				elm_object_signal_emit(ad->editfield,
							"elm,state,eraser,show",
							"elm");
			}

			if (ad->event_type == BT_EVENT_PASSKEY_REQUEST) {
				if (text_length > BT_PK_MLEN) {
					text_length = BT_PK_MLEN;
					convert_input_text[BT_PK_MLEN] = '\0';
					output_text =
					    elm_entry_utf8_to_markup
					    (convert_input_text);

					elm_entry_entry_set(obj, output_text);
					elm_entry_cursor_end_set(obj);
					free(output_text);
				}
			} else {
				if (text_length > BT_PIN_MLEN) {
					text_length = BT_PIN_MLEN;
					convert_input_text[BT_PIN_MLEN] = '\0';
					output_text =
					    elm_entry_utf8_to_markup
						(convert_input_text);

					elm_entry_entry_set(obj, output_text);
					elm_entry_cursor_end_set(obj);
					free(output_text);
				}
			}
			free(convert_input_text);
		}
	}
}

static void __bluetooth_entry_focused_cb(void *data, Evas_Object *obj,
				      void *event_info)
{
	if (!elm_entry_is_empty(obj))
		elm_object_signal_emit(data, "elm,state,eraser,show", "elm");

	elm_object_signal_emit(data, "elm,state,guidetext,hide", "elm");
}

static void __bluetooth_entry_unfocused_cb(void *data, Evas_Object *obj,
				      void *event_info)
{
	if (elm_entry_is_empty(obj))
		elm_object_signal_emit(data, "elm,state,guidetext,show", "elm");

	elm_object_signal_emit(data, "elm,state,eraser,hide", "elm");
}

static void __bluetooth_eraser_clicked_cb(void* data, Evas_Object* obj,
				const char* emission, const char* source)
{
	elm_entry_entry_set(data, "");
}

static void __bluetooth_check_chagned_cb(void *data, Evas_Object *obj,
				      void *event_info)
{
	Eina_Bool state = EINA_FALSE;

        if (obj == NULL)
                return;

        state = elm_check_state_get(obj);
        elm_entry_password_set(data, !state);
}

static void __bluetooth_draw_popup(struct bt_popup_appdata *ad,
			const char *title, char *btn1_text,
			char *btn2_text, void (*func) (void *data,
			Evas_Object *obj, void *event_info))
{
	char temp_str[BT_TITLE_STR_MAX_LEN+BT_TEXT_EXTRA_LEN] = { 0 };
	Evas_Object *btn1;
	Evas_Object *btn2;

	BT_DBG("__bluetooth_draw_popup");

	ad->popup = elm_popup_add(ad->win_main);
	evas_object_size_hint_weight_set(ad->popup, EVAS_HINT_EXPAND,
					 EVAS_HINT_EXPAND);

	elm_object_style_set(ad->popup, "transparent");

	if (title != NULL) {
		snprintf(temp_str, BT_TITLE_STR_MAX_LEN+BT_TEXT_EXTRA_LEN,
			"<align=center>%s</align>", title);
		elm_object_text_set(ad->popup, temp_str);
	}

	if ((btn1_text != NULL) && (btn2_text != NULL)) {
		btn1 = elm_button_add(ad->popup);
		elm_object_style_set(btn1, "popup_button/default");
		elm_object_text_set(btn1, btn1_text);
		elm_object_part_content_set(ad->popup, "button1", btn1);
		evas_object_smart_callback_add(btn1, "clicked", func, ad);

		btn2 = elm_button_add(ad->popup);
		elm_object_style_set(btn2, "popup_button/default");
		elm_object_text_set(btn2, btn2_text);
		elm_object_part_content_set(ad->popup, "button2", btn2);
		evas_object_smart_callback_add(btn2, "clicked", func, ad);
	} else if (btn1_text != NULL) {
		btn1 = elm_button_add(ad->popup);
		elm_object_style_set(btn1, "popup_button/default");
		elm_object_text_set(btn1, btn1_text);
		elm_object_part_content_set(ad->popup, "button1", btn1);
		evas_object_smart_callback_add(btn1, "clicked", func, ad);
	}

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);

	BT_DBG("__bluetooth_draw_popup END");
}

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
	Evas_Object *editfield = NULL;
	Evas_Object *entry = NULL;
	Evas_Object *check = NULL;
	Evas_Object *l_button = NULL;
	Evas_Object *r_button = NULL;

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
	elm_layout_file_set(layout, CUSTOM_POPUP_PATH, "popup_checkview_image");
	evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	label = elm_label_add(passpopup);
	elm_object_style_set(label, "popup/default");
	elm_label_line_wrap_set(label, ELM_WRAP_CHAR);
	elm_object_text_set(label, text);
	evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_show(label);

	editfield = elm_layout_add(passpopup);
	elm_layout_theme_set(editfield, "layout", "editfield", "default");
	evas_object_size_hint_weight_set(editfield, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(editfield, EVAS_HINT_FILL, EVAS_HINT_FILL);
	ad->editfield = editfield;

	entry = elm_entry_add(passpopup);
	elm_entry_single_line_set(entry, EINA_TRUE);
	elm_entry_scrollable_set(entry, EINA_TRUE);
	elm_entry_prediction_allow_set(entry, EINA_FALSE);
	elm_entry_password_set(entry, TRUE);
	elm_entry_input_panel_layout_set(entry, ELM_INPUT_PANEL_LAYOUT_NUMBERONLY);
	elm_object_part_content_set(editfield, "elm.swallow.content", entry);
	ad->entry = entry;

	evas_object_smart_callback_add(entry, "changed",
				__bluetooth_entry_change_cb,
				ad);

	evas_object_smart_callback_add(entry, "focused",
				__bluetooth_entry_focused_cb,
				editfield);

	evas_object_smart_callback_add(entry, "unfocused",
				__bluetooth_entry_unfocused_cb,
				editfield);

	elm_object_signal_callback_add(editfield, "elm,eraser,clicked", "elm",
				(Edje_Signal_Cb)__bluetooth_eraser_clicked_cb,
				entry);

	evas_object_show(entry);
	evas_object_show(editfield);

	check = elm_check_add(passpopup);
	elm_object_text_set(check, BT_STR_SHOW_PASSWORD);
	elm_object_focus_allow_set(check, EINA_FALSE);
	evas_object_size_hint_weight_set(check, EVAS_HINT_EXPAND,
					EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(check, EVAS_HINT_FILL,
					EVAS_HINT_FILL);
	evas_object_smart_callback_add(check, "changed",
				__bluetooth_check_chagned_cb, entry);
	evas_object_show(check);

	r_button = elm_button_add(ad->win_main);
	elm_object_style_set(r_button, "popup_button/default");
	elm_object_text_set(r_button, BT_STR_CANCEL);
	elm_object_part_content_set(passpopup, "button1", r_button);
	evas_object_smart_callback_add(r_button, "clicked", func, ad);

	l_button = elm_button_add(ad->win_main);
	elm_object_style_set(l_button, "popup_button/default");
	elm_object_text_set(l_button, BT_STR_OK);
	elm_object_part_content_set(passpopup, "button2", l_button);
	evas_object_smart_callback_add(l_button, "clicked", func, ad);
	elm_object_disabled_set(l_button, EINA_TRUE);

	ad->edit_field_save_btn = l_button;

	elm_object_part_content_set(layout, "elm.swallow.content", label);
	elm_object_part_content_set(layout, "elm.swallow.entry", editfield);
	elm_object_part_content_set(layout, "elm.swallow.end", check);

	evas_object_show(layout);
	evas_object_show(content);
	evas_object_show(passpopup);
	elm_object_content_set(passpopup, layout);
	elm_object_focus_set(entry, EINA_TRUE);
}

static void __bluetooth_delete_input_view(struct bt_popup_appdata *ad)
{
	__bluetooth_ime_hide();
}

static DBusGProxy* __bluetooth_create_agent_proxy(DBusGConnection *conn,
								const char *path)
{
	return dbus_g_proxy_new_for_name(conn, "org.projectx.bt", path,
							"org.bluez.Agent");
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

	BT_DBG("__bluetooth_launch_handler");

	if (!reset_data || !event_type)
		return -1;

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
	} else if (!strcasecmp(event_type, "passkey-confirm-request")) {
		device_name = bundle_get_val(kb, "device-name");
		passkey = bundle_get_val(kb, "passkey");
		agent_path = bundle_get_val(kb, "agent-path");

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name && passkey) {
			conv_str = elm_entry_utf8_to_markup(device_name);

			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			     BT_STR_CONFIRM_PASSKEY_PS_TO_PAIR_WITH_PS,
			     conv_str, passkey);

			BT_DBG("title: %s", view_title);

			if (conv_str)
				free(conv_str);

			__bluetooth_draw_popup(ad, view_title,
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
			 "%s", BT_STR_BLUETOOTH_PAIRING_REQUEST);

		snprintf(text, BT_GLOBALIZATION_STR_LENGTH,
			 BT_STR_ENTER_PIN_TO_PAIR, conv_str);

		if (conv_str)
			free(conv_str);

		/* Request user inputted Passkey for basic pairing */
		__bluetooth_draw_input_view(ad, view_title, text,
					  __bluetooth_input_request_cb);

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

		__bluetooth_draw_popup(ad, view_title, BT_STR_NO, BT_STR_YES,
				     __bluetooth_authorization_request_cb);
	} else if (!strcasecmp(event_type, "app-confirm-request")) {
		BT_DBG("app-confirm-request");
		timeout = BT_AUTHORIZATION_TIMEOUT;

		const char *title = NULL;
		const char *type = NULL;

		title = bundle_get_val(kb, "title");
		type = bundle_get_val(kb, "type");

		if (!title)
			return -1;

		if (strcasecmp(type, "twobtn") == 0) {
			__bluetooth_draw_popup(ad, title, BT_STR_NO, BT_STR_YES,
					     __bluetooth_app_confirm_cb);
		} else if (strcasecmp(type, "onebtn") == 0) {
			timeout = BT_NOTIFICATION_TIMEOUT;
			__bluetooth_draw_popup(ad, title, BT_STR_YES, NULL,
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

		__bluetooth_draw_popup(ad, view_title, BT_STR_NO, BT_STR_YES,
				__bluetooth_push_authorization_request_cb);
	} else if (!strcasecmp(event_type, "confirm-overwrite-request")) {
		timeout = BT_AUTHORIZATION_TIMEOUT;

		file = bundle_get_val(kb, "file");

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 BT_STR_OVERWRITE_FILE_Q, file);

		__bluetooth_draw_popup(ad, view_title, BT_STR_NO, BT_STR_YES,
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
			if (strlen(title) > 255)
				return -1;
		} else
			return -1;

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
			 BT_STR_EXCHANGE_OBJECT_WITH_PS_Q, conv_str);

		if (conv_str)
			free(conv_str);

		__bluetooth_draw_popup(ad, view_title, BT_STR_NO, BT_STR_YES,
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

		__bluetooth_draw_popup(ad, view_title, BT_STR_NO, BT_STR_YES,
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

		__bluetooth_draw_popup(ad, view_title, BT_STR_NO, BT_STR_YES,
				     __bluetooth_authorization_request_cb);
	} else {
		return -1;
	}

	if (ad->event_type != BT_EVENT_FILE_RECEIVED && timeout != 0) {
		ad->timer = ecore_timer_add(timeout, (Ecore_Task_Cb)
					__bluetooth_request_timeout_cb,
					ad);
	}

	return 0;
}

static void __bluetooth_win_del(void *data)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;

	__bluetooth_cleanup(ad);

	elm_exit();
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

static int __bluetooth_create(void *data)
{
	struct bt_popup_appdata *ad = data;
	Evas_Object *win = NULL;

	BT_DBG("__bluetooth_create() start.\n");

	/* create window */
	win = __bluetooth_create_win(PACKAGE);
	if (win == NULL)
		return -1;
	ad->win_main = win;

	/* init internationalization */
	if (appcore_set_i18n(BT_COMMON_PKG, BT_COMMON_RES) < 0)
		return -1;

	ecore_imf_init();
	ad->event_handle = ecore_event_handler_add(ECORE_EVENT_KEY_DOWN,
						   (Ecore_Event_Handler_Cb)
						   __bluetooth_keydown_cb,
						   ad);

	__bluetooth_session_init(ad);

	return 0;
}

static int __bluetooth_terminate(void *data)
{
	struct bt_popup_appdata *ad = data;

	__bluetooth_ime_hide();

	if (ad->conn) {
		dbus_g_connection_unref(ad->conn);
		ad->conn = NULL;
	}

	if (ad->event_handle)
		ecore_event_handler_del(ad->event_handle);

	if (ad->popup)
		evas_object_del(ad->popup);

	if (ad->win_main)
		evas_object_del(ad->win_main);

	ad->popup = NULL;
	ad->win_main = NULL;

	return 0;
}

static int __bluetooth_pause(void *data)
{

	return 0;
}

static int __bluetooth_resume(void *data)
{

	return 0;
}

static int __bluetooth_reset(bundle *b, void *data)
{
	struct bt_popup_appdata *ad = data;
	const char *event_type = NULL;
	int ret = 0;

	BT_DBG("__bluetooth_reset()\n");

	if (ad == NULL) {
		BT_ERR("App data is NULL\n");
		return -1;
	}

	/* Start Main UI */
	event_type = bundle_get_val(b, "event-type");

	if (event_type != NULL) {
		if (!strcasecmp(event_type, "terminate")) {
			__bluetooth_win_del(ad);
			return 0;
		}

		if (syspopup_has_popup(b)) {
			/* Destroy the existing popup*/
			__bluetooth_cleanup(ad);
			/* create window */
			ad->win_main = __bluetooth_create_win(PACKAGE);
			if (ad->win_main == NULL)
				return -1;
		}

		__bluetooth_parse_event(ad, event_type);

		elm_win_alpha_set(ad->win_main, EINA_TRUE);

		ret = syspopup_create(b, &handler, ad->win_main, ad);
		if (ret == -1) {
			BT_ERR("syspopup_create err");
			__bluetooth_remove_all_event(ad);
		} else {
			ret = __bluetooth_launch_handler(ad,
						       b, event_type);

			if (ret != 0)
				__bluetooth_remove_all_event(ad);

			__bluetooth_notify_event();

			/* Change LCD brightness */
			ret = pm_change_state(LCD_NORMAL);
			if (ret != 0)
				BT_ERR("Fail to change LCD");
		}
	} else {
		BT_ERR("event type is NULL \n");
	}

	return 0;
}

EXPORT int main(int argc, char *argv[])
{
	struct bt_popup_appdata ad;
	struct appcore_ops ops = {
		.create = __bluetooth_create,
		.terminate = __bluetooth_terminate,
		.pause = __bluetooth_pause,
		.resume = __bluetooth_resume,
		.reset = __bluetooth_reset,
	};

	memset(&ad, 0x0, sizeof(struct bt_popup_appdata));
	ops.data = &ad;

	return appcore_efl_main(PACKAGE, &argc, &argv, &ops);
}

