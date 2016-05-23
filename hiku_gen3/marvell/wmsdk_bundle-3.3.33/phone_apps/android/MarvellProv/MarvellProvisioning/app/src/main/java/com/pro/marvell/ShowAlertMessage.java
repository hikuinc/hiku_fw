package com.pro.marvell;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.AlertDialog.Builder;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.text.Html;
import android.util.Log;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.animation.AnimationUtils;
import android.widget.TextView;

public class ShowAlertMessage {
	private static Builder alertDialogBuilder;
	private static AlertDialog alertBox;

	public static void hideAlert() {
		if (alertBox != null && alertBox.isShowing()) {
			alertBox.hide();
			alertBox.cancel();
			alertBox.dismiss();
		}
	}

	private static void showPopup(String msg, String btnTxt,
			OnClickListener listner, final Activity act) {
		alertDialogBuilder = new AlertDialog.Builder(new ContextThemeWrapper(
				act, android.R.style.Theme_Holo_Light_Dialog_NoActionBar));

		alertDialogBuilder
				.setMessage(Html.fromHtml("<center>" + msg + "</center>"))
				.setCancelable(false)
				.setView(
						act.getLayoutInflater().inflate(R.layout.custom_alert,
								null)).setPositiveButton(btnTxt, listner);
		alertBox = alertDialogBuilder.create();
		alertBox.show();
	}

	private static void showPopup(String msg, String btnTxt,
			OnClickListener listner, String title, final Activity act) {
		alertDialogBuilder = new AlertDialog.Builder(new ContextThemeWrapper(
				act, android.R.style.Theme_Holo_Light_Dialog_NoActionBar));

		alertDialogBuilder
				.setTitle(title)
				.setMessage(Html.fromHtml("<center>" + msg + "</center>"))
				.setCancelable(false)
				.setView(
						act.getLayoutInflater().inflate(R.layout.custom_alert,
								null)).setPositiveButton(btnTxt, listner);
		alertBox = alertDialogBuilder.create();
		alertBox.show();
	}
	
	private static void showPopup(String msg, String btnTxt1, String btnTxt2,
			OnClickListener listner1, OnClickListener listner2,
			final Activity act) {
		alertDialogBuilder = new AlertDialog.Builder(new ContextThemeWrapper(
				act, android.R.style.Theme_Holo_Light_Dialog_NoActionBar));

		alertDialogBuilder
				.setMessage(Html.fromHtml("<center>" + msg + "</center>"))
				.setCancelable(false)
				.setView(
						act.getLayoutInflater().inflate(R.layout.custom_alert,
								null)).setPositiveButton(btnTxt1, listner1)
				.setNegativeButton(btnTxt2, listner2);

		alertBox = alertDialogBuilder.create();
		alertBox.show();

	}

	static Runnable createPopUp(final String msg, final String btnTxt,
			final OnClickListener listner, final Activity act) {
		Runnable aRunnable = new Runnable() {
			public void run() {
				showPopup(msg, btnTxt, listner, act);
			}
		};
		return aRunnable;

	}

	static Runnable createPopUp(final String msg, final String btnTxt,
			final OnClickListener listner, final String title, final Activity act) {
		Runnable aRunnable = new Runnable() {
			public void run() {
				showPopup(msg, btnTxt, listner, title, act);
			}
		};
		return aRunnable;

	}
	
	static Runnable createPopUp(final String msg, final String btnTxt1,
			final String btnTxt2, final OnClickListener listner1,
			final OnClickListener listner2, final Activity act) {
		Runnable aRunnable = new Runnable() {
			public void run() {
				showPopup(msg, btnTxt1, btnTxt2, listner1, listner2, act);
			}
		};
		return aRunnable;

	}

	public static void showPopupAlert(String msg, String btnTxt,
			OnClickListener listner, final Activity act) {
		act.runOnUiThread(createPopUp(msg, btnTxt, listner, act));
	}

	public static void showPopupAlert(String msg, String btnTxt,
			OnClickListener listner, String title, final Activity act) {
		act.runOnUiThread(createPopUp(msg, btnTxt, listner, title, act));
	}
	
	public static void showPopupAlert(String msg, String btnTxt1,
			OnClickListener listner1, String btnTxt2, OnClickListener listner2,
			final Activity act) {
		act.runOnUiThread(createPopUp(msg, btnTxt1, btnTxt2, listner1,
				listner2, act));
	}
}
