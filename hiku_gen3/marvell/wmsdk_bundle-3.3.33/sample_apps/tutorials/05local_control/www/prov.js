document.write('<scr'+'ipt type="text/javascript" src="curve255.js" ></scr'+'ipt>');

/*--------------------------- ProvDataModel ---------------------------------*/

function ProvDataModel() {
	this.sys = null;
	this.scanList = null;

	/* The states can be :
	 * "unknown" : The state is unknown (at beginning).
	 * "unconfigured" : The device is known to be not configured
	 * "nw_posted" : User has entered required credentials and posted network
	 * "configured" : The device is known to be configured 
	 *
	 * The emitted errors can be:
	 * "conn_err" : Connection error; can't communicate with device
	 * "invalid_nw_param" : Invalid network parameters specified
	 */
	this.state = "unknown";
	this.secure_pin = null;
	this.configured = null;
	this.status = null;
	this.failure = null;
	this.failure_cnt = null;
	this.ssid = null;

	/* Private members */
	var uuid = null;
	var failedSysGet = 0;
	var failedScanGet = 0;
	var scanListRequestAborted = false;
	var sysRequestAborted = false;
	var secure_session_id = 0;
        var configurednotupdated = 0;

	this.changedState = new Event(this);
};

ProvDataModel.prototype = {
	stateMachine : function(obj, event) {
		console.log("In stateMachine " + obj.state + " " + event);

		switch (obj.state) {
		case "unknown":
		          if (event === "init") {
			          obj.deviceState(obj);
			  } else if (event === "begin_prov") {
				  obj.changedState.notify({"state" : obj.state, "event":"begin_prov"});
			  } else if (event === "pin_entered") {
				  obj.secureSession(obj);
			  } else if (event === "secure_session_success") {
				  obj.fetchSys(obj, 3);
			  } else if (event === "secure_session_failed") {
				  alert("Invalid pin");
				  obj.changedState.notify({"state" : obj.state, "event":"begin_prov"});
			  } else if (event === "sysdata_updated") {
				if (obj.configured === 0) {
					  /* Check if device is already configured. Also note the
					   * device uuid in case the connection gets switched to
					   * other device behind our back. */
					//obj.uuid = obj.sys.uuid;
					  obj.state = "unconfigured";
					  obj.changedState.notify({"state" : obj.state});
				} else if (obj.configured === 1) {
					  obj.state = "configured";
					  obj.changedState.notify({"state" : obj.state});
				}
			  } else if (event === "conn_err") {
				  obj.changedState.notify({"state" : obj.state, "err":"conn_err"});
			  } else if (event === "device_configured") {
				  obj.changedState.notify({"state" : obj.state, "err":"device_configured"});
			  }
			  break;
		case "unconfigured":
			  if (event === "scanlist_updated") {
				  obj.changedState.notify({"state" : obj.state, "event":"scanlist_updated"});
			  } else if (event === "conn_err") {
				  obj.changedState.notify({"state" : obj.state, "err":"conn_err"});
			  } else if (event === "nw_posted") {
				  obj.state = "nw_posted";
				  obj.changedState.notify({"state" : obj.state});
			  }
			  break;
		case "nw_posted":
			  if (event === "nw_post_success") {
				  obj.state = "configured";
			          /* Once nw is posted, device takes few seconds to update configured
				   * variable, hence adding sleep for 5 seconds
				   */
			          sleep(5000);
				  obj.changedState.notify({"state" : obj.state});
			  } else if (event === "conn_err") {
				  obj.changedState.notify({"state" : obj.state, "err":"conn_err"});
			  } else if (event === "nw_post_invalid_param") {
				  obj.changedState.notify({"state" : obj.state, "err":"invalid_nw_param"});
				  obj.state = "unconfigured";
			  }
			  break;
		case "configured":
			  if (event === "sysdata_updated") {
				  if (obj.uuid && (obj.uuid !== obj.sys.uuid)) {
					  /* The saved uuid and uuid in GET /sys don't match. It means
					   * in-between the client moved to some other network that
					   * also supports marvell provisioning protocol. Yes, this
					   * is real and we have seen this happening in our environment
					   * many times */
					  console.log("Connection moved to different device");
					  obj.changedState.notify({"state" : obj.state, "err":"network_switched"});
				  } else if (obj.configured === 1) {
				    console.log("Configured SUCCESS");
					  obj.changedState.notify({"state" : obj.state, "event":"sysdata_updated"});
				    configurednotupdated = 0;
				  } else if (obj.configured === 0) {
				          if (configurednotupdated < 3) {
					    configurednotupdated++;
					    return;
					  }
				          console.log("Configured -> Unconfigured");
					  obj.state = "unconfigured";
					  obj.changedState.notify({"state" : obj.state});
				  }
			  } else if (event === "conn_err") {
				  obj.changedState.notify({"state" : obj.state, "err":"conn_err"});
			  } 
			  break;
		case "finish":
			  obj.state = "unknown";
			  obj.changedState.notify({"state" : obj.state});
			  break;

		}
	},

	reinit : function() {
		this.sys = null;
		this.scanList = null;
		this.state = "unknown";
	},

	beginProv : function() {
		this.stateMachine(this, "init");
	},

	destroy : function () {
		this.sys = null;
		this.scanList = null;
	},

	finish : function() {
		this.stateMachine(this, "finish");
	},

	connectionError : function () {
		this.stateMachine(this, "conn_err");
	},

	sendProvDoneAck : function (obj) {
		var response = new Object();
		response["prov_client_ack"] = 1;
	  var jsonData = encrypt_data(response);
		var postUrl = '/prov/net-info?session_id='+obj.secure_session_id;
		var provError = function() {
		  console.log("Provisioning protocol error");
		};

		var provSuccess = function() {
		  console.log("Provisioning protocol success");
		};
		$.ajax({
			type: 'POST',
			url: postUrl,
			async : 'false',
			dataType : 'json',
			data : JSON.stringify(jsonData),
			contentType : 'application/json',
			timeout : 5000,
			success : provSuccess,
			error: provError
		}).done();
	},

	fetchSys : function (obj, max_failures, retry_interval) {
		var ajax_obj = null;

	        if (obj.sysRequestAborted)
	                return;
		if (typeof retry_interval === 'undefined')
			retry_interval = 3000;

		var fetchFailed = function() {
			if (!(typeof max_failures === 'undefined')) {
				obj.failedSysGet++;
				if (max_failures === obj.failedSysGet) {
					obj.connectionError();
				} else {
					// try again
					ajax_obj = this;
					setTimeout(function() { $.ajax(ajax_obj); }, retry_interval);
				}
			} else {
				if (obj.sysRequestAborted)
					return;
				obj.connectionError();
			}
		};

		var fetchDone = function(data) {
			obj.failedSysGet = 0;
			if (obj.sysRequestAborted)
				return;
			var jsonData = decrypt_secure_data(data);
			if (jsonData !== null) {
				obj.configured = jsonData.configured;
			    obj.status = jsonData.status;
			    obj.failure = jsonData.failure;
			    obj.failure_cnt = jsonData.failure_cnt;
			    obj.stateMachine(obj, "sysdata_updated");
			} else {
				obj.stateMachine(obj, "conn_err");
			}
		};

		var getUrl = '/prov/net-info?session_id='+obj.secure_session_id;
		$.ajax({
			type: 'GET',
			url: getUrl,
			async : 'true',
			dataType : 'json',
			timeout : 5000,
            cache: false,
			success : fetchDone,
			error: fetchFailed
		}).done();
	},

	cancelSysRequest : function() {
		this.sysRequestAborted = true;
	},

	secureSession : function(obj) {

	    var padding = "0000";
	    var pin = obj.secure_pin;
	    var nw = new Object();
	    var random_bytes
	    var len;

	    var private_key = new Array(16);
	    var public_key = new Array(16);
	    var device_pub_key = new Array(16);
	    var random_number = new Array(64);
	    var aes_key = new Array(4);

	    /* Generate random number */
	    for (i = 0; i < 64; i++) {
	        random_number[i] = randomUInt8();
	    }

	    var random_number_str = "";
	    len = random_number[0].toString(16).length;
	    if (len != 2) {
	        random_number_str = padding.slice(0, 2 - len);
	    }
	    random_number_str = random_number_str + random_number[0].toString(16);

	    for (i = 1; i < 64; i++) {
	        len = random_number[i].toString(16).length;
	        if (len != 2) {
	            random_number_str = random_number_str + padding.slice(0, 2 - len);
	        }
	        random_number_str = random_number_str + random_number[i].toString(16);
	    }

	    /* Generate Private Key */
	    for (i = 0; i < 16; i++) {
	        private_key[i] = randomUInt16();
	    }
	    /* Generate the Public Key */
	    public_key = curve25519(private_key);

	    var public_key_str = "";
	    var j = 0;
	    for (i = 0; i < 16; i++) {
			var val  = public_key[i].toString(16);
			/* Converting Big-Endian to Little-Endian */
			val = val.replace(/^(.(..)*)$/, "0$1");
			var a = val.match(/../g);
			a.reverse();
			var s2 = a.join("");
			public_key_str += pad(s2, 4);
			j++;
	    }

	    nw["client_pub_key"] = public_key_str;
	    nw["random_sig"] = random_number_str;

		var secureDone = function(data) {
			var device_pub_key = data.device_pub_key;
			var encrypted_sig = data.data;
			var checksum_str = data.checksum;
			var session_id = data.session_id;
			var iv_str = data.iv;
			if (device_pub_key === undefined || encrypted_sig === undefined ||
                checksum_str === undefined || session_id === undefined) {
                console.log("Failure : Bad response");
                return;
            }

            var device_pub_key_array = new Array(16);
            var j = 0;
            for (i = 0; i < 64; i = i + 4) {
                /* Changing the Endianess */
                /* The Curve25519 library used in this reference implementation accepts data as array
                of 16-bit Big-Endian numbers. Therefore, below conversion is required */
                var val  = device_pub_key.slice(i, i + 4).toString(16);
                var val1 = parseInt(device_pub_key.slice(i, i + 4).toString(16));
                val = val.replace(/^(.(..)*)$/, "0$1");
                var a = val.match(/../g);
                a.reverse();
                var s2 = a.join("");

                device_pub_key_array[j] = parseInt(s2, 16);
                j++;
            }

            var shared_secret = curve25519(private_key, device_pub_key_array);
            var j = 0;
            var shared_secret_array = new Array(8);
            for (i = 0; i < 16; i += 2) {
                var val = shared_secret[i].toString(16);
                var val2 = shared_secret[i + 1].toString(16);
                var result = (val + val2).toString(16);
                /* Changing the Endianess */
                /* The Sha512 library used in this reference implementation accepts data as array
                of 32-bit Big-Endian numbers. Therefore, below conversion is required */
                val = val.replace(/^(.(..)*)$/, "0$1");
                var a = val.match(/../g);
                a.reverse();
                var s2 = a.join("");
                result = s2;

                val2 = val2.replace(/^(.(..)*)$/, "0$1");
                a = val2.match(/../g);
                a.reverse();
                s2 = a.join("");
                result = result + s2 ;

                shared_secret_array[j] = parseInt(result, 16);

                j++;
            }

            var shared_secret_hash = sjcl.hash.sha512.hash(shared_secret_array);

            /* Computing Sha512 HASH of pin */
            var pin_hash = sjcl.hash.sha512.hash(pin);

            var len;
            for (i = 0; i < 4; i++) {
                var temp = "";
                aes_key[i] = (pin_hash[i] ^ shared_secret_hash[i]);
                len = (aes_key[i] >>> 0).toString(16).length;
                if (len != 8) {
                    temp = padding8.slice(0, 8 - len);
                }
                var t = temp + (aes_key[i] >>> 0).toString(16);
                aes_key[i] = parseInt(temp + (aes_key[i] >>> 0).toString(16), 16);
            }
            shared_aes_key = aes_key;

            /* Decrypt the "data" received as a part of the /prov/secure-session response */
            /* First retrive "iv" */
            var k = 0;
            var iv = new Array(4);
            for (i = 0; i < 32; i+=8) {
                var temp = "";
                var str = pad(iv_str.slice(i, i + 8), 8);
                iv[k] = parseInt(str, 16);
                k++;
            }
            /* Decrypt the Data */
            var data_array = new Array();
            var data_len = encrypted_sig.length;
            var j = 0;
            for (i = 0; i < data_len; i = i + 8) {
                var val  = encrypted_sig.slice(i, i + 8).toString(16);
                var val1 = parseInt(encrypted_sig.slice(i, i + 8).toString(16), 16);

                if (i + 8 > data_len) {
                    /* This is required since this last element should have complete 4 byte data.
                    So, adding trailing 0s. For example, ffff -> ffff0000 */
                    trim = ((i + 8) - data_len);
                    var x = encrypted_sig.slice(i, data_len).toString(16) + padding8.slice(0, trim);
                    var val1 = parseInt(x, 16);
                }
                data_array[j] = val1;
                j++;
            }

            var cipher = new AES.CTR(aes_key, iv);
            cipher.encrypt(data_array);

            var data_checksum = sjcl.hash.sha512.hash(data_array);
            j = 0;
            for (i = 0; i < 64; i += 8) {
                var tmp_data = parseInt(checksum_str.slice(i, i + 8), 16)
                if (tmp_data.toString(16) != (data_checksum[j] >>> 0).toString(16))
                {
                    console.log("Failure : Checksum Verification failed");
                    obj.stateMachine(obj, "secure_session_failed");
                    return;
                }
                j++;
            }

            var test = "";
            for (i = 0; i < (data_len/8); i++) {
                test += pad((data_array[i] >>> 0).toString(16), 8);
            }
            if (test != random_number_str) {
                console.log("Failure : Random number validation failed");
                obj.stateMachine(obj, "secure_session_failed");
                return;
            }

            obj.secure_session_id = session_id;
            console.log("Success : Secure-session-Established");
            obj.stateMachine(obj, "secure_session_success");
		};

		var secureError = function(xhr, textStatus, error) {
			console.log("Error " + xhr.responseText);
		  obj.stateMachine(obj, "conn_err");

		};

		var secureCheckSuccess = function(data) {
			console.log("Success" + JSON.stringify(data));
		};

		var secureCheckError = function(xhr, textStatus, error) {
			console.log("Error " + xhr.responseText);
		};

		var postUrl = '/prov/secure-session';
		$.ajax({
			type: 'GET',
			url: postUrl,
			async : 'true',
			dataType : 'json',
			timeout : 5000,
            cache: false,
			success : secureCheckSuccess,
			error: secureCheckError
		}).done();

		$.ajax({
			type: 'POST',
			url: postUrl,
			data: JSON.stringify(nw),
			async : 'false',
			dataType : 'json',
			contentType : 'application/json',
			timeout : 5000,
			success : secureDone,
			error: secureError
		}).done();
	},

	fetchScanList : function (obj, max_failures, retry_interval) {
		var ajax_obj = null;

		if (typeof retry_interval === 'undefined')
			retry_interval = 3000;

		var fetchError = function() {
			if (!(typeof max_failures === 'undefined')) {
				obj.failedScanGet++;
				if (max_failures === obj.failedScanGet) {
					obj.connectionError();
				} else {
					ajax_obj = this;
					setTimeout(function() { $.ajax(ajax_obj); }, retry_interval);
				}
			} else {
				if (obj.scanListRequestAborted)
					return;
				obj.connectionError();
			}
		};
		var fetchDone = function(encrypt_data) {
			obj.failedScanGet = 0;
			if (obj.scanListRequestAborted)
				return;
			var data = decrypt_secure_data(encrypt_data);
			obj.scanList = new Array();
			for (var i = 0; i < data.networks.length; i++) {
				obj.scanList[i] = new Array();
				for (var j = 0; j < data.networks[i].length; j++) {
					obj.scanList[i][j] = data.networks[i][j];
				}
			}
			obj.scanList.sort(function(a, b) {return b[4] - a[4];});
			obj.stateMachine(obj, "scanlist_updated");
		};
		obj.scanListRequestAborted = false;
		var scanUrl = '/prov/scan?session_id='+obj.secure_session_id;
		$.ajax({
			type: 'GET',
			url: scanUrl,
			async : 'true',
			dataType : 'json',
			timeout : 5000,
            cache: false,
			success : fetchDone,
			error: fetchError
		}).done();
	},
	
	cancelScanListRequest : function() {
		this.scanListRequestAborted = true;
	},

	getScanResults : function() {
		return this.scanList;
	},

	getScanListEntry : function(index) {
		return this.scanList[index];
	},

	setPin : function(pin) {
	  this.secure_pin = pin;
			this.stateMachine(this, "pin_entered");

	},

	setNetwork : function(obj) {
		var obj_this = this;
		var setNetworkFailed = function(xhr, status, error) {
			obj_this.stateMachine(obj_this, "conn_err");
		};

		var setNetworkDone = function(data) {
		  var jsonData = decrypt_secure_data(data);
			if (jsonData.success === 0) {
				obj_this.stateMachine(obj_this, "nw_post_success");
			} else {
				obj_this.stateMachine(obj_this, "nw_post_invalid_param");
			}
		};

		var jsonData = encrypt_data(obj);

		var postUrl = '/prov/network?session_id='+obj_this.secure_session_id;
		$.ajax({
			type: 'POST',
			url: postUrl,
			data: JSON.stringify(jsonData),
			async : 'false',
			dataType : 'json',
			contentType : 'application/json',
			timeout : 5000,
			success : setNetworkDone,
			error: setNetworkFailed
		}).done();

		this.stateMachine(this, "nw_posted");
	},

	deviceState : function(obj) {
		var deviceUnconfigured = function(data) {
		  console.log("Success : Device is unconfigured");
		  obj.stateMachine(obj, "begin_prov");
		};

		var deviceConfigured = function(xhr, textStatus, error) {
			console.log("Error : Device configured");
		        obj.stateMachine(obj, "device_configured");
		};
		var postUrl = '/prov/secure-session';
		$.ajax({
			type: 'GET',
			url: postUrl,
			async : 'false',
			dataType : 'json',
			contentType : 'application/json',
			timeout : 5000,
                        cache: false,
			success : deviceUnconfigured,
			error: deviceConfigured
		}).done();
	}

};

/*--------------------------- ProvDataView ----------------------------------*/

function ProvDataView(model, contentDiv, titleDiv, homeBtnDiv, backBtnDiv) {
	this._model = model;

	this.pageContent = contentDiv;
	this.pageHeaderTitle = titleDiv;
	this.pageHeaderHomeBtn = homeBtnDiv;
	this.pageHeaderBackBtn = backBtnDiv;

	var _this = this;
	var scanListTimer = null;
	var sysTimer = null;
	var selectedNw = null;

	this.pageHeaderHomeBtn.hide();
	this.pageHeaderBackBtn.hide();
	this.pageHeaderTitle.text("Please wait...");

	this.backToScanList = function(event, ui) {
		_this.selectedNw = null;
		_this.autoRefreshScanList(5000);
		_this.render({"state":"unconfigured", "event":"scanlist_updated"});
	};

	this.submit_clicked = function() {
		var nw = new Object();
		$.mobile.loading('show', {
			text: 'Please wait...',
			textVisible: true,
			textonly: true,
			html: ""	
		});

		if (_this.selectedNw[2] === 1 || _this.selectedNw[2] === 3||
			_this.selectedNw[2] === 4 || _this.selectedNw[2] === 5) {
			if ($("#show_pass").is(":checked") === true) {
				nw["key"] = $("#wpa_pass_plain").val();
			} else {
				nw["key"] = $("#wpa_pass_crypt").val();
			}
		}
		nw["ssid"] = _this.selectedNw[0];
	    _this._model.ssid = _this.selectedNw[0];
		nw["security"] = _this.selectedNw[2];
	        if (nw["security"] === 3)
		        nw["security"] = 5;
		nw["channel"] = _this.selectedNw[3];
		nw["ip"] = 1;

		_this._model.setNetwork(nw);
	};

	this.pinEntered = function() {
		var secure_pin;
		secure_pin = $("#pin").val();
		$.mobile.loading('show', {
			text: 'Please wait...',
			textVisible: true,
			textonly: true,
			html: ""
		});
	    _this._model.setPin(secure_pin);
	};


	this.scanEntrySelect = function(obj) {
		_this.cancelAutoRefreshScanList();
		_this.selectedNw = _this._model.getScanListEntry(obj.data.index);
		_this.renderSelectNetwork();
	};

	this.backToSelectedNetwork = function () {
		_this.renderSelectNetwork();
	};

	this.show = function(what) {
		this._model.changedState.attach(function(obj, stateobj) {
			_this.render(stateobj);
		});

		this.pageHeaderTitle.text("Please wait...");
		this.pageContent.html("");

		this._model.reinit();
		this._model.beginProv();
		return this;
	};
	this.autoRefreshSys = function(interval) {
		/* IE8 fails to pass additional parameters */
		if (typeof interval === "undefined")
			interval = 5000;
		_this._model.fetchSys(_this._model, 5);
		_this.sysTimer = setInterval(_this._model.fetchSys, interval, _this._model, 5);
	};
	this.doReinit = function() {
		_this._model.reinit();
	};


};

ProvDataView.prototype = {
	/* Maste render function */
	render : function(obj) {
		console.log("Render: " + obj.state + " Er:" + obj.err + " Ev:" + obj.event);
		$.mobile.loading('hide');
		
		switch (obj.state) {
		  case "unknown":
			  if (typeof obj.event !== "undefined" && obj.event === "begin_prov") {
				  this.renderPinPage();
			  } else if (obj.err === "conn_err") {
				  this.renderConnError();
			  } else if (obj.err == "device_configured")  {
				  this.renderConfigured();
			  }
			  break;
		  case "unconfigured":
			  if (typeof obj.event === "undefined" &&
				  typeof obj.err === "undefined") {
				  this.autoRefreshScanList(5000);
			  } else if (obj.event === "scanlist_updated")
					  this.renderScanResults();
			  else if (obj.err === "conn_err") {
				  this.renderConnError();
			  }
			  break;
		  case "nw_posted":
			  if (obj.err === "invalid_nw_param") {
				  this.renderInvalidNetworkParam();
			  } else if (obj.err === "conn_err") {
				  this.renderConnError();
			  } else {
				  this.pageHeaderHomeBtn.hide();
				  this.pageHeaderBackBtn.hide();
				  this.pageContent.html("");
				  this.pageHeaderTitle.text("Configuring...");
				  $.mobile.loading('show', {
					text: 'Please wait...',
					textVisible: true,
					textonly: true,
					html: ""	
				  });
			  }
			  break;
		  case "configured":
			  if (obj.err === "network_switched") {
				  this.renderConnError();
			  } else if (obj.err === "conn_err") {
				  this.renderConnLostAfterProv();
			  } else if (this._model.status === 1 ||
						 this._model.status === 0) {
				  if (obj.event === "sysdata_updated") {
					  this.renderConnecting();
				  } else {
				    console.log("AutoRefresh");
					  /* Make request after 1 sec as it may take longer to reflect
					     the correct status of provisioning */
					  setTimeout(this.autoRefreshSys, 1000, 5000);
				  }
			  } else if(this._model.status === 2) {
			          this._model.sendProvDoneAck(this._model);
				  this.renderConnected();
			  }
			  break;
		}
		return;
	},

	hide : function () {
		this._model.finish();
		this.cancelAutoRefreshScanList();
		this.cancelAutoRefreshSys();
		this._this = null;
		$.mobile.loading('hide');
		this.pageHeaderBackBtn.hide();
		this.pageHeaderHomeBtn.hide();
		this.pageContent.html("");
		this.pageHeaderTitle.text("");
	},

	get_img : function (rssi, nf, security) {
		var snr = rssi - nf;
		var level, img;

		if (snr >= 40) level = 3;
		else if (snr >= 25 && snr < 40) level = 2;
		else if (snr >= 15 && snr < 25) level = 1;
		else level = 0;

		img = "signal"+level;
		if (security) img += "L";
		img += ".png";

		return img;
	},

	get_security : function (security) {
		if (security === 0)
			return "Unsecured open network";
		else if (security === 1)
			return "WEP secured network";
		else if (security === 3)
			return "WPA secured network";
		else if (security === 4)
			return "WPA2 secured network";
		else if (security === 5)
			return "WPA/WPA2 Mixed secured network";
		else
			return "Invalid security";
	},

	renderInvalidNetworkParam : function() {
		this.pageHeaderHomeBtn.hide();
		this.pageContent.html("");

		this.pageHeaderTitle.text("Error");
		this.pageHeaderBackBtn.unbind("click");
		this.pageHeaderBackBtn.bind("click", this.backToSelectedNetwork);
		this.pageHeaderBackBtn.show();
		this.pageContent.append($("<h3/>")
								.append("Incorrect configuration parameters specified. Please retry."));

	},

	renderPinPage : function() {
		this.pageHeaderBackBtn.hide();
		this.pageContent.html("");

		this.pageHeaderTitle.text("Enter Pin");
		this.pageContent.append($("<label/>", {"for":"basic"})
						.append("Pin"));
		placeholder = "Pin";
		this.pageContent.append($("<div/>", {"id":"div_pin", "class":"ui-hide-label"})
						.append($("<input/>", {"type":"text", "id":"pin", "value":"",
						"placeholder":placeholder})));
	    this.pageContent.append($("<div/>").append($("<a/>", {"href":"#", "data-role":"button", "id":"submit-pin"})
						.append("Connect")));
		this.pageContent.trigger("create");
		$("#submit-pin").bind("click", this.pinEntered);
	},

	renderConnError : function() {
		this.pageHeaderBackBtn.hide();
		this.pageContent.html("");

		this.pageHeaderTitle.text("Error");
		this.pageHeaderHomeBtn.show();
		this.pageContent.append($("<h3/>")
						.append("The connection to the device has been lost." +
								" Please re-connect to the device network and reload" +
								" the page to restart the provisioning"));
	},

	renderConfigured : function() {
		this.pageHeaderBackBtn.hide();
		this.pageContent.html("");

		this.pageHeaderTitle.text("Status");
		this.pageHeaderHomeBtn.show();
		this.pageContent.append($("<h3/>")
						.append("The device is already provisioned with the configured network." +
								" Please reset the device network and reload" +
								" the page to restart the provisioning"));
	},

	renderConnLostAfterProv : function() {
		this.pageHeaderBackBtn.hide();
		this.pageHeaderHomeBtn.show();
		this.pageContent.html("");

		this.pageHeaderTitle.text("Success");
		this.pageHeaderHomeBtn.show();
		this.pageContent.append($("<h3/>")
						.append("The device has been configured with provided" +
								" settings. However this client has lost" +
								" the connectivity with the device.  Please reconnect to the device"+
								" or home network and try to reload this page. Please check status" +
								" indicators on the device to check connectivity."));
	},

	renderConnecting : function() {
		this.pageHeaderBackBtn.hide();
		this.pageHeaderHomeBtn.show();
		this.pageContent.html("");

		this.pageHeaderTitle.text("Success");

		var status = this._model.status;
		var failure = this._model.failure;
		var failure_cnt = this._model.failure_cnt;
		if (status === 1) {
            if (typeof failure === "undefined" || typeof failure_cnt === "undefined") {
                this.pageContent.append($("<h3/>")
					.append("The device is configured with provided settings. "+
						"The device is trying to connect to configured network."));
				$.mobile.loading('show', {
					text: 'Please wait...',
					textVisible: true,
                    textonly: true,
                    html: ""
                });
            } else {
					this.pageContent.append($("<h3/>")
									.append("The device is configured with provided settings. "+
											"However device can't connect to configured network."));
				if (failure === "auth_failed") {
					this.pageContent.append($("<p/>")
									.append("Reason: Authentication failure")
									.append($("<p/>")
									.append("Number of attempts:"+failure_cnt)));
				} else if (failure === "network_not_found") {
					this.pageContent.append($("<p/>")
									.append("Reason: Network not found")
									.append($("<p/>")
									.append("Number of attempts:"+failure_cnt)));
				}
				this.pageContent.append($("<h3/>")
								.append("Please reset the device to factory defaults"));

                                $.mobile.loading('hide');
			}
		}
	},

	renderConnected : function() {
		this.pageHeaderBackBtn.hide();
		this.pageHeaderHomeBtn.hide();
		this.pageContent.html("");
		
		var nw_name = this._model.ssid;
		this.pageHeaderHomeBtn.show();
		this.cancelAutoRefreshSys();

		this.pageHeaderTitle.text("Success");
		this.pageContent.append($("<h3/>")
						.append("The device is configured and connected to \""+nw_name+"\"."));
	},

	autoRefreshScanList : function(interval) {
		this._model.fetchScanList(this._model, 5);
		this.scanListTimer = setInterval(this._model.fetchScanList, interval, this._model, 5);
	},

	cancelAutoRefreshScanList : function() {
		if (this.scanListTimer !== null) {
			clearInterval(this.scanListTimer);
			this._model.cancelScanListRequest();
			this.scanListTimer = null;
		}
	},

	cancelAutoRefreshSys : function() {
		if (this.sysTimer) {
			clearInterval(this.sysTimer);
			this._model.cancelSysRequest();
			this.sysTimer = null;
		}
	},


	/* This function renders the scan results on to the page */
	renderScanResults : function() {
		this.pageHeaderBackBtn.hide();
		this.pageHeaderHomeBtn.hide();
		this.pageContent.html("");
		
		this.pageHeaderTitle.text("Provisioning");
		this.pageHeaderHomeBtn.show();

		var lv = $("<ul/>", {'data-role' : 'listview', 'data-inset' : true, 'id' : 'my-listview'});
		this.pageContent.append(lv);

		var scanList = this._model.getScanResults();
		
		for (var i = 0; i < scanList.length; i++) {
			var img = this.get_img(scanList[i][4], scanList[i][5], scanList[i][2]);
			lv.append($("<li/>", {'data-icon':false, 'data-theme':'c'})
					  .append($("<a/>", {'href':'#', 'data-rel':'dialog', 'id':'scanEntry'+i})
					  .append($("<p/>", {'class':'my_icon_wrapper'})
					  .append($("<img/>", {'src': ''+img })))
					  .append($("<h3/>")
					  .append(scanList[i][0]))
					  .append($("<p/>", {'class': 'ui-li-desc'})
					  .append(this.get_security(scanList[i][2])))));
			$("#scanEntry"+i).bind("click", {"index":i}, this.scanEntrySelect);
		}
		$("#my-listview").listview().listview('refresh');
		
	},

	checkbox_show_pass_changed : function() {
		if ($("#show_pass").is(":checked") === true) {
			$("#wpa_pass_plain").val($("#wpa_pass_crypt").val());
			$("#div_wpa_pass_crypt").hide();
			$("#div_wpa_pass_plain").show();
		} else {
			$("#wpa_pass_crypt").val($("#wpa_pass_plain").val());
			$("#div_wpa_pass_plain").hide();
			$("#div_wpa_pass_crypt").show();
		}	
	},

	getNetworkData : function() {
		var sel_network = this._model.scanList[this._model.selectedNetworkIndex];
		var nw = new Object();

		if (sel_network[2] === 3 || sel_network[2] === 4 || sel_network[2] === 5) {
			if ($("#show_pass").is(":checked") === true) {
				nw["key"] = $("#wpa_pass_plain").val();
			} else {
				nw["key"] = $("#wpa_pass_crypt").val();
			}
		}
		nw["ssid"] = sel_network[0];
		nw["security"] = sel_network[2];
	        if (sel_network[2] === 3)
		        nw["security"] = 5;
		/* nw["channel"] = sel_network[3]; */
		nw["ip"] = 1;
		return nw;
	},

	/* This function renders a selected network to entry passphrase */
	renderSelectNetwork : function() {
		this.pageHeaderHomeBtn.hide();
		this.pageHeaderBackBtn.unbind("click");
		this.pageHeaderBackBtn.bind("click", this.backToScanList);
		this.pageHeaderBackBtn.show();
		this.pageContent.html("");

		var network = this.selectedNw;
		this.pageHeaderTitle.text(network[0]);

		if (network[2] === 1 || network[2] === 3 || network[2] === 4 || network[2] === 5) {
			var placeholder;
			if (network[2] === 3 || network[2] === 4 || network[2] === 5) {
				this.pageContent.append($("<label/>", {"for":"basic"})
										.append("Passphrase"));
				placeholder = "Passphrase";
			} else {
				this.pageContent.append($("<label/>", {"for":"basic"})
										.append("WEP Key"));
				placeholder = "WEP Key";
			}
			this.pageContent.append($("<div/>", {"id":"div_wpa_pass_plain", "class":"ui-hide-label"})
									.append($("<input/>", {"type":"text", "id":"wpa_pass_plain", "value":"",
										"placeholder":placeholder})));
			this.pageContent.append($("<div/>", {"id":"div_wpa_pass_crypt", "class":"ui-hide-label"})
									.append($("<input/>", {"type":"password", "id":"wpa_pass_crypt", "value":"",
										"placeholder":placeholder})));
			this.pageContent.append($("<input/>", {"type":"checkbox", "id":"show_pass", "data-mini":"true", "class":"custom",
									  "data-theme":"c"}));
			this.pageContent.append($("<label/>", {"for":"show_pass"})
									.append("Unmask "+placeholder));
		}
		this.pageContent.append($("<div/>", {"class":"ui-grid-a"})
								.append($("<div/>",{"class":"ui-block-a"})
								.append($("<a/>", {"href":"#", "data-role":"button", "id":"cancel-btn", "data-theme":"c"})
								.append("Cancel")))
								.append($("<div/>",{"class":"ui-block-b"})
								.append($("<a/>", {"href":"#", "data-role":"button", "id":"submit-btn"})
								.append("Connect"))));
		this.pageContent.trigger("create");
		
		$("#div_wpa_pass_plain").hide();	
		$("#show_pass").bind("change", this.checkbox_show_pass_changed);
		$("#cancel-btn").bind("click", this.backToScanList);
		$("#submit-btn").bind("click", this.submit_clicked);
	}
};

var prov_init = function() {
	return new ProvDataModel();
};

var prov_show = function(provdata, what) {
	return new ProvDataView(provdata,
							$("#page_content"),
							$("#header #title"),
							$("#header #home"),
							$("#header #back")).show(what);
};

var padding8 = "00000000";
var pad = function(str, len) {
    var temp = "";
    var pad_str = padding8.slice(0, len);
    tmp_len = str.length;
    if (tmp_len != len) {
        temp = pad_str.slice(0, len - tmp_len);
    }
    var new_str = temp + str;
    return new_str;
}

var randomUInt8 = function() {
        return Math.floor((Math.random() * 256));
}

var randomUInt16 = function() {
        return Math.floor((Math.random() * 65536));
}

var randomUInt32 = function() {
        return Math.floor((Math.random() * 4294967295));
}

var decrypt_secure_data = function(data) {
	if (data.data == undefined || data.iv == undefined || data.checksum == undefined) {
		console.log("Failure: Bad request");
		return null;
	}

	var iv_str = data.iv;
	var data_str = data.data;
	var esp_checksum_str = data.checksum;

	var k = 0;
    var iv = new Array(4);
    for (i = 0; i < 32; i+=8) {
        var temp = "";
        var str = pad(iv_str.slice(i, i + 8), 8);
        iv[k] = parseInt(str, 16);
        k++;
    }

    var data_array = new Array();
    var data_len = data_str.length;
    var j = 0;
    var trim = 0;
    for (i = 0; i < data_len; i = i + 8) {
        var val  = data_str.slice(i, i + 8).toString(16);
        var val1 = parseInt(data_str.slice(i, i + 8).toString(16), 16);
        if (i + 8 > data_len) {
          /* This is required since last element of array should have complete 4 byte data.
          So, adding trailing 0s */
          trim = ((i + 8) - data_len);
          var x = data_str.slice(i, data_len).toString(16) + padding8.slice(0, trim);
          var val1 = parseInt(x, 16);
        }
        data_array[j] = val1;
        j++;
    }

    /* Decrypt the data using "shared secret" */
    cipher = new AES.CTR(shared_aes_key, iv);
    cipher.encrypt(data_array);

    var test = "";
    for (i = 0; i < (data_len/8); i++) {
         test += pad((data_array[i] >>> 0).toString(16), 8);
    }
    var str = '';
    for (var i = 0; i < test.toString().length; i += 2)
        str += String.fromCharCode(parseInt(test.toString().substr(i, 2), 16));
    var new_buf = str.slice(0, (str.length - trim/2));

    /* This is required since the last few bytes in the decrypted string may be invalid.
    So, need to be trimmed */

    /* Computing checksum and verifying it */
    var data_checksum = sjcl.hash.sha512.hash(new_buf.toString());

    j = 0;
    for (i = 0; i < 64; i += 8) {
        var tmp_data = parseInt(esp_checksum_str.slice(i, i + 8), 16)
        if (tmp_data.toString(16) != (data_checksum[j] >>> 0).toString(16))
        {
          console.log("Failure : Checksum Verification Failed");
          return null;
        }
        j++;
    }

    var jsonData = JSON.parse(new_buf.toString());

    return (jsonData);
}


var encrypt_data = function(data) {

	var esp = new Object();
	var data_str = JSON.stringify(data);
	var enc_data_str = "";
	var data_checksum_str = "";
	var data_array = new Array();
	var data_len = data_str.length;
	var iv = new Array(4);
	var iv_str = "";
	var len = 0, j = 0;

	  /* Generate the "iv" to be used for encryption randomly */
	 for (i = 0; i < 4; i++) {
	      iv[i] = randomUInt32();
	  }
	  for (i = 0; i < 4; i++) {

	      len = iv[i].toString(16).length;
	       if (len != 8) {
	         iv_str += pad(iv[i].toString(16), 8);
	       } else {
	         iv_str += iv[i].toString(16);
	      }
	  }
	  esp['iv'] = iv_str;

	  /* Encrypt the data using AES-CTR 128-bit using "shared secret" */
	  data_array = AES.Codec.strToWords(data_str);
	  cipher = new AES.CTR(shared_aes_key, iv);
	  var temp = cipher.encrypt(data_array);

	  for (i = 0; i < (temp.length); i++) {

	      len = (temp[i] >>> 0).toString(16).length;
	       if (len != 8) {
	         enc_data_str += pad((temp[i] >>> 0).toString(16), 8);
	       } else {
	         enc_data_str += (temp[i] >>> 0).toString(16);
	       }
	  }
	  esp['data'] = enc_data_str;

	  /* Computing checksum and writing it to the ESP */
	  esp_checksum = sjcl.hash.sha512.hash(data_str);
	  for (i = 0; i < (esp_checksum.length); i++) {

	      len = (esp_checksum[i] >>> 0).toString(16).length;
	       if (len != 8) {
	         data_checksum_str += pad((esp_checksum[i] >>> 0).toString(16), 8);
	       } else {
	         data_checksum_str += (esp_checksum[i] >>> 0).toString(16);
	       }
	  }
	  esp['checksum'] = data_checksum_str;

	  /* return the object */
	  return esp;
}

function sleep(milliseconds) {
    var start = new Date().getTime();
    for (var i = 0; i < 1e7; i++) {
        if ((new Date().getTime() - start) > milliseconds){
	  break;
        }
    }
}