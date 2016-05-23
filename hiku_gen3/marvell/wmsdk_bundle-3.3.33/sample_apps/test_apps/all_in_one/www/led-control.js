function PSDataModel() {
    this.state = null;

    var fetchStateAborted = false;

    this.changedState = new Event(this);
    this.init();
};

PSDataModel.prototype = {
    init : function() {
	this.changedState.notify({"event" : "init"});
    },

    fetchState : function (obj, max_failures, retry_interval) {
        var ajax_obj = null;

        if (typeof retry_interval === 'undefined')
            retry_interval = 3000;

        var fetchFailed = function() {
            if (!(typeof max_failures === 'undefined')) {
                obj.failedStateGet++;
                if (max_failures === obj.failedStateGet) {
		    obj.changedState.notify({"error" : "conn_err"});
                } else {
                    // try again
                    ajax_obj = this;
		    setTimeout(function() { $.ajax(ajax_obj); }, retry_interval);
                }
            } else {
                if (obj.fetchStateAborted)
                    return;
		obj.changedState.notify({"error" : "conn_err"});
            }
        };

        var fetchDone = function(data) {
            if (obj.fetchStateAborted)
                return;
            console.log(data);
            obj.failedStateGet = 0;
            obj.state = data.state;
	    obj.changedState.notify({"state" : obj.state});
        };

        this.fetchStateAborted = false;
        $.ajax({
            type: 'GET',
            url: '/led-state',
	    async : 'true',
	    dataType : 'json',
	    timeout : 5000,
	    success : fetchDone,
            error: fetchFailed,
        }).done();
    },

    abortFetchState : function() {
        this.fetchStateAborted = true;
    },

    setPSState : function(state) {
        var obj = new Object();
        var this_obj = this;
        obj.state = state;

        var setStateErr = function() {
	    this_obj.changedState.notify({"error" : "conn_err"});
        };

        $.ajax({
            type: 'POST',
            url: '/led-state',
	    async : 'false',
	    contentType : 'application/json',
	    data : JSON.stringify(obj),
	    timeout : 5000,
            error: setStateErr
        }).done();

    }
}

function PSDataView(model, contentDiv, titleDiv, homeBtnDiv, backBtnDiv) {
    this._model = model;
    this.provState = null;

    var _this = this;

    this.pageContent = contentDiv;
    this.pageHeaderTitle = titleDiv;
    this.pageHeaderHomeBtn = homeBtnDiv;
    this.pageHeaderBackBtn = backBtnDiv;

    this.stateChanged = function() {
        var state = parseInt($("#led-state-flip").val());
	console.log("stateChanged "+state);
        _this._model.setPSState(state);
    };


    this._model.changedState.attach(function(obj, stateobj) {
        _this.render(stateobj);
    });

    this.show = function() {
	_this.render({"event":"init"});
        return _this;
    }

    var fetchStateTimer = null;

    this.stopSelfRefresh = function() {
        if (_this.fetchStateTimer) {
            console.log("stopping timer");
            clearInterval(_this.fetchStateTimer);
            _this._model.abortFetchState();
            _this.fetchStateTimer = null;
        }
    }

    this.showProv = function(provdata, what) {
        var label;
        if (what === "reset_to_prov") {
            label = "Reset to Provisioning";
            $("#prov-btn").bind('click', function() {
                _this.stopSelfRefresh();
                provdataview = prov_show(provdata, "reset_to_prov");
            });
            $("#cloud-li").show();
        } else if (what === "prov") {
            label = "Provisioning";
            $("#prov-btn").bind('click', function() {
                _this.stopSelfRefresh();
                provdataview = prov_show(provdata);
            });
            $("#cloud-li").hide();
        } else {
            return;
        }

        $("#prov-btn").html(label);
        $("#div-prov-btn").show();

    }
};

var loaded = 0;
PSDataView.prototype = {
    render : function(obj) {
        if (obj.event === "init") {
            this.renderInit(obj);
            this._model.fetchState(this._model, 3);
            return;
        }
        if (!(typeof obj.error === "undefined")) {
            this.renderError(obj);
            return;
        }
        if (obj.state === 1 || obj.state === 0) {
            this.renderStateChange(obj);
            if (this.provState === "unconfigured")
                this.show_prov();
        }
    },

    renderError : function(obj) {
        this.pageContent.html("");
        if (obj.error === "conn_err")
            this.pageContent.append($("<h3/>").append("Connection Error"));
        this.pageContent.trigger("create");
    },

    renderStateChange : function(obj) {
        if (obj.state == 1) {
            $("#led-state-flip").slider("enable");
            $("#led-state-flip").val(1).slider("refresh");
        } else {
            $("#led-state-flip").slider("enable");
            $("#led-state-flip").val(0).slider("refresh");
        }
        this.pageContent.trigger("create");
        this.fetchStateTimer = setTimeout(this._model.fetchState, 3000, this._model, 3);
        return;
    },

    renderInit : function(obj) {
        this.pageHeaderBackBtn.hide();
        this.pageHeaderHomeBtn.hide();
        this.pageContent.html("");

	this.pageContent.append($("<ul/>", {"data-role":"listview","data-inset":true, "data-divider-theme":"d", "id":"ps-control"})
				.append($("<li/>", {"data-role":"list-divider"})
					.append("LED Control"))
				.append($("<li/>", {"data-role": "fieldcontain", "data-theme": "c"})
					.append($("<label/>", {"for": "led-state-flip"})
						.append("State"))
					.append($("<select/>", {"id": "led-state-flip", "data-track-theme": "c", "data-theme": "b", "data-role": "slider"})
						.append($("<option/>", {"value": 0}).append("Off"))
						.append($("<option/>", {"value": 1}).append("On")))));

        this.pageContent.append($("<ul/>", {"data-role": "listview", "data-inset": true, "data-divider-theme": "d", id: "ul-prov-btn"})
				.append($("<li/>", {"data-role": "fieldcontain", "data-theme": "c"})
					.append($("<a/>", {"href": "#", "id": "prov-btn", "data-theme": "c", "data-shadow": "false"}))))
            .append($("<ul/>", {"data-role": "listview", "data-inset": true, "data-divider-theme": "d", id: "ul-cloud-btn"})
                    .append($("<li/>", {"id": "cloud-li", "data-role": "fieldcontain", "data-theme": "c"})
                            .append($("<a/>", {"href": "/cloud_ui", "target": "new", "id": "cloud-ui-btn", "data-theme": "c", "data-shadow": "false"})
                                    .append("Cloud UI"))));

        $("#div-prov-btn").hide();
        $("#cloud-li").hide();

        $("#led-state-flip").bind('change', this.stateChanged);
        $("#led-state-flip").slider();

        $("#led-state-flip").slider("disable");
        if (loaded) {
            $("#ul-prov-btn").listview().listview('refresh');
            $("#ul-cloud-btn").listview().listview('refresh');
        }
        loaded = 1;
    },

    hide : function() {
        this.stopSelfRefresh();
        this._this = null;
    }

}

var provdata;
var provdataview;
var psdata;
var psdataview;

var show = function() {
    provdata = prov_init();

    psdata = new PSDataModel();
    psdataview = new PSDataView(psdata,
				$("#page_content"),
				$("#header #title"),
				$("#header #home"),
				$("#header #back")).show();

    provdata.changedState.attach(function(obj, state) {
	console.log("App: state = "+obj.state);
        if (obj.state === "unconfigured")
            psdataview.showProv(provdata, "prov");
        if (obj.state === "configured")
            psdataview.showProv(provdata, "reset_to_prov");
    });

}

var pageLoad = function() {
    commonInit();
    $("#home").bind("click", function() {
        provdataview.hide();
        provdata.destroy();
        psdataview.hide();
        psdata = null;
        show();
    });

    show();
}
