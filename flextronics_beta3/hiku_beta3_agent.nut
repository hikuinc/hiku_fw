// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.

enum AudioStates {
  AudioRecording,  // Hiku's button has been pressed and audio is being recorded.
  AudioFinished,   // The button has been released and the audio has to be interpreted by
                   // the server. This is the case if only audio was recorded or if the audio
		   // is to add an entry to an unknown barcode (superscan).
  AudioFinishedBarcode, // The button has been released and the server can ignore the audio
  AudioError       // The most recent audio recording encountered an error. This state is also
                   // used on agent initialization.
}

if (!("nv" in getroottable()))
{
    nv <- { 
            gImpeeId = "", 
    	    gChargerState = "removed" , 
    	    gLinkedRecordTimeout = null,
			gBootTime = 0.0,
			gSleepDuration = 0.0,
			gWakeUpReason = 0x0000,
			gBatteryLevel = 0.0,
			gFwVersion = 0.0,
			at_factory = false,
	        macAddress = null,
			extendTimeout = 0.0
    	  };
}

// use a round robin table to do logging
const MAX_TABLE_COUNT = 5; // going to have 5 buffers
const MAX_LOG_COUNT = 30;
gLogTableIndex <- 0;
gLogTable <- [{count=0,data=""},
	      {count=0,data=""},
	      {count=0,data=""},
	      {count=0,data=""},
	      {count=0,data=""}];

server.log(format("Agent started, external URL=%s at time=%ds", http.agenturl(), time()));

gAgentVersion <- "1.3.07";

gExtendTimer <- null;

gAudioState <- AudioStates.AudioError;
gAudioAbort <- false;

// Byte pointer indicating the number of bytes of audio data
// the server has picked up from the Imp agent with HTTP GETs
gAudioReadPointer <- 0;

// Audio token issued by the server as an identifier for the 
// transaction
gAudioToken <- "";

// Global variable for measuring transaction time of list POSTs 
gTransactionTime <- 0;

// Byte string containing the audio data for the current session
gAudioString <- "";

gChunkCount <- 0; // Used to verify that we got the # we expected
gLinkedRecord <- ""; // Used to link unknown UPCs to audio records. 
                     // We will set this when we get a request to do
                     // so from the server, then clear it after we 
                     // send the next audio beep, after next barcode, 
                     // or after a timeout

gEan <- ""; // stored EAN for a superscan
                     
local boot_reasons = [
						"accelerometer", 
						"charger_status", 
						"button", 
						"touch", 
						"not-used", 
						"not-used",
						"not-used",
						"charger-det", 
					  ];

gAuthData <-{
			    app_id="e3a8ccb635d08ce76b407ec644",
    			secret="c923b1e09386",
			};
			
gMixPanelData <- {
		    url="https://api.mixpanel.com/track/",
		    token="d4dca732e8c83981405c87c65ae3e834",
		 };

const MIX_PANEL_EVENT_DEVICE_BUTTON = "DeviceButton";
const MIX_PANEL_EVENT_BARCODE = "DeviceScan";
const MIX_PANEL_EVENT_SPEAK = "DeviceSpeak";
const MIX_PANEL_EVENT_CHARGER = "DeviceCharger";
const MIX_PANEL_EVENT_BATTERY = "DeviceBatteryLevel";
const MIX_PANEL_EVENT_CONNECTIVITY = "DeviceConnectivity";
const MIX_PANEL_EVENT_CONFIG = "DeviceConfig";
const MIX_PANEL_EVENT_STATUS = "DeviceStatus";
const MIX_PANEL_EVENT_BUTTON_TIMEOUT = "DeviceButtonTimeout";

// Heroku server base URL	
gBaseUrl <- "https://app.hiku.us/api/v1";
gFactoryUrl <- "https://hiku-mfg.herokuapp.com/api/v1";

gServerUrl <- gBaseUrl + "/list";	

gBatteryUrl <- gBaseUrl + "/device";

gLogUrl <- gBaseUrl + "/log";

gSetupUrl <- gBaseUrl + "/apps";

gAudioUrl <- gBaseUrl + "/audio";

const BATT_0_PERCENT = 43726.16;

// prefixes indicating special barcodes with minimum
// and maximum barcode lengths in characters
const PREFIX_IMP_MAC = "0c2a69";
const PREFIX_IMP_LABELING = ".HCMGETMAC";
const PREFIX_GENERAL = ".HFB";

const TEST_CMD_PACKAGE = "package";
const TEST_CMD_PRINT_LABEL = "label";

gSpecialBarcodePrefixes <- [{
	// MAC addresses of Electric Imp modules printed as barcodes at the factory
	prefix = PREFIX_IMP_MAC,
	min_len = 12,
	max_len = 12,
	url = gFactoryUrl + "/factory"},{
	// barcode for generating a 2D datamatrix barcode for the scanner at the
        // label printer in production	
	prefix = PREFIX_IMP_LABELING,
	min_len = 10,
	max_len = 10,
	url = gFactoryUrl + "/factory"},{
	// general special barcodes
	prefix = PREFIX_GENERAL,
	min_len = 5,
	max_len = 64,
	url = null
    }];

//======================================================================
function sendMixPanelEvent(event, parameters)
{
/*
  // Disabled for now
  local defaultParam = (parameters)?parameters:{};
  defaultParam["token"] <- gMixPanelData.token;
  defaultParam["distinct_id"] <- nv.gImpeeId;
  defaultParam["time"] <- time();
  
  local data = {
	    "event":event,
	    "properties":defaultParam,
         };
	 
  local request = {data=http.base64encode(http.jsonencode(data))};
  
    // Create and send the request
  local req = http.post(
            gMixPanelData.url,
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            http.urlencode(request));
            
    // If the server is down, this will block all other events
    // until it times out.  Events seem to be queued on the server 
    // with no ill effects.  They do not block the device.  Could consider 
    // moving to async. The timeout period (tested) is 60 seconds.  
    req.sendasync(onMixPanelEventComplete);
  */
}

function onMixPanelEventComplete(m)
{
    if (m.statuscode != 200)
    {
        server.log(format("AGENT: MixPanelEvent: Error: got status code %d, expected 200", 
                    m.statuscode));
    }
    else
    {
      local body = http.jsondecode(m.body);
      server.log(body);
      server.log("MixPanelEventSuccess");
    }
}

//======================================================================
// Beep handling 

// Send Device Status to Hiku Server
function sendDeviceEvents(data)
{
    local disableSendToServer = false;
    //disableSendToServer = true;
    if (disableSendToServer)
    {
        server.log("AGENT: (sending to hiku server not enabled)");
        return;
    }
   
    // URL-encode the whole thing
    //data = http.urlencode(data);
    
    local timeStr = getUTCTime();
    local mySig = http.hash.sha256(gAuthData.app_id+gAuthData.secret+timeStr);
    mySig = BlobToHexString(mySig);       
    
	data = {    
            "sig": mySig,
            "time": timeStr, 
            "app_id": gAuthData.app_id,
			serialNumber = nv.gImpeeId,
			 logData = http.jsonencode(data) };
			 	 
    data = http.urlencode( data );
    server.log("AGENT: "+data);


    // Create and send the request
    server.log("AGENT: Sending Event to server...");
    local req = http.post(
            //"http://199.115.118.221/cgi-bin/addDeviceEvent.py",
            //"http://srv2.hiku.us/cgi-bin/addDeviceEvent.py",
            gLogUrl,
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            data);

    req.sendasync(onComplete);
}

function sendBatteryLevelToHikuServer(data)
{
    local disableSendToServer = false;
    local urlToPut = gBatteryUrl + "/" + nv.gImpeeId;
    local newData;
    
    //sendMixPanelEvent(MIX_PANEL_EVENT_BATTERY,{"batteryLevel":data.batteryLevel});
    
    server.log(format("AGENT: Battery Level URL: %s", urlToPut));
    //disableSendToServer = true;
    if (disableSendToServer)
    {
        server.log("AGENT: (sending to hiku server not enabled)");
        return;
    }
    
    local timeStr = getUTCTime();
    local mySig = http.hash.sha256(gAuthData.app_id+gAuthData.secret+timeStr);
    mySig = BlobToHexString(mySig);    
        
    // URL-encode the whole thing
    //data = http.jsonencode(data);
    
    newData = {
    			"batteryLevel":data.batteryLevel,
    			"token": nv.gImpeeId,
                "sig": mySig,
                "time": timeStr,              
                "app_id": gAuthData.app_id
    		  };
    
    data = http.urlencode( newData );
    server.log("AGENT: "+data);

    // Create and send the request
    server.log("AGENT: Sending Event to server...");
    local req = http.put(
            //"http://199.115.118.221/cgi-bin/addDeviceEvent.py",
           // "http://srv2.hiku.us/cgi-bin/addDeviceEvent.py",
           urlToPut,
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            data);

    req.sendasync(onCompleteEvent);
}

function onCompleteEvent(m)
{
    if (m.statuscode != 200)
    {
        server.log(format("AGENT: Battery Event: Error: got status code %d, expected 200", 
                    m.statuscode));
    }
    else
    {

        // Parse the response (in JSON format)
        local body = http.jsondecode(m.body);
        local body = body.response;
        try 
        {
            // Handle the various non-OK responses.  Nothing to do for "ok". 
            if (body.status != "ok")
            {
		server.log(format("AGENT: Battery Event - Error: %s", body.errMsg));
            }
        }
        catch(e)
        {
            server.log(format("AGENT: Battery Event - Caught exception: %s", e));
        }
    }
}

function mixPanelEvent(event,extraParam)
{
  local data = {};
  data[event] <- extraParam;
  return data;
}

//**********************************************************************
// Send audio and/or barcode to hiku's server
function sendBeepToHikuServer(data)
{
    //server.log(format("AGENT: Audio get at %d", gAudioReadPointer));

    local disableSendToServer = false;
    local newData = null;
    
    local is_superscan = false;
    //disableSendToServer = true;
    if (disableSendToServer)
    {
        agentLog("(sending to hiku server not enabled)");
        gAudioState = AudioStates.AudioError;
        return;
    }
    
    if(data.scandata != "") {
	if (handleSpecialBarcodes(data)) {
	    if (gAudioState != AudioStates.AudioError) {
		gAudioState = AudioStates.AudioFinishedBarcode;
	    }
	    return;
	}
    }
    
    local timeStr = getUTCTime();
    local mySig = http.hash.sha256(gAuthData.app_id+gAuthData.secret+timeStr);
    mySig = BlobToHexString(mySig);
    
    server.log(format("Current Impee Id=%s Valid ImpeeId=%s",nv.gImpeeId, data.serial));
    nv.gImpeeId = data.serial;
        
    // Special handling for audio beeps
    if (data.scandata == "")
    {
        // If not expired, attach the current linkedrecord (usually 
        // blank). Then reset the global. 
        agentLog("checking if linked record");
        if (nv.gLinkedRecordTimeout && time() < nv.gLinkedRecordTimeout)
        {
            agentLog("record linked");
            data.scandata = gLinkedRecord;
	          is_superscan = true;
        }
        gLinkedRecord = ""; 
        nv.gLinkedRecordTimeout = null;
    }
    
    if ( data.scandata == "" )
    {
    	newData = {
    		        "audioToken": gAudioToken,
    			      "audioType": "alaw",
    			      "token": nv.gImpeeId,
                "sig": mySig,
                "app_id": gAuthData.app_id,
                "time": timeStr,
    		  };  
    	if (gAudioState != AudioStates.AudioError) {
	  gAudioState = AudioStates.AudioFinished;
	  sendMixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"AudioFinished"});
	  //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"AudioFinished"}));
	}
    }
    else 
    {
      if (is_superscan) {
	  newData = {
	    "ean":format("%s",data.scandata),
	    "audioToken": gAudioToken,
	    "audioType": "alaw",    			
	    "token": nv.gImpeeId,
	    "sig": mySig,
	    "app_id": gAuthData.app_id,
	    "time": timeStr,
	  };   
	if (gAudioState != AudioStates.AudioError) {
	  gAudioState = AudioStates.AudioFinished;
	  sendMixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"CrowdSourced","barcode":data.scandata});
	  //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"CrowdSourced","barcode":data.scandata}));
	}
      } else {
	  newData = {
	    "ean":data.scandata,
	    "audioType": "alaw",    			
	    "token": nv.gImpeeId,
	    "sig": mySig,
	    "app_id": gAuthData.app_id,
	    "time": timeStr,
	  };   
    	   if (gAudioState != AudioStates.AudioError) {
	          gAudioState = AudioStates.AudioFinishedBarcode;
	       }
	    }
	    sendMixPanelEvent(MIX_PANEL_EVENT_BARCODE,{"status":"Scanned", "barcode":data.scandata});
	    //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_BARCODE,{"status":"Scanned", "barcode":data.scandata}));
      // save EAN in case of a superscan
      gEan = newData.ean;
    }
    // URL-encode the whole thing
    newData = http.urlencode(newData);
    // Create and send the request
    agentLog("Sending beep to server...");
    local req = http.post(
            //"http://bobert.net:4444", 
            //"http://www.hiku.us/sand/cgi-bin/readRawDeviceData.py", 
            //"http://199.115.118.221/scanner_1/imp_beep",
            gServerUrl,
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            newData);
            
    // If the server is down, this will block all other events
    // until it times out.  Events seem to be queued on the server 
    // with no ill effects.  They do not block the device.  Could consider 
    // moving to async. The timeout period (tested) is 60 seconds.  
    gTransactionTime = time();
    req.sendasync(onBeepReturn);
}

function onBeepReturn(res) {

    gTransactionTime = time() - gTransactionTime;
    agentLog(format("Server transaction time: %ds", gTransactionTime));

    // Handle the response
    local returnString = "success-server";

    if (res.statuscode != 200)
    {
        returnString = "failure"
        agentLog(format("Beep Error: got status code %d, expected 200", 
                    res.statuscode));
    }
    else
    {
        // Parse the response (in JSON format)
        local body = http.jsondecode(res.body);
		local body = body.response;
        try 
        {
            // Handle the various non-OK responses.  Nothing to do for "ok". 
            if (body.status != "ok")
            {
                // Possible causes: speech2text failure, unknown.
                if (body.errMsg == "EAN_NOT_FOUND")
                {
                    gLinkedRecord = gEan;
                    nv.gLinkedRecordTimeout = time()+10; // in seconds
                    returnString = "unknown-upc";
                    agentLog("Response: unknown UPC code");
                }
                else
                {
                    returnString = "failure";
                }
				agentLog(format("Beep Error: %s",http.jsonencode(body)));
            }
        }
        catch(e)
        {
            agentLog(format("Beep Error: Caught exception: %s", e));
            returnString = "failure";
        }
    }

    // Return status to device
    // TODO: device.send will be dropped if response took so long that 
    // the device went back to sleep.  Handle that?
    local event = (gAudioState == AudioStates.AudioFinished)?MIX_PANEL_EVENT_SPEAK:MIX_PANEL_EVENT_BARCODE;
    sendMixPanelEvent(event,{"barcode":gEan,"status":returnString});
    //sendDeviceEvents(mixPanelEvent(event,{"barcode":gEan,"status":returnString}));
    device.send("uploadCompleted", returnString);
}

// Send the barcode to hiku's server
function handleSpecialBarcodes(data)
{
    local barcode = data.scandata;
    local req = null;
    local is_special = false;
    local dataToSend = null;
    // test the barcode against all known special barcode prefixes
    for (local i=0; i<gSpecialBarcodePrefixes.len() && !is_special; i++) {
	local prefix = gSpecialBarcodePrefixes[i]["prefix"];
	local min_len = gSpecialBarcodePrefixes[i]["min_len"];
	local max_len = gSpecialBarcodePrefixes[i]["max_len"];
	local barcode_len = barcode.len();
	if ((barcode_len >= min_len) && (barcode_len <= max_len)) {
	    local temp = barcode.slice(0,prefix.len());
  	    server.log("Original Barcode: "+barcode+" Sliced Barcode: "+temp);
	    if (temp == prefix)
		switch (prefix) {
	    case PREFIX_IMP_MAC :
		server.log(format("Scanned MAC %s", barcode));
		if (!nv.at_factory) {
		    return false;
		}
		dataToSend = {
			"macAddress": nv.macAddress,
			"serialNumber": nv.gImpeeId,
			"scannedMacAddress": barcode,
			"command": TEST_CMD_PACKAGE
    		    };
		is_special = true;
		local json_data = http.jsonencode (dataToSend);
		server.log(json_data);
		req = http.post(
		    gSpecialBarcodePrefixes[i]["url"],
		    {"Content-Type": "application/json", 
			"Accept": "application/json"}, 
		    json_data);
		break;
	    case PREFIX_IMP_LABELING :
		server.log(format("Scanned label code %s", barcode));
		if (!nv.at_factory) {
		    return false;
		}
		is_special = true;
		dataToSend = {
			"macAddress": nv.macAddress,
			"serialNumber": nv.gImpeeId,
			"agentUrl": http.agenturl(),
			"command": TEST_CMD_PRINT_LABEL
    		    };
		sendMixPanelEvent(MIX_PANEL_EVENT_CONFIG,dataToSend);
		//sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_CONFIG,dataToSend));
		local json_data = http.jsonencode (dataToSend);
		server.log(json_data);
		req = http.post(
		    gSpecialBarcodePrefixes[i]["url"],
		    {"Content-Type": "application/json", 
			"Accept": "application/json"}, 
		    json_data);
		break;
	    case PREFIX_GENERAL :
		server.log(format("Scanned special barcode %s", barcode));
		is_special = true;
		local timeStr = getUTCTime();
		local mySig = http.hash.sha256(gAuthData.app_id+gAuthData.secret+timeStr);
		mySig = BlobToHexString(mySig);
		
		server.log(format("Current Impee Id=%s Valid ImpeeId=%s",nv.gImpeeId, data.serial));
		nv.gImpeeId = data.serial;
		
		local newData = {
    		    "frob":data.scandata,   			
    		    "token": nv.gImpeeId,
                    "sig": mySig,
                    "app_id": gAuthData.app_id,
                    "time": timeStr,
                    "serialNumber": nv.gImpeeId,
    		};
		
		dataToSend = {"frob":data.scandata, "command":"ExternalAppAuthentication"};
		local url = gSetupUrl+"/"+data.scandata;
		server.log("Put URL: "+url);
		// URL-encode the whole thing
		dataToSend = http.urlencode(newData);
		server.log(dataToSend);
		req = http.put(
		    url,
		    {"Content-Type": "application/x-www-form-urlencoded", 
			"Accept": "application/json"}, 
		    dataToSend);
		break;
	    }
	}
    }
    if (!is_special)
	return false;
	
    sendMixPanelEvent(MIX_PANEL_EVENT_CONFIG,dataToSend);
    //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_CONFIG,dataToSend));	
    // Create and send the request
    agentLog("Sending special barcode to server...");
            
    // If the server is down, this will block all other events
    // until it times out.  Events seem to be queued on the server 
    // with no ill effects.  They do not block the device.  Could consider 
    // moving to async. The timeout period (tested) is 60 seconds.  
    gTransactionTime = time();
    req.sendasync(onSpecialBarcodeReturn);
    return true;
}

function onSpecialBarcodeReturn(res) {
    gTransactionTime = time() - gTransactionTime;
    agentLog(format("Server transaction time: %ds", gTransactionTime));

    // Handle the response
    local returnString = "success-server";

    if (res.statuscode != 200)
    {
        returnString = "failure"
        agentLog(format("Error: got status code %d, expected 200", 
                    res.statuscode));
    }
    else
    {
        // Parse the response (in JSON format)
        local body = http.jsondecode(res.body);
		local body = body.response;
        try 
        {
            // Handle the various non-OK responses.  Nothing to do for "ok". 
            if (body.status != "ok")
            {
                returnString = "failure";
				agentLog(format("AGENT: Beep Error: %s",http.jsonencode(body)));
            }
        }
        catch(e)
        {
            agentLog(format("Caught exception: %s", e));
            returnString = "failure";
        }
    }

    // Return status to device
    // TODO: device.send will be dropped if response took so long that 
    // the device went back to sleep.  Handle that? 
    device.send("uploadCompleted", returnString);
}

function sendLogToServer(data)
{
    local disableSendToServer = false;
    //disableSendToServer = true;
    if (disableSendToServer)
    {
        server.log("AGENT: (sending to hiku server not enabled)");
        return;
    }
    
    
    gLogTable[gLogTableIndex].data +=data+"\n";
    gLogTable[gLogTableIndex].count++;
    
    if( gLogTable[gLogTableIndex].count == MAX_LOG_COUNT)
    {
       local prevIndex = gLogTableIndex;
       
       gLogTableIndex = (gLogTableIndex + 1) % MAX_TABLE_COUNT;
       gLogTable[prevIndex].count = 0;
       data = {log=gLogTable[prevIndex].data};
       gLogTable[prevIndex].data = "";
    }
    else
    {
      server.log(format("still budnling up: Table Index: %d, Log Count: %d",gLogTableIndex,gLogTable[gLogTableIndex].count));
      return;
    }    
        
    // URL-encode the whole thing
   // data = http.urlencode(data);
    local timeStr = getUTCTime();
    local mySig = http.hash.sha256(gAuthData.app_id+gAuthData.secret+timeStr);
    mySig = BlobToHexString(mySig);
    data = {    
			"sig": mySig,
			"time":timeStr,
            "app_id": gAuthData.app_id,
			serialNumber = nv.gImpeeId,
			logData = http.jsonencode(data)
		   };
    data = http.urlencode(data);
    server.log("sendToLogServer: "+data);

    // Create and send the request
    local req = http.post(
            //"http://199.115.118.221/cgi-bin/addDeviceLog.py",
            //"http://srv2.hiku.us/cgi-bin/addDeviceLog.py",
            gLogUrl,
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            data);
            
    // If the server is down, this will block all other events
    // until it times out.  Events seem to be queued on the server 
    // with no ill effects.  They do not block the device.  Could consider 
    // moving to async. The timeout period (tested) is 60 seconds.  
    req.sendasync(onComplete);
}

function onComplete(m)
{
    if (m.statuscode != 200)
    {
        server.log(format("AGENT: Log Status: Error: got status code %d, expected 200", 
                    m.statuscode));
    }
    else
    {

        // Parse the response (in JSON format)
        local body = http.jsondecode(m.body);
        local body = body.response;
        try 
        {
            // Handle the various non-OK responses.  Nothing to do for "ok". 
            // Handle the various non-OK responses.  Nothing to do for "ok". 
            //dumpTable(body);
            if (body.status != "ok")
            {
				server.log(format("AGENT: Log Status - Error: %s", body.errMsg));
            }
            else
            {
            	server.log(format("AGENT: Log Status Success: %s", http.jsonencode(body.data)));
            }
        }
        catch(e)
        {
            server.log(format("AGENT: Log Status - Caught exception: %s", e));
        }
    }
}

//**********************************************************************
// Receive and send out the beep packet
device.on("uploadBeep", function(data) {
    gLinkedRecord = "";  // Clear on next (i.e. this) barcode scan
    nv.gLinkedRecordTimeout = null;
    sendBeepToHikuServer(data);  
});

//**********************************************************************
// Receive and send out the beep packet
device.on("batteryLevel", function(data) {

    agentLog(format("Battery Level Raw Reading: %d", 
                   data));	
	if( data >= 59760 ) // >= 4.10V battery voltage
	{
		data = 100;
	}
	else if ( data < 59760 && data >= 59031 ) // >= 4.05 V battery voltage
	{
		data = 95;
	}	
	else if ( data < 59031 && data >= 58302 ) // >= 4.00V battery voltage
	{
		data = 90;
	}	
	else if ( data < 58302 && data >= 57719 ) // >= 3.96V battery voltage
	{
		data = 85;
	}
	else if ( data < 57719 && data >= 57136 ) // >= 3.92V battery voltage
	{
		data = 80;
	}
	else if ( data < 57136 && data >= 56699 ) // >= 3.89V battery voltage
	{
		data = 75;
	}
	else if ( data < 56699 && data >= 56407 ) // >= 3.87V battery voltage
	{
		data = 70;
	}
	else if ( data < 56407 && data >= 56043 ) // >= 3.845V battery voltage
	{
		data = 65;
	}
	else if ( data < 56043 && data >= 55678 ) // >= 3.82V battery voltage
	{
		data = 60;
	}
	else if ( data < 55678 && data >= 55460 ) // >= 3.805V battery voltage
	{
		data = 55;
	} 	
	else if ( data < 55460 && data >= 55241 ) // >= 3.79V battery voltage
	{
		data = 50;
	} 
	else if ( data < 55241 && data >= 54950 ) // >= 3.77V battery voltage
	{
		data = 45;
	}
	else if( data < 54950 && data >= 54658 ) // >= 3.75V battery voltage
	{
		data = 40;
	}
	else if( data < 54658 && data >= 54294 ) // >= 3.725V battery voltage
	{
		data = 35;
	}
	else if( data < 54294 && data >= 53929 ) // >= 3.70V battery voltage
	{
		data = 30;
	}
	else if( data < 53929 && data >= 53565 ) // >= 3.675V battery voltage
	{
		data = 25;
	}	
	else if( data < 53565 && data >= 53200 ) // >= 3.65V battery voltage
	{
		data = 20;
	}
	else if( data < 53200 && data >= 52763 ) // >= 3.62V battery voltage
	{
		data = 15;
	}
	else if( data < 52763 && data >= 51014) // >= 3.5V battery voltage
	{
		data = 10;
	}	
	else if( data < 51014 && data >= 50285 ) // >= 3.45V battery voltage
	{
		data = 5;
	}			
	else if( data < 50285 ) // <3.40V battery voltage
	{
		// This means we are below 5% and its 43726.16722 for 0%
		// Perhaps we should give finer granular percentage here until it hits 1% to 0%
		data = 1;
	}

	nv.gBatteryLevel = data;

    sendBatteryLevelToHikuServer({batteryLevel=data});
    sendMixPanelEvent(MIX_PANEL_EVENT_BATTERY,{"status":nv.gBatteryLevel});
    
});


//**********************************************************************
// Prepare to receive audio from the device
device.on("startAudioUpload", function(data) {
    //agentLog("in startAudioUpload");
    sendMixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"StartAudio"});
    //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"StartAudio"}));
    // Reset our audio buffer
    // HACK
    // HACK
    // HACK
    // Have to correctly handle the case where the user presses the button
    // again to submit a second audio recording while the data of the first
    // recording hasn't yet been sent to the server. This can be the case
    // if audio data is still on the device or if the final HTTP GET from the server
    // to pick up the audio data is still in flight.
    // 
    // Currently, the audio data for the first recording gets destroyed when the
    // user presses the button again.
    gAudioToken = "";
    gChunkCount = 0;
    gAudioReadPointer = 0;
    gAudioString = "";
    gAudioAbort = false;
    
    // POST to the server to indicate that a new audio recording is
    // starting; receive an audio token in return
    local newData;
    local timeStr = getUTCTime();
    local mySig = http.hash.sha256(gAuthData.app_id+gAuthData.secret+timeStr);
    mySig = BlobToHexString(mySig);
    
    newData = {
          // FIXME don't send agent URL in the clear over HTTP
          "agentUrl": http.agenturl(),
          "agentAudioRate": 8000,
    	  "token": nv.gImpeeId,
          "sig": mySig,
          "app_id": gAuthData.app_id,
          "time": timeStr,
    };    	
    
    data = http.urlencode( newData );
    
    server.log("AGENT: Posting audio start to server...");
    local req = http.post(
            //"http://199.115.118.221/cgi-bin/addDeviceEvent.py",
            //"http://srv2.hiku.us/cgi-bin/addDeviceEvent.py",
            gAudioUrl,
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            data);

    req.sendasync(onReceiveAudioToken);
});

function onReceiveAudioToken(m)
{
    if (m.statuscode != 200)
    {
        agentLog(format("Token POST: Got status code %d, expected 200.", m.statuscode));
        gAudioState = AudioStates.AudioError;
    }
    else
    {
      // Parse the response (in JSON format)
        local body = http.jsondecode(m.body);
        local body = body.response;
        try 
        {
          if (body.status != "ok") {
            gAudioState = AudioStates.AudioError;
            agentLog("Token POST: Received non-ok status");
          } else {  
            gAudioToken = body.data.audioToken;
            agentLog(format("Token POST: AudioToken received: %s", gAudioToken));
            gAudioState = gAudioAbort ? AudioStates.AudioError : AudioStates.AudioRecording;
          }
        }
        catch(e)
        {
            agentLog(format("Token POST: Caught exception: %s", e));
            gAudioState = AudioStates.AudioError;
	    //sendMixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"AudioFailed"});
	    //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"AudioFailed"}));
        }
    }
}

device.on("button",function(str){
  server.log(str);
  sendMixPanelEvent(MIX_PANEL_EVENT_DEVICE_BUTTON,{"status":str});
  sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_DEVICE_BUTTON,str));
});

device.on("usbState",function(str){
  server.log(str);
  sendMixPanelEvent(MIX_PANEL_EVENT_CHARGER,{"USBState":str});
  //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_CHARGER,{"USBState":str}));
});

device.on("deviceLog", function(str){
	// this needs to be changed post to an http url
	server.log(format("DEVICE: %s",str));
	sendLogToServer(format("%s DEVICE: %s",getUTCTime(), str));
});

//**********************************************************************
// Send complete audio sample to the server
device.on("abortAudioUpload", function(data) {
    agentLog("Error: aborting audio upload");
    gAudioAbort = true;
    gAudioState = AudioStates.AudioError;
});
  
//**********************************************************************
// Send complete audio sample to the server
device.on("endAudioUpload", function(data) {
    //agentLog("in endAudioUpload");
    sendMixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"AudioUploadEnd"});
    //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"AudioUploadEnd"}));
    // If  no audio data, just exit
    if (gAudioString.len() == 0)
    {
        agentLog("No audio data to send to server.");
        return;
    }

    if (gChunkCount != data.scansize)
    {
        agentLog(format("ERROR: expected %d chunks, got %d", 
                   data.scansize, gChunkCount));
    }

    if (data.scandata != "")
    {
        agentLog("Error: found barcode when expected only audio data");
        gAudioState = AudioStates.AudioError;
	sendMixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"AudioUploadEndFail","barcode":data.scandata});
	//sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"AudioUploadEndFail","barcode":data.scandata}));
    }

    local sendToDebugServer = false;
    //sendToDebugServer = true;
    // HACK
    // HACK 
    // HACK
    // needs to be rewritten as gAudioBuffer has been replaced with
    // gAudioString
    /*
    if (sendToDebugServer)
    {
        // Send audio to server
        agentLog(format("Audio ready to send. Size=%d", gAudioBuffer.len()));
        local req = http.post("http://bobert.net:4444/"+nv.gImpeeId, 
                             {"Content-Type": "application/octet-stream"}, 
                             http.base64encode(gAudioBuffer));
        local res = req.sendsync();

        if (res.statuscode != 200)
        {
            agentLog("An error occurred:");
            agentLog(format("statuscode=%d", res.statuscode));
            device.send("uploadCompleted", "failure");
        }
        else
        {
            agentLog("Audio sent to server.");
            device.send("uploadCompleted", "success-server");
        }

        return;
    } */
    sendBeepToHikuServer(data);  
});


//**********************************************************************
// Handle an audio buffer from the device
device.on("uploadAudioChunk", function(data) {
    //agentLog(format("in device.on uploadAudioChunk"));
    //agentLog(format("chunk length=%d", data.length));
    //dumpBlob(data.buffer);
    sendMixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"AudioChunkReceivedFromDevice"});
    //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"AudioChunkReceivedFromDevice"}));
    // Add the new data to the audio buffer, truncating if necessary
    data.buffer.resize(data.length);  // Most efficient way to truncate? 
    gAudioString=gAudioString+data.buffer.tostring();
    gChunkCount++;
});

device.on("shutdownRequestReason", function(status){
	agentLog(format("Hiku shutting down. Reason=%d", status));
});

device.on("buttonTimeout", function(data) {
    
    agentLog(format("Button timeout occurred: %d", data.instantonTimeout));	
    //sendMixPanelEvent(MIX_PANEL_EVENT_BUTTON_TIMEOUT,{"timeoutSource": (data.instantonTimeout?"instant-on":"standard") });
    sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_BUTTON_TIMEOUT, (data.instantonTimeout?"instant-on":"standard") ));

});

//======================================================================
// External HTTP request handling

//**********************************************************************
// Handle incoming requests to my external agent URL.  
http.onrequest(function (request, res)
{
    // Handle supported requests
	server.log(format("Request camethrough for: %s, method: %s",request.path, request.method));
    try {
      if ( request.method == "GET") { 
        if (request.path == "/getImpeeId") 
        {
          res.send(200, nv.gImpeeId);
        }
        else if (request.path == "/shippingMode") 
        {
          // HACK
          // safer implementation should require the device to call back the 
          // backend confirming that it is entering shipping mode
          device.send("shippingMode",1);
    	  res.send(200, "OK");
        } 
        else if (request.path == "/devicePage") 
        {
          //device.send("devicePage",1);
    	    res.send(200, "OK");
        } 
        else if( request.path == "/getAgentVersion" )
        {   
    	    res.send(200,gAgentVersion);
        }
        else if( request.path == "/audio/" + gAudioToken)
        {
          local audioLen = gAudioString.len();
          local audioSubstring;

          if (gAudioReadPointer < audioLen) {
             audioSubstring = gAudioString.slice(gAudioReadPointer, audioLen);
             gAudioReadPointer = audioLen;
          } else {
             audioSubstring = "";
          } 
          //agentLog(format("AUDIOPOINTER %d len %d", gAudioReadPointer, audioLen));
    
    	    res.header("Content-Type", "audio/x-wav");
    	    res.header("audioState", gAudioState);

    	    res.send(200, audioSubstring);
        } 
        // for audio debug
        /*
        else if( request.path == "/getAudioRecording" )
        {
    	    res.send(200, gAudioString);
        } */
        else
        {
          agentLog(format("HTTP GET: Unexpected path %s", request.path));
          res.send(404, format("HTTP GET: Unexpected path %s", request.path));
        }
	  } else if (request.method == "POST"){
		if (request.path == "/extendTimeout")
		{
		  local data = http.jsondecode(request.body);
		  // receive the extendTimeout message from the server
		  if ("timeout" in data) {
			server.log(format("Timeout is set to: %d",data["timeout"]));
			// if we have the timeout flag set in the query
			// then enable it on the agent and kick off a timer
			nv.extendTimeout = data["timeout"];
			if (nv.extendTimeout != 0.0 )
			{
			   // send the timeout value to the device
			   // and schedule a timer so that we can clear it at the end of
			   // expiry
			   server.log("Timerset!!");
			   gExtendTimer = imp.wakeup(nv.extendTimeout,function(){
			     server.log("ExtendTimeout expired!!");
			     nv.extendTimeout = 0.0;
				 gExtendTimer = null;
				 device.send("stayAwake",false);
			   });
			   device.send("stayAwake",true);
			}
			else
			{
			   if ( gExtendTimer )
			   {
			     imp.cancelwakeup(gExtendTimer);
				 gExtendTimer = null;
			   }
			   device.send("stayAwake",false);
			}
			res.send(200,"OK");
		  }
		  else
		  {
		    res.send(400,"Missing timeout field");
		  }
		}
      } else {
        agentLog(format("HTTP method not allowed: %s", request.method));
        res.send(405, format("HTTP method not allowed: %s", request.method));
      }
    } catch (ex) {
     agentLog("Internal Server Error: " + ex);
     res.send(501, "Internal Server Error: " + ex);
  }
});


function getDisconnectReason(reason)
{
  //NO_WIFI=1, NO_IP_ADDRESS=2, NO_SERVER=4, NOT_RESOLVED=3
    if (reason == 1) {
        return "Wifi went away";
    }
 
    if (reason == 2) {
        return "Failed to get IP address";
    }
 
    if (reason == 4) {
        return "Failed to connect to server";
    }
 
    if (reason == 3) {
        return "Failed to resolve server";
    }
 
    return "No Disconnects"
}

function xlate_bootreason_to_string(boot_reason)
{
	local reason = "";
	local pin = 0;
	for( pin =0; pin < 8; pin ++ )
	{
		if( boot_reason & ( 1 << pin ) )
		{
			reason += boot_reasons[pin];
		}
	}
	if( reason == "")
	{
		reason = "COLDBOOT";
	}
	return reason;
}


function updateImpeeId(data)
{
	nv.gImpeeId = data
    server.log(format("Impee Id got Updated: %s", nv.gImpeeId));
    sendDeviceEvents(
    					{  	  
    						  fw_version=nv.gFwVersion,
    						  wakeup_reason = xlate_bootreason_to_string(nv.gWakeUpReason),
    						  boot_time = nv.gBootTime,
    						  sleep_duration = nv.gSleepDuration,
    						  rssi = data.rssi,
    					}
    				);	
}


//**********************************************************************
// Receive impee ID from the device and send to the external requestor 
device.on("init_status", function(data) {
    
	imp.wakeup(0.001, function(){
	  device.send("stayAwake",(nv.extendTimeout != 0.0));
	})

    nv.gImpeeId = data.impeeId;
    nv.gFwVersion = data.fw_version;
    nv.gWakeUpReason = data.bootup_reason;
    nv.gSleepDuration = data.sleep_duration;
    nv.at_factory = data.at_factory;
    nv.macAddress = data.macAddress;
    
    //server.log(format("Device to Agent Time: %dms", (time()*1000 - data.time_stamp)));
    server.log(format("Device OS Version: %s", data.osVersion));
    local dataToSend =  {  	  
		      fw_version=nv.gFwVersion,
		      wakeup_reason = xlate_bootreason_to_string(nv.gWakeUpReason),
		      boot_time = nv.gBootTime,
		      sleep_duration = nv.gSleepDuration,
		      rssi = data.rssi,
		      dc_reason = getDisconnectReason(data.disconnect_reason),
		      os_version = data.osVersion,
		      connectTime = data.time_to_connect,
			  ssid = data.ssid,
			  agent_url = http.agenturl(),
    		};
	
    sendDeviceEvents(dataToSend);
    sendMixPanelEvent(MIX_PANEL_EVENT_STATUS,dataToSend);
    //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_STATUS,dataToSend));
    
});

// Receive the Charger state update from the device to be used to send to the
// external server
// @param: chargerState of True means Charger is attached, false otherwise
device.on("chargerState", function( chargerState ){
    nv.gChargerState = chargerState;
    local dataToSend = {  	  
    		   charger_state = nv.gChargerState?"charging":"not charging",
    		 }
    sendDeviceEvents(dataToSend);
    sendMixPanelEvent(MIX_PANEL_EVENT_CHARGER,dataToSend);
    //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_CHARGER,dataToSend));
});


//======================================================================
// Utility Functions



function getUTCTime()
{
	local str ="";
	//[dateFormatter setDateFormat:@"yyyy-MM-dd HH:mm:ss.SSSSSS"];
	local d=date();
    str = format("%04d-%02d-%02d %02d:%02d:%02d.000000", d.year, d.month+1, d.day, d.hour, d.min, d.sec);
    return str;
}

function BlobToHexString(data) {
  local str = "";
  foreach (b in data) str += format("%02x", b);
  return str;
}

//**********************************************************************
// Print all ServerRequest fields 
function logServerRequest(request)
{
    agentLog(format("request.body: \"%s\"", request.body));
    agentLog(format("request.headers:"));
    foreach (key, value in request.headers)
    {
        agentLog(format("-->   \"%s=%s\"", key, value));
    }
    agentLog(format("request.method: \"%s\"", request.method));
}


//**********************************************************************
// Proxy for server.log that prints a line prefix showing it is from the agent
function agentLog(str)
{
    sendLogToServer(format("%s AGENT: %s", getUTCTime(), str));
    server.log(format("AGENT: %s", str));
}


//**********************************************************************
// Print the contents of a table
function dumpTable(data, prefix="")
{
    server.log("Dumping the table");
    foreach (k, v in data)
    {
        if (typeof v == "table")
        {
            server.log(prefix + k.tostring() + " {");
            dumpTable(v, prefix+"-");
            server.log(prefix + "}");
        }
        else
        {
            if (v == null)
            {
                v = "(null)"
            }
            server.log(prefix + k.tostring() + "=" + v.tostring());
        }
    }
}


//**********************************************************************
// Print the contents of a blob to the log in a formatted way
function dumpBlob(data)
{
    // Constants
    const cBlob8BitSigned = 'c';
    const cBlob16BitSigned = 's';
    const cBlob16BitUnsigned = 'w';

    // Output parameters
    local dataType = cBlob8BitSigned; // Type of data stored (see
                                      // blob documentation)
    local elementsPerLine = 12; // Number of elements to print per line
    local linesToDump = 10; // Max number of lines to dump

    local str = ""; 
    local elements = 0;
    local lines = 0;

    data.seek(0);
    while(!data.eos())
    {
        // If too many lines, indicate there is more data not printed,
        // drop any current data, and exit the loop
        if (lines > linesToDump)
        {
            agentLog("(truncated...)");
            elements = 0; 
            break;
        }

        // Get the next element
        str += data.readn(dataType) + " ";
        elements++;

        // If we have a full line, print it out 
        if (elements >= elementsPerLine)
        {
            agentLog(str);
            str = "";
            elements = 0;
            lines++;
        }
    }
    if (elements > 0)
    {
        // Got to end of buffer with less than a full line. Print remainder. 
        agentLog(str);
    }
}