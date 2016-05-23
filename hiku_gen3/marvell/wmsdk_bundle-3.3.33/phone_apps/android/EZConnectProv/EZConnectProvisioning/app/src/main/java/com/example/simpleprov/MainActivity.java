package com.example.simpleprov;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.StreamCorruptedException;
import java.net.DatagramPacket;
import java.net.InetAddress;
import java.net.MulticastSocket;
import java.net.UnknownHostException;
import java.security.AlgorithmParameters;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.spec.AlgorithmParameterSpec;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.KeySpec;
import java.util.HashMap;
import java.util.Map;
import java.util.zip.CRC32;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;
import javax.crypto.SecretKeyFactory;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.PBEKeySpec;
import javax.crypto.spec.SecretKeySpec;

import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.MulticastLock;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.text.InputType;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.Window;
import android.view.View.OnClickListener;
import android.view.animation.TranslateAnimation;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

public class MainActivity extends Activity implements OnClickListener {

	Button BtnStartStop;
	EditText EditSSID;
	static EditText EditPass;
	static EditText EditDeviceKey;
	static TextView TxtDebug;
	static CheckBox MaskPassphrase;
	static CheckBox RememberPassphrase;
	WifiManager wifiMgr;
	WifiInfo wifiInf;
	Boolean xmitStarted = false;
	xmitterTask xmitter;
	static HashMap<String, String> passhash = new HashMap<String, String>();

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		setContentView(R.layout.activity_main);
		setContentView(R.layout.activity_main);
		BtnStartStop = (Button) findViewById(R.id.button1);
		EditSSID = (EditText) findViewById(R.id.ssid);
		EditPass = (EditText) findViewById(R.id.editText1);
		EditDeviceKey = (EditText) findViewById(R.id.editText2);
		TxtDebug = (TextView) findViewById(R.id.textView2);
		MaskPassphrase = (CheckBox) findViewById(R.id.unmaskPassphrase);
		RememberPassphrase = (CheckBox) findViewById(R.id.rememberCheckbox);
		HashMapInit();
	}

    public static byte[] myEncrypt(String key, byte[] plainText, String ssid) {

        byte[] iv = new byte[16];
        for (int i = 0; i < 16; i++)
            iv[i] = 0;

        IvParameterSpec ivSpec = new IvParameterSpec(iv);

        Cipher cipher;
        byte[] encrypted = null;
        try {
            int iterationCount = 4096;
            int keyLength = 256;
            //int saltLength = keyLength / 8;
            byte salt[] = ssid.getBytes();

            Log.d("MRVL", "key salt itercount " + key + " " + ssid + " " + iterationCount);
            KeySpec keySpec = new PBEKeySpec(key.toCharArray(), salt, iterationCount, keyLength);

            SecretKeyFactory keyFactory = SecretKeyFactory.getInstance("PBKDF2WithHmacSHA1");

            byte[] keyBytes = keyFactory.generateSecret(keySpec).getEncoded();
            SecretKeySpec skeySpec = new SecretKeySpec(keyBytes, "AES");

            cipher = Cipher.getInstance("AES/CBC/PKCS7Padding");
            cipher.init(Cipher.ENCRYPT_MODE, skeySpec, ivSpec);
            encrypted = cipher.doFinal(plainText);
        } catch (InvalidKeySpecException e) {
            e.printStackTrace();
        } catch (NoSuchAlgorithmException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (NoSuchPaddingException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (InvalidKeyException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (IllegalBlockSizeException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (BadPaddingException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (InvalidAlgorithmParameterException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return encrypted;
	}

	public static String toHex(byte[] buf) {
		if (buf == null)
			return "";
		StringBuffer result = new StringBuffer(2 * buf.length);
		for (int i = 0; i < buf.length; i++) {
			appendHex(result, buf[i]);
		}
		return result.toString();
	}

	private final static String HEX = "0123456789ABCDEF";

	private static void appendHex(StringBuffer sb, byte b) {
		sb.append(HEX.charAt((b >> 4) & 0x0f)).append(HEX.charAt(b & 0x0f));
	}

	private void HashMapInit() {
		try {
			File toRead = new File("DoNotDeleteThis");
			FileInputStream fileInputStream = new FileInputStream(toRead);
			ObjectInputStream objectInputStream = new ObjectInputStream(
					fileInputStream);
			passhash = (HashMap<String, String>)objectInputStream.readObject();
			objectInputStream.close();
			fileInputStream.close();

		} catch (FileNotFoundException e) {
			// Lets create the file then

			Log.d("FIleNotFound", "Create New File");
			try {
				FileOutputStream fileOutputStream = openFileOutput(
						"DoNotDeleteThis", Context.MODE_PRIVATE);
				ObjectOutputStream objectOutputStream;
				objectOutputStream = new ObjectOutputStream(fileOutputStream);
				objectOutputStream.writeObject("");
				objectOutputStream.close();
			} catch (IOException e1) {
				Log.d("FileNotCreated", "FileNotCreated");
				// TODO Auto-generated catch block
				e1.printStackTrace();
			}

		} catch (StreamCorruptedException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (ClassNotFoundException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}

	private void saveToPhoneMemory(String ssid, String passphrase) {
		FileOutputStream fileOutputStream;
		try {
			if (passphrase == "" || passphrase == null)
				passhash.remove(ssid);
			else
				passhash.put(ssid, passphrase);
			fileOutputStream = openFileOutput("DoNotDeleteThis",
					Context.MODE_PRIVATE);
			ObjectOutputStream objectOutputStream = new ObjectOutputStream(
					fileOutputStream);
			objectOutputStream.writeObject(passhash);
			objectOutputStream.close();

		} catch (FileNotFoundException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}

	}

	private static String getPassphrase(String ssid) {
		return passhash.get(ssid);
	}

	private void rememberPass(View v) {

		if (((CheckBox) v).isChecked()) {
			saveToPhoneMemory(EditSSID.getText().toString(), EditPass.getText()
					.toString());
		} else {
			saveToPhoneMemory(EditSSID.getText().toString(), "");
		}

	}

	protected void onResume() {
		super.onResume();
		BtnStartStop.setEnabled(false);
		BtnStartStop.setOnClickListener(this);
		MaskPassphrase.setOnClickListener(new OnClickListener() {

			@Override
			public void onClick(View v) {
				if (((CheckBox) v).isChecked()) {
					EditPass.setInputType(InputType.TYPE_TEXT_VARIATION_PASSWORD);
				} else {
					EditPass.setInputType(129);
				}

			}
		});

		RememberPassphrase.setOnClickListener(new OnClickListener() {

			@Override
			public void onClick(View v) {
				// TODO Auto-generated method stub
				rememberPass(v);

			}
		});
		TxtDebug.setText("App Started");
		checkWiFi();
	}

	private static boolean validate(String pass, String key) {

		 if ((pass.length() != 0) && (pass.length() < 8 || pass.length() > 63)) {
			TxtDebug.setText("Invalid passphrase. Passphrase must be 8 to 63 characters long.");
			return false;
		}
		if (key.length() > 16 && key.length() < 8) {
			TxtDebug.setText("Invalid key. Key must be 8 to 16 characters long.");
			return false;
		}
		return true;
	}

	final Handler handler = new Handler() {
		@Override
		public void handleMessage(Message msg) {
			if (msg.what == 42) {
				Log.d("MRVL", "ADI ASync task exited");
				xmitStarted = false;
				BtnStartStop.setText("Start");
				TxtDebug.setText("Please check indicators on the device.\n The device should have been provisioned.\n If not, please retry.");
			} else if (msg.what == 43) {
				TxtDebug.setText("Information sent " + msg.arg1 / 2 + " times.");
			}
			super.handleMessage(msg);
		}
	};

	public void onClick(View v) {
		Log.d("MRVL", "Btn clicked");

		String pass = EditPass.getText().toString();
		String key = EditDeviceKey.getText().toString();

		if (RememberPassphrase.isChecked()) {
			saveToPhoneMemory(EditSSID.getText().toString(), EditPass.getText()
					.toString());
		} else {
			saveToPhoneMemory(EditSSID.getText().toString(), "");
		}

		try {
			if (xmitStarted == false) {

				if (!validate(pass, key))
					return;
				xmitter = new xmitterTask();
				xmitter.handler = handler;

				xmitStarted = true;
				BtnStartStop.setText("Stop");

				CRC32 crc32 = new CRC32();
				crc32.reset();
				crc32.update(pass.getBytes());
				xmitter.passCRC = (int) crc32.getValue() & 0xffffffff;
				Log.d("MRVL", Integer.toHexString(xmitter.passCRC));

                xmitter.ssid = wifiMgr.getConnectionInfo().getSSID();
                xmitter.ssidLen = wifiMgr.getConnectionInfo().getSSID().length();

                int deviceVersion = Build.VERSION.SDK_INT;

                if (deviceVersion >= 17) {
                    if (xmitter.ssid.startsWith("\"") && xmitter.ssid.endsWith("\"")) {
                        xmitter.ssidLen = wifiMgr.getConnectionInfo().getSSID().length() - 2;
                        xmitter.ssid = xmitter.ssid.substring(1, xmitter.ssid.length() - 1 );
                    }
                }
                Log.d("MRVL", "SSID LENGTH IS " + xmitter.ssidLen);
                CRC32 crc32_ssid = new CRC32();
                crc32_ssid.reset();
                crc32_ssid.update(xmitter.ssid.getBytes());
                xmitter.ssidCRC = (int) crc32_ssid.getValue() & 0xffffffff;

                if (key.length() != 0) {
					if (pass.length() % 16 == 0) {
						xmitter.passLen = pass.length();
					} else {
						xmitter.passLen = (16 - (pass.length() % 16))
								+ pass.length();
					}

					byte[] plainPass = new byte[xmitter.passLen];

					for (int i = 0; i < pass.length(); i++)
						plainPass[i] = pass.getBytes()[i];

					xmitter.passphrase = myEncrypt(key, plainPass, xmitter.ssid);
				} else {
					xmitter.passphrase = pass.getBytes();
					xmitter.passLen = pass.length();
				}

				xmitter.mac = new char[6];
				xmitter.preamble = new char[6];
				String[] macParts = wifiInf.getBSSID().split(":");

				xmitter.preamble[0] = 0x45;
				xmitter.preamble[1] = 0x5a;
				xmitter.preamble[2] = 0x50;
				xmitter.preamble[3] = 0x52;
				xmitter.preamble[4] = 0x32;
				xmitter.preamble[5] = 0x32;

				Log.d("MRVL", wifiInf.getBSSID());
				for (int i = 0; i < 6; i++)
					xmitter.mac[i] = (char) Integer.parseInt(macParts[i], 16);
				xmitter.resetStateMachine();
				xmitter.execute("");
			} else {
				xmitStarted = false;
				BtnStartStop.setText("Start");
				xmitter.cancel(true);
			}
		} catch (Error err) {
			Log.e("MRVL", err.toString());
		}
	}

	private int checkWiFi() {
		wifiMgr = (WifiManager) getSystemService(Context.WIFI_SERVICE);
		wifiInf = wifiMgr.getConnectionInfo();
		Log.d("MRVL", "In checkWiFi");
		try {
			if (wifiMgr.isWifiEnabled() == false) {
				TxtDebug.setText("WiFi not enabled. Please enable WiFi and connect to the home network.");
				return -1;
			} else if (wifiMgr.getConnectionInfo().getSSID().length() == 0) {
				TxtDebug.setText("WiFi is enabled but device is not connected to any network.");
				return -1;
			}
		} catch (NullPointerException e) {
			TxtDebug.setText("WiFi is enabled but device is not connected to any network.");
		}

		EditSSID.setText(wifiMgr.getConnectionInfo().getSSID());
		EditPass.setText(getPassphrase(EditSSID.getText().toString()));
		BtnStartStop.setEnabled(true);
		return 0;
	}

	private class xmitterTask extends AsyncTask<String, Void, String> {
		byte[] passphrase;
		String ssid;
		char[] mac;
		char[] preamble;
		int passLen;
		int ssidLen;
		int passCRC;
		int ssidCRC;
		Handler handler;

		private int state, substate;

		public void resetStateMachine() {
			state = 0;
			substate = 0;
		}

		private void xmitRaw(int u, int m, int l) {
			MulticastSocket ms;
			InetAddress sessAddr;
			DatagramPacket dp;

			byte[] data = new byte[2];
			data = "a".getBytes();

			u = u & 0x7f; /* multicast's uppermost byte has only 7 chr */

			try {
//				Log.d("MRVL", "239." + u + "." + m + "." + l);
				sessAddr = InetAddress
						.getByName("239." + u + "." + m + "." + l);
				ms = new MulticastSocket(1234);
				dp = new DatagramPacket(data, data.length, sessAddr, 5500);
				ms.send(dp);
				ms.close();
			} catch (UnknownHostException e) {
				// TODO Auto-generated catch block
				// e.printStackTrace();
				Log.e("MRVL", "Exiting 5");
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}

		private void xmitState0(int substate) {
			int i, j, k;

			// Frame-type for preamble is 0b11110<substate1><substate0>
			// i = <frame-type> | <substate> i.e. 0x78 | substate
			k = preamble[2 * substate];
			j = preamble[2 * substate + 1];
			i = substate | 0x78;

			xmitRaw(i, j, k);
		}

		private void xmitState1(int substate, int len) {
			// Frame-type for SSID is 0b10<5 substate bits>
			// u = <frame-type> | <substate> i.e. 0x40 | substate
			if (substate == 0) {
				int u = 0x40;
				xmitRaw(u, ssidLen, ssidLen);
			} else if (substate == 1 || substate == 2) {
				int k = (int)(ssidCRC >> ((2 * (substate - 1) + 0) * 8)) & 0xff;
				int j = (int)(ssidCRC >> ((2 * (substate - 1) + 1) * 8)) & 0xff;
				int i = substate | 0x40;
				xmitRaw(i, j, k);
			} else {
				int u = 0x40 | substate;
				int l = (0xff & ssid.getBytes()[(2 * (substate - 3))]);
				int m;
				if (len == 2)
					m = (0xff & ssid.getBytes()[(2 * (substate - 3)) + 1]);
				else
					m = 0;
				xmitRaw(u, m, l);
			}
		}

		private void xmitState2(int substate, int len) {
			// Frame-type for Passphrase is 0b0<6 substate bits>
			// u = <frame-type> | <substate> i.e. 0x00 | substate
			if (substate == 0) {
				int u = 0x00;
				xmitRaw(u, passLen, passLen);
			} else if (substate == 1 || substate == 2) {
				int k = (int)(passCRC >> ((2 * (substate - 1) + 0) * 8)) & 0xff;
				int j = (int) (passCRC >> ((2 * (substate - 1) + 1) * 8)) & 0xff;
				int i = substate;
				xmitRaw(i, j, k);
			} else {
				int u = substate;
				int l = (0xff & passphrase[(2 * (substate - 3))]);
				int m;
				if (len == 2)
					m = (0xff & passphrase[(2 * (substate - 3)) + 1]);
				else
					m = 0;
				xmitRaw(u, m, l);
			}
		}

		private void stateMachine() {
			switch (state) {
			case 0:
				if (substate == 3) {
					state = 1;
					substate = 0;
				} else {
					xmitState0(substate);
					substate++;
				}
				break;
			case 1:
				xmitState1(substate, 2);
				substate++;
				if (ssidLen % 2 == 1) {
					if (substate  * 2 == ssidLen + 5) {
						xmitState1(substate, 1);
						state = 2;
						substate = 0;
					}
				} else {
					if ((substate - 1) * 2 == (ssidLen + 4)) {
						state = 2;
						substate = 0;
					}
				}
				break;
			case 2:
				xmitState2(substate, 2);
				substate++;
				if (passLen % 2 == 1) {
					if (substate  * 2 == passLen + 5) {
						xmitState2(substate, 1);
						state = 0;
						substate = 0;
					}
				} else {
					if ((substate - 1) * 2 == (passLen + 4)) {
						state = 0;
						substate = 0;
					}
				}
				break;
			default:
				Log.e("MRVL", "I shouldn't be here");
			}
		}

		protected String doInBackground(String... params) {
			WifiManager wm = (WifiManager) getSystemService(Context.WIFI_SERVICE);
			MulticastLock mcastLock = wm.createMulticastLock("mcastlock");
			mcastLock.acquire();

			int i = 0;

			while (true) {
				if (state == 0 && substate == 0)
					i++;

				if (i % 5 == 0) {
					Message msg = handler.obtainMessage();
					msg.what = 43;
					msg.arg1 = i;
					handler.sendMessage(msg);
				}

				/* Stop trying after doing 50 iterations. Let user retry. */
				if (i >= 600)
					break;

				if (isCancelled())
					break;

				stateMachine();

//				try {
//					Thread.sleep(10);
//				} catch (InterruptedException e) {
					// TODO Auto-generated catch block
					// e.printStackTrace();
//					break;
//				}
			}

			mcastLock.release();

			if (i >= 50) {
				Message msg = handler.obtainMessage();
				msg.what = 42;
				handler.sendMessage(msg);
			}
			return null;
		}


		@Override
		protected void onPostExecute(String result) {
		}

		@Override
		protected void onPreExecute() {
		}

		@Override
		protected void onProgressUpdate(Void... values) {
		}
	}
	/*
	 * @Override public boolean onCreateOptionsMenu(Menu menu) { // Inflate the
	 * menu; this adds items to the action bar if it is present.
	 * getMenuInflater().inflate(R.menu.main, menu); return true; }
	 */
}
