package com.pro.marvell;

import android.app.Dialog;
import android.content.Context;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.TextView;

public class CustomizeAlertDialog extends Dialog implements android.view.View.OnClickListener {

Context mContext;
TextView mTitle = null;
TextView mMessage = null;
View v = null;
Button alertBtn;
//ProgressBar progressBar;

public CustomizeAlertDialog(Context context) {
    super(context);
    mContext = context;
    /** 'Window.FEATURE_NO_TITLE' - Used to hide the mTitle */
    requestWindowFeature(Window.FEATURE_NO_TITLE);
    /** Design the dialog in main.xml file */
    setContentView(R.layout.customalertprogress);
    v = getWindow().getDecorView();
    v.setBackgroundResource(android.R.color.transparent);
 //   mTitle = (TextView) findViewById(R.id.dialogTitle);
   // progressBar=(ProgressBar)findViewById(R.id.loading_icon);
    mMessage = (TextView) findViewById(R.id.loading_text);
    alertBtn=(Button)findViewById(R.id.alertbtn);
   // okButton = (Button) findViewById(R.id.OkButton);
    alertBtn.setOnClickListener(this);
    
}

@Override
public void setTitle(CharSequence title) {
    super.setTitle(title);
    mTitle.setText(title);
}

@Override
public void setTitle(int titleId) {
    super.setTitle(titleId);
    mTitle.setText(mContext.getResources().getString(titleId));
}

/**
 * Set the message text for this dialog's window.
 * 
 * @param message
 *            - The new message to display in the title.
 */
public void setMessage(CharSequence message) {
    mMessage.setText(message);
    mMessage.setGravity(Gravity.CENTER);
    mMessage.setMovementMethod(ScrollingMovementMethod.getInstance());
}

/**
 * Set the message ID
 * 
 * @param messageId
 */
public void setMessage(int messageId) {
    mMessage.setText(mContext.getResources().getString(messageId));
    mMessage.setMovementMethod(ScrollingMovementMethod.getInstance());
}

@Override
public void onClick(View v) {
	Log.d("In customer Alert", "In customer Alert");
	// TODO Auto-generated method stub
	
}


}