// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.


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


gAudioBuffer <- blob(0); // Contains the audio data for the current 
                         // session.  Resized as new buffers come in.  
gChunkCount <- 0; // Used to verify that we got the # we expected
gLinkedRecord <- ""; // Used to link unknown UPCs to audio records. 
                     // We will set this when we get a request to do
                     // so from the server, then clear it after we 
                     // send the next audio beep, after next barcode, 
                     // or after a timeout
                     
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
    data = http.jsonencode(data);
	data = { deviceID = nv.gImpeeId,
			 eventData = data };
    data = http.urlencode( data );
    server.log("AGENT: "+data);

    // Create and send the request
    server.log("AGENT: Sending Event to server...");
    local req = http.post(
            "http://199.115.118.221/cgi-bin/addDeviceEvent.py",
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            data);

    req.sendasync(onComplete);
}

//**********************************************************************
// Send the barcode to hiku's server
function sendBeepToHikuServer(data)
{
    local disableSendToServer = false;
    //disableSendToServer = true;
    if (disableSendToServer)
    {
        agentLog("(sending to hiku server not enabled)");
        return;
    }
    
    // Special handling for audio beeps 
    if (data.scandata == "")
    {
        // Encode the audio data as base64, and store the size. The 
        // "scansize" parameter is applicable for both audio and barcodes. 
        data.audiodata = http.base64encode(gAudioBuffer);
        data.scansize = data.audiodata.len();

        // If not expired, attach the current linkedrecord (usually 
        // blank). Then reset the global. 
        agentLog("checking if linked record");
        if (nv.gLinkedRecordTimeout && time() < nv.gLinkedRecordTimeout)
        {
            agentLog("record linked");
            data.linkedrecord = gLinkedRecord;
        }
        gLinkedRecord = ""; 
        nv.gLinkedRecordTimeout = null;
    }
        
    // URL-encode the whole thing
    data = http.urlencode(data);
    server.log(data);

    // Create and send the request
    agentLog("Sending beep to server...");
    local req = http.post(
            //"http://bobert.net:4444", 
            //"http://www.hiku.us/sand/cgi-bin/readRawDeviceData.py", 
            "http://199.115.118.221/scanner_1/imp_beep",
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            data);
            
    // If the server is down, this will block all other events
    // until it times out.  Events seem to be queued on the server 
    // with no ill effects.  They do not block the device.  Could consider 
    // moving to async. The timeout period (tested) is 60 seconds.  
    local res;
    local transactionTime = time();
    res = req.sendsync();
    transactionTime = time() - transactionTime;
    agentLog(format("Server transaction time: %ds", transactionTime));

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
        // TODO DEBUG remove
        agentLog(res.body);

        // Parse the response (in JSON format)
        local body = http.jsondecode(res.body);

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
    data = http.urlencode(data);
    server.log("sendToLogServer: "+data);

    // Create and send the request
    server.log("AGENT: Sending Logs to server...");
    local req = http.post(
            "http://199.115.118.221/cgi-bin/addDeviceLog.py",
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

        try 
        {
            // Handle the various non-OK responses.  Nothing to do for "ok". 
            if (body.returnVal != 1)
            {
				server.log("AGENT: sendLogToServer: "+body);
            }
        }
        catch(e)
        {
            server.log(format("AGENT: Caught exception: %s", e));
            server.log("AGENT: Error: malformed response");
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
// Prepare to receive audio from the device
device.on("startAudioUpload", function(data) {
    //agentLog("in startAudioUpload");

    // Reset our audio buffer
    gAudioBuffer.resize(0);
    gChunkCount = 0;
});

device.on("deviceLog", function(str){
	// this needs to be changed post to an http url
	server.log(format("DEVICE: %s",str));
	sendLogToServer({log=format("DEVICE: %s",str), deviceID=nv.gImpeeId});
});

//**********************************************************************
// Send complete audio sample to the server
device.on("endAudioUpload", function(data) {
    //agentLog("in endAudioUpload");

    // If  no audio data, just exit
    if (gAudioBuffer.len() == 0)
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
    }

    local sendToDebugServer = false;
    //sendToDebugServer = true;
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
    }
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
    gAudioBuffer.writeblob(data.buffer);
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
    if (request.path == "/getImpeeId") 
    {
        res.send(200, nv.gImpeeId);
    }
    else if (request.path == "/devicePage") 
    {
        //device.send("devicePage",1);
    	res.send(200, "OK");
    } 
    else
    {
        agentLog(format("AGENT Error: unexpected path %s", request.path));
        res.send(400, format("unexpected path %s", request.path));
  }
});


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


//**********************************************************************
// Receive impee ID from the device and send to the external requestor 
device.on("init_status", function(data) {
    nv.gImpeeId = data.impeeId;
    nv.gFwVersion = data.fw_version;
    nv.gWakeUpReason = data.bootup_reason;
    
    //server.log(format("Device to Agent Time: %dms", (time()*1000 - data.time_stamp)));
    
    sendDeviceEvents(
    					{  	  
    						  fw_version=nv.gFwVersion,
    						  charger_state = nv.gChargerState?"attached":"removed",
    						  battery_level = nv.gBatteryLevel,
    						  wakeup_reason = xlate_bootreason_to_string(nv.gWakeUpReason),
    						  boot_time = nv.gBootTime,
    						  sleep_duration = nv.gSleepDuration
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
            agentLog(prefix + k.tostring() + " {");
            dumpTable(v, prefix+"-");
            agentLog(prefix + "}");
        }
        else
        {
            if (v == null)
            {
                v = "(null)"
            }
            agentLog(prefix + k.tostring() + "=" + v.tostring());
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

