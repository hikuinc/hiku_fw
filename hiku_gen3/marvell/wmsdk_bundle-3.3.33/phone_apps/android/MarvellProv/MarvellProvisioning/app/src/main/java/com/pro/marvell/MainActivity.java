package com.pro.marvell;

import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.StreamCorruptedException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;

import android.R.color;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Dialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.text.Html;
import android.text.InputFilter;
import android.text.method.HideReturnsTransformationMethod;
import android.text.method.PasswordTransformationMethod;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.Spinner;
import android.widget.TextView;

@SuppressLint("NewApi")
public class MainActivity extends Activity {

	/* Change this to search for different SSID */
	String deviceSearchSSID = "wmdemo";

	public enum LastScreen {
		NONE, SCREEN1, MAIN_SCREEN
	}

	public static LastScreen lastScreen = LastScreen.NONE;
	Spinner networkSpinner;
	EditText passkey;
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
	ScanAdapterUiList scanEntries;
	WifiManager wifiMgr;
	private BroadcastReceiver receiver;

	int securityType, configured, status;
	String provResult = "", connectedNetwork = "";
	private SharedPreferences loginPreferences;
	private SharedPreferences.Editor loginPrefsEditor;
	boolean prov;
	String savedNetwork = "", savedPasskey = "";
	boolean savedNetworkVis = false;
	String Ssid = "", Passphrase = "";
	int Sec;
	int requestSysAttempt = 0, i = 1;
	String PASSWORD_PATTERN = "^[0-9A-Fa-f]+$",
			ASCII_PATERN = "^[0-9a-zA-Z]+$";
	final int MAX_RETRY_CNT = 3;
	protected int reTrycnt = MAX_RETRY_CNT;
	private Intent devicesPage;
	private String saveSSID;
	private String savePasswd;
	private String prvSSID = "";
	private String lastSSID;
	private CheckBox check1;
	private CheckBox RememberPassphrase;
	static HashMap<String, String> passhash = new HashMap<String, String>();

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		// setTheme(R.style.AppBaseTheme);
		super.onCreate(savedInstanceState);
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		setContentView(R.layout.activity_main);
		try {
			getActionBar()
					.setBackgroundDrawable(new ColorDrawable(Color.WHITE));
		} catch (Exception e) {
			e.printStackTrace();
		}
		MainActivity.lastScreen = LastScreen.MAIN_SCREEN;
		adapter = new ArrayAdapter<CharSequence>(this, R.layout.spinner_item);
		scanadapter = new ArrayAdapter<CharSequence>(this,
				R.layout.spinner_item);
		wmdemoadapter = new ArrayAdapter<CharSequence>(this,
				R.layout.spinner_item);

//		HashMapInit();

		findElements();
		setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_NOSENSOR);

		con = this;
		mainWifi = (WifiManager) getSystemService(Context.WIFI_SERVICE);

		progDailog = new Dialog(con);
		progDailog.requestWindowFeature(Window.FEATURE_NO_TITLE);
		clickEvents();
		startWifi();
		// startScan();
	}

	@Override
	public void onResume() {
		Log.d("onResume", "onResume");
		super.onResume();
		scanadapter.add("Scanning...");
		networkSpinner.setAdapter(scanadapter);
		startScan();
	}

	@Override
	public void onPause() {
		super.onPause();
		Log.d("Pause", "Pasue");
		// System.exit(0);
	}

	@Override
	public void finish() {
		// TODO Auto-generated method stub
		int pos = networkSpinner.getFirstVisiblePosition();
		if (pos > 0 && scan_result != null) {
			if (RememberPassphrase.isChecked()) {
				saveToPhoneMemory(scan_result.get(pos).SSID, passkey.getText()
						.toString());
			} else {
				saveToPhoneMemory(scan_result.get(pos).SSID, "");
			}
		}
		ShowAlertMessage.showPopupAlert(
				"Do you want to close this application?", "Yes",
				new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int id) {
						dialog.cancel();
						addNetworkAndActivate();
						// System.exit(0);
					}
				}, "No", null, MainActivity.this);
	}

	private boolean addNetworkAndActivate() {

		WifiConfiguration wc = null;
		if (saveSSID == null || scan_result == null) {
			Log.d("d", "eror");
			android.os.Process.killProcess(android.os.Process.myPid());
			System.exit(0);
			return false;
		}

		List<WifiConfiguration> configs = wifiMgr.getConfiguredNetworks();

		for (WifiConfiguration wifiConfiguration : configs) {
			try {
				if (wifiConfiguration.SSID.equals("\"" + saveSSID + "\"")) {
					Log.d("Password", wifiConfiguration.preSharedKey);
					wc = wifiConfiguration;
					break;
				}
			} catch (Exception e) {
				e.printStackTrace();
			}
		}

		ScanResult scanResult = null;
		for (int i = 0; i < scan_result.size(); i++) {
			if (scan_result.get(i).SSID.toString().replace("\"", "")
					.equals(saveSSID)) {
				scanResult = scan_result.get(i);
				break;
			}
		}

		if (scanResult == null) {
			Log.d("d", "Not found");
			android.os.Process.killProcess(android.os.Process.myPid());
			System.exit(0);
			return false;
		}
		// not configured, create new
		if (wc == null) {
			wc = new WifiConfiguration();

			ConfigurationSecuritiesV8 conf = new ConfigurationSecuritiesV8();
			conf.setupSecurity(wc, conf.getScanResultSecurity(scanResult),
					savePasswd);
			wc.SSID = "\"" + scanResult.SSID + "\"";

			int res = wifiMgr.addNetwork(wc);

			if (res == -1) {
				Log.d("d", "Can't add");
				android.os.Process.killProcess(android.os.Process.myPid());
				System.exit(0);
				return false;

			}
			if (!wifiMgr.saveConfiguration()) {
				Log.d("d", "Can't save");
				android.os.Process.killProcess(android.os.Process.myPid());
				System.exit(0);
				return false;
			}
		}
		wifiMgr.disconnect();
		boolean active = wifiMgr.enableNetwork(wc.networkId, true);
		Log.d("Error", "Error Check" + active);
		wifiMgr.reconnect();
		try {
			/*
			 * Wait for some time till network stack is up
			 */
			Thread.sleep(100);
			Log.d(">>", "Wait");

		} catch (InterruptedException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		Log.d("d", "Saved");
		android.os.Process.killProcess(android.os.Process.myPid());
		System.exit(0);
		return active;
	}

	private void HashMapInit() {
		try {

			FileInputStream fileInputStream = openFileInput("DoNotDeleteThis");
			ObjectInputStream objectInputStream = new ObjectInputStream(
					fileInputStream);
			passhash = (HashMap) objectInputStream.readObject();
			Log.d("HASH", "");
			objectInputStream.close();
			Log.d("File Read", passhash.toString());

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
		Log.d("HaspMap Entry", ssid + " : " + passphrase);
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
		return (String) passhash.get(ssid);
	}

	private void rememberPass(View v) {
		int pos = networkSpinner.getFirstVisiblePosition();
		if (((CheckBox) v).isChecked()) {
			saveToPhoneMemory(scan_result.get(pos).SSID, passkey.getText()
					.toString());
		} else {
			saveToPhoneMemory(scan_result.get(pos).SSID, "");
		}
	}

	protected int startWifi() {
		// Now you can call this and it should execute the broadcastReceiver's
		// onReceive()
		wifiMgr = (WifiManager) getSystemService(Context.WIFI_SERVICE);

		if (wifiMgr.isWifiEnabled() == false) {
			ShowAlertMessage.showPopupAlert(
					"Click on <b>Enable</b> to start Wi-Fi", "Exit",
					new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface dialog, int id) {
							dialog.cancel();
							System.exit(0);
						}
					}, "Enable", new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface dialog, int id) {
							dialog.cancel();
							wifiMgr.setWifiEnabled(true);
						}
					}, MainActivity.this);
		} else {
			saveSSID = wifiMgr.getConnectionInfo().getSSID();
			lastSSID = wifiMgr.getConnectionInfo().getSSID();
			Log.d("Saved SSID", ">>>>>" + saveSSID);
		}
		return 0;
	}

	private void findElements() {

		loginPreferences = getSharedPreferences("MarvellPrefs", MODE_PRIVATE);
		loginPrefsEditor = loginPreferences.edit();
		prov = loginPreferences.getBoolean("prov", false);

		networkSpinner = (Spinner) findViewById(R.id.network);
		passkey = (EditText) findViewById(R.id.passkey);
		passkey.setFilters(new InputFilter[] { new InputFilter.LengthFilter(26) });
		btnProvision = (Button) findViewById(R.id.btnProvision);
		txtPasskey = (TextView) findViewById(R.id.txtPassword);
		Log.d("findElements", "setting scan");
		scanadapter.add("Scanning...");
		networkSpinner.setAdapter(scanadapter);

		if (prov == true) {
			savedNetwork = loginPreferences.getString("network", "");
			savedPasskey = loginPreferences.getString("passkey", "");
		}

		check1 = (CheckBox) findViewById(R.id.check1);
		// check1.setBackgroundColor(color.holo_orange_light);
		check1.setOnCheckedChangeListener(new OnCheckedChangeListener() {

			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				// checkbox status is changed from uncheck to checked.
				if (!isChecked) {
					// show password
					passkey.setTransformationMethod(PasswordTransformationMethod
							.getInstance());
				} else {
					// hide password
					passkey.setTransformationMethod(HideReturnsTransformationMethod
							.getInstance());
				}
			}
		});

		RememberPassphrase = (CheckBox) findViewById(R.id.rememberCheckbox);
		RememberPassphrase.setOnClickListener(new OnClickListener() {

			@Override
			public void onClick(View v) {
				// TODO Auto-generated method stub
				rememberPass(v);

			}
		});
	}

	protected void selectedNetwork(int pos) {
		try {
			Log.d("Postion", " : " + pos);
			if (scan_result == null)
				return;
			networkSpinner.setSelection(pos);
			String sec = scan_result.get(pos).capabilities;

			if (sec.contains("WPA2")) {
				passkey.setVisibility(View.VISIBLE);
				txtPasskey.setVisibility(View.VISIBLE);
				check1.setVisibility(View.VISIBLE);
				passkey.setText(getPassphrase(scan_result.get(pos).SSID));
			} else if (sec.contains("WPA")) {
				passkey.setVisibility(View.VISIBLE);
				txtPasskey.setVisibility(View.VISIBLE);
				check1.setVisibility(View.VISIBLE);
				passkey.setText(getPassphrase(scan_result.get(pos).SSID));
			} else if (sec.contains("WEP")) {
				passkey.setVisibility(View.VISIBLE);
				txtPasskey.setVisibility(View.VISIBLE);
				check1.setVisibility(View.VISIBLE);
				passkey.setText(getPassphrase(scan_result.get(pos).SSID));
			} else {
				passkey.setVisibility(View.GONE);
				txtPasskey.setVisibility(View.GONE);
				check1.setVisibility(View.INVISIBLE);
				passkey.setText(getPassphrase(scan_result.get(pos).SSID));
			}
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	public class CustomOnItemSelectedListener implements OnItemSelectedListener {

		public void onItemSelected(AdapterView<?> parent, View view, int pos,
				long id) {
			selectedNetwork(pos);
		}

		@Override
		public void onNothingSelected(AdapterView<?> arg0) {
			// TODO Auto-generated method stub
		}
	}

	public void handleScanReult(List<ScanResult> configs) {
		scanEntries = new ScanAdapterUiList(MainActivity.this, configs);

		networkSpinner
				.setOnItemSelectedListener(new CustomOnItemSelectedListener());

		networkSpinner.setAdapter(scanEntries);

		if (!scanEntries.isEmpty())
			networkSpinner.setSelection(0);

		for (int i = 0; i < configs.size(); i++) {
			if (configs.get(i).SSID.toString().replace("\"", "")
					.equals(saveSSID)) {
				Log.d("Setting", "Selecting " + i);
				networkSpinner.setSelection(i);
				getPassphrase(networkSpinner.getSelectedItem().toString());
				break;
			}
		}

		Log.d("Handle Res", scanEntries.toString());
	}

	private ArrayList<Integer> channelsFrequency = new ArrayList<Integer>(
			Arrays.asList(0, 2412, 2417, 2422, 2427, 2432, 2437, 2442, 2447,
					2452, 2457, 2462, 2467, 2472, 2484));

	public Integer getFrequencyFromChannel(int channel) {
		return channelsFrequency.get(channel);
	}

	public int getChannelFromFrequency(int frequency) {
		return channelsFrequency.indexOf(Integer.valueOf(frequency));
	}

	protected void putWmdemoInDevicePage(int pos) {
		devicesPage = new Intent(MainActivity.this, DevicesPage.class);
		String[] wmdemoArray = wmdemoArrayList
				.toArray(new String[wmdemoArrayList.size()]);
		String[] wmdemobssidArray = wmdemobssidArrayList
				.toArray(new String[wmdemobssidArrayList.size()]);
		devicesPage.putExtra("wmdemoArray", wmdemoArray);
		devicesPage.putExtra("wmdemobssidArray", wmdemobssidArray);
		devicesPage.putExtra("network", networkSpinner.getSelectedItem()
				.toString());
		devicesPage.putExtra("passkey", passkey.getText().toString());
		devicesPage.putExtra("security", securityType);
		devicesPage.putExtra("channel",
				getChannelFromFrequency(scan_result.get(pos).frequency));
		devicesPage.putExtra("provType", "multi");
		devicesPage.putExtra("devSSID", scan_result.get(pos).SSID);
		devicesPage.putExtra("devPasskey", passkey.getText().toString());
		devicesPage.putExtra("devSecurity", securityType);
		Log.d("Pos ::" + pos, " SSID " + scan_result.get(pos).SSID
				+ " Passkey ::" + passkey.getText());

		startActivity(devicesPage);
	}

	protected void sortWmdemo(List<ScanResult> configs) {

		wmdemoadapter.clear();
		wmdemoArrayList = new ArrayList<String>();
		wmdemobssidArrayList = new ArrayList<String>();

		for (int i = 0; i < configs.size(); i++) {
			if ((configs.get(i).SSID).toString().contains(deviceSearchSSID) == true) {
				wmdemoadapter.add(configs.get(i).SSID);
				wmdemoArrayList.add(configs.get(i).SSID);
				wmdemobssidArrayList.add(configs.get(i).BSSID);
				System.out.println("wmdemo devices "
						+ (configs.get(i).SSID).toString());
			}
		}
	}

	protected void startScan() {
		// startWifi();
		scan_result = null;
		Log.d("MainApplication", "Start Scanning...");
		IntentFilter filter = new IntentFilter();
		filter.addAction(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION);

		receiver = new BroadcastReceiver() {
			@Override
			public void onReceive(Context c, Intent intent) {
				WifiManager w = (WifiManager) c
						.getSystemService(Context.WIFI_SERVICE);
				if (scan_result != null)
					return;
				List<ScanResult> configs = w.getScanResults();
				try {
					unregisterReceiver(receiver);
				} catch (Exception e) {
					e.printStackTrace();
				}
				Log.d("Scan Result", "Size of Scan result : " + configs.size());
				scan_result = configs;
				sortWmdemo(configs);
				handleScanReult(configs);
			}
		};
		registerReceiver(receiver, filter);
		wifiMgr.startScan();
	}

	private void clickEvents() {

		btnProvision.setOnClickListener(new OnClickListener() {

			@Override
			public void onClick(View v) {

				if (scan_result == null)
					return;
				System.out.println("wmdemoadapter.getCount() "
						+ wmdemoadapter.getCount());

				int pos = networkSpinner.getFirstVisiblePosition();
				saveSSID = scan_result.get(pos).SSID;
				String sec = scan_result.get(pos).capabilities;
				if (sec.contains("WPA2")) {
					securityType = 4;
				} else if (sec.contains("WPA")) {
					securityType = 3;
				} else if (sec.contains("WEP")) {
					securityType = 1;
				} else {
					securityType = 0;
				}

				if (RememberPassphrase.isChecked()) {
					saveToPhoneMemory(scan_result.get(pos).SSID, passkey
							.getText().toString());
				} else {
					saveToPhoneMemory(scan_result.get(pos).SSID, "");
				}

				System.out.println("securityType " + securityType);
				if (!(securityType == 0)
						&& passkey.getText().toString().equalsIgnoreCase("")) {
					ShowAlertMessage.showPopupAlert(
							"Please enter the passphrase/key", "OK", null,
							MainActivity.this);
				} else if (securityType == 1
						&& !((passkey.getText().toString()
								.matches(ASCII_PATERN)
								&& passkey.getText().toString().length() == 5 || passkey
								.getText().toString().length() == 13) || (passkey
								.getText().toString().matches(PASSWORD_PATTERN)
								&& passkey.getText().toString().length() == 10 || passkey
								.getText().toString().length() == 26))) {
					System.out.println("Invalid WEP Key");
					ShowAlertMessage
							.showPopupAlert(
									"WEP key must be 5/13 ASCII characters or 10/26 Hexadigits.",
									"OK", null, MainActivity.this);
				} else if (securityType == 3
						&& (passkey.getText().toString().length() < 8)) {
					System.out.println("Invalid WAP Key");
					ShowAlertMessage
							.showPopupAlert(
									"WPA passphrase length must be minimum 8 characters long.",
									"OK", null, MainActivity.this);
				} else if (securityType == 4
						&& (passkey.getText().toString().length() < 8)) {
					ShowAlertMessage
							.showPopupAlert(
									"WPA2 passphrase length must be minimum 8 characters long.",
									"OK", null, MainActivity.this);
				} else if (wmdemoadapter.getCount() > 0) {
					MainActivity.lastScreen = LastScreen.SCREEN1;
					savePasswd = passkey.getText().toString();
					putWmdemoInDevicePage(pos);
				} else if (wmdemoadapter.getCount() == 0) {
					ShowAlertMessage.showPopupAlert("No wmdemo device found!",
							"Refresh Scanlist",
							new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog,
										int id) {
									dialog.cancel();
									Log.d("Scan Again", "Scan Again");
									startScan();
								}
							}, MainActivity.this);
				}
			}
		});
	}

	class ScanAdapterUiList extends BaseAdapter {
		public static final String TAG = "[SampleAdapter]";
		List<ScanResult> scanList;
		Context mCtx;

		public ScanAdapterUiList(Context context, List<ScanResult> list) {
			scanList = list;
			mCtx = context;
		}

		public View getView(int position, View convertView, ViewGroup parent) {
			if (convertView == null) {
				convertView = LayoutInflater.from(mCtx).inflate(
						R.layout.scan_list_item, null);
			}

			ImageView icon = (ImageView) convertView
					.findViewById(R.id.iv_signal_icon);

			TextView titleText = (TextView) convertView
					.findViewById(R.id.tv_titleText);
			TextView secureityText = (TextView) convertView
					.findViewById(R.id.tv_securityText);
			titleText.setText(scanList.get(position).SSID);
			if (scanList.get(position).capabilities.contains("WPA2")) {
				secureityText.setText("Secured with WPA2");
				icon.setImageDrawable(getResources().getDrawable(
						R.drawable.signal_secure));
			} else if (scanList.get(position).capabilities.contains("WPA")) {
				secureityText.setText("Secured with WPA");
				icon.setImageDrawable(getResources().getDrawable(
						R.drawable.signal_secure));
			} else if (scanList.get(position).capabilities.contains("WEP")) {
				secureityText.setText("Secured with WEP");
				icon.setImageDrawable(getResources().getDrawable(
						R.drawable.signal_secure));
			} else {
				secureityText.setText("Unsecured");
				icon.setImageDrawable(getResources().getDrawable(
						R.drawable.signal_normal));
			}

			return convertView;
		}

		@Override
		public int getCount() {
			// TODO Auto-generated method stub
			return scanList.size();
		}

		@Override
		public Object getItem(int position) {
			// TODO Auto-generated method stub
			return scanList.get(position);
		}

		@Override
		public long getItemId(int position) {
			// TODO Auto-generated method stub
			return 0;
		}

	}

}
