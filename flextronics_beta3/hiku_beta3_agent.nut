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
			
    	  };
}

server.log(format("Agent started, external URL=%s at time=%ds", http.agenturl(), time()));

gAgentVersion <- "1.1.10";

gAudioState <- AudioStates.AudioError;

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
    			secret="c923b1e09386"
			}

// Heroku server base URL			
gBaseUrl <- "https://hiku.herokuapp.com/api/v1";

gServerUrl <- gBaseUrl + "/list";	

gBatteryUrl <- gBaseUrl + "/device";

gLogUrl <- gBaseUrl + "/log";

gSetupUrl <- gBaseUrl + "/apps";

gAudioUrl <- gBaseUrl + "/audio";

const BATT_0_PERCENT = 43726.16;


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

//**********************************************************************
// Send audio and/or barcode to hiku's server
function sendBeepToHikuServer(data)
{
    //server.log(format("AGENT: Audio get at %d", gAudioReadPointer));

    local disableSendToServer = false;
    local newData;
    local is_superscan = false;
    //disableSendToServer = true;
    if (disableSendToServer)
    {
        agentLog("(sending to hiku server not enabled)");
        gAudioState = AudioStates.AudioError;
        return;
    }
    
    
    if( data.scandata != "" && isSpecialBarcode(data.scandata))
    {
      server.log("Checking Special Barcode Successful");
      if (gAudioState != AudioStates.AudioError) {
        gAudioState = AudioStates.AudioFinishedBarcode;
      }
    	sendSpecialBarcode(data);
    	return;
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
      }
    }
    else 
    {
      if (is_superscan) {
    	    newData = {
    			  "ean":data.scandata,
    			  "audioToken": gAudioToken,
    			  "audioType": "alaw",    			
    			  "token": nv.gImpeeId,
            "sig": mySig,
            "app_id": gAuthData.app_id,
            "time": timeStr,
    		 };   
    	   if (gAudioState != AudioStates.AudioError) {
	          gAudioState = AudioStates.AudioFinished;
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
      // save EAN in case of a superscan
      gEan = newData.ean;
    }
    data = newData;
        
    // URL-encode the whole thing
    data = http.urlencode(data);
    server.log(data);

    // Create and send the request
    agentLog("Sending beep to server...");
    local req = http.post(
            //"http://bobert.net:4444", 
            //"http://www.hiku.us/sand/cgi-bin/readRawDeviceData.py", 
            //"http://199.115.118.221/scanner_1/imp_beep",
            gServerUrl,
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            data);
            
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
    device.send("uploadCompleted", returnString);
}

// Send the barcode to hiku's server
function sendSpecialBarcode(data)
{
    local disableSendToServer = false;
    local newData;
    //disableSendToServer = true;
    if (disableSendToServer)
    {
        agentLog("(sending to hiku server not enabled)");
        return;
    }
    
    local timeStr = getUTCTime();
    local mySig = http.hash.sha256(gAuthData.app_id+gAuthData.secret+timeStr);
    mySig = BlobToHexString(mySig);
    
    server.log(format("Current Impee Id=%s Valid ImpeeId=%s",nv.gImpeeId, data.serial));
    nv.gImpeeId = data.serial;

    newData = {
    			"frob":data.scandata,   			
    			"token": nv.gImpeeId,
                "sig": mySig,
                "app_id": gAuthData.app_id,
                "time": timeStr,
                "serialNumber": nv.gImpeeId,
    		  };
	  
    //data = gAuthData + newData;
    
    
    local url = gSetupUrl+"/"+data.scandata;
    server.log("Put URL: "+url);
    data = newData;
        
    // URL-encode the whole thing
    data = http.urlencode(data);
    server.log(data);
    // Create and send the request
    agentLog("Sending beep to server...");
    local req = http.put(
            //"http://bobert.net:4444", 
            //"http://www.hiku.us/sand/cgi-bin/readRawDeviceData.py", 
            //"http://199.115.118.221/scanner_1/imp_beep",
            url,
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            data);
            
    // If the server is down, this will block all other events
    // until it times out.  Events seem to be queued on the server 
    // with no ill effects.  They do not block the device.  Could consider 
    // moving to async. The timeout period (tested) is 60 seconds.  
    local res;
    gTransactionTime = time();
    req.sendasync(onSpecialBarcodeReturn);
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


function isSpecialBarcode(barcode)
{
  local specialPrefix = ".HFB";
  
  if( barcode.len() > specialPrefix.len() )
  {
  	// The barcode is longer than specialPrefix length
  	// at this time we can compare the 4 characters and validate
  	local temp = barcode.slice(0,specialPrefix.len());
  	server.log("Original Barcode: "+barcode+" Sliced Barcode: "+temp);
  	return (temp == specialPrefix);
  }
  
  return false;
}

/*
function onBeepComplete(m)
{

    // Handle the response
    local returnString = "success-server";

    if (m.statuscode != 200)
    {
        returnString = "failure"
        agentLog(format("Error: got status code %d, expected 200", 
                    m.statuscode));
    }
    else
    {

        // Parse the response (in JSON format)
        local body = http.jsondecode(m.body);

        try 
        {
            // Handle the various non-OK responses.  Nothing to do for "ok". 
            if (body.status != "ok")
            {
                // Possible causes: speech2text failure, unknown.
                if ("error" in body.cause)
                {
                    returnString = "failure";
                    agentLog(format("Error: server responded with %s", 
                                         body.cause.error));
                }
                // Possible causes: unknown UPC code
                else if ("linkedrecord" in body.cause)
                {
                    gLinkedRecord = body.cause.linkedrecord;
                    nv.gLinkedRecordTimeout = time()+10; // in seconds
                    returnString = "unknown-upc";
                    agentLog("Response: unknown UPC code");
                }
                // Unknown response type
                else
                {
                    returnString = "failure";
                    agentLog("Error: unexpected cause in response");
                }
            }
        }
        catch(e)
        {
            agentLog(format("Caught exception: %s", e));
            returnString = "failure";
            agentLog("Error: malformed response");
        }
    }

    // Return status to device
    // TODO: device.send will be dropped if response took so long that 
    // the device went back to sleep.  Handle that? 
    device.send("uploadCompleted", returnString);
}
*/

function sendLogToServer(data)
{
    local disableSendToServer = false;
    //disableSendToServer = true;
    if (disableSendToServer)
    {
        server.log("AGENT: (sending to hiku server not enabled)");
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
	if( data >= 60200 )
	{
		data = 100;
	}
	else if ( data < 60200 && data >= 59649.77978 ) 
	{
		data = 95;
	}	
	else if ( data < 59649.77978 && data >= 58811.69491 ) 
	{
		data = 90;
	}	
	else if ( data < 58811.69491 && data >= 57973.61004 ) 
	{
		data = 85;
	}
	else if ( data < 57973.61004 && data >= 57135.52517 ) 
	{
		data = 80;
	}
	else if ( data < 57135.52517 && data >= 56297.44029 ) 
	{
		data = 75;
	}
	else if ( data < 56297.44029 && data >= 55459.35542 ) 
	{
		data = 70;
	}
	else if ( data < 55459.35542 && data >= 54621.27055 ) 
	{
		data = 65;
	}
	else if ( data < 54621.27055 && data >= 53783.18568 ) 
	{
		data = 60;
	}
	else if ( data < 53783.18568 && data >= 52945.10081 ) 
	{
		data = 55;
	} 	
	else if ( data < 52945.10081 && data >= 52107.01594 ) 
	{
		data = 50;
	} 
	else if ( data < 52107.01594 && data >= 51268.93106 )
	{
		data = 45;
	}
	else if( data < 51268.93106 && data >= 50430.84619 )
	{
		data = 40;
	}
	else if( data < 50430.84619 && data >= 49592.76132 )
	{
		data = 35;
	}
	else if( data < 49592.76132 && data >= 48754.67645 )
	{
		data = 30;
	}
	else if( data < 48754.67645 && data >= 47916.59158 )
	{
		data = 25;
	}	
	else if( data < 47916.59158 && data >= 47078.50671 )
	{
		data = 20;
	}
	else if( data < 47078.50671 && data >= 46240.42183 )
	{
		data = 15;
	}
	else if( data < 46240.42183 && data >= 45402.33696 )
	{
		data = 10;
	}	
	else if( data < 45402.33696 && data >= 44564.252094 )
	{
		data = 5;
	}			
	else if( data < 44564.25209 )
	{
		// This means we are below 5% and its 43726.16722 for 0%
		// Perhaps we should give finer granular percentage here until it hits 1% to 0%
		data = 1;
	}

	nv.gBatteryLevel = data;
    sendDeviceEvents(
    					{  	  
    						  battery_level = nv.gBatteryLevel
    					}
    				);	
    sendBatteryLevelToHikuServer({batteryLevel=data});  
});


//**********************************************************************
// Prepare to receive audio from the device
device.on("startAudioUpload", function(data) {
    //agentLog("in startAudioUpload");

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
            gAudioState = AudioStates.AudioRecording;
          }
        }
        catch(e)
        {
            agentLog(format("Token POST: Caught exception: %s", e));
            gAudioState = AudioStates.AudioError;
        }
    }
}

device.on("deviceLog", function(str){
	// this needs to be changed post to an http url
	server.log(format("DEVICE: %s",str));
	sendLogToServer({log=format("DEVICE: %s",str)});
});

//**********************************************************************
// Send complete audio sample to the server
device.on("abortAudioUpload", function(data) {
    agentLog("Error: aborting audio upload");
    gAudioState = AudioStates.AudioError;
});
  
//**********************************************************************
// Send complete audio sample to the server
device.on("endAudioUpload", function(data) {
    //agentLog("in endAudioUpload");

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

    // Add the new data to the audio buffer, truncating if necessary
    data.buffer.resize(data.length);  // Most efficient way to truncate? 
    gAudioString=gAudioString+data.buffer.tostring();
    gChunkCount++;
});

device.on("shutdownRequestReason", function(status){
	agentLog(format("Hiku shutting down. Reason=%d", status));
});


//======================================================================
// External HTTP request handling

//**********************************************************************
// Handle incoming requests to my external agent URL.  
http.onrequest(function (request, res)
{
    // Handle supported requests
    try {
      if ( request.method == "GET") { 
        if (request.path == "/getImpeeId") 
        {
          res.send(200, nv.gImpeeId);
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
    nv.gImpeeId = data.impeeId;
    nv.gFwVersion = data.fw_version;
    nv.gWakeUpReason = data.bootup_reason;
    nv.gSleepDuration = data.sleep_duration;
    
    //server.log(format("Device to Agent Time: %dms", (time()*1000 - data.time_stamp)));
    server.log(format("Device OS Version: %s", data.osVersion));
    sendDeviceEvents(
    					{  	  
    						  fw_version=nv.gFwVersion,
    						  wakeup_reason = xlate_bootreason_to_string(nv.gWakeUpReason),
    						  boot_time = nv.gBootTime,
    						  sleep_duration = nv.gSleepDuration,
    						  rssi = data.rssi,
    						  dc_reason = getDisconnectReason(data.disconnect_reason),
    						  os_version = data.osVersion,
							  connectTime = data.time_to_connect
    					}
    				);
});

// Receive the Charger state update from the device to be used to send to the
// external server
// @param: chargerState of True means Charger is attached, false otherwise
device.on("chargerState", function( chargerState ){
    nv.gChargerState = chargerState;
    sendDeviceEvents(
    					{  	  
    						  charger_state = nv.gChargerState?"attached":"removed",
    					}
    				); 		 
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
    sendLogToServer({log=format("AGENT: %s", str), deviceID=nv.gImpeeId});
    server.log(format("AGENT: %s", str));
}


//**********************************************************************
// Print the contents of a table
function dumpTable(data, prefix="")
{
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