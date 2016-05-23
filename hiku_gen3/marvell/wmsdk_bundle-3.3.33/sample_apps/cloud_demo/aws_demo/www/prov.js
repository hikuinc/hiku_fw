/*--------------------------- ProvDataModel ---------------------------------*/

function ProvDataModel() {
	this.sys = null;
	this.scanList = null;

	/* The states can be :
	 * "unknown" : The state is unknown (at beginning).
	 * "unconfigured" : The device is known to be not configured
	 * "nw_posted" : User has entered required credentials and posted network
	 * "configured" : The device is known to be configured 
	 * "reset_to_prov_done" : Reset to provisioning mode request has been sent
	 * "reset_to_prov_success" : Reset to provisioning successful
	 *
	 * The emitted errors can be:
	 * "conn_err" : Connection error; can't communicate with device
	 * "invalid_nw_param" : Invalid network parameters specified
	 */
	this.state = "unknown";
	this.pubCert = null;
        this.privKey = null;
        this.region = null;
        this.thing = null;

	/* Private members */
	var uuid = null;
	var failedSysGet = 0;
	var failedScanGet = 0;
	var scanListRequestAborted = false;
	var sysRequestAborted = false;
        var configurednotupdated = 0;

	this.changedState = new Event(this);
	this.stateMachine(this, "init");
};

ProvDataModel.prototype = {
	stateMachine : function(obj, event) {
		console.log("In stateMachine " + obj.state + " " + event);

		switch (obj.state) {
		  case "unknown":
			  if (event === "init") {
			          obj.certenabled(obj);
			  } else if (event === "cert_supported") {
				  obj.changedState.notify({"state" : obj.state, "event":"config_cert"});
			  } else if (event === "cert_entered") {
				  obj.configureRegion(obj);
				  obj.configureCert(obj);
				  obj.configureKey(obj);
			          obj.configureThing(obj);
			  } else if (event === "cert_config_success") {
				  obj.fetchSys(obj, 3);
			  } else if (event === "cert_config_failure") {
				  obj.changedState.notify({"state" : obj.state, "event":"config_cert"});
			  } else if (event === "sysdata_updated") {
				if (obj.sys.connection.station.configured === 0) {
					  /* Check if device is already configured. Also note the
					   * device uuid in case the connection gets switched to
					   * other device behind our back. */
					  obj.uuid = obj.sys.uuid;
					  obj.state = "unconfigured";
					  obj.changedState.notify({"state" : obj.state});
				  } else if (obj.sys.connection.station.configured === 1) {
					  obj.state = "configured";
					  obj.changedState.notify({"state" : obj.state});
				  }
			  } else if (event === "conn_err") {
				  obj.changedState.notify({"state" : obj.state, "err":"conn_err"});
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
				  } else if (obj.sys.connection.station.configured === 1) {
					  obj.changedState.notify({"state" : obj.state, "event":"sysdata_updated"});
				          configurednotupdated = 0;
				  } else if (obj.sys.connection.station.configured === 0) {
				          if (configurednotupdated < 3) {
					    configurednotupdated++;
					    return;
					  }
					  obj.state = "unconfigured";
					  obj.changedState.notify({"state" : obj.state});
				  }
			  } else if (event === "conn_err") {
				  obj.changedState.notify({"state" : obj.state, "err":"conn_err"});
			  } else if (event === "reset_to_prov_done") {
				  obj.state = "reset_to_prov_done";
				  obj.changedState.notify({"state" : obj.state});
			  } 
			  break;
		  case "reset_to_prov_done":
			  if (event === "reset_to_prov_success") {
				  obj.state = "reset_to_prov_success";
				  obj.changedState.notify({"state" : obj.state});
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

		this.stateMachine(this, "init");
	},

	configCert : function() {
		this.stateMachine(this, "config_cert");
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

	sendProvDoneAck : function () {
		var response = new Object();
		response.prov = new Object();
		response.prov.client_ack = 1;

		$.ajax({
			type: 'POST',
			url: '/sys',
			async : 'false',
			data : JSON.stringify(response),
			contentType : 'application/json',
			timeout : 5000
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
			obj.sys = new Object();
			$.extend(true, obj.sys, data);
			obj.stateMachine(obj, "sysdata_updated");
		};

		obj.sysRequestAborted = false;
		$.ajax({
			type: 'GET',
			url: '/sys',
			async : 'false',
			dataType : 'json',
		        cache: false,
			timeout : 5000,
			success : fetchDone,
			error: fetchFailed
		}).done();
	},

	cancelSysRequest : function() {
		this.sysRequestAborted = true;
	},

	certenabled : function(obj) {
		var certEnabled = function(data) {
		  console.log("Success : Cert supported");
		  obj.stateMachine(obj, "cert_supported");
		};

		var certDisabled = function(xhr, textStatus, error) {
			console.log("Error : Cert not supported");
		        obj.stateMachine(obj, "cert_config_success");
		};
		var postRegionUrl = '/aws_iot/pubCert';
		$.ajax({
			type: 'GET',
			url: postRegionUrl,
			async : 'false',
			dataType : 'json',
			contentType : 'application/json',
			timeout : 5000,
                        cache: false,
			success : certEnabled,
			error: certDisabled
		}).done();
	},

	configureRegion : function(obj) {
	        var configRegion = new Object();
	        configRegion["region"] = this.region;
		var configRegionDone = function(data) {
		  console.log("Success : Region configured successfully");
		  obj.stateMachine(obj, "cert_config_success");
		};

		var configRegionError = function(xhr, textStatus, error) {
			console.log("Error " + xhr.responseText);
		        obj.stateMachine(obj, "cert_config_failure");
		};
		var postRegionUrl = '/aws_iot/region';
		$.ajax({
			type: 'POST',
			url: postRegionUrl,
			data: JSON.stringify(configRegion),
			async : 'false',
			dataType : 'json',
			contentType : 'application/json',
			timeout : 5000,
			success : configRegionDone,
			error: configRegionError
		}).done();

	},

	configureCert : function(obj) {
	        var configCert = new Object();
       	        configCert["cert"] = this.pubCert;
		var configCertDone = function(data) {
		  console.log("Success : Certificate configured successfully");
		  obj.stateMachine(obj, "cert_config_success");
		};

		var configCertError = function(xhr, textStatus, error) {
			console.log("Error " + xhr.responseText);
		        obj.stateMachine(obj, "cert_config_failure");
		};
		var postCertUrl = '/aws_iot/pubCert';
		$.ajax({
			type: 'POST',
			url: postCertUrl,
			data: JSON.stringify(configCert),
			async : 'false',
			dataType : 'json',
			contentType : 'application/json',
			timeout : 5000,
			success : configCertDone,
			error: configCertError
		}).done();

	},

	configureThing : function(obj) {
	        var configKey = new Object();
	        configKey["key"] = this.privKey;
		var configKeyDone = function(data) {
		  console.log("Success : Key configured successfully");
		  obj.stateMachine(obj, "cert_config_success");
		};

		var configKeyError = function(xhr, textStatus, error) {
			console.log("Error " + xhr.responseText);
		        obj.stateMachine(obj, "key_config_failure");
		};
		var postKeyUrl = '/aws_iot/privKey';
		$.ajax({
			type: 'POST',
			url: postKeyUrl,
			data: JSON.stringify(configKey),
			async : 'false',
			dataType : 'json',
			contentType : 'application/json',
			timeout : 5000,
			success : configKeyDone,
			error: configKeyError
		}).done();
	},

	configureKey : function(obj) {
	        var configThing = new Object();
	        configThing["thing"] = this.thing;
		var configThingDone = function(data) {
		  console.log("Success : Thing configured successfully");
		  obj.stateMachine(obj, "thing_config_success");
		};

		var configThingError = function(xhr, textStatus, error) {
			console.log("Error " + xhr.responseText);
		        obj.stateMachine(obj, "thing_config_failure");
		};
		var postThingUrl = '/aws_iot/thing';
		$.ajax({
			type: 'POST',
			url: postThingUrl,
			data: JSON.stringify(configThing),
			async : 'false',
			dataType : 'json',
			contentType : 'application/json',
			timeout : 5000,
			success : configThingDone,
			error: configThingError
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
					// try again
					ajax_obj = this;
					setTimeout(function() { $.ajax(ajax_obj); }, retry_interval);
				}
			} else {
				if (obj.scanListRequestAborted)
					return;
				obj.connectionError();
			}
		};
		var fetchDone = function(data) {
			console.log("!!");
			obj.failedScanGet = 0;
			if (obj.scanListRequestAborted)
				return;
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
		console.log("!");
		$.ajax({
			type: 'GET',
			url: '/sys/scan',
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

	setCert : function(cert, key, region, thing) {
	        this.pubCert = cert;
	        this.region = region;
	        this.privKey = key;
        	this.thing = thing;
		this.stateMachine(this, "cert_entered");
	},

	resetToProvDone : function() {
		var obj = new Object();
		var obj_this = this;

		$.mobile.loading('show', {
			text: 'Please wait...',
			textVisible: true,
			textonly: true,
			html: ""	
		});

		obj.connection = new Object();
		obj.connection.station = new Object();
		obj.connection.station.configured = 0;

		var resetToProvSuccess = function(data) {
			obj_this.stateMachine(obj_this, "reset_to_prov_success");
		
		};

		var resetToProvFailed = function() {
			obj_this.stateMachine(obj_this, "conn_err");
		};

		this.stateMachine(this, "reset_to_prov_done");

		$.ajax({
			type: 'POST',
			url: '/sys',
			data: JSON.stringify(obj),
			async : 'false',
			dataType : 'json',
			contentType : 'application/json',
			timeout : 5000,
			success : resetToProvSuccess,
			error : resetToProvFailed
		}).done();
	},

	setNetwork : function(obj) {
		var obj_this = this;

		var setNetworkFailed = function(xhr, status, error) {
			obj_this.stateMachine(obj_this, "conn_err");
		};

		var setNetworkDone = function(data) {
			if (data.success === 0) {
				obj_this.stateMachine(obj_this, "nw_post_success");
			} else {
				obj_this.stateMachine(obj_this, "nw_post_invalid_param");
			}
		};

		$.ajax({
			type: 'POST',
			url: '/sys/network',
			data: JSON.stringify(obj),
			async : 'false',
			dataType : 'json',
			contentType : 'application/json',
			timeout : 5000,
			success : setNetworkDone,
			error : setNetworkFailed
		}).done();

		this.stateMachine(this, "nw_posted");
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
		nw["security"] = _this.selectedNw[2];
		nw["channel"] = _this.selectedNw[3];
		nw["ip"] = 1;

		_this._model.setNetwork(nw);
	};

	this.certEntered = function() {
		var cert;
                var key;
	        var region;
	        var thing;
		cert = $("#cert").val();
	        key = $("#key").val();
		region = $("#region").val();
	        thing = $("#thing").val();
	        if (cert.indexOf('-----BEGIN CERTIFICATE-----') === -1 ||
	            cert.indexOf('-----END CERTIFICATE-----') === -1) {
		    alert("Invalid certificate.\r\nPlease enter entire certificate including -----BEGIN CERTIFICATE----- to -----END CERTIFICATE-----");
		  return;
		}
	        if (key.indexOf('-----BEGIN RSA PRIVATE KEY-----') === -1 ||
	            key.indexOf('-----END RSA PRIVATE KEY-----') === -1) {
		    alert("Invalid private key.\r\nPlease enter entire key including -----BEGIN RSA PRIVATE KEY----- to -----END RSA PRIVATE KEY");
		  return;
		}

		$.mobile.loading('show', {
			text: 'Please wait...',
			textVisible: true,
			textonly: true,
			html: ""
		});
	    _this._model.setCert(cert, key, region, thing);
	};

	this.scanEntrySelect = function(obj) {
		_this.cancelAutoRefreshScanList();
		_this.selectedNw = _this._model.getScanListEntry(obj.data.index);
		_this.renderSelectNetwork();
	};

	this.resetToProvIntended = function () {
		_this.renderResetToProv();
	};

	this.resetToProvDone = function() {
		_this._model.resetToProvDone();
	};
	
	this.backFromResetToProv = function() {
		_this.render({"state":"configured"});
	};

	this.backToSelectedNetwork = function () {
		_this.renderSelectNetwork();
	};

	this.show = function(what) {
		this._model.changedState.attach(function(obj, stateobj) {
			_this.render(stateobj);
		});
		if (what === "reset_to_prov") {
			this.renderResetToProv(what);
		} else {
			this.pageHeaderTitle.text("Please wait...");
			this.pageContent.html("");

			this._model.reinit();
		  this._model.configCert();
		}
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
		          if (typeof obj.event !== "undefined" && obj.event == "config_cert") {
			          this.renderCertPage();
		          } else if (obj.err === "conn_err") {
				  this.renderConnError();
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
			  } else if (this._model.sys.connection.station.status === 1 || 
						 this._model.sys.connection.station.status === 0) {
				  if (obj.event === "sysdata_updated")
					  this.renderConnecting();
				  else {
					  /* Make request after 1 sec as it may take longer to reflect
					     the correct status of provisioning */
					  setTimeout(this.autoRefreshSys, 1000, 5000);
				  }
			  } else if(this._model.sys.connection.station.status === 2) {
                                  this._model.sendProvDoneAck();
				  this.renderConnected();
                          }
			  break;
		  case "reset_to_prov_done":
			  if (obj.err === "conn_err") {
				  this.renderConnError();
			  }
			  break;
		  case "reset_to_prov_success":
			  this.renderResetToProvSuccess();
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

	renderCertPage : function() {
		this.pageHeaderBackBtn.hide();
		this.pageContent.html("");

	        var input = document.createElement("textarea");
	        input.name = "cert";
	        input.maxLength = "5000";
	        input.cols = "120";
	        input.rows = "40";

		this.pageHeaderTitle.text("AWS Starter Demo Configuration");
		this.pageContent.append($("<label/>", {"for":"basic"})
						.append("Thing Name"));
		placeholder = "starterkit1";
		this.pageContent.append($("<div/>", {"id":"div_thing", "class":"ui-hide-label"})
					.append($("<textarea/>", {"type":"text", "id":"thing", "value":"", "cols":40, "rows":1,
						"placeholder":placeholder})));
		this.pageContent.append($("<label/>", {"for":"basic"})
						.append("Region"));
		placeholder = "us-east-1";
		this.pageContent.append($("<div/>", {"id":"div_region", "class":"ui-hide-label"})
					.append($("<textarea/>", {"type":"text", "id":"region", "value":"", "cols":40, "rows":1,
						"placeholder":placeholder})));
		this.pageContent.append($("<label/>", {"for":"basic"})
						.append("Certificate"));
		placeholder = "-----BEGIN CERTIFICATE-----";
		this.pageContent.append($("<div/>", {"id":"div_cert", "class":"ui-hide-label"})
					.append($("<textarea/>", {"type":"text", "id":"cert", "value":"", "cols":40, "rows":40,
						"placeholder":placeholder})));
		this.pageContent.append($("<label/>", {"for":"basic"})
						.append("Private Key"));
		placeholder = "-----BEGIN RSA PRIVATE KEY-----";

		this.pageContent.append($("<div/>", {"id":"div_key", "class":"ui-hide-label"})
						.append($("<textarea/>", {"type":"text", "id":"key", "value":"", "cols":40, "rows":40,
						"placeholder":placeholder})));
	    this.pageContent.append($("<div/>").append($("<a/>", {"href":"#", "data-role":"button", "id":"submit-cert"})
						.append("Submit")));
		this.pageContent.trigger("create");
		$("#submit-cert").bind("click", this.certEntered);
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

		console.log(this._model.sys);
		var status = this._model.sys.connection.station.status;
		var failure = this._model.sys.connection.station.failure;
		var failure_cnt = this._model.sys.connection.station.failure_cnt;

		console.log(status + " " + failure + " " + failure_cnt);

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
							.append("Press reset to factory button for more than 10 seconds to reset the device"));
/*				this.pageContent.append($("<h3/>")
								.append("Click ")
								.append($("<a/>", {"href":"#", "id":"reset_prov"})
								.append("here"))
								.append(" to
                       reset to provisioning mode."));
*/
				$("#reset_prov").bind("click", this.resetToProvIntended);
                                $.mobile.loading('hide');
			}
		}
	},


	renderResetToProvSuccess : function() {
		this.pageHeaderBackBtn.hide();
		this.pageHeaderHomeBtn.show();
		this.pageContent.html("");

		this.pageHeaderTitle.text("Success");
		if (this._model.sys["interface"] === "station") {
			this.pageContent.append($("<h3/>").append("The device has been successfully reset to provisioning mode. Please reconnect to the device network and refresh."));
		} else if (this._model.sys["interface"] === "uap") {
			this.pageContent.append($("<h3/>")
							.append("The device has been successfully reset to provisioning mode" +
									". You shall be automatically redirected to select the network."));
			$.mobile.loading('show', {
				text: 'Please wait...',
				textVisible: true,
				textonly: true,
				html: ""	
			});
			setTimeout(this.doReinit, 5000, this);
		}
	},

	renderResetToProv : function(what) {
		var obj_this = this;

		this.pageHeaderTitle.text("Reset to Provisioning");
		$.mobile.loading('hide');
		if (what === "reset_to_prov") {
			this.pageHeaderHomeBtn.show();
			this.pageHeaderBackBtn.hide();
		} else  {
			this.pageHeaderHomeBtn.hide();
			this.pageHeaderBackBtn.unbind("click");
			this.pageHeaderBackBtn.bind("click", this.backFromResetToProv);
			this.pageHeaderBackBtn.show();
		}
		this.pageContent.html("");

		this.cancelAutoRefreshSys();

		this.pageContent.append($("<div/>", {"class":"ui-grid-a"})
								.append($("<div/>",{"class":"ui-block-a"})
								.append($("<a/>", {"href":"#", "data-role":"button", "id":"reset-cancel-btn", "data-theme":"c"})
								.append("Cancel")))
								.append($("<div/>",{"class":"ui-block-b"})
								.append($("<a/>", {"href":"#", "data-role":"button", "id":"reset-to-prov-btn"})
								.append("Reset"))));
		this.pageContent.trigger("create");
		if (what === "reset_to_prov") {
			$("#reset-cancel-btn").bind("click", function() {
				/* If we are directly showing reset to prov; we need to tie cancel button to the
				 * clicking home button */
				obj_this.pageHeaderHomeBtn.trigger("click");
			});
		} else {
			$("#reset-cancel-btn").bind("click", this.backFromResetToProv);
		}
		$("#reset-to-prov-btn").bind("click", this.resetToProvDone);
	},	

	renderConnected : function() {
		this.pageHeaderBackBtn.hide();
		this.pageHeaderHomeBtn.hide();
		this.pageContent.html("");
		
		var nw_name = this._model.sys.connection.station.ssid;
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

		console.log(this);	
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

