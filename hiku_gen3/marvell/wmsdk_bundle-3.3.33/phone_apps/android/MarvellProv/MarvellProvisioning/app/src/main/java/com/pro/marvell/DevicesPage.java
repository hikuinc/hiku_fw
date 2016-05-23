package com.pro.marvell;

import java.io.IOException;
import java.io.InputStream;
import java.io.SequenceInputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Semaphore;

import org.apache.http.HttpResponse;
import org.apache.http.client.HttpClient;
import org.apache.http.client.ResponseHandler;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.conn.HttpHostConnectException;
import org.apache.http.entity.StringEntity;
import org.apache.http.impl.client.BasicResponseHandler;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.message.BasicHeader;
import org.apache.http.params.BasicHttpParams;
import org.apache.http.params.HttpConnectionParams;
import org.apache.http.params.HttpParams;
import org.apache.http.protocol.HTTP;
import org.json.JSONArray;
import org.json.JSONObject;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.AlertDialog.Builder;
import android.app.Dialog;
import android.app.ProgressDialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.graphics.drawable.Drawable;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.wifi.ScanResult;
import android.net.wifi.SupplicantState;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.Html;
import android.util.Log;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.animation.AnimationUtils;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.ArrayAdapter;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.Spinner;
import android.widget.TextView;

public class DevicesPage extends Activity {

	ListView listView;
	DeviceListViewAdapter deviceListViewAdapter;
	ArrayAdapter<CharSequence> wmdemobssidadapter;
	String network = "", passkey = "", deviceSSID = "", deviceBSSID = "";
	private static final int UNCONFIGURED = 0;
	private static final int CONFIGURED = 1;
	private static final int SERVER_NOT_FOUND = 2;
	private static final int UNKNOWN_DEVICE = 3;
	private static final int UNKNOWN_ERROR = 4;
	private static final int CONFIGURED_NOT_CONNECTED = 5;
	private static final int CONNECTING_ERROR = 6;

	private static final int FAILED = 1;
	private static final int SUCCESS = 0;

	Spinner networkSpinner;
	Button btnProvision;
	TextView txtPasskey;
	private Dialog progDailog;
	Context con;
	CustomizeDialog prodialog;
	CustomizeAlertDialog alertDialog;
	WifiManager mainWifi;
	ArrayList<String> networkssid, Capabilities;
	List<ScanResult> wifiList;
	ArrayAdapter<CharSequence> adapter;
	ArrayAdapter<CharSequence> scanadapter;
	ArrayAdapter<CharSequence> wmdemoadapter;
	String TAG = "Marvell";
	ArrayList<String> wmdemoArrayList;
	ArrayList<String> wmdemobssidArrayList;
	ArrayList<Integer> wmdemosectypeArrayList;

	public List<ScanResult> scan_result = null;
	ArrayAdapter<CharSequence> scanEntries;
	WifiManager wifiMgr;
	private BroadcastReceiver receiver = null;

	int securityType, configured, status;
	String provResult = "", connectedNetwork = "";
	boolean prov;
	String savedNetwork = "", savedPasskey = "";
	boolean savedNetworkVis = false;
	String Ssid = "", Passphrase = "";
	int Sec;
	int requestSysAttempt = 0, i = 1;
	String PASSWORD_PATTERN = "^[0-9A-Fa-f]+$",
			ASCII_PATERN = "^[0-9a-zA-Z]+$";
	private int SERVER_TIMEOUT;
	private int channel_no = -1;
	private AlertDialog alertBox;
	private boolean isReqToProv;
	final int MAX_RETRY_CNT = 3;
	protected int reTrycnt = MAX_RETRY_CNT;
	private int security;
	String devSSID;
	String devPasskey;
	int devSecurity;
	private Builder alertDialogBuilder;
	private String SaveAlertMessageString = "";
	private BroadcastReceiver bcast_reg;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		// TODO Auto-generated method stub
		setTheme(R.style.AppBaseTheme);
		super.onCreate(savedInstanceState);
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		setContentView(R.layout.devices_page);
		con = this;
		progDailog = new Dialog(con);
		progDailog.requestWindowFeature(Window.FEATURE_NO_TITLE);

		Bundle extras = getIntent().getExtras();

		String provType = extras.getString("provType");

		devSSID = extras.getString("devSSID");
		devPasskey = extras.getString("devPasskey");
		devSecurity = extras.getInt("devSecurity");
		channel_no = extras.getInt("channel");
		
		Log.d("Network Details", "SSID:" + devSSID + ", Passkey:" + devPasskey
				+ " SecurityType" + devSecurity);
		String[] wmdemoArray = extras.getStringArray("wmdemoArray");
		if (provType.contains("multi")) {
			Log.d("Prov Type", "Multi");
			String[] wmdemobssidArray = extras
					.getStringArray("wmdemobssidArray");
			network = extras.getString("network");
			passkey = extras.getString("passkey");
			security = extras.getInt("security");
			Log.d("Channel ::", "Channel # is " + channel_no);
			wmdemobssidadapter = new ArrayAdapter<CharSequence>(con,
					R.layout.spinner_item, wmdemobssidArray);
			// wmdemoArray = new ArrayAdapter<CharSequence>(context, resource)
			System.out.println("wmdemobssidadaptercount "
					+ wmdemobssidadapter.getCount());
		} else if (provType.contains("single")) {
			Log.d("Prov Type", "Single");
			provResult = extras.getString("result");
		}

		wmdemoadapter = new ArrayAdapter<CharSequence>(con,
				R.layout.spinner_item, wmdemoArray);
		System.out.println("wmdemoadaptercount " + wmdemoadapter.getCount());
		System.out.println("security " + security);

		wifiMgr = (WifiManager) getSystemService(Context.WIFI_SERVICE);

		findElements();
	}

	private void findElements() {
		// TODO Auto-generated method stub
		listView = (ListView) findViewById(R.id.listView);

		deviceListViewAdapter = new DeviceListViewAdapter(
				getApplicationContext(), wmdemoadapter);
		listView.setAdapter(deviceListViewAdapter);
	}

	class DeviceListViewAdapter extends BaseAdapter {
		Context rContext;
		private LayoutInflater rInflater;
		boolean ignoreDisabled = true;
		ArrayAdapter<CharSequence> rWmdemoadapter;

		public DeviceListViewAdapter(Context c,
				ArrayAdapter<CharSequence> wmdemoadapter) {
			rInflater = LayoutInflater.from(c);
			rContext = c;
			rWmdemoadapter = wmdemoadapter;
		}

		public int getCount() {

			return rWmdemoadapter.getCount();

		}

		public Object getItem(int arg0) {

			return null;
		}

		public long getItemId(int position) {

			return 0;
		}

		public View getView(final int position, View convertView,
				ViewGroup parent) {

			final DeviceTabModel mydat = new DeviceTabModel();
			if (convertView == null) {
				convertView = rInflater.inflate(R.layout.devices_row, null);
				mydat.deviceName = (TextView) convertView
						.findViewById(R.id.deviceName);
				mydat.txtProvision = (TextView) convertView
						.findViewById(R.id.txtProvision);
			} /*
			 * else { mydat = (DeviceTabModel) convertView.getTag(); }
			 */

			try {

				System.out.println("wmdemoadapter.getCount() "
						+ rWmdemoadapter.getCount() + " Position" + position);

				mydat.deviceName.setText(rWmdemoadapter.getItem(position)
						.toString());

				System.out.println("wmdemoadapter.getItem(i).toString() "
						+ rWmdemoadapter.getItem(position).toString());
				try {
					if (provResult.contains("Successful")) {
						mydat.txtProvision.setText("Provision Successful");
					} else if (provResult.contains("Reset")) {
						mydat.txtProvision.setText("Reset to Provision");
					} else if (provResult.contains("Tap")) {
						mydat.txtProvision.setText("Tap to Provision");
					}
				} catch (Exception e) {
					e.printStackTrace();
				}


				convertView.setOnClickListener(new OnClickListener() {

					@Override
					public void onClick(View v) {
						// TODO Auto-generated method stub
						final String text = mydat.txtProvision.getText().toString();
						status = 1;
						if (text.contains("Device connected to" +
                                "")) {
							Log.d("Info","Device already provisioned");
						} else if (text.contains("Tap")) {
							mydat.settxtProvision("Starting Provisoning Process");
							deviceSSID = rWmdemoadapter.getItem(position)
									.toString();
							deviceBSSID = wmdemobssidadapter.getItem(position)
									.toString();
                            Log.d("Hello", "Hi");
							startProvisioningProcess(deviceSSID,
									mydat.getTxtProvObj());
						}
					}

				});

				final String text = mydat.txtProvision.getText().toString();
				if (this.getCount() == 1) {
					if (text.contains("Successful")) {

					} else if (text.contains("Tap")) {
						mydat.settxtProvision("Starting Provisoning Process");
						deviceSSID = rWmdemoadapter.getItem(position)
								.toString();
						deviceBSSID = wmdemobssidadapter.getItem(position)
								.toString();
                        Log.d("NIKITA", "NIKITA");
						startProvisioningProcess(deviceSSID,
								mydat.getTxtProvObj());
					}
				}

			} catch (Exception e) {
				e.printStackTrace();
			}

			return convertView;

		}

		class DeviceTabModel {
			TextView deviceName, txtProvision;
			CheckBox chkAcknowledge;

			public void settxtProvision(String text) {
				this.txtProvision.setText(text);
			}

			public TextView getTxtProvObj() {
				return txtProvision;
			}
		}
	}

	protected void startWifiStateMachine(final String ssidCheck,
			final TextView statusView) {

		statusView.setText("Connecting to " + ssidCheck);

		showPopupwithCancelable("Connecting to " + ssidCheck, new OnCancelListener(){

			@Override
			public void onCancel(DialogInterface arg0) {
				// TODO Auto-generated method stub
				statusView.setText("Tap To Provision");
				hideAllPopups();
			}
			
		});
		

		try {
			bcast_reg = new BroadcastReceiver() {
				@Override
				public void onReceive(Context c, Intent i) {
					try {
						if (i.getAction().equals(
								WifiManager.SUPPLICANT_STATE_CHANGED_ACTION)) {
							SupplicantState state = i
									.getParcelableExtra(WifiManager.EXTRA_NEW_STATE);
							if (state == SupplicantState.COMPLETED) {
																
								try {
									/*
									 * Wait for some time till network stack is
									 * up
									 */
									Thread.sleep(2000);
									Log.d("Wifi Callback", "Connected");

								} catch (InterruptedException e) {
									// TODO Auto-generated catch block
									e.printStackTrace();
								}
								try {
									Log.d("Network info", "wifi ssid : "
											+ wifiMgr.getConnectionInfo()
													.getSSID()
													.replace("\"", "")
											+ " check ssid" + ssidCheck);
									if (wifiMgr.getConnectionInfo().getSSID()
											.replace("\"", "")
											.equals(ssidCheck)) {
										
										Log.d("Start provisioning",
												"Start provisioning");
										provisionAsync = new ProvisionAsync(
												wifiMgr.getConnectionInfo()
														.getSSID()
														.replace("\"", ""),
												statusView);
										provisionAsync.execute();
										
										hideAllPopups();
										statusView.setText("Connected to "
												+ ssidCheck);
										showPopup("Connected to " + ssidCheck);
										unregisterReceiver(bcast_reg);
										System.out.println("Unregister Sucess");
									} else {
										//hideAllPopups();
									}
								} catch (Exception e) {
									statusView.setText("Tap To Provision");
								}
							} else if (state == SupplicantState.DISCONNECTED) {
								/* TODO: Handle disconnected event here */
								Log.d("Wifi Callback", "Disconnected");
							}
						}
					} catch (Exception e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
					}
				}
			};
			registerReceiver(bcast_reg, new IntentFilter(
					WifiManager.SUPPLICANT_STATE_CHANGED_ACTION));
		} catch (Exception e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
			System.out.println("WIFIExcotion");
		}
	}

	protected void startProvisioningProcess(String ssid,
			final TextView statusView) {

		startWifiStateMachine(ssid, statusView);

		WifiConfiguration wc = new WifiConfiguration();
		wc.SSID = "\"" + ssid + "\"";
//		wc.preSharedKey = "\"" + "marvellwm" + "\"";
		wc.priority = 1;
		wc.status = WifiConfiguration.Status.ENABLED;
		wc.allowedGroupCiphers.set(WifiConfiguration.AuthAlgorithm.OPEN);
//		wc.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.TKIP);
//		wc.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.CCMP);
		wc.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
//		wc.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.TKIP);
//		wc.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.CCMP);
		wc.allowedProtocols.set(WifiConfiguration.Protocol.RSN);
//		wc.allowedProtocols.set(WifiConfiguration.Protocol.WPA);
		int netId = wifiMgr.addNetwork(wc);
		if (netId < 0) {
			return;
		}
		Log.d("Start Wmdemo", "Disconnect previous network");
		wifiMgr.disconnect();
		wifiMgr.enableNetwork(netId, true);
		Log.d("Start Wmdemo", "Connect to " + ssid);
		wifiMgr.reconnect();
//        Log.d("Start wmdemo", "")
	}

	protected int sendJson(final String ssid, final String key,
			final int security, final int ip) {
		int TIMEOUT_MILLISEC = 10000; // = 10 seconds
		HttpParams httpParams = new BasicHttpParams();
		HttpConnectionParams.setConnectionTimeout(httpParams, TIMEOUT_MILLISEC);
		HttpConnectionParams.setSoTimeout(httpParams, TIMEOUT_MILLISEC);

		HttpClient client = new DefaultHttpClient(httpParams);
		HttpResponse response;
		JSONObject json = new JSONObject();
		JSONArray jsonArray = new JSONArray();

		try {
			HttpPost post = new HttpPost("http://192.168.10.1/sys/network");
			json.put("ssid", ssid);
			json.put("security", security);
			json.put("channel", channel_no);
			json.put("key", key);
			json.put("ip", ip);
			jsonArray.put(json);

			StringEntity se = new StringEntity(json.toString());
			Log.d("Post Data", json.toString());
			se.setContentEncoding(new BasicHeader(HTTP.CONTENT_TYPE,
					"application/json"));
			post.setEntity(se);
			response = client.execute(post);

			/* Checking response */
			if (response != null) {
				InputStream in = response.getEntity().getContent();
				int b;
				StringBuilder stringBuilder = new StringBuilder();

				while ((b = in.read()) != -1) {
					stringBuilder.append((char) b);
				}

				System.out.println("Marvell " + stringBuilder.toString());
				Log.d("Marvell", stringBuilder.toString());
				if (stringBuilder.toString().contains("success")) {
					return SUCCESS;
				}
				return -FAILED;
			}
		} catch (Exception e) {
			return -FAILED;
		}
		return -FAILED;
	}

	protected int postSysNetwork() {
		Log.d("Data for /sys/network POST", "SSID : " + devSSID + " Passkey :"
				+ devPasskey + " security :" + devSecurity);
		return sendJson(devSSID, devPasskey, devSecurity, 1);
	}

    String ipaddress;
    protected void startProvStateMachine(TextView statusView,
			ProvisionAsync provAsync_t) {
		/*
		 * 1. GET on sys to check whether device is in provisioning mode or not
		 * 2. If not in provisioning mode, show
		 * "Provisioning Successful message" 3. If in provisioning mode, POST on
		 * /sys/network 4. Check for failure cases like: network failure,
		 * authentication failure, device not found, others, etc. 5. If
		 * provisioning successful, show appropriate message
		 */

		// TODO: Move this variables to some other location
		final int UNKNOWN = 1;
		final int PROVISIONING = 2;
		final int CONNECTING = 3;
		final int CONNECTED = 4;
		int cur_state = UNKNOWN, status;
		boolean prov_done = false;
		isReqToProv = false;
		while (!prov_done) {

			if (provAsync_t.isCancelled())
				return;

			if (isReqToProv) {
				alertBox.cancel();
				alertBox.dismiss();
				alertBox = null;
				showPopupAlert("Please wait...");
				Log.d("Req to prov", " Req to prov");
				while (true) {
					if (resetProvision() != SUCCESS) {
						try {
							Thread.sleep(500);
						} catch (InterruptedException e) {
							// TODO Auto-generated catch block
							e.printStackTrace();
						}
					} else {
						ShowAlertMessage.showPopupAlert(
								"Device is now in provisioning mode.", "OK",
								null, DevicesPage.this);
						setTextView(statusView, "Tap To Provision");
						// showPopupAlertPro("Device is now in provisioning mode.");
						break;
					}
				}

				return;
			}

			switch (cur_state) {
			case UNKNOWN:
				status = checkSys();
				if (status == CONFIGURED) {
					cur_state = CONNECTED;
					showPopupAlert("Device is already connected");
				} else if (status == UNCONFIGURED) {
					cur_state = PROVISIONING;
					setTextView(statusView, "Provisioning Started");
					showPopupAlert("Started provisioning process");
				} else if (status == CONFIGURED_NOT_CONNECTED
						|| status == CONNECTING_ERROR) {
					cur_state = CONNECTING;
				}
				reTrycnt = 0;
				Log.d("Provisioning[UNKNOWN]", "cur_state " + cur_state
						+ " , status" + status);
				break;
			case PROVISIONING:
				if (postSysNetwork() == SUCCESS) {
					cur_state = CONNECTING;
					setTextView(statusView, "Device Configured...");
					showPopupAlert("Device configured with provided network configurations");
				}
				Log.d("Provisioning[PROVISIONING]", "cur_state " + cur_state);
				break;
			case CONNECTING:
				status = checkSys();
				if (reTrycnt < MAX_RETRY_CNT) {
					if (status == CONFIGURED) {
						cur_state = CONNECTED;
					}
					reTrycnt++;
					//showPopupAlert("Device is trying to connect to " + devSSID
						//	+ "...");
					showPopupAlertWithCancel("Device is trying to connect to <b>" + devSSID
							+ "</b>", null);
					Log.d("Provisioning[CONNECTING]", "cur_state " + cur_state);

				} else if (status == CONFIGURED_NOT_CONNECTED) {
					reTrycnt++;
					showPopupAlertWithCancel("Device is trying to connect to <b>" + devSSID
							+ "</b>", null);
					//showPopupAlert("Device is trying to connect to " + devSSID
						//	+ "...");

					Log.d("Provisioning[CONNECTING]", "cur_state " + cur_state);
				} else if (status == CONNECTING_ERROR) {
					reTrycnt++;
					setTextView(statusView, "Device Connecting to Network...");
					showAlertWithResetToProv("The device is configured with provided settings. However the device can't connect to the configured network. \nReason: <b>"
							+ provResult
							+ "</b> <br>You can click <b>Reset</b> button to get the device in Provisioning mode again.");
					Log.d("Provisioning[CONNECTING] MAX Reached", "cur_state "
							+ cur_state + " , status" + status);
				} else if (status == CONFIGURED) {
					Log.d("Provisioning[CONNECTED]", "Device is connected to "
							+ devSSID);
					cur_state = CONNECTED;

				} else {
					reTrycnt++;
					showPopupAlertWithCancel("Device is trying to connect to <b>" + devSSID
							+ "</b>", null);
					Log.d("Provisioning[UNKNOWN]", "cur_state " + cur_state
							+ "status :" + status);
				}
				if (reTrycnt > 50 && status != CONNECTING_ERROR) {
					reTrycnt = 0;
					cur_state = PROVISIONING;
				}
				break;
			case CONNECTED:
				hideAllPopups();
				clientAck();
				prov_done = true;
				setTextView(statusView, "Device connected to " + devSSID + " with IP address " + ipaddress);

				ShowAlertMessage.showPopupAlert("Device connected to <b>" + devSSID + "</b>",
						"OK", null, "Provisioning Successful",DevicesPage.this);
				// showPopupAlertPro("Provisioning Successful!!!");

				Log.d("Provisioning[CONNECTED]", "cur_state " + cur_state);
				return;
			default:
				/* TODO: Show Unknown Error. */
			}
			try {
				Thread.sleep(1000);
			} catch (InterruptedException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}
	}

	private Runnable createPopUp(final String paramStr) {
		Runnable aRunnable = new Runnable() {
			public void run() {
				showPopup(paramStr);
			}
		};
		return aRunnable;

	}

	private Runnable createPopUpWithCancel(final String msg, final OnCancelListener list) {
		Runnable aRunnable = new Runnable() {
			public void run() {
				showPopupwithCancelable(msg, list);
			}
		};
		return aRunnable;

	}
	
	private Runnable setTextViewRunnable(final TextView textView,
			final String paramStr) {
		Runnable aRunnable = new Runnable() {
			public void run() {
				textView.setText(paramStr);
			}
		};
		return aRunnable;

	}

	protected void setTextView(TextView txt, String msg) {
		DevicesPage.this.runOnUiThread(setTextViewRunnable(txt, msg));
	}

	private Runnable createAlertR2PRunnable(final String paramStr) {
		Runnable aRunnable = new Runnable() {
			public void run() {
				alertResetToProv(paramStr);
			}
		};
		return aRunnable;
	}

	private Runnable destroyPopups() {
		Runnable aRunnable = new Runnable() {
			public void run() {
				if (alertBox != null && alertBox.isShowing()) {
					alertBox.hide();
				}
				if (prodialog != null && prodialog.isShowing()) {
					prodialog.hide();
				}
				ShowAlertMessage.hideAlert();
			}
		};
		return aRunnable;

	}

	protected void showPopupAlert(String msg) {
		if (isReqToProv)
			return;
		DevicesPage.this.runOnUiThread(createPopUp(msg));
	}

	protected void showPopupAlertWithCancel(String msg, OnCancelListener list) {
		if (isReqToProv)
			return;
		DevicesPage.this.runOnUiThread(createPopUpWithCancel(msg, list));
	}
	
	protected void showPopupAlertPro(String msg) {
		DevicesPage.this.runOnUiThread(createPopUpPro(msg));
	}

	private Runnable createPopUpPro(final String msg) {
		// TODO Auto-generated method stub
		Runnable aRunnable = new Runnable() {
			public void run() {
				showPopupPro(msg);
			}

		};
		return aRunnable;
	}

	private void showPopupPro(String msg) {
		// TODO Auto-generated method stub
		if (alertBox != null && alertBox.isShowing()) {
			alertBox.hide();
		}
		if (prodialog != null && prodialog.isShowing()) {
			prodialog.hide();
		}
		ShowAlertMessage.hideAlert();
		if (alertDialog == null)
			alertDialog = new CustomizeAlertDialog(DevicesPage.this);

		alertDialog.setMessage(msg);
		alertDialog.alertBtn.setOnClickListener(new OnClickListener() {
			@Override
			public void onClick(View v) {
				alertDialog.dismiss();
			}
		});
		alertDialog.show();
	}

	protected void showAlertWithResetToProv(String msg) {
		if (isReqToProv)
			return;

		DevicesPage.this.runOnUiThread(createAlertR2PRunnable(msg));
	}

	protected void hideAllPopups() {
		DevicesPage.this.runOnUiThread(destroyPopups());
		SaveAlertMessageString = "";
	}

	protected int checkSys() {

		int TIMEOUT_MILLISEC = 5000;

		HttpParams httpParams = new BasicHttpParams();
		HttpClient client = new DefaultHttpClient(httpParams);
		HttpConnectionParams.setConnectionTimeout(client.getParams(),
				TIMEOUT_MILLISEC);
		HttpConnectionParams.setSoTimeout(client.getParams(), TIMEOUT_MILLISEC);

		try {
			HttpGet get = new HttpGet("http://192.168.10.1/sys/");

			BasicResponseHandler responseHandler = new BasicResponseHandler();
			String SetServerString = client.execute(get, responseHandler);
			System.out.println("SetServerString " + SetServerString.toString());
			/* Checking response */
			if (SetServerString != null) {

				Log.d(TAG, "in if checking");
				JSONObject jsonObject = null;
				jsonObject = new JSONObject(SetServerString.toString());
				JSONObject routes_Array = jsonObject
						.getJSONObject("connection");
				JSONObject routes_Array1 = routes_Array
						.getJSONObject("station");
				configured = Integer.parseInt(routes_Array1
						.getString("configured"));
				status = Integer.parseInt(routes_Array1.getString("status"));
				if (configured == 0) {
					provResult = "unconfigured";
					return UNCONFIGURED;
				} else if (configured == 1 && status == 2) {
					provResult = "configured";
                    JSONObject station = routes_Array.getJSONObject("station");
                    JSONObject ip = station.getJSONObject("ip");
                    JSONObject ipv4 = ip.getJSONObject("ipv4");
                    ipaddress = ipv4.getString("ipaddr");
					return CONFIGURED;
				} else if (configured == 1 && status != 2) {
					/*
					 * Since device is not configured to network, we should
					 * check for error cases.
					 */
					if (SetServerString.contains("auth_failed")) {
						System.out.println("inside provresult authentication failed");
						provResult = "authentication failed";
					} else if (SetServerString.contains("network_not_found")) {
						System.out.println("inside provresult network not found");
						provResult = "Network not found";
					} else if (SetServerString.contains("dhcp_failed")) {
						System.out.println("inside provresult DHCP Failure");
						provResult = "DHCP Failure";
					} else if (SetServerString.contains("other")) {
						System.out.println("inside provresult other");
						provResult = "Network not found";
					} else {
						System.out.println("inside provresult tap");
						provResult = "Connecting...";
						return CONFIGURED_NOT_CONNECTED;
					}
					return CONNECTING_ERROR;
				} else if (SetServerString.contains("404")) {
					provResult = "404";
					return SERVER_NOT_FOUND;
				} else if (SetServerString.contains("Timeout ")) {
					provResult = "Timeout";
					return SERVER_TIMEOUT;
				} else if (SetServerString.contains("no \"marvell\"")) {
					provResult = "Not a valid device";
					return UNKNOWN_DEVICE;
				}
			}
			return UNKNOWN_ERROR;
		} catch (Exception e) {
			e.printStackTrace();
			/* Give some sleep so that network in reconnected */
			try {
				Thread.sleep(1000);
			} catch (InterruptedException e1) {
				// TODO Auto-generated catch block
				e1.printStackTrace();
			}
			Log.d("Exception", "Exception");
			provResult = "Phone disconnected from Marvell Network.";
			return UNKNOWN_ERROR;
		}
	}

	public int resetProvision() {

		int TIMEOUT_MILLISEC = 6000;
		HttpParams httpParams = new BasicHttpParams();
		HttpClient client = new DefaultHttpClient(httpParams);
		HttpConnectionParams.setConnectionTimeout(client.getParams(),
				TIMEOUT_MILLISEC);
		HttpConnectionParams.setSoTimeout(client.getParams(), TIMEOUT_MILLISEC);
		HttpResponse response2;
		JSONObject json121 = new JSONObject();

		try {
			HttpPost post = new HttpPost("http://192.168.10.1/sys/");
			json121.put("configured", 0);
			JSONObject json12 = new JSONObject();
			json12.put("station", json121);
			JSONObject json1 = new JSONObject();
			json1.put("connection", json12);
			Log.d("Reset to Prov JSON", json1.toString());
			StringEntity se = new StringEntity(json1.toString());
			se.setContentEncoding(new BasicHeader(HTTP.CONTENT_TYPE,
					"application/json"));
			post.setEntity(se);
			response2 = client.execute(post);

			/* Checking response */
			if (response2 != null) {
				InputStream in = response2.getEntity().getContent();
				int b;
				StringBuilder stringBuilder3 = new StringBuilder();

				while ((b = in.read()) != -1) {
					stringBuilder3.append((char) b);
				}

				System.out.println("Marvell " + stringBuilder3.toString());
				Log.d("Marvell", stringBuilder3.toString());
				if (stringBuilder3.toString().contains("success")) {
					provResult = "Reset Provision Success";
					return SUCCESS;
				} else if (stringBuilder3.toString().contains("Timeout")) {
					provResult = "Timeout";
				} else if (stringBuilder3.toString().contains("404")) {
					provResult = "404";
				} else if (stringBuilder3.toString().contains("-34")) {
					provResult = "-34";
				} else {
					provResult = "fail";
				}
			}
		} catch (Exception e) {
			e.printStackTrace();
			provResult = "Reset";

		}
		return -FAILED;
	}

	public void alertResetToProv(String msg) {

		prodialog.setOnCancelListener(null);
		prodialog.hide();
		prodialog.cancel();
		
		if (alertBox != null && alertBox.isShowing() && msg.equalsIgnoreCase(SaveAlertMessageString)) {
			Log.d("Don't Refresh", "Don't REfresh");
			return;
		}
		
		SaveAlertMessageString = msg;

		if (alertBox != null && alertBox.isShowing()) {
			alertBox.hide();
		}
		if (prodialog != null && prodialog.isShowing()) {
			prodialog.hide();
		}
		ShowAlertMessage.hideAlert();

		if (alertDialogBuilder == null)
			alertDialogBuilder = new AlertDialog.Builder(
					new ContextThemeWrapper(DevicesPage.this,
							android.R.style.Theme_Holo_Light_Dialog_NoActionBar));

		View view = DevicesPage.this.getLayoutInflater().inflate(
				R.layout.customprogress, null);
		view.findViewById(R.id.loading_icon).startAnimation(
				AnimationUtils
						.loadAnimation(DevicesPage.this, R.anim.rotate360));
		TextView txtView = (TextView) view.findViewById(R.id.loading_text);
		txtView.setText(Html.fromHtml("<center>" + msg + "</center>"));
		alertDialogBuilder
				.setCancelable(false)
				.setView(view)
				.setPositiveButton("Reset",
						new DialogInterface.OnClickListener() {
							public void onClick(DialogInterface dialog, int id) {
								dialog.cancel();
								SaveAlertMessageString = "";
								Log.d("Req to prov>>>", " Req to prov>>>");
								isReqToProv = true;
							}
						})
				.setNegativeButton("Continue",
						new DialogInterface.OnClickListener() {

							public void onClick(DialogInterface dialog, int id) {
								dialog.cancel();
								SaveAlertMessageString = "";
								Log.d("Req to prov", "Cancel max");
								reTrycnt = 0;
							}
						});

		alertBox = alertDialogBuilder.create();
		alertBox.show();
	}

/*	@Override
	public void finish() {
		Log.d("finish", "finish...");
		// TODO Auto-generated method stub
		if (provisionAsync != null ) {
			provisionAsync.cancel(true);
			Log.d("finish" , "done");
		}
		Intent mainPage = new Intent(DevicesPage.this, MainActivity.class);
		mainPage.setFlags(Intent.FLAG_ACTIVITY_NO_HISTORY);
		startActivity(mainPage);
	}
*/	
	public void showPopup(String msg) {
		if (alertBox != null && alertBox.isShowing()) {
			alertBox.hide();
		}
		prodialog.setOnCancelListener(null);
		if (prodialog != null && prodialog.isShowing()) {
			prodialog.hide();
		}
		ShowAlertMessage.hideAlert();

		if (prodialog == null)

		prodialog = new CustomizeDialog(DevicesPage.this);
		prodialog.setMessage(Html.fromHtml("<center>" + msg + "</center>"));
		prodialog.setCancelable(false);
		prodialog.show();
		prodialog.findViewById(R.id.loading_icon).startAnimation(
				AnimationUtils
						.loadAnimation(DevicesPage.this, R.anim.rotate360));
	}

	public void showPopupwithCancelable(String msg, OnCancelListener listner) {
		Log.d("show", "with cancelable");
		if (alertBox != null && alertBox.isShowing()) {
			alertBox.hide();
		}
		if (prodialog != null && prodialog.isShowing()) {
			prodialog.hide();
		}
		ShowAlertMessage.hideAlert();

		if (prodialog == null)

		prodialog = new CustomizeDialog(DevicesPage.this);
		prodialog.setMessage(Html.fromHtml("<center>" + msg + "</center>"));
		prodialog.setCancelable(true);
		prodialog.setOnCancelListener(listner);
		prodialog.show();
		prodialog.findViewById(R.id.loading_icon).startAnimation(
				AnimationUtils
						.loadAnimation(DevicesPage.this, R.anim.rotate360));
	}
	
	@Override
	public void onBackPressed() {

		/*
		 * if (provisionAsync != null || status1==1){
		 * provisionAsync.cancel(true); Log.d("Back Button Pressed",
		 * "Back Button Pressed"); }
		 */

		super.onBackPressed();

	}

	public ProvisionAsync provisionAsync;

	public class ProvisionAsync extends AsyncTask<Void, Integer, Void> {

		String marvellSSID = null;
		TextView statusView;

		ProvisionAsync(String ssid, TextView statusView) {
			marvellSSID = ssid;
			this.statusView = statusView;
		}

		@Override
		protected void onCancelled() {
			Log.d("Cancelled", "Task cancelled...");
			setTextView(this.statusView, "Tap To Provision");

		}

		@Override
		protected Void doInBackground(Void... params) {
			Log.d("ProvisionAsync", "Connect to :" + marvellSSID);
			startProvStateMachine(this.statusView, this);
			return null;
		}

		@Override
		protected void onPreExecute() {

			super.onPreExecute();
		}

	}

	public void clientAck() {
		int TIMEOUT_MILLISEC = 60000;
		HttpParams httpParams = new BasicHttpParams();
		HttpClient client = new DefaultHttpClient(httpParams);
		HttpConnectionParams.setConnectionTimeout(client.getParams(),
				TIMEOUT_MILLISEC);
		HttpConnectionParams.setSoTimeout(client.getParams(), TIMEOUT_MILLISEC);
		HttpResponse response2;
		JSONArray jsonArray = new JSONArray();
		JSONObject json121 = new JSONObject();

		try {
			HttpPost post = new HttpPost("http://192.168.10.1/sys/");
			json121.put("client_ack", 1);
			JSONObject json12 = new JSONObject();
			json12.put("prov", json121);
			jsonArray.put(json12);
			StringEntity se = new StringEntity(json12.toString());
			se.setContentEncoding(new BasicHeader(HTTP.CONTENT_TYPE,
					"application/json"));
			post.setEntity(se);
			response2 = client.execute(post);
			/* Checking response */
			if (response2 != null) {
				InputStream in1 = response2.getEntity().getContent();
				int b;
				StringBuilder stringBuilder1 = new StringBuilder();
				try {
					while ((b = in1.read()) != -1) {
						stringBuilder1.append((char) b);
					}
				} catch (IOException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
				System.out.println("Marvell " + stringBuilder1.toString());
				Log.d("Marvell2", stringBuilder1.toString());
				if (stringBuilder1.toString().contains("success")) {
					provResult = "provisioning successfull";
					System.out.println("Provisioning successfull2");
				}
			}
		} catch (Exception e) {
			e.printStackTrace();
			provResult = "Reset";
			System.out.println("Client:" + e);
		}
	}

}
